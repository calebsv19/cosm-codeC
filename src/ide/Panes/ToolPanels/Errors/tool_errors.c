#include "tool_errors.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "core/Diagnostics/diagnostic_explanations.h"
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
#include "ide/UI/panel_text_edit.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/ToolPanels/Errors/errors_context_detail.h"
#include "ide/Panes/ToolPanels/Errors/errors_filter.h"
#include "ide/Panes/ToolPanels/Errors/errors_units_detail.h"

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
    UIPanelTaggedRect control_hit_storage[6];
    UIPanelTaggedRectList control_hits;
    UIDoubleClickTracker double_click_tracker;
    UIPanelTextFieldButtonStripLayout search_layout;
    char search_query[128];
    int search_cursor;
    bool search_focused;
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
static bool detail_has_room(const SDL_Rect* clip, int y, int lineHeight);
static void clear_selected(void);

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
#define g_searchLayout (errors_panel_state()->search_layout)
#define g_searchQuery (errors_panel_state()->search_query)
#define g_searchCursor (errors_panel_state()->search_cursor)
#define g_searchFocused (errors_panel_state()->search_focused)
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
        f->diags[i].length = src->diags[i].length;
        f->diags[i].message = src->diags[i].message;
        f->diags[i].hint = src->diags[i].hint;
        f->diags[i].severity = src->diags[i].severity;
        f->diags[i].category = src->diags[i].category;
        f->diags[i].codeId = src->diags[i].codeId;
        f->diags[i].codeName = src->diags[i].codeName;
        f->diags[i].stage = src->diags[i].stage;
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

static void errors_reset_after_filter_change(void) {
    clear_selected();
    errorScroll.offset_px = 0.0f;
    errorScroll.target_offset_px = 0.0f;
}

static UIPanelTextEditBuffer errors_search_buffer(void) {
    return (UIPanelTextEditBuffer){
        .text = g_searchQuery,
        .capacity = (int)sizeof(errors_panel_state()->search_query),
        .cursor = &g_searchCursor
    };
}

const char* errors_get_search_query(void) { return g_searchQuery; }
int errors_get_search_cursor(void) { return g_searchCursor; }
bool errors_is_search_focused(void) { return g_searchFocused; }
bool errors_has_active_search_query(void) { return g_searchQuery[0] != '\0'; }
void errors_set_search_focused(bool focused) { g_searchFocused = focused; }
void errors_search_cursor_end(void) { g_searchCursor = (int)strlen(g_searchQuery); }
void errors_set_search_strip_layout(UIPanelTextFieldButtonStripLayout layout) { g_searchLayout = layout; }
UIPanelTextFieldButtonStripLayout errors_get_search_strip_layout(void) { return g_searchLayout; }

bool errors_clear_search_query(void) {
    UIPanelTextEditBuffer buffer = errors_search_buffer();
    bool changed = ui_panel_text_edit_clear(&buffer);
    if (changed) errors_reset_after_filter_change();
    return changed;
}

bool errors_handle_search_text_input(const SDL_Event* event) {
    UIPanelTextEditBuffer buffer = errors_search_buffer();
    bool changed = ui_panel_text_edit_handle_text_input(&buffer, event);
    if (changed) errors_reset_after_filter_change();
    return changed;
}

bool errors_handle_search_edit_key(SDL_Keycode key) {
    UIPanelTextEditBuffer buffer = errors_search_buffer();
    bool changed = ui_panel_text_edit_handle_keydown(&buffer, key);
    if (changed) errors_reset_after_filter_change();
    return changed;
}

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
    if (diagHeight) *diagHeight = lh * 3;
    if (contentTop && pane) {
        ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
        const int paddingY = d.controls_top +
                             d.button_h + d.row_gap +
                             d.button_h + d.row_gap +
                             d.button_h + d.row_gap;
        *contentTop = pane->y + paddingY;
    }
}

