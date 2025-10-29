# Application Layer

The `app/` folder owns process start-up and the shared “singletons” that other
modules query for global state. Its code is deliberately thin: anything with
substance lives in `core/` or `ide/`, while this layer wires the pieces
together.

| File | Responsibility |
| --- | --- |
| `main.c` | SDL entry point. Sets up global state, initialises rendering, enters the frame loop, and hands off to subsystems defined in `core/` and `ide/`. |

## Subdirectories

- [`GlobalInfo/`](GlobalInfo/README.md) – Definitions for the global state
  structs that are shared across modules (core state, project info, etc.).
