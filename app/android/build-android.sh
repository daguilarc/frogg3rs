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
# Parallelism: `nice` plus Gradle's own `--max-workers=2` -- one build at a
# time, at most two Gradle compile jobs, the rule this repository holds
# every Gradle invocation on this Mac to.
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
export JAVA_HOME="${JAVA_HOME:-/Applications/Android Studio.app/Contents/jbr/Contents/Home}"

if [ ! -x "$PROJUCER" ]; then
  echo "build-android.sh: Projucer not found at $PROJUCER (build it from ~/JUCE first)" >&2
  exit 1
fi
if [ ! -x "$JAVA_HOME/bin/java" ]; then
  echo "build-android.sh: no java at \$JAVA_HOME/bin/java ($JAVA_HOME)" >&2
  exit 1
fi

nice "$PROJUCER" --resave "$JUCER_PROJECT"

ANDROID_PROJECT_DIR="$(dirname "$JUCER_PROJECT")/Builds/Android"
cd "$ANDROID_PROJECT_DIR"
nice ./gradlew --max-workers=2 "$GRADLE_TASK"
