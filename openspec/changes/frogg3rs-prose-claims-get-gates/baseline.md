# Baseline gate — `frogg3rs-prose-claims-get-gates`, 2026-09-11

Measured before any file in this change was touched, per task 0.1. Re-measured
rather than carried forward from `frogg3rs-effect-page-hierarchy`'s own run
earlier today, because "that number is still current" is exactly the kind of
claim this change exists to stop taking on trust.

## Commands

```
rm -f app/build/froggers_*tests
/usr/bin/nice -n 10 make -C app test -j2
```

then every test binary run directly by path under `/usr/bin/nice -n 10`, since
the `test` target is known to stop at its first failing check rather than
running everything.

## Result

**371 PASS / 0 FAIL.** All twelve test binaries present, each exit 0. Every
check script passed. Tree at `81ee8cf`, clean apart from this change's own
untracked directory and `frogg3rs-midi-controller-resilience`.

This change sweeps comments and adds check scripts. It compiles no new code
into any test binary, so the PASS count must be unchanged at delivery; any
movement in it is a defect in this change and not a new test.