bool errors_get_detail_panel_rect(const UIPane* pane, SDL_Rect* outRect) {
    if (!pane || !outRect || !errors_get_selected_diagnostic_ref()) return false;
    TTF_Font* font = get_error_font();
    int lineHeight = font ? TTF_FontHeight(font) : 14;
    if (lineHeight < 14) lineHeight = 14;
    if (pane->h < lineHeight * 6 || pane->w < 24) return false;
    int extraContextRows = 0;
    ErrorsContextDetail contextDetail;
    if (errors_context_detail_for_diagnostic(errors_get_selected_diagnostic_ref(), &contextDetail) &&
        contextDetail.rowCount > 1) {
        extraContextRows = contextDetail.rowCount - 1;
    }
    int h = lineHeight * (7 + extraContextRows) + 18;
    int maxH = pane->h / 2;
    int minH = lineHeight * 4 + 18;
    if (maxH < minH) maxH = pane->h - 16;
    if (maxH < minH) return false;
    if (h > maxH) h = maxH;
    int w = pane->w - 16;
    if (w < 1) w = 1;
    *outRect = (SDL_Rect){
        pane->x + 8,
        pane->y + pane->h - h - 8,
        w,
        h
    };
    return true;
}

static const char* diagnostic_effective_code_name(const Diagnostic* diag) {
    if (!diag) return NULL;
    return (diag->codeName && diag->codeName[0])
        ? diag->codeName
        : diagnostic_code_name(diag->codeId);
}

static const DiagnosticExplanation* find_diagnostic_explanation(const Diagnostic* diag) {
    if (!diag) return NULL;
    const DiagnosticExplanation* explanation = diagnostic_explanations_find_by_code(diag->codeId);
    const char* codeName = diagnostic_effective_code_name(diag);
    if (!explanation && codeName && codeName[0]) {
        explanation = diagnostic_explanations_find_by_name(codeName);
    }
    return explanation;
}

bool errors_get_context_summary_for_diagnostic(const Diagnostic* diag,
                                               ErrorsDiagnosticContextSummary* outSummary) {
    if (!outSummary) return false;
    memset(outSummary, 0, sizeof(*outSummary));
    ErrorsContextDetail detail;
    if (!errors_context_detail_for_diagnostic(diag, &detail)) return false;
    outSummary->includeCount = detail.includeCount;
    outSummary->macroCount = detail.macroCount;
    outSummary->hasDetails = detail.hasDetails;
    outSummary->hasNavigationTarget = detail.hasNavigationTarget;
    snprintf(outSummary->targetPath, sizeof(outSummary->targetPath), "%s", detail.targetPath);
    snprintf(outSummary->targetKind, sizeof(outSummary->targetKind), "%s", detail.targetKind);
    outSummary->targetLine = detail.targetLine;
    outSummary->targetColumn = detail.targetColumn;
    return true;
}

static bool detail_has_room(const SDL_Rect* clip, int y, int lineHeight) {
    return clip && y + lineHeight <= clip->y + clip->h;
}

bool errors_get_detail_context_line_rect(const UIPane* pane, SDL_Rect* outRect) {
    return errors_get_detail_context_row_rect(pane, 0, outRect);
}

bool errors_get_detail_context_row_rect(const UIPane* pane, int contextRow, SDL_Rect* outRect) {
    if (!pane || !outRect) return false;
    const Diagnostic* diag = errors_get_selected_diagnostic_ref();
    if (!diag) return false;

    ErrorsContextDetail contextDetail;
    if (!errors_context_detail_for_diagnostic(diag, &contextDetail)) return false;
    if (contextRow < 0 || contextRow >= contextDetail.rowCount) return false;

    SDL_Rect detailRect = {0};
    if (!errors_get_detail_panel_rect(pane, &detailRect)) return false;
    TTF_Font* font = get_error_font();
    int lineHeight = font ? TTF_FontHeight(font) : 14;
    if (lineHeight < 14) lineHeight = 14;

    SDL_Rect clip = {
        detailRect.x + 8,
        detailRect.y + 6,
        detailRect.w - 16,
        detailRect.h - 12
    };
    if (clip.w < 1 || clip.h < 1) return false;

    int y = clip.y + lineHeight * 4;
    if (diag->hint && diag->hint[0]) y += lineHeight;
    const DiagnosticExplanation* explanation = find_diagnostic_explanation(diag);
    if (explanation && explanation->description && explanation->description[0] &&
        detail_has_room(&clip, y, lineHeight)) {
        y += lineHeight;
    }
    ErrorsUnitsDiagnosticDetail unitsDetail;
    if (errors_units_detail_for_diagnostic(diag, &unitsDetail) &&
        detail_has_room(&clip, y, lineHeight)) {
        y += lineHeight;
    }
    y += lineHeight * contextRow;
    if (!detail_has_room(&clip, y, lineHeight)) return false;

    *outRect = (SDL_Rect){clip.x, y, clip.w, lineHeight};
    return true;
}

