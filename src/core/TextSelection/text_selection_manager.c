#include "core/TextSelection/text_selection_manager.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TEXT_SPANS 4096

static TextSelectionSpan g_spans[MAX_TEXT_SPANS];
static size_t g_span_count = 0;

static void free_span(TextSelectionSpan* span) {
    if (span->text) {
        free(span->text);
        span->text = NULL;
    }
    if (span->rects) {
        free(span->rects);
        span->rects = NULL;
    }
    span->text_length = 0;
    span->rect_count = 0;
    span->owner = NULL;
    span->owner_role = PANE_ROLE_UNKNOWN;
    span->flags = TEXT_SELECTION_FLAG_NONE;
    span->copy_handler = NULL;
    span->copy_user_data = NULL;
}

void text_selection_manager_begin_frame(void) {
    for (size_t i = 0; i < g_span_count; ++i) {
        free_span(&g_spans[i]);
    }
    g_span_count = 0;
}

static bool copy_descriptor_into_span(const TextSelectionDescriptor* desc,
                                      TextSelectionSpan* out_span) {
    memset(out_span, 0, sizeof(*out_span));

    if (!desc) return false;

    if (desc->text && desc->text_length > 0) {
        out_span->text = (char*)malloc(desc->text_length + 1);
        if (!out_span->text) {
            return false;
        }
        memcpy(out_span->text, desc->text, desc->text_length);
        out_span->text[desc->text_length] = '\0';
        out_span->text_length = desc->text_length;
    }

    out_span->rect_count = desc->rect_count;
    if (desc->rect_count > 0 && desc->rects) {
        size_t bytes = desc->rect_count * sizeof(TextSelectionRect);
        out_span->rects = (TextSelectionRect*)malloc(bytes);
        if (!out_span->rects) {
            free_span(out_span);
            return false;
        }
        memcpy(out_span->rects, desc->rects, bytes);
    }

    out_span->owner = desc->owner;
    out_span->owner_role = desc->owner_role;
    out_span->flags = desc->flags;
    out_span->copy_handler = desc->copy_handler;
    out_span->copy_user_data = desc->copy_user_data;
    return true;
}

bool text_selection_manager_register(const TextSelectionDescriptor* desc) {
    if (!desc) return false;
    if (g_span_count >= MAX_TEXT_SPANS) {
        return false;
    }
    TextSelectionSpan* span = &g_spans[g_span_count];
    if (!copy_descriptor_into_span(desc, span)) {
        return false;
    }
    if (span->rect_count == 0 && span->text_length > 0 && span->rects == NULL) {
        free_span(span);
        return false;
    }
    if (span->flags == TEXT_SELECTION_FLAG_NONE) {
        span->flags = TEXT_SELECTION_FLAG_SELECTABLE;
    }
    g_span_count++;
    return true;
}

const TextSelectionSpan* text_selection_manager_hit_test(int x, int y) {
    for (size_t i = 0; i < g_span_count; ++i) {
        const TextSelectionSpan* span = &g_spans[i];
        for (size_t r = 0; r < span->rect_count; ++r) {
            SDL_Rect rect = span->rects[r].bounds;
            if (x >= rect.x && x < rect.x + rect.w &&
                y >= rect.y && y < rect.y + rect.h) {
                return span;
            }
        }
    }
    return NULL;
}

size_t text_selection_manager_span_count(void) {
    return g_span_count;
}

const TextSelectionSpan* text_selection_manager_span_at(size_t index) {
    if (index >= g_span_count) return NULL;
    return &g_spans[index];
}
