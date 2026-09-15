# Preflight 11 — adjudication

Adjudicated by a context that wrote none of the change, none of the briefs and
none of the six reports. Every ruling below rests on a command I ran myself
against the trees, with its literal output. Agreement between reporters is not
evidence here; a finding survives only where my own run reproduces it.

Audited at frogg3rs `f5d2557a8263adda58842a870cb07de86d21f479`
(`worktree-midi-resilience`) and Sheaf `1fa0113e31a84ebf787407d09b36623db15743b9`
(`midi-resilience-merge`), both clean before and after:

```
$ git rev-parse HEAD ; git status --short
f5d2557a8263adda58842a870cb07de86d21f479
$ git -C External/Sheaf rev-parse HEAD ; git -C External/Sheaf status --short
1fa0113e31a84ebf787407d09b36623db15743b9
```

Nothing was built, installed or committed. The two reproduction experiments ran
in a throwaway mirror under the scratchpad (symlinks to `app/`, `External/`,
`src/`; a copy of `openspec/`), removed afterwards; the worktree was verified
clean after each.

**Nine blocking entries entered, seven distinct** (E-ADV1 ≡ F-ADV2;
E-ADV2 ≡ F-ADV1; E-ADV3 ≡ F-ADV5 after F's exchange revision).

**Ruling: 5 CONFIRMED blocking, 2 confirmed on the facts with severity REFUTED
(downgraded to SHOULD-FIX), 0 OPEN.**

---

# 1. Confirmed blocking findings

Work down this list; nothing here requires opening the six reports.

## BLOCK-1 — frogg3rs task 4.1's gate asserts completeness and measures none of it, and the two repositories contradict each other in writing about what it reads

**Sources:** E-ADV1, F-ADV2 (independent). Confirmed in exchange by A, B, C, D.
**Class:** assertion wider than measurement, compounded by a cross-artifact
contradiction about which signal the gate consumes.
**Blocks:** execution (Sheaf-dependent tasks 3.1 and group 4 start on this
gate's word) **and delivery** (the pin 4.2 makes is this cycle's deliverable).

### What I ran

The gate's own text carries no executable term over tick state:

```
$ sed -n '395,461p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md \
  | grep -n 'grep -c\|\[ \]\|\[x\]\|tick'
1:- [ ] 4.1 **Gate.** Do not start the rest of this group — **except tasks 4.5
17:         ticks 7.7, 7.8 and 7.9 themselves in the working tree before
56:         box count or the commit's own tick state says: a ticked box is not
```

Line 1 is the task's own bullet; line 17 is a descriptive parenthetical inside
step 1; line 56 is the disclaimer. No command reads the count.

Sheaf task 7.8, verbatim:

```
so that `grep -c '^- \[ \]' openspec/changes/midi-controller-resilience/tasks.md`
run against that commit (not the working tree) reports `0`. This is the
convention the frogg3rs half's own cross-repository gate reads
...
The coordinator may also commit at earlier group boundaries; those are
intermediate and are not the object the frogg3rs half pins. Pinning an
intermediate commit would pin a mid-change tree ... what the operator's later
merge then carries is a half-finished Sheaf change.
```

frogg3rs 4.1, verbatim: "...regardless of what the tasks.md box count or the
commit's own tick state says: a ticked box is not evidence of delivered
behaviour, and **this gate no longer reads one**."

Step 1's only identifying term — "`git -C External/Sheaf rev-parse HEAD` names
the exact commit Sheaf's own task 7.8 made" — restates the proposition to be
established as the method for establishing it.

The six cases 4.1 requires to report `[PASS]`, resolved to the Sheaf task that
delivers each (nearest preceding `- [ ] N.M`):

```
HeldModifierCeilingIsWallClockNotPumpCount                        -> 3.5 / 3.10
HeldModifierCeilingDoesNotFireBeforeItElapses                     -> 3.10
EndpointOpenClearsHeldModifierThroughTheAbiEntryPoint             -> 3.7
ReconcileAloneDoesNotClearAHeldModifier                           -> 3.7
EndpointOpenClearsHeldModifierAtRuntimeBinding                    -> 3.7
AppDeviceDefaultCarriesItsDeclaredPreconditionsOntoItsDescriptor  -> 6.4
```

Groups 3 and 6 only. Against Sheaf's own group-4 and group-5 case names:

```
$ grep -cE 'Mismatch|InboundTemplateChange|TemplateChangeFlood|UnmappedInbound|SilentMappedAddresses|SilentSetIsWithheld|EncoderAndAnalogAddressedProfile|PositionAddressedProfile|InboundNonMatchingSysEx' \
    openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
0
```

So a coordinator commit taken at the end of group 6 — which 7.8 **expressly
permits** — satisfies every executable term 4.1 applies, while sru-63 (group 4)
and smi-17 (group 5) are absent and group 7 (7.1's re-enumeration, 7.2's gate
re-run, 7.4's review, 7.5's sweep, 7.6's `coverage.md` repair, 7.7's archive)
has not run. That is exactly the object 7.8 forbids pinning, and the pin is what
ships.

No third thing reads 7.8's signal. frogg3rs's own `check_spec_checks_resolve.py`
cannot substitute — its spec scan is repo-local while its case index is not:

```
$ sed -n '104,118p' app/check_spec_checks_resolve.py   # spec_files(repo)
    specs   = os.path.join(repo, "openspec", "specs")
    changes = os.path.join(repo, "openspec", "changes")
$ grep -n 'for base in ("app", "External/Sheaf")' app/check_spec_checks_resolve.py
224:    for base in ("app", "External/Sheaf"):
```

Sheaf case names resolve; Sheaf `Check:` lines are never scanned.

### Correction to both reporting reports

E and F both write that an accepting commit has groups 4 and 5 "entirely
absent" as the gate's blind spot. That is imprecise in E's and F's favour and
against them at once: `AppDeviceDefaultCarriesItsDeclaredPreconditionsOntoIts
Descriptor` is a **group 6** case, so any accepting commit has groups 1–6 done.
What 4.1 cannot see is **group 7** — precisely the span "complete through its
own task 7.9" asserts. The finding survives with the corrected mechanism.

### Remedy, as a condition on the artifacts

Task 4.1 must carry, as a conjunct alongside its six `[PASS]` cases, an
executable term over the commit itself — `git -C External/Sheaf grep -c '^- \[ \]'
<SHA>:openspec/changes/midi-controller-resilience/tasks.md` reporting `0` — and
must name at least one group-4 and one group-5 case among the cases it requires.
Alternatively Sheaf 7.8's sentence "This is the convention the frogg3rs half's
own cross-repository gate reads" is struck and replaced by what 4.1 actually
reads. The two texts must agree in writing about the signal; today they
contradict. This remedy cannot be met by removing a check (operator ruling 5):
4.1 is the gate the deliverable rests on, and it currently measures less than it
asserts.

---

## BLOCK-2 — frogg3rs task 6.6's two stated conditions are both satisfied by a rewrite that delivers nothing, and no figure anywhere registers it

**Sources:** E-ADV2, F-ADV1 (independent); B7, B8, C1, D and E-ADV14 supply the
count and form defects folded into the remedy.
**Class:** a gate whose accepting condition is met by a null edit; no positive
difference is asserted anywhere.
**Blocks:** delivery. 6.6 is the last gate before `openspec archive` writes the
delta permanently into the tracked `openspec/specs/`, and 6.7 pushes.

### What I ran (reproduced independently in a mirror)

```
=== baseline (mirror, unmodified delta) ===
check-spec-checks-resolve: OK - 55 Check reference(s) resolved, 18 declared as having no automated check
exit=0
=== 6.6 condition 1: deferred-line count ===
10
--- rewrote all 10 'Check: not yet delivered; ...' bodies to 'none.' ---
=== 6.6 condition 1 ===
0
=== 6.6 condition 2 ===
check-spec-checks-resolve: OK - 55 Check reference(s) resolved, 18 declared as having no automated check
exit=0
=== 6.7's STOP grep ===
(empty -> STOP condition satisfied)
=== real worktree ===
(git status --short empty)
```

Both of 6.6's conditions go green on an edit that delivers nothing, and **both
printed figures are byte-identical across the substitution** (55/18 → 55/18),
because the script's declared-manual branch already counted the deferred lines:

```
$ grep -n NO_CHECK app/check_spec_checks_resolve.py
96:NO_CHECK = re.compile(r"^\s*(none|operator step|not yet delivered)", re.I)
```

**The `<M>`/`<N>` remedy both reports propose is not sufficient.** I ran the
third path — rewriting every deferred line to the pre-existing case name already
attached to the very scenario task 4.5 must deliver a new case for:

```
--- all 10 lines rewritten to `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls` ---
6.6 condition 1: 0
check-spec-checks-resolve: OK - 65 Check reference(s) resolved, 8 declared as having no automated check
exit=0
$ grep -n "TEST_CASE(device_defaults_are_valid_and_address_exactly_the_documented_controls)" app/FroggersMidiCatalogTests.cpp
397:TEST_CASE(device_defaults_are_valid_and_address_exactly_the_documented_controls) {
$ grep -n "device_defaults_are_valid_and_address_exactly_the_documented_controls" openspec/specs/froggers-midi-controller-mappings/spec.md
13: / 28: / 33:   (already attached, tracked, permanent)
```

55→65 and 18→8 is the movement an `<M>`/`<N>` delta assertion **expects**. Path 3
defeats that remedy while writing no new test.

The one measurement that would catch it is assigned in one task and disclaimed
in the other:

```
$ grep -n '<M>\|<N>' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
148:      compares its own post-rewrite run of the same script against the `<N>`   (task 1.6)
149:      and `<M>` recorded here, so a number copied instead of measured ...
897:      for writing the line in the first place. No `<M>`/`<N>` count is         (task 6.6)
898:      asserted here ...
```

The by-hand clause that is the only thing standing behind the mechanism
miscounts its own subject:

```
$ grep -hE '^[[:space:]]*-[[:space:]]*Check:' openspec/changes/frogg3rs-midi-preset-preconditions/specs/*/spec.md | grep -c 'not yet delivered'
10
```

against 6.6's "for each of the **nine** rewritten lines" (task 4.4 delivers two
scenarios, so one line falls outside the enumeration). Two of the ten cannot
take the prescribed form at all — `:45` (task 1.8) and `:70` (task 4.8) name a
check *script* half, not a test case, and the prescribed form demands "the real
case name it defines, after a colon".

