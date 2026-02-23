# Terminal + Codex Failure Dossier (IDE)

## Purpose

This document captures the current terminal/Codex behavior problems, the implementation state, what has already been changed, and what still appears broken. It is intended as a handoff artifact for deep redesign discussion.

Date context:
- Project: `ide`
- Focus area: terminal PTY + renderer + Codex CLI behavior
- Related plan track: `docs/terminal_upgrade/north_star_reliability_rebuild.md`

---

## 1) Current High-Impact Failures

### A. Codex startup appears to wipe prior shell history
Observed behavior:
- User runs normal shell commands in IDE terminal.
- User starts `codex`.
- Previously visible shell history is no longer shown in expected way, or appears replaced by Codex block output.

Expected behavior:
- History should remain one continuous timeline: shell -> codex -> shell.
- Starting Codex must not destroy or hide prior rows (except normal viewport scrolling behavior).

### B. Codex startup box/rect gets "stuck" near top of pane
Observed behavior:
- Codex startup UI (box glyph section) can appear constrained by viewport behavior.
- As output grows, it does not always continue into stable scrollback progression.

Expected behavior:
- Output should keep flowing with stable row commit semantics.
- Viewport should scroll through full transcript without top-of-pane overwrite artifacts.

### C. Overwrite/repaint traffic becomes line corruption
Observed behavior:
- Spinner/progress/status redraws (CR + cursor controls) can become repeated/garbled rows.
- In bad cases, output looks like repeated overwriting of one line, prompt duplication, or mixed fragments.

Expected behavior:
- Repaint operations should update current visual row state, not append fake history rows.
- Progress/spinner lines should behave like real terminal emulators.

### D. Resize instability
Observed behavior:
- Resizing pane can trigger prompt duplication-like output.
- Prior iterations produced artifacts where resize acted like synthetic terminal activity.

Expected behavior:
- Resize should only change terminal dimensions/viewport mapping.
- Resize should not append content by itself.

### E. Right-edge truncation / missing trailing chars
Observed behavior:
- End-of-line characters (especially UTF-8 Codex box glyphs) truncated.
- Last typed chars may appear delayed or clipped.

Status:
- Improved by increasing UTF-8 render buffer sizing (`cols * 4 + 1`) in terminal render path.
- May still have residual width/metric mismatch edge cases.

---

## 2) Current Architecture (as implemented now)

Main files:
- `src/ide/Panes/Terminal/terminal.c`
- `src/ide/Panes/Terminal/terminal_grid.c`
- `src/ide/Panes/Terminal/render_terminal.c`
- `src/ide/Panes/Terminal/input_terminal.c`
- `src/core/Terminal/terminal_backend.*` (PTY/backend handling)

### Terminal session model
`TerminalSession` currently contains:
- PTY backend handle + consumed offset.
- `TermGrid` emulator state.
- Scroll state.
- Viewport/cache fields.
- `TerminalTranscript` ring-like line store.
- `TerminalVisibleBuffer` and `TerminalScrollbackRing` projection metadata.

### Two parallel content models (core tension)
1. Emulator grid model (`TermGrid`):
- Tracks rows/cols/cells/cursor.
- Parses VT sequences (ESC/CSI/OSC).
- Knows alternate-screen state.

2. Transcript parser model (`TerminalTranscript`):
- Independently parses bytes for line capture.
- Has its own CR/LF behavior and simple escape skipping.
- Stores lines as text strings.

This dual-path design creates risk:
- "Source of truth" conflicts between grid and transcript.
- One path may treat CR/CSI as repaint while other treats as append/reset.
- Render/copy/selection can diverge from what emulator actually represents.

### Rendering path
`render_terminal.c`:
- Uses pane viewport and scroll offsets.
- Alternate mode: renders from grid cells directly.
- Primary mode: now uses `terminal_line_to_string(...)` / grid-backed projection path.
- Scrollbar and cursor drawn from debug stats/projection values.

---

## 3) Specific Technical Changes Already Applied

## A. R3 baseline changes
1. Primary visible row/cursor source moved toward grid authority:
- `terminal_session_content_rows(...)` now uses `grid.used_rows` in primary mode.
- cursor in primary mode now uses `grid.cursor_row/grid.cursor_col`.

2. Selection/copy/render text line access shifted to projected grid rows.

3. Backend resize spam reduced:
- PTY resize call now gated by row/col change (`lastBackendRows/Cols`) instead of every pixel resize.

