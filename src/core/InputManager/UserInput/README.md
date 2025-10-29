# Stateful User Input Flows

Temporary interaction flows (currently just rename) live here so they can
maintain state across multiple raw SDL events without bloating pane code.

| File | Responsibility |
| --- | --- |
| `rename_flow.h/c` | Owns the rename state machine: buffers text input, exposes `beginRename/submitRename/cancelRename`, and renders minimal diagnostics when validation fails. |
| `rename_access.h` | Header that panes include to query the active rename state (`RENAME` macro). |

Add new multi-event flows here (e.g. multi-step dialogs, modal prompts) and
have panes call into them instead of rolling bespoke state.
