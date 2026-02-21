# Phase B Plan: Editor Split Resize + Split Ratio Persistence

## Goal
Make editor split layouts feel fluid and predictable:
- Users can drag split dividers to resize leaf editors.
- Split proportions persist across restart (not reset to 50/50).
- Layout remains stable with minimum-size guards.

## Current System Findings (from code)
- Split layout is hard-coded to equal halves in `performEditorLayout(...)` (`src/ide/Panes/Editor/editor_view.c`).
- Split model stores orientation but no ratio field (`src/ide/Panes/Editor/editor_view.h`).
- Editor session saves split tree/orientation/files/active tab, but no split size ratio (`src/ide/Panes/Editor/editor_session.c`).
- Editor input path handles tabs/text/scrollbar drag, but not split divider drag (`src/ide/Panes/Editor/Input/editor_input_mouse.c`, `src/ide/Panes/Editor/editor_view_state.h`).

## Scope
- In scope:
  - Split divider hit-testing + drag behavior.
  - Ratio-based split layout math with min-size constraints.
  - Persist/restore split ratio in `editor_session.json`.
  - Manual verification for nested splits, restart persistence, and resize edge cases.
- Out of scope:
  - New split commands/hotkeys.
  - Visual redesign of editor tab bar/content.
  - Multi-monitor/high-DPI cursor polish beyond correctness.

## Implementation Plan

### 1) Extend split model with persisted ratio
Files:
- `src/ide/Panes/Editor/editor_view.h`
- `src/ide/Panes/Editor/editor_view.c`

Changes:
- Add `float splitRatio;` to `EditorView` (used only when `type == VIEW_SPLIT`).
- Initialize to `0.5f` in `createSplitView(...)`.
- Add helper clamp utilities (internal) to keep ratio in safe bounds.

Design notes:
- Ratio is interpreted as size of `childA` in the split axis.
- Valid logical range is `(0, 1)`, but clamped tighter at layout time using min pixel sizes.

### 2) Switch layout math from 50/50 to ratio + min-size clamp
Files:
- `src/ide/Panes/Editor/editor_view.c`

Changes:
- Replace equal-halves math in `performEditorLayout(...)` and `layoutSplitChildren(...)`.
- Compute available axis as `total - splitGap`.
- Compute `childA` from ratio.
- Enforce min sizes for both children:
  - Example constants:
    - `EDITOR_SPLIT_MIN_CHILD_W = 220`
    - `EDITOR_SPLIT_MIN_CHILD_H = 140`
- Convert back to an effective ratio when clamped, so behavior is stable as container resizes.

Acceptance for this step:
- Dragged split stays near the user-selected proportion.
- No child collapses to unusable size.

### 3) Add split divider interaction state + hitboxes
Files:
- `src/ide/Panes/Editor/editor_view_state.h`
- `src/ide/Panes/Editor/editor_view_state.c`

Changes:
- Add divider hitbox storage (similar to tab/scroll hitboxes), e.g.:
  - `SplitDividerHitbox { SDL_Rect rect; EditorView* split; }`
- Add drag state:
  - `bool draggingSplitDivider`
  - `EditorView* draggingSplit`
  - `int dragStartMouseX/dragStartMouseY`
  - `float dragStartRatio`
- Add helpers:
  - begin/end split drag
  - hit-test split divider at cursor point

Design notes:
- Divider hitbox should be wider than visual gap for easy targeting.
- Track only split nodes visible in current layout pass.

### 4) Wire split drag into editor mouse input path
Files:
- `src/ide/Panes/Editor/Input/editor_input_mouse.c`
- `src/core/InputManager/input_mouse.c` (only if routing adjustments are needed)

Changes:
- On left mouse down in editor pane:
  - check split divider hitboxes before tab/content click handlers.
  - if matched, start split drag and capture initial mouse + ratio.
- On mouse motion while dragging:
  - compute delta in split axis.
  - update split ratio incrementally with clamping.
  - trigger normal frame layout refresh path.
- On mouse up:
  - stop split drag.

Behavior guarantees:
- Divider drag should not trigger text selection.
- Divider drag should not fight with scrollbar drag.

### 5) Persist split ratios in editor session JSON
Files:
- `src/ide/Panes/Editor/editor_session.c`

Changes:
- Serialization:
  - Add `"ratio": <float>` for `type=split` nodes.
- Deserialization:
  - Parse `"ratio"` when present.
  - Fallback to `0.5f` for older sessions.
  - Clamp on load.
- Keep session schema backward-compatible (existing files load unchanged).

### 6) Validation + hardening pass
Files:
- `src/ide/Panes/Editor/*` (small follow-up fixes)

Manual test matrix:
- Single vertical split: drag near left/right limits.
- Single horizontal split: drag near top/bottom limits.
- Nested split (v inside h): drag parent and child independently.
- Open/close app: ratios restore exactly enough to be visually consistent.
- Window resize after drag: layout remains proportional, min sizes respected.
- Scrollbar drag and text selection still work correctly.

## Risks and Mitigations
- Risk: Divider drag conflicts with existing mouse routing.
  - Mitigation: prioritize divider hit-test before tab/text handling and keep a dedicated drag flag.
- Risk: Ratio jitter at extreme sizes.
  - Mitigation: clamp by pixel min-size and rewrite ratio after clamping.
- Risk: Old session files break.
  - Mitigation: default ratio to `0.5` when key is absent.

## Acceptance Criteria
- User can resize any split by dragging divider.
- Split sizing is stable and constrained (no collapsed children).
- Restart preserves split ratios and split structure.
- No regressions in tab click, text edit, or scrollbar drag behavior.

## Execution Order
1. Model + layout ratio field.
2. Divider hitboxes + drag state.
3. Mouse integration.
4. Session persistence.
5. Validation and fixes.
