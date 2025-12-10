#include "tool_errors.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "core/Clipboard/clipboard.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static bool selected[512];
static bool dragging = false;
static int dragAnchor = -1;
static int flatCount = 0;
static PaneScrollState errorScroll;
static SDL_Rect errorScrollTrack = {0};
static SDL_Rect errorScrollThumb = {0};
static bool fileCollapsed[512];
PaneScrollState* errors_get_scroll_state(void) { return &errorScroll; }
SDL_Rect errors_get_scroll_track_rect(void) { return errorScrollTrack; }
SDL_Rect errors_get_scroll_thumb_rect(void) { return errorScrollThumb; }
void errors_set_scroll_rects(SDL_Rect track, SDL_Rect thumb) { errorScrollTrack = track; errorScrollThumb = thumb; }

int getSelectedErrorDiag(void) {
    for (int i = 0; i < (int)(sizeof(selected) / sizeof(selected[0])); ++i) {
        if (selected[i]) return i;
    }
    return -1;
}

void setSelectedErrorDiag(int index) {
    memset(selected, 0, sizeof(selected));
    if (index >= 0 && index < (int)(sizeof(selected) / sizeof(selected[0]))) {
        selected[index] = true;
    }
}

static void clear_selected(void) {
    memset(selected, 0, sizeof(selected));
}

static void toggle_selected(int idx, bool additive) {
    if (idx < 0 || idx >= flatCount || idx >= (int)(sizeof(selected) / sizeof(selected[0]))) return;
    if (!additive) {
        clear_selected();
    }
    selected[idx] = !selected[idx];
}

static bool is_selected(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(selected) / sizeof(selected[0]))) return false;
    return selected[idx];
}

static void select_range(int a, int b) {
    if (a < 0 || b < 0) return;
    clear_selected();
    if (a > b) {
        int tmp = a; a = b; b = tmp;
    }
    for (int i = a; i <= b && i < (int)(sizeof(selected) / sizeof(selected[0])); ++i) {
        selected[i] = true;
    }
}

bool is_error_selected(int idx) {
    return is_selected(idx);
}

int flatten_diagnostics(FlatDiagRef* out, int max) {
    int total = 0;
    size_t files = analysis_store_file_count();
    for (size_t fi = 0; fi < files && total < max; ++fi) {
        const AnalysisFileDiagnostics* f = analysis_store_file_at(fi);
        if (!f || f->count <= 0) continue;
        if (total < max) {
            out[total].diag = NULL;
            out[total].path = f->path;
            out[total].fileIndex = (int)fi;
            out[total].isHeader = true;
            total++;
        }
        bool collapsed = (fi < sizeof(fileCollapsed) / sizeof(fileCollapsed[0])) ? fileCollapsed[fi] : false;
        if (collapsed) continue;
        for (int di = 0; di < f->count && total < max; ++di) {
            out[total].diag = &f->diags[di];
            out[total].path = f->path;
            out[total].fileIndex = (int)fi;
            out[total].isHeader = false;
            total++;
        }
    }
    return total;
}
static void jump_to_diag(const Diagnostic* d) {
    if (!d) return;
    IDECoreState* core = getCoreState();
    if (!core || !core->activeEditorView) return;
    EditorView* view = core->activeEditorView;

    char fullPath[1024];
    if (d->filePath && d->filePath[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s", d->filePath);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", projectPath, d->filePath ? d->filePath : "");
    }
    OpenFile* file = openFileInView(view, fullPath);
    if (!file || !file->buffer) return;

    int targetRow = d->line > 0 ? d->line - 1 : 0;
    if (targetRow >= file->buffer->lineCount) targetRow = file->buffer->lineCount - 1;
    if (targetRow < 0) targetRow = 0;
    int lineLen = file->buffer->lines && file->buffer->lines[targetRow]
                      ? (int)strlen(file->buffer->lines[targetRow])
                      : 0;
    int targetCol = d->column > 0 ? d->column - 1 : 0;
    if (targetCol > lineLen) targetCol = lineLen;

    file->state.cursorRow = targetRow;
    file->state.cursorCol = targetCol;
    file->state.viewTopRow = (targetRow > 2) ? targetRow - 2 : 0;
    file->state.selecting = false;
    file->state.draggingWithMouse = false;
    setActiveEditorView(view);
}

static void jump_to_first_diag_for_file(int fileIndex) {
    if (fileIndex < 0) return;
    const AnalysisFileDiagnostics* f = analysis_store_file_at((size_t)fileIndex);
    if (!f || f->count <= 0) return;
    jump_to_diag(&f->diags[0]);
}

static void copy_diag(const Diagnostic* d) {
    if (!d) return;
    const char* sev = (d->severity == DIAG_SEVERITY_ERROR) ? "[E]"
                     : (d->severity == DIAG_SEVERITY_WARNING) ? "[W]"
                     : "[I]";
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s %s:%d:%d\n    %s",
             sev,
             d->filePath ? d->filePath : "(unknown)",
             d->line,
             d->column,
             d->message ? d->message : "(no message)");
    clipboard_copy_text(buf);
}

