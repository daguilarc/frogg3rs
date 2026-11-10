> Applied reviewed planning revision. Fresh-context review passed and Diego explicitly approved implementation; see SOURCE.md.

## Authority and status

Diego approved geometric-depth implementation after a fresh-context review. Execution then stopped on a reproduced scene-write conflict. The app-only absolute-edit repair specified in design.md passed fresh before-code review and its recorded rewrite was accepted before execution resumed. The current code has completed runtime gates and independent after-code reviewer A accepted the source, checks, manual and evidence. Both independent final reviewers accepted delivery of main commit/push commands. Existing count-only evidence is historical. After final after-code review passes, provide commit/push-to-main commands only, without AI attribution. Do not execute commits/pushes or update releases/tags.

## Why

Diego wants Randomize All to explore sparse through dense modulation with equal probability for each density quartile, and wants each selected route's depth magnitude to follow a geometric step law. Source count and depth amount are separate random variables; the previous count-only proposal incorrectly excluded changes to the depth-value law.

## What Changes

- Choose one independent fair density quartile per Randomize All press, with repeats allowed, shared across all banks and drilled-in targets. Draw each target's source count within that quartile using the finite-count mapping in design.md, retaining connected eligibility and the existing zero/one floors.
- For each selected route and each scene pole, independently draw K >= 0 with P(K=k)=2^(-(k+1)), use a fair independent sign for bipolar depths and request D=sign*2^(-K) of full scale. For one-way depths, including drilled Crispy/Crunchy and their descendants, use positive D=2^(-K), preserving one-way behavior and consuming no sign draw. Magnitudes 1, 1/2, 1/4, ... have probabilities 1/2, 1/4, 1/8, ... . The bipolar signed mean is zero; one-way mean and both mean magnitudes are 2/3. This law is independent of the density quartile.
- Define these magnitudes in nominal base modulation-depth units, after the existing depth curve but before further live submodulation; convert the requested amount to the framework's edit coordinate. Use its absolute edit API when no gesture is selected and its existing randomize API while a gesture is held. Do not apply geometric steps to raw knob positions and then unintentionally reshape them again.
- Preserve Randomize Page's current count and value behavior, Crispy's separate page-level bank-count/value behavior (drilled All depths instead use the positive geometric law), source uniqueness, scene membership, held-gesture semantics, scope/cap, reset/reclaim behavior and detectable partial reporting. Do not apply the geometric depth sampler to top-level parameter values.
- Revise tests and the manual for both source counts and depth amounts. Reassess seeded storage/load fixtures under the new draw stream; prior measured numbers are historical evidence, not approved thresholds for the revised sampler.
- Retain the formatting-only openspec/config.yaml repair so the existing task rules load without changing their wording.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `froggers-modulation-slate`: shared fair source-count density quartile plus independently signed geometric depth magnitudes for Randomize All, retaining the stated scope, mutator and storage contracts.

## Impact

Planned app implementation and tests: app/FroggersModulation.hpp, app/FroggersModulationTests.cpp, app/FroggersAudioRoutingTests.cpp; app/FroggersMidiCatalogTests.cpp only if an explicit API change requires it. app/FroggersAppCore.hpp has comment-only hygiene; preserve release/recompute code and ordering. Documentation: MANUAL.md's randomize count/depth explanation. Planning: this change directory and the formatting-only openspec/config.yaml prerequisite. External/Sheaf is read-only for RNG, depth mapping, mutator and storage contracts; no dependency edit is authorized. Preserve the completed add-vco-mipmapped-wavetables work and its waveform provenance/identity regions.

## Non-goals

No new control, density cycling/history, stored quartile, patch-format change, DSP-response redesign, top-level value-law change, dependency edit, unrelated tempo-test repair, sound-quality/performance claim, commit, push or release. The latest user instruction supersedes the earlier exclusion of depth-value-law changes. This remains the existing structural change and its revised derived plan, not a new execution loop.
