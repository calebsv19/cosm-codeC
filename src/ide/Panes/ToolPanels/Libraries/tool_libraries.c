#include "tool_libraries.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "engine/Render/render_text_helpers.h"
#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/core_state.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/analysis_status.h"
#include "ide/UI/editor_navigation.h"
#include "ide/UI/flat_list_selection.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/Clipboard/clipboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static LibraryPanelState g_libraryPanelBootstrapState = {0};
static bool g_libraryPanelBootstrapInitialized = false;

static void libraries_panel_init_state(void* ptr);
static void libraries_panel_destroy_state(void* ptr);
static void toggle_bucket(int bucketIndex);
static void toggle_header(int bucketIndex, int headerIndex);

LibraryPanelState* libraries_panel_state(void) {
    return (LibraryPanelState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_LIBRARIES,
        sizeof(LibraryPanelState),
        libraries_panel_init_state,
        libraries_panel_destroy_state,
        &g_libraryPanelBootstrapState,
        &g_libraryPanelBootstrapInitialized
    );
}

static void clear_flat_rows(LibraryPanelState* st) {
    if (!st || !st->flatRows) return;
    for (int i = 0; i < st->flatCount; ++i) {
        free(st->flatRows[i].labelPrimary);
        free(st->flatRows[i].labelSecondary);
        st->flatRows[i].labelPrimary = NULL;
        st->flatRows[i].labelSecondary = NULL;
    }
}

static char* dup_label(const char* text) {
    if (!text || !*text) return NULL;
    return strdup(text);
}

static int ensure_header_expand_capacity(int bucketIndex, size_t count) {
    LibraryPanelState* st = libraries_panel_state();
    if (bucketIndex < 0 || bucketIndex >= LIB_BUCKET_COUNT) return 0;
    size_t cur = st->headerExpandedCount[bucketIndex];
    if (cur >= count) return 1;
    size_t newCount = count;
    bool* tmp = realloc(st->headerExpanded[bucketIndex], newCount * sizeof(bool));
    if (!tmp) return 0;
    // Initialize new slots to false
    for (size_t i = cur; i < newCount; ++i) tmp[i] = false;
    st->headerExpanded[bucketIndex] = tmp;
    st->headerExpandedCount[bucketIndex] = newCount;
    return 1;
}

static const char* bucket_label_local(LibraryBucketKind kind) {
    switch (kind) {
        case LIB_BUCKET_PROJECT:    return "Project headers";
        case LIB_BUCKET_SYSTEM:     return "System headers";
        case LIB_BUCKET_EXTERNAL:   return "External headers";
        case LIB_BUCKET_UNRESOLVED: return "Unresolved headers";
        default:                    return "Headers";
    }
}

static void select_range(int a, int b) {
    LibraryPanelState* st = libraries_panel_state();
    (void)ui_flat_list_selection_select_range(st->selected,
                                              st->selectedCapacity,
                                              st->flatCount,
                                              a,
                                              b);
}

bool library_row_is_selected(int idx) {
    LibraryPanelState* st = libraries_panel_state();
    if (!st->selected || idx < 0 || idx >= st->selectedCapacity) return false;
    return st->selected[idx];
}

void select_all_library_rows(void) {
    LibraryPanelState* st = libraries_panel_state();
    st->selectedRow = ui_flat_list_selection_select_all(st->selected,
                                                        st->selectedCapacity,
                                                        st->flatCount);
}

