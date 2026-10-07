#ifndef IDE_UI_PANEL_TEXT_EDIT_H
#define IDE_UI_PANEL_TEXT_EDIT_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>
#include "kit_ui_text_edit.h"

typedef struct UIPanelTextEditBuffer {
    char* text;
    int capacity;
    int* cursor;
    KitUiTextEdit* retained;
} UIPanelTextEditBuffer;

/* Bounded fields borrow the host buffer/cursor. KitUI owns UTF-8 mechanics;
 * controllers retain focus, submission, persistence and selection policy. */
static inline bool ui_panel_text_edit_valid(const UIPanelTextEditBuffer* buffer) {
    return buffer && buffer->text && buffer->cursor && buffer->capacity > 1;
}
static inline bool ui_panel_text_edit_bind(UIPanelTextEditBuffer* buffer, KitUiTextEdit* edit) {
    if (!ui_panel_text_edit_valid(buffer) || !edit) return false;
    *edit=buffer->retained ? *buffer->retained : (KitUiTextEdit){0};
    bool same=edit->text==buffer->text && edit->capacity==(size_t)buffer->capacity;
    if (kit_ui_text_bind(edit,buffer->text,(size_t)buffer->capacity,KIT_UI_TEXT_SINGLE_LINE).code!=CORE_OK)return false;
    size_t cursor=*buffer->cursor<0?0:(size_t)*buffer->cursor;
    if(!same || cursor!=edit->cursor){
        if(kit_ui_text_position(edit,cursor,cursor).code!=CORE_OK)return false;
    }
    return true;
}
static inline void ui_panel_text_edit_save(UIPanelTextEditBuffer* buffer,const KitUiTextEdit* edit) {
    *buffer->cursor=(int)edit->cursor;
    if(buffer->retained)*buffer->retained=*edit;
}
static inline bool ui_panel_text_edit_pointer(UIPanelTextEditBuffer* buffer, size_t position, bool extend) {
    KitUiTextEdit edit = {0};
    if (!ui_panel_text_edit_bind(buffer, &edit)) return false;
    size_t anchor = extend ? edit.anchor : position;
    if (kit_ui_text_position(&edit, position, anchor).code != CORE_OK) return false;
    ui_panel_text_edit_save(buffer, &edit);
    return true;
}
static inline void ui_panel_text_edit_blur(UIPanelTextEditBuffer* buffer) {
    KitUiTextEdit edit={0};
    if(!ui_panel_text_edit_bind(buffer,&edit))return;
    (void)kit_ui_text_position(&edit,edit.cursor,edit.cursor);
    ui_panel_text_edit_save(buffer,&edit);
}
static inline int ui_panel_text_edit_clamp_cursor(UIPanelTextEditBuffer* buffer) {
    KitUiTextEdit edit = {0};
    return ui_panel_text_edit_bind(buffer, &edit) ? (int)edit.cursor : 0;
}
static inline bool ui_panel_text_edit_set_text(UIPanelTextEditBuffer* buffer, const char* text) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    if (!text) text = "";
    size_t length = strlen(text);
    if (length >= (size_t)buffer->capacity || strchr(text, '\n') || strchr(text, '\r')) return false;
    /* Bind validates without modifying the source, including aliased input. */
    KitUiTextEdit source = {0};
    if (kit_ui_text_bind(&source, (char*)text, length+1, KIT_UI_TEXT_SINGLE_LINE).code != CORE_OK) return false;
    memmove(buffer->text, text, length+1);
    *buffer->cursor = (int)length;
    if(buffer->retained)memset(buffer->retained,0,sizeof(*buffer->retained));
    return true;
}
static inline bool ui_panel_text_edit_insert(UIPanelTextEditBuffer* buffer, const char* text) {
    if (!text || !text[0]) return false;
    KitUiTextEdit edit = {0};
    if (!ui_panel_text_edit_bind(buffer, &edit)) return false;
    if (kit_ui_text_insert(&edit, text).code != CORE_OK) return false;
    ui_panel_text_edit_save(buffer,&edit);
    return true;
}
static inline bool ui_panel_text_edit_command(UIPanelTextEditBuffer* buffer, KitUiTextCommand command) {
    KitUiTextEdit edit = {0};
    if (!ui_panel_text_edit_bind(buffer, &edit)) return false;
    size_t before = strlen(edit.text), cursor = edit.cursor;
    if (kit_ui_text_command(&edit, command, 0).code != CORE_OK) return false;
    ui_panel_text_edit_save(buffer,&edit);
    return before != strlen(edit.text) || cursor != edit.cursor;
}
static inline bool ui_panel_text_edit_backspace(UIPanelTextEditBuffer* b) { return ui_panel_text_edit_command(b, KIT_UI_TEXT_BACKSPACE); }
static inline bool ui_panel_text_edit_delete(UIPanelTextEditBuffer* b) { return ui_panel_text_edit_command(b, KIT_UI_TEXT_DELETE); }
static inline bool ui_panel_text_edit_move_left(UIPanelTextEditBuffer* b) { return ui_panel_text_edit_command(b, KIT_UI_TEXT_LEFT); }
static inline bool ui_panel_text_edit_move_right(UIPanelTextEditBuffer* b) { return ui_panel_text_edit_command(b, KIT_UI_TEXT_RIGHT); }
static inline bool ui_panel_text_edit_move_home(UIPanelTextEditBuffer* b) { return ui_panel_text_edit_command(b, KIT_UI_TEXT_HOME); }
static inline bool ui_panel_text_edit_move_end(UIPanelTextEditBuffer* b) { return ui_panel_text_edit_command(b, KIT_UI_TEXT_END); }
static inline bool ui_panel_text_edit_clear(UIPanelTextEditBuffer* buffer) {
    if (!ui_panel_text_edit_valid(buffer)) return false;
    if (!buffer->text[0] && *buffer->cursor == 0) { ui_panel_text_edit_blur(buffer);return false; }
    buffer->text[0] = '\0'; *buffer->cursor = 0;
    if(buffer->retained)memset(buffer->retained,0,sizeof(*buffer->retained));
    return true;
}

