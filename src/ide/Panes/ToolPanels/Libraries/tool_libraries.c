#include "tool_libraries.h"
#include "engine/Render/render_text_helpers.h"
#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/core_state.h"
#include "core/Analysis/library_index.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/Clipboard/clipboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

LibraryPanelState g_libraryPanelState = {0};

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
    if (bucketIndex < 0 || bucketIndex >= LIB_BUCKET_COUNT) return 0;
    size_t cur = g_libraryPanelState.headerExpandedCount[bucketIndex];
    if (cur >= count) return 1;
    size_t newCount = count;
    bool* tmp = realloc(g_libraryPanelState.headerExpanded[bucketIndex], newCount * sizeof(bool));
    if (!tmp) return 0;
    // Initialize new slots to false
    for (size_t i = cur; i < newCount; ++i) tmp[i] = false;
    g_libraryPanelState.headerExpanded[bucketIndex] = tmp;
    g_libraryPanelState.headerExpandedCount[bucketIndex] = newCount;
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
    LibraryPanelState* st = &g_libraryPanelState;
    if (!st->selected || st->selectedCapacity <= 0) return;
    if (a < 0 || b < 0) return;
    if (a >= st->flatCount || b >= st->flatCount) return;
    if (a > b) { int tmp = a; a = b; b = tmp; }
    memset(st->selected, 0, (size_t)st->selectedCapacity * sizeof(bool));
    for (int i = a; i <= b && i < st->selectedCapacity; ++i) {
        st->selected[i] = true;
    }
}

bool library_row_is_selected(int idx) {
    LibraryPanelState* st = &g_libraryPanelState;
    if (!st->selected || idx < 0 || idx >= st->selectedCapacity) return false;
    return st->selected[idx];
}

