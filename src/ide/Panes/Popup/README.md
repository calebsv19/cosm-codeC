# Popup Pane

Handles modal overlays used for confirmations, rename prompts, and future
dialogs.

| File | Responsibility |
| --- | --- |
| `popup_pane.h/c` | Pane construction, shared data (message text, buttons). |
| `popup_system.h/c` | Small manager that tracks whether a popup is active and marshals callbacks. |
| `render_popup.h/c` | Draws the translucent backdrop and the popup content. |
| `input_popup.h/c` | Captures mouse clicks and keyboard shortcuts while the popup is active. |
| `command_popup.h/c` | Executes commands routed to the popup (confirm/cancel actions). |

The popup system cooperates with `core/InputManager/UserInput/rename_flow` to
display rename prompts without blocking the rest of the IDE.
