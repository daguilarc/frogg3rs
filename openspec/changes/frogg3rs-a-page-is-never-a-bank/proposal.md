# Proposal — `frogg3rs-a-page-is-never-a-bank`

Based on frogg3rs `main` at `34b8f27`, which pins Sheaf `f6f49067`. This
proposal makes no Sheaf change and moves no submodule pin.

## Why

A player reads "Page" on screen — Reset Page, Randomize Page, the per-page
Crispy — and reads "Bank" in the same interface for the identical thing: the
six-way Audio/Envelope/Filter/Drive/Delay/Reverb selector, its Next/Previous
arrows, and MANUAL.md's running prose. A reader of the code sees the same
split at the same seam: in `app/FroggersUiSurface.hpp`'s action-dispatch
switch, `FroggersActions::kResetPage` and `FroggersActions::kBankNext` sit a
few lines apart, naming the same "which of the six pages is showing" concept
under two different words, with nothing to tell a reader they are the same
concept:

```
$ grep -n 'FroggersActions::kResetPage\|FroggersActions::kBankNext\|FroggersActions::kBankSelect\|FroggersActions::kBankPrevious' app/FroggersUiSurface.hpp
164:inline constexpr const char* kBankTabsRow = "froggers.layout.right.banks";
175:inline constexpr const char* kBankPrevArrow = "froggers.bank.prev";
176:inline constexpr const char* kBankNextArrow = "froggers.bank.next";
233:inline constexpr const char* kRandomizePage = "froggers.randomize.page";
235:inline constexpr const char* kResetPage = "froggers.reset.page";
236:inline constexpr const char* kBankSelect = "froggers.bank.select";
240:inline constexpr const char* kBankPrevious = "froggers.bank.previous";
241:inline constexpr const char* kBankNext = "froggers.bank.next";
2380:        if (action.name == FroggersActions::kBankSelect) {
...
2418:        if (action.name == FroggersActions::kRandomizePage) {
2426:        if (action.name == FroggersActions::kResetPage) {
```

The operator's standing ruling is that "bank" must never appear bare, because
this project has a second, genuine sense of the word: the MIDI Fighter
Twister's own onboard hardware bank, a device feature unrelated to Froggers'
parameter pages, documented once in MANUAL.md:

```
$ grep -n -i "bank" MANUAL.md | grep -i "utility\|side buttons\|whatever bank"
399:Enc 3FH/41H" (relative), all six side buttons to "CC Hold", and "Bank Side Buttons" unchecked, so the
400:side buttons keep sending CC 8 to 13 on channel 4 (channel 3 counted from 0) whatever bank the Twister
```

"Bank Side Buttons" is a literal setting name in the third-party MIDI Fighter
Utility, and "whatever bank the Twister shows" describes the device's own
onboard multi-bank memory, which the preset works around by holding the side
buttons on CC regardless of which onboard bank is showing. This sentence is
never touched by this change.

## What "bank" means where, established by reading, not by guessing