### B. CR/LF behavior hardening
`terminal_grid.c`:
- Carriage return now always sets `cursor_col = 0` (rewrite semantics).
- Removed transcript-mode special case that converted `\r` into newline append.
- Removed transcript-mode suppression of key CSI cursor/erase commands.

Intent:
- Avoid spinner/progress and shell redraw becoming fake appended lines.

### C. UTF-8 truncation improvement
`render_terminal.c`:
- Primary line buffer capacity changed from cell-count sized to UTF-8 safe capacity:
  - from roughly `lineLen + 1`
  - to `cols * 4 + 1`

---

## 4) Why the System Is Still Fragile

Primary reason:
- We still have two parsers/models consuming the same PTY stream with overlapping responsibilities.

Even after fixes, risks remain:
1. Transcript parser and emulator can disagree on row commit boundaries.
2. CR/LF/CSI semantics may still be interpreted differently between models.
3. Alternate-screen transitions (1047/1049) can produce ambiguous history ownership.
4. Prompt detection/dedup is implicit rather than explicit.
5. Resize behavior can still expose mismatch between viewport projection and underlying row history.

---

## 5) Concrete Repro Scenarios (for investigation)

### Scenario 1: History retention across Codex startup
1. Open IDE terminal.
2. Run: `pwd`, `ls`, `echo hello`.
3. Start `codex`.
4. Observe whether pre-codex rows remain accessible in scrollback.

Failure signals:
- Prior rows disappear or are replaced by Codex startup region.

### Scenario 2: Spinner/progress rewrite correctness
1. In Codex session, trigger long action with spinner/status.
2. Watch whether single status line rewrites cleanly.

Failure signals:
- Many partial fragments appended as separate lines.
- Prompt/status text duplicated strangely.

### Scenario 3: Resize under active output
1. While Codex is streaming output, resize terminal pane repeatedly.
2. Check prompt and output stability.

Failure signals:
- Duplicate prompts.
- Fake empty command-looking lines.
- Output corruption or stuck top region.

### Scenario 4: UTF-8 right-edge handling
1. Produce box-drawing/multibyte output (Codex startup UI).
2. Compare right-edge clipping behavior before/after resize.

Failure signals:
- Missing last glyphs.
- Late appearance of final chars.

---

## 6) Expected Terminal Semantics (Target Spec)

Hard target semantics:
1. Single canonical model for displayed rows/cursor.
2. Clear separation of:
   - row commit (append history)
   - in-place repaint/update
3. Resize does not append content.
4. Alternate screen does not erase primary scrollback.
5. Codex full session remains scrollable after exit.
6. Prompt returns exactly once after Codex exit.

Additional desired parity:
- Behavior close to macOS Terminal for history and redraw stability.

---

## 7) Current Debug Controls / Relevant Settings

Known env toggles in code:
- `IDE_TERMINAL_DEBUG_MODEL`
- `IDE_TERMINAL_DEBUG_OVERLAY`
- `IDE_TERMINAL_ENABLE_ALT_SCREEN`
- `IDE_TERMINAL_SCROLLBACK_ROWS`

Analysis/log toggles (related but separate subsystem):
- `IDE_ANALYSIS_FILE_PROGRESS`
- frontend log controls in analysis status/libraries panel

---

## 8) Suggested Redesign Direction (for external review)

Recommended architecture reset:
1. Make emulator grid the only parser of PTY bytes.
2. Derive scrollback/history from emulator row commit events only.
3. Remove independent transcript byte parser from rendering truth path.
4. Keep transcript text export as derived artifact, not primary source.
5. Introduce explicit "line commit" and "line repaint" events.
6. Add replay fixtures for Codex startup/spinner/menu/exit sequences.

Practical migration strategy:
1. Keep current UI rendering but switch all row reads to canonical projection API.
2. Gate old transcript path behind debug flag.
3. Validate replay fixtures.
4. Remove legacy transcript parser once parity proven.

---

## 9) Related Docs

- `docs/terminal_upgrade/north_star_reliability_rebuild.md`
- `docs/terminal_upgrade/phases/phase_r3_resize_viewport_and_prompt_stability.md`
- `docs/terminal_upgrade/phases/phase_r2_primary_alternate_scrollback_separation.md`

---

## 10) Summary

The terminal has improved (history retention, resize gating, CR rewrite handling, UTF-8 buffer sizing), but core instability likely persists because stream interpretation is still split between emulator and transcript models. The most reliable path forward is a single-source-of-truth terminal model with explicit commit vs repaint semantics and replay-based validation against real Codex transcripts.
