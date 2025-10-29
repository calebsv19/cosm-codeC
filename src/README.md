# Custom IDE Source Layout

This tree houses every subsystem that powers the IDE runtime. Each top-level
folder is self-contained and can be reasoned about independently; the blurbs
below outline how the pieces fit together and point to the sub‑directory
README files for deeper context.

| Segment | What lives here | Notes |
| --- | --- | --- |
| `app/` | Process entry points plus `GlobalInfo` singletons that expose core state objects to the rest of the codebase. | Bootstraps SDL, owns the frame loop, and drives background refreshes. |
| `core/` | Engine‑agnostic systems such as the command bus, input routing, file IO, build/run helpers, and diagnostics hooks. | Anything that should remain usable outside of the IDE UI ends up here. |
| `ide/` | The actual IDE UI: pane definitions, widget rendering, layout logic, and the command implementations for each pane. | Organised by pane type; summaries live alongside the code. |
| `engine/` | Rendering back ends and performance tooling (TimerHUD, optional Vulkan reference app, etc.). | These modules can be embedded by other apps and therefore keep their own READMEs. |
| `Libraries/` | External or experimental third‑party drops that the project vendors. | Currently mostly placeholders; see subfolder READMEs for integration status. |
| `Parser/` | Early experiments for language parsing and analysis utilities. | Mostly stubs today; kept separate so parser work can evolve in isolation. |
| `Project/` | Sample content loaded by the IDE (fixtures, generated build outputs). | Useful when developing features without pointing the IDE at a real workspace. |
| `build_config.h` | Build‑time switches consumed throughout the source tree. | Treated as configuration rather than code and therefore left as a single header. |

Every subdirectory under `src/` has its own `README.md` with details about the
files it contains and how that module interacts with the rest of the system.
