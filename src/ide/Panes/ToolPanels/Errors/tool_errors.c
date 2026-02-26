#include "tool_errors.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "core/Clipboard/clipboard.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL_ttf.h>

static bool selected[512];
static bool dragging = false;
static int dragAnchor = -1;
static int flatCount = 0;
static PaneScrollState errorScroll;
static SDL_Rect errorScrollTrack = {0};
static SDL_Rect errorScrollThumb = {0};
static bool fileCollapsed[512];
static bool fileCollapseInitialized[512];
static bool g_filterAll = true;
static bool g_filterErrors = true;
static bool g_filterWarnings = true;
static SDL_Rect g_btnAll = {0};
static SDL_Rect g_btnErrors = {0};
static SDL_Rect g_btnWarnings = {0};
static SDL_Rect g_btnOpenAll = {0};
static SDL_Rect g_btnCloseAll = {0};

typedef struct {
    char* path;
    Diagnostic* diags;
    int count;
    bool seen;
} ErrorFileSnapshot;

static ErrorFileSnapshot* g_snapshotFiles = NULL;
static int g_snapshotCount = 0;
static int g_snapshotCap = 0;

static void free_snapshot_file(ErrorFileSnapshot* f) {
    if (!f) return;
    free(f->path);
    f->path = NULL;
    free(f->diags);
    f->diags = NULL;
    f->count = 0;
    f->seen = false;
}

