# ide Current Truth

Last updated: 2026-03-29

## Scaffold Migration Status
- `IDE-S0` through `IDE-S5` scaffold migration slices are complete.

## Program Identity
- repo/program directory: `ide`
- canonical symbol/file prefix: `ide`
- private planning bucket: `docs/private_program_docs/ide/`

## Top-Level Layout (Current)
- required scaffold floor:
  - `docs/`
  - `src/`
  - `include/`
  - `tests/`
  - `build/` (created by build workflow)
- currently present optional/support lanes:
  - `dist/`
  - `ide_files/`
  - `idebridge/`
  - `line_history/`
  - `third_party/`
  - `timerhud/`
  - `tools/`

## Subsystem Lanes (Current)
- `src/app/`: startup, global state wiring, and top-level frame loop ownership.
- `src/core/`: runtime core services (input manager, command bus, watcher, terminal, diagnostics, build system, loop/event infrastructure).
- `src/ide/`: pane/UI behavior and editor/tool-panel interactions.
- `src/engine/`: renderer integration and engine-facing rendering layers.
- `src/Parser/`: parser/analysis support lane.

## Lifecycle Entry (Current)
- entrypoint now delegates through canonical wrapper in `src/app/main.c`:
  - `return ide_app_main(argc, argv);`
- canonical lifecycle wrapper is active in:
  - `include/ide/ide_app_main.h`
  - `src/app/ide_app_main.c`
- locked lifecycle stage symbols are present and used by wrapper flow:
  - `ide_app_bootstrap`
  - `ide_app_config_load`
  - `ide_app_state_seed`
  - `ide_app_subsystems_init`
  - `ide_runtime_start`
  - `ide_app_run_loop`
  - `ide_app_shutdown`
- legacy startup/runtime body is preserved as explicit fallback path:
  - `ide_app_main_legacy`

## Verification Contract (Current)
- `make -C ide clean && make -C ide`
- `make -C ide run-headless-smoke`
- `make -C ide visual-harness`
- `make -C ide test-stable`
- `make -C ide test-legacy`

Legacy verification lanes are preserved:
- `make -C ide test-fast`
- `make -C ide test-idebridge`
- `make -C ide test-extended`

## Header Strategy (Current)
- `ide` is currently `src`-header dominant (`src/*.h` heavy, `include/*.h` currently empty).
- policy lock: keep this strategy for internal lanes while adding explicit app-level lifecycle/public entry headers under `include/ide/`.

## Dependency Lane Policy (Current)
- `third_party/` is locked as the explicit vendored shared-subtree lane.
- vendored shared integration remains under:
  - `third_party/codework_shared/`

## Runtime/Temp Lane Policy (Current)
- runtime/config/user-state outputs use explicit app lanes such as `ide_files/` and timer HUD outputs under `timerhud/`.
- ignored mutable/runtime lanes are now policy-locked in `.gitignore`:
  - `tmp/`
  - `ide_files/`
  - `timing.json`
  - `timerhud/ide/timing.json`
  - `timerhud/ide/runtime/`

## Defaults vs Runtime Persistence (Current)
- tracked defaults remain in source-controlled lanes (for example project-shipped defaults such as `timerhud/ide/settings.json`).
- mutable runtime artifacts stay in ignored runtime lanes (`ide_files/`, timer HUD timing/runtime outputs, and `tmp/`).
