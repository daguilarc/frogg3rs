## ADDED Requirements

### Requirement: The Controllers page catalog integration tracks Sheaf's row-editing contract

WHEN the `External/Sheaf` pin changes, THE app SHALL keep `app/FroggersControllersPageTests.cpp` and `app/FroggersMidiCatalogTests.cpp` passing against `MakeControllerWizardRegistry(FroggersMidiCatalog())` and the Controllers page's row rendering, updating them in the same change that bumps the pin if Sheaf's own row contract moved; SHALL NOT assume a row-level pressure-mapping editor, since Sheaf's runtime library edits a pressure mapping only through its grid row; and SHALL rely on Sheaf's own device-label width check to cover this app's device display names by literal, not by adding a JUCE-linked test to this tree's JUCE-free app core (`app/Makefile`'s `check_no_juce` target).

#### Scenario: The catalog test tracks the registry's device-plus-library-fallback shape

- **WHEN** the Sheaf pin is bumped
- **THEN** `real_catalog_registers_one_descriptor_per_device_default_plus_the_uncovered_library_kind` and `real_catalog_defaults_generate_and_accept_adds_through_the_view_model` continue to pass without source changes, or are updated in the same change that bumps the pin
- Check: `app/FroggersControllersPageTests.cpp: real_catalog_registers_one_descriptor_per_device_default_plus_the_uncovered_library_kind`, `real_catalog_defaults_generate_and_accept_adds_through_the_view_model`

#### Scenario: No row-level pressure-mapping editor is assumed

- **WHEN** any frogg3rs source file references the Controllers page row's editable surface
- **THEN** it does not reference a row-level pressure-mapping list, add, delete, or field-commit API, since Sheaf's runtime library offers none; a pressure mapping is edited only through its grid row
- Check: none; verified by absence, not by a test — no frogg3rs source file references a pressure-mapping row API.
