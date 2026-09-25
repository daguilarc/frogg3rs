#!/usr/bin/env bash
# check_catalog_covers_screen_actions.sh -- a mechanical check that every
# MIDI-mappable action the screen exposes (a `FroggersActions` constant in
# FroggersUiSurface.hpp) is both offered by the MIDI catalog
# (FroggersMidiCatalog.hpp) and actually routed by the screen's own
# HandleAction -- so a control added to the screen without a matching
# catalog entry fails this check by name. HandleAction's branches cannot be
# enumerated at runtime (there is no reflection over an if/else chain), so
# this is the mechanical half of that coverage; the runtime half (that the
# catalog's own actions move the right observable) is
# FroggersMidiCatalogTests.cpp.
#
# Usage: check_catalog_covers_screen_actions.sh <app-dir> <excluded-name>...

set -euo pipefail

app_dir="${1:?app directory required}"
shift
excluded=("$@")

ui_surface="$app_dir/FroggersUiSurface.hpp"
catalog="$app_dir/FroggersMidiCatalog.hpp"

[ -f "$ui_surface" ] || { echo "check-catalog-covers-screen-actions: FAIL - missing $ui_surface" >&2; exit 1; }
[ -f "$catalog" ] || { echo "check-catalog-covers-screen-actions: FAIL - missing $catalog" >&2; exit 1; }

names="$(sed -n '/^namespace FroggersActions {/,/^}  \/\/ namespace FroggersActions/p' "$ui_surface" \
    | grep -oE 'inline constexpr const char\* k[A-Za-z0-9_]+' \
    | grep -oE 'k[A-Za-z0-9_]+')"

status=0
missing=()

for name in $names; do
    skip=0
    for ex in "${excluded[@]}"; do
        if [ "$name" = "$ex" ]; then
            skip=1
            break
        fi
    done
    if [ "$skip" -eq 1 ]; then
        continue
    fi

    in_catalog=0
    if grep -qE "FroggersActions::${name}\b" "$catalog"; then
        in_catalog=1
    fi

    # A name is routed either by its own `if (action.name ==
    # FroggersActions::X)` branch, or by a row of the app-command table
    # HandleAction dispatches through in one loop (`{FroggersActions::X,
    # FroggersCommand::...}`) -- the eight presses that travel as
    # synth::MessageIn::AppCommand share that one loop rather than a branch
    # each.
    routed=0
    if grep -qE "action\.name == FroggersActions::${name}\b" "$ui_surface"; then
        routed=1
    elif grep -qE "\{FroggersActions::${name}, FroggersCommand::" "$ui_surface"; then
        routed=1
    fi

    if [ "$in_catalog" -eq 0 ] || [ "$routed" -eq 0 ]; then
        missing+=("$name")
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo "check-catalog-covers-screen-actions: FAIL - not offered by the catalog and/or not routed by HandleAction: ${missing[*]}" >&2
    exit 1
fi

echo "check-catalog-covers-screen-actions: OK - every FroggersActions constant (minus the ${#excluded[@]} excluded) is in the MIDI catalog and routed by HandleAction"
