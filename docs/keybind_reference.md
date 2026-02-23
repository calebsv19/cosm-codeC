# IDE Keybind Reference

This file is the current keybind map based on active input handlers.

## Input Routing Rules

1. Global key handling runs first in `src/core/InputManager/input_keyboard.c`.
2. If no global shortcut consumes the key, input is forwarded to the focused pane handler.
3. Text-entry modes (rename/search/git message/editor/terminal text input) can intentionally override shortcuts.

## Global (Always Checked First)

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+1` | Switch editor tab (`COMMAND_SWITCH_TAB`) | `src/core/InputManager/input_keyboard.c` |
| `Ctrl+R` | Toggle control panel visibility (`COMMAND_TOGGLE_CONTROL_PANEL`) | `src/core/InputManager/input_keyboard.c` |
| `Ctrl+T` | Toggle tool panel visibility (`COMMAND_TOGGLE_TOOL_PANEL`) | `src/core/InputManager/input_keyboard.c` |
| `Ctrl+Shift+C` or `Cmd+Shift+C` | Clear analysis cache (`COMMAND_CLEAR_ANALYSIS_CACHE`) | `src/core/InputManager/input_keyboard.c` |
| `P` (plain, no modifiers) | Toggle control-panel search pause/resume | `src/core/InputManager/input_keyboard.c` |

## Rename Flow (Global Capture While Active)

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Enter` | Submit rename | `src/core/InputManager/input_keyboard.c` |
| `Escape` | Cancel rename | `src/core/InputManager/input_keyboard.c` |
| `Backspace` | Delete char | `src/core/InputManager/input_keyboard.c` |
| `Left` / `Right` | Move cursor | `src/core/InputManager/input_keyboard.c` |

## Menu Bar (When Focused)

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+B` | Build project | `src/ide/Panes/MenuBar/input_menu_bar.c` |
| `Ctrl+R` | Run executable | `src/ide/Panes/MenuBar/input_menu_bar.c` |
| `Ctrl+D` | Debug executable | `src/ide/Panes/MenuBar/input_menu_bar.c` |
| `Ctrl+S` | Save file | `src/ide/Panes/MenuBar/input_menu_bar.c` |
| `Ctrl+L` | Choose workspace | `src/ide/Panes/MenuBar/input_menu_bar.c` |

Note: global `Ctrl+R` is handled first, so menu-bar `Ctrl+R` can be shadowed by global routing.

## Editor Pane (When Focused)

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+S` | Save file | `src/ide/Panes/Editor/Input/input_editor.c` |
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copy / Cut / Paste | `src/ide/Panes/Editor/Input/input_editor.c` |
| `Ctrl+A` | Select all | `src/ide/Panes/Editor/Input/input_editor.c` |
| `Alt+-` / `Alt+=` | Undo / Redo | `src/ide/Panes/Editor/Input/input_editor.c` |
| `Enter` / `Backspace` / `Delete` / `Tab` | Insert newline / delete / forward delete / tab | `src/ide/Panes/Editor/Input/input_editor.c` |
| `Cmd+E` | Split active editor view | `src/ide/Panes/Editor/Input/editor_input_keyboard.c`, `src/ide/Panes/Editor/Commands/editor_commands.c` |
| `Cmd+Shift+E` | Split with opposite orientation | `src/ide/Panes/Editor/Commands/editor_commands.c` |
| `Alt+C` | Collapse selected editor leaf with its partner | `src/ide/Panes/Editor/Input/editor_input_keyboard.c` |
| `Cmd+1` | Switch tab forward | `src/ide/Panes/Editor/Input/editor_input_keyboard.c` |
| `Cmd+Tab` | Tab switch (`Shift` reverses direction) | `src/ide/Panes/Editor/Commands/editor_commands.c` |
| `Cmd+Arrow` / `Cmd+[ ]` / `Cmd+Backspace` | Word/paragraph/top-bottom/trim-line-start navigation actions | `src/ide/Panes/Editor/Commands/editor_commands.c` |

## Control Panel (When Focused)

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+F` | Focus search bar | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Escape` (search focused) | Unfocus search bar | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Backspace/Delete/Left/Right/Home/End` | Edit search query | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Ctrl+P` | Toggle parse mode | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Ctrl+E` | Toggle errors visibility | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Ctrl+M` | Toggle macros visibility | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Right-click` match button + `Left/Right` | Reorder selected match button | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Shift+Left/Right` (while selected) | Jump selected match button to edge | `src/ide/Panes/ControlPanel/input_control_panel.c` |
| `Enter` | Clear selected match-button highlight | `src/ide/Panes/ControlPanel/input_control_panel.c` |

