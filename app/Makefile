# app/Makefile — build rules for the Froggers-on-Sheaf app.
#
# The build-skeleton target ($(APP), built from Main.cpp) proves the include
# path into Sheaf's synth
# core resolves and that an empty translation unit links against Sheaf's
# static core library (build/libsynth.a, built by Sheaf's own Makefile at
# External/Sheaf/projects/synth/Makefile:89). This Makefile does NOT include Sheaf's
# runtime/juce_build.mk — that lives with the out-of-tree app-registration
# hook (FroggersRegistration.hpp) and the launcher build instead
# (see app/build-launcher.sh); nothing here touches the
# sheaf-patch launcher or JUCE headers.
#
# Path anchoring: resolved relative to this Makefile's own location (mirrors
# apps/miniapp/Makefile's APP_DIR pattern), so the build works regardless of
# the caller's cwd.

APP_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SHEAF_SYNTH_DIR := $(APP_DIR)/../External/Sheaf/projects/synth

CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
CPPFLAGS := -I$(SHEAF_SYNTH_DIR)/include

BUILD_DIR := $(APP_DIR)/build
APP_NAME := froggers_skeleton
APP := $(BUILD_DIR)/$(APP_NAME)
APP_SOURCES := $(APP_DIR)/Main.cpp

SHEAF_LIB := $(SHEAF_SYNTH_DIR)/build/libsynth.a

# --- headless FroggersApp test -------------------------------------
# synth_rig::SynthRig lives under Sheaf's tests/ tree; add it to the include
# path so FroggersHeadlessTests.cpp's `#include "support/SynthRig.hpp"`
# resolves (it is a quoted include, so without this it would search relative
# to app/, not Sheaf's tests/ directory).
TEST_SOURCES := $(APP_DIR)/FroggersHeadlessTests.cpp
TEST_BIN := $(BUILD_DIR)/froggers_headless_tests
TEST_CPPFLAGS := $(CPPFLAGS) -I$(SHEAF_SYNTH_DIR)/tests

# --- mono-assumption validation rig (stop-and-ask gate) -------------
# Standalone throwaway validation app (not FroggersApp) exercising one mono
# ParameterGroup + Bank + BankSlot + modulator + depth cell end-to-end.
MONO_VALIDATION_SOURCES := $(APP_DIR)/FroggersMonoValidationTests.cpp
MONO_VALIDATION_BIN := $(BUILD_DIR)/froggers_mono_validation_tests

# --- parameter model (mono, 16-slot banks) tests ---------------
# Structural checks use a bare ParameterManager + FroggersParameterModel;
# scene-blend and shared-Crunchy runtime checks (a stop-and-ask
# gate) drive FroggersApp through synth_rig::SynthRig, same as the
# headless/mono-validation test binaries below.
PARAMETER_MODEL_SOURCES := $(APP_DIR)/FroggersParameterModelTests.cpp
PARAMETER_MODEL_BIN := $(BUILD_DIR)/froggers_parameter_model_tests

# --- modulation slate + drill-in + randomize + default patch --
# Bare ParameterManager + FroggersParameterModel + FroggersModulationSlate,
# same structural-check convention as the parameter-model tests above -- no
# Engine/SynthRig needed (FroggersHeadlessTests.cpp already covers the
# FroggersApp::ProcessBlock/PrepareToPlay wiring end-to-end).
MODULATION_SOURCES := $(APP_DIR)/FroggersModulationTests.cpp
MODULATION_BIN := $(BUILD_DIR)/froggers_modulation_tests

# --- audio routing (parameters -> DSP -> stereo output) tests --------
# Drives FroggersApp through synth_rig::SynthRig, same convention as the
# test binaries above -- ProcessBlock is the thing under
# test, so this needs the real Engine/SynthRig headless harness, not the
# bare-model convention DSP_TEST_BIN/MODULATION_BIN's structural checks use.
# Also covers tempo-change
# tracking, missing-clock-plan safety, and tempo-tracking tests.
AUDIO_ROUTING_SOURCES := $(APP_DIR)/FroggersAudioRoutingTests.cpp
AUDIO_ROUTING_BIN := $(BUILD_DIR)/froggers_audio_routing_tests

