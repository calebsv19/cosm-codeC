# Plug-in Interface

This directory contains the in-process plugin loader used for early extension experiments.

## Security Model (Current)

- Plugins are loaded in-process (`dlopen`/`LoadLibrary`) and therefore are fully trusted.
- A loaded plugin has the same effective permissions as the IDE process (memory, filesystem, IPC access, subprocess launch).
- This means there is no hard isolation boundary yet between plugin code and core code.

## Baseline Guardrails

To make trust explicit, plugin loading now uses default-deny policy:

- Plugin loading is disabled unless `IDE_ENABLE_PLUGINS=1`.
- Plugin path must be allowlisted before load:
  - `IDE_PLUGIN_ALLOWLIST` (path list; `:` separator on macOS/Linux, `;` on Windows), or
  - `IDE_PLUGIN_ALLOWLIST_FILE`, or
  - workspace file: `ide_files/plugin_allowlist.txt`.
- Allowlist entries can be:
  - exact plugin file path, or
  - directory path (allows any plugin under that directory).
- Paths are canonicalized before matching.

Optional manifest validation:

- If `<plugin_path>.manifest.json` exists, loader validates:
  - `name` (string)
  - `version` (string)
  - `capabilities` (array of strings)
- If `IDE_PLUGIN_REQUIRE_MANIFEST=1`, manifest is mandatory.

## Files

| File | Responsibility |
| --- | --- |
| `plugin_interface.h/c` | Plugin lifecycle, allowlist checks, and optional manifest validation before dynamic load. |
| `out_of_process_plugin_host_roadmap.md` | North-star migration path to isolate plugins out-of-process. |

## Trust Boundary Note

These controls improve policy enforcement but do not create sandboxing. For real containment, plugins must move to a separate host process with constrained IPC.