static void copy_selected_block(void) {
    FlatDiagRef refs[512];
    flatCount = flatten_diagnostics(refs, 512);
    size_t cap = 2048;
    size_t len = 0;
    char* out = malloc(cap);
    if (!out) return;
    out[0] = '\0';

    bool any = false;
    for (int i = 0; i < flatCount; ++i) {
        if (!is_selected(i)) continue;
        any = true;
        char line[1024];
        if (refs[i].isHeader) {
            snprintf(line, sizeof(line), "%s\n", refs[i].path ? refs[i].path : "(unknown file)");
        } else {
            const Diagnostic* d = refs[i].diag;
            const char* sev = (d->severity == DIAG_SEVERITY_ERROR) ? "[E]"
                             : (d->severity == DIAG_SEVERITY_WARNING) ? "[W]"
                             : "[I]";
            snprintf(line, sizeof(line), "  %s %s:%d:%d\n      %s\n",
                     sev,
                     d->filePath ? d->filePath : "(unknown)",
                     d->line,
                     d->column,
                     d->message ? d->message : "(no message)");
        }
        size_t add = strlen(line);
        if (len + add + 1 > cap) {
            cap = (len + add + 1) * 2;
            char* tmp = realloc(out, cap);
            if (!tmp) { free(out); return; }
            out = tmp;
        }
        memcpy(out + len, line, add);
        len += add;
        out[len] = '\0';
    }

    if (any) {
        clipboard_copy_text(out);
    } else {
        int sel = getSelectedErrorDiag();
        if (sel >= 0 && sel < flatCount) copy_diag(refs[sel].diag);
    }
    free(out);
}

void handleErrorsEvent(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    const int lineHeight = 20;
    const int headerHeight = lineHeight;
    const int diagHeight = lineHeight * 2;
    const int firstY = pane->y + 32;
    static Uint32 lastClickTicks = 0;
    static int lastClickIndex = -1;
    const Uint32 doubleClickMs = 400;

    if (event->type == SDL_MOUSEWHEEL) {
        PaneScrollState* scroll = errors_get_scroll_state();
        if (scroll && scroll_state_handle_mouse_wheel(scroll, event)) {
            return;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        FlatDiagRef refs[512];
        flatCount = flatten_diagnostics(refs, 512);
        int my = event->button.y;
        int y = firstY - (int)scroll_state_get_offset(errors_get_scroll_state());
        int hit = -1;
        for (int i = 0; i < flatCount; ++i) {
            int blockTop = y;
            int h = refs[i].isHeader ? headerHeight : diagHeight;
            int blockBottom = y + h;
            if (my >= blockTop && my < blockBottom) {
                hit = i;
                break;
            }
            y = blockBottom;
        }
        if (hit >= 0) {
            Uint32 now = SDL_GetTicks();
            bool dbl = (hit == lastClickIndex) && (now - lastClickTicks < doubleClickMs);
            lastClickTicks = now;
            lastClickIndex = hit;
            Uint16 mod = SDL_GetModState();
            bool additive = (mod & KMOD_CTRL) || (mod & KMOD_GUI) || (mod & KMOD_SHIFT);
            if (refs[hit].isHeader) {
                // shift-click collapses/expands that file
                bool shift = (mod & KMOD_SHIFT) != 0;
                if (shift && refs[hit].fileIndex >= 0 && refs[hit].fileIndex < (int)(sizeof(fileCollapsed)/sizeof(fileCollapsed[0]))) {
                    fileCollapsed[refs[hit].fileIndex] = !fileCollapsed[refs[hit].fileIndex];
                    // refresh selection to just this header
                    clear_selected();
                    toggle_selected(hit, false);
                    return;
                }

                bool add = additive;
                toggle_selected(hit, add);
                add = true;
                for (int i = 0; i < flatCount; ++i) {
                    if (refs[i].fileIndex == refs[hit].fileIndex && !refs[i].isHeader) {
                        toggle_selected(i, add);
                        add = true;
                    }
                }
                if (dbl) {
                    jump_to_first_diag_for_file(refs[hit].fileIndex);
                }
            } else {
                toggle_selected(hit, additive);
            }
            dragging = true;
            dragAnchor = hit;
            if (dbl) {
                jump_to_diag(refs[hit].diag);
            }
        } else {
            clear_selected();
            dragging = false;
            dragAnchor = -1;
        }
    } else if (event->type == SDL_MOUSEMOTION && dragging) {
        FlatDiagRef refs[512];
        flatCount = flatten_diagnostics(refs, 512);
        int my = event->motion.y;
        int y = firstY - (int)scroll_state_get_offset(errors_get_scroll_state());
        int hit = -1;
        for (int i = 0; i < flatCount; ++i) {
            int blockTop = y;
            int h = refs[i].isHeader ? headerHeight : diagHeight;
            int blockBottom = y + h;
            if (my >= blockTop && my < blockBottom) {
                hit = i;
                break;
            }
            y = blockBottom;
        }
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