# --- VCO scope + transfer-function visualizer tests -------------
# Scope-channel-bound/connected-flag checks need a real FroggersApp
# (constructor wiring + ProcessBlock), so this uses the same
# synth_rig::SynthRig convention as AUDIO_ROUTING_BIN; the transfer-function
# bounded-plot check is a standalone Visualizer/UIState check needing no rig.
VISUALIZER_SOURCES := $(APP_DIR)/FroggersVisualizerTests.cpp
VISUALIZER_BIN := $(BUILD_DIR)/froggers_visualizer_tests

# --- scope AdvanceIndex() bug + regression tests ----------------
# Guards against the missing ScopeWriter::AdvanceIndex() call: a
# reader-non-empty check plus a stronger
# regression guard (published index strictly advances across blocks). Same
# synth_rig::SynthRig convention as AUDIO_ROUTING_BIN/VISUALIZER_BIN -- the
# scope wiring lives in FroggersApp's own constructor/ProcessBlock.
SCOPE_ADVANCE_INDEX_SOURCES := $(APP_DIR)/FroggersScopeAdvanceIndexTests.cpp
SCOPE_ADVANCE_INDEX_BIN := $(BUILD_DIR)/froggers_scope_advance_index_tests

# --- Marbles clock/rate + no-source-level-control tests ---------
# Bare ParameterManager + FroggersParameterModel + FroggersModulationSlate,
# same structural-check convention as MODULATION_BIN -- clock/tempo values
# are driven directly (FroggersModulationSlate::Step/PrepareBlockClock take
# a plain std::optional<double>/std::optional<double>, no real MasterClock
# or AudioBlock needed to exercise the per-source rate logic).
MARBLES_CLOCK_SOURCES := $(APP_DIR)/FroggersMarblesClockTests.cpp
MARBLES_CLOCK_BIN := $(BUILD_DIR)/froggers_marbles_clock_tests

# --- surface layout tests -------------------------------------------
# Layout-bounds/no-overlap/cell-containment checks are pure math
# (FroggersPageLayout/FroggersEncoderGridLayout, no Engine/SynthRig); the
# in-place drill-in swap, encoder-ring-shows-fuegoized-value, BPM
# normal/external-clock-slaved states, and randomize-control-inventory
# checks drive FroggersApp through synth_rig::SynthRig, same convention as
# AUDIO_ROUTING_BIN/VISUALIZER_BIN, dispatching actions through the real
# FroggersUiSurface rather than reaching into the app model directly.
SURFACE_SOURCES := $(APP_DIR)/FroggersSurfaceTests.cpp
SURFACE_BIN := $(BUILD_DIR)/froggers_surface_tests

# --- MIDI catalog tests -----------------------------------------------
# Drives FroggersApp through synth_rig::SynthRig, same convention as
# SURFACE_BIN above -- pushing MessageIn::AppAction/ParamPush on
# Engine::MidiBus() and letting the rig's own per-block message-thread tick
# carry the dispatch to FroggersUiSurface::HandleAction, the same route a
# real controller uses; the catalog's own device defaults are checked
# in-process against the library's per-kind validator.
MIDI_CATALOG_SOURCES := $(APP_DIR)/FroggersMidiCatalogTests.cpp
MIDI_CATALOG_BIN := $(BUILD_DIR)/froggers_midi_catalog_tests

# --- Controllers page real-catalog tests ------------------------------
# Drives synth::MidiConfigViewModel/synth::ControllerWizard directly (no
# Engine/SynthRig, no JUCE) against synth_froggers::FroggersMidiCatalog()'s
# six real device defaults, the catalog Sheaf's own Controllers page tests
# never exercise (they build the wizard registry from an empty
# synth::MidiAppCatalog{}, which only reaches the library's single Twister
# descriptor).
CONTROLLERS_PAGE_SOURCES := $(APP_DIR)/FroggersControllersPageTests.cpp
CONTROLLERS_PAGE_BIN := $(BUILD_DIR)/froggers_controllers_page_tests

# --- mechanically enforce no-JUCE-in-app-core ----------------------
# check_no_juce.cpp includes only Froggers.hpp, compiled with JUCE's module
# directory ON the include path (so "we never pointed at JUCE" can't
# masquerade as passing) and asserts JUCE_MAJOR_VERSION stays undefined.
JUCE_DIR ?= $(HOME)/JUCE
NO_JUCE_CHECK_SRC := $(APP_DIR)/check_no_juce.cpp
NO_JUCE_CHECK := $(BUILD_DIR)/check_no_juce