void rebuildLibraryFlatRows(void) {
    LibraryPanelState* st = libraries_panel_state();
    clear_flat_rows(st);
    st->flatCount = 0;

    // Reserve space (rough heuristic)
    int estimated = 0;
    library_index_lock();
    for (size_t b = 0; b < library_index_bucket_count(); ++b) {
        if (!st->includeSystemHeaders &&
            (int)b == LIB_BUCKET_SYSTEM) {
            continue;
        }
        const LibraryBucket* bucket = library_index_get_bucket(b);
        if (!bucket || bucket->header_count == 0) continue;
        estimated += 1 + (int)bucket->header_count;
        for (size_t h = 0; h < bucket->header_count; ++h) {
            const LibraryHeader* header = library_index_get_header(bucket, h);
            if (header) estimated += (int)header->usage_count;
        }
    }
    if (estimated > st->flatCapacity) {
        int newCap = (st->flatCapacity == 0) ? (estimated + 16) : (st->flatCapacity * 2);
        if (newCap < estimated) newCap = estimated + 16;
        LibraryFlatRow* tmp = realloc(st->flatRows, newCap * sizeof(LibraryFlatRow));
        if (!tmp) {
            library_index_unlock();
            return;
        }
        st->flatRows = tmp;
        st->flatCapacity = newCap;
        bool* selTmp = realloc(st->selected, newCap * sizeof(bool));
        if (selTmp) {
            st->selected = selTmp;
            st->selectedCapacity = newCap;
        }
    }
    if (st->selected && st->selectedCapacity > 0) {
        memset(st->selected, 0, (size_t)st->selectedCapacity * sizeof(bool));
    }

    // Populate rows
    for (size_t b = 0; b < library_index_bucket_count(); ++b) {
        if (!st->includeSystemHeaders &&
            (int)b == LIB_BUCKET_SYSTEM) {
            continue;
        }
        const LibraryBucket* bucket = library_index_get_bucket(b);
    if (!bucket || bucket->header_count == 0) continue;

    LibraryFlatRow* row = &st->flatRows[st->flatCount++];
    memset(row, 0, sizeof(*row));
    row->type = LIB_NODE_BUCKET;
        row->bucketIndex = (int)b;
        row->headerIndex = -1;
        row->usageIndex = -1;
        row->depth = 0;
        row->bucketHeaderCount = (int)bucket->header_count;

        // Bucket labels will be rendered based on bucketIndex; no primary string needed here.
        bool bucketOpen = st->bucketExpanded[b];
        if (!bucketOpen) continue;

        // Ensure header expansion buffer
        ensure_header_expand_capacity((int)b, bucket->header_count);

        for (size_t h = 0; h < bucket->header_count; ++h) {
            const LibraryHeader* header = library_index_get_header(bucket, h);
            if (!header) continue;
            LibraryFlatRow* hrow = &st->flatRows[st->flatCount++];
            memset(hrow, 0, sizeof(*hrow));
            hrow->type = LIB_NODE_HEADER;
            hrow->bucketIndex = (int)b;
            hrow->headerIndex = (int)h;
            hrow->usageIndex = -1;
            hrow->depth = 1;
            hrow->labelPrimary = dup_label(header->name);
            hrow->labelSecondary = dup_label(header->resolved_path);
            hrow->includeKind = header->kind;

            bool headerOpen = (h < st->headerExpandedCount[b]) ? st->headerExpanded[b][h] : false;
            if (!headerOpen) continue;

            for (size_t u = 0; u < header->usage_count; ++u) {
                const LibraryUsage* usage = library_index_get_usage(header, u);
                if (!usage) continue;
                LibraryFlatRow* urow = &st->flatRows[st->flatCount++];
                memset(urow, 0, sizeof(*urow));
                urow->type = LIB_NODE_USAGE;
                urow->bucketIndex = (int)b;
                urow->headerIndex = (int)h;
                urow->usageIndex = (int)u;
                urow->depth = 2;
                urow->labelPrimary = dup_label(usage->source_path);
                urow->usageLine = usage->line;
                urow->usageColumn = usage->column;
            }
        }
    }
    library_index_unlock();

    if (st->selectedRow >= st->flatCount) st->selectedRow = -1;
    if (st->hoveredRow >= st->flatCount) st->hoveredRow = -1;
    if (st->selected && st->selectedCapacity > 0 && st->flatCount > 0) {
        // Clamp selection to valid range; if empty set, keep cleared.
        for (int i = 0; i < st->flatCount && i < st->selectedCapacity; ++i) {
            if (st->selected[i]) { st->selectedRow = i; break; }
        }
    }
    scroll_state_clamp(&st->scroll);
}

static void libraries_panel_init_state(void* ptr) {
    LibraryPanelState* st = (LibraryPanelState*)ptr;
    if (!st) return;

    memset(st, 0, sizeof(*st));
    st->selectedRow = -1;
    st->hoveredRow = -1;
    st->dragAnchorRow = -1;
    st->selecting = false;
    scroll_state_init(&st->scroll, NULL);
    for (int i = 0; i < LIB_BUCKET_COUNT; ++i) {
        st->bucketExpanded[i] = true;
        st->headerExpanded[i] = NULL;
        st->headerExpandedCount[i] = 0;
    }
    st->includeSystemHeaders = true;
    st->control_hits.items = st->control_hit_storage;
    st->control_hits.capacity =
        (int)(sizeof(st->control_hit_storage) / sizeof(st->control_hit_storage[0]));
}

static void libraries_panel_release_dynamic_state(LibraryPanelState* st) {
    if (!st) return;
    clear_flat_rows(st);
    free(st->flatRows);
    st->flatRows = NULL;
    st->flatCount = 0;
    st->flatCapacity = 0;
    free(st->selected);
    st->selected = NULL;
    st->selectedCapacity = 0;
    for (int i = 0; i < LIB_BUCKET_COUNT; ++i) {
        free(st->headerExpanded[i]);
        st->headerExpanded[i] = NULL;
        st->headerExpandedCount[i] = 0;
    }
}

static void libraries_panel_destroy_state(void* ptr) {
    LibraryPanelState* st = (LibraryPanelState*)ptr;
    if (!st) return;
    libraries_panel_release_dynamic_state(st);
    free(st);
}

