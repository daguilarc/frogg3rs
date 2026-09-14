# Research index — frogg3rs-delay-width-wysiwyg-repair

This index names the six files this directory is meant to hold; none of them
exist yet. Each is to be extracted verbatim from the measurement sessions
that did this work, by a pass separate from the one that wrote `proposal.md`
and `tasks.md`, and land before this change archives. This table is the
target for that extraction, not the record itself.

The figures each file's topic produced are already inlined in `proposal.md`,
one line at a time, wherever it cites this directory — those are the
conclusions. The files below hold the underlying measurement data behind
each conclusion — the sweeps, grids and sessions that produced it — not a
second statement of the conclusion alone.

| File | Holds |
|---|---|
| `cross-feed-widening.md` | The correlation figures across Stereo width's travel for today's shipped law and the repaired law, at Feedback 0.0/0.35/0.7; the Feedback-0 structural dead zone (`fbEff == 0`); the panning-law candidates measured and rejected (right-channel silence at Feedback 0, level imbalance at Feedback 0.7); and the corrected `dwid == 0` divergence measurement — which weights hold bit-exact and which drift, and after how many samples, at Feedback 0.35 and 0.70. |
| `retired-mechanisms.md` | The periodicity argument and its retraction (1408/2552 vs. 980/980 samples, the window-choice sensitivity that produced three different winning periods); the 220 Hz tonal-material point and the full frequency sweep that contextualizes it (range 0.00032-0.99998, mean 0.64); the fixed-0.125-weight comparison against the shipped law. |
| `prior-art-crossfeed-separation.md` | The Logic Pro Stereo Delay citation (per-channel Delay/Feedback/Mix, Crossfeed as its own control, decoupled at 0%), the Bitwig Delay citation (Width and Cross Feedback as independent controls), the Valhalla Delay citation (ping-pong as one discrete routing mode among several, not a continuous width-linked weight), and the correction that `RESEARCH2-drive-delay.md`'s own Bitwig citation supports keeping crossfeed and width separate rather than a complementary ratio knob between them. |
| `instrument-derivation.md` | Why band-limited noise (50 Hz one-pole lowpass over the seed-20260913 LCG burst) replaces white noise as the probe stimulus; the autocorrelation e-folding width (152.8 samples) against `widthSpread`'s maximum at `dtim = 0.3` (164.29 samples) that gives the probe dynamic range; why `dtim = 0.3` is the pinned operating point — the `dtim >= 0.8` noise-dominated failure and the 1000-sample-window failure that ruled out other choices. |
| `side-mid-rejection.md` | The `sqrt((1-rho)/(1+rho))` identity connecting side/mid energy ratio to correlation at equal channel levels; the DC-reachability argument for using Pearson correlation instead of the raw identity; the unequal-level case showing the ratio drifts toward 1.0 regardless of correlation. |
| `read-at-capacity.md` | The `ReadAt`/capacity trace: `baseSeconds` and `widthSpread` at `dtim = 0.99`, full Stereo width, default Width balance; the resulting `timeR` against the 96000-sample (2.0 s) capacity; the measured wrap distance and the short lag it produces. |
