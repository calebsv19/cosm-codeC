# Custom C IDE

*A minimalist IDE and pane-based code editor built from scratch for C projects and beyond.*

This is an early-stage experimental IDE designed for full control over input handling, editor behavior, and rendering. It's being 
developed as part of a broader ecosystem for simulation, compiler development, and creative coding tools.

---

## Features

- **Split-pane layout**  
  Arrange panes flexibly to display multiple editors and tool views at once.

- **Basic text editor**  
  View and edit multiple files with syntax-aware buffers.

- **Simple file explorer** *(project-local only)*  
  Browse and open files from the current working directory. (Full file system access not yet implemented.)

- **Custom command routing system**  
  Input events pass through a structured pipeline:  
  `InputManager → CommandBus → Action Handlers`  
  Enables deeply customizable keyboard/mouse interaction.

- **Tag-aware editor architecture** *(WIP)*  
  Foundation laid for future syntax and metadata-aware highlighting, filtering, and semantic overlays.

- **Fully SDL-based rendering system**  
  Lightweight, portable graphics stack using SDL2 and SDL_ttf for UI and font rendering.

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

## Usage

- **Navigation:**  
  Use mouse clicks or keybinds to activate and switch between panes.

- **Editing:**  
  Basic text entry and saving is supported.  
  (Tab switching and closing are in development.)

- **File Management:**  
  File tabs can be opened from the current project directory, but can't yet be closed or navigated with shortcuts.

- **Known Issues:**  
  - Tab closing not supported yet  
  - No terminal integration  
  - No build/run or compiler integration  
  - Errors are not yet shown in a dedicated panel  
  - No project selector or settings UI

---

## Roadmap / Coming Soon

- Tab switching and closing  
- Integrated terminal pane  
- Build + Run system for C files  
- IDE-to-compiler bridge (for custom language integration)  
- Project loader with directory selection  
- Enhanced error tracking and inline diagnostics  
- Tool panels for:  
  - Errors  
  - Build output  
  - Git log  
  - Parser view  
  - Task system

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

