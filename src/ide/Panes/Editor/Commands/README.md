# Editor Commands

This folder bridges the command bus and the editor core. Commands are queued
by input handlers and land here for validation and execution.

| File | Responsibility |
| --- | --- |
| `command_editor.h/c` | Entry point called by the command bus; unpacks `InputCommandMetadata`, routes to text-edit helpers, and frees payloads as needed. |
| `editor_commands.h/c` | Shared helpers for higher-level editor commands (word navigation, clipboard operations, splitting views). |
| `editor_command_payloads.h` | Struct definitions for commands that carry extra data (e.g. key repeat metadata for buffered keydown events). |

The pane keeps rendering/input concerns elsewhere so command execution logic
stays focused and testable.
