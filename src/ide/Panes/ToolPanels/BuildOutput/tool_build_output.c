#include "tool_build_output.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Clipboard/clipboard.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include <SDL2/SDL.h>
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"
#include "app/GlobalInfo/project.h" // projectPath
#include <stdio.h>
#include <string.h>

static int hit_test_diag(UIPane* pane, int mx, int my, int lineHeight, int firstY) {
    if (!pane) return -1;
    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    int y = firstY;
    for (size_t i = 0; i < count; ++i) {
        int rows = 2;
        if (diags[i].notes[0]) rows += 1;
        int blockTop = y;
        int blockBottom = y + rows * lineHeight;
        if (my >= blockTop && my < blockBottom) {
            return (int)i;
        }
        y = blockBottom;
    }
    return -1;
}

static void copy_diag_to_clipboard(const BuildDiagnostic* d) {
    if (!d) return;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s %s:%d:%d\n    %s%s%s",
             d->isError ? "[E]" : "[W]",
             d->path, d->line, d->col,
             d->message,
             d->notes[0] ? "\n    note: " : "",
             d->notes[0] ? d->notes : "");
    clipboard_copy_text(buf);
}

static void jump_to_diag(const BuildDiagnostic* d) {
    if (!d) return;
    IDECoreState* core = getCoreState();
    if (!core || !core->activeEditorView) return;
    EditorView* view = core->activeEditorView;
    char fullPath[1024];
    if (d->path[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s", d->path);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", projectPath, d->path);
    }
    OpenFile* file = openFileInView(view, fullPath);
    if (!file || !file->buffer) return;
    int targetRow = d->line > 0 ? d->line - 1 : 0;
    if (targetRow >= file->buffer->lineCount) targetRow = file->buffer->lineCount - 1;
    if (targetRow < 0) targetRow = 0;
    int lineLen = file->buffer->lines && file->buffer->lines[targetRow]
                      ? (int)strlen(file->buffer->lines[targetRow])
                      : 0;
    int targetCol = d->col > 0 ? d->col - 1 : 0;
    if (targetCol > lineLen) targetCol = lineLen;
    file->state.cursorRow = targetRow;
    file->state.cursorCol = targetCol;
    file->state.viewTopRow = (targetRow > 2) ? targetRow - 2 : 0;
    file->state.selecting = false;
    file->state.draggingWithMouse = false;
    setActiveEditorView(view);
}


void handleBuildOutputEvent(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    const int lineHeight = 20;
    const int firstY = pane->y + 32;
    static Uint32 lastClickTicks = 0;
    static int lastClickIndex = -1;
    const Uint32 doubleClickMs = 400;

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int hit = hit_test_diag(pane, event->button.x, event->button.y, lineHeight, firstY);
        if (hit >= 0) {
            Uint32 now = SDL_GetTicks();
            bool dbl = (hit == lastClickIndex) && (now - lastClickTicks < doubleClickMs);
            lastClickTicks = now;
            lastClickIndex = hit;
            setSelectedBuildDiag(hit);
            if (dbl) {
                size_t count = 0;
                const BuildDiagnostic* diags = build_diagnostics_get(&count);
                if (hit >= 0 && hit < (int)count) jump_to_diag(&diags[hit]);
            }
        } else {
            setSelectedBuildDiag(-1);
        }
    } else if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        Uint16 mod = event->key.keysym.mod;
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool gui = (mod & KMOD_GUI) != 0;
        if ((ctrl || gui) && key == SDLK_c) {
            int sel = getSelectedBuildDiag();
            size_t count = 0;
            const BuildDiagnostic* diags = build_diagnostics_get(&count);
            if (sel >= 0 && sel < (int)count) {
                copy_diag_to_clipboard(&diags[sel]);
            }
        }
    }
}
#include "app/GlobalInfo/project.h"
