#pragma once

// synth_froggers::FroggersApp -- outer composed application.
//
// `FroggersApp`'s class body is split in two the same
// way apps/braid-4 splits Braid4Core.hpp / Braid4UI.hpp / Braid4.hpp:
//   - FroggersAppCore.hpp: Config()/Init()/PrepareToPlay()/ProcessBlock()
//     and every DSP/parameter-model member -- the full
//     `synth::SynthApplicationCore` contract, plus
//     `ApplyAppCommand`, the audio-thread hook Sheaf's message bus calls for
//     each of the app's own commands -- see that file's header comment for
//     the command/value/publication rule every cross-thread member follows.
//   - FroggersUiSurface.hpp: the portable `synth::ui::Surface` -- the real
//     layout (scopes, chrome band, 16-slot grid, in-place mod-detail swap).
//
// `synth_froggers::FroggersApp` (this file) composes them exactly like
// Braid4.hpp composes Braid4Core + Braid4UiSurface: it derives from
// FroggersAppCore, adds only the FroggersUiSurface member, and wires
// `Init()` to call `ui_.Attach(context, this)`. Every existing test TU's
// `#include "Froggers.hpp"` / `synth_rig::SynthRig<synth_froggers::
// FroggersApp>` / `synth_froggers::FroggersApp` compiles and runs
// against this file's own public surface (Config/Init/PrepareToPlay/
// ProcessBlock/ProcessFrame/PortableSurface, plus every test
// accessor), split across a base class and
// this thin derived one.
//
// This header (and everything it includes) is the app core: it must never
// include a JUCE header. The launcher-registration path
// (FroggersRegistration.hpp) is allowed to reach JUCE; this file
// is not.

#include "FroggersAppCore.hpp"
#include "FroggersMidiCatalog.hpp"
#include "FroggersUiSurface.hpp"

#include "synth/AppConcepts.hpp"

namespace synth_froggers {

class FroggersApp final : public FroggersAppCore {
public:
    void Init(synth::AppContext* context) {
        FroggersAppCore::Init(context);
        // Wires the portable surface to this app's
        // context/state, the same call Braid4.hpp makes
        // (`ui_.Attach(context, this)`).
        ui_.Attach(context, this);
    }

    synth::ui::Surface& PortableSurface() { return ui_; }

    synth::MidiAppCatalog MidiCatalog() const { return FroggersMidiCatalog(); }

private:
    FroggersUiSurface ui_;
};

static_assert(synth::SynthApplication<FroggersApp>,
              "FroggersApp must satisfy the full synth::SynthApplication concept "
              "(SynthApplicationCore + PortableSurface())");
static_assert(synth::HasMidiCatalog<FroggersApp>,
              "FroggersApp must satisfy synth::HasMidiCatalog (MidiCatalog() -> "
              "synth::MidiAppCatalog)");

}  // namespace synth_froggers
