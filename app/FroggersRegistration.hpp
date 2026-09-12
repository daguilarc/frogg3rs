#pragma once

// synth_froggers::MakeFroggersRegistration.
// Modelled on
// External/Sheaf/projects/synth/apps/braid-4/Braid4Registration.hpp:20-23. Registers FroggersApp into the
// sheaf-patch launcher's out-of-tree app-registration hook; nothing in
// External/Sheaf is edited to do so.
//
// This header is included from the launcher build (via the EXTRA_APP_HEADER
// build variable), which reaches JUCE elsewhere in that translation unit --
// but this header itself only needs AppRegistry.hpp and Froggers.hpp, and
// stays JUCE-free like its Braid 4 counterpart.

#include "Froggers.hpp"
#include "synth/AppRegistry.hpp"

#include <utility>

namespace synth_froggers {

inline synth::SynthAppManifest FroggersManifest() {
    return synth::SynthAppManifest{
        .appId = "frogg3rs",
        .displayName = "Frogg3rs Synth",
        .author = "daguilarc",
        .category = "synth",
        .hardware = synth::SynthHardwareRequirements{.minEncoders = 16},
    };
}

template <typename LaunchFn>
synth::SynthAppRegistration MakeFroggersRegistration(LaunchFn&& launchFn) {
    return synth::MakeSynthAppRegistration<FroggersApp>(FroggersManifest(), std::forward<LaunchFn>(launchFn));
}

}  // namespace synth_froggers
