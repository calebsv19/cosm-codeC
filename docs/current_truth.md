# codeC Current Truth

Last updated: 2026-08-08

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
- Public release version is now `0.3.0`.
- The Libraries tool panel now has a first dependency-view mode over the
  compiler include graph:
  - `Headers` keeps the existing bucketed header/index view
  - `Deps` shows source-to-project-header dependency edges from the IDE
    include-graph store
  - `Graph` converts the same include-graph snapshot into shared
    `kit_graph_struct` node/edge arrays for layered dependency visualization
    with click selection, repeat-click open, shared `core_viewport2d`
    drag-pan, and cursor-anchored wheel zoom
  - `Graph` suppresses quiet zero-edge sources; sidepane widths use compact
    source/dependency node lanes in stable content coordinates, so resizing the
    pane reveals more viewport content instead of stretching the graph. Compact
    mode keeps the source/dependency lane gap constrained enough for skinny
    panels, places all graph nodes, uses the pane height for initial vertical
    distribution, routes edges orthogonally, supports zooming out to 18%, shows
    labels on hover/selection, and uses a compact node HUD for graph totals,
    zoom, role, and source/header connectivity
  - the graph view is not a package-manager or build-execution surface

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active subsystems:
  - `src/app`, `src/core`, `src/ide`, `src/engine`, `src/Parser`
- Dependency lane:
  - vendored shared subtree under `third_party/codework_shared/`
  - the default presentation path links vendored `vk_renderer 1.3.1` and
    `vk_runtime 0.6.0`; renderer instance/device/queue ownership aliases the
    runtime lifecycle instead of creating a second Vulkan owner
  - this adoption is presentation-only: IDE source does not call the runtime
    compute, residency, or timing APIs
  - IDE button chrome resolves hover, selected, pressed, disabled, and focused
    state through vendored `kit_ui` via the app-local `ide_ui_button.*`
    adapter; pane-specific meaning and action dispatch remain IDE-owned
  - Libraries dependency graph layout/hit math uses vendored
    `kit_graph_struct`; graph pan/zoom camera state uses vendored
    `core_viewport2d`; panel drawing and source/header semantics stay IDE-owned
- Compiler analysis lane:
  - `fisiCs` live-file/background analysis feeds diagnostics, symbols, tokens,
    header index data, and include dependency edges into IDE-local stores
  - diagnostics preserve source span length, optional hint text, normalized
    category/code names, and stage metadata through analysis persistence and
    IPC `diag` responses
  - the Errors panel consumes the richer diagnostic metadata for a selected-row
    detail inspector with severity, source span, category/code/stage, message,
    hint, explanation text, expanded include-stack/macro-trace/details context
    rows with per-row source navigation where available, units/dimension
    details with optional symbol-units enrichment, panel-local search over
    message/file/hint/category/code/stage, keyboard row navigation, and
    Enter-to-jump behavior
  - diagnostic explanation/context sidecars preserve compiler explanation
    catalog entries, `include_stack`, `macro_trace`, and units `details`
    payloads for richer diagnostics consumers
  - declaration-level units attachments persist by stable symbol id and can be
    published through the `symbols` IPC response
  - the Control panel has a separate `Units` target that searches declaration
    unit attachments by symbol name, unit text/name, unit family, and dimension
    text through the existing right-panel search and Active/Project scope
    controls
  - Control tree rows use typed payloads and stable row IDs for section, file,
    symbol, unit, and empty-message rows; double-click activation resolves
    those payloads before legacy fallbacks, so row opens no longer depend on
    rendered labels or stale tree user data
  - activating Control tree rows preserves the selected Target, Scope, Match,
    Editor, Parse, and Units filters; persisted Control settings restore on
    relaunch and are not reset by active-file refreshes
  - Units row activation treats the owning units file as authoritative for
    source navigation, preventing stale per-row source paths or SDK/header
    symbol matches from redirecting project unit rows into unrelated headers
  - editor goto/navigation opens the resolved file, places the cursor on the
    resolved line/column, frames long-file targets into the upper reading band,
    and gives the cursor row a subtle gutter marker, line-number tint, and
    short goto flash
  - build graph summaries persist from `fisiCs.build_graph` artifacts and can
    be published through the `build_graph` IPC response
  - runtime memory-check sidecar summaries persist from
    `memory_check_report_v1` artifacts and can be published through the
    `memory_reports` IPC response
  - mutating IPC edit requests require the session auth token, peer UID checks
    where supported, workspace-confined existing file paths, bounded unified
    diff resources, and hash verification by default; explicit
    `check_hash=false` edits are limited to single-file diffs and report
    `hash_policy=unchecked_single_file`

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
- User-triggered Build and Run actions print a terminal-facing `[Trust]`
  notice before command dispatch, including action, working directory, command
  source, and command display string. Passive analysis remains separated from
  these trusted-workspace execution actions.
