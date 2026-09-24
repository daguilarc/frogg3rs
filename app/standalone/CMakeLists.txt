# app/standalone/CMakeLists.txt -- the Windows standalone build. macOS keeps
# building through app/build-launcher.sh, unchanged; this file exists
# because that path runs External/Sheaf's juce_build.mk under MinGW g++ on
# Windows, and MinGW g++ cannot compile JUCE's own headers there --
# `memcpy`/`memset` missing from `std::` in juce_UMPFactory.h/juce_Memory.h,
# and `pointer_sized_int` resolving to 32-bit `int` on a 64-bit MinGW
# target in juce_FloatVectorOperations.cpp/juce_HashMap.h. That is JUCE's
# own vendored code, not ours to patch, failing under a compiler JUCE is
# not primarily tested against. CMake's default Windows generator resolves
# to MSVC instead, which is what both the deleted v1 desktop/CMakeLists.txt
# and app/vst/CMakeLists.txt already build against successfully.
#
# This produces the SAME application build-launcher.sh does -- Sheaf's
# runtime shell launching app/FroggersMain.cpp directly, not JUCE's own
# Standalone plugin wrapper (which substitutes JUCE's audio-settings dialog
# for Sheaf's own Audio page, and would make the two platforms ship
# different applications). The synth core is reused exactly the way
# app/vst/CMakeLists.txt already reuses it: Sheaf's own Makefile builds
# libsynth.a, linked here, not re-listed.
#
# Build (binding): -j2 max, always, nice'd -- this machine locks up above -j2.
#   cmake -S app/standalone -B app/standalone/build
#   cmake --build app/standalone/build --config Release -- -j2

cmake_minimum_required(VERSION 3.22)
project(FroggersStandalone VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# Identical mechanism to app/vst/CMakeLists.txt's own FroggersSheafLibsynth
# target -- see that file's header comment for why this is a build-time
# add_custom_target (not a configure-time execute_process) and why
# add_dependencies() below is what keeps this machine's two nested -j2's
# serialized rather than multiplying.
# Sheaf's synth core reaches this target two different ways, because the
# toolchains do.
#
# Everywhere except Windows: build libsynth.a through Sheaf's own Makefile
# and link it, exactly as app/vst does, so there is one archive and one
# build recipe shared with the macOS paths.
#
# On Windows: compile those same sources INTO this target instead. The
# Makefile drives `CXX ?= clang++` with GCC-style flags, entirely outside
# CMake's compiler selection, so `make` there produces an archive from a
# different toolchain than the one linking it -- and MSVC's linker cannot
# resolve MSVC-mangled references against those objects. The symbols are all
# genuinely in the archive; they are simply unusable, which surfaces as
# LNK2019 on every Sheaf symbol app/FroggersMain.cpp needs for the runtime
# shell. Compiling them here keeps one compiler for the whole link.
#
# Globbed rather than listed, deliberately: Sheaf's Makefile builds exactly
# src/*.cpp, so a hand-copied list here would be a second definition of that
# set and would drift the day Sheaf gains a source -- macOS would keep
# working and Windows would fail at the link again, the same way it just
# did. CONFIGURE_DEPENDS makes a new source trigger a reconfigure.
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
        COMMENT "app/standalone: building Sheaf's libsynth.a (nice make -j2 build in ${SHEAF_SYNTH_DIR})"
        VERBATIM
    )
endif()

# ICON_BIG/ICON_SMALL take Resources/Icon.png. Its sibling Resources/
# Icon.icns is what build-launcher.sh copies into the macOS bundle, and
# neither file is generated from the other -- no iconutil, sips or magick
# step exists anywhere in the tree. They are two images kept in step by
# hand, so an edit to one is an edit owed to the other.
# juce_add_gui_app converts the PNG per platform through juceaide (JUCE's
# own _juce_generate_icon), which is the mechanism this target relies on
# for the Windows .ico. Nothing here has exercised it yet: app/vst passes
# no ICON_BIG at all, so no build in this repository has produced an icon
# through this path on any platform.
juce_add_gui_app(FroggersStandalone
    PRODUCT_NAME "Frogg3rs"
    COMPANY_NAME "frogg3rs"
    BUNDLE_ID "dev.sheaf.Frogg3rs.standalone"
    ICON_BIG "${APP_DIR}/Resources/Icon.png"
    ICON_SMALL "${APP_DIR}/Resources/Icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Frogg3rs needs audio input access for external ring-mod (mic, line-in, or interface)."
)

