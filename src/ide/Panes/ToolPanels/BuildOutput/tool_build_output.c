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
#include <stdbool.h>

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

static bool selected[1024];
static bool dragging = false;
static int dragAnchor = -1;

static void clear_selection(void) {
    memset(selected, 0, sizeof(selected));
    setSelectedBuildDiag(-1);
}

static void toggle_selection(int idx, bool additive) {
    if (idx < 0 || idx >= (int)(sizeof(selected) / sizeof(selected[0]))) return;
    if (!additive) {
        clear_selection();
    }
    selected[idx] = !selected[idx];
    setSelectedBuildDiag(idx);
}

static bool is_selected(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(selected) / sizeof(selected[0]))) return false;
    return selected[idx];
}

static void select_range(int a, int b) {
    if (a < 0 || b < 0) return;
    clear_selection();
    if (a > b) {
        int tmp = a; a = b; b = tmp;
    }
    for (int i = a; i <= b && i < (int)(sizeof(selected) / sizeof(selected[0])); ++i) {
        selected[i] = true;
    }
    setSelectedBuildDiag(b);
}

bool build_output_is_selected(int idx) {
    return is_selected(idx);
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

static void copy_selected_block(void) {
    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    size_t cap = 4096;
    size_t len = 0;
    char* out = malloc(cap);
    if (!out) return;
    out[0] = '\0';

    bool any = false;
    for (size_t i = 0; i < count; ++i) {
        if (!is_selected((int)i)) continue;
        const BuildDiagnostic* d = &diags[i];
        any = true;
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s %s:%d:%d\n    %s",
                 d->isError ? "[E]" : "[W]",
                 d->path, d->line, d->col,
                 d->message);
        size_t add = strlen(buf);
        if (d->notes[0]) {
            snprintf(buf + add, sizeof(buf) - add, "\n    note: %s", d->notes);
            add = strlen(buf);
        }
        buf[add++] = '\n';
        buf[add] = '\0';
        if (len + add + 1 > cap) {
            cap = (len + add + 1) * 2;
            char* tmp = realloc(out, cap);
            if (!tmp) { free(out); return; }
            out = tmp;
        }
        memcpy(out + len, buf, add);
        len += add;
        out[len] = '\0';
    }

    if (any) {
        clipboard_copy_text(out);
    } else {
        int sel = getSelectedBuildDiag();
        if (sel >= 0 && sel < (int)count) {
            copy_diag_to_clipboard(&diags[sel]);
        }
    }
    free(out);
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
            Uint16 mod = SDL_GetModState();
            bool additive = (mod & KMOD_CTRL) || (mod & KMOD_GUI) || (mod & KMOD_SHIFT);
            toggle_selection(hit, additive);
            dragging = true;
            dragAnchor = hit;
            if (dbl) {
                size_t count = 0;
                const BuildDiagnostic* diags = build_diagnostics_get(&count);
                if (hit >= 0 && hit < (int)count) jump_to_diag(&diags[hit]);
            }
        } else {
            clear_selection();
            dragging = false;
            dragAnchor = -1;
        }
    } else if (event->type == SDL_MOUSEMOTION && dragging) {
        int hit = hit_test_diag(pane, event->motion.x, event->motion.y, lineHeight, firstY);
        if (hit >= 0 && dragAnchor >= 0) {
            select_range(dragAnchor, hit);
        }
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        dragging = false;
        dragAnchor = -1;
    } else if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        Uint16 mod = event->key.keysym.mod;
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool gui = (mod & KMOD_GUI) != 0;
        if ((ctrl || gui) && key == SDLK_c) {
            copy_selected_block();
        }
    }
}
#include "app/GlobalInfo/project.h"
