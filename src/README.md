# Custom IDE Source Layout

This tree houses every subsystem that powers the IDE runtime. Each top-level
folder is self-contained enough to reason about independently, and the blurbs
below show how the major branches fit together.

| Segment | What lives here | Notes |
| --- | --- | --- |
| `app/` | Process entry points plus `GlobalInfo` singletons that expose core state objects to the rest of the codebase. | Bootstraps SDL, owns the frame loop, and drives background refreshes. |
| `core/` | Headless runtime systems such as analysis, IPC, build/run helpers, loop infrastructure, terminal back ends, and selection helpers. | Anything that should remain usable outside of the pane/UI layer ends up here. |
| `ide/` | The actual IDE UI: pane definitions, widget rendering, layout logic, and the command implementations for each pane. | Organised by pane type; summaries live alongside the code. |
| `engine/` | Rendering infrastructure shared by the IDE runtime. | This is currently centered on the renderer and related frame instrumentation. |
| `Parser/` | Lightweight parser scaffolding kept separate from the main Fisics analysis path. | Initialized during startup, but still intentionally narrow in scope. |
| `build_config.h` | Build‑time switches consumed throughout the source tree. | Treated as configuration rather than code and therefore left as a single header. |

Active directories under `src/` now have local `README.md` files so you can
drill into a subsystem without reconstructing it from the code first.

### Workspace persistence

User preferences such as the active workspace, build/run overrides, and current
run target live in `~/.custom_c_ide/config.ini`.

Workspace-local state is stored under `<workspace>/ide_files/`. That includes
session data, tasks, build output, and the persisted analysis artifacts that
the IDE warms on startup before scheduling a fresh analysis pass.

The analysis lane consumes compiler output through the versioned
`fisiCs.analysis.contract` boundary. When a contract-major mismatch is
detected, the IDE degrades safely (diagnostics/includes remain available while
symbol/token ingestion is suppressed).
