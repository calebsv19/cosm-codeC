# ide Current Truth

Last updated: 2026-04-04

## Scaffold Migration Status
- `IDE-S0` through `IDE-S5` scaffold migration slices are complete.

## Post-Scaffold Font Pass Status
- post-scaffold font-size standardization lane is complete.
- runtime zoom controls are active and modifier-gated:
  - `Ctrl/Cmd +` increase
  - `Ctrl/Cmd -` decrease
  - `Ctrl/Cmd 0` reset
  - handler lane: `src/core/InputManager/input_keyboard.c`
- zoom preference persists through workspace prefs:
  - key: `font_zoom_step`
  - load/save lane: `src/app/GlobalInfo/workspace_prefs.c`
- startup applies persisted zoom before runtime init when env override is absent:
  - lane: `src/app/ide_app_main.c`
- closeout verification rerun on 2026-03-30:
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke`
  - `make -C ide visual-harness`

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
- `W1` wrapper contract upgrade is now landed for IDE:
  - typed `IdeAppContext`
  - typed dispatch request/outcome seam
  - explicit ownership ledger
  - deterministic shutdown stage completion

## Verification Contract (Current)
- `make -C ide clean && make -C ide`
- `make -C ide run-headless-smoke`
- `make -C ide visual-harness`
- `make -C ide test-stable`
- `make -C ide test-legacy`
- packaging verification:
  - `make -C ide package-desktop`
  - `make -C ide package-desktop-smoke`
  - `make -C ide package-desktop-self-test`
  - `make -C ide package-desktop-copy-desktop`
  - `make -C ide package-desktop-remove`
  - `make -C ide package-desktop-refresh`
- `/Users/<user>/Desktop/codeC.app/Contents/MacOS/ide-launcher --print-config`
- `W1` verification rerun (2026-04-02):
  - `make -C ide`
  - `make -C ide run-headless-smoke`
  - `make -C ide visual-harness`
- `W2` verification rerun (2026-04-02):
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke` (outside-sandbox for IPC bind checks)
  - `make -C ide visual-harness`
  - `make -C ide test-stable` (outside-sandbox for IPC bind checks)
- `W3-S0/S1` verification rerun (2026-04-02):
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke` (outside-sandbox for IPC bind checks)
  - `make -C ide visual-harness`
  - `make -C ide test-stable` (outside-sandbox for IPC bind checks)
- `W3-S2` verification rerun (2026-04-02):
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke` (outside-sandbox for IPC bind checks)
  - `make -C ide visual-harness`
  - `make -C ide test-stable` (outside-sandbox for IPC bind checks)
- `W3-S3` verification rerun (2026-04-02):
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke` (outside-sandbox for IPC bind checks)
  - `make -C ide visual-harness`
  - `make -C ide test-stable` (outside-sandbox for IPC bind checks)

## Cross-Program Wrapper Wave State
- `W0`: complete (shared canonical wrapper contract frozen)
- `W1`: complete for `ide`
- `W2`: complete for `ide` (wrapper diagnostics normalization lane)
- `W3`: complete (`S0`/`S1`/`S2`/`S3`/`S4` complete; typed runtime-loop adapter + handoff seams + seam diagnostics/ownership hardening + closeout docs/commit lane landed)

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

## App Packaging Status (Current)
- IDE packaging contract is now at full parity with scaffold standard:
  - `package-desktop`
  - `package-desktop-smoke`
  - `package-desktop-self-test`
  - `package-desktop-copy-desktop`
  - `package-desktop-sync`
  - `package-desktop-open`
  - `package-desktop-remove`
  - `package-desktop-refresh`
- launcher diagnostics now include:
  - `--print-config` for non-interactive root inspection
  - startup logging to `~/Library/Logs/IDE/launcher.log` (with tmp fallback)

## Release Readiness Snapshot
- `IDE-RL0` complete:
  - release contract locked in `makefile`:
    - `RELEASE_PRODUCT_NAME := codeC`
    - `RELEASE_PROGRAM_KEY := ide`
    - `RELEASE_BUNDLE_ID := com.cosm.codec`
  - canonical version source present:
    - `VERSION` (`0.1.0`)
- `IDE-RL1` complete:
  - portable dylib closure script is active:
    - `tools/packaging/macos/bundle-dylibs.sh`
  - launcher runtime hardening is active:
    - writable runtime root (`~/Library/Application Support/IDE/runtime`, tmp fallback)
    - runtime Vulkan ICD generation (`vk/MoltenVK_icd.json`)
    - runtime env exports (`VK_ICD_FILENAMES`, `VK_DRIVER_FILES`)
  - transitive `@rpath` closure was fixed for image/decode stack (`libjxl_cms`, `libsharpyuv`, `libwebp` chain)
  - release bundle audit enforces no unresolved `@rpath` links.
- `IDE-RL2` complete:
  - signing/notary/staple/verify passed.
  - accepted notary id:
    - `2d41d9a4-061d-4bb5-b863-1f4356e79ddf`
  - trust evidence:
    - `spctl --assess --type execute --verbose=2 dist/codeC.app`
      - `accepted`
      - `source=Notarized Developer ID`
- `IDE-RL3` complete:
  - release artifacts:
    - `build/release/codeC-0.1.0-macOS-stable.zip`
    - `build/release/codeC-0.1.0-macOS-stable.zip.sha256`
    - `build/release/codeC-0.1.0-macOS-stable.manifest.txt`
  - Desktop copy refreshed to:
    - `/Users/calebsv/Desktop/codeC.app`
- `IDE-RL4` complete:
  - Finder launch/log evidence confirms app progresses past dyld into runtime:
    - `[TimerHUD] IDE profiling DISABLED ...`
    - `[TaskJSON] File is empty or corrupt; resetting to empty task list.`
- `IDE-RL5` complete:
  - one-shot release command succeeds:
    - `make -C ide release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: ... (J4ZR8UWD8G)" APPLE_NOTARY_PROFILE="cosm-notary"`

## Defaults vs Runtime Persistence (Current)
- tracked defaults remain in source-controlled lanes (for example project-shipped defaults such as `timerhud/ide/settings.json`).
- mutable runtime artifacts stay in ignored runtime lanes (`ide_files/`, timer HUD timing/runtime outputs, and `tmp/`).