if(NOT WIN32)
    add_dependencies(FroggersStandalone FroggersSheafLibsynth)
endif()

target_sources(FroggersStandalone PRIVATE
    ${FROGGERS_SHEAF_SYNTH_SOURCES}
    "${APP_DIR}/FroggersMain.cpp"
    # The tiny data-root helper, compiled directly rather than through
    # libsynth.a -- same split app/vst/CMakeLists.txt uses, and the same
    # reason: it is JUCE-adjacent runtime code, not synth-core code.
    "${SHEAF_SYNTH_DIR}/runtime/HostDataPaths.cpp"
)

target_include_directories(FroggersStandalone PRIVATE
    "${APP_DIR}"
    "${SHEAF_SYNTH_DIR}/include"
    "${SHEAF_SYNTH_DIR}/runtime"
    "${SHEAF_SYNTH_DIR}/juce"
    "${APP_DIR}/../External/q/q_lib/include"
    "${APP_DIR}/../External/infra/include"
)

target_compile_definitions(FroggersStandalone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

# MSVC does not define M_PI unless _USE_MATH_DEFINES is set before <cmath>
# (same fix the deleted v1 desktop/CMakeLists.txt carried).
if(MSVC)
    target_compile_definitions(FroggersStandalone PRIVATE _USE_MATH_DEFINES)
endif()

# Exactly the eight JUCE modules juce_build.mk compiles for this app on
# macOS (JUCE_MODULE_SRC, that file) -- no juce_audio_utils/
# juce_audio_formats/juce_audio_processors, since this app never links them
# there either.
# Only where the archive was produced by the compiler doing this link; on
# Windows those sources are compiled into the target instead (see above).
if(NOT WIN32)
    target_link_libraries(FroggersStandalone PRIVATE "${SHEAF_LIB}")
endif()

target_link_libraries(FroggersStandalone PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# The two operator documents FroggersBundledDocs.hpp opens, copied from the
# repository's single copy so no second checked-in copy exists to drift from
# it. The destination is wherever OpenBundledDoc looks on the platform being
# built: a macOS bundle's Contents/Resources, or beside the executable
# everywhere else, since currentApplicationFile resolves to a bundle root on
# macOS and to the executable file itself elsewhere.
# On macOS this target is not the shipping path -- build-launcher.sh is, and
# it does its own copy -- so the copy here exists to keep both branches
# testable rather than to ship.
if(APPLE)
    set(FROGGERS_DOC_DEST "$<TARGET_BUNDLE_CONTENT_DIR:FroggersStandalone>/Resources")
else()
    set(FROGGERS_DOC_DEST "$<TARGET_FILE_DIR:FroggersStandalone>")
endif()
add_custom_command(TARGET FroggersStandalone POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FROGGERS_DOC_DEST}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${APP_DIR}/../MANUAL.md" "${FROGGERS_DOC_DEST}/MANUAL.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${APP_DIR}/../QUICK_DICT.md" "${FROGGERS_DOC_DEST}/QUICK_DICT.md"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FROGGERS_DOC_DEST}/assets/manual"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${APP_DIR}/../assets/manual/twister-preset.png" "${FROGGERS_DOC_DEST}/assets/manual/twister-preset.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${APP_DIR}/../assets/manual/twister-preset-shift.png" "${FROGGERS_DOC_DEST}/assets/manual/twister-preset-shift.png"
    COMMENT "app/standalone: copying MANUAL.md/QUICK_DICT.md and its MIDI Fighter Twister diagrams next to the app"
    VERBATIM
)

# Signing must be the last step, after every file the bundle needs has
# landed -- see app/build-launcher.sh's and app/vst/CMakeLists.txt's own
# header comments for the full reasoning. macOS is not a target of this
# file (build-launcher.sh owns it), so this only ever runs where codesign
# exists, which on Windows it does not -- guarded accordingly.
if(APPLE)
    find_program(FROGGERS_CODESIGN_EXE codesign)
    if(FROGGERS_CODESIGN_EXE)
        add_custom_command(TARGET FroggersStandalone POST_BUILD
            COMMAND "${FROGGERS_CODESIGN_EXE}" --force --deep --sign - "$<TARGET_BUNDLE_DIR:FroggersStandalone>"
            COMMAND "${FROGGERS_CODESIGN_EXE}" --verify --deep --strict "$<TARGET_BUNDLE_DIR:FroggersStandalone>"
            COMMENT "app/standalone: signing and verifying the bundle after assembly"
            VERBATIM
        )
    endif()
endif()
