# Pane Visible-State Copy Plan

## Goal
Add a consistent `Cmd+A`/`Cmd+C` flow for non-editor panes so copy reflects the **current visible UI state** (collapsed/expanded, filters, and current list/tree projection), not raw backing data.

## Scope
- Control panel symbol tree
- Errors panel
- Build Output panel
- Assets panel
- Libraries panel
- Git panel tree
- Project panel tree (follow-up pass)

## Architecture
1. Add a shared tree serializer for visible-node snapshots.
2. Add pane-local `select all visible` helpers where rows are flattened.
3. Route `Cmd+A`/`Cmd+C` in each pane input handler using the same visible view model used for rendering.
4. Copy through shared clipboard (`SDL_SetClipboardText` path already centralized).

## Progress
- [x] Plan documented
- [x] Shared tree snapshot serializer
- [x] Control panel visible-tree copy + `Cmd+A`
- [x] Errors panel `Cmd+A` + visible-aware copy
- [x] Build Output panel `Cmd+A` + copy
- [x] Assets panel `Cmd+A` + copy
- [x] Libraries panel `Cmd+A` + copy
- [x] Git panel visible-tree copy + `Cmd+A`
- [x] Project panel visible-tree copy + `Cmd+A`
- [ ] Validate external paste behavior (Notes/Terminal/browser)

## Notes
- “Visible state” means honoring collapse/expand and active filters.
- For trees, snapshot includes only currently visible branches (expanded descendants only).
- For flat panels, `Cmd+A` selects all currently rendered rows and `Cmd+C` copies that set.
