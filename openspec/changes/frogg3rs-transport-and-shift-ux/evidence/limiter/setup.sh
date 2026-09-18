#!/bin/sh
# Builds the source trees the Filter-limiter measurements compile against,
# from the frogg3rs commit they were taken on, into WORK:
#
#   WORK/app    app/ at COMMIT, unchanged
#   WORK/cur    app/ plus measurement taps (taps.patch) and a print of the
#               gate-period test's edge counts (routing-print.patch)
#   WORK/moved  cur/ with the Filter page's limiter moved from the peak
#               branch to after the Comb/Peak blend (move.patch); this is the
#               only difference between cur/ and moved/
#
# and, in both cur/ and moved/, FroggersAudioRoutingTestsBypass.cpp (the
# Filter limiter configured to unity gain). moved/ also gets
# FroggersAudioRoutingTestsRepin.cpp (gate_period_tracks_tempo_change's lower
# bound at 1.3) and FroggersAudioRoutingTestsBreak.cpp (the same bound with
# the doubled tempo set equal to the base tempo and the period assertion
# skipped).
#
# Usage: setup.sh WORK [COMMIT]
# COMMIT defaults to 4ab820f, the tree the recorded figures were measured on,
# with External/Sheaf at 751e82e0. The patches are written against that tree.
# Then build with this directory's Makefile (see its header).
set -eu
WORK=$1
COMMIT=${2:-4ab820f}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(git -C "$HERE" rev-parse --show-toplevel)

mkdir -p "$WORK"
rm -rf "$WORK/app" "$WORK/cur" "$WORK/moved"
git -C "$REPO" archive "$COMMIT" app | tar -x -C "$WORK"

cp -R "$WORK/app" "$WORK/cur"
patch -s -d "$WORK/cur" -p1 < "$HERE/taps.patch"
patch -s -d "$WORK/cur" -p1 < "$HERE/routing-print.patch"

cp -R "$WORK/cur" "$WORK/moved"
patch -s -d "$WORK/moved" -p1 < "$HERE/move.patch"

for v in cur moved; do
    patch -s -o "$WORK/$v/FroggersAudioRoutingTestsBypass.cpp" \
        "$WORK/$v/FroggersAudioRoutingTests.cpp" < "$HERE/routing-bypass.patch"
done
for t in Repin Break; do
    lower=$(echo "$t" | tr 'A-Z' 'a-z')
    patch -s -o "$WORK/moved/FroggersAudioRoutingTests$t.cpp" \
        "$WORK/moved/FroggersAudioRoutingTests.cpp" < "$HERE/routing-$lower.patch"
done
echo "setup.sh: $WORK/app, $WORK/cur and $WORK/moved built from $COMMIT"
