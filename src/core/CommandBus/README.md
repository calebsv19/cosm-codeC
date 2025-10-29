# Command Bus

The command bus decouples input capture from execution. Input handlers enqueue
`InputCommandMetadata` instances, the bus validates them against the current
focused pane, and dispatches one command per tick. This keeps pane logic small
and lets us add instrumentation around command flow.

| File | Responsibility |
| --- | --- |
| `command_bus.h/c` | Queue management, lifecycle hooks (`initCommandBus`, `tickCommandBus`), and dispatch logic that invokes pane handlers. |
| `command_metadata.h` | Definition of `InputCommandMetadata`, including the payload pointer used by panes such as the editor. |
| `command_registry.h/c` | Declarative list of commands and the pane roles allowed to execute them, plus helper to stringify commands for logging. |
| `save_queue.h/c` | Background queue for save operations. Shares the same ticking model so the bus can report overall “busy” status. |

## Implementation Notes

This pass routes the bulk of editor keystrokes through the `CommandBus`, but a few paths still bypass the queue:

- Mouse-driven editor gestures (drag-select, scroll-wheel, resize zones) remain immediate so cursor feedback stays responsive. If we decide to queue them later we'll need richer payloads to describe pointer deltas per frame.
- The rename overlay still captures SDL text input directly inside `handleInput` because that flow predates the bus and rewires UI widgets synchronously. Once the rename controller is moved into its own pane we can convert those events to queued commands as well.
- `COMMAND_SWITCH_TAB` currently derives its direction from `meta.keyMod` (Shift reverses direction). If we add other tab navigation keys we can switch to an explicit payload instead of probing modifiers at dispatch time.
- Editor command execution still asks `getCoreState()->activeEditorView` at dispatch; that keeps us from holding stale pointers across frames, but it assumes the active view is updated before `tickCommandBus()` runs each frame.

Everything else that used to call editor manipulation helpers directly now gathers the context at dispatch time and feeds it through the queue.
