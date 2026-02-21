# Phase 1 Plan: Search Input + Filtered Symbol Tree Foundation

## Phase Goal
Deliver an end-to-end working control-panel symbol search where typing in the search bar live-filters the visible tree, while preserving existing tree interactions and avoiding pointer-lifetime bugs.

## Scope
In scope:
- Real search text input in control panel.
- Rename-style text-edit semantics for search query updates.
- Base tree + filtered tree separation.
- Case-insensitive substring matching against symbol label/metadata (initial subset).
- Filtered tree rendering and interaction continuity.
- Tree selection safety when tree instances are replaced.

Out of scope:
- Advanced ranking/scoring.
- Token-aware match heuristics and fuzziness tuning.
- Background incremental filtering optimizations.

## Implementation Units

### Unit A: Search State Model (`control_panel.*`)
Files:
- `src/ide/Panes/ControlPanel/control_panel.h`
- `src/ide/Panes/ControlPanel/control_panel.c`

Add state:
- `char searchQuery[...]` (fixed buffer, ASCII-safe).
- `int searchCursor`.
- `bool searchFocused`.
- `SDL_Rect searchBoxRect` (latest layout rect for hit-testing).
- `UITreeNode* baseSymbolTree`.
- `UITreeNode* visibleSymbolTree`.

Add APIs:
- `const char* control_panel_get_search_query(void)`
- `int control_panel_get_search_cursor(void)`
- `bool control_panel_is_search_focused(void)`
- `void control_panel_set_search_focused(bool focused)`
- `void control_panel_set_search_box_rect(SDL_Rect rect)`
- `bool control_panel_point_in_search_box(int x, int y)`
- `bool control_panel_apply_search_insert(const char* utf8_text)`
- `bool control_panel_apply_search_backspace(void)`
- `bool control_panel_clear_search_query(void)`
- `void control_panel_refresh_visible_symbol_tree(void)`

Rules:
- Query edits set a dirty flag and trigger visible-tree rebuild.
- Clearing query uses base tree as visible tree.
- `control_panel_reset_symbol_tree()` frees both trees safely.

### Unit B: Input Wiring with Rename-Style Editing (`input_control_panel.*`)
Files:
- `src/ide/Panes/ControlPanel/input_control_panel.h`
- `src/ide/Panes/ControlPanel/input_control_panel.c`

Add handler:
- `handleControlPanelTextInput(UIPane* pane, SDL_Event* event)`

Behavior mapping (rename-style parity):
- Character insert appends/inserts at cursor.
- Backspace deletes previous character when cursor > 0.
- Left/Right arrows move cursor within bounds.
- Escape clears query and may defocus (final policy in this phase: clear + keep focus).
- Mouse click inside search rect focuses search.
- Mouse click outside search rect defocuses search.
- If search is focused, text editing keys are consumed by control panel path.

Registration:
- Set `controlPanelInputHandler.onTextInput = handleControlPanelTextInput`.

### Unit C: Filter Tree Construction (`symbol_tree_adapter.*`)
Files:
- `src/ide/Panes/ControlPanel/symbol_tree_adapter.h`
- `src/ide/Panes/ControlPanel/symbol_tree_adapter.c`

Add APIs:
- `bool symbol_tree_query_matches_node(const UITreeNode* node, const char* query)`
- `UITreeNode* symbol_tree_clone_filtered(const UITreeNode* root, const char* query)`

Initial match fields:
- Node label.
- If `node->userData` is `FisicsSymbol*`:
- `name`
- `return_type`
- symbol kind label (`fn`, `struct`, `enum`, etc.)
- parameter type/name strings where available.

Filter logic:
- Keep node if self-matches OR any child kept.
- Clone only kept nodes.
- Preserve `fullPath`, `type`, `color`, `userData`.
- For filtered tree, force `isExpanded = true` on retained ancestors.

### Unit D: Render Integration (`render_control_panel.c`)
File:
- `src/ide/Panes/ControlPanel/render_control_panel.c`

Changes:
- Replace placeholder-only rendering with live query text.
- Show placeholder only when query empty and unfocused.
- Draw focus border when search focused.
- Draw caret using `searchCursor` position when focused.
- Persist current search box rect into control-panel state every frame.
- Render `visibleSymbolTree` instead of raw single tree pointer.

### Unit E: Pointer Safety and Selection Reset (`tree_renderer.*` + control panel)
Files:
- `src/ide/UI/Trees/tree_renderer.h`
- `src/ide/UI/Trees/tree_renderer.c`
- `src/ide/Panes/ControlPanel/control_panel.c`
- `src/ide/Panes/ControlPanel/input_control_panel.c`

Add helper:
- `void clearTreeSelectionState(void)` to reset selected/hovered node globals.

Call sites:
- Before freeing/replacing `visibleSymbolTree`.
- Before full reset in `control_panel_reset_symbol_tree()`.

Reason:
- Prevent selected node pointing at freed tree nodes after query changes.

## Data/Control Flow After Phase 1
1. User clicks search box in control panel.
2. `searchFocused=true`.
3. `SDL_TEXTINPUT` and edit keys update query using rename-style semantics.
4. Query change triggers `control_panel_refresh_visible_symbol_tree()`.
5. Visible tree is rebuilt as filtered clone from base tree.
6. Render path draws filtered tree and updated search field every frame.

## Acceptance Criteria
- Typing characters updates query and immediately filters symbol tree.
- Backspace/arrow/escape behavior works consistently.
- Query `Po` can match symbols by return type `Point`.
- Query `mar` can match names like `mark_one`.
- Query with no matches shows empty filtered tree state without crash.
- Double-click jump still works on filtered tree nodes.
- No stale-pointer crashes when editing query repeatedly.

## Verification Plan
Manual checks:
- Click search, type, backspace, arrow navigation, escape clear.
- Toggle between empty and non-empty query rapidly.
- Expand/collapse nodes, then type query; ensure no invalid selection behavior.
- Double-click symbols from filtered tree and confirm file jump.

Build checks:
- Compile touched objects individually.
- Run full `make` build.

Regression checks:
- Confirm existing option toggles still function.
- Confirm active-file/project-file sections still render.

## Risks and Mitigations
- Risk: selection node becomes dangling after tree rebuild.
- Mitigation: explicit selection reset helper before tree replacement.

- Risk: input routing conflict with editor shortcuts.
- Mitigation: consume search-edit keys only when search is focused.

- Risk: query edits trigger heavy rebuilds.
- Mitigation: Phase 1 rebuilds only visible filtered clone; base tree remains cached.

## Exit Gate To Start Phase 2
- All acceptance criteria pass.
- No ASan issues during repeated query edits.
- Known UX edge cases documented in Phase 1 completion notes.
