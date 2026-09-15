# Prior-art crossfeed separation — measurement transcript

## Source: agent-a35df12ecc456a8c8 — in-repo prior-art sweep

Confirmed. I have everything needed. Here is the consolidated research report.

---

# Delay page Stereo width — consolidated prior design research

Repo: `/Users/diegoaguilar-canabal/Desktop/frogg3rs`. No files changed; read-only sweep. Directories/files covered are named at the end of each section.

## Sweep coverage (what I looked at)

- `openspec/changes/archive/*/research/` — **only one exists in the whole tree**: `openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/` (5 files: `RESEARCH-drive-delay.md`, `RESEARCH2-drive-delay.md`, `RESEARCH-reverb.md`, `RESEARCH-audio-filter.md`, `RESEARCH2-audio-filter.md`). Confirmed via `find openspec -type d -iname "research*"` and `find openspec -type f -ipath "*research*"` — no other archived change carries a `research/` directory or loose research file.
- Every archived change's proposal/design/postflight: grepped case-insensitively for `stereo|width|cross-?feed|ping.?pong|haas|mid.?side|decorrelat|allpass` (194 file hits) and separately for `valhalla|bitwig|dattorro|strymon` (39 hits). Read every file that was actually about audio stereo image (most `width` hits were UI-layout width or filter-Q width, dispositioned "nothing needed" below).
- `openspec/specs/froggers-sheaf-parameter-model/spec.md` (promoted spec) — checked for the stereo-image requirement/scenario.
- `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/` — an **active, unarchived change** that turned out to be the direct continuation of this exact question (proposal.md, tasks.md, specs/froggers-sheaf-parameter-model/spec.md).
- `app/dsp/Delay.hpp`, `app/dsp/Reverb.hpp`, `app/dsp/StereoField.hpp` — read in full/targeted around the stereo mechanisms.
- `git log --all --grep` and `git log --all --format` grepped for stereo/width/cross-feed/ping-pong/mono, plus direct reads of the resulting commits.
- `MANUAL.md` — current (about-to-go-stale) prose for both pages' Stereo width / Width balance / Density.
- Second-pass tokens for a possible ping-pong/alternation mechanism: `ping`/`pong` repo-wide (only unrelated WebSocket-protocol and Node.js changelog hits, in `External/Sheaf` and `node-v22...`), and `alternat|swap|bounce` in `app/dsp/*.hpp` and openspec (only unrelated UI/audio-export "bounce" hits and the reverb's channel-transposition "swap", dispositioned below).

---

## Q1 — Every stereo-image mechanism this project has surveyed or considered

**Time-offset / Haas-style spread** (delay read-tap offset between L/R)
- Discussed: `openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md:185-217` (candidate "Width Balance"), and it is the shipped mechanism — `app/dsp/Delay.hpp:679-681` (`widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance`, applied to `timeR` only).
- Verdict: **Adopted.** It is the mechanism the manual credits with "the whole of the widening you hear on the page's own default patch" (`MANUAL.md:692-694`).

**Cross-feed blend inside the recirculating feedback write**
- Discussed: same research doc, `RESEARCH2-drive-delay.md:185-217`, proposing it become one axis of a complementary "Width Balance" pair with time-offset, citing Bitwig's separation of Width and Cross Feedback (`RESEARCH2-drive-delay.md:205-210`).
- Shipped: `app/dsp/Delay.hpp:720,726` (`cross = p.dwid * 0.5f * widthBalance`, fed to `dsp::CrossFeedPair`, and the result written into the recirculating lines at `:808-809`).
- Verdict: **Adopted, then found defective.** `af643e3` (2026-08-29) first recorded that with the old mono fold downstream this cross-feed was inert; once `04efe9c` removed that fold, `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md:48-71` measured that this cross-feed *re-correlates* the signal toward exact mono at weight 0.5 rather than widening it. Currently **REFUTED** against the promoted spec's stereo-image scenario (`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/specs/froggers-sheaf-parameter-model/spec.md:111`), unrepaired as of this reading (see "current code state" below).

**Mid/side output scaling** (Reverb's own Stereo width)
- Discussed and shipped for Reverb only: `app/dsp/Reverb.hpp:806-809` — `mid = 0.5*(aOut+bOut)`, `wetL = mid + width*(aOut-mid)`, `wetR = mid + width*(bOut-mid)`. Rationale for why this now works (splitting the shared damping filter) is at `app/dsp/Reverb.hpp:260-278`.
- Verdict: **Adopted for Reverb.** Explicitly the sole mechanism the promoted spec credits Reverb's Stereo width with: "Reverb's Stereo width drives one mechanism, the tank's mid/side output scaling; giving it the cross-feed as a second was considered and dropped" (`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/specs/froggers-sheaf-parameter-model/spec.md:111`).
- Considered for Delay's cross-feed too, i.e. "should the tank's cross-feed also ride Stereo width" — **rejected**, same line: "no surveyed design ties the two, Dattorro's cross-feed being a fixed figure-eight with no knob and stereo coming from the output tap structure."

**Allpass decorrelation / diffusion**
- Discussed: `RESEARCH-drive-delay.md:203-228` ("Diffusion" candidate, citing Valhalla Delay's Diffusion section and Schroeder/Moorer allpass diffusers) and the promoted spec's "One mechanism carries one name across pages" requirement (`openspec/specs/froggers-sheaf-parameter-model/spec.md` / the delta at lines 94-104), which states Diffusion/Density smear **in time**, not stereo image, on both pages.
- Verdict: **Adopted, but explicitly NOT a stereo-width mechanism.** It decorrelates the delay's wet tap or the reverb's pre-tank feed in time; the spec is explicit this is a different job from widening ("Diffusion means smearing a signal in time; a control that only cross-feeds two channels is not diffusion").

**Ping-pong (alternating input into one line only)**
- See Q3, its own answer below — surveyed once, in passing, never for this control.

**Haas as a named/separate control from spread** — no separate discussion found; the shipped `widthSpread` mechanism *is* a Haas-style read-time offset and is referred to that way directly: RESEARCH2-drive-delay.md:189 calls the 1.0 end of "Width Balance" "a Haas-style, more aggressively 'smeared' stereo character."

Nothing needed for the generic "width" hits in `RESEARCH-audio-filter.md`/`RESEARCH2-audio-filter.md` (filter-Q bandwidth, unrelated) or the many UI-layout/mobile "width" hits across `openspec/changes/archive/*/design.md` etc. (screen/column width, unrelated) — confirmed by reading representative samples and the audio-filter files in full for stereo terms (only Q/bandwidth hits).

## Q2 — External products/papers actually cited on this subject

- **Bitwig** — `openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md:205-210`: "Bitwig's own stereo Delay device ships `Width` and `Cross Feedback` as two **independent, separately-exposed** controls rather than one fixed-ratio knob" — cited as proof the two mechanisms are separable, and used again verbatim in the active proposal (`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md:102-104`) as the precedent pointing at the correct fix.
- **Valhalla** (Delay) — `RESEARCH-drive-delay.md:207-215` and `RESEARCH2-drive-delay.md:243-244,252-262`: cited for its Diffusion section (time-smearing, not stereo) and for reverse-delay always shipping as a discrete mode rather than a continuous crossfade — not cited for stereo width/imaging.
- **Valhalla Shimmer** — `RESEARCH-reverb.md:241-246`: cited for pitch-shift-in-feedback precedent, unrelated to stereo width.
- **Dattorro** — cited exactly once in this codebase, and only about Reverb's Density (input diffusion), not about Stereo width: `app/dsp/Reverb.hpp:709-710` ("Dattorro's input-diffusion role -- ahead of the tank, not inside its feedback loop"), and expanded in the active change's spec delta, `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/specs/froggers-sheaf-parameter-model/spec.md:315`: "Dattorro's cross-feed being a fixed figure-eight with no knob and stereo coming from the output tap structure" — used as the reason NOT to drive the reverb tank's cross-feed from Stereo width.
- **Strymon** (El Capistan, Timeline) — `RESEARCH-drive-delay.md:140-146,164-170,184-191` and `RESEARCH2-drive-delay.md:219-226`: cited for Feedback Drive/Tone/Crush precedent (tone/saturation in the feedback loop), not for stereo imaging.
- No other stereo-image papers/products (no citation of any Haas-effect paper, no dedicated ping-pong product, nothing else) appear anywhere in the tree outside the above.

## Q3 — Was ping-pong ever considered for this page?

**Honest answer: surveyed once, in passing, for a different bank grouping, and never followed up — not surveyed or rejected specifically for the Delay page's stereo image.**

Two-token search performed as instructed: `ping.?pong|pong` (repo-wide, case-insensitive) and, separately, `alternat|swap|bounce` in `app/dsp/*.hpp` and openspec. Findings:

- The only in-context hit for "ping-pong" in the whole repository is `openspec/changes/archive/2026-08-06-frogg3rs-modulation-truth-and-voicing/tasks.md:723-725`, in section `§J.2 — empty-slot candidates for the OTHER FIVE BANKS` (i.e., explicitly not the Delay bank, whose own dedicated exploration is `§J`, lines 618-644, immediately above): "**Tier 3** (genuinely new DSP): sub-oscillator, hard sync, per-voice gates, true ping-pong, true reverb freeze..., multi-tap early reflections, state-variable/ladder filter." It is listed with no adoption/rejection reasoning of its own — Tier 3 items are simply "deferred; recorded so a future design starts from evidence rather than a wishlist" (line 692). Nobody surveyed it against the Delay page's own Stereo width; it is a one-line mention in a generic cross-bank wishlist.
- The Delay bank's own dedicated exploration in the same file (`§J`, lines 618-644) lists "Cheap on this architecture: Crossfeed (the `dwid` cross-feed exists...)" and "NOT cheap: Diffusion..., Halo..., true Freeze..., Reverse" — **ping-pong is absent from this list entirely.**
- All other "ping"/"pong" hits found in the repo are from `External/Sheaf` (WebSocket RPC protocol `rpc.ping`/`server.pong`) and the vendored `node-v22.16.0-darwin-arm64/CHANGELOG.md` — unrelated to audio, dispositioned "nothing needed."
- Arithmetic-shape search: "feeding the input to one line only" or "a cross weight of 1.0" — not found anywhere. The one place a full 1.0-style swap exists is the *reverb's* fixed cross-feed, which is a **read-tap swap** (`CrossFeedPair(valB, valA, 0.0f)`, `app/dsp/Reverb.hpp:695-705` — passing the reads transposed at weight 0 to get a full swap), not an input-alternation ping-pong; it is unrelated to how new input reaches the two lines and is not stereo imaging (Density drives it, and it is explicitly a fixed non-control coupling, not a width mechanism).

So: report a genuine, honest zero for "ping-pong considered and rejected for Delay's stereo image." It was named once, generically, for unrelated banks, and dropped without discussion.

## Q4 — How does the input reach the two delay lines today? (`dsp::StereoDelay::Process`, `app/dsp/Delay.hpp`)

Read directly, line numbers as they stand in the file today:

- `app/dsp/Delay.hpp:731`: `const float inSignal = bumpIn * send;` — **one scalar**, computed once, not per-channel.
- `app/dsp/Delay.hpp:808`: `WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbL), lineL);`
- `app/dsp/Delay.hpp:809`: `WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);`

**What this says, read literally:** the `inSignal` term in both writes is the identical scalar, multiplied by the identical `(1.0f - freezeEff)` — i.e., new input is written into `lineL` and `lineR` **symmetrically**. The *only* asymmetry between the two writes is which cross-fed feedback value (`fbL` vs `fbR`) is added to it, and `fbL`/`fbR` themselves come from `CrossFeedPair(dL, dR, cross)` (`:726-728`), where `dL`/`dR` are reads of the same mono-fed lines at two different times (`timeL`, `timeR`, differing only by `widthSpread`, `:679-684`).

**Implication for what a cross-feed can do:** since fresh input is mono to both lines, the only source of any L/R difference at all is the read-time offset (`widthSpread`). A cross-feed applied to two reads of an otherwise-identical mono signal blends two time-shifted copies of one signal, which is exactly the finding driving the active repair: it can only ever move the pair *toward* correlation (exact mono at weight 0.5), never away from it, because there is no independent stereo content for it to decorrelate. This matches the active change's own diagnosis verbatim (`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md:94-99`) and is why task 1.1 of that change treats "can cross-feed widen this signal at all" as an open, unresolved empirical question rather than assumed fact.

## Q5 — What `dsp::Reverb::Process` does for its own Stereo width

Read directly, `app/dsp/Reverb.hpp`:
- `:806`: `const float mid = 0.5f * (aOut + bOut);`
- `:807`: `const float width = widthKnob01;  // :460, direct passthrough`
- `:808`: `wetL = mid + width * (aOut - mid);`
- `:809`: `wetR = mid + width * (bOut - mid);`

This is a mid/side-style output scaling: `mid` is the average of the two (already independently-damped) tank taps, and `width` scales how much of each tap's deviation from that average survives into `wetL`/`wetR`. At `width=0`, `wetL=wetR=mid` (mono); at `width=1`, `wetL=aOut`, `wetR=bOut` (full tap separation).

**Why this mechanism was chosen — is it recorded?** Yes, explicitly, in two places:
1. `app/dsp/Reverb.hpp:810-813`: "`wetL`/`wetR` used to be summed here, which made the Width control above mathematically inert: with `mid == 0.5(aOut+bOut)`, `wetL + wetR` is `2*mid` at every width, so the knob could not change what was heard... Keeping the pair is what makes it a control." (i.e., the mid/side form was chosen because it's the minimal change that makes the control non-inert once summed downstream mixing stopped discarding the difference.)
2. `app/dsp/Reverb.hpp:260-278` (header comment on `dampFilterA`/`dampFilterB`): the mechanism only became audible after splitting the previously-shared damping filter state, because a shared filter's own recursive output "pinned the tank's two output taps close together whatever Stereo width was set to." This is confirmed by commit `43856d1` (2026-08-29): "Reverb's Width cancels exactly: with `mid = (aOut+bOut)/2`, the two width terms sum to zero, so `wet == mid` at every knob position. The control does nothing today."

No citation of an external product for *why mid/side specifically* (as opposed to e.g. cross-feed) was chosen for Reverb — the choice is justified purely from this codebase's own arithmetic (making an already-existing `mid`/tap-pair non-degenerate), not from precedent literature. The Dattorro citation (Q2) explains why cross-feed was kept *off* Reverb's width, not why mid/side was chosen for it.

## Q6 — Recorded operator rulings about the Delay page's stereo image / mono folding / where width belongs

- **`04efe9c`**, 2026-08-29, "Fold to mono at the device, not in the middle of the chain" — the file changed is `openspec/changes/archive/frogg3rs-browser-microphone-permission-path/proposal.md`. The commit's own message body: *"The signal now stays stereo from the delay onward and folds only where the device is mono."* The exact ruling sentence the operator wrote **inside that diff** (not the commit message header) is:
  > "RULING, 2026-08-29: the signal SHALL NOT be collapsed to mono in the middle of the chain. Folding belongs at the output, and only where the device itself is mono — a phone speaker, a single-channel interface."
  This is the sentence quoted (slightly compressed) by the active proposal and HANDOFF.md.
- **`af643e3`**, 2026-08-29, "Record that the stereo delay is mono by the time it is heard" — precedes and motivates the above; commit body: *"StereoDelay is stereo inside... but ToReverbMono folds L and R and everything after it is a single float, so no image from this stage reaches the output. Stereo width therefore cannot widen anything, and its cross-feed cancels exactly in a sum... Recorded rather than fixed."*
- **`43856d1`**, 2026-08-29, "Stereo is plumbing, not computation, and both Width knobs are inert" — the follow-through: *"Reverb's Width cancels exactly... Delay's Stereo width survives only through the feedback loop... Both become real controls when the fold moves to the device."*
- **`ff26687`**, 2026-08-29, "Correct the stereo work's blast radius: the Daisy is untouched" — scoping correction on the same lineage (not read in full; title only, disposition: consistent with the above, no new stereo-mechanism content).
- **`3b1f4e9`**, 2026-09-13, "Give the stereo field one definition, and keep both tanks" — the commit that created `app/dsp/StereoField.hpp` and lifted `CrossFeedPair` to a shared helper; its message records a near-miss: naively sharing the helper "in reading order at both sites would have turned the reverb's swap into an identity at every knob position," which would have silently changed the reverb's rest-state sound. Relevant because it establishes the constraint (repeated in the active proposal) that `CrossFeedPair` is shared and must not be edited.
- **`c5fe881`**, 2026-09-13, "Supersede the density change with one that repairs the Delay page's Stereo width" — this is the commit that created the still-open `frogg3rs-delay-width-wysiwyg-repair` change itself; its body states the measured defect (correlation 0.0017→0.1193 as Stereo width rises at Feedback 0.7) and repeats the `04efe9c` ruling as the reason it must be fixed rather than merely documented.
- **`053e64c`**, 2026-09-13, "Leave the next session a handoff for the Delay width repair" — added `HANDOFF.md` (read in full above), records that six sessions in a row reached this defect and handed it forward without attempting a fix, and restates the same `04efe9c` ruling as settling *whether* it should be fixed (not *how*).

**No other operator rulings** specifically about Delay's stereo image, mono folding placement, or where width belongs in the chain were found beyond this one lineage (`af643e3` → `04efe9c` → `ff26687` → `43856d1` → ... → `c5fe881` → `053e64c`). Second-pass token used to check for a second, independent ruling: `git log --all --grep` on `"SHALL NOT.*mono"` and `"collapse.*mono"` — no additional hits beyond the diff quoted above.

## Current state of the code (as of this reading, for context on the decision)

`app/dsp/Delay.hpp:679-681,720,726` still compute `widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance` and `cross = p.dwid * 0.5f * widthBalance`, unrepaired — i.e., the defect described in `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md` is present in the tree exactly as measured there, and that change's `tasks.md` stage 1 is entirely unticked. `SetWidthBalance` (`app/dsp/Delay.hpp:617-635`) is confirmed still an identity map (`widthBalance = knob01`) applying one common scalar to both terms, matching the "parent defect" the proposal describes rather than the complementary-ratio law its own design document (`RESEARCH2-drive-delay.md`, "Width Balance — WBal — NEW") specified.
