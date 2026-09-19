#!/usr/bin/env bash
# Builds the sheaf-patch launcher with the Froggers app injected out-of-tree.
#
# EXTRA_APP_HEADERS must list EVERY header the app compiles: the sheaf-patch
# Makefile uses them as literal prerequisites on the EXTRA_APP_HEADERS variable
# and generates no -MMD/-MP dependency files, so an unlisted header is
# untracked and edits to it produce a build that succeeds while silently
# ignoring the change.
# Globbing keeps that list from rotting -- a hand-written list
# tracked 4 of 18 headers, missing all of app/dsp/.
set -euo pipefail

cd "$(dirname "$0")/.."
REPO_ROOT="$PWD"

APP_HEADERS="$(ls "$REPO_ROOT"/app/*.hpp "$REPO_ROOT"/app/dsp/*.hpp | tr '\n' ' ')"

# -j2 + nice: 8-core / 16 GB machine, higher parallelism freezes it.
#
# APP_NAME/APP_SOURCES/APP_BUILD_DIR/APP_INFO_PLIST are make command-line
# overrides, which beat the sheaf-patch Makefile's `:=` assignments -- they
# point the build at our direct-launch entry point (app/FroggersMain.cpp)
# instead of the picker (Main.cpp). APP_BUILD_DIR also moves our build
# artifacts out of the External/Sheaf submodule and into our own tree.
#
# APP_INFO_PLIST must be overridden TOGETHER WITH APP_NAME: juce_build.mk's
# $(APP_BUNDLE) rule copies the plist verbatim with no templating, so leaving
# sheaf-patch's plist
# in place ships a bundle whose CFBundleExecutable (SheafPatch) names a binary
# that does not exist (Frogg3rs). A Finder double-click then fails while
# running the binary directly still works, so the breakage hides from any
# build-exit-code check.
nice make -j2 -C External/Sheaf/projects/synth/apps/sheaf-patch \
  EXTRA_APP_DIR="$REPO_ROOT/app" \
  EXTRA_APP_HEADER=FroggersRegistration.hpp \
  EXTRA_APP_TYPE=synth_froggers::FroggersApp \
  EXTRA_APP_REGISTRAR=synth_froggers::MakeFroggersRegistration \
  EXTRA_APP_HEADERS="$APP_HEADERS" \
  APP_NAME=Frogg3rs \
  APP_SOURCES="$REPO_ROOT/app/FroggersMain.cpp" \
  APP_BUILD_DIR="$REPO_ROOT/app/build-launcher" \
  APP_INFO_PLIST="$REPO_ROOT/app/Frogg3rs-Info.plist" \
  "$@"

# Icon: the launcher build should use the same logo as the v1 build, but
# juce_build.mk's $(APP_BUNDLE) rule has no
# icon/resource step at all -- it only mkdir's Contents/MacOS and copies the
# binary and Info.plist, so Contents/Resources/ does not exist after the make
# above runs. app/Frogg3rs-Info.plist's CFBundleIconFile (see that file's own
# comment) names Icon.icns, but the plist key alone does nothing: the file it
# names has to actually be sitting in Contents/Resources/, and since we must
# not touch External/Sheaf or its Makefile, this script is the only place
# left to put it there. `set -euo pipefail` above means a missing
# app/Resources/Icon.icns fails this script loudly (good) -- but a
# SUCCESSFUL run is not proof the icon is visible: Finder caches app icons
# per bundle path/mtime, so a stale generic icon can keep showing after a
# clean build and a correct plist, which is exactly the kind of thing that
# hides from a build-exit-code check. If the icon looks wrong after a
# rebuild, verify with `plutil -p Contents/Info.plist` and `ls
# Contents/Resources/` before assuming the build is broken -- Finder's icon
# cache is the more likely culprit (`killall Finder` or moving/renaming the
# .app forces a refresh).
APP_BUNDLE_DIR="$REPO_ROOT/app/build-launcher/Frogg3rs.app"
mkdir -p "$APP_BUNDLE_DIR/Contents/Resources"
cp "$REPO_ROOT/app/Resources/Icon.icns" "$APP_BUNDLE_DIR/Contents/Resources/Icon.icns"

# Operator documentation ships with the app (froggers-sheaf-runtime-app
# spec, "Operator documentation ships with the app"): copied from the
# repository's single copy at build time, same as the icon above -- no
# second checked-in copy anywhere, so an edit to either doc is picked up by
# the next build with nothing to re-sync.
cp "$REPO_ROOT/MANUAL.md" "$APP_BUNDLE_DIR/Contents/Resources/MANUAL.md"
cp "$REPO_ROOT/QUICK_DICT.md" "$APP_BUNDLE_DIR/Contents/Resources/QUICK_DICT.md"

# The two MIDI Fighter Twister diagrams MANUAL.md's copy above references, at
# the same relative path (assets/manual/) so the manual's relative image
# links resolve inside the bundle exactly as they do in the repository.
mkdir -p "$APP_BUNDLE_DIR/Contents/Resources/assets/manual"
cp "$REPO_ROOT/assets/manual/twister-preset.png" "$APP_BUNDLE_DIR/Contents/Resources/assets/manual/twister-preset.png"
cp "$REPO_ROOT/assets/manual/twister-preset-shift.png" "$APP_BUNDLE_DIR/Contents/Resources/assets/manual/twister-preset-shift.png"

# Signing must be the LAST step, after every file above has landed in the
# bundle: the linker already ad-hoc-signed the raw binary, but that signature
# covers only the binary, not the assembled bundle -- juce_build.mk's
# $(APP_BUNDLE) rule copies the binary and Info.plist in afterward with no
# re-sign, and this script then adds the icon and docs on top of that. A
# bundle whose signature does not cover its own contents is what macOS calls
# damaged, not unsigned, once a download carries com.apple.quarantine.
# Re-signing here replaces the linker's partial signature with one that seals
# everything actually in Contents/, verified below rather than assumed.
codesign --force --deep --sign - "$APP_BUNDLE_DIR"
codesign --verify --deep --strict "$APP_BUNDLE_DIR"
