# Froggers-on-Sheaf app

## Sheaf

The app is built on [Sheaf](https://github.com/jvictor0/Sheaf), a git submodule
at `External/Sheaf`. Development tracks a fork of it rather than upstream.

## Build the launcher

```bash
./app/build-launcher.sh
```

Use the script rather than a hand-written `make` command. `EXTRA_APP_HEADERS`
must list every header the app compiles: the sheaf-patch Makefile treats them as
literal prerequisites (`Makefile:47-48`) and generates no `-MMD` dependency
files, so a header left off the list is silently untracked and edits to it
produce a build that succeeds while ignoring the change. The script globs, so
the list cannot go stale.

## Tests

```sh
cd app && make test
```

Builds and runs every test binary. It stops at the first failing binary, so check that all twelve ran
before reading a partial result as green.

## Status

A full port of the Froggers synth onto Sheaf: the DSP, parameter/bank model,
modulation slate, and portable UI surface live under `app/` alongside this
file; `app/vst/` hosts the VST3/AU plugin build, and `app/browser/` hosts the
browser build and its site/e2e suite. `Main.cpp` is a minimal build-skeleton
translation unit kept separate from the real app entry points
(`Froggers.hpp`/`FroggersMain.cpp`) -- see its own header comment.
