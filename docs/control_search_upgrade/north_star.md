# Control Search Upgrade North Star

## Purpose
Make the Control Panel symbol tree search fully interactive, fast, and reliable so users can type and immediately see a reduced symbol tree that reflects only relevant symbols and metadata.

## Product Outcome
- The search bar is a real text input control, not a placeholder.
- Search filtering updates symbol-tree visualization on each input edit.
- Matching works across symbol identifiers and metadata (name, kind, return type, parameter types/names).
- Filtered view keeps tree context (ancestors shown when descendants match).
- Existing tree actions still work (expand/collapse, selection, double-click jump).
- UX for text editing matches existing rename-flow behavior expectations.

## Engineering Constraints
- Preserve current symbol source of truth (`analysis_symbols_store`) and current tree rendering pipeline.
- Avoid unnecessary full tree rebuilds while typing.
- Keep memory ownership explicit for base tree vs filtered tree.
- Keep behavior deterministic and debuggable (no hidden async mutation in Phase 1).

## Current-State Snapshot (as of 2026-02-20)
- Search box in `src/ide/Panes/ControlPanel/render_control_panel.c` is visual only.
- No `onTextInput` handler is registered for control panel input.
- Symbol tree is produced in `control_panel_refresh_symbol_tree(...)` and built by `symbol_tree_adapter.c`.
- Tree selection state is global in `tree_renderer.c`, so tree swaps must avoid stale pointers.
- Rename flow already defines expected text-edit semantics in `core/InputManager/UserInput/rename_flow.*`.

## North Star Architecture
1. Input Layer
- Control panel owns search focus state.
- Text editing semantics mirror rename flow patterns (insert chars, backspace, caret progression, escape clear/cancel behavior policy).

2. Data Layer
- Maintain two tree models:
- Base symbol tree (full snapshot).
- Filtered symbol tree (ephemeral, query-dependent).

3. Match Layer
- Case-insensitive char/substring matching across symbol label + metadata fields.
- Parent retention rule: node is kept if self matches OR any descendant matches.

4. Render/Interaction Layer
- Render current query and focus feedback in search control.
- Display filtered tree without breaking selection, expand/collapse, or jump-to-definition interactions.

5. Refresh/Invalidation Layer
- Rebuild base tree only on symbol/project invalidation events.
- Rebuild filtered tree on query changes.
- Clear stale selection references when view tree is replaced.

## Delivery Phases

### Phase 1: Search Input Foundation + Safe Filtered Tree Pipeline
- Add control panel search state and event handling.
- Implement rename-style editing semantics for the search query.
- Add base-tree vs filtered-tree model separation.
- Implement first-pass match/filter clone path.
- Wire render path to visible tree.
- Add pointer-safety resets when swapping trees.

### Phase 2: Match Quality and Symbol Semantics
- Expand matching against complete symbol metadata fields.
- Normalize token boundaries (`snake_case`, punctuation, type qualifiers).
- Add lightweight relevance scoring and stable ordering rules.

### Phase 3: UX + Discoverability
- Better search field visuals (caret, selection/focus border, placeholder behavior).
- Empty-state messaging (`No matches for ...`).
- Optional query chips/toggles (names-only vs include-types).

### Phase 4: Performance + Incremental Updates
- Cache lowercase/match keys for symbols.
- Incremental filtered-tree rebuild for small query edits.
- Throttle expensive rebuild paths as tree size grows.

### Phase 5: Reliability + Regression Hardening
- Add integration tests for typing, clearing, selection, and jump behavior.
- Add stress tests for large workspaces.
- Final polish and documentation updates.

## Completion Criteria
- Typing in search produces immediate, correct, and stable symbol tree filtering.
- Clearing search returns full tree with expected interaction behavior.
- No crash or stale-pointer regressions during rebuild/filter cycles.
- Match behavior supports your examples:
- `Po` can surface function returning `Point`.
- `mar` can surface `mark_one`.
- unrelated query (e.g. `try`) hides non-matching symbols.

## Workflow Rules For This Upgrade
- Each phase gets its own plan document under `docs/control_search_upgrade/phases/`.
- When a phase is completed, move its document to `docs/control_search_upgrade/completed/` with final implementation notes.
- Do not start next phase until current phase acceptance criteria are met.
