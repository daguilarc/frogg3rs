# Froggers-on-Sheaf app

## Sheaf

The app is built on [Sheaf](https://github.com/jvictor0/Sheaf), a git submodule
at `External/Sheaf`. Development tracks a fork of it rather than upstream.

## Audio and matrix rates

At the preferred 48 kHz device rate, the audio engine and modulation sources run
at 192 kHz. The editable modulation matrix and Fuego/UI phases run at 48 kHz;
filtered sources feed each evaluation, and processed parameter values interpolate
across four engine samples and pass through a low-pass that removes the
interpolation images. Other device rates retain the same 4:1 ratio.
Recording and scope history stay at device rate. The output FIR adds 143 engine
samples (35.75 device samples) of delay; the plugin reports 36 samples.

The reusable bridge and Lean behavior model live in Sheaf's `projects/synth`.
With Lean 4.32.0 installed, `make -C External/Sheaf/projects/synth formal-test`
checks the proofs, source correspondence and allowed axioms. See that model's
`formal/README.md` for the proof boundary.

## Build the launcher

```bash
./app/build-launcher.sh
```

Use the script rather than a hand-written `make` command. `EXTRA_APP_HEADERS`
must list every header the app compiles: the sheaf-patch Makefile's
`$(APP): $(APP_HEADERS)` rule treats them as literal prerequisites and
generates no `-MMD` dependency files, so a header left off the list is
silently untracked and edits to it produce a build that succeeds while
ignoring the change. The script globs, so the list cannot go stale.

## Tests

```sh
cd app && make test
```

Builds and runs every test binary. It stops at the first failing binary, so check that all twelve ran
before reading a partial result as green.

## Status

A full port of the Froggers synth onto Sheaf: the DSP, parameter/bank model,
modulation slate, and portable UI surface live under `app/` alongside this
file; `app/vst/` hosts the VST3/AU plugin build, `app/browser/` hosts the
browser build and its site/e2e suite, `app/standalone/` hosts the Windows
standalone build, a CMake project producing the same application
`app/build-launcher.sh` produces on macOS, and `app/android/` hosts the
Android app build (the `.jucer` project and `build-android.sh`). `Main.cpp` is a minimal
build-skeleton translation unit kept separate from the real app entry points
(`Froggers.hpp`/`FroggersMain.cpp`) -- see its own header comment.
