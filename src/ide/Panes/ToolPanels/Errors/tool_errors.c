#include "tool_errors.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "core/Clipboard/clipboard.h"
#include "ide/UI/editor_navigation.h"
#include "ide/UI/flat_list_hit_test.h"
#include "ide/UI/flat_list_interaction.h"
#include "engine/Render/render_font.h"
#include "ide/UI/flat_list_selection.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
    char* path;
    Diagnostic* diags;
    int count;
    bool seen;
} ErrorFileSnapshot;

typedef struct {
    bool selected[512];
    UIFlatListDragState drag_state;
    int flat_count;
    PaneScrollState scroll;
    SDL_Rect scroll_track;
    SDL_Rect scroll_thumb;
    bool file_collapsed[512];
    bool file_collapse_initialized[512];
    bool filter_all;
    bool filter_errors;
    bool filter_warnings;
    UIPanelTaggedRect control_hit_storage[5];
    UIPanelTaggedRectList control_hits;
    UIDoubleClickTracker double_click_tracker;
    ErrorFileSnapshot* snapshot_files;
    int snapshot_count;
    int snapshot_cap;
    uint64_t snapshot_store_stamp;
    bool snapshot_store_stamp_valid;
} ErrorPanelState;

static ErrorPanelState g_errorPanelBootstrapState = {0};
static bool g_errorPanelBootstrapInitialized = false;

static void errors_panel_init_state(void* ptr) {
    ErrorPanelState* state = (ErrorPanelState*)ptr;
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->drag_state.active = false;
    state->drag_state.anchor = -1;
    state->filter_all = true;
    state->filter_errors = true;
    state->filter_warnings = true;
    state->control_hits.items = state->control_hit_storage;
    state->control_hits.capacity =
        (int)(sizeof(state->control_hit_storage) / sizeof(state->control_hit_storage[0]));
}

static void free_snapshot_file(ErrorFileSnapshot* f);
static void jump_to_diag(const Diagnostic* d);
static void jump_to_first_diag_for_file(int fileIndex);

static void errors_panel_destroy_state(void* ptr) {
    ErrorPanelState* state = (ErrorPanelState*)ptr;
    if (!state) return;
    for (int i = 0; i < state->snapshot_count; ++i) {
        free_snapshot_file(&state->snapshot_files[i]);
    }
    free(state->snapshot_files);
    free(state);
}

static ErrorPanelState* errors_panel_state(void) {
    return (ErrorPanelState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_ERRORS,
        sizeof(ErrorPanelState),
        errors_panel_init_state,
        errors_panel_destroy_state,
        &g_errorPanelBootstrapState,
        &g_errorPanelBootstrapInitialized
    );
}

#define selected (errors_panel_state()->selected)
#define g_errorDragState (errors_panel_state()->drag_state)
#define flatCount (errors_panel_state()->flat_count)
#define errorScroll (errors_panel_state()->scroll)
#define errorScrollTrack (errors_panel_state()->scroll_track)
#define errorScrollThumb (errors_panel_state()->scroll_thumb)
#define fileCollapsed (errors_panel_state()->file_collapsed)
#define fileCollapseInitialized (errors_panel_state()->file_collapse_initialized)
#define g_filterAll (errors_panel_state()->filter_all)
#define g_filterErrors (errors_panel_state()->filter_errors)
#define g_filterWarnings (errors_panel_state()->filter_warnings)
#define g_errorDoubleClickTracker (errors_panel_state()->double_click_tracker)
#define g_snapshotFiles (errors_panel_state()->snapshot_files)
#define g_snapshotCount (errors_panel_state()->snapshot_count)
#define g_snapshotCap (errors_panel_state()->snapshot_cap)

typedef struct {
    const FlatDiagRef* refs;
    int headerHeight;
    int diagHeight;
} ErrorHitTestContext;

typedef struct {
    const FlatDiagRef* refs;
    int hit_index;
    bool collapse_only;
} ErrorRowActivationState;

static int errors_row_height_for_index(int rowIndex, void* context) {
    ErrorHitTestContext* ctx = (ErrorHitTestContext*)context;
    if (!ctx || !ctx->refs || rowIndex < 0) return 0;
    return ctx->refs[rowIndex].isHeader ? ctx->headerHeight : ctx->diagHeight;
}

static int errors_hit_test_flat_index(const FlatDiagRef* refs,
                                      int count,
                                      int mouseY,
                                      int firstY,
                                      float offset,
                                      int headerHeight,
                                      int diagHeight) {
    ErrorHitTestContext ctx = {
        .refs = refs,
        .headerHeight = headerHeight,
        .diagHeight = diagHeight
    };
    return ui_flat_list_hit_test_variable(mouseY,
                                          firstY,
                                          offset,
                                          count,
                                          errors_row_height_for_index,
                                          &ctx);
}

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
    // Refresh only from the diagnostics stamp that has been published by the
    // main-thread event dispatch path (DiagnosticsUpdated), so UI does not
    // chase worker-side intermediate mutations.
    uint64_t store_stamp = analysis_store_published_stamp();
    if (errors_panel_state()->snapshot_store_stamp_valid &&
        errors_panel_state()->snapshot_store_stamp == store_stamp) {
        return;
    }

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
    errors_panel_state()->snapshot_store_stamp = store_stamp;
    errors_panel_state()->snapshot_store_stamp_valid = true;
}