void rebuildLibraryFlatRows(void) {
    LibraryPanelState* st = &g_libraryPanelState;
    clear_flat_rows(st);
    st->flatCount = 0;

    // Reserve space (rough heuristic)
    int estimated = 0;
    library_index_lock();
    for (size_t b = 0; b < library_index_bucket_count(); ++b) {
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

void initLibrariesPanel() {
    memset(&g_libraryPanelState, 0, sizeof(g_libraryPanelState));
    g_libraryPanelState.selectedRow = -1;
    g_libraryPanelState.hoveredRow = -1;
    g_libraryPanelState.dragAnchorRow = -1;
    g_libraryPanelState.selecting = false;
    scroll_state_init(&g_libraryPanelState.scroll, NULL);
    // Buckets default to expanded to show contents.
    for (int i = 0; i < LIB_BUCKET_COUNT; ++i) {
        g_libraryPanelState.bucketExpanded[i] = true;
        g_libraryPanelState.headerExpanded[i] = NULL;
        g_libraryPanelState.headerExpandedCount[i] = 0;
    }
    rebuildLibraryFlatRows();
}

void updateHoveredLibraryMousePosition(int x, int y) {
    (void)x;
    (void)y;
    // Hover handled in render; kept for interface symmetry.
}

static bool row_is_prefix_hit(const LibraryFlatRow* row, UIPane* pane, int clickX) {
    if (!row) return false;
    int indent = row->depth * 20;
    int drawX = pane->x + 12 + indent;
    int prefixWidth = getTextWidth("[-] ");
    return (clickX >= drawX && clickX <= drawX + prefixWidth);
}

static void toggle_bucket(int bucketIndex) {
    if (bucketIndex < 0 || bucketIndex >= LIB_BUCKET_COUNT) return;
    g_libraryPanelState.bucketExpanded[bucketIndex] = !g_libraryPanelState.bucketExpanded[bucketIndex];
    rebuildLibraryFlatRows();
}

static void toggle_header(int bucketIndex, int headerIndex) {
    if (bucketIndex < 0 || bucketIndex >= LIB_BUCKET_COUNT) return;
    ensure_header_expand_capacity(bucketIndex, (size_t)headerIndex + 1);
    g_libraryPanelState.headerExpanded[bucketIndex][headerIndex] =
        !g_libraryPanelState.headerExpanded[bucketIndex][headerIndex];
    rebuildLibraryFlatRows();
}

static void open_usage(const LibraryFlatRow* row) {
    if (!row || row->type != LIB_NODE_USAGE) return;
    if (!row->labelPrimary) return;

    IDECoreState* core = getCoreState();
    if (!core || !core->activeEditorView) return;

    // Build full path if the stored path is relative.
    char fullPath[1024];
    if (row->labelPrimary[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s", row->labelPrimary);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", projectPath, row->labelPrimary);
    }
    OpenFile* file = openFileInView(core->activeEditorView, fullPath);
    if (!file || !file->buffer) return;

    int targetRow = row->usageLine > 0 ? row->usageLine - 1 : 0;
    if (targetRow >= file->buffer->lineCount) targetRow = file->buffer->lineCount - 1;
    if (targetRow < 0) targetRow = 0;
    int lineLen = file->buffer->lines && file->buffer->lines[targetRow]
                      ? (int)strlen(file->buffer->lines[targetRow])
                      : 0;
    int targetCol = row->usageColumn > 0 ? row->usageColumn - 1 : 0;
    if (targetCol > lineLen) targetCol = lineLen;

    file->state.cursorRow = targetRow;
    file->state.cursorCol = targetCol;
    file->state.viewTopRow = (targetRow > 2) ? targetRow - 2 : 0;
    file->state.selecting = false;
    file->state.draggingWithMouse = false;
    setActiveEditorView(core->activeEditorView);
}

void copy_selected_rows(void) {
    LibraryPanelState* st = &g_libraryPanelState;
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
    const int headerHeight = 32;
    int contentY = pane->y + headerHeight;
    float offset = scroll_state_get_offset(&g_libraryPanelState.scroll);
    int localY = mouseY - contentY + (int)offset;
    if (localY < 0) return -1;
    int idx = localY / LIBRARY_ROW_HEIGHT;
    return (idx >= 0 && idx < g_libraryPanelState.flatCount) ? idx : -1;
}

void handleLibraryEntryClick(UIPane* pane, int clickX, int clickY) {
    LibraryPanelState* st = &g_libraryPanelState;
    int rowIndex = row_index_from_position(pane, clickY);
    if (rowIndex < 0 || rowIndex >= st->flatCount) return;

    LibraryFlatRow* row = &st->flatRows[rowIndex];
    Uint32 now = SDL_GetTicks();
    bool isDoubleClick = (st->hoveredRow == st->lastClickedRow) &&
                         (now - st->lastClickTicks < 400);
    st->lastClickedRow = st->hoveredRow;
    st->lastClickTicks = now;

    st->selectedRow = rowIndex;
    st->dragAnchorRow = rowIndex;
    st->selecting = true;
    if (st->selected && st->selectedCapacity > rowIndex) {
        memset(st->selected, 0, (size_t)st->selectedCapacity * sizeof(bool));
        st->selected[rowIndex] = true;
    }

    bool clickedPrefix = row_is_prefix_hit(row, pane, clickX);

    if (row->type == LIB_NODE_BUCKET) {
        if (clickedPrefix || isDoubleClick) toggle_bucket(row->bucketIndex);
    } else if (row->type == LIB_NODE_HEADER) {
        if (clickedPrefix || isDoubleClick) toggle_header(row->bucketIndex, row->headerIndex);
    } else if (row->type == LIB_NODE_USAGE && isDoubleClick) {
        open_usage(row);
    }

    // Stop selection drag on click release handled elsewhere; for now, this is the anchor set.
}

void updateLibraryDragSelection(UIPane* pane, int mouseY) {
    LibraryPanelState* st = &g_libraryPanelState;
    if (!st->selecting) return;
    int rowIndex = row_index_from_position(pane, mouseY);
    if (rowIndex < 0 || rowIndex >= st->flatCount) return;
    if (st->dragAnchorRow < 0) st->dragAnchorRow = rowIndex;
    select_range(st->dragAnchorRow, rowIndex);
}

void endLibrarySelectionDrag(void) {
    g_libraryPanelState.selecting = false;
}