void initLibrariesPanel() {
    LibraryPanelState* st = libraries_panel_state();
    libraries_panel_release_dynamic_state(st);
    libraries_panel_init_state(st);
    rebuildLibraryFlatRows();
}

UIPanelTaggedRectList* libraries_control_hits(void) {
    return &libraries_panel_state()->control_hits;
}

void updateHoveredLibraryMousePosition(int x, int y) {
    (void)x;
    (void)y;
    // Hover handled in render; kept for interface symmetry.
}

static bool row_is_prefix_hit(const LibraryFlatRow* row, UIPane* pane, int clickX) {
    if (!row) return false;
    int indent = row->depth * 20;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    int drawX = pane->x + (d.pad_left - 1) + indent;
    int prefixWidth = getTextWidth("[-] ");
    return (clickX >= drawX && clickX <= drawX + prefixWidth);
}

static void toggle_bucket(int bucketIndex) {
    LibraryPanelState* st = libraries_panel_state();
    if (bucketIndex < 0 || bucketIndex >= LIB_BUCKET_COUNT) return;
    st->bucketExpanded[bucketIndex] = !st->bucketExpanded[bucketIndex];
    rebuildLibraryFlatRows();
}

static void toggle_header(int bucketIndex, int headerIndex) {
    LibraryPanelState* st = libraries_panel_state();
    if (bucketIndex < 0 || bucketIndex >= LIB_BUCKET_COUNT) return;
    ensure_header_expand_capacity(bucketIndex, (size_t)headerIndex + 1);
    st->headerExpanded[bucketIndex][headerIndex] =
        !st->headerExpanded[bucketIndex][headerIndex];
    rebuildLibraryFlatRows();
}

static void open_usage(const LibraryFlatRow* row) {
    if (!row || row->type != LIB_NODE_USAGE) return;
    if (!row->labelPrimary) return;
    (void)ui_open_path_at_location_in_active_editor(row->labelPrimary,
                                                    row->usageLine,
                                                    row->usageColumn);
}

typedef struct LibraryRowActivationState {
    LibraryPanelState* panel_state;
    LibraryFlatRow* row;
    int row_index;
} LibraryRowActivationState;

static void library_row_select_single(void* user_data) {
    LibraryRowActivationState* state = (LibraryRowActivationState*)user_data;
    if (!state || !state->panel_state) return;

    state->panel_state->selectedRow = state->row_index;
    state->panel_state->dragAnchorRow = state->row_index;
    state->panel_state->selecting = true;
    (void)ui_flat_list_selection_select_single(state->panel_state->selected,
                                               state->panel_state->selectedCapacity,
                                               state->panel_state->flatCount,
                                               state->row_index);
}

static void library_row_select_additive(void* user_data) {
    LibraryRowActivationState* state = (LibraryRowActivationState*)user_data;
    if (!state || !state->panel_state) return;

    state->panel_state->selectedRow = state->row_index;
    state->panel_state->dragAnchorRow = state->row_index;
    state->panel_state->selecting = true;
    (void)ui_flat_list_selection_toggle(state->panel_state->selected,
                                        state->panel_state->selectedCapacity,
                                        state->panel_state->flatCount,
                                        state->row_index,
                                        true);
}

static void library_row_select_range_action(void* user_data) {
    LibraryRowActivationState* state = (LibraryRowActivationState*)user_data;
    if (!state || !state->panel_state) return;

    if (state->panel_state->dragAnchorRow < 0) {
        state->panel_state->dragAnchorRow =
            state->panel_state->selectedRow >= 0 ? state->panel_state->selectedRow : state->row_index;
    }
    state->panel_state->selectedRow = state->row_index;
    state->panel_state->selecting = true;
    select_range(state->panel_state->dragAnchorRow, state->row_index);
}

static void library_row_prefix_action(void* user_data) {
    LibraryRowActivationState* state = (LibraryRowActivationState*)user_data;
    if (!state || !state->row) return;

    if (state->row->type == LIB_NODE_BUCKET) {
        toggle_bucket(state->row->bucketIndex);
    } else if (state->row->type == LIB_NODE_HEADER) {
        toggle_header(state->row->bucketIndex, state->row->headerIndex);
    }
}

static void library_row_activate(void* user_data) {
    LibraryRowActivationState* state = (LibraryRowActivationState*)user_data;
    if (!state || !state->row) return;
    if (state->row->type == LIB_NODE_USAGE) {
        open_usage(state->row);
    }
}

