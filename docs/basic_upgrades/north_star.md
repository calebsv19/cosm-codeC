# Basic IDE Upgrades North Star

## Purpose
Track high-impact core IDE upgrades across performance, editor ergonomics, and panel functionality in one place.

## Outcomes
- Project open and refresh paths are fast (incremental first, full rebuild only as fallback).
- Editor view layout and interactions are fluid (split resizing, cleaner scroll behavior).
- Control panel search/filter evolves into a true semantic navigation tool.
- Tool panels share consistent UX structure and reliable interaction behaviors.

## Phase Roadmap
1. Phase A: Incremental analysis + dirty/dependent rebuild pipeline.
2. Phase B: Editor split resize + split ratio persistence.
3. Phase C: Control panel semantic filter modes (methods/types/tags/scope behavior).
4. Phase D: Build Output panel modernization (scrolling, clipping, layout parity).
5. Phase E: Git panel hardening + interaction polish.

## Tracking Rules
- Active plans go in `docs/basic_upgrades/phases/`.
- Completed phases move to `docs/basic_upgrades/completed/`.
- Each phase doc must define:
  - Goal
  - Scope
  - Acceptance criteria
  - Completion notes

## Current Status (2026-02-20)
- Phase A: Complete (documented in `docs/basic_upgrades/completed/phase_a_incremental_analysis.md`)
- Phase B-E: Pending
