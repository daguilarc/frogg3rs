# frogg3rs-firmware-verification Specification

## Purpose
TBD - created by archiving change frogg3rs-omni-audit-repairs. Update Purpose after archive.
## Requirements
### Requirement: The firmware tests have a checked-in invocation
The firmware test suite under `test/firmware/` SHALL be runnable from one checked-in command that configures, builds and runs every registered test, and that command SHALL run in continuous integration whenever the firmware core or the tests change.

#### Scenario: One command runs every firmware test
- **WHEN** an operator runs the firmware test target from the repository root
- **THEN** every test registered in `test/firmware/CMakeLists.txt` is built and executed
- **AND** the command exits non-zero if any test fails

#### Scenario: A firmware change runs the tests without a person
- **WHEN** a push or pull request changes a file under `src/core/` or `test/firmware/`
- **THEN** continuous integration runs the same target
- **AND** a failing test fails the run

### Requirement: Every firmware DSP path has a test that can fail
Each parameter path in the firmware core that a flag or knob gates SHALL have a test that sets the gate and asserts a value that moves, so that a path is never counted as tested while every test leaves its gate closed.

#### Scenario: Fuegoization is tested at a non-zero mask
- **WHEN** the fuegoization test runs
- **THEN** it asserts the scrambled value at a mask other than zero equals the documented formula
- **AND** it asserts that value differs from the unscrambled input for at least one row

