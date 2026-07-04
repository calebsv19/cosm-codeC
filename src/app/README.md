# Application Layer

The `app/` folder owns process start-up, the canonical lifecycle wrapper, and
the shared "singletons" that other modules query for global state. The process
entry file stays deliberately thin; substantial runtime, analysis, pane, and
rendering behavior lives in `core/`, `ide/`, and `engine/`, while this layer
wires the pieces together.

| File | Responsibility |
| --- | --- |
| `main.c` | Process entry point that delegates directly to `ide_app_main(...)`. |
| `ide_app_main.c` | Canonical lifecycle wrapper. It owns the scaffold stage sequence (`bootstrap`, `config_load`, `state_seed`, `subsystems_init`, `runtime_start`, `run_loop`, `shutdown`), wrapper diagnostics, runtime-loop handoff, and fallback into the legacy editor runtime. |

## Subdirectories

- [`GlobalInfo/`](GlobalInfo/README.md) – Definitions for the global state
  structs that are shared across modules (core state, project info, etc.).
