# Terminal Input Notes

## Paste Modes

- Default: `Ctrl+V` / `Cmd+V`
  - Safe paste mode (enabled by default) normalizes multiline clipboard text to a single editable line.
  - No auto-enter is sent.
- Explicit raw: `Ctrl+Shift+V` / `Cmd+Shift+V`
  - Sends exact clipboard content using bracketed paste, including newlines.
- `Shift+Insert` follows the safe-paste mode flag.

Internal flag for future UI hookup:
- `terminal_safe_paste_enabled()` (default true)
- `terminal_set_safe_paste_enabled(bool)`
- `terminal_toggle_safe_paste_enabled()`

## Shell Editing Shortcuts

- `Ctrl+A` / `Cmd+A`: start of line
- `Ctrl+E` / `Cmd+E`: end of line
- `Ctrl+K` / `Cmd+K`: kill to end of line
- `Ctrl+U` / `Cmd+U`: kill to start of line
- `Ctrl+W` / `Cmd+W`: delete previous word
- `Alt+Left`: word left (`ESC b`)
- `Alt+Right`: word right (`ESC f`)
- `Alt+Backspace`: delete previous word (`ESC DEL`)

## Scope Guards

Terminal keyboard shortcuts run only when:
- terminal pane is focused
- no rename flow is active
- no popup modal is active