# --- DSP port (pure, dependency-free, own test suite) ------
# app/dsp/*.hpp are copies of Froggers DSP formulas -- no
# #include of src/, and no Sheaf/JUCE dependency at
# all: the port is deliberately pure DSP. DSP_HEADERS is
# listed as a prerequisite so editing any ported unit rebuilds the suite.
DSP_HEADERS := $(wildcard $(APP_DIR)/dsp/*.hpp)
#
DSP_TEST_SOURCES := $(APP_DIR)/FroggersDspParityTests.cpp
DSP_TEST_BIN := $(BUILD_DIR)/froggers_dsp_parity_tests

.PHONY: all clean check-no-juce check-no-firmware-includes check-catalog-covers-screen-actions check-spec-checks-resolve check-citations-resolve check-modified-requirements-restate-promoted check-no-planning-history check-artifact-symbols-resolve test force

all: check-no-firmware-includes $(APP)

$(BUILD_DIR):
	mkdir -p $@

# Sheaf's own Makefile owns the object list for its static core library;
# delegate to it rather than duplicating that knowledge here. $(MAKE) (not a
# literal "make") is required so the recursive invocation shares the caller's
# job server instead of running unbounded parallel jobs of its own.
force:

$(SHEAF_LIB): force
	$(MAKE) -C $(SHEAF_SYNTH_DIR) build

# The `force` prerequisite makes the delegation run every invocation, so a
# genuine Sheaf change is always picked up. Make re-stats $(SHEAF_LIB) after
# the recipe returns, so $(APP) relinks only when libsynth.a's mtime actually
# moved -- not merely because the sub-make ran.
$(APP): $(APP_SOURCES) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(APP_SOURCES) $(SHEAF_LIB) -o $@

# --- mechanical firmware-tree-include guard --------------------
# Fails the build if any file under app/ (**/*.hpp, **/*.cpp) #includes
# from src/ -- the DSP port is a sanctioned copy, not a shared
# header, so that tree may never be reached from app/ code, however the
# include path is spelled.
check-no-firmware-includes:
	@bash $(APP_DIR)/check_no_firmware_includes.sh "$(APP_DIR)"

# --- microphone usage string drift guard -------------------------------------
# The sentence is declared once per platform because the two build systems
# share no substrate; this fails when they stop agreeing. See the script's own
# header for why the duplication cannot be collapsed.
check-microphone-usage:
	@bash $(APP_DIR)/check_microphone_usage_strings.sh "$(APP_DIR)"

# --- parameter reference drift guard ------------------------------------------
# MANUAL.md and QUICK_DICT.md restate FroggersParameters.hpp's bank tables and
# FroggersUiSurface.hpp's rendered labels in prose; this fails when either
# document's name, label or slot for a parameter drifts from the source. See
# the script's own header for the entry grammar it parses.
check-docs-match-parameter-table:
	@python3 $(APP_DIR)/check_docs_match_parameter_table.py "$(APP_DIR)"

# A spec's `Check:` line is evidence that a requirement is backed by something
# real. It is also the cheapest false claim in the repository, because nothing
# else reads prose -- one ADDED requirement shipped naming a test that existed
# nowhere. This resolves every such name against the tree's real test cases and
# files, and accepts `none` / `operator step` so an honest gap stays sayable.
check-spec-checks-resolve:
	@python3 $(APP_DIR)/check_spec_checks_resolve.py "$(APP_DIR)"

# A comment that cites a path and line is making a claim about where something
# lives. This fails when the path exists nowhere, and when a citation into this
# tree still carries a line number -- line numbers into a tree under edit are
# wrong by the next commit, and one change left 14 of 24 stale. Citations into
# External/Sheaf and src/, and any citation carrying a commit pin, are out of
# scope by construction; the script's header records why.
check-citations-resolve:
	@python3 $(APP_DIR)/check_citations_resolve.py "$(APP_DIR)"

# A MODIFIED requirement replaces the promoted requirement's whole body, so a
# clause the delta fails to restate is deleted at archive time and nothing says
# so: openspec's strict validator counts neither scenarios nor clauses. One
# delta dropped "anchored so that silence-in still produces silence-out" from a
# scenario whose title and clause count both matched, and it passed the
# validator, this directory's other gates, and two audits by contexts that had
# not written it. This fails when a restated scenario loses a promoted clause
# that the delta does not declare -- and declaring one requires saying what the
# replacement still keeps, because a declaration that only says "changed" tells
# the gate to stop reading exactly where the defect lives.
check-modified-requirements-restate-promoted:
	@python3 $(APP_DIR)/check_modified_requirements_restate_promoted.py "$(APP_DIR)"

# Comments explain behaviour, never which task or numbered item produced them:
# a reader has never seen the planning document and cannot check anything
# against it. The rule predates this check, was acted on twice, and was
# re-violated twice.
check-no-planning-history:
	@python3 $(APP_DIR)/check_no_planning_history.py "$(APP_DIR)"

# A change's tasks.md and proposal.md say what the code is and what it will
# become, and nothing else reads them: the citation gate reads comments and the
# spec gate reads `Check:` lines. This resolves the names in them against the
# tree -- paths, qualified names against the scope that declares the member,
# constants and test cases -- and accepts a name the change itself declares it
# is about to create. It resolves names only; a false statement about a real
# name passes.
check-artifact-symbols-resolve:
	@python3 $(APP_DIR)/check_artifact_symbols_resolve.py "$(APP_DIR)"

# --- no-JUCE-in-app-core target ---------------------------------------------
$(NO_JUCE_CHECK): $(NO_JUCE_CHECK_SRC) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(DSP_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -I$(JUCE_DIR)/modules $(CXXFLAGS) $(NO_JUCE_CHECK_SRC) -o $@

check-no-juce: $(NO_JUCE_CHECK)
	@echo "check-no-juce: OK - Froggers.hpp (app core) resolves no JUCE header"

# --- headless test target -------------------------------------------------------------
$(TEST_BIN): $(TEST_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(TEST_SOURCES) $(SHEAF_LIB) -o $@

# --- mono-validation target -------------------------------------------------------------
$(MONO_VALIDATION_BIN): $(MONO_VALIDATION_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(MONO_VALIDATION_SOURCES) $(SHEAF_LIB) -o $@

# --- parameter-model target ----------------------------------------------------------
$(PARAMETER_MODEL_BIN): $(PARAMETER_MODEL_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersParameters.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(PARAMETER_MODEL_SOURCES) $(SHEAF_LIB) -o $@

# --- modulation-slate target ------------------------------------------------
$(MODULATION_BIN): $(MODULATION_SOURCES) $(APP_DIR)/FroggersModulation.hpp $(APP_DIR)/FroggersParameters.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(MODULATION_SOURCES) $(SHEAF_LIB) -o $@

# --- audio-routing target ------------------------------------------------------------
$(AUDIO_ROUTING_BIN): $(AUDIO_ROUTING_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersModulation.hpp $(APP_DIR)/FroggersParameters.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(AUDIO_ROUTING_SOURCES) $(SHEAF_LIB) -o $@

# --- visualizer target ---------------------------------------------------------
$(VISUALIZER_BIN): $(VISUALIZER_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersModulation.hpp $(APP_DIR)/FroggersParameters.hpp $(APP_DIR)/FroggersTransferFunctionVisualizer.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(VISUALIZER_SOURCES) $(SHEAF_LIB) -o $@

# --- scope-advance-index target ----------------------------------------------------------
$(SCOPE_ADVANCE_INDEX_BIN): $(SCOPE_ADVANCE_INDEX_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersModulation.hpp $(APP_DIR)/FroggersParameters.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(SCOPE_ADVANCE_INDEX_SOURCES) $(SHEAF_LIB) -o $@

# --- marbles-clock target ----------------------------------------------------------
$(MARBLES_CLOCK_BIN): $(MARBLES_CLOCK_SOURCES) $(APP_DIR)/FroggersModulation.hpp $(APP_DIR)/FroggersParameters.hpp $(APP_DIR)/FroggersRandomShVisualizer.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(MARBLES_CLOCK_SOURCES) $(SHEAF_LIB) -o $@

# --- DSP parity suite target -----------------------------------------
# app/dsp/*.hpp's usual convention: NO Sheaf/JUCE dependency and no -I into
# Sheaf at all (pure, standalone C++). The VCO scope
# UIState and ResonantBump/Comb transfer-function UIState
# deliberately widen this by exactly one Sheaf -I: both mirror Sheaf's own
# DSP-struct-embeds-UIState convention (DspOscillators.hpp/DspFilters.hpp),
# which needs synth/DspScope.hpp, synth/Color.hpp, synth/AtomicColor.hpp,
# and synth/DspTransferFunction.hpp -- every one of them header-only with no
# further transitive Sheaf includes and no link-time dependency, so this
# still needs no SHEAF_LIB. See app/dsp/Vco.hpp's and app/dsp/FilterFx.hpp's
# own header comments for the full justification.
$(DSP_TEST_BIN): $(DSP_TEST_SOURCES) $(DSP_HEADERS) | $(BUILD_DIR)
	$(CXX) -I$(APP_DIR) $(CPPFLAGS) $(CXXFLAGS) $(DSP_TEST_SOURCES) -o $@

# --- stop-flush-repro target -----------------------------------------------------------

# --- surface target -----------------------------------------------------------
$(SURFACE_BIN): $(SURFACE_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersParameters.hpp $(APP_DIR)/FroggersModulation.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(SURFACE_SOURCES) $(SHEAF_LIB) -o $@

# --- MIDI catalog target -------------------------------------------------------
$(MIDI_CATALOG_BIN): $(MIDI_CATALOG_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersMidiCatalog.hpp $(APP_DIR)/FroggersParameters.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(MIDI_CATALOG_SOURCES) $(SHEAF_LIB) -o $@

# --- Controllers page real-catalog target -----------------------------------
$(CONTROLLERS_PAGE_BIN): $(CONTROLLERS_PAGE_SOURCES) $(APP_DIR)/Froggers.hpp $(APP_DIR)/FroggersAppCore.hpp $(APP_DIR)/FroggersUiSurface.hpp $(APP_DIR)/FroggersMidiCatalog.hpp $(APP_DIR)/FroggersParameters.hpp $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)
	$(CXX) $(TEST_CPPFLAGS) $(CXXFLAGS) $(CONTROLLERS_PAGE_SOURCES) $(SHEAF_LIB) -o $@

# --- catalog-covers-every-screen-action check -----------------------------
# Every FroggersActions constant (FroggersUiSurface.hpp) must be both
# offered by the MIDI catalog and routed by HandleAction, except the five
# that are not MIDI-mappable actions:
#   kSceneBlend      -- offered as the library's own analog Scene Blend kind.
#   kEncoderPress    -- the catalog's own encoderPressAction, not a listed action.
#   kEncoderDrag     -- the screen's mouse route into the library's ParamIncDec.
#   kInputSelect     -- the plugin host's input picker, not offered.
#   kViewportNarrow  -- a browser shell flag, not a control.
check-catalog-covers-screen-actions:
	@bash $(APP_DIR)/check_catalog_covers_screen_actions.sh "$(APP_DIR)" \
	    kSceneBlend kEncoderPress kEncoderDrag kInputSelect kViewportNarrow

test: check-no-juce check-no-firmware-includes check-microphone-usage check-catalog-covers-screen-actions check-docs-match-parameter-table check-spec-checks-resolve check-citations-resolve check-modified-requirements-restate-promoted check-no-planning-history check-artifact-symbols-resolve $(TEST_BIN) $(MONO_VALIDATION_BIN) $(DSP_TEST_BIN) $(PARAMETER_MODEL_BIN) $(MODULATION_BIN) $(AUDIO_ROUTING_BIN) $(VISUALIZER_BIN) $(SCOPE_ADVANCE_INDEX_BIN) $(MARBLES_CLOCK_BIN) $(SURFACE_BIN) $(MIDI_CATALOG_BIN) $(CONTROLLERS_PAGE_BIN)
	$(TEST_BIN)
	$(MONO_VALIDATION_BIN)
	$(DSP_TEST_BIN)
	$(PARAMETER_MODEL_BIN)
	$(MODULATION_BIN)
	$(AUDIO_ROUTING_BIN)
	$(VISUALIZER_BIN)
	$(SCOPE_ADVANCE_INDEX_BIN)
	$(MARBLES_CLOCK_BIN)
	$(SURFACE_BIN)
	$(MIDI_CATALOG_BIN)
	$(CONTROLLERS_PAGE_BIN)

clean:
	rm -rf $(BUILD_DIR)
