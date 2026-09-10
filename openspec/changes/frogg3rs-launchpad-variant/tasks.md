# Tasks — `frogg3rs-launchpad-variant`

## 1. The presets

- [x] 1.1 Record each Launchpad preset's model on the profile it installs.
- [x] 1.2 Require it in the catalog tests, against the same table that pins
      each preset's id and controller.

## 2. Gates

- [x] 2.1 `make -C app test` (capped at `-j2`), against the moved pin: 13
      binaries, 356 passes, 0 failures, recipe exit 0.

## 3. Delivery

- [x] 3.1 Move the submodule pin to the Sheaf commit carrying
      `launchpad-model-on-the-row`.
- [x] 3.2 Commit and push to `main`.
