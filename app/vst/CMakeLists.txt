# app/vst/CMakeLists.txt -- the VST/AU plugin skeleton. ALL JUCE code lives
# under app/vst/;
# app/'s JUCE-free core (Froggers.hpp et al.) and its own build
# (app/Makefile, check_no_juce.cpp) are untouched by this file and never
# depend on it.
#
# The plugin is built with juce_add_plugin (FORMATS VST3 AU, IS_SYNTH TRUE,
# COPY_PLUGIN_AFTER_BUILD FALSE), and:
#   - adds JUCE via add_subdirectory($ENV{HOME}/JUCE ...) from a local
#     checkout rather than FetchContent;
#   - targets the app core (app/Froggers.hpp,
#     synth_froggers::FroggersApp) through Sheaf's synth::Engine<App>;
#   - declares no input bus by default -- see BusesProperties below;
#   - builds Sheaf's JUCE-free static core through a custom step (below),
#     since that core comes from Sheaf's own Makefile rather than from
#     sources this file compiles.
#
# Build (binding): -j2 max, always, nice'd -- this machine
# locks up above -j2.
#   cmake -S app/vst -B app/vst/build
#   cmake --build app/vst/build --config Release -- -j2

cmake_minimum_required(VERSION 3.22)
# JUCE's own top-level CMakeLists.txt declares `LANGUAGES C CXX` (some of its
# dependencies -- e.g. VST3 SDK helper code -- compile .c files); omitting C
# here left CMAKE_C_COMPILE_OBJECT unset and the generate step failed.
project(FroggersVst VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target")

set(FROGGERS_JUCE_DIR "$ENV{HOME}/JUCE" CACHE PATH
    "Path to a local JUCE checkout, added via add_subdirectory (not FetchContent)")
if(NOT EXISTS "${FROGGERS_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${FROGGERS_JUCE_DIR} (set -DFROGGERS_JUCE_DIR=... "
                         "or check out JUCE there) -- see app/vst/CMakeLists.txt's own header comment")
endif()
add_subdirectory("${FROGGERS_JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SHEAF_SYNTH_DIR "${APP_DIR}/../External/Sheaf/projects/synth")
set(SHEAF_LIB "${SHEAF_SYNTH_DIR}/build/libsynth.a")

# --- Sheaf's JUCE-free static core -----------------------------------------
# app/Makefile's own SHEAF_LIB rule delegates to Sheaf's Makefile the same
# way (`$(MAKE) -C $(SHEAF_SYNTH_DIR) build`), every time, and lets Sheaf's
# own build decide whether anything actually needs recompiling.
#
# This USED to be an
# execute_process() at CONFIGURE time, specifically to avoid ever running
# concurrently with the `cmake --build ... -- -j2` step below and multiplying
# this machine's effective parallelism past the hard -j2 cap. The problem
# that traded away: `cmake --build` alone (no reconfigure) could link a
# STALE libsynth.a -- a Sheaf source edit with no app/vst/ reconfigure in
# between would never be seen. Fixed here with a build-time
# add_custom_target instead, WITHOUT reintroducing the over-parallelism risk:
# add_dependencies() below makes FroggersVst (and therefore everything that
# links or depends on it, including FroggersVstSmokeTest) wait on this
# target before compiling ANYTHING, so the outer build graph's second -j2
# slot has no other job available to run concurrently with the nested
# `nice make -j2` while it executes -- the two -j2's are serialized by the
# dependency edge, not by configure-vs-build timing. (add_custom_target's
# COMMAND always reruns when the target is built, exactly like the old
# execute_process did every configure -- it is Sheaf's own Makefile that
# decides per invocation whether anything is actually stale, same
# "every time, let Sheaf decide" contract as app/Makefile's SHEAF_LIB rule.)
#
# On Windows, none of the above applies and the archive cannot be used at all.
# Sheaf's Makefile drives `CXX ?= clang++` with GCC-style flags, outside
# CMake's compiler selection, so `make` there produces objects from a
# different toolchain than the one linking them -- and MSVC's linker cannot
# resolve MSVC-mangled references against them. The symbols are genuinely in
# the archive and simply unusable, which surfaces as LNK2019 on every Sheaf
# symbol the plugin needs. app/standalone/CMakeLists.txt hit exactly this and
# compiles those sources into its own target instead; the same is done here,
# so one compiler handles the whole link.
#
# Globbed, not listed: Sheaf's Makefile builds exactly src/*.cpp, so a
# hand-copied list would be a second definition of that set and would drift
# the day Sheaf gains a source -- macOS would keep working and Windows would
# fail at the link again. CONFIGURE_DEPENDS makes a new source reconfigure.
if(WIN32)
    file(GLOB FROGGERS_SHEAF_SYNTH_SOURCES CONFIGURE_DEPENDS "${SHEAF_SYNTH_DIR}/src/*.cpp")
    if(NOT FROGGERS_SHEAF_SYNTH_SOURCES)
        message(FATAL_ERROR "no Sheaf synth sources found in ${SHEAF_SYNTH_DIR}/src")
    endif()
else()
    find_program(FROGGERS_NICE_EXE nice)
    if(FROGGERS_NICE_EXE)
        set(FROGGERS_SHEAF_LIB_BUILD_COMMAND "${FROGGERS_NICE_EXE}" -n 10 make -j2 build)
    else()
        set(FROGGERS_SHEAF_LIB_BUILD_COMMAND make -j2 build)
    endif()
    add_custom_target(FroggersSheafLibsynth ALL
        COMMAND ${FROGGERS_SHEAF_LIB_BUILD_COMMAND}
        WORKING_DIRECTORY "${SHEAF_SYNTH_DIR}"
        BYPRODUCTS "${SHEAF_LIB}"
        COMMENT "app/vst: building Sheaf's libsynth.a (nice make -j2 build in ${SHEAF_SYNTH_DIR})"
        VERBATIM
    )
endif()

# Instrument/generator identity must stay IS_SYNTH TRUE. Named
# FROGGERS_VST_PLUGIN_IS_SYNTH rather than FROGGERS_VST_IS_SYNTH so this
# CMake-script-only variable is not confused with a C++ preprocessor macro
# of the near-identical name that once existed for unrelated targets (
# namespace, no actual collision -- this is a readability-only rename).
set(FROGGERS_VST_PLUGIN_IS_SYNTH TRUE)
if(NOT FROGGERS_VST_PLUGIN_IS_SYNTH)
    message(FATAL_ERROR "FroggersVst must build with IS_SYNTH TRUE -- it is an instrument/generator plugin, not an effect")
endif()

# VST3 is cross-platform; the Audio Unit exists only on macOS, so asking for it
# anywhere else is a build that cannot succeed rather than a plugin that ships
# one format fewer. The desktop application already builds for Windows, so the
# plugin's macOS-only format list was a scoping decision, not a constraint.
set(FROGGERS_VST_FORMATS VST3)
if(APPLE)
    list(APPEND FROGGERS_VST_FORMATS AU)
endif()

juce_add_plugin(FroggersVst
    PRODUCT_NAME "Frogg3rs"
    COMPANY_NAME "frogg3rs"
    BUNDLE_ID "dev.sheaf.Frogg3rs.vst"
    PLUGIN_MANUFACTURER_CODE Shef
    PLUGIN_CODE Fr3g
    FORMATS ${FROGGERS_VST_FORMATS}
    IS_SYNTH TRUE
    # "a synth that declares MIDI input accepted is fine; no
    # note wiring" -- FroggersPluginProcessor::acceptsMidi() agrees (TRUE),
    # and processBlock() ignores the MIDI buffer entirely.
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# FroggersVst (the shared-code target every format
# wrapper links, and what FroggersVstSmokeTest itself links) cannot start
# compiling until libsynth.a has been (re)built -- see
# FroggersSheafLibsynth's own comment above for why this also keeps total
# concurrent build parallelism at the -j2 cap.
if(NOT WIN32)
    add_dependencies(FroggersVst FroggersSheafLibsynth)
endif()

# Operator documentation ships with the plugin (froggers-sheaf-runtime-app
# spec, "Operator documentation ships with the app"): the manual and quick
# dictionary are embedded from the repository's single copy at build time,
# so no second checked-in copy exists to drift -- the two docs are copied
# straight into each
# built plugin bundle's Contents/Resources after the format target links,
# since juce_add_plugin has no built-in "bundle this file" option the way
# juce_add_gui_app's ICON_BIG has for the standalone app.
get_filename_component(FROGGERS_REPO_ROOT "${APP_DIR}/.." ABSOLUTE)
set(FROGGERS_DOC_SOURCES
    "${FROGGERS_REPO_ROOT}/MANUAL.md"
    "${FROGGERS_REPO_ROOT}/QUICK_DICT.md"
)
# The two MIDI Fighter Twister diagrams MANUAL.md references, copied
# alongside it below at the same relative path (assets/manual/).
set(FROGGERS_DOC_IMAGE_SOURCES
    "${FROGGERS_REPO_ROOT}/assets/manual/twister-preset.png"
    "${FROGGERS_REPO_ROOT}/assets/manual/twister-preset-shift.png"
)
# Where "inside the plugin" is differs by platform, but the requirement does
# not: operator documentation ships with the plugin everywhere. A macOS bundle
# has Contents/Resources and CMake can name it directly. A Windows VST3 is also
# a directory bundle -- Frogg3rs.vst3/Contents/x86_64-win/Frogg3rs.vst3 -- whose
# sibling Contents/Resources is the same place in that layout, reached from the
# binary's own directory because $<TARGET_BUNDLE_CONTENT_DIR> is a macOS-only
# generator expression and expands to nothing off it.
#
# One definition, because the copy below and the check further down must name
# the same directory. Computed twice they can drift, and the drift is silent:
# the check passes against a directory the copy never wrote.
function(froggers_vst_doc_dir OUT_VAR FORMAT)
    if(APPLE)
        set(${OUT_VAR} "$<TARGET_BUNDLE_CONTENT_DIR:FroggersVst_${FORMAT}>/Resources" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "$<TARGET_FILE_DIR:FroggersVst_${FORMAT}>/../Resources" PARENT_SCOPE)
    endif()
endfunction()

foreach(FROGGERS_VST_FORMAT ${FROGGERS_VST_FORMATS})
    if(TARGET FroggersVst_${FROGGERS_VST_FORMAT})
        froggers_vst_doc_dir(FROGGERS_VST_DOC_DEST ${FROGGERS_VST_FORMAT})
        add_custom_command(TARGET FroggersVst_${FROGGERS_VST_FORMAT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${FROGGERS_VST_DOC_DEST}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FROGGERS_DOC_SOURCES}
                "${FROGGERS_VST_DOC_DEST}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${FROGGERS_VST_DOC_DEST}/assets/manual"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FROGGERS_DOC_IMAGE_SOURCES}
                "${FROGGERS_VST_DOC_DEST}/assets/manual"
            COMMENT "app/vst: bundling MANUAL.md/QUICK_DICT.md and its MIDI Fighter Twister diagrams into ${FROGGERS_VST_FORMAT} Resources"
            VERBATIM
        )
    endif()
endforeach()

# Signing must be the LAST step for both formats, after the doc-copy loop
# above -- not merely present. JUCE's own _juce_adhoc_sign (JUCEUtils.cmake)
# is attached to the VST3 target as part of juce_add_plugin() above, which
# runs BEFORE this file's own doc-copy POST_BUILD commands were registered,
# so JUCE's signature seals the VST3 bundle before MANUAL.md/QUICK_DICT.md
# land in it -- the same "signed before assembly finished" defect the app
# bundle had. AU gets no JUCE signature at all: _juce_adhoc_sign is wired to
# VST3 and LV2 targets only (JUCEUtils.cmake:1001,1237), never to AU, so
# Frogg3rs.component ships ad-hoc-linker-signed with no sealed resources.
# Both are fixed the same way: sign again, last, after everything is in the
# bundle, and verify before the build is allowed to succeed.
# macOS only. `codesign` does not exist on Windows, and no Authenticode
# certificate exists for this project, so the Windows VST3 ships unsigned
# exactly as the Windows standalone already does. Inventing a signing step
# here would be a business decision wearing an engineering one.
if(APPLE)
    foreach(FROGGERS_VST_FORMAT ${FROGGERS_VST_FORMATS})
        if(TARGET FroggersVst_${FROGGERS_VST_FORMAT})
            add_custom_command(TARGET FroggersVst_${FROGGERS_VST_FORMAT} POST_BUILD
                COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:FroggersVst_${FROGGERS_VST_FORMAT}>"
                COMMAND codesign --verify --deep --strict "$<TARGET_BUNDLE_DIR:FroggersVst_${FROGGERS_VST_FORMAT}>"
                COMMENT "app/vst: signing and verifying ${FROGGERS_VST_FORMAT} bundle after assembly"
                VERBATIM
            )
        endif()
    endforeach()
endif()

target_sources(FroggersVst PRIVATE
    # Empty everywhere but Windows, where these replace the archive above.
    ${FROGGERS_SHEAF_SYNTH_SOURCES}
    FroggersPluginProcessor.cpp
    # The real portable-surface editor. Needs
    # SHEAF_SYNTH_DIR/juce on the include path (below) for
    # PortableJuceBackend.hpp -- runtime/ and include/ alone (the
    # original set) never pulled that directory in.
    FroggersPluginEditor.cpp
    # Only the tiny data-root helper (juce_core dependency, no
    # window/device/MIDI-connection machinery) -- see
    # FroggersPluginProcessor.hpp's header comment on why reusing this one
    # file is not "launcher-code duplication."
    "${SHEAF_SYNTH_DIR}/runtime/HostDataPaths.cpp"
)

target_include_directories(FroggersVst PRIVATE
    "${APP_DIR}"
    "${SHEAF_SYNTH_DIR}/include"
    "${SHEAF_SYNTH_DIR}/runtime"
    # synth_juce::PortableComponent (PortableJuceBackend.hpp), the
    # render-host seam FroggersPluginEditor.cpp builds on -- see that file's
    # own header comment for the full trace.
    "${SHEAF_SYNTH_DIR}/juce"
)

target_compile_features(FroggersVst PUBLIC cxx_std_20)

target_link_libraries(FroggersVst
    PUBLIC
        juce::juce_audio_utils
        juce::juce_audio_processors
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Not on Windows: there the sources above are compiled into the target and no
# archive exists to link.
if(NOT WIN32)
    target_link_libraries(FroggersVst PRIVATE "${SHEAF_LIB}")
endif()

target_compile_definitions(FroggersVst PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # This plugin has never shipped a VST2 build and FORMATS never requests
    # one -- JUCE's juce_audio_plugin_client_VST3.cpp hard-errors by default
    # (JUCE_VST3_CAN_REPLACE_VST2, on unless disabled) warning about VST2/VST3
    # parameter-ID migration that cannot apply here. Per that file's own
    # comment: "If you have released neither a VST2 or VST3 version of the
    # plugin, consider only releasing a VST3 version and disabling
    # JUCE_VST3_CAN_REPLACE_VST2."
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# MSVC does not define M_PI unless _USE_MATH_DEFINES is set before <cmath>,
# and the app's DSP headers use it in twenty-two places. Same fix
# app/standalone/CMakeLists.txt already carries for the Windows standalone,
# which is the only Windows build here that has ever gone green. PUBLIC rather
# than that file's PRIVATE because the format targets and the three CTest
# binaries all compile these same headers through this target.
if(MSVC)
    target_compile_definitions(FroggersVst PUBLIC _USE_MATH_DEFINES)
endif()

# --- processBlock smoke test (CTest) -----------------------------------
# The format list is a platform claim, so it is asserted at configure time
# rather than left to a build failure on a runner nobody watches. VST3 exists
# everywhere; AU exists only where JUCE can build it.
if(APPLE)
    if(NOT "AU" IN_LIST FROGGERS_VST_FORMATS)
        message(FATAL_ERROR "the Audio Unit must be requested on macOS; FORMATS is '${FROGGERS_VST_FORMATS}'")
    endif()
elseif("AU" IN_LIST FROGGERS_VST_FORMATS)
    message(FATAL_ERROR "the Audio Unit cannot build off macOS; FORMATS is '${FROGGERS_VST_FORMATS}'")
endif()
if(NOT "VST3" IN_LIST FROGGERS_VST_FORMATS)
    message(FATAL_ERROR "VST3 ships on every platform; FORMATS is '${FROGGERS_VST_FORMATS}'")
endif()

enable_testing()

add_executable(FroggersVstSmokeTest FroggersVstSmokeTest.cpp)
target_include_directories(FroggersVstSmokeTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${APP_DIR}"
    "${SHEAF_SYNTH_DIR}/include"
    "${SHEAF_SYNTH_DIR}/runtime"
)
target_compile_features(FroggersVstSmokeTest PRIVATE cxx_std_20)
# Links the plugin's own shared-code target (juce_add_plugin's FroggersVst
# static library, the same object code every format wrapper links) so the
# test drives the REAL compiled FroggersPluginProcessor, not a second copy.
target_link_libraries(FroggersVstSmokeTest PRIVATE FroggersVst)

add_test(NAME FroggersVstSmokeTest COMMAND FroggersVstSmokeTest)

# --- DAW-external transport/tempo/surface-row tests (CTest) -----------
# Same "drive the real compiled processor" shape as FroggersVstSmokeTest
# above (links FroggersVst, the shared-code target every format wrapper
# links), plus app/FroggersUiSurface.hpp directly for the plugin-mode
# surface-row assertions -- that header is JUCE-free itself, but
# this test binary already links JUCE transitively via FroggersVst, so no
# separate target is needed for it.
add_executable(FroggersVstHostTests FroggersVstHostTests.cpp)
target_include_directories(FroggersVstHostTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${APP_DIR}"
    "${SHEAF_SYNTH_DIR}/include"
    "${SHEAF_SYNTH_DIR}/runtime"
)
target_compile_features(FroggersVstHostTests PRIVATE cxx_std_20)
target_link_libraries(FroggersVstHostTests PRIVATE FroggersVst)

add_test(NAME FroggersVstHostTests COMMAND FroggersVstHostTests)

# --- Operator documentation ships inside the plugin (CTest, every format) ---
# The requirement is platform-neutral, so the check is too: the doc-copy loop
# writes to a macOS bundle's Contents/Resources and to a Windows VST3's
# sibling Resources directory, and only inspecting the BUILT artifact can say
# whether either landed. Reading the CMake would only restate the edit.
foreach(FROGGERS_VST_FORMAT ${FROGGERS_VST_FORMATS})
    if(TARGET FroggersVst_${FROGGERS_VST_FORMAT})
        froggers_vst_doc_dir(FROGGERS_VST_DOC_CHECK_DIR ${FROGGERS_VST_FORMAT})
        add_test(NAME FroggersVstDocsBundled_${FROGGERS_VST_FORMAT}
            COMMAND ${CMAKE_COMMAND}
                -DFROGGERS_VST_DOC_DIR=${FROGGERS_VST_DOC_CHECK_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckBundledDocs.cmake")
    endif()
endforeach()

# --- Editor construction/destruction (CTest, own binary) ---------------
# Deliberately ISOLATED from FroggersVstHostTests above: this is the one
# place in the app/vst/ suite that constructs a real juce::Component
# (FroggersPluginEditor), and a headless CTest process has no guarantee a
# JUCE GUI Component can be constructed/shown safely -- see
# FroggersVstEditorTest.cpp's own header comment. Keeping this experiment in
# its own process means a hard crash here reports as exactly one failed
# CTest entry and never takes FroggersVstHostTests's other (already solid,
# GUI-free) cases down with it.
add_executable(FroggersVstEditorTest FroggersVstEditorTest.cpp)
target_include_directories(FroggersVstEditorTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${APP_DIR}"
    "${SHEAF_SYNTH_DIR}/include"
    "${SHEAF_SYNTH_DIR}/runtime"
    "${SHEAF_SYNTH_DIR}/juce"
)
target_compile_features(FroggersVstEditorTest PRIVATE cxx_std_20)
target_link_libraries(FroggersVstEditorTest PRIVATE FroggersVst)

add_test(NAME FroggersVstEditorTest COMMAND FroggersVstEditorTest)

# The three test binaries construct FroggersPluginProcessor as a local, and it
# embeds synth::Engine<FroggersApp>: 1,204,864 bytes, measured. A Windows
# thread gets 1 MB by default and macOS gives its main thread 8, so the same
# object that is comfortable there overflows the stack on the first
# construction -- all three exited 0xC00000FD, STATUS_STACK_OVERFLOW, before
# reaching their own first write.
#
# The stack is raised rather than the fifty-five declarations rewritten: one
# mechanism instead of fifty-five copies of one edit, and 8 MB is not a
# tuned number but the size macOS already hands these same binaries. A host
# never does this -- it allocates the processor itself -- so the fragility
# stops at the test executables.
if(MSVC)
    foreach(FROGGERS_VST_TEST_BIN FroggersVstSmokeTest FroggersVstHostTests FroggersVstEditorTest)
        target_link_options(${FROGGERS_VST_TEST_BIN} PRIVATE /STACK:8388608)
    endforeach()
endif()