void errors_refresh_snapshot(void) {
    errors_refresh_snapshot_from_store();
}
UIPanelTaggedRectList* errors_get_control_hits(void) { return &errors_panel_state()->control_hits; }
bool errors_filter_all_enabled(void) { return g_filterAll; }
bool errors_filter_errors_enabled(void) { return g_filterErrors; }
bool errors_filter_warnings_enabled(void) { return g_filterWarnings; }
PaneScrollState* errors_get_scroll_state(void) { return &errorScroll; }
SDL_Rect errors_get_scroll_track_rect(void) { return errorScrollTrack; }
SDL_Rect errors_get_scroll_thumb_rect(void) { return errorScrollThumb; }
void errors_set_scroll_rects(SDL_Rect track, SDL_Rect thumb) { errorScrollTrack = track; errorScrollThumb = thumb; }

TTF_Font* get_error_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (font) return font;
    return getActiveFont();
}

void errors_get_layout_metrics(const UIPane* pane,
                               int* contentTop,
                               int* headerHeight,
                               int* diagHeight,
                               int* lineHeight) {
    TTF_Font* font = get_error_font();
    int lh = font ? TTF_FontHeight(font) : 14;
    if (lh < 14) lh = 14;
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
    ui_flat_list_selection_clear(selected, (int)(sizeof(selected) / sizeof(selected[0])));
    (void)ui_flat_list_selection_select_single(selected,
                                               (int)(sizeof(selected) / sizeof(selected[0])),
                                               0,
                                               index);
}

static void clear_selected(void) {
    ui_flat_list_selection_clear(selected, (int)(sizeof(selected) / sizeof(selected[0])));
}

static void errors_clear_selected_cb(void* context) {
    (void)context;
    clear_selected();
}

static void toggle_selected(int idx, bool additive) {
    (void)ui_flat_list_selection_toggle(selected,
                                        (int)(sizeof(selected) / sizeof(selected[0])),
                                        flatCount,
                                        idx,
                                        additive);
}

static bool is_selected(int idx) {
    return ui_flat_list_selection_contains(selected,
                                           (int)(sizeof(selected) / sizeof(selected[0])),
                                           idx);
}

static void select_range(int a, int b) {
    (void)ui_flat_list_selection_select_range(selected,
                                              (int)(sizeof(selected) / sizeof(selected[0])),
                                              flatCount,
                                              a,
                                              b);
}

static void errors_select_range_cb(int anchorIndex, int hitIndex, void* context) {
    (void)context;
    select_range(anchorIndex, hitIndex);
}

static void mark_selected(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(selected) / sizeof(selected[0]))) return;
    selected[idx] = true;
}

static void errors_select_file_group_rows(const ErrorRowActivationState* state, bool additive) {
    if (!state || !state->refs || state->hit_index < 0) return;
    const FlatDiagRef* hitRef = &state->refs[state->hit_index];
    if (!additive) {
        clear_selected();
    }

    mark_selected(state->hit_index);
    if (!hitRef->isHeader) {
        return;
    }

    for (int i = 0; i < flatCount; ++i) {
        if (i == state->hit_index) continue;
        if (state->refs[i].fileIndex == hitRef->fileIndex && !state->refs[i].isHeader) {
            mark_selected(i);
        }
    }
}

static void errors_row_select_single(void* context) {
    ErrorRowActivationState* state = (ErrorRowActivationState*)context;
    if (!state || !state->refs || state->hit_index < 0) return;
    if (state->collapse_only) {
        clear_selected();
        mark_selected(state->hit_index);
        return;
    }
    if (state->refs[state->hit_index].isHeader) {
        errors_select_file_group_rows(state, false);
        return;
    }
    toggle_selected(state->hit_index, false);
}

static void errors_row_select_additive(void* context) {
    ErrorRowActivationState* state = (ErrorRowActivationState*)context;
    if (!state || !state->refs || state->hit_index < 0) return;
    if (state->collapse_only) {
        clear_selected();
        mark_selected(state->hit_index);
        return;
    }
    if (state->refs[state->hit_index].isHeader) {
        errors_select_file_group_rows(state, true);
        return;
    }
    toggle_selected(state->hit_index, true);
}

static void errors_row_prefix_action(void* context) {
    ErrorRowActivationState* state = (ErrorRowActivationState*)context;
    if (!state || !state->refs || state->hit_index < 0) return;
    const FlatDiagRef* ref = &state->refs[state->hit_index];
    if (!ref->isHeader) return;
    if (ref->fileIndex < 0 ||
        ref->fileIndex >= (int)(sizeof(fileCollapsed) / sizeof(fileCollapsed[0]))) {
        return;
    }
    fileCollapsed[ref->fileIndex] = !fileCollapsed[ref->fileIndex];
    fileCollapseInitialized[ref->fileIndex] = true;
}

