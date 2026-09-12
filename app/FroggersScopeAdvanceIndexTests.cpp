// FroggersScopeAdvanceIndexTests.cpp -- guards Sheaf's ScopeWriter's two-feed
// contract for the VCO oscilloscopes.
//
// Sheaf's ScopeWriter needs TWO feeds per sample: Write() to store the
// value (called by FroggersAppCore::RouteAudioSample(), once per VCO, on
// the post-gate signal) and
// AdvanceIndex() to move the ring-buffer cursor (`index_ += amount`,
// External/Sheaf/projects/synth/include/synth/DspScope.hpp:126-128). Once,
// Froggers called Write() and Publish() but never AdvanceIndex(). index_
// stayed 0, so every Write()
// overwrote slot 0 and Publish() always republished index 0; ScopeReader's
// no-marker fallback then computed endIndex_ == startIndex_ == 0
// (External/Sheaf/projects/synth/include/synth/DspScope.hpp:266-267), making Empty() permanently true (:270,298), so
// BuildScopePolylines returned early and the scope panel drew only
// background fill + midline.
//
// vco_scope_reader_non_empty_after_one_block_with_transport_running
// is the failing-test-first check: a real FroggersApp (scope wiring happens
// in its own constructor) driven through one block with the transport
// running must leave the reader non-Empty().
// vco_scope_published_index_advances_across_successive_blocks is the
// regression guard: non-emptiness alone is a weaker assertion a future
// refactor could satisfy by accident (e.g. any single nonzero write), so
// this instead asserts the PUBLISHED index itself strictly advances
// between successive blocks -- which can only happen if AdvanceIndex() (or
// an equivalent per-sample cursor move) runs every block.
//
// Same JUCE-free headless harness (synth_rig::SynthRig<FroggersApp>) and
// per-file test-registry convention as FroggersVisualizerTests.cpp/
// FroggersAudioRoutingTests.cpp; wired into app/Makefile's `test` target.

#include "Froggers.hpp"
#include "support/SynthRig.hpp"
#include "synth/DspScope.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers scope AdvanceIndex tests must not see JUCE headers -- the app core must stay JUCE-free"
#endif

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Register {
    Register(const char* name, void (*fn)()) {
        Registry().push_back({name, fn});
    }
};

#define TEST_CASE(name)     \
    void name();            \
    Register reg_##name(#name, &name); \
    void name()

#define REQUIRE_TRUE(expr)                                                 \
    do {                                                                   \
        if (!(expr)) {                                                    \
            std::ostringstream oss;                                       \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #expr; \
            throw std::runtime_error(oss.str());                          \
        }                                                                  \
    } while (false)

synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-scope-advance-index-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

using Rig = synth_rig::SynthRig<synth_froggers::FroggersApp>;

// -----------------------------------------------------------------------
// Guards against the missing AdvanceIndex()
// call reappearing: without it, ScopeReader is permanently Empty() regardless of
// how many blocks run, because index_ (and therefore publishedIndex_)
// never leaves 0.
// -----------------------------------------------------------------------
TEST_CASE(vco_scope_reader_non_empty_after_one_block_with_transport_running) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("reader_non_empty"));

    rig.StartAt(0);
    rig.RunBlocks(1);

    const auto& state = rig.Application().VcoScopeUiState(0);
    REQUIRE_TRUE(state.connected.load());
    const synth::ScopeWriter* const writer = state.scope.load();
    REQUIRE_TRUE(writer != nullptr);

    const synth::ScopeReader reader(writer, state.scopeChannel.load(), /*numXSamples=*/64);
    REQUIRE_TRUE(!reader.Empty());
}

// -----------------------------------------------------------------------
// Fails if AdvanceIndex() is ever removed
// again. Asserts the PUBLISHED scope index strictly advances between
// successive blocks, not merely that output is non-empty (a weaker
// assertion a future refactor could satisfy accidentally -- e.g. a single
// stray nonzero Write() to slot 0 would already make a "non-empty" check
// pass without index_ ever moving).
// -----------------------------------------------------------------------
TEST_CASE(vco_scope_published_index_advances_across_successive_blocks) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("published_index_advances"));

    rig.StartAt(0);
    rig.RunBlocks(1);

    const synth::ScopeWriter* const writer = rig.Application().VcoScopeUiState(0).scope.load();
    REQUIRE_TRUE(writer != nullptr);
    const std::size_t firstPublishedIndex = writer->PublishedIndex();

    rig.RunBlocks(1);
    const std::size_t secondPublishedIndex = writer->PublishedIndex();

    REQUIRE_TRUE(secondPublishedIndex > firstPublishedIndex);
}

}  // namespace

int main() {
    int failed = 0;
    for (const auto& test : Registry()) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
        }
    }
    return failed == 0 ? 0 : 1;
}