### Remedy, as a condition on the artifacts

6.6's Check must assert a positive difference a null rewrite cannot produce:
every line that leaves the deferred set names a case that `git diff` on this
branch shows **this change** introduced. State it as that condition, not as an
`<M>`/`<N>` delta, which path 3 satisfies. Fix the count (nine → ten) and state
the form the two script-naming lines take. Strike either 1.6's sentence
assigning the comparison to 6.6 or 6.6's disclaimer of it — they contradict.
Sheaf's own task 7.2 already guards its side against the identical
short-circuit and names frogg3rs 6.6 as having the hole; adopt the same guard
here.

---

## BLOCK-3 — Sheaf task 3.4 says "both" store sites, names three, and the tree has four; the unnamed one is where the ruling matters

**Sources:** B2 (alone). Confirmed in exchange by A, C, D, E, F.
**Class:** the blank — an instruction whose enumeration disagrees with the tree,
leaving the executor to rule on a case no artifact raises, where the two
dispositions have opposite consequences.
**Blocks:** execution.

### What I ran

```
$ grep -n "timestampMicros_\.store" projects/synth/include/synth/browser/BrowserRuntime.hpp
506:        timestampMicros_.store(nowMicros, std::memory_order_release);
674:        this->timestampMicros_.store(timestampMicros, std::memory_order_relaxed);
695:            timestampMicros_.store(timestampMicros, std::memory_order_relaxed);
721:        this->timestampMicros_.store(timestampMicros, std::memory_order_relaxed);

enclosing signature, by walking back:
506 -> 463 bool StartAudioWorklet(EMSCRIPTEN_WEBAUDIO_T suppliedContext = 0)
674 -> 667 void Process(...)
695 -> 683 bool ProcessAudioWorkletPlanarBlock(
721 -> 718 void MessageTick(std::uint64_t timestampMicros)

$ grep -rn "StartAudioWorklet" openspec/changes/midi-controller-resilience/
(no output)
```

