# Phase 04 - Plugin Trust Boundary Baseline (`SEC-004`)

Status: `completed`
Owner: `IDE core / Plugin`
Depends on: `completed/phase_03_ipc_auth_and_capability_tiers.md`

## Objective

Move plugin trust from implicit to explicit by requiring enablement + allowlisting, and document the in-process trust model clearly.

## Scope

In-scope:

- Document plugin security model and trust boundary in `src/ide/Plugin/README.md`.
- Add explicit plugin enablement gate (default deny).
- Add plugin path allowlist enforcement before `dlopen`.
- Add optional plugin manifest validation (`name`, `version`, `capabilities`) before load.
- Add roadmap doc for out-of-process plugin host.

Out-of-scope:

- Full out-of-process plugin runtime implementation.
- Plugin signing/PKI verification.

## Step Plan (Execution Checklist)

1. [x] Define plugin policy inputs (env + workspace allowlist file) and default-deny behavior.
2. [x] Add canonical path allowlist enforcement in `loadPlugin`.
3. [x] Add optional manifest validation gate before `dlopen`.
4. [x] Update plugin README with explicit trust model and operational guidance.
5. [x] Add out-of-process plugin host roadmap doc.
6. [x] Compile affected units and verify no unintended runtime behavior changes.

## Acceptance Criteria

- Plugin loading is denied by default unless explicitly enabled.
- Unlisted plugin paths are rejected.
- Listed plugin paths continue loading as before.
- Manifest checks run when sidecar manifest is present (or required by env).
- Documentation reflects current trust model and future isolation direction.

## Implementation Notes

- Updated `src/ide/Plugin/plugin_interface.c`:
  - Added explicit enablement gate (`IDE_ENABLE_PLUGINS=1`).
  - Added canonical allowlist checks using:
    - `IDE_PLUGIN_ALLOWLIST`
    - `IDE_PLUGIN_ALLOWLIST_FILE`
    - `workspace/ide_files/plugin_allowlist.txt`
  - Added optional sidecar manifest validation (`<plugin>.manifest.json`) for:
    - `name` (string)
    - `version` (string)
    - `capabilities` (array of strings)
  - Added support for optional strict manifest mode via `IDE_PLUGIN_REQUIRE_MANIFEST=1`.
- Updated `src/ide/Plugin/plugin_interface.h`:
  - Extended plugin metadata with `capabilities`.
  - Switched metadata fields to owned heap strings for cleanup safety.
- Updated `src/ide/Plugin/README.md`:
  - Added explicit in-process trust boundary documentation and policy usage.
- Added roadmap:
  - `src/ide/Plugin/out_of_process_plugin_host_roadmap.md`

## Validation Performed

- Compiled:
  - `build/debug/ide/Plugin/plugin_interface.o`
