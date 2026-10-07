# UI Utilities & Layout

Shared UI infrastructure lives here. These modules keep panes lightweight by
handling layout math, resize hit-testing, and global UI state.

| File | Responsibility |
| --- | --- |
| `layout.h/c` | Calculates pane rectangles based on the current renderer output size and which optional panes are visible (handles HiDPI scaling). |
| `layout_config.h/c` | Stores configuration (default panel sizes, margins) and helper accessors used by `layout.c`. |
| `resize.h/c` | Tracks resize zones and pointer interaction so panes can be resized with the mouse. |
| `ui_state.h/c` | Global UI toggles: panel visibility, cached pointers to shared panes, theme flags, etc. |

## Subdirectories

- [`Trees/`](Trees/README.md) – Minimal tree widget framework used by the
  project tool and other hierarchical viewers.


`window_lifecycle.c` observes logical/drawable size, focus and presentability
through shared KitUI SDL adapters. Hidden/minimized windows suspend rendering;
interruption cancels transient captures and restores unfinished splitter ratios.
`window_lifecycle_proof.c` is an opt-in real-window proof driver, inactive unless
`CODEWORK_WINDOW_LIFECYCLE_PROOF` names a capture directory. It tracks successful
presentations, exercises native transitions and requires clean Vulkan validation.

`button_scope.c` is a bounded app bridge to the shared interaction engine for
icon/project controls. Hosts supply actions, clipped geometry, domain selection
and workspace revision; scope changes disarm old presses. `pane_composition.c`
uses shared kit_pane composition for visible shell/content geometry and hits,
with fixed role IDs and existing app-owned layout/pointer policies.

`panel_button_scope.c` publishes bounded painted header geometry for Control,
Errors and Git. Pane bounds, workspace/tool revisions, modal/focus takeover and
pane retirement invalidate ownership. Disabled controls are excluded from
activation. App handlers retain action meaning and tree/scroll gestures.
`panel_text_edit.h` borrows each controller's buffer/cursor and adopts KitUI's
single-line scalar-safe editing; malformed/oversized edits are rejected intact.
No selection/preedit, editor document or terminal PTY migration is implied.

Header scopes now include Libraries, Tasks and Assets, with borrowed owner/model
revision domains. Terminal headers use registry revisions and pointer semantics;
PTY keyboard ownership remains local. `panel_text_presentation.c` binds bounded
UTF-8 text to pinned KitUI measured presentation. The native Vulkan field painter
clips text/caret to the logical viewport and preserves the parent pane clip.
Selection/preedit and IME candidate policy remain follow-on work.

Editor header ownership uses the same pinned shared interaction engine through
`Editor/editor_header_controls.c`; up to eight leaves retain separate scopes.
The static scope registry accommodates these bounded editor scopes. Global rename
uses `panel_text_edit.h` and the measured field painter; caret blink visibility
is independent of focus/reveal. Retained selection/preedit remains pending.


2026-10-07 retained text and mixed traversal source checkpoint

Control/Errors search, Git message, Project inline rename and global rename now
retain KitUI selection anchors and composition in their owning controllers.
Shift navigation, select all, copy/cut/paste, and scalar edit mechanics use the
pinned shared text API. Clipboard tests use a mock and never overwrite the user
clipboard; failed copy prevents cut. Composition does not mutate committed text,
refresh search results or submit rename/commit. Focus departure cancels preedit;
new drafts reset retained state. Native fields paint selection and preedit through
KitUI text presentation, with SDL candidate positioning only for the active native
window. Project inline rename now uses the measured painter instead of an underscore.

Control/Errors/Git mixed Tab/Shift-Tab traversal uses one shared interaction ring
in visual order; disabled controls are skipped. Text owns editing and Enter;
terminal PTY Tab remains local. Modified Tab does not enter the panel ring. Existing
workspace/tool/geometry/modal/retirement guards remain in force. Mouse click-to-end
is preserved; pointer drag selection is not implemented in this checkpoint.
Git metadata and ordinary tree/section text now use shared palette foregrounds;
explicit tree color overrides and status colors remain supported.

Contained full build and production UI, rename, editor, terminal ownership,
headless/bridge, document/workspace IPC and Vulkan contract pass. Initial missing
text-type include and terminal Tab regression were corrected; attempt logs retained.
Final contrast correction passes rebuilt app, shared theme/font adapter and Vulkan
contract. Dark/light native proof passes 16 stages and 12 captures with zero Vulkan
validation warnings/errors and unchanged fixture C documents. Captures visually
confirm selection, preedit underline and restored light Git caption contrast.
These synthetic preedit events do not prove a real macOS input-method session.

Remaining: glyph fallback for mixed CJK/emoji fonts, real native IME candidate/
commit/cancel acceptance, pointer hit/drag selection, and human/contained-package
comparison. core_font fallback_path selects an alternate file if the primary is
unavailable; it is not a glyph cascade. KitUI measurement/presentation accepts a
host callback, so a shared font-run adapter must keep glyph resolution, advances,
cache/DPI and painting consistent. Do not hand-edit the pinned vendor to simulate
adoption. Colored status rows still need light-theme semantic contrast acceptance.
Editor document and terminal text engines keep their existing owners.

Shared SDK pin and APIs, IDE VERSION 0.4.0, canonical checkout and installed
bundles remain unchanged. No clean, commit, app replacement or publication occurred.
Evidence: artifacts/ide/ui-retained-text-20261007/STATUS.md.