static void errors_row_activate(void* context) {
    ErrorRowActivationState* state = (ErrorRowActivationState*)context;
    if (!state || !state->refs || state->hit_index < 0) return;
    const FlatDiagRef* ref = &state->refs[state->hit_index];
    if (ref->isHeader) {
        jump_to_first_diag_for_file(ref->fileIndex);
    } else {
        jump_to_diag(ref->diag);
    }
}

static void errors_row_drag_start(void* context) {
    ErrorRowActivationState* state = (ErrorRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    ui_flat_list_drag_state_begin(&g_errorDragState, state->hit_index);
}

static bool errors_handle_top_control_click(int mx, int my) {
    switch ((ErrorTopControlId)ui_panel_tagged_rect_list_hit_test(errors_get_control_hits(), mx, my)) {
        case ERROR_TOP_CONTROL_FILTER_ALL:
            g_filterAll = !g_filterAll;
            clear_selected();
            return true;
        case ERROR_TOP_CONTROL_FILTER_ERRORS:
            g_filterErrors = !g_filterErrors;
            clear_selected();
            return true;
        case ERROR_TOP_CONTROL_FILTER_WARNINGS:
            g_filterWarnings = !g_filterWarnings;
            clear_selected();
            return true;
        case ERROR_TOP_CONTROL_OPEN_ALL:
            for (int i = 0; i < g_snapshotCount && i < (int)(sizeof(fileCollapsed) / sizeof(fileCollapsed[0])); ++i) {
                fileCollapsed[i] = false;
            }
            return true;
        case ERROR_TOP_CONTROL_CLOSE_ALL:
            for (int i = 0; i < g_snapshotCount && i < (int)(sizeof(fileCollapsed) / sizeof(fileCollapsed[0])); ++i) {
                fileCollapsed[i] = true;
                fileCollapseInitialized[i] = true;
            }
            return true;
        case ERROR_TOP_CONTROL_NONE:
        default:
            return false;
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
    (void)ui_open_path_at_location_in_active_editor(d->filePath, d->line, d->column);
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
    (void)ui_flat_list_selection_select_all(selected,
                                            (int)(sizeof(selected) / sizeof(selected[0])),
                                            flatCount);
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
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        const int mx = event->button.x;
        const int my = event->button.y;
        if (errors_handle_top_control_click(mx, my)) {
            return;
        }

        FlatDiagRef refs[512];
        flatCount = flatten_diagnostics(refs, 512);
        int hit = errors_hit_test_flat_index(refs,
                                             flatCount,
                                             event->button.y,
                                             firstY,
                                             scroll_state_get_offset(errors_get_scroll_state()),
                                             headerHeight,
                                             diagHeight);
        if (!ui_flat_list_drag_state_prepare_hit(&g_errorDragState,
                                                 hit,
                                                 errors_clear_selected_cb,
                                                 NULL)) {
            return;
        }

        Uint16 mod = SDL_GetModState();
        bool collapseOnly = refs[hit].isHeader && ui_input_has_shift(mod);
        bool additive = !collapseOnly && ui_input_is_additive_selection(mod);
        ErrorRowActivationState activation = {
            .refs = refs,
            .hit_index = hit,
            .collapse_only = collapseOnly
        };
        (void)ui_row_activation_handle_primary(
            &(UIRowActivationContext){
                .double_click_tracker = &g_errorDoubleClickTracker,
                .row_identity = (uintptr_t)(uint32_t)(hit + 1),
                .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
                .clicked_prefix = collapseOnly,
                .additive_modifier = additive,
                .range_modifier = false,
                .wants_drag_start = true,
                .on_select_single = errors_row_select_single,
                .on_select_additive = errors_row_select_additive,
                .on_prefix = errors_row_prefix_action,
                .on_activate = errors_row_activate,
                .on_drag_start = errors_row_drag_start,
                .user_data = &activation
            });
    } else if (event->type == SDL_MOUSEMOTION && g_errorDragState.active) {
        FlatDiagRef refs[512];
        flatCount = flatten_diagnostics(refs, 512);
        int hit = errors_hit_test_flat_index(refs,
                                             flatCount,
                                             event->motion.y,
                                             firstY,
                                             scroll_state_get_offset(errors_get_scroll_state()),
                                             headerHeight,
                                             diagHeight);
        (void)ui_flat_list_drag_state_apply_range(&g_errorDragState,
                                                  hit,
                                                  errors_select_range_cb,
                                                  NULL);
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        ui_flat_list_drag_state_reset(&g_errorDragState);
    } else if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        Uint16 mod = event->key.keysym.mod;
        if (ui_input_has_primary_accel(mod) && key == SDLK_a) {
            errors_select_all_visible();
            return;
        }
        if (ui_input_has_primary_accel(mod) && key == SDLK_c) {
            errors_copy_selection_to_clipboard();
            return;
        }
    }
}
