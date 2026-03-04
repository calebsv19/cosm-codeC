#ifndef IDE_UI_PANEL_TEXT_EDIT_H
#define IDE_UI_PANEL_TEXT_EDIT_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>

typedef struct UIPanelTextEditBuffer {
    char* text;
    int capacity;
    int* cursor;
} UIPanelTextEditBuffer;

static inline bool ui_panel_text_edit_valid(const UIPanelTextEditBuffer* buffer) {
    return buffer && buffer->text && buffer->cursor && buffer->capacity > 1;
}

static inline int ui_panel_text_edit_clamp_cursor(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return 0;
    int len = (int)strlen(buffer->text);
    int cur = *buffer->cursor;
    if (cur < 0) cur = 0;
    if (cur > len) cur = len;
    *buffer->cursor = cur;
    return cur;
}

static inline bool ui_panel_text_edit_set_text(UIPanelTextEditBuffer* buffer, const char* text) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    if (!text) text = "";
    snprintf(buffer->text, (size_t)buffer->capacity, "%s", text);
    *buffer->cursor = (int)strlen(buffer->text);
    return true;
}

static inline bool ui_panel_text_edit_insert(UIPanelTextEditBuffer* buffer, const char* text) {
    if (!ui_panel_text_edit_valid(buffer) || !text || !text[0]) return false;
    size_t cur_len = strlen(buffer->text);
    int cur = ui_panel_text_edit_clamp_cursor(buffer);
    size_t insert_len = strlen(text);
    size_t room = (size_t)(buffer->capacity - 1) - cur_len;
    if (room == 0) return false;
    if (insert_len > room) insert_len = room;
    memmove(buffer->text + cur + insert_len,
            buffer->text + cur,
            cur_len - (size_t)cur + 1);
    memcpy(buffer->text + cur, text, insert_len);
    *buffer->cursor = cur + (int)insert_len;
    return true;
}

static inline bool ui_panel_text_edit_backspace(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    size_t len = strlen(buffer->text);
    int cur = ui_panel_text_edit_clamp_cursor(buffer);
    if (cur <= 0 || len == 0) return false;
    int remove_at = cur - 1;
    memmove(buffer->text + remove_at,
            buffer->text + remove_at + 1,
            len - (size_t)remove_at);
    *buffer->cursor = remove_at;
    return true;
}

static inline bool ui_panel_text_edit_delete(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    size_t len = strlen(buffer->text);
    int cur = ui_panel_text_edit_clamp_cursor(buffer);
    if ((size_t)cur >= len) return false;
    memmove(buffer->text + cur,
            buffer->text + cur + 1,
            len - (size_t)cur);
    return true;
}

static inline bool ui_panel_text_edit_move_left(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    int cur = ui_panel_text_edit_clamp_cursor(buffer);
    if (cur <= 0) return false;
    *buffer->cursor = cur - 1;
    return true;
}

static inline bool ui_panel_text_edit_move_right(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    int cur = ui_panel_text_edit_clamp_cursor(buffer);
    int len = (int)strlen(buffer->text);
    if (cur >= len) return false;
    *buffer->cursor = cur + 1;
    return true;
}

static inline bool ui_panel_text_edit_move_home(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    ui_panel_text_edit_clamp_cursor(buffer);
    if (*buffer->cursor == 0) return false;
    *buffer->cursor = 0;
    return true;
}

static inline bool ui_panel_text_edit_move_end(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    int end = (int)strlen(buffer->text);
    if (*buffer->cursor == end) return false;
    *buffer->cursor = end;
    return true;
}

static inline bool ui_panel_text_edit_clear(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    if (!buffer->text[0] && *buffer->cursor == 0) return false;
    buffer->text[0] = '\0';
    *buffer->cursor = 0;
    return true;
}

static inline bool ui_panel_text_edit_handle_text_input(UIPanelTextEditBuffer* buffer,
                                                        const SDL_Event* event) {
    if (!event || event->type != SDL_TEXTINPUT) return false;
    return ui_panel_text_edit_insert(buffer, event->text.text);
}

static inline bool ui_panel_text_edit_handle_keydown(UIPanelTextEditBuffer* buffer,
                                                     SDL_Keycode key) {
    switch (key) {
        case SDLK_BACKSPACE: return ui_panel_text_edit_backspace(buffer);
        case SDLK_DELETE: return ui_panel_text_edit_delete(buffer);
        case SDLK_LEFT: return ui_panel_text_edit_move_left(buffer);
        case SDLK_RIGHT: return ui_panel_text_edit_move_right(buffer);
        case SDLK_HOME: return ui_panel_text_edit_move_home(buffer);
        case SDLK_END: return ui_panel_text_edit_move_end(buffer);
        default: return false;
    }
}

#endif
