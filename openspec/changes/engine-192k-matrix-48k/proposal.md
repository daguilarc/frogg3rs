# Four engine samples per matrix frame

Diego requests the Smart Grid encoder rate split and a Lean behavioral model in Sheaf, retaining Froggers features.

Source baseline: Froggers 305ca6a; Sheaf 3b84a6d. Reference Smart Grid e7afa9761bb2238b5affc3c61d92be27aec88476.

Pipeline: host input and transport -> four engine-rate modulation-source steps -> eighth-order Butterworth source low-pass decimation -> one editable-tree matrix evaluation with the phase-1/Fuego/phase-2 seam -> root interpolation -> engine-rate audio DSP -> existing Sheaf output FIR -> host recording/output/scope cursor.

At a 48 kHz host the engine is 192 kHz and the matrix 48 kHz. Other host rates retain the four-to-one ratio. Existing recursive depths, gestures, arbitrary scene/voice counts, crossfade/attenuverter blend modes, normalized absolute edits and quiescent skipping remain in the authoritative ParameterGroup path. No prepared graph replaces those semantics. Unchanged scalar nested leaves reuse their evaluated image; fresh raw-center comparison and mutation invalidation retain edit responsiveness.

Impact: app core and parameters, delay capacity, Sheaf parameter timing and reusable rate bridge, native test/launcher build dependencies, plugin smoke/host/packaging validation, tests and formal behavioral contracts. Desktop uncommitted randomization-density changes are excluded from this worktree.

Validation: native Sheaf parameter tests; Froggers headless, modulation, parameter-model, routing and scope suites; focused cadence/interpolation/source rejection checks; Lean build where toolchain is available. The model is an abstraction, not a proof of C++ refinement or floating-point execution. Audio FIR latency is 143 engine samples (35.75 host samples).
