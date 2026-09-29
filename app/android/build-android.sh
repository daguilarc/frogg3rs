#!/usr/bin/env bash
# Resaves app/android/Frogg3rs.jucer with Projucer, then runs the generated
# Android Studio/Gradle project's debug or release assemble step.
#
# Usage: app/android/build-android.sh [debug|release]   (default: debug)
#
# Projucer: built out-of-tree, outside both repositories (see
# `app/android/Frogg3rs.jucer`'s own header comment for why -- its global
# JUCE module path must already be set to `~/JUCE/modules` via
# `Projucer --set-global-search-path osx defaultJuceModulePath ~/JUCE/modules`,
# since every MODULE in the .jucer has useGlobalPath="1"). PROJUCER may be
# overridden by the caller (e.g. a from-scratch build pointed at a scratch
# copy of the .jucer); it defaults to the location that build produced.
#
# JAVA_HOME: Android Studio's own bundled JBR on this Mac (Java 25) unless
# the caller already set JAVA_HOME -- a CI workflow building this same
# project sets its own Temurin JAVA_HOME before calling this script instead.
#
# JUCE_CHECKOUT: the JUCE checkout androidAdditionalJavaFolders' second line
# must resolve into (see Frogg3rs.jucer's own header comment) -- defaults to
# ~/JUCE, the checkout already built from on this Mac; a CI workflow
# overrides it with wherever it cloned JUCE. This script substitutes it for
# the .jucer's literal @JUCE_CHECKOUT@ token in place immediately before each
# resave and restores the original file content on exit, so the committed
# .jucer never ends up holding a real path.
#
# Parallelism: `nice` plus Gradle's own `--max-workers=2` caps GRADLE TASK
# parallelism (at most two Gradle compile jobs, the rule this repository
# holds every Gradle invocation on this Mac to) but does NOT cap the ninja
# invocation AGP's CMake integration runs inside a single
# buildCMake<Variant>[arm64-v8a] task -- confirmed empirically: with only
# --max-workers=2 set, `ps` during a build showed 5 concurrent clang++
# processes. CMAKE_BUILD_PARALLEL_LEVEL is CMake's own, generator-agnostic
# parallelism cap (respected by the `cmake --build` driver AGP uses
# regardless of Ninja vs Make, since CMake 3.12), set here to 2 for the
# same reason: C++ builds run at -j2 under nice on this Mac.
export CMAKE_BUILD_PARALLEL_LEVEL=2
set -euo pipefail

cd "$(dirname "$0")/../.."
REPO_ROOT="$PWD"

BUILD_TYPE="${1:-debug}"
case "$BUILD_TYPE" in
  debug)   GRADLE_TASK="assembleDebug" ;;
  release) GRADLE_TASK="assembleRelease" ;;
  *)
    echo "usage: $0 [debug|release]" >&2
    exit 1
    ;;
esac

PROJUCER="${PROJUCER:-$HOME/.cache/frogg3rs-projucer/build/extras/Projucer/Projucer_artefacts/Release/Projucer.app/Contents/MacOS/Projucer}"
JUCER_PROJECT="${JUCER_PROJECT:-$REPO_ROOT/app/android/Frogg3rs.jucer}"
JUCE_CHECKOUT="${JUCE_CHECKOUT:-$HOME/JUCE}"
export JAVA_HOME="${JAVA_HOME:-/Applications/Android Studio.app/Contents/jbr/Contents/Home}"

if [ ! -x "$PROJUCER" ]; then
  echo "build-android.sh: Projucer not found at $PROJUCER (build it from ~/JUCE first)" >&2
  exit 1
fi
if [ ! -x "$JAVA_HOME/bin/java" ]; then
  echo "build-android.sh: no java at \$JAVA_HOME/bin/java ($JAVA_HOME)" >&2
  exit 1
fi
if [ ! -d "$JUCE_CHECKOUT/modules" ]; then
  echo "build-android.sh: no JUCE checkout at \$JUCE_CHECKOUT ($JUCE_CHECKOUT)" >&2
  exit 1
fi

# Substitute the .jucer's literal @JUCE_CHECKOUT@ token for this caller's
# real JUCE checkout path in place, resave, then restore the original
# (tokenized) file content -- see the JUCE_CHECKOUT comment above and
# Frogg3rs.jucer's own header comment. `cp`, not a shell-variable capture,
# so the restore is byte-for-byte (a trailing newline survives).
JUCER_BACKUP="$(mktemp)"
cp "$JUCER_PROJECT" "$JUCER_BACKUP"
trap 'cp "$JUCER_BACKUP" "$JUCER_PROJECT"; rm -f "$JUCER_BACKUP"' EXIT
sed -i.bak "s#@JUCE_CHECKOUT@#$JUCE_CHECKOUT#g" "$JUCER_PROJECT"
rm -f "$JUCER_PROJECT.bak"

nice "$PROJUCER" --resave "$JUCER_PROJECT"

ANDROID_PROJECT_DIR="$(dirname "$JUCER_PROJECT")/Builds/Android"
cd "$ANDROID_PROJECT_DIR"
nice ./gradlew --max-workers=2 "$GRADLE_TASK"

# task 4.6: assert the produced APK is actually this app, failing loudly on
# a mismatch instead of trusting a successful Gradle exit alone -- a wrong
# applicationId, a stale APK left over from a different build, or an
# aapt2/Projucer version mismatch mangling the manifest would otherwise
# only surface much later (at install time on the emulator, or never, on CI).
# The newest build-tools aapt2 under the Android SDK this build already used
# (Gradle's own SDK download on a fresh machine, or the SDK already on this
# Mac) -- portable across machines and SDK layouts instead of one build-tools
# version's path, so a CI runner's own SDK location and installed version
# need no separate override here. AAPT2 may still be set directly by the
# caller to bypass this lookup.
ANDROID_SDK_DIR="${ANDROID_SDK_DIR:-${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}}"
AAPT2="${AAPT2:-$(find "$ANDROID_SDK_DIR/build-tools" -mindepth 2 -maxdepth 2 -type f -name aapt2 2>/dev/null | sort -V | tail -1)}"
if [ -z "$AAPT2" ] || [ ! -x "$AAPT2" ]; then
  echo "build-android.sh: no aapt2 found under $ANDROID_SDK_DIR/build-tools (set AAPT2 to override)" >&2
  exit 1
fi
EXPECTED_PACKAGE="io.github.daguilarc.frogg3rs"
APK_PATH="$(find "$ANDROID_PROJECT_DIR/app/build/outputs/apk" -iname "*${BUILD_TYPE}*.apk" | head -1)"
if [ -z "$APK_PATH" ]; then
  echo "build-android.sh: no $BUILD_TYPE APK found under app/build/outputs/apk" >&2
  exit 1
fi
ACTUAL_PACKAGE="$("$AAPT2" dump badging "$APK_PATH" | sed -n "s/^package: name='\([^']*\)'.*/\1/p")"
if [ "$ACTUAL_PACKAGE" != "$EXPECTED_PACKAGE" ]; then
  echo "build-android.sh: $APK_PATH package is '$ACTUAL_PACKAGE', expected '$EXPECTED_PACKAGE'" >&2
  exit 1
fi
echo "build-android.sh: $APK_PATH package $ACTUAL_PACKAGE confirmed"