## Terminal Pane (When Focused)

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+V` or `Cmd+V` | Safe paste (default): multiline normalized to single-line draft text | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+Shift+V` or `Cmd+Shift+V` | Raw multiline bracketed paste (exact clipboard) | `src/ide/Panes/Terminal/input_terminal.c` |
| `Shift+Insert` | Safe paste (follows terminal safe-paste flag) | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+L` / `Cmd+L` | Clear terminal panel output | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+R` / `Cmd+R` | Run executable | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+C` / `Cmd+C` | Copy selection, else send SIGINT | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+D` / `Cmd+D` | Send EOF | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+A` / `Cmd+A` | Move cursor to start of line | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+E` / `Cmd+E` | Move cursor to end of line | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+K` / `Cmd+K` | Kill from cursor to end of line | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+U` / `Cmd+U` | Kill from cursor to start of line | `src/ide/Panes/Terminal/input_terminal.c` |
| `Ctrl+W` / `Cmd+W` | Delete previous word | `src/ide/Panes/Terminal/input_terminal.c` |
| `Alt+Left` / `Alt+Right` | Word-left / word-right | `src/ide/Panes/Terminal/input_terminal.c` |
| `Alt+Backspace` | Delete previous word (meta-backspace) | `src/ide/Panes/Terminal/input_terminal.c` |
| `PageUp/PageDown/Home/End` | Scroll terminal history | `src/ide/Panes/Terminal/input_terminal.c` |
| `Enter/Backspace/Tab/Arrows` | Send terminal control input | `src/ide/Panes/Terminal/input_terminal.c` |

## Tool Panels

### Icon Bar

| Keybind | Behavior | Source |
| --- | --- | --- |
| `1..7` | Select active tool panel icon | `src/ide/Panes/IconBar/input_icon_bar.c` |

### Project Panel

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+N` | New file | `src/ide/Panes/ToolPanels/Project/input_tool_project.c` |
| `Ctrl+R` | Rename selected entry | `src/ide/Panes/ToolPanels/Project/input_tool_project.c` |
| `Ctrl+O` | Open selected file | `src/ide/Panes/ToolPanels/Project/input_tool_project.c` |
| `Ctrl+D` | Delete selected file/folder | `src/ide/Panes/ToolPanels/Project/input_tool_project.c` |
| `Enter` | Open selected file | `src/ide/Panes/ToolPanels/Project/input_tool_project.c` |
| `Delete` | Delete selected file/folder | `src/ide/Panes/ToolPanels/Project/input_tool_project.c` |

### Tasks Panel

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+N` | Add task | `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c` |
| `Ctrl+R` | Rename task | `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c` |
| `Ctrl+K` / `Ctrl+J` | Move task up/down | `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c` |
| `Ctrl+D` | Delete task | `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c` |
| `Delete` | Delete task | `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c` |
| `Enter` | Rename task | `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c` |

### Git Panel

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Escape` | Unfocus commit message field | `src/ide/Panes/ToolPanels/Git/input_git.c` |
| `Enter` | Commit with current message (when message focused) | `src/ide/Panes/ToolPanels/Git/input_git.c` |
| `Backspace/Delete/Left/Right/Home/End` | Edit commit message (when focused) | `src/ide/Panes/ToolPanels/Git/input_git.c` |

### Libraries / Assets / Errors / Build Output

| Keybind | Behavior | Source |
| --- | --- | --- |
| `Ctrl+C` or `Cmd+C` (Libraries) | Copy selected rows | `src/ide/Panes/ToolPanels/Libraries/input_tool_libraries.c` |
| `Ctrl+C` or `Cmd+C` (Assets) | Copy selected asset rows | `src/ide/Panes/ToolPanels/Assets/input_tool_assets.c` |
| Errors / Build Output | Delegated to panel-specific event handlers | `src/ide/Panes/ToolPanels/Errors/input_tool_errors.c`, `src/ide/Panes/ToolPanels/BuildOutput/input_tool_build_output.c` |

## Maintenance Notes

When changing bindings:
1. Update this file.
2. Update the owning pane/input handler.
3. Check for global-precedence conflicts in `src/core/InputManager/input_keyboard.c`.
