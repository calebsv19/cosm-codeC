#include "panel_text_presentation.h"
#include <string.h>
bool ide_panel_text_presentation_build(KitUiTextPresentation* view, const char* text,
    int cursor, SDL_Rect viewport, float line_height, bool focused, KitUiTextMeasure measure, void* user) {
    if (!view || !measure || viewport.w<=0 || viewport.h<=0) return false;
    if (!text) text="";
    KitUiTextEdit edit={0};
    /* Binding/positioning validate boundaries without modifying borrowed text. */
    if (kit_ui_text_bind(&edit,(char*)text,strlen(text)+1,KIT_UI_TEXT_SINGLE_LINE).code!=CORE_OK) return false;
    size_t position=cursor<0 ? 0 : (size_t)cursor;
    if (kit_ui_text_position(&edit,position,position).code!=CORE_OK) return false;
    KitUiTextPresentationOptions options={.viewport={viewport.x,viewport.y,viewport.w,viewport.h},
        .line_height=line_height,.caret_width=1,.wrap=0,.active=focused,.reveal_caret=focused};
    return kit_ui_text_presentation_build(view,&edit,&options,measure,user).code==CORE_OK;
}