Task 3.4's literal instruction (`tasks.md:619-621`): "change **both**
`timestampMicros_.store(...)` call sites in `BrowserRuntime.hpp` (`Process`,
`ProcessAudioWorkletPlanarBlock` **and** `MessageTick`)". Count, list and tree
disagree three ways.

The unnamed site is the clock-origin re-seed, and it is the only one of the four
that is not a `relaxed` store:

```
$ sed -n '501,506p' projects/synth/include/synth/browser/BrowserRuntime.hpp
        const auto localNowMicros = static_cast<std::uint64_t>(
            std::llround(std::max(0.0, emscripten_get_now() * 1000.0)));
        const auto nowMicros = ApplyTimestampEpochOffset(
            localNowMicros,
            timestampEpochOffsetMicros_.load(std::memory_order_acquire));
        timestampMicros_.store(nowMicros, std::memory_order_release);
```

Clamped, an epoch correction that lowers the origin is silently discarded.
Unclamped, the supplier the Ceiling reads can regress at worklet start — the
exact regression the clamp exists to prevent. And 3.4's prescribed replacement
specifies `std::memory_order_relaxed` on both halves, which would downgrade this
store's `release` without saying so.

The task's own trace also mislocates the seed. It reads "seeded once from
`emscripten_get_now()` corrected by `ApplyTimestampEpochOffset` **at
`Prepare()`**". `Prepare` is defined at `:453` and *called* at `:500`; the seed
stores are at `:506`/`:507`, inside `StartAudioWorklet`, after that call. No
store to `timestampMicros_` exists inside `Prepare` (the four-site grep above is
exhaustive).

### Remedy, as a condition on the artifacts