- The Git tool panel launches branch/status/log/watcher/stage/commit commands
  through argv-based `fork`/`execvp` with child-side `chdir` into the selected
  project path. It does not interpolate project paths or commit messages into
  shell command strings.

## Verification Contract
- Vulkan rollout proof:
  - `make -C ide vulkan-rollout-contract` checks the exact canonical shared
    source commit and vendored runtime/renderer versions
  - `make -C ide -j1 vulkan-rollout-self-test` runs a display-backed,
    validation-enabled lifecycle proof with startup readback, high-DPI extent
    checks, resize/swapchain recreation, changed capture readback, and renderer
    restart
  - generated proof receipts and BMP captures live under ignored
    `build/targets/<target-triple>/debug/vulkan-rollout/`
- Proof-of-life ladder:
  - `make -C ide run-headless-smoke`
    - builds the app and runs `test-stable`
    - expected final line: `run-headless-smoke completed.`
  - `make -C ide visual-harness`
    - builds the app output and prints the manual UI validation command
    - expected final line starts with: `visual-harness build gate ready:`
    - this is not an automated screenshot or visual-artifact capture route
  - `make -C ide visual-artifact`
    - launches the IDE in `IDE_VISUAL_ARTIFACT_ONCE=1` mode, writes the first
      rendered frame, and exits
    - expected final line:
      `visual-artifact ready: visual_artifacts/ide_first_frame.bmp`
    - generated artifacts live under ignored `ide/visual_artifacts/`
  - `make -C ide package-desktop-self-test`
    - validates packaged resources and launcher self-test behavior
    - expected final line: `package-desktop-self-test passed.`
- Core gates:
  - `make -C ide clean && make -C ide`
  - `make -C ide run-headless-smoke`:
    aggregate non-interactive smoke coverage
  - `make -C ide visual-harness`:
    build-only readiness; does not execute the interactive editor shell
  - `make -C ide visual-artifact`:
    display-backed first-frame proof artifact at
    `ide/visual_artifacts/ide_first_frame.bmp`
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
- Current release target chain requires notarized closeout before artifact/distribution success:
  `release-verify-signed -> release-notarize -> release-staple -> release-verify-notarized -> release-artifact -> release-distribute`.
- Icon contract is active with optional `PACKAGE_APP_ICON_SRC` / `PACKAGE_APP_ICONSET_SRC` inputs.
- Target-aware packaging/release outputs are active:
  - `build/targets/<target-triple>/...`
  - `codeC-<version>-macOS-x86_64-stable` Intel artifact naming
- Launcher/runtime resource hardening is active:
  - packaged runtime roots are copied into the writable runtime lane before launch
  - `VK_RENDERER_SHADER_ROOT` now resolves through the runtime root instead of a bundle-relative direct shader path
  - `IDE_RUNTIME_DIR` overrides are validated and canonicalized before runtime
    copy/removal; invalid relative, broad, or symlink-resolved unsafe override
    paths fall back to the default user runtime lane or temp runtime lane
  - launcher `--print-config` and `--self-test` report
    `IDE_RUNTIME_DIR_SOURCE` for runtime-root provenance

## Runtime/Temp Policy
- Mutable/runtime lanes are explicitly ignored (`tmp`, `ide_files`, timer output lanes).
- Release artifacts run a hygiene check before `release-artifact` completes:
  the public manifest uses artifact basenames/digests instead of local
  evidence paths, and ZIP contents are scanned for private/generated runtime
  lanes.
- Defaults vs runtime persistence split is maintained.

## Current Boundary
- The IDE Vulkan presentation adoption is complete at the local proof and
  packaged-desktop boundary. Preserve the runtime-owned renderer lifecycle and
  automated validation/readback/resize/restart contract; do not infer compute
  acceleration or begin another application rollout from this result.
- Preserve workspace-root contract and wrapper diagnostics stability while helper extraction and pane-centralization follow-ups continue.
- Treat structural-upgrade scaffolding as historical/archived; the live work is maintenance plus subsystem improvement, not another scaffold migration phase.
- Workspace Authoring is at host-attach parity; real pane/module mutation should
  start a new plan rather than extending `IDEWA1`.
- The compiler bridge ingestion/publication lane is complete for the current
  Phase 3 boundary: diagnostics metadata, explanations, diagnostic context,
  units attachments, build graph summaries, include graph data, and
  memory-check reports are available as IDE-owned stores and IPC query
  surfaces. The next bridge-related work should be planned as UI/presentation
  slices that consume these stores, not as more compiler ABI work.
- The dependency graph is a first visual bridge over include edges only. It
  uses `kit_graph_struct` for wide-layout/hit math and app-local compact
  sidepane layout for narrow widths; compact layout uses stable content-space
  lanes and then is controlled by `core_viewport2d` cursor-anchor zoom and
  drag-pan camera math.
  Collapse groups, richer
  selection state, edge labels, build-manifest nodes, and deeper package or
  dependency execution semantics remain future work.

## History and Deep Lane References
- Full phase/execution history is in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ide/`
- This file is the compressed public current-state contract.
