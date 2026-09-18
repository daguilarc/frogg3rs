// Scratch measurement harness for moving the Filter page's limiter from the
// peak branch to the page's output. Built three ways:
//   - against the worktree's app/ unchanged (no TAPS): master output only;
//   - against cur/ (TAPS): the shipped placement plus taps;
//   - against moved/ (TAPS): the limiter moved after the Comb/Peak blend, plus taps.
// Taps: driveOut (the Filter page's input), the blended composite before any
// limiter placed after the blend (`mixed`), the page's output (`filterOut`,
// what RouteFilterBank returns), the two legs after their blend gains, and the
// master output the rig captures.

#include "Froggers.hpp"
#include "FroggersModulation.hpp"
#include "FroggersParameters.hpp"
#include "support/SynthRig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using Rig = synth_rig::SynthRig<synth_froggers::FroggersApp>;
namespace dsp = synth_froggers::dsp;
using synth_froggers::FroggersBankId;

synth::RuntimeDataPaths ScratchPaths() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "flim2-harness";
    std::filesystem::remove_all(root);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(root);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

struct Knob {
    FroggersBankId bank;
    std::size_t slot;
    float value;
};

struct Patch {
    std::string name;
    bool mod = false;  // VCO1 audio at depth knob 1.0 (full positive) on Filter slots 3..7
    float crispy = 0.0f;
    std::vector<Knob> knobs;
};

enum class Variant { Shipped, LimiterBypass };

struct Stats {
    double peak = 0.0, sumSq = 0.0;
    std::size_t n = 0, over08 = 0, atClamp = 0;
    void Add(float x) {
        const double a = std::fabs(static_cast<double>(x));
        peak = std::max(peak, a);
        sumSq += a * a;
        ++n;
        if (a > 0.8) ++over08;
        if (a >= 0.999) ++atClamp;
    }
    double RmsDb() const { return n ? 20.0 * std::log10(std::sqrt(sumSq / n) + 1e-30) : -999.0; }
    double PeakDb() const { return 20.0 * std::log10(peak + 1e-30); }
};

std::uint64_t Fnv(std::uint64_t h, float x) {
    std::uint32_t bits;
    std::memcpy(&bits, &x, sizeof bits);
    for (int i = 0; i < 4; ++i) {
        h ^= (bits >> (8 * i)) & 0xffu;
        h *= 1099511628211ull;
    }
    return h;
}

double MaxShortRmsDb(const std::vector<float>& x) {
    constexpr std::size_t kWin = 480;  // 10 ms at 48 kHz
    double best = 0.0;
    for (std::size_t i = 0; i + kWin <= x.size(); i += kWin) {
        double ss = 0.0;
        for (std::size_t k = 0; k < kWin; ++k) ss += static_cast<double>(x[i + k]) * x[i + k];
        best = std::max(best, std::sqrt(ss / kWin));
    }
    return 20.0 * std::log10(best + 1e-30);
}

constexpr std::size_t kWarmBlocks = 94;      // ~0.5 s at 48 kHz, 256-sample blocks
constexpr std::size_t kMeasureBlocks = 375;  // 2.0 s

