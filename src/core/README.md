# Core Runtime Systems

Code under `core/` holds reusable subsystems that do not depend on any specific
pane or UI. These modules can be embedded in other front ends or tested in
isolation.

| Subdirectory | What it provides |
| --- | --- |
| [`Analysis/`](Analysis/README.md) | Async workspace analysis, cache metadata, diagnostics/symbol/token persistence, include graph tracking, and library indexing. |
| [`CommandBus/`](CommandBus/README.md) | Central command queue, metadata definitions, and validators that decouple input events from pane actions. |
| [`BuildSystem/`](BuildSystem/README.md) | Helpers for triggering project builds, parsing build diagnostics, and running executables. |
| [`Clipboard/`](Clipboard/README.md) | Small wrapper around the OS clipboard with a cached local fallback. |
| [`Diagnostics/`](Diagnostics/README.md) | Persisted diagnostics state used by analysis/build surfaces. |
| [`FileIO/`](FileIO/README.md) | Small filesystem utilities shared by multiple panes and systems. |
| [`InputManager/`](InputManager/README.md) | SDL input routing, per-device handlers, and high-level command generation. |
| [`Ipc/`](Ipc/README.md) | Local Unix socket protocol for editor automation, patch application, and guarded workspace file access. |
| [`LoopJobs/`](LoopJobs/README.md) | Main-thread job queue for work that must re-enter the UI thread safely. |
| [`LoopKernel/`](LoopKernel/README.md) | Frame-loop coordination for queued work, timers, messages, and render requests. |
| [`LoopEvents/`](LoopEvents/README.md) | Main-thread runtime event queue for bounded, deterministic event-driven updates. |
| [`LoopResults/`](LoopResults/README.md) | Per-subsystem completed result queues used by workers to hand results to the deterministic main-thread apply path. |
| [`LoopTime/`](LoopTime/README.md) | Monotonic timing helpers shared by loop subsystems. |
| [`LoopTimer/`](LoopTimer/README.md) | Main-thread timer scheduler used for delayed and repeating callbacks. |
| [`LoopWake/`](LoopWake/README.md) | SDL wake-event bridge so worker threads can nudge the frame loop. |
| [`Terminal/`](Terminal/README.md) | PTY-backed terminal process management used by interactive shells and run/build panes. |
| [`TextSelection/`](TextSelection/README.md) | Shared selectable-text registration and hit-testing across panes. |
| [`Watcher/`](Watcher/README.md) | File-watcher loop that keeps the project tree in sync with disk changes. |

The rule of thumb is simple: if a feature should work without any specific pane
code, it belongs in `core/`.