Task 3.4 must name all four sites, rule explicitly on `StartAudioWorklet`'s
`:506` (clamp or exempt, with the reason), and either preserve its
`memory_order_release` or state the downgrade. Correct the trace sentence from
`Prepare()` to `StartAudioWorklet:506`.

---

## BLOCK-4 — Sheaf task 3.6's export derivation states a predicate false for 14 of 29 definitions, and asserts one-way containment with no count

**Sources:** F-ADV3 (alone). Confirmed in exchange by A, B, C, D, E; B and D each
narrowed a statement of their own on it.
**Class:** a stated derivation predicate the tree refutes, paired with an
assertion that passes more comfortably the less the derivation finds.
**Blocks:** execution — the task instructs the executor to confirm a premise by
counting, and the count returns the opposite.

### What I ran

```
$ grep -c 'extern "C"' cpp/BrowserRuntimeAbi.cpp
30
$ grep -nE 'extern "C".*;[[:space:]]*$' cpp/BrowserRuntimeAbi.cpp
3:extern "C" synth_browser::RuntimeAbi* synth_browser_create_runtime();

classification of every non-declaration `extern "C"` line by whether the NEXT line begins '{':
  declaration-only:              1
  defs with { on the next line:  15
  defs with a multi-line signature: 14
    :51 initialize  :74 process  :81 start_audio_worklet  :89 set_audio_input_source
    :99 clear_audio_input_source :107 consume_pending_audio_request
    :119 set_timestamp_epoch_offset :162 submit_midi_endpoints
    :169 submit_audio_devices :176 dequeue_midi_action :182 dequeue_file_export
    :188 deliver_midi :197 dequeue_midi_output :203 midi_diagnostics
```

Task 3.6's confirmation instruction (`tasks.md:796-799`): "confirm this by
counting: every other `extern "C"` line in that file is a declaration
**immediately followed by a body**, and only this one ends in `;`". The `;` half
is true (exactly one). The "immediately followed" half is false for 14 of 29,
and an executor mechanizing the sentence the task tells it to count by derives
**15**. Its assertion (`tasks.md:792`) is "asserts **each one is present** in
`EXPORTED_FUNCTIONS`" — one-way containment, no count, no set equality — so a
15-name derivation is green, and greener the less it derives.

Set equality is available and exact today, in both directions, at no cost:

```
definitions: 29   exports: 29
defs - exports: []
exports - defs: []
```