**The engine's own `Bank` is a third thing, and it is not conflated with
anything.** Sheaf, the runtime library this app is built on, has its own
foundational C++ type `synth::Bank` (a group of addressable parameters a
`Slot` can select among) and its own generic `MessageIn` vocabulary —
`SelectParamBank`, `NextParamBank`, `PrevParamBank`, `ParamSetAbsoluteOnBank`
— used by more than this app (the generic controller wizard, VST parameter
automation, Sheaf's own miniapp). Nothing inside Sheaf calls this same thing
"page"; there is no internal conflation to fix there, and the type serves
consumers this proposal does not touch. It stays named `Bank`.

**Froggers' own outward-facing layer says both.** Two families of
Froggers-authored identifiers name the *same* "which of the six pages is
showing" concept:

- Says "page": `FroggersActions::kResetPage`, `kRandomizePage`,
  `app::RequestResetPage`, `RequestRandomizePage`, `FroggersModulation.hpp`'s
  `ResetPage`/`RandomizePage` functions, MANUAL.md's "Reset Page"/"Randomize
  Page" button names, the per-page-control language throughout the manual.
- Says "bank": `FroggersActions::kBankNext`/`kBankPrevious`/`kBankSelect`,
  `FroggersNodeIds::kBankTabsRow`/`kBankPrevArrow`/`kBankNextArrow`,
  `FroggersUiSurface::CurrentBankIndex()`, `FroggersAppCore::ActiveBankIndex()`
  / `FroggersVisibleBankIndex()` / `activeBankIx_`,
  `FroggersParameters.hpp`'s `kFroggersBankCount`,
  `FroggersPluginProcessor.cpp`'s `kVisibleBankIndexKey`, and MANUAL.md's
  "Bank selection" section, "Bank Next"/"Bank Previous"/"Bank 1" through
  "Bank 6" button names, and its "## Audio bank" through "## Reverb bank"
  section headers.

This second family is the fix's target. It is every place a reader — player
or programmer — meets "bank" as the name for what the rest of the interface
calls "page," with no marker that they are the same thing.

**`FroggersBankId` (the *which*-page enum) is named after `synth::Bank` and
is left alone here — named as a decision, not a silent omission.**
`FroggersBankId::Audio/Envelope/Filter/Drive/Delay/Reverb` is Froggers' own
enum for which page, and its value *is* used as the argument to `page`-named
functions in the same file:

```
$ grep -n 'PageParameter(FroggersBankId::Audio' app/FroggersModulation.hpp
1412:    synth::Parameter& target = model.PageParameter(FroggersBankId::Audio, spec.targetParamIx);
```

`model.PageParameter(FroggersBankId::Audio, …)` is the same seam again, at a
deeper layer. Renaming `FroggersBankId` is a real, defensible continuation of
this fix, but it is not this change: it is the single most pervasive family
in the tree —

```
$ grep -rIcn -i "bank" app/FroggersParameters.hpp app/FroggersModulation.hpp app/FroggersAppCore.hpp
app/FroggersParameters.hpp:102
app/FroggersModulation.hpp:185
app/FroggersAppCore.hpp:154
$ grep -c -i bank openspec/specs/froggers-sheaf-parameter-model/spec.md
88
```

— threaded through `synth::Bank&`-typed helper functions that are correctly
named after the Sheaf type they take (`RandomizeBankLevel1Depths(synth::
ParameterManager&, synth::Bank&)`, `PressBankWithRandomValue`,
`RandomizeBankValues`), through the parameter model's own addressing scheme
(bank + slot), and through 88 "bank" occurrences in one capability spec
alone, most of them normative. Folding that rename into this change would
multiply its diff by an order of magnitude for a form of the same ambiguity
that is far less acute: a reader working inside `FroggersModulation.hpp`
already reads `synth::Bank&` on every other line and reasonably infers "bank"
there follows the engine type, not the reverse. The acute ambiguity — a
player-visible label and a player-visible label for the identical thing
disagreeing, and a dispatch switch naming the identical concept two ways a
few lines apart — lives entirely in the outward accessor/action layer above.
**This is two changes. This proposal is the first: the outward-facing
surface. Renaming `FroggersBankId` and its call sites, and the matching
rewrite of `froggers-sheaf-parameter-model`'s spec prose, is named here as
follow-on work for the operator to commission separately, not started.**

## The compatibility question, traced through the persistence code

`FroggersActions::kBankNext` etc. are `constexpr const char*` values
(`"froggers.bank.next"`). Renaming the *identifier* is free — it is a
compile-time symbol. Whether the *string value* can change is a different
question, answered by reading the serializer, not by guessing:

```
$ grep -n 'json.SetNew("appAction"' External/Sheaf/projects/synth/src/MidiController.cpp
2688:        json.SetNew("appAction", arena.String(value.appAction.c_str()));
```

A saved controller row's JSON carries `"appAction": "froggers.bank.next"`
verbatim — this is a real, on-disk, player-owned value, and Sheaf's own
loader resolves it back to a live action by exact string match against the
current build's catalog:

```
$ sed -n '1186,1210p' External/Sheaf/projects/synth/include/synth/Engine.hpp
    // Resolves every AppAction row of one controller's profile config
    // against midiCatalog_, called from RebuildMidiProcessors on a copy of
    // the slot's persisted config (never on instrumentConfig_ itself, so an
    // action the running catalog does not know about stays in the saved
    // instrument and comes back once a later app version adds it). A row
    // whose (action, value) the catalog has sets its resolved index; a row
    // it does not have is dropped from the copy, logged once by name. A
    // shifted press is resolved by its own (shiftedAppAction,
    // shiftedAppActionValue) pair; when the catalog does not have it, only
    // the shifted press is cleared from the copy -- the row itself stays.
    void ResolveAppActionsAgainstCatalog(MidiControllerProfileConfig& config) {
        for (auto it = config.systemMessages.begin(); it != config.systemMessages.end();) {
            if (it->press.type != MessageIn::Type::AppAction) {
                ++it;
                continue;
            }
            if (const auto ix = FindMidiAppAction(midiCatalog_, it->appAction, it->appActionValue)) {
                it->press.appActionIx = *ix;
                if (it->shiftedPress.has_value() && it->shiftedPress->type == MessageIn::Type::AppAction) {
                    if (const auto shiftedIx = ...
```

Read plainly: **if the string value changed from `"froggers.bank.next"` to
something else with no back-compat entry, a Twister row a player already
saved would have its Bank Next / Bank Previous / Bank Select buttons
silently dropped on next load** — "dropped from the copy, logged once,"
nothing shown on screen. The same mechanism resolves
`app/vst/FroggersPluginProcessor.cpp`'s `kVisibleBankIndexKey` session-extras
value (`"visibleBankIndex"`), read back by exact key match; an old DAW
project missing the new key would simply fall back to page 0 — a milder,
cosmetic loss, but the same shape of problem.

**Decision: identifiers, UI labels, and MANUAL.md prose are renamed; every
wire value is not.** `FroggersActions::kBankNext`'s C++ name becomes
`kPageNext`; its *string value* stays `"froggers.bank.next"`, with a comment
at the declaration explaining why. The same holds for
`kBankPrevious`/`kBankSelect`/the two `FroggersNodeIds` arrow constants/
`kBankTabsRow`, and for `kVisibleBankIndexKey`'s JSON key string. A wire
identifier is not prose a player reads; it is a stored cross-version
contract, and Sheaf's own `MessageIn` type-name serialization already
carries this precedent — old serialized type names (`"toggleShift"`,
`"setReset"`, `"setShift"`) are still accepted as aliases for the current
`ToggleReset` type, permanently:

```
$ grep -n '"toggleShift"\|"setReset"\|"setShift"' External/Sheaf/projects/synth/src/MidiController.cpp
257:    } else if (value == "toggleReset" || value == "toggleShift" || value == "setReset" || value == "setShift") {
```

Button *labels* (`"Bank Next"`, `"Bank Previous"`, `"Bank " + N`) are not
serialized anywhere — confirmed by the same `ToJSON` read above, which writes
`appAction`/`appActionValue`/`shiftedAppAction`/`shiftedAppActionValue` and
nothing else from the catalog entry — so labels rename freely with no
migration question.

A patch or config saved before this change is unaffected: the C++ symbol a
label and a dispatch branch are spelled with is not part of the file. A patch
saved after this change is byte-identical, for every field this change
touches, to one saved before it.

## What Changes

- **`FroggersActions` / `FroggersNodeIds` in `app/FroggersUiSurface.hpp`.**
  `kBankNext`→`kPageNext`, `kBankPrevious`→`kPagePrevious`,
  `kBankSelect`→`kPageSelect`, `kBankTabsRow`→`kPageTabsRow`,
  `kBankPrevArrow`→`kPagePrevArrow`, `kBankNextArrow`→`kPageNextArrow`.
  String *values* unchanged; each declaration gets a one-line comment stating
  the value is a stored wire identifier and does not follow the symbol name.
- **`FroggersUiSurface::CurrentBankIndex()`** → `CurrentPageIndex()`.
- **`FroggersAppCore.hpp`**: `ActiveBankIndex()`→`ActivePageIndex()`,
  `activeBankIx_`→`activePageIx_`, `FroggersVisibleBankIndex()`→
  `FroggersVisiblePageIndex()`, `RequestBankSelect()`→`RequestPageSelect()`.
- **`FroggersParameters.hpp`**: `kFroggersBankCount`→`kFroggersPageCount`
  (value 6 unchanged).
- **`app/vst/FroggersPluginProcessor.cpp`/`.hpp`**:
  `kVisibleBankIndexKey`→`kVisiblePageIndexKey` (C++ name only; JSON key
  string `"visibleBankIndex"` unchanged, commented as a wire identifier).
- **`FroggersMidiCatalog.hpp` catalog labels**: `"Bank Next"`→`"Page Next"`,
  `"Bank Previous"`→`"Page Previous"`, `"Bank " + std::to_string(ix + 1)`→
  `"Page " + std::to_string(ix + 1)`. These are display strings only (the
  compatibility trace above shows labels are never serialized).
- **MANUAL.md**: every occurrence of "bank"/"banks" that names the page
  concept becomes "page"/"pages" — the "Bank selection" section heading, the
  six `## <Name> bank` section headings (become `## <Name> page`), "Bank
  Next"/"Bank Previous"/"Bank 1" through "Bank 6" in the controller tables,
  and the running prose ("Six parameter banks", "switching banks", "that
  bank's own 14 page parameters," etc.) — except the two sentences about the
  Twister's own onboard hardware bank ("Bank Side Buttons", "whatever bank
  the Twister shows"), which are the genuine second sense and are unchanged.
- **`assets/manual/twister-controls.json`**: regenerated by `make
  manual-diagrams` after the catalog label rename; its `"press"`/
  `"shiftedPress"` label fields read "Page Next"/"Page Previous"; its
  `"appAction"`/`"shiftedAppAction"` wire values and its `"bankIx"` field
  (Sheaf's own `MessageIn` field name, unrelated to this change) are
  untouched.
- **`app/check_docs_match_parameter_table.py`**: its MANUAL.md heading suffix
  changes from `" bank"` to `" page"` so the existing bank/label/manual
  cross-check keeps matching MANUAL.md's renamed section headings against
  `FroggersBankLayouts()` (which keeps its own name — see the `FroggersBankId`
  decision above). `QUICK_DICT.md`'s headings (already bare — `## Audio`,
  never `## Audio bank`) are unaffected; this is itself a pre-existing third
  instance of the same inconsistency, resolved as a side effect of MANUAL.md
  converging on the same word QUICK_DICT.md already used.
- **`app/dsp/*.hpp` comments** (`Limiter.hpp`, `Delay.hpp`, `Drive.hpp`,
  `DspMath.hpp`, `FilterFx.hpp`, `Fuegoize.hpp`, `VoiceEnvelope.hpp`,
  `EnvelopeFollowers.hpp` — 25 hits total, counted below) that name the page
  concept ("the Filter bank's output," "the Drive bank's Tone," "Delay-bank
  slot") become "page." `EnvelopeFollowers.hpp`'s citations of the retired
  simulator's `V2EnvelopeFollowerBank.hpp` are a different, unrelated sense
  (a bank of DSP units) and are excluded — Task 1 classifies every hit by
  reading it before any substitution runs.
- **A new hygiene check**, `app/check_no_bank_page_conflation.py`, wired into
  `app/Makefile` as `check-no-bank-page-conflation`, following the existing
  `check_no_planning_history.py`/`check_common.py` shape. It fails the build
  if any of the renamed-away identifiers reappear
  (`\bBank(Next|Previous|Select|TabsRow|PrevArrow|NextArrow)\b`,
  `\bBankIndex\b`, `\bkFroggersBankCount\b`, `\bkVisibleBankIndexKey\b`) in
  `app/*.hpp`/`app/*.cpp`/`app/vst/*.hpp`/`app/vst/*.cpp`, or if MANUAL.md
  contains a bare `bank`/`Bank` outside its two allowed device-hardware
  sentences. `app/dsp/` is scanned too, once Task 1 has classified and
  renamed its 25 hits (below), with the one allowed exception being the
  citations to the retired simulator's `V2EnvelopeFollowerBank.hpp` — a
  *third*, unrelated sense of "bank" (a bank of DSP units, standard
  audio-engineering usage) that must stay. The script does not scan Sheaf:
  that is `synth::Bank` territory, out of scope by the decision above, and a
  pattern broad enough to reach it would need an allowlist as large as the
  library's own legitimate `Bank` usage — the same failure mode
  `check_no_planning_history.py`'s own header comment warns against ("a
  check that fails the build on legitimate content gets switched off, and
  then it protects nothing").

## Capabilities

### Modified Capabilities
- `froggers-app-surface-layout`: the "Bank selector with direct selection"
  requirement, the "Sixteen-slot encoder grid" requirement, and the opening
  chrome-band paragraph are restated in "page" language for the
  display-selection concept; `FroggersBankId`-addressed content (e.g. "every
  parameter of every bank" as parameter-model addressing) is untouched.
- `froggers-midi-controller-mappings`: the Twister preset requirement's side
  button names change from Bank Next/Bank Previous to Page Next/Page
  Previous, layered on top of the pending change
  `frogg3rs-shift-and-crispy-move-the-tempo`'s own modification of the same
  requirement (see Overlap, below).
- `froggers-vst-host`: the "Automation does not steal the operator's view"
  requirement's display-selection language ("which page the editor is
  currently showing") is disentangled from its parameter-model-addressing
  language ("that parameter's own bank and slot"), which is untouched.

## Overlap with the other active change

`openspec/changes/frogg3rs-shift-and-crispy-move-the-tempo` is still open and
modifies the same Twister preset requirement in
`froggers-midi-controller-mappings/spec.md`, adding Crispy's shifted-tempo
turn and renaming its side buttons' prose from "Bank Next"/"Bank Previous" in
the base spec to the same words, unchanged, in its own delta. This proposal's
delta is written against that change's version of the requirement (Crispy's
shifted turn included), not the older base spec, and only touches the button
names, which that change does not touch. Neither change's task list depends
on the other; whichever lands first, the other's delta still applies
cleanly, since they touch disjoint words in the same requirement body.

## What this proposal does not do, and why

- **Does not rename `FroggersBankId`, its ~600 call sites, or the matching
  rewrite of `froggers-sheaf-parameter-model`'s 88 "bank" occurrences.**
  Named above as real, deliberate, deferred follow-on work — an order of
  magnitude larger diff for a materially less acute ambiguity.
- **Does not rename `synth::Bank`, `MessageIn::SelectParamBank`/
  `NextParamBank`/`PrevParamBank`/`ParamSetAbsoluteOnBank`, or the serialized
  `bankIx` field in Sheaf.** These are Sheaf's own, internally consistent,
  cross-app vocabulary; nothing inside Sheaf calls this same concept "page,"
  so there is no conflation to resolve there, and Sheaf serves consumers
  beyond this app. If the operator wants Sheaf's own naming addressed too,
  that is a decision for a separate Sheaf-side change; this proposal does not
  make that decision on the operator's behalf, and it creates no Sheaf change
  directory.
- **Does not rename by pattern-matching alone anywhere.** Every file this
  proposal touches, including `app/dsp/`'s 25 comment hits, is classified by
  a human/executor reading each hit against the three-way rule above (page
  concept / Sheaf `Bank` type / third-sense DSP citation) before any
  substitution, specifically so a mechanical find-and-replace does not
  rename `EnvelopeFollowers.hpp`'s citation of the retired simulator's
  `V2EnvelopeFollowerBank.hpp`.

## Impact

- `app/FroggersUiSurface.hpp` (action/node-id identifiers, `CurrentBankIndex`).
- `app/FroggersAppCore.hpp` (`ActiveBankIndex`, `FroggersVisibleBankIndex`,
  `activeBankIx_`, `RequestBankSelect`).
- `app/FroggersParameters.hpp` (`kFroggersBankCount`).
- `app/FroggersMidiCatalog.hpp` (catalog display labels).
- `app/vst/FroggersPluginProcessor.hpp`, `app/vst/FroggersPluginProcessor.cpp`
  (`kVisibleBankIndexKey`).
- `app/dsp/Limiter.hpp`, `Delay.hpp`, `Drive.hpp`, `DspMath.hpp`,
  `FilterFx.hpp`, `Fuegoize.hpp`, `VoiceEnvelope.hpp`,
  `EnvelopeFollowers.hpp` (comments; the last file's historical citations
  are excluded).
- Tests: every `app/*Tests.cpp` and `app/vst/FroggersVstHostTests.cpp` file
  that references a renamed identifier (enumerated by Task 1/2's grep, not
  hand-picked here).
- `MANUAL.md`, `assets/manual/twister-controls.json`,
  `assets/manual/twister-preset.png`, `assets/manual/twister-preset-shift.png`.
- `app/check_docs_match_parameter_table.py` (heading suffix).
- New: `app/check_no_bank_page_conflation.py`, wired into `app/Makefile`.
- `openspec/changes/frogg3rs-a-page-is-never-a-bank/specs/froggers-app-surface-layout/spec.md`,
  `.../froggers-midi-controller-mappings/spec.md`,
  `.../froggers-vst-host/spec.md`: this change's own deltas.
- No file under `External/Sheaf` and no submodule pin move.

## Delivery

Delivered by pushing to `main` on `daguilarc/frogg3rs`, never as a pull
request, after `frogg3rs-shift-and-crispy-move-the-tempo` (or this change,
whichever lands second) is rebased against the other's landed diff to the
same requirement body.
