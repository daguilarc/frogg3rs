// Two TSan probes, plus a deliberately-injected race used as this harness's
// positive control before either probe's clean result is trusted.
//
// The first probe mirrors FroggersModulationTests.cpp's own bare-fixture
// convention (manager + FroggersParameterModel + FroggersModulationSlate, no
// Engine/SynthRig): one thread repeatedly steps the slate (the audio-thread
// analog, walking the batch chain through ParameterGroup::firstStorageBatch_
// and each batch's own next pointer inside Compute/ProcessSample) while the
// other repeatedly calls ParameterGroup::AddParameterStorageBatch (the
// message-thread analog, appending a new batch onto that same chain).
// Clean after 10,000 batch requests with no TSan report.
//
// The second probe uses synth_rig::SynthRig<FroggersApp> (the real
// production ArmRecording/StopRecording/ProcessBlock): one thread loops
// StopRecording() then ArmRecording(n) with a small capacityFramesOverride
// against ProcessBlock running on another, with the transport running.
// Clean after 10^6 iterations with no TSan report.

#include "Froggers.hpp"
#include "FroggersModulation.hpp"
#include "FroggersParameters.hpp"
#include "support/SynthRig.hpp"

#include "synth/ParameterModulation.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "probe must not see JUCE headers"
#endif

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>

using namespace synth_froggers;

namespace {

using Rig = synth_rig::SynthRig<FroggersApp>;

synth::RuntimeDataPaths ScratchPaths(const char* tag) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "runA-tsan" / tag;
    std::filesystem::remove_all(root);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(root);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

// Positive control: two threads race a plain (non-atomic, unsynchronized)
// shared counter -- nothing to do with production code -- to prove this
// binary's TSan instrumentation actually flags a race it is handed, before
// either real finding below is trusted.
void RunInjectedRaceControl() {
    static int sharedCounter = 0;  // deliberately not atomic, not locked
    std::atomic<bool> go{false};
    std::thread a([&] {
        while (!go.load()) std::this_thread::yield();
        for (int i = 0; i < 100000; ++i) sharedCounter++;
    });
    std::thread b([&] {
        while (!go.load()) std::this_thread::yield();
        for (int i = 0; i < 100000; ++i) sharedCounter++;
    });
    go.store(true);
    a.join();
    b.join();
    std::printf("injected_race_control_done sharedCounter=%d (expected 200000 only if race-free)\n",
                sharedCounter);
}

void RunRun01(long batchRequests) {
    synth::ParameterManager manager;
    FroggersParameterModel model;
    FroggersModulationSlate slate;
    model.Init(manager);
    slate.Init(model.Group(), /*extraDepthCapacity=*/4);  // the run's own literal setup
    slate.Prepare(48000.0);

    std::atomic<bool> go{false};
    std::atomic<bool> stop{false};
    std::atomic<long> stepsRun{0};

    std::thread audioThread([&] {
        while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
        FroggersModulationSlate::VcoDrive drive{0.5f, 0.5f, 0.0f};
        slate.SetExternalAudioConnected(false);
        while (!stop.load(std::memory_order_acquire)) {
            slate.Step(drive, drive, drive, std::nullopt, 0.0f);
            stepsRun.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread messageThread([&] {
        while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
        for (long i = 0; i < batchRequests; ++i) {
            model.Group().AddParameterStorageBatch(
                synth::MakeParameterStorageBatch(model.Group().Config(), model.Group().GestureCount(), 4));
        }
    });

    go.store(true, std::memory_order_release);
    messageThread.join();
    stop.store(true, std::memory_order_release);
    audioThread.join();

    std::printf("run01_done batch_requests=%ld audio_steps=%ld\n", batchRequests, stepsRun.load());
}

void RunRun02(long iterations, std::size_t capacityFramesOverride) {
    Rig::AudioSettings settings;
    settings.sampleRate = 48000.0;
    settings.blockSize = 64;
    Rig rig(/*patchPumpBudgetBlocks=*/64, ScratchPaths("run02"), settings);
    rig.StartAt(0);
    rig.RunBlocks(8);
    rig.ClearOutput();

    FroggersApp& app = rig.Application();

    std::atomic<bool> go{false};
    std::atomic<bool> stop{false};
    std::atomic<long> blocksRun{0};

    std::thread audioThread([&] {
        while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
        while (!stop.load(std::memory_order_acquire)) {
            rig.RunBlocks(1);
            rig.ClearOutput();
            blocksRun.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread uiThread([&] {
        while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
        for (long i = 0; i < iterations; ++i) {
            app.StopRecording();
            app.ArmRecording(capacityFramesOverride);
        }
    });

    go.store(true, std::memory_order_release);
    uiThread.join();
    stop.store(true, std::memory_order_release);
    audioThread.join();

    std::printf("run02_done arm_iterations=%ld audio_blocks=%ld capacity_frames_override=%zu\n", iterations,
                blocksRun.load(), capacityFramesOverride);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s {control|run01|run02} [count]\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "control") {
        RunInjectedRaceControl();
    } else if (mode == "run01") {
        const long batchRequests = argc > 2 ? std::atol(argv[2]) : 10000;
        RunRun01(batchRequests);
    } else if (mode == "run02") {
        const long iterations = argc > 2 ? std::atol(argv[2]) : 1000000;
        const std::size_t capacityFramesOverride = argc > 3 ? static_cast<std::size_t>(std::atol(argv[3])) : 64;
        RunRun02(iterations, capacityFramesOverride);
    } else {
        std::fprintf(stderr, "unknown mode %s\n", mode.c_str());
        return 2;
    }
    return 0;
}
