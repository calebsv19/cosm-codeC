# ide Future Intent

Last updated: 2026-04-02

## Scaffold Alignment Targets
- keep `ide` on the required scaffold floor (`docs/src/include/tests/build`) while preserving current editor/runtime behavior.
- keep canonical lifecycle wrapper (`ide_app_main`) stable with locked stage symbols.
- keep scaffold verification aliases stable without removing legacy phase gates.

## Startup Shape Target
- lock startup/runtime stage order to the shared scaffold lifecycle model:
  1. `ide_app_bootstrap`
  2. `ide_app_config_load`
  3. `ide_app_state_seed`
  4. `ide_app_subsystems_init`
  5. `ide_runtime_start`
  6. `ide_app_run_loop`
  7. `ide_app_shutdown`

## Verification Contract Target
- keep both lanes active:
  - scaffold aliases:
    - `run-headless-smoke`
    - `visual-harness`
    - `test-stable`
    - `test-legacy`
  - legacy lanes:
    - `test-fast`
    - `test-idebridge`
    - `test-extended`
    - phase gates (`test-phase1` through `test-phase5`)

## Header and Path Policy Target
- maintain `src`-dominant internal headers for this migration window.
- route new app-level/public lifecycle entry APIs through `include/ide/`.
- keep `third_party/` as the explicit vendored-shared subtree lane.
- keep runtime/temp ignore lanes locked (`tmp/` plus mutable runtime artifact lanes such as `ide_files/`/`timerhud` outputs).

## Migration Mode
- execute bounded, phase-based rollout (`IDE-S1` -> `IDE-S5`) with verification at each phase.
- use non-destructive structure changes (copy -> verify -> remove).
- keep public docs (`ide/docs/`) and private execution docs (`docs/private_program_docs/ide/`) synchronized per phase.

## Current Migration Note
- `IDE-S3` lifecycle/symbol wrapper lock is now landed.
- `IDE-S4` naming/path hygiene and runtime-temp lane policy lock is now landed.
- `IDE-S5` stabilization closeout is now landed.
- post-scaffold font-size lane is now documented as complete with standardized closeout title:
  - `Post-Scaffold Font Size Standardization`
- packaging parity pass is now landed:
  - IDE now includes full `package-desktop*` target parity (`remove` + `refresh` included).
  - launcher diagnostics are aligned with packaging contract (`--print-config` + startup log file).
- next focus is routine maintenance and subsystem improvements; no additional scaffold/font baseline migration lane is pending for `ide`.
- cross-program wrapper initiative state:
  - `W0` complete (canonical shared wrapper contract frozen)
  - `W1` complete for `ide` (typed context/dispatch contract landed in `src/app/ide_app_main.c`)
  - `W2` complete for `ide` (wrapper-level structured lifecycle failure reporting is now normalized in `src/app/ide_app_main.c`)
  - `W3` complete:
    - `S0` baseline freeze + verification rerun complete
    - `S1` typed runtime-loop seam definition complete (`ide_app_runtime_loop_adapter(...)`)
    - `S2` typed run-loop handoff cutover complete (`ide_app_runtime_loop_handoff_ctx(...)`)
    - `S3` seam diagnostics/ownership hardening complete
    - `S4` closeout complete (tracker/docs sync + commit packaging)
  - next focus:
    - optional targeted extraction only where high-value (`W4+`), otherwise maintain current wrapper contract as reference baseline.
