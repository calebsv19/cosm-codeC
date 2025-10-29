# Rendering & Instrumentation Engine

The `engine/` branch hosts reusable rendering and performance modules that can
be embedded in the IDE or other SDL-based apps.

| Subdirectory | Highlights |
| --- | --- |
| [`Render/`](Render/README.md) | Rendering back ends. Includes the Vulkan reference renderer alongside shared rendering utilities. |
| [`TimerHUD/`](TimerHUD/README.md) | Real-time performance HUD and timer infrastructure (the TimeScope project). |

These modules have existing documentation inside their respective folders;
consult those READMEs for detailed usage notes.
