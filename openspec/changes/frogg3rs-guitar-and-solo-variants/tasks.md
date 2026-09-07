# Tasks — `frogg3rs-guitar-and-solo-variants`

Two Daisy Field programs: Froggers Solo (today's build) and Froggers Guitar,
built and flashed independently. This change unfreezes `src/`.

Revised after preflight (`preflight.md`). Items the preflight refuted are marked
REFUSED and carry the measurement that refuted them; they are not silently
dropped.

There is no time pressure on any item here. Bounded checks limit what is
printed, not what is verified.

## 0. Hygiene

- [x] 0.1 Sweep `src/core/FroggersEngine.hpp`, `src/common/DaisyIO.hpp` and
      `src/mk/`. Establish invocation by SEARCHING for it, by bare name and by
      path, never by a file's existence.
- [x] 0.2 Remove the tracked editor backups: `src/mk/daisy.mk~`,
      `src/Blink/Makefile.bak`, `src/Blink/Makefile~`. `daisy.mk~` diffs against
      `daisy.mk` as an older copy and nothing includes it. All three are tracked
      by git, so removal is a real repo change, not a working-tree tidy.
- [x] 0.3 `external-ring-mod-mix` asserts its formula holds for "any host
      (Daisy Field, desktop, web WASM)" at `:40` AND "across all hosts" in the
      Purpose line at `:5`. Correct BOTH — the preflight found the delta fixed
      the requirement and left the Purpose overreaching. Only the Daisy firmware
      implements it. The Sheaf app has ring modulation of its own from an
      internal per-VCO carrier (`app/dsp/Vco.hpp:181-235`), a different mechanism
      sharing the word, and no external-signal ring mod at all.
      `field-operator-doc-parity` was checked for the same overreach and is
      clean — record that, so the check is not repeated.
      SPEC PROSE ONLY. `app/dsp/Vco.hpp` and the Ringmod knobs are out of bounds
      for this change; they are cited as evidence of a separate mechanism and are
      not to be edited.
- [x] 0.4 The archived `2026-07-25-field-button-latency-headroom` is superseded
      by this change for everything it proposed. Say so in its banner so a
      reader does not execute it separately. NOTE: `.gitignore:39` now ignores
      `openspec/changes/archive/`, so this edit is visible in the working tree
      only. It is not a published correction and must not be reported as one.

## 0.5 The latency and freezing defects in this tree — fixed here, first

Hygiene is step zero, and these are defects in the tree this change touches, so
they land inside it rather than after it.

- [x] 0.5.1 REFUSED, SETTLED BY MEASUREMENT. Moving `UpdateParams()` to block
      rate would advance every `RuntimeParam` smoother 48× less often. Block size
      is 48 (`External/libDaisy/src/daisy_field.cpp:79`, never overridden by
      `DaisyIO::Init`) at 48 kHz (`App.hpp:15`), so block rate is 1 kHz. Measured
      on the host with the real headers, time to 90% of a unit step:

      | call rate | alpha | time to 90% |
      |---|---|---|
      | per sample, today | 0.122694 | 0.375 ms |
      | per block, same alpha | 0.122694 | 18.0 ms |
      | per block, alpha re-derived | 0.956514 | 1.0 ms |

      The re-derivation asks `SetAlphaFromNatFreq(1.0)` against `x_maxCutoff`
      0.499 (`OPLowPassFilter.hpp:9,28`) and is silently clamped: a 1 kHz
      smoother cannot exist at a 1 kHz update rate. Keeping the alpha smears
      every knob 48×; clamping removes smoothing rather than preserving it.
      The controlling quantity moved, so this is a real negative, not a void run.
      **The move does not happen.** Headroom comes from 0.5.2 instead.
- [x] 0.5.2 DELETE THE DEAD PARALLEL FILTER BRANCH. `SetUseV2FilterParallel`
      (`FroggersEngine.hpp:263`) has zero callers; `m_useV2FilterParallel` is
      `false` at `:113` and nothing sets it, so `ApplyOutputFx`'s branch
      `:825-833` never runs. `m_scoopNotch` is configured three times per sample
      (`:561,:562,:564`), each call running a full peaking-EQ biquad
      (`ResonantBump.hpp:42-72`), for a filter no audio passes through.
      Two independent confirmations it is inert even if reached:
      `m_filterScoop` reads `m_filterParams->GetParam(8)` (`:481`) but the page
      initialises only positions 0–6 (`:632-638`), so the target is a
      default-constructed zero and the notch height is 1.0, transparent per
      `ResonantBump.hpp:44-45`; and `m_filterCombPeak` reads position 7 (`:480`),
      which `SetFuegoization()` initialises as `FUEG` (`Page.hpp:75,81`) — the
      blend is wired to the Crispy knob.
      Remove: the branch, `m_scoopNotch`, `m_filterScoop`, `m_filterCombPeak`,
      `m_useV2FilterParallel`, `SetUseV2FilterParallel`, and the `:481`/`:480`
      target reads. A guard whose sole target is deleted goes with it.
      Then collapse `m_resonantBump`'s three setters into one entry point on
      `ResonantBump` that assigns all three fields and calls
      `UpdateCoefficients()` once, leaving the individual setters intact for any
      caller that genuinely changes one value.
      Report six biquad recomputations per sample reduced to ONE, not to two.
- [x] 0.5.3 READ EACH SMOOTHER ONCE. Three smoothers are read twice per sample,
      not two: `m_bumpFreq` at `:558` and `:561`, `m_bumpWidth` at `:560` and
      `:562`, `m_filterScoop` at `:563` and `:830`. (`:559` is
      `m_bumpResonance`, read once — the earlier citation of `:557/:559/:560/:561`
      was wrong by one line throughout.) Each call advances the one-pole, so each
      of the three smooths at twice the rate of every other parameter and its two
      consumers read different points of the same sweep.
      0.5.2 removes `m_filterScoop` and the second reads of `m_bumpFreq` and
      `m_bumpWidth` outright. Assert afterwards that every smoother in
      `UpdateParams` is read exactly once.
      The two halves differ in what they promise and must not be blurred.
      Collapsing the discarded coefficient recomputations is bit-identical —
      they are overwritten before any audio reads them. Reading each smoother
      once is NOT bit-identical, and deliberately so: today the second
      `Process()` returns a further-advanced value. Assert the first as
      bit-identity; assert the second as the surviving filter seeing one
      consistent point of the sweep, and do not report it as a no-op.
- [x] 0.5.4 Satisfied by 0.5.1's measured refusal plus 0.5.2's deletion. The
      separable cost was the coefficient recompute, not the smoother advance,
      and 0.5.2 removes five of six recomputes without touching the call rate.
      Recording this as "not done, and that is the correct outcome" is the
      deliverable.
- [x] 0.5.5 Throttle `led_driver.SwapBuffersAndTransmit()` (`DaisyIO.hpp:137`):
      compute LEDs every poll, transmit on dirty or ~30 Hz. The screen is already
      throttled this way (`:14`, `:218`); the LEDs are not, and `MainLoop`
      (`:211-224`) spins without delay, so I2C traffic scales with poll rate.
- [x] 0.5.6 Dry-reverb early-out with hysteresis for SOLO ONLY (Guitar has no
      reverb): skip `ProcessReverb` while the mix rests at zero, with separate
      enter and exit thresholds so a knob at the boundary does not chatter.
- [x] 0.5.7 Drain B2/B4 mutations one page per `DrainOne`, keeping coalescing;
      B1/B3 stay immediate. Verified as described: `DaisyIO.hpp:52-74` enqueues
      keys 1 and 3 and runs keys 0 and 2 inline; `FieldMutationQueue::DrainOne`
      (`:42-58`) currently runs `RandomizeAllPages()` whole.
- [x] 0.5.8 POSITIVE CONTROL: none of the above may be reported as an
      improvement without a measurement that MOVED. Name the quantity (audio ISR
      headroom, poll rate, or time-to-first-response under Randomize All spam),
      measure before and after, report both. A fix with no moved number is not a
      fix, it is a plausible edit. 0.5.1 is the worked example: its number moved
      and the answer was no.

## 1. Two programs, one source tree

- [x] 1.1 SETTLED BY OPERATOR RULING: two app directories, two `TARGET`s, two
      `.bin`s, flashed independently. Neither binary carries the other's code.
      Rename `src/FroggersTiga/` to `src/FroggersSolo/` with
      `TARGET := FroggersSolo`; add `src/FroggersGuitar/` with
      `TARGET := FroggersGuitar`. `BUILD_DIR` is per app directory
      (`config.mk:10`), so the two builds cannot collide. Each Makefile sets its
      `-D` before `include ../mk/daisy.mk`, which composes
      `DEFS := $(DEFS_COMMON) $(BOOT_DEFS) $(DEFS)` (`daisy.mk:98`).
      The engine stays single-sourced so a DSP fix lands once; the divergence is
      resolved by the preprocessor, so Guitar's binary does not contain the
      reverb page or its buffers.
- [x] 1.2 The variant name reaches the operator through the firmware artifact's
      filename and the manual. NOT through a boot screen: there is none today
      (`DaisyIO::Init` clears the OLED and draws nothing), so satisfying that
      clause meant ADDING a boot splash, which is a new operator-facing feature
      the change was never asked for and which cost ~900 ms on every power-on.
      Built, then removed on operator instruction. The clause is deliberately
      unmet. Identification is the `.bin` filename, the manual, and the fact that
      Guitar has four pages and no Reverb.
      The lesson is the preflight's: it FOUND that no boot screen existed and
      recorded the fact instead of challenging the task. A task that says
      "label X" when X does not exist is not asking for a label.
- [x] 1.3 The inbound half of the rename. `DAISY_MANUAL.md` names
      `src/FroggersTiga` at `:3,:288,:311,:323,:331,:347,:353` and
      `build/FroggersTiga.bin` at `:331`. Each becomes a dangling path the moment
      the directory moves. `app/browser/check-renamed-origin.sh` also greps for
      the string, but against browser output, not `src/` — confirm it is
      unaffected rather than assuming it.

## 2. Guitar: no reverb page

- [x] 2.1 Remove the reverb page, its parameters, and its delay buffers from the
      Guitar build. Enumerate the inbound half first: everything that MENTIONS
      the reverb page — page indices, cursor ranges, doc prose, randomization
      targets, LED/OLED labels — becomes a dangling reference the moment it goes.
      Pages are created in order at `:607,:618,:621,:631,:640` — audio, marbles,
      reverb, filter, drive — so reverb is index 2 and removing it shifts filter
      to 2 and drive to 3.
- [x] 2.2 A guard whose only target is the reverb page goes with it. A check
      that exists to reject a reverb condition guards the impossible once the
      page is gone.
- [x] 2.3 Page navigation must not leave a hole where the page was. State what
      the page count becomes and what the cursor does at the boundary.
      `m_numPages` is counted by `AddPage()` (`Page.hpp:146-151`) against a fixed
      capacity `x_numPages = 8` (`:107`), so the count follows automatically —
      confirm that rather than assuming it, and confirm nothing hardcodes 5.

## 3. Guitar: dry external in parallel with the ring mod

- [x] 3.1 With the gate OPEN, Guitar's pre-drive mix is
      `(7/12)*extIn + (5/12)*((extIn*v1 + extIn*v2 + extIn*v3)/3)`.
      7:5 is exactly 1.4:1, so the dry path is 40% louder than the ring-mod path
      in relative terms, and the weights sum to 1 so the total is unchanged from
      Solo's gate-open level.
- [x] 3.2 Both terms enter the SAME chain as one summed input: `m_frogBlock`
      then `ApplyOutputFx`. Not two chain instances. The chain is nonlinear, so
      the paths interact; that is intended.
- [x] 3.3 With the gate CLOSED, Guitar is identical to Solo: `olvl * oscMix`.
      Assert this rather than assuming it falls out.
- [x] 3.4 Solo's gate-open formula does not change. Assert that too, in the same
      test file, so a future edit to one variant cannot silently move the other.

## 4. Intentionally absent

The earlier task list skipped from 3 to 5. Nothing was lost; the gap is recorded
here so a reader does not go looking for it.

## 5. Tests

- [x] 5.1 The Guitar mix at the gate-open boundary, asserted against the exact
      weights, not against a rounded decimal.
- [x] 5.2 Both variants at the gate-closed condition produce identical output.
- [x] 5.3 Solo's gate-open formula is unchanged by this work.
- [x] 5.4 A test that FAILS against today's single-variant code, with its failure
      text recorded before the fix.
- [x] 5.5 PRESERVATION, asserted rather than assumed: FUEG/Crispy does not
      influence the external mix in either variant, and the ring-mod term uses
      raw per-VCO samples rather than routing through `MixOscVoices`, with
      pair-AR enabled.
- [x] 5.6 PRESERVATION, found by preflight: `MixOscVoices` is called at `:813`
      before the gate test and its result discarded when the gate is open, but
      that call advances pair-AR state (`tickSmoothers()` and two `Step()` calls,
      `:791-805`). Guitar must keep calling it. Assert that the pair-AR state
      after N gate-open samples is identical between variants, so no future
      early-out can skip it silently.
      Pair-AR removed 2026-09-06 by frogg3rs-omni-audit-repairs: it was never wired on the firmware; the VariantMix probe for it is gone with it.

## 6. Operator

- [x] 6.1 OPERATOR: a guitar into Guitar sounds like a guitar plus ring mod, not
      like ring mod alone, and is not louder overall than Solo at the same knobs.
- [x] 6.2 OPERATOR: Guitar has no reverb page and page navigation has no hole.
- [x] 6.3 OPERATOR: rapid Randomize All no longer freezes either variant.
- [x] 6.4 DROPPED with 1.2's boot splash. There is nothing to check on boot;
      identification is by filename before flashing and by page count after.

- [x] Hygiene handed over by the MIDI mappings change's preflight: the 19
      `FroggersEngine.hpp:LINE` citations in `app/FroggersAppCore.hpp` (7)
      and `app/FroggersDspParityTests.cpp` (12) resolve against `main` but
      not against the engine this change rewrites; re-sweep them against
      the engine as committed, in this change's postflight.
      Done 2026-09-06 by frogg3rs-omni-audit-repairs task 5.7 (every app/ citation into src/core re-resolved after the engine edit).

Operator confirmed all three checks 2026-09-07.
