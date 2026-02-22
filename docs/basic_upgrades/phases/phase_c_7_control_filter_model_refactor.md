# Phase C.7 Plan: Control Filter Model Refactor

## Goal
Refactor the control-panel filter section into a compact, reliable model that is visually concise and behaviorally explicit for symbol-tree and editor search workflows.

## Product Decisions Locked
- `Target` supports three states via two toggles:
  - `Symbols` on/off
  - `Editor` on/off
  - if both are on, behavior is `Both`
- `Match Kind` stays:
  - `All`, `Methods`, `Types`, `Vars` (and optional `Macros`)
- `Scope` is unified to two choices:
  - `Active File`
  - `Project Files`
- `View Mode` is editor-only:
  - `Projection` (rewrite temp render source)
  - `Markers` (no rewrite, keep real buffer and show hit markers)
- Marker behavior rule:
  - markers are shown only in `Markers` mode
  - markers are not shown in `Projection` mode

## Why this model
- Removes ambiguity between tree filtering and editor rendering modes.
- Avoids `All Open Files` cross-project ambiguity and cache complexity.
- Keeps a single, compact interaction model across all targets.

## UI Layout Plan (compact chips)
1. Row 1: `Target` chips (`Symbols`, `Editor`) + `Scope` chips (`Active File`, `Project Files`)
2. Row 2: `Match Kind` chips
3. Row 3: `Editor View` chips (`Projection`, `Markers`) shown only when `Editor` target is enabled

Layout constraints:
- small text, fixed chip height
- compressed spacing for narrow widths
- centered block at larger widths

## Behavior Matrix
1. `Target=Symbols`, query non-empty:
   - apply query/filter to symbol tree only
   - no editor projection/marker actions
2. `Target=Editor`, `View=Projection`, query non-empty:
   - projection render enabled for scoped files
   - marker rendering disabled
3. `Target=Editor`, `View=Markers`, query non-empty:
   - keep real buffer render
   - marker rendering enabled for scoped files
4. `Target=Both`:
   - apply symbol-tree filtering and editor behavior simultaneously
5. empty query:
   - disable projection and markers
   - restore real file rendering

## Scope Semantics
- `Active File`:
  - symbol tree filtered relative to active file context
  - editor mode applies only to active editor file
- `Project Files`:
  - symbol tree filtered project-wide
  - editor mode applies only to files that belong to current project
  - non-project tabs are ignored for projection/marker operations

## Existing Controls Cleanup
- Current `Fields` row (`name/type/params/kind`) moves behind an `Advanced` gate or is temporarily hidden until behavior parity is validated.
- Current `Parse` toggles remain available but visually separated from search-filter controls (different concern).

## Implementation Steps
1. Add new filter-state model in control panel state (`target`, `scope`, `match kind`, `editor view mode`).
2. Update control panel render/input to new compact row layout and toggle logic.
3. Wire symbol-tree refresh path to `target` + `match kind` + `scope`.
4. Wire editor projection/marker behavior to `target` + `editor view mode` + `scope`.
5. Enforce marker/projection exclusivity in renderer and projection sync logic.
6. Add project-file ownership check for editor scope `Project Files`.
7. Validate empty-query resets and no-regression behavior.

## Acceptance
- Filter section is smaller and supports the full decision model above.
- `Symbols`, `Editor`, and `Both` targets behave correctly.
- `Projection` and `Markers` are exclusive and mode-correct.
- Scope works as `Active File` vs `Project Files` without cross-project leakage.