void copy_selected_rows(void) {
    LibraryPanelState* st = libraries_panel_state();
    if (!st->selected || !st->flatRows || st->flatCount <= 0) return;

    size_t cap = 2048;
    size_t len = 0;
    char* out = malloc(cap);
    if (!out) return;
    out[0] = '\0';

    for (int i = 0; i < st->flatCount; ++i) {
        if (!library_row_is_selected(i)) continue;
        LibraryFlatRow* row = &st->flatRows[i];
        char line[1024];
        line[0] = '\0';
        if (row->type == LIB_NODE_BUCKET) {
            snprintf(line, sizeof(line), "[Bucket] %s", bucket_label_local((LibraryBucketKind)row->bucketIndex));
        } else if (row->type == LIB_NODE_HEADER) {
            const char* kindGlyph = (row->includeKind == LIB_INCLUDE_KIND_SYSTEM) ? "<>" : "\"\"";
            snprintf(line, sizeof(line), "[Header] %s %s", kindGlyph, row->labelPrimary ? row->labelPrimary : "(header)");
        } else if (row->type == LIB_NODE_USAGE) {
            snprintf(line, sizeof(line), "[Use] %s:%d:%d",
                     row->labelPrimary ? row->labelPrimary : "(usage)",
                     row->usageLine,
                     row->usageColumn);
        }

        size_t add = strlen(line);
        if (len + add + 2 > cap) {
            cap = (len + add + 2) * 2;
            char* tmp = realloc(out, cap);
            if (!tmp) { free(out); return; }
            out = tmp;
        }
        memcpy(out + len, line, add);
        len += add;
        out[len++] = '\n';
        out[len] = '\0';
    }

    if (len > 0) {
        clipboard_copy_text(out);
    }
    free(out);
}

static int row_index_from_position(UIPane* pane, int mouseY) {
    LibraryPanelState* st = libraries_panel_state();
    int contentY = tool_panel_single_row_content_top(pane);
    float offset = scroll_state_get_offset(&st->scroll);
    int localY = mouseY - contentY + (int)offset;
    if (localY < 0) return -1;
    int idx = localY / LIBRARY_ROW_HEIGHT;
    return (idx >= 0 && idx < st->flatCount) ? idx : -1;
}

bool handleLibraryHeaderClick(UIPane* pane, int clickX, int clickY) {
    (void)pane;
    LibraryPanelState* st = libraries_panel_state();
    switch ((LibraryTopControlId)ui_panel_tagged_rect_list_hit_test(&st->control_hits, clickX, clickY)) {
        case LIB_TOP_CONTROL_SYSTEM_TOGGLE:
            st->includeSystemHeaders = !st->includeSystemHeaders;
            rebuildLibraryFlatRows();
            return true;
        case LIB_TOP_CONTROL_LOGS_TOGGLE:
            analysis_toggle_frontend_logs_enabled();
            printf("[Analysis] Frontend logs: %s\n",
                   analysis_frontend_logs_enabled() ? "ON" : "OFF");
            return true;
        case LIB_TOP_CONTROL_NONE:
        default:
            break;
    }

    return false;
}

void handleLibraryEntryClick(UIPane* pane, int clickX, int clickY, Uint16 modifiers) {
    LibraryPanelState* st = libraries_panel_state();
    int rowIndex = row_index_from_position(pane, clickY);
    if (rowIndex < 0 || rowIndex >= st->flatCount) return;

    LibraryFlatRow* row = &st->flatRows[rowIndex];
    bool clickedPrefix = row_is_prefix_hit(row, pane, clickX);
    bool rangeModifier = ui_input_has_shift(modifiers);
    bool additiveModifier = ui_input_is_additive_selection(modifiers) && !rangeModifier;
    LibraryRowActivationState activation = {
        .panel_state = st,
        .row = row,
        .row_index = rowIndex
    };

    (void)ui_row_activation_handle_primary(
        &(UIRowActivationContext){
            .double_click_tracker = &st->doubleClickTracker,
            .row_identity = (uintptr_t)(uint32_t)rowIndex,
            .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
            .clicked_prefix = clickedPrefix,
            .additive_modifier = additiveModifier,
            .range_modifier = rangeModifier,
            .wants_drag_start = false,
            .on_select_single = library_row_select_single,
            .on_select_additive = library_row_select_additive,
            .on_select_range = library_row_select_range_action,
            .on_prefix = library_row_prefix_action,
            .on_activate = library_row_activate,
            .user_data = &activation
        });

    // Stop selection drag on click release handled elsewhere; for now, this is the anchor set.
}

void updateLibraryDragSelection(UIPane* pane, int mouseY) {
    LibraryPanelState* st = libraries_panel_state();
    if (!st->selecting) return;
    int rowIndex = row_index_from_position(pane, mouseY);
    if (rowIndex < 0 || rowIndex >= st->flatCount) return;
    if (st->dragAnchorRow < 0) st->dragAnchorRow = rowIndex;
    select_range(st->dragAnchorRow, rowIndex);
}

void endLibrarySelectionDrag(void) {
    LibraryPanelState* st = libraries_panel_state();
    st->selecting = false;
}