static inline bool ui_panel_text_edit_handle_text_input(UIPanelTextEditBuffer* buffer,
                                                        const SDL_Event* event) {
    if(!event)return false;
    if(event->type==SDL_TEXTINPUT)return ui_panel_text_edit_insert(buffer,event->text.text);
    if(event->type==SDL_TEXTEDITING){
        KitUiTextEdit edit={0};
        if(!ui_panel_text_edit_bind(buffer,&edit) || strchr(event->edit.text,'\n') || strchr(event->edit.text,'\r'))return false;
        if(kit_ui_text_compose(&edit,event->edit.text,event->edit.start,event->edit.length).code!=CORE_OK)return false;
        ui_panel_text_edit_save(buffer,&edit);return true;
    }
    return false;
}
/* Navigation/selection is handled even at an edge, preventing command leakage. */
static inline bool ui_panel_text_edit_handle_keydown_mod(UIPanelTextEditBuffer* buffer,SDL_Keycode key,SDL_Keymod mod) {
    KitUiTextEdit edit={0};
    if(!ui_panel_text_edit_bind(buffer,&edit))return false;
    bool accel=(mod&(KMOD_CTRL|KMOD_GUI))!=0;
    if(mod&KMOD_ALT)return false;
    if(accel && (key==SDLK_c || key==SDLK_x)){
        char selected[4096];
        if(kit_ui_text_selection(&edit,selected,sizeof(selected)).code!=CORE_OK)return true;
        if(selected[0] && SDL_SetClipboardText(selected)==0 && key==SDLK_x){
            if(kit_ui_text_command(&edit,KIT_UI_TEXT_DELETE,0).code==CORE_OK)ui_panel_text_edit_save(buffer,&edit);
        }
        return true;
    }
    if(accel && key==SDLK_v){
        char* text=SDL_GetClipboardText();
        if(text){(void)ui_panel_text_edit_insert(buffer,text);SDL_free(text);}return true;
    }
    KitUiTextCommand command;
    if(accel && key==SDLK_a)command=KIT_UI_TEXT_SELECT_ALL;
    else{
        if(accel)return false;
        switch(key){
            case SDLK_BACKSPACE:command=KIT_UI_TEXT_BACKSPACE;break;
            case SDLK_DELETE:command=KIT_UI_TEXT_DELETE;break;
            case SDLK_LEFT:command=KIT_UI_TEXT_LEFT;break;
            case SDLK_RIGHT:command=KIT_UI_TEXT_RIGHT;break;
            case SDLK_HOME:command=KIT_UI_TEXT_HOME;break;
            case SDLK_END:command=KIT_UI_TEXT_END;break;
            default:return false;
        }
    }
    if(kit_ui_text_command(&edit,command,(mod&KMOD_SHIFT)!=0).code==CORE_OK)ui_panel_text_edit_save(buffer,&edit);
    return true;
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