static int snapshot_find_file_by_path(const char* path) {
    if (!path) return -1;
    for (int i = 0; i < g_snapshotCount; ++i) {
        if (g_snapshotFiles[i].path && strcmp(g_snapshotFiles[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

static bool snapshot_reserve(int need) {
    if (need <= g_snapshotCap) return true;
    int newCap = g_snapshotCap > 0 ? g_snapshotCap : 8;
    while (newCap < need) newCap *= 2;
    ErrorFileSnapshot* next = realloc(g_snapshotFiles, (size_t)newCap * sizeof(ErrorFileSnapshot));
    if (!next) return false;
    for (int i = g_snapshotCap; i < newCap; ++i) {
        next[i].path = NULL;
        next[i].diags = NULL;
        next[i].count = 0;
        next[i].seen = false;
    }
    g_snapshotFiles = next;
    g_snapshotCap = newCap;
    return true;
}

static bool snapshot_replace_diags(int idx, const AnalysisFileDiagnostics* src) {
    if (idx < 0 || idx >= g_snapshotCount || !src) return false;
    ErrorFileSnapshot* f = &g_snapshotFiles[idx];
    free(f->diags);
    f->diags = NULL;
    f->count = 0;
    if (src->count <= 0) return true;
    f->diags = calloc((size_t)src->count, sizeof(Diagnostic));
    if (!f->diags) return false;
    f->count = src->count;
    for (int i = 0; i < src->count; ++i) {
        f->diags[i].filePath = f->path ? f->path : src->path;
        f->diags[i].line = src->diags[i].line;
        f->diags[i].column = src->diags[i].column;
        f->diags[i].message = src->diags[i].message;
        f->diags[i].severity = src->diags[i].severity;
    }
    return true;
}

static void snapshot_remove_at(int idx) {
    if (idx < 0 || idx >= g_snapshotCount) return;
    free_snapshot_file(&g_snapshotFiles[idx]);
    for (int i = idx + 1; i < g_snapshotCount; ++i) {
        g_snapshotFiles[i - 1] = g_snapshotFiles[i];
    }
    g_snapshotCount--;
}

static void errors_refresh_snapshot_from_store(void) {
    for (int i = 0; i < g_snapshotCount; ++i) {
        g_snapshotFiles[i].seen = false;
    }

    analysis_store_lock();
    size_t files = analysis_store_file_count();
    for (size_t fi = 0; fi < files; ++fi) {
        const AnalysisFileDiagnostics* src = analysis_store_file_at(fi);
        if (!src || !src->path || src->count <= 0) continue;

        int idx = snapshot_find_file_by_path(src->path);
        if (idx < 0) {
            if (!snapshot_reserve(g_snapshotCount + 1)) continue;
            idx = g_snapshotCount++;
            g_snapshotFiles[idx].path = strdup(src->path);
            g_snapshotFiles[idx].diags = NULL;
            g_snapshotFiles[idx].count = 0;
            g_snapshotFiles[idx].seen = false;
            if (idx < (int)(sizeof(fileCollapseInitialized) / sizeof(fileCollapseInitialized[0])) &&
                !fileCollapseInitialized[idx]) {
                // New entries default collapsed for stable, compact updates.
                fileCollapseInitialized[idx] = true;
                fileCollapsed[idx] = true;
            }
        }

        g_snapshotFiles[idx].seen = true;
        (void)snapshot_replace_diags(idx, src);
    }
    analysis_store_unlock();

    for (int i = g_snapshotCount - 1; i >= 0; --i) {
        if (!g_snapshotFiles[i].seen) {
            snapshot_remove_at(i);
        }
    }
}

void errors_refresh_snapshot(void) {
    errors_refresh_snapshot_from_store();
}
void errors_set_control_button_rects(SDL_Rect allRect,
                                     SDL_Rect errorsRect,
                                     SDL_Rect warningsRect,
                                     SDL_Rect openAllRect,
                                     SDL_Rect closeAllRect) {
    g_btnAll = allRect;
    g_btnErrors = errorsRect;
    g_btnWarnings = warningsRect;
    g_btnOpenAll = openAllRect;
    g_btnCloseAll = closeAllRect;
}
bool errors_filter_all_enabled(void) { return g_filterAll; }
bool errors_filter_errors_enabled(void) { return g_filterErrors; }
bool errors_filter_warnings_enabled(void) { return g_filterWarnings; }
PaneScrollState* errors_get_scroll_state(void) { return &errorScroll; }
SDL_Rect errors_get_scroll_track_rect(void) { return errorScrollTrack; }
SDL_Rect errors_get_scroll_thumb_rect(void) { return errorScrollThumb; }
void errors_set_scroll_rects(SDL_Rect track, SDL_Rect thumb) { errorScrollTrack = track; errorScrollThumb = thumb; }

// Shared font/layout
static TTF_Font* gErrorSmallFont = NULL;
TTF_Font* get_error_font(void) {
    if (gErrorSmallFont) return gErrorSmallFont;
    gErrorSmallFont = TTF_OpenFont("include/fonts/Montserrat/Montserrat-Regular.ttf", 12);
    if (!gErrorSmallFont) {
        fprintf(stderr, "Failed to load error panel font: %s\n", TTF_GetError());
    }
    return gErrorSmallFont;
}

void errors_get_layout_metrics(const UIPane* pane,
                               int* contentTop,
                               int* headerHeight,
                               int* diagHeight,
                               int* lineHeight) {
    TTF_Font* font = get_error_font();
    int lh = font ? TTF_FontHeight(font) : 16;
    if (lineHeight) *lineHeight = lh;
    if (headerHeight) *headerHeight = lh;
    if (diagHeight) *diagHeight = lh * 2;
    if (contentTop && pane) {
        ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
        const int paddingY = d.controls_top + d.button_h + d.row_gap + d.button_h + d.row_gap;
        *contentTop = pane->y + paddingY;
    }
}

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

static bool diag_visible(const Diagnostic* d) {
    if (!d) return false;
    if (g_filterAll) return true;
    if (d->severity == DIAG_SEVERITY_ERROR) return g_filterErrors;
    if (d->severity == DIAG_SEVERITY_WARNING) return g_filterWarnings;
    return false;
}

int flatten_diagnostics(FlatDiagRef* out, int max) {
    int total = 0;
    for (int fi = 0; fi < g_snapshotCount && total < max; ++fi) {
        const ErrorFileSnapshot* f = &g_snapshotFiles[fi];
        if (!f || f->count <= 0) continue;
        int visibleCount = 0;
        for (int di = 0; di < f->count; ++di) {
            if (diag_visible(&f->diags[di])) visibleCount++;
        }
        if (visibleCount <= 0) continue;
        if (total < max) {
            out[total].diag = NULL;
            out[total].path = f->path;
            out[total].fileIndex = fi;
            out[total].isHeader = true;
            total++;
        }
        bool collapsed = (fi < sizeof(fileCollapsed) / sizeof(fileCollapsed[0])) ? fileCollapsed[fi] : false;
        if (collapsed) continue;
        for (int di = 0; di < f->count && total < max; ++di) {
            if (!diag_visible(&f->diags[di])) continue;
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
    if (fileIndex >= g_snapshotCount) return;
    const ErrorFileSnapshot* f = &g_snapshotFiles[fileIndex];
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

bool errors_copy_selection_to_clipboard(void) {
    errors_refresh_snapshot_from_store();
    FlatDiagRef refs[512];
    flatCount = flatten_diagnostics(refs, 512);
    size_t cap = 2048;
    size_t len = 0;
    char* out = malloc(cap);
    if (!out) {
        return false;
    }
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
            if (!tmp) {
                free(out);
                return false;
            }
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
    return any;
}

void errors_select_all_visible(void) {
    errors_refresh_snapshot_from_store();
    FlatDiagRef refs[512];
    flatCount = flatten_diagnostics(refs, 512);
    memset(selected, 0, sizeof(selected));
    int maxSel = (int)(sizeof(selected) / sizeof(selected[0]));
    for (int i = 0; i < flatCount && i < maxSel; ++i) {
        selected[i] = true;
    }
}

void handleErrorsEvent(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    errors_refresh_snapshot_from_store();
    int firstY = 0;
    int headerHeight = 0;
    int diagHeight = 0;
    int lineHeight = 0;
    errors_get_layout_metrics(pane, &firstY, &headerHeight, &diagHeight, &lineHeight);
    firstY += tool_panel_content_inset_default();
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
        const int mx = event->button.x;
        const int my = event->button.y;
        if (mx >= g_btnAll.x && mx < g_btnAll.x + g_btnAll.w &&
            my >= g_btnAll.y && my < g_btnAll.y + g_btnAll.h) {
            g_filterAll = !g_filterAll;
            clear_selected();
            return;
        }
        if (mx >= g_btnErrors.x && mx < g_btnErrors.x + g_btnErrors.w &&
            my >= g_btnErrors.y && my < g_btnErrors.y + g_btnErrors.h) {
            g_filterErrors = !g_filterErrors;
            clear_selected();
            return;
        }
        if (mx >= g_btnWarnings.x && mx < g_btnWarnings.x + g_btnWarnings.w &&
            my >= g_btnWarnings.y && my < g_btnWarnings.y + g_btnWarnings.h) {
            g_filterWarnings = !g_filterWarnings;
            clear_selected();
            return;
        }
        if (mx >= g_btnOpenAll.x && mx < g_btnOpenAll.x + g_btnOpenAll.w &&
            my >= g_btnOpenAll.y && my < g_btnOpenAll.y + g_btnOpenAll.h) {
            for (int i = 0; i < g_snapshotCount && i < (int)(sizeof(fileCollapsed) / sizeof(fileCollapsed[0])); ++i) {
                fileCollapsed[i] = false;
            }
            return;
        }
        if (mx >= g_btnCloseAll.x && mx < g_btnCloseAll.x + g_btnCloseAll.w &&
            my >= g_btnCloseAll.y && my < g_btnCloseAll.y + g_btnCloseAll.h) {
            for (int i = 0; i < g_snapshotCount && i < (int)(sizeof(fileCollapsed) / sizeof(fileCollapsed[0])); ++i) {
                fileCollapsed[i] = true;
                fileCollapseInitialized[i] = true;
            }
            return;
        }

        FlatDiagRef refs[512];
        flatCount = flatten_diagnostics(refs, 512);
        int listMy = event->button.y;
        int y = firstY - (int)scroll_state_get_offset(errors_get_scroll_state());
        int hit = -1;
        for (int i = 0; i < flatCount; ++i) {
            int blockTop = y;
            int h = refs[i].isHeader ? headerHeight : diagHeight;
            int blockBottom = y + h;
            if (listMy >= blockTop && listMy < blockBottom) {
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
        if ((ctrl || gui) && key == SDLK_a) {
            errors_select_all_visible();
            return;
        }
        if ((ctrl || gui) && key == SDLK_c) {
            errors_copy_selection_to_clipboard();
            return;
        }
    }
}