bool errors_get_detail_context_row_at_point(const UIPane* pane,
                                            int x,
                                            int y,
                                            ErrorsContextDetailRow* outRow) {
    const Diagnostic* diag = errors_get_selected_diagnostic_ref();
    if (!pane || !diag) return false;
    ErrorsContextDetail contextDetail;
    if (!errors_context_detail_for_diagnostic(diag, &contextDetail)) return false;
    for (int i = 0; i < contextDetail.rowCount; ++i) {
        SDL_Rect rowRect = {0};
        if (!errors_get_detail_context_row_rect(pane, i, &rowRect)) continue;
        if (x >= rowRect.x && x < rowRect.x + rowRect.w &&
            y >= rowRect.y && y < rowRect.y + rowRect.h) {
            if (outRow) *outRow = contextDetail.rows[i];
            return true;
        }
    }
    return false;
}

bool errors_open_selected_context_target(void) {
    const Diagnostic* diag = errors_get_selected_diagnostic_ref();
    if (!diag) return false;
    ErrorsDiagnosticContextSummary summary;
    if (!errors_get_context_summary_for_diagnostic(diag, &summary)) return false;
    if (!summary.hasNavigationTarget || !summary.targetPath[0]) return false;
    return ui_open_path_at_location_in_active_editor(summary.targetPath,
                                                     summary.targetLine,
                                                     summary.targetColumn);
}

bool errors_open_context_detail_row(const ErrorsContextDetailRow* row) {
    if (!row || !row->hasNavigationTarget || !row->targetPath[0]) return false;
    return ui_open_path_at_location_in_active_editor(row->targetPath,
                                                     row->targetLine,
                                                     row->targetColumn);
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
            errors_reset_after_filter_change();
            return true;
        case ERROR_TOP_CONTROL_FILTER_ERRORS:
            g_filterErrors = !g_filterErrors;
            errors_reset_after_filter_change();
            return true;
        case ERROR_TOP_CONTROL_FILTER_WARNINGS:
            g_filterWarnings = !g_filterWarnings;
            errors_reset_after_filter_change();
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
        case ERROR_TOP_CONTROL_CLEAR_SEARCH:
            errors_set_search_focused(true);
            (void)errors_clear_search_query();
            return true;
        case ERROR_TOP_CONTROL_NONE:
        default:
            return false;
    }
}

bool is_error_selected(int idx) {
    return is_selected(idx);
}

const Diagnostic* errors_get_selected_diagnostic_ref(void) {
    FlatDiagRef refs[512];
    int count = flatten_diagnostics(refs, 512);
    for (int i = 0; i < count; ++i) {
        if (!is_selected(i) || refs[i].isHeader || !refs[i].diag) continue;
        return refs[i].diag;
    }
    return NULL;
}

static bool diag_visible(const Diagnostic* d) {
    if (!d) return false;
    bool severityVisible = false;
    if (g_filterAll) {
        severityVisible = true;
    } else if (d->severity == DIAG_SEVERITY_ERROR) {
        severityVisible = g_filterErrors;
    } else if (d->severity == DIAG_SEVERITY_WARNING) {
        severityVisible = g_filterWarnings;
    }
    return severityVisible && errors_filter_diagnostic_matches_query(d, g_searchQuery);
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
    const char* category = diagnostic_category_name(d->category);
    const char* codeName = (d->codeName && d->codeName[0]) ? d->codeName : diagnostic_code_name(d->codeId);
    const char* stage = (d->stage && d->stage[0]) ? d->stage : diagnostic_stage_name(d->codeId);
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s %s:%d:%d\n    %s\n    %s%s%s%s%s%s%s",
             sev,
             d->filePath ? d->filePath : "(unknown)",
             d->line,
             d->column,
             d->message ? d->message : "(no message)",
             category ? category : "unknown",
             (codeName && codeName[0]) ? " / " : "",
             (codeName && codeName[0]) ? codeName : "",
             (stage && stage[0]) ? " / " : "",
             (stage && stage[0]) ? stage : "",
             (d->hint && d->hint[0]) ? " / hint: " : "",
             (d->hint && d->hint[0]) ? d->hint : "");
    clipboard_copy_text(buf);
}

