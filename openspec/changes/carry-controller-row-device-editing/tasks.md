## 1. Correct the Promoted Requirement This Work Falsifies

- [ ] 1.1 Write the spec delta as a MODIFIED requirement against
      `froggers-sheaf-runtime-app`, reproducing
      "The MIDI configuration page fits this application's window in every
      state" from `openspec/specs/froggers-sheaf-runtime-app/spec.md` with
      exactly these corrections and every other clause, scenario and `Check:`
      line carried through verbatim:
      - the header's first line is identity: the row's name and its device
        label, plus Variant for a Launchpad — not the device kind;
      - the page shows a controller's device by its display name: the
        descriptor the row's wizard id resolves against, or the bound MIDI
        input's stored endpoint label when none resolves — replacing the clause
        that says it shows the device kind by its display name;
      - the add row offers this application's presets followed by exactly one
        Custom entry — replacing "a Custom entry per device kind and nothing
        else".
      In the scenario "The row reads as its parts", a MIDI Fighter Twister row
      shows its device label on the first line, not the name-and-kind pair the
      scenario currently asserts.
      Check: the modified requirement differs from the promoted one only in the
      clauses above, confirmed by diffing the two texts, and
      `openspec validate` accepts the delta. Keep each requirement's SHALL on
      the first line of its body; the validator only reads it there.
- [ ] 1.2 Delete the previously proposed `froggers-midi-controller-mappings`
      delta from this change.
      Check: the change contains one spec delta, against
      `froggers-sheaf-runtime-app`, and `openspec validate` accepts the change.

## 2. Correct the Catalog Header Comment

- [ ] 2.1 Read `app/FroggersMidiCatalog.hpp`'s header comment and determine
      which of its statements are false of the page as it renders now. Two are
      claimed: that the add control is a "Layout dropdown" where the page
      captions it Preset, and that choosing Custom leaves a slot's mappings
      untouched and editable by hand where Custom now adds an empty Generic
      row. Correct whatever is false and leave whatever is true alone.
      Check: each statement in that comment is true of the current page,
      established by reading the code it describes rather than the comment; the
      corrected text names no commit, task, plan or document.

## 3. Bump the Pin and Re-run This App's Suite

- [ ] 3.1 Update `External/Sheaf` to the commit that carries the paired
      change's implementation.
      Check: `git -C External/Sheaf show --stat HEAD` and the log between the
      old pin and the new one show the pressure-mapping removal and the
      connect-message test in the source tree, not only openspec text. A
      descendant commit is not sufficient: the pin this change inherited
      already points at a descendant that carries proposal text and no
      implementation.
- [ ] 3.2 Build and run `app/FroggersControllersPageTests.cpp` and
      `app/FroggersMidiCatalogTests.cpp` against the new pin. Neither
      references the removed pressure-mapping API, and both link
      `MakeControllerWizardRegistry` and `ControllersPageUI.hpp`, which change.
      Check: both binaries pass against the new pin without source changes in
      either file. If either fails, that is a real regression in how this app's
      catalog meets the shared registry; fix it here rather than reopening the
      Sheaf change.
- [ ] 3.3 Run this app's full suite, not only the two binaries above.
      Check: the run's own pass count is reported as measured, against the 413
      passes across twelve binaries this change inherited.

## 4. Delivery

- [ ] 4.1 Capture the six real-app Controllers-page screenshots this change's
      Delivery Gate names, alongside Sheaf's, for the operator's review before
      this merges to main.

## Named for the documentation step, not done here

`MANUAL.md` tells a player the add row offers "a **Custom** entry for each
device kind". The page offers one. User documentation is reviewed as its own
step after the code diff, so the correction belongs there, with this change
named as what made it false.
