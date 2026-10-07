#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static char clipboard[4096];
static int clipboard_fail;
static int test_clipboard_set(const char* value) { if(clipboard_fail)return -1;snprintf(clipboard,sizeof(clipboard),"%s",value);return 0; }
static char* test_clipboard_get(void) { return strdup(clipboard); }
#define SDL_SetClipboardText test_clipboard_set
#define SDL_GetClipboardText test_clipboard_get
#include "ide/UI/panel_text_edit.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    char text[32]=""; int cursor=0;
    UIPanelTextEditBuffer b={text,sizeof(text),&cursor};
    assert(ui_panel_text_edit_insert(&b,"Aé水🙂Z")); assert(cursor==11);
    assert(ui_panel_text_edit_move_left(&b) && cursor==10);
    assert(ui_panel_text_edit_backspace(&b)); assert(!strcmp(text,"Aé水Z") && cursor==6);
    assert(ui_panel_text_edit_move_left(&b) && cursor==3);
    assert(ui_panel_text_edit_delete(&b)); assert(!strcmp(text,"AéZ") && cursor==3);
    assert(ui_panel_text_edit_move_left(&b) && cursor==1);
    assert(ui_panel_text_edit_move_right(&b) && cursor==3);
    cursor=2; assert(ui_panel_text_edit_clamp_cursor(&b)==1);
    assert(ui_panel_text_edit_move_end(&b) && cursor==4);
    assert(!ui_panel_text_edit_move_end(&b));
    assert(!ui_panel_text_edit_insert(&b,"\x80")); assert(!strcmp(text,"AéZ") && cursor==4);
    assert(!ui_panel_text_edit_insert(&b,"\xed\xa0\x80"));
    assert(!ui_panel_text_edit_insert(&b,"\xf4\x90\x80\x80"));
    assert(!ui_panel_text_edit_insert(&b,"line\nnext"));
    assert(!ui_panel_text_edit_insert(&b,text)); assert(!strcmp(text,"AéZ"));
    assert(!ui_panel_text_edit_set_text(&b,"abcdefghijklmnopqrstuvwxyz0123456789")); assert(!strcmp(text,"AéZ"));
    assert(!ui_panel_text_edit_set_text(&b,"\xc0\xaf")); assert(!strcmp(text,"AéZ"));
    assert(ui_panel_text_edit_set_text(&b,text+1)); assert(!strcmp(text,"éZ") && cursor==3);
    char small[5]="é"; cursor=2; b=(UIPanelTextEditBuffer){small,sizeof(small),&cursor};
    assert(!ui_panel_text_edit_insert(&b,"水")); assert(!strcmp(small,"é") && cursor==2);
    assert(ui_panel_text_edit_insert(&b,"é")); assert(!strcmp(small,"éé") && cursor==4);
    assert(!ui_panel_text_edit_insert(&b,"x")); assert(cursor==4);
    assert(ui_panel_text_edit_clear(&b)); assert(!small[0] && cursor==0);
    assert(!ui_panel_text_edit_clear(&b));
    assert(ui_panel_text_edit_insert(&b,"abc")); assert(ui_panel_text_edit_move_home(&b));
    assert(!ui_panel_text_edit_backspace(&b)); assert(ui_panel_text_edit_delete(&b)); assert(!strcmp(small,"bc"));
    SDL_Event e={0}; e.type=SDL_TEXTINPUT; strcpy(e.text.text,"é");
    assert(ui_panel_text_edit_handle_text_input(&b,&e)); assert(!strcmp(small,"ébc"));
    assert(ui_panel_text_edit_handle_keydown(&b,SDLK_BACKSPACE)); assert(!strcmp(small,"bc"));
    char unterminated[4]={'a','b','c','d'}; cursor=0; b=(UIPanelTextEditBuffer){unterminated,4,&cursor};
    assert(!ui_panel_text_edit_insert(&b,"x")); assert(!memcmp(unterminated,"abcd",4));
    char retained_text[64]="Aé水🙂Z";cursor=(int)strlen(retained_text);
    KitUiTextEdit retained={0};b=(UIPanelTextEditBuffer){retained_text,sizeof(retained_text),&cursor,&retained};
    assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_LEFT,KMOD_SHIFT));
    assert(cursor==10 && retained.anchor==11);
    assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_LEFT,KMOD_SHIFT));
    assert(cursor==6 && retained.anchor==11);
    KitUiTextEdit rebound={0};assert(ui_panel_text_edit_bind(&b,&rebound));assert(rebound.anchor==11);
    assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_c,KMOD_GUI));assert(!strcmp(clipboard,"🙂Z"));
    clipboard_fail=1;assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_x,KMOD_CTRL));assert(!strcmp(retained_text,"Aé水🙂Z"));
    clipboard_fail=0;assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_x,KMOD_CTRL));assert(!strcmp(retained_text,"Aé水"));
    assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_v,KMOD_GUI));assert(!strcmp(retained_text,"Aé水🙂Z"));
    assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_a,KMOD_CTRL));assert(retained.anchor==0);
    SDL_Event composition={0};composition.type=SDL_TEXTEDITING;strcpy(composition.edit.text,"é水");composition.edit.start=1;composition.edit.length=1;
    assert(ui_panel_text_edit_handle_text_input(&b,&composition));assert(!strcmp(retained_text,"Aé水🙂Z"));assert(!strcmp(retained.composition,"é水"));
    composition.edit.start=9;assert(!ui_panel_text_edit_handle_text_input(&b,&composition));assert(!strcmp(retained.composition,"é水"));
    SDL_Event commit={0};commit.type=SDL_TEXTINPUT;strcpy(commit.text.text,"Done");
    assert(ui_panel_text_edit_handle_text_input(&b,&commit));assert(!strcmp(retained_text,"Done") && !retained.composition[0]);
    assert(ui_panel_text_edit_handle_keydown_mod(&b,SDLK_LEFT,KMOD_SHIFT));ui_panel_text_edit_blur(&b);assert(retained.cursor==retained.anchor);
    composition.edit.start=0;composition.edit.length=0;assert(ui_panel_text_edit_handle_text_input(&b,&composition));
    assert(ui_panel_text_edit_set_text(&b,"new"));assert(!retained.composition[0]);
    assert(!ui_panel_text_edit_handle_keydown_mod(&b,SDLK_LEFT,KMOD_ALT));
    puts("Retained selection, clipboard failure safety, composition commit and blur passed");
    puts("Panel text editing: ASCII parity, scalar boundaries, capacity and malformed UTF-8 passed");
}
