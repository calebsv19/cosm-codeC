# Rendering & Instrumentation Engine

The `engine/` branch hosts reusable rendering infrastructure for the IDE
runtime.

| Subdirectory | Highlights |
| --- | --- |
| [`Render/`](Render/README.md) | Rendering back ends. Includes the Vulkan reference renderer alongside shared rendering utilities. |

There is no standalone `src/engine/TimerHUD/` subtree in this codebase anymore.
Timing and instrumentation helpers are wired into the current renderer/runtime
path rather than documented as a separate source branch.
