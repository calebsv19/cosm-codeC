# Custom C IDE

*A minimalist IDE and pane-based code editor built from scratch for C projects and beyond.*

This is an early-stage experimental IDE designed for full control over input handling, editor behavior, and rendering. It's being 
developed as part of a broader ecosystem for simulation, compiler development, and creative coding tools.

---

## Features (current)

- **Split-pane layout + tool sidebars**  
  Editors, terminal, build/errors, git, assets, and tasks all live in panes with shared chrome.

- **PTY-backed terminal**  
  Real shell (forkpty) inside the terminal pane with keyboard mapping, scrollback, resizing, and multiple sessions (interactive/build/run).

- **Build + run terminals**  
  Build output streams into a dedicated build terminal; run uses its own session. Build output is also parsed into a structured build panel with click-to-open errors.

- **Diagnostics panel**  
  Analysis diagnostics per file (grouped, collapsible, multi-select, double-click to jump).

- **Git panel**  
  Shows changes grouped by status and a collapsible git log. Scrollable, selectable entries.

- **Assets panel**  
  Scans project for non-code assets (images/audio/data/other), grouped + collapsible with scrolling and text-like double-click open.

- **Project tree + tasks**  
  Project file browser with drag/open and a task list tool.  
  Input routed through `InputManager → CommandBus → handlers`.

- **SDL-based rendering**  
  SDL2/SDL_ttf renderer with custom clipping, scrollbars, and shared tree renderer.

---

## Screenshot

*(Coming soon – will include layout with multiple editor panes and sidebars)*  
You can still build and run to explore the UI!

---

## Getting Started

### Build Requirements

- SDL2  
- SDL2_ttf  
- A C compiler (e.g., gcc or clang)

### Build & Run

```bash
git clone https://github.com/yourusername/custom-c-ide.git
cd custom-c-ide
make
./ide
```

--- or ---

If you're not using make, here's the manual build line:
gcc -g -Wall -I./src $(find src -type f -name '*.c' ! -path 'src/Libraries/*' ! -path 'src/Project/*') -o ide -lSDL2 -lSDL2_ttf



---

## Usage (high level)

- **Load workspace:** use the menu bar “Load” to pick a project directory. Tool panels refresh (project tree, assets, git, diagnostics).
- **Terminal:** use the terminal tabs; build/run buttons target their own terminals.
- **Build:** click Build to stream into the build terminal; errors also populate the Build Output panel (click entries to open files).
- **Errors panel:** grouped per file, scrollable, multi-select/copy; double-click to jump to file/line.
- **Git:** view changes + log; entries are selectable (actions TBD).
- **Assets:** grouped by type; text-like assets open in the editor; others are stubbed for future preview.

---

## Roadmap / Coming Soon

- Tab closing & better editor UX
- Inline diagnostics/highlighting fed by the analysis bridge
- Rich terminal emulation (colors, cursor modes)
- Git actions (stage/commit), log details
- Asset previews (image/audio), per-bucket scrolling polish
- Settings and keybinding UI

---

## Contributing / Feedback

This is a solo hobby project for now — but feedback, suggestions, or contributions are welcome.

- Open an issue  
- Fork and PR  
- Email: `your.email@example.com`  
- Discord: `@yourhandle` (optional)

---

## License

No license yet – ask before using or modifying. License will be added as the project matures.

---

## FAQ

**Q: Is this ready for daily use?**  
**A:** Not yet — still alpha. But it boots, edits, and saves files with custom pane handling.

**Q: Why make a new IDE?**  
**A:** Total control. This project aims to grow into a powerful editor for simulation and rendering tools, not just a general-purpose 
code editor.