**Narrowing I checked, which does not rescue it.** Task 3.7 does state a
positive control for this derivation ("whose break is removing the new
`_synth_browser_*` name from `EXPORTED_FUNCTIONS`"), so an under-derivation that
misses the *new* export is discoverable when that control fails to fire. It does
nothing for the other 13 multi-line definitions, which stay outside the
derivation permanently, nor for the task's own false premise.

### Remedy, as a condition on the artifacts

**Strike** the confirmation sentence "every other `extern "C"` line in that file
is a declaration immediately followed by a body" — it is false, and removal is
the repair (operator ruling 5). The task's primary wording ("a declaration
followed by a `{ ... }` body, not merely a declaration terminated by `;`") is
correct as stated and needs no change. Replace the containment assertion with
set equality against `EXPORTED_FUNCTIONS`, which is exact today.

---

## BLOCK-5 — the recovery half's rule 2 is one literal, and the delta's promoted Check line claims a condition the script never checks

**Sources:** E-ADV3; F-ADV5 (independent, revised SHOULD-FIX → BLOCKING in F's
exchange). Confirmed by A, B, C, D.
**Class:** a promoted claim wider than the mechanism that backs it.
**Blocks:** delivery. Task 6.7 runs `openspec archive` before commit and push, so
the overstated line is written verbatim and permanently into the tracked
`openspec/specs/` inside this cycle.

### What I ran

I implemented the three rules from design.md's "Recovery half — rule, floor,
positive control" and task 1.8, without reading any reporter's script. Control
against today's text:

```
$ python3 adj_rec.py MANUAL.md
rule1: ### Shift names no Rebuild phrase
rule1: ### Shift names no Ceiling phrase
rule1: ### Shift names no EndpointOpen phrase
rule2: ### Shift contains 'pressed and released again'
rule1: ### Hold Drill names no Rebuild phrase
rule1: ### Hold Drill names no Ceiling phrase
rule1: ### Hold Drill names no EndpointOpen phrase
exit=1
```

Seven control lines, same set and order as A's, C's, E's and F's independent
implementations. Then a rewrite that offers, as its **first** recovery for the
exact failure the proposal opens with, pressing the button whose address has
stopped transmitting — in both subsections:

```
### Hold Drill: "...press and release the Hold Drill button one more time..."
### Shift:      "...press and release the Shift button once more to drop the shift..."

$ python3 adj_rec.py rewrite.md          # bare rule-3 literal
OK
exit=0
$ grep -c "pressed and released again" rewrite.md
0
```

All three rules pass. The delta scenario's THEN — "does not state a recovery
that requires the Shift address to transmit" — is false while its Check is
green.

**This settles B's OPEN item.** B verified `### Shift` only and left the
`### Hold Drill` half open. I constructed both and both pass.

The permanent text is what makes it a delivery defect.
`specs/froggers-midi-controller-mappings/spec.md:45` promises the script "fails
unless ... (2) do not contain 'pressed and released again' **or any other
sentence naming the modifier's own button as what clears it**", while task 1.8
and design.md both state that the second half is never built ("the broader
instruction is a drafting rule for task 3.1's own prose, not a second mechanical
pattern ... none is invented here"). The two artifacts that disclose the gap stay
in `openspec/changes/`; the line that overstates it is promoted.

### Remedy, as a condition on the artifacts

Narrow the delta's Check line at `spec.md:45` to what the script mechanizes:
delete the clause "or any other sentence naming the modifier's own button as
what clears it" from the promoted text. That is a removal, and it is the repair
operator ruling 5 prefers — 1.8 and design.md both decline to invent a second
pattern, so the alternative (building one) is not on offer in this change.

---

# 2. Severity disputes ruled

## B1 — the plugin-exclusion literal has three (four) spellings — facts CONFIRMED, severity **SHOULD-FIX**

Positions: B, C, F graded BLOCKING; D and E graded SHOULD-FIX.

**The facts reproduce.** Every occurrence, whitespace-flattened:

```
tasks.md   ...contains the literal substring "not available in the plugin." Run it now,...     1.8 rule 3, PERIOD inside quotes
tasks.md   ...controller" with no "not available in the plugin" phrase anywhere...             1.8 control 4, BARE
tasks.md   ...must also contain the literal phrase "not available in the plugin," naming...    3.1, COMMA inside quotes
tasks.md   ...standalone and browser builds — not available in the plugin, which takes...      3.1 worked example
design.md  ...contain the literal substring "not available in the plugin" — task 3.1's...      BARE
design.md  ...controller" without also stating "not available in the plugin" anywhere...       BARE
spec.md    ...phrasing above), also contain "not available in the plugin"                      BARE
$ grep -c "not available in the plugin" MANUAL.md
0
```

**The consequence is real and I measured it.** Under the period spelling, task
3.1's own prescribed sentence fails rule 3 in both subsections:

```
$ python3 adj_rec.py rewrite.md "not available in the plugin."
rule3: ### Shift reconnects without the plugin exclusion
rule3: ### Hold Drill reconnects without the plugin exclusion
exit=1
$ python3 adj_rec.py rewrite.md "not available in the plugin,"
OK
```

**Why I rule it SHOULD-FIX, not BLOCKING.** The value is written down, in the
artifact task 1.8 defers to in its own first sentence: "Implement **exactly the
recovery-half rule design.md's** 'Recovery half — rule, floor, positive control'
section states". design.md gives the bare form twice, unambiguously (once
followed by an em-dash, once by "anywhere"), and task 1.8's own fourth positive
control and the spec delta both spell it bare — three bare against two whose
trailing mark sits where American convention puts sentence punctuation inside
quotes. This is not a blank the executor fills; it is a written value stated
inconsistently, with a named tie-breaker that resolves it and resolves it
correctly. The failure mode is also the loud one: the period reading produces a
red `check-*`, and the 1.8→3.1 window is already declared knowingly red, so the
executor is looking at this check either way. Nothing gets past.

**Remedy:** one spelling, written once — the bare form design.md already uses —
with its punctuation disposition stated the way rule 2's is ("the exact current
text, with no trailing comma"). Second limb, which I confirm and which the same
remedy should cover: nothing in 1.8, 3.1 or design.md normalises whitespace
before matching, and `MANUAL.md` is hard-wrapped, so whether a 26–42 character
phrase lands whole on one line is decided by where the executor's rewrite breaks.
Say that the match is against whitespace-normalised text.

## B3 — Sheaf rule 2(a)'s delivering-vs-context classification — facts CONFIRMED, severity **SHOULD-FIX**

Positions: B, A, D graded BLOCKING; C and F graded SHOULD-FIX.

**The facts reproduce exactly.** Mechanizing rule 2(a)'s own stated pattern over
all three Sheaf deltas:

```
deferred lines: 25
not matching rule 2(a)'s literal pattern: 1
  specs/synth-midi-instrument/spec.md:97
```

`:97`'s shaped tokens sit two sentences from the task citation, and the line
separately cites task 3.6 in a role that must classify as context. Task 1.9's
positive control (e) is defined on `:71`, which **does** match the easy shape, so
no control exercises the classification on the one line that needs it. The rule's
whole provision for that line is the phrase "or the equivalent", inside a
preamble that says "All three are fixed here; none is left to the executor."

**Why I rule it SHOULD-FIX.** I walked every wrong classification to see whether
any lets an unsatisfied claim reach the archive. None does.

- *Literalist mechanization* (no cited task classifies as delivering): 2(a) is
  unsatisfiable, `:97` is RED at task 1.9 itself. Task 1.9's own last clause
  catches this in the same task: "The script is green on arrival: run it
  immediately after wiring, before any other change in this group, and record
  `exit 0`." A written expectation, contradicted immediately, in the task that
  authored the rule. That is a stall with its own correction signal.
- *Loose mechanization* (every cited task classifies as delivering): the line
  stays green longer, but condition (b) — "no backticked token on the line
  already resolves as a real test case" — is **independent of the
  classification** and turns the line red the moment any of the three named cases
  exists. `openspec archive` will not archive past incomplete tasks without
  `--yes`, so at archive time either the cases exist (2(b) reds the line) or the
  delivering tasks are ticked (2(a) reds it).
- *3.6 delivering, 3.7 context*: spurious red, a stall.

Every reading either reds loudly or is closed by 2(b). Nothing certifies a claim
that was not delivered. That is the distinction this round's BLOCKING grade is
for, and this does not meet it.

**Remedy:** replace "or the equivalent" with a stated pattern that matches
`:97`'s prose shape, or move `:97` explicitly under rule 3's carve-out and say
so; and redefine positive control (e) on `:97` rather than `:71`, so the control
exercises the shape that needs it.

## E-ADV3 — recovery-half rule 2 — ruled **BLOCKING (delivery)**

Positions: E and A graded BLOCKING (blocks execution); F revised SHOULD-FIX →
BLOCKING; B and D confirmed but re-graded it to blocks-delivery; C graded
SHOULD-FIX.

I rule with B and D on the mechanism and with E, A and F on the grade: see
BLOCK-5. It does not block execution — task 3.1 instructs this cycle's executor
correctly, and the check never goes red on the prescribed prose. It blocks
delivery, because `openspec archive` promotes the overstated Check line into the
permanent `openspec/specs/` inside this cycle's own delivery sequence, and the
gate cannot fail on the half it promises. C's SHOULD-FIX rests on the same
observation about task 3.1 that I accept; what C does not weigh is that the
over-claim ships and outlives every artifact that discloses it.

---

# 3. SHOULD-FIX and NOTE findings, consolidated

## Verified here

| id(s) | finding | my measurement |
|---|---|---|
| A1 ≡ D1 | The main-checkout coordination parenthetical records three files and calls the disposition "empty" | `git -C ~/Desktop/frogg3rs status --short app/ MANUAL.md QUICK_DICT.md README.md` names **five** today, including `MANUAL.md` and `QUICK_DICT.md` — both on this change's own edit path. The recorded disposition ("empty by this broader command too") is contradicted. Today's hunks (`MANUAL.md` @452, @579-582) are disjoint from this change's `MANUAL.md:258-390`, so the procedure still reaches "proceed"; it is the recorded measurement that is stale, not the rule. SHOULD-FIX. |
| A2 ≡ D3 | The sibling-change overlap sweep says `bank-addressed-absolute-write` cites `MidiController.cpp`/`ControllersPageUI.hpp` "once" | Three citations: `bank-addressed-absolute-write/design.md:120`, `:145`, `proposal.md:60`. A graded NOTE, D graded SHOULD-FIX; I take **SHOULD-FIX** — the sentence advertises having read ("read, not assumed"), which makes a wrong number a different defect from a stale estimate. |
| A4 | Sheaf's `Check:`-line census is stale | Task 1.9 records 103 files / 132 lines repo-wide and 31 for this change, 24 deferred. Today: **103 / 133**, this change **32**, deferred **25**. Mitigated — the task's own text says "re-run both rather than reusing those numbers". NOTE. |
| B6 ≡ C4 | Task 1.9's by-hand carve-out re-application names four multi-token lines | Five exist: `synth-midi-instrument/spec.md:71,76,81,97` **plus `synth-runtime-ui/spec.md:44`**. The task's own re-run instruction is scoped to one file (`this file`), which would not find the fifth. SHOULD-FIX. |
| B7 ≡ C1 ≡ E-ADV14 | Task 6.6 says "nine" rewritten lines | Ten measured. Two of them (`:45` task 1.8, `:70` task 4.8) name a check-script half and cannot take the prescribed `path: case` form. **Folded into BLOCK-2's remedy.** |
| B8 | Task 6.6's claim about its own script's short-circuit | Confirmed at `check_spec_checks_resolve.py:96`. B's original "blocks neither" grading was withdrawn in its own exchange. **Folded into BLOCK-2.** |
| B12 | Task 1.9 instructs adding a marker all three deltas already carry | `<!-- check-lines-resolve -->` present at line 4 of all three delta files. NOTE. |
| F-ADV6 (ingredient) | Several scenarios share one case name | Four deferred lines (`:55`, `:60`, `:65`, `:83`, tasks 4.3/4.4×2/5.2) all name `device_defaults_declare_their_preconditions`. Ingredient verified; the finding itself not adjudicated. |

## Not adjudicated — verify at the point of repair

I did not run these. Each is recorded with its source ids so the author can
verify it as part of the repair that touches the same artifact.

- **frogg3rs artifacts:** A3 (the `SliderElement` sentence vs the recorded
  disassembly); C2 (an undisclosed red window from task 5.1 to 5.6); B10
  (4.5/4.6 declared gate-free while 4.6's positive control edits a file Sheaf
  group 3 rewrites in the same tree); B16 (wiring position of the knowingly-red
  check in `app/Makefile`'s linear `test:` list); B17 ≡ F-ADV7 (nothing re-runs
  `check-spec-checks-resolve` over the promoted lines after the archive);
  E-ADV13 (6.7's STOP fires only on a third entry); E-ADV9 ≈ F-ADV4 (the drift
  half's comparison sits inside HTML comments, covered only by a non-failing
  WARNING); E-ADV4 (rule 3's plugin literal satisfied by any sentence in the
  subsection — same rule as B1, different failure direction); E-ADV6, E-ADV7 and
  F-ADV8 (all three on `check_modified_requirements_restate_promoted.py`:
  requirement-wide bullet matching, the ungated prose body, and an unbaselined
  dropped-scenario figure — treat as one repair); F-ADV6 (shared case name);
  E-ADV10 (no gate ties the manual's trigger vocabulary to
  `HeldModifierClearSource`); D2 (the coordination command is scoped to four
  paths while the change also edits `openspec/`); D4 (a promoted spec enumerates
  the preset list the seventh device default extends, named in no artifact); D5
  (the Launch Control XL's single exact-match alias vs the manual sentence — note
  operator ruling 1 settles the preset itself, not this sentence).
- **Sheaf artifacts:** B4 (the prescribed clamp is not an atomic
  read-modify-write, and the prescribed test cannot catch that — repair
  alongside BLOCK-3, same task); B5 (rule 3's basename-uniqueness scan has no
  written scope while task 1.5 creates the duplicates); B9 (task 7.2 compares
  binary paths against a name nothing chooses); B11 (task 1.3's "seven, not
  five" gate); B13 (task 1.1's `HeldButton` expectation names two `openspec/`
  hits); B14 (rule 3's bare `<case>` form unreachable under its own carve-out);
  C3 (smi-16 reverses a promoted clause with no declaration, where the sibling
  delta declares the identical reversal); E-ADV5 (the new gate re-introduces the
  defect its frogg3rs twin documents fixing); E-ADV8 (sru-63's observed-address
  set has no stated lifetime across a profile rebuild); E-ADV11 ≡ F-ADV9 (the
  rate-limiter bound is off by one between 5.2/5.3 and design.md); E-ADV12
  (smi-17's template-change case passes most comfortably when the recognizer is
  absent); E-ADV15 (the scope-shrink guard is itself scoped to directories that
  still have a marker).
- **Hygiene, both repositories:** B15 (steps needing an operator, hardware, a
  network install, or a state this cycle cannot reach — largely settled by
  operator rulings 1–4); B18 (the measured-versus-stated literal table, of which
  B itself withdrew one line in its exchange); D6 (six byte-identical preflight
  files committed in both repositories); D7 (a tracked scratch progress file in
  `openspec/` with three unresolvable commit ids); D8 (`sheaf-patch`'s test
  target invoked by nothing); D9 (the single-live-copy guard checks a directory
  and branch name never used); D10 (tracked agent-transcript dumps poison
  unscoped operand greps).

---

# 4. Open items

**None of the five confirmed blocking findings is open, and none needs the
operator.** Each is settled by text already in the trees, and every remedy is a
change to an artifact this change owns.

Items the reporters left open, and how they stand:

| item | raised by | disposition |
|---|---|---|
| Does the forbidden-recovery rewrite also pass all three rules in a `### Hold Drill` subsection? | B (OPEN) | **SETTLED by me — yes.** Constructed and run for both subsections; `exit=0`. See BLOCK-5. |
| Did any of A, B, C reach the multi-line-signature accepting path at lower severity? | F (OPEN) | **SETTLED by me — none did.** A judged the `EXPORTED_FUNCTIONS` claims TRUE; B measured with a `;`-terminated predicate (which it withdrew in its own exchange as being the *remedy* predicate, not the stated rule); C only located the file; D and E do not mention it. F-ADV3 was reached by exactly one report. |
| Overlap of F-ADV4/6/7/8 with peers' lower-severity findings | F (OPEN) | **SETTLED as duplicates by inspection:** F-ADV7 ≡ B17; F-ADV4 ≈ E-ADV9; F-ADV8 belongs with E-ADV6/E-ADV7 (one script, one repair); F-ADV6 shares C's `device_defaults_declare_their_preconditions` observation. All below blocking; none re-verified. |
| Behaviour of the four Sheaf test cases at the commit 4.1 would pin | D | Not observable from here and not needed: they do not exist today, which is 4.1's own declared positive control. Not an open item against the change. |

Nothing in this ruling requires an operator decision. Operator rulings 1–4 are
respected throughout and no finding above reopens any of them.

---

# 5. Verdict for the pair

## REJECT

Five confirmed blocking findings stand against the pair:

1. **BLOCK-1** — frogg3rs task 4.1's cross-repository gate asserts Sheaf is
   complete through 7.9 while measuring only groups 3 and 6, contains no
   executable term over the commit, and explicitly refuses the one signal Sheaf's
   own 7.8 says it reads. *Blocks execution and delivery.*
2. **BLOCK-2** — frogg3rs task 6.6's two conditions are both satisfied by a
   rewrite that delivers nothing, with both printed figures unmoved; and the one
   measurement that would catch it is assigned in task 1.6 and disclaimed in 6.6.
   *Blocks delivery.*
3. **BLOCK-3** — Sheaf task 3.4 says "both" store sites, names three, and the
   tree has four; the unnamed one is the epoch-corrected re-seed, the only
   `release` store, where clamping and not clamping are both wrong in opposite
   ways. *Blocks execution.*
4. **BLOCK-4** — Sheaf task 3.6's stated confirmation predicate is false for 14
   of 29 definitions, and its assertion is one-way containment with no count, so
   an under-derivation of 15 is green. *Blocks execution.*
5. **BLOCK-5** — the recovery half's rule 2 is one literal; a rewrite offering
   the forbidden recovery in other words passes all three rules in both
   subsections, and the delta's Check line promoting that over-claim is written
   permanently into `openspec/specs/` at 6.7's archive. *Blocks delivery.*

Four of the five are one class: **an assertion stated in prose whose measurement
is narrower than the assertion, with no count, set or difference pinned to hold
the two together.** BLOCK-3 is the other class: a blank the executor must fill,
on the premise the whole Ceiling trigger rests on.

Two findings entered as BLOCKING and leave as SHOULD-FIX (B1, B3), each because
the value the executor needs is written down somewhere the task names, and each
wrong reading fails loudly rather than certifying something false.

I would lift REJECT on the five remedies in §1 as stated, plus B1's single
spelling and B3's control (e) redefinition. The SHOULD-FIX and NOTE items in §3
belong in the same pass but do not hold the change on their own.

---

# 6. Coverage note

**Reached independently by more than one report:**

- BLOCK-1 — E-ADV1 and F-ADV2, separately, on the same two artifacts.
- BLOCK-2 — E-ADV2 and F-ADV1, separately; E ran the substitution, F ran it in
  its own mirror.
- BLOCK-5 — E-ADV3 (as BLOCKING) and F-ADV5 (constructed by reading, graded
  SHOULD-FIX, revised to BLOCKING in F's exchange after running it).

**Reached by exactly one report:**

- BLOCK-3 — B2 alone. A tree fact no amount of re-reading the artifacts produces;
  every other auditor confirmed it only after B named it.
- BLOCK-4 — F-ADV3 alone. A, B and C all name `EXPORTED_FUNCTIONS` and none
  found the accepting path; B measured past it with the remedy's own predicate.

**Axes that found nothing blocking this round:**

- **A's axis** — claim truth at the symbol, and Impact completeness. A's own
  finding was that the citations are exact, the recorded measurements reproduce,
  and Impact is complete in both repositories; I found nothing to contradict
  that. Its four findings are all stale-figure or wrong-count defects.
- **C's axis** — do the spec deltas, the promoted specs, the code and the check
  names agree. They do, to the degree C could falsify and to the degree I
  re-measured (the `Check:` census, the multi-token enumeration, the deferred
  counts).
- **D's axis** — opening hygiene sweep and active-work overlap. Ten findings, no
  blocking.

All three of those axes issued ACCEPT and all three revised to REJECT in the
exchange on peers' evidence. The common gap each named in its own words is the
same one: they checked whether the artifacts' claims about the **trees** were
true, and not whether the artifacts' own **gates** could fail. Every blocking
finding this round came from the runnability axis (B) or the two adversarial
axes (E, F).
