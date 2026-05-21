# codeC Current Truth

Last updated: 2026-05-21

## Program Identity
- Repository/program directory: `ide`
- Release/product identity: `codeC`
- Canonical symbol/file prefix: `ide`

## Current Shipped State
- Scaffold migration slices `IDE-S0` through `IDE-S5` are complete.
- Post-scaffold font-size pass is complete with persisted zoom controls (`Ctrl/Cmd +`, `-`, `0`).
- Data-path contract is complete with workspace-root-first policy and startup fallback handling.
- Wrapper wave lanes are complete through `W3` with typed runtime-loop adapter + diagnostics hardening.
- Intel macOS `x86_64` desktop packaging is complete under the shared target-contract flow.
- Workspace Authoring first host attach is complete:
  - `Alt+C` then `Alt+V` enters/cancels authoring through `kit_workspace_authoring`
  - normal runtime does not show authoring HUD/reminder text
  - active pane overlay uses shared overlay button geometry/hit testing with
    IDE-owned pane labels
  - shared full-screen Font/Theme overlay provides live theme, font-preset, and
    text-size preview
  - accepted-only persistence is active; `Apply` saves theme preset, font
    preset, and text-size step, while cancel/toggle-off restores the entry
    baseline without saving
- Public release version is now `0.2.0`.

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active subsystems:
  - `src/app`, `src/core`, `src/ide`, `src/engine`, `src/Parser`
- Dependency lane:
  - vendored shared subtree under `third_party/codework_shared/`

## Lifecycle and Runtime Contract
- Entry delegates through `ide_app_main(...)`.
- Canonical lifecycle wrapper stage symbols are active (`bootstrap` through `shutdown`).
- Legacy startup/runtime body remains available as explicit fallback.
- Workspace-root-first startup policy is explicit:
  1. stored workspace path from `~/.custom_c_ide/config.ini` when it is still
     valid
  2. `IDE_DEFAULT_WORKSPACE` when set to a valid directory
  3. `~/Desktop/CodeWork` when available
  4. current working directory as the final fallback
- Invalid stored workspace state does not stay sticky:
  - codeC warns when the saved workspace disappears
  - startup switches to the next valid workspace root and preserves normal
    prefs behavior from there
- Build/run preferences remain app-local alongside that workspace choice in
  `~/.custom_c_ide/config.ini`, while packaged launcher/runtime roots are still
  resolved through the packaged runtime path contract

## Verification Contract
- Core gates:
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke`:
    aggregate non-interactive smoke coverage
  - `make -C ide visual-harness`:
    build-only readiness; does not execute the interactive editor shell
  - `make -C ide test-stable`
  - `make -C ide test-legacy`
- Packaging gates:
  - `make -C ide package-desktop`
  - `make -C ide package-desktop-smoke`
  - `make -C ide package-desktop-self-test`
  - `make -C ide package-desktop-copy-desktop`
  - `make -C ide package-desktop-remove`
  - `make -C ide package-desktop-refresh`

## Packaging and Release Snapshot
- Desktop packaging contract is at parity with standard app bundle flow.
- Release-readiness lanes are complete through signed/notarized/stapled distribution verification.
- Icon contract is active with optional `PACKAGE_APP_ICON_SRC` / `PACKAGE_APP_ICONSET_SRC` inputs.
- Target-aware packaging/release outputs are active:
  - `build/targets/<target-triple>/...`
  - `codeC-<version>-macOS-x86_64-stable` Intel artifact naming
- Launcher/runtime resource hardening is active:
  - packaged runtime roots are copied into the writable runtime lane before launch
  - `VK_RENDERER_SHADER_ROOT` now resolves through the runtime root instead of a bundle-relative direct shader path

## Runtime/Temp Policy
- Mutable/runtime lanes are explicitly ignored (`tmp`, `ide_files`, timer output lanes).
- Defaults vs runtime persistence split is maintained.

## Current Boundary
- Preserve workspace-root contract and wrapper diagnostics stability while helper extraction and pane-centralization follow-ups continue.
- Treat structural-upgrade scaffolding as historical/archived; the live work is maintenance plus subsystem improvement, not another scaffold migration phase.
- Workspace Authoring is at host-attach parity; real pane/module mutation should
  start a new plan rather than extending `IDEWA1`.

## History and Deep Lane References
- Full phase/execution history is in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ide/`
- This file is the compressed public current-state contract.
