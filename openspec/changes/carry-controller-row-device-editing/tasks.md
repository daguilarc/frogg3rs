## 1. Bump the Sheaf Pin

- [ ] 1.1 Wait for Sheaf's `finish-controller-row-device-editing` change to
      land (its own tasks done, its delivery gate reviewed), then update
      `External/Sheaf` to that commit.
      Check: `git -C External/Sheaf log -1 --oneline` names a commit that is
      a descendant of `8e56c779` and carries the pressure-mapping removal and
      test fixes described in Sheaf's change.

## 2. Confirm Frogg3rs's Own Suite Still Passes

- [ ] 2.1 Build and run `app/FroggersControllersPageTests.cpp` against the
      new pin. Its
      `real_catalog_registers_one_descriptor_per_device_default_plus_the_uncovered_library_kind`
      and `real_catalog_defaults_generate_and_accept_adds_through_the_view_model`
      cases reference `MakeControllerWizardRegistry` and
      `ControllersPageUI.hpp` symbols the paired Sheaf change edits; neither
      references the removed pressure-mapping API, so both should pass
      unmodified.
      Check: both test cases pass against the new pin without source changes
      in this file.
- [ ] 2.2 Build and run `app/FroggersMidiCatalogTests.cpp` against the new
      pin.
      Check: it passes against the new pin without source changes.
- [ ] 2.3 If either 2.1 or 2.2 fails, that is a real regression the paired
      Sheaf change introduced for a downstream catalog it did not exercise;
      fix it here rather than reopening the Sheaf change, since the defect
      would be in how frogg3rs's own catalog interacts with the shared
      registry, not in Sheaf's library-only behavior.

## 3. Delivery

- [ ] 3.1 Capture and share the six real-app Controllers-page screenshots
      named in this change's Delivery Gate, alongside Sheaf's own, for the
      operator's review before this merges to main.