void RunOne(const Patch& p, Variant v) {
    Rig rig(64, ScratchPaths());
    if (v == Variant::LimiterBypass) {
        rig.Application().TestFilterPeakLimiter().Configure(48000.0f, 1.0e6f, 2.0e6f, dsp::kPeakLimiterAttackSeconds,
                                                            dsp::kPeakLimiterReleaseSeconds);
    }
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    if (p.mod) {
        for (std::size_t slot = 3; slot <= 7; ++slot) {
            synth::Parameter* depth =
                model.PageParameter(FroggersBankId::Filter, slot).EnsureModulationDepth(synth_froggers::kModSlotVco1Audio);
            if (depth == nullptr) {
                std::printf("FATAL: no depth slot\n");
                std::exit(2);
            }
            depth->SceneCenter(0) = 1.0f;
        }
    }
    model.Crispy(FroggersBankId::Filter).SceneCenter(0) = p.crispy;
    for (const Knob& k : p.knobs) model.PageParameter(k.bank, k.slot).SceneCenter(0) = k.value;
    rig.Application().TestParameterManager().ComputeAllParameters();
    rig.StartAt(0);
    rig.RunBlocks(kWarmBlocks);
    rig.ClearOutput();
#ifdef TAPS
    synth_froggers::FroggersApp::ScratchTaps taps;
    rig.Application().testTaps_ = &taps;
#endif
    float minLimEnv = 1.0f, minMasterEnv = 1.0f;
    for (std::size_t b = 0; b < kMeasureBlocks; ++b) {
        rig.RunBlocks(1);
        minLimEnv = std::min(minLimEnv, rig.Application().TestFilterPeakLimiter().envelope);
        minMasterEnv = std::min(minMasterEnv, rig.Application().TestOutputLimiter().envelope);
    }
    Stats master;
    std::uint64_t masterHash = 1469598103934665603ull;
    std::vector<float> masterL;
    for (const auto& f : rig.Output()) {
        masterL.push_back(f.channels[0]);
        for (float s : f.channels) {
            master.Add(s);
            masterHash = Fnv(masterHash, s);
        }
    }
    const char* vname = v == Variant::Shipped ? "limiter-on" : "limiter-bypassed";
#ifdef TAPS
    rig.Application().testTaps_ = nullptr;
    Stats fo, mx, cl, pl, dr;
    std::uint64_t foHash = 1469598103934665603ull, drHash = 1469598103934665603ull;
    float combDriveMin = 1e9f, combDriveMax = -1e9f, fbMin = 1e9f, fbMax = -1e9f;
    for (std::size_t i = 0; i < taps.filterOut.size(); ++i) {
        fo.Add(taps.filterOut[i]);
        mx.Add(taps.mixed[i]);
        cl.Add(taps.combLeg[i]);
        pl.Add(taps.peakLeg[i]);
        dr.Add(taps.driveOut[i]);
        foHash = Fnv(foHash, taps.filterOut[i]);
        drHash = Fnv(drHash, taps.driveOut[i]);
        combDriveMin = std::min(combDriveMin, taps.combDrive[i]);
        combDriveMax = std::max(combDriveMax, taps.combDrive[i]);
        fbMin = std::min(fbMin, taps.combFb[i]);
        fbMax = std::max(fbMax, taps.combFb[i]);
    }
    std::printf("%-40s %-16s | FILTER OUT pk %6.2f rms %7.2f st10 %6.2f | pre-limit blend pk %6.2f | comb leg pk %6.2f | "
                "peak leg pk %6.2f | drive pk %6.2f | MASTER pk %6.2f rms %7.2f st10 %6.2f clamp %.3f%% env %.4f | "
                "filterLimEnvMin %.4f | fb[%.3f..%.3f] cdrv[%.3f..%.3f] | hash filter %016llx drive %016llx master %016llx\n",
                p.name.c_str(), vname, fo.PeakDb(), fo.RmsDb(), MaxShortRmsDb(taps.filterOut), mx.PeakDb(), cl.PeakDb(),
                pl.PeakDb(), dr.PeakDb(), master.PeakDb(), master.RmsDb(), MaxShortRmsDb(masterL),
                100.0 * master.atClamp / std::max<std::size_t>(master.n, 1), minMasterEnv, minLimEnv, fbMin, fbMax,
                combDriveMin, combDriveMax, static_cast<unsigned long long>(foHash),
                static_cast<unsigned long long>(drHash), static_cast<unsigned long long>(masterHash));
#else
    std::printf("%-40s %-16s | MASTER pk %6.2f rms %7.2f clamp %.3f%% env %.4f | filterLimEnvMin %.4f | hash master %016llx\n",
                p.name.c_str(), vname, master.PeakDb(), master.RmsDb(),
                100.0 * master.atClamp / std::max<std::size_t>(master.n, 1), minMasterEnv, minLimEnv,
                static_cast<unsigned long long>(masterHash));
#endif
    std::fflush(stdout);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "grid";
    const FroggersBankId F = FroggersBankId::Filter;
    // Comb delay knob that puts the comb's pitch on VCO1's default 109.97 Hz:
    // combFreq = 100 Hz * 100^knob (RouteFilterBank's ExpMapCompute(100/sr, 10000/sr, knob)).
    const float kCombOnVco1 = static_cast<float>(std::log(109.97 / 100.0) / std::log(100.0));
    std::vector<Patch> patches;
    if (mode == "grid") {
        patches = {
            {"default", false, 0.0f, {}},
            {"default blend0.5", false, 0.0f, {{F, 12, 0.5f}}},
            {"default blend1.0", false, 0.0f, {{F, 12, 1.0f}}},
            {"modcomb crispy1 blend0", true, 1.0f, {}},
            {"modcomb crispy1 blend0.5", true, 1.0f, {{F, 12, 0.5f}}},
            {"modcomb crispy1 blend1.0", true, 1.0f, {{F, 12, 1.0f}}},
            {"modcomb crispy1 blend0.5 topo1", true, 1.0f, {{F, 12, 0.5f}, {F, 13, 1.0f}}},
            {"comb-resonant fb1 cdrv0 blend0", false, 0.0f, {{F, 4, kCombOnVco1}, {F, 5, 1.0f}, {F, 7, 0.0f}}},
            {"comb-resonant fb1 cdrv0 blend1.0", false, 0.0f,
             {{F, 4, kCombOnVco1}, {F, 5, 1.0f}, {F, 7, 0.0f}, {F, 12, 1.0f}}},
            {"comb-resonant fb1 cdrv0.5 blend1.0", false, 0.0f,
             {{F, 4, kCombOnVco1}, {F, 5, 1.0f}, {F, 7, 0.5f}, {F, 12, 1.0f}}},
        };
    } else if (mode == "order") {
        patches = {
            {"default", false, 0.0f, {}},
            {"drive gain1 wet1", false, 0.0f, {{FroggersBankId::Drive, 0, 1.0f}, {FroggersBankId::Drive, 1, 1.0f}}},
            {"delay wet1 send1", false, 0.0f, {{FroggersBankId::Delay, 0, 1.0f}, {FroggersBankId::Delay, 1, 1.0f}}},
        };
        for (const Patch& p : patches) RunOne(p, Variant::Shipped);
        return 0;
    } else if (mode == "hashcheck") {
        patches = {
            {"default", false, 0.0f, {}},
            {"modcomb crispy1 blend0.5", true, 1.0f, {{F, 12, 0.5f}}},
            {"comb-resonant fb1 cdrv0 blend1.0", false, 0.0f,
             {{F, 4, kCombOnVco1}, {F, 5, 1.0f}, {F, 7, 0.0f}, {F, 12, 1.0f}}},
        };
    }
    for (const Patch& p : patches) {
        RunOne(p, Variant::Shipped);
        RunOne(p, Variant::LimiterBypass);
    }
    return 0;
}
