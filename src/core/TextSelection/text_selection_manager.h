#ifndef TEXT_SELECTION_MANAGER_H
#define TEXT_SELECTION_MANAGER_H

#include <stddef.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "ide/Panes/PaneInfo/pane_role.h"

struct TextSelectionSpan;

typedef struct TextSelectionRect {
    SDL_Rect bounds;
    int line;
    int column_start;
    int column_end;
} TextSelectionRect;

typedef char* (*TextSelectionCopyHandler)(const struct TextSelectionSpan* span, void* user_data);

typedef enum TextSelectionFlags {
    TEXT_SELECTION_FLAG_NONE = 0,
    TEXT_SELECTION_FLAG_SELECTABLE = 1 << 0,
    TEXT_SELECTION_FLAG_PRIMARY = 1 << 1,
    TEXT_SELECTION_FLAG_INTERACTIVE = 1 << 2,
} TextSelectionFlags;

typedef struct TextSelectionSpan {
    TextSelectionRect* rects;
    size_t rect_count;
    char* text;
    size_t text_length;
    void* owner;
    UIPaneRole owner_role;
    unsigned int flags;
    TextSelectionCopyHandler copy_handler;
    void* copy_user_data;
} TextSelectionSpan;

typedef struct TextSelectionDescriptor {
    void* owner;
    UIPaneRole owner_role;
    const char* text;
    size_t text_length;
    const TextSelectionRect* rects;
    size_t rect_count;
    unsigned int flags;
    TextSelectionCopyHandler copy_handler;
    void* copy_user_data;
} TextSelectionDescriptor;

void text_selection_manager_begin_frame(void);
bool text_selection_manager_register(const TextSelectionDescriptor* desc);
const TextSelectionSpan* text_selection_manager_hit_test(int x, int y);
size_t text_selection_manager_span_count(void);
const TextSelectionSpan* text_selection_manager_span_at(size_t index);

#endif /* TEXT_SELECTION_MANAGER_H */