static bool errors_select_relative_diagnostic(int delta) {
    FlatDiagRef refs[512];
    flatCount = flatten_diagnostics(refs, 512);
    int current = getSelectedErrorDiag();
    int start = current;
    if (start < 0 || start >= flatCount) {
        start = (delta >= 0) ? -1 : flatCount;
    }

    for (int i = start + delta; i >= 0 && i < flatCount; i += delta) {
        if (refs[i].isHeader || !refs[i].diag) continue;
        setSelectedErrorDiag(i);
        return true;
    }

    return false;
}

static bool errors_jump_selected_diagnostic(void) {
    const Diagnostic* diag = errors_get_selected_diagnostic_ref();
    if (!diag) return false;
    jump_to_diag(diag);
    return true;
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
            const char* category = diagnostic_category_name(d->category);
            const char* codeName = (d->codeName && d->codeName[0]) ? d->codeName : diagnostic_code_name(d->codeId);
            const char* stage = (d->stage && d->stage[0]) ? d->stage : diagnostic_stage_name(d->codeId);
            snprintf(line,
                     sizeof(line),
                     "  %s %s:%d:%d\n      %s\n      %s%s%s%s%s%s%s\n",
                     sev,
                     d->filePath ? d->filePath : "(unknown)",
                     d->line,
                     d->column,
                     d->message ? d->message : "(no message)",
                     category ? category : "unknown",
                     (codeName && codeName[0]) ? " / " : "",
                     (codeName && codeName[0]) ? codeName : "",
                     (stage && stage[0]) ? " / " : "",
                     (stage && stage[0]) ? stage : "",
                     (d->hint && d->hint[0]) ? " / hint: " : "",
                     (d->hint && d->hint[0]) ? d->hint : "");
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
    if (event->type == SDL_TEXTINPUT && errors_is_search_focused()) {
        (void)errors_handle_search_text_input(event);
        return;
    }

    int firstY = 0;
    int headerHeight = 0;
    int diagHeight = 0;
    int lineHeight = 0;
    errors_get_layout_metrics(pane, &firstY, &headerHeight, &diagHeight, &lineHeight);
    firstY += tool_panel_content_inset_default();
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        const int mx = event->button.x;
        const int my = event->button.y;
        UIPanelTextFieldButtonStripLayout searchLayout = errors_get_search_strip_layout();
        if (errors_handle_top_control_click(mx, my)) {
            return;
        }
        if (ui_panel_rect_contains(&searchLayout.text_field_rect, mx, my)) {
            errors_set_search_focused(true);
            errors_search_cursor_end();
            return;
        }
        errors_set_search_focused(false);
        SDL_Rect detailRect = {0};
        if (errors_get_detail_panel_rect(pane, &detailRect) &&
            mx >= detailRect.x && mx < detailRect.x + detailRect.w &&
            my >= detailRect.y && my < detailRect.y + detailRect.h) {
            ErrorsContextDetailRow contextRow;
            if (errors_get_detail_context_row_at_point(pane, mx, my, &contextRow) &&
                contextRow.hasNavigationTarget) {
                (void)errors_open_context_detail_row(&contextRow);
            }
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
        bool accel = ui_input_has_primary_accel(mod);
        if (errors_is_search_focused()) {
            bool plainEdit = !(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT));
            if (plainEdit) {
                if (errors_handle_search_edit_key(key)) {
                    return;
                }
                if (key == SDLK_ESCAPE) {
                    errors_set_search_focused(false);
                    return;
                }
            }
        }
        if (accel && key == SDLK_f) {
            errors_set_search_focused(true);
            errors_search_cursor_end();
            return;
        }
        if (accel && key == SDLK_a) {
            errors_select_all_visible();
            return;
        }
        if (accel && key == SDLK_c) {
            errors_copy_selection_to_clipboard();
            return;
        }
        if (key == SDLK_DOWN) {
            if (errors_select_relative_diagnostic(1)) return;
        }
        if (key == SDLK_UP) {
            if (errors_select_relative_diagnostic(-1)) return;
        }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            if (errors_jump_selected_diagnostic()) return;
        }
    }
}
