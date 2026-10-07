#include "ide/UI/panel_text_field.h"
#include "ide/UI/panel_text_presentation.h"
#include "render_pipeline.h"
#include "app/GlobalInfo/core_state.h"
#include "kit_ui_native_text_sdl.h"
typedef struct FieldPainter {
    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Rect clip;
    SDL_Color text_color;
    bool native_fallback;
} FieldPainter;
static CoreResult measure_text(void* user, const char* text, float* width) {
    FieldPainter* painter=user;
    int w = 0, h = 0;
    if (painter->native_fallback && measureNativeFontText(painter->font,text,&w,&h)) *width=(float)w;
    else *width=(float)getTextWidthUTF8WithFont(text,painter->font);
    return core_result_ok();
}
static CoreResult paint_rect(void* user, KitRenderRect rect, KitRenderColor color) {
    FieldPainter* painter=user;
    SDL_SetRenderDrawColor(painter->renderer,color.r,color.g,color.b,color.a);
    SDL_Rect r={(int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height};
    SDL_RenderFillRect(painter->renderer,&r);
    return core_result_ok();
}
static CoreResult paint_text(void* user, KitRenderVec2 origin, const char* text) {
    FieldPainter* painter=user;
    drawTextUTF8WithFontColorClipped((int)origin.x,(int)origin.y,text,painter->font,
                                    painter->text_color,false,&painter->clip);
    return core_result_ok();
}
void ui_panel_text_field_render(SDL_Renderer* renderer, const UIPanelTextFieldSpec* spec) {
    RenderContext* context=getRenderContext();
    if (!spec || !context || renderer!=context->renderer || spec->rect.w<=0 || spec->rect.h<=0) return;
    IDEThemePalette palette={0};
    (void)ide_shared_theme_resolve_palette(&palette);
    SDL_SetRenderDrawColor(renderer,palette.input_fill.r,palette.input_fill.g,palette.input_fill.b,80);
    SDL_RenderFillRect(renderer,&spec->rect);
    SDL_Color border=spec->focused ? palette.input_focus_border : palette.input_border;
    SDL_SetRenderDrawColor(renderer,border.r,border.g,border.b,255);
    SDL_RenderDrawRect(renderer,&spec->rect);
    TTF_Font* font=getUIFontByTier(spec->tier);
    if (!font) font=getActiveFont();
    FieldPainter painter={renderer,font,{spec->rect.x+6,spec->rect.y+3,spec->rect.w-12,spec->rect.h-6},palette.text_primary};
    if (painter.clip.w<=0 || painter.clip.h<=0) return;
    const char* text=spec->text ? spec->text : "";
    int native_w=0, native_h=0;
    painter.native_fallback=measureNativeFallbackText(font,text,&native_w,&native_h) != 0;
    if (spec->edit && spec->edit->composition[0])
        painter.native_fallback |= measureNativeFallbackText(font,spec->edit->composition,&native_w,&native_h) != 0;
    KitUiTextPresentation view;
    float line_height=font ? TTF_FontHeight(font) : 14;
    if(spec->edit && spec->edit->text==text){
        KitUiTextPresentationOptions options={.viewport={painter.clip.x,painter.clip.y,painter.clip.w,painter.clip.h},.line_height=line_height,.caret_width=1,.active=spec->focused,.reveal_caret=spec->focused};
        if(kit_ui_text_presentation_build(&view,spec->edit,&options,measure_text,&painter).code!=CORE_OK)return;
    }else if(!ide_panel_text_presentation_build(&view,text,spec->cursor,painter.clip,line_height,spec->focused,measure_text,&painter))return;
    if (spec->owner_pane) ide_panel_text_publish(spec->owner_group, spec->owner_pane, &view, text, measure_text, &painter);
    if (!text[0] && !spec->focused && spec->placeholder && spec->placeholder[0]) {
        painter.text_color=palette.text_muted;
        (void)paint_text(&painter,(KitRenderVec2){painter.clip.x,painter.clip.y},spec->placeholder);
        return;
    }
    if(spec->focused && (!spec->owner_pane || getCoreState()->focusedPane == spec->owner_pane) && context->window && SDL_GetKeyboardFocus()==context->window)
        (void)kit_ui_native_text_rect_sdl(context->window,&view,context->width,context->height,NULL);
    KitUiTextPresentationColors colors={.caret={border.r,border.g,border.b,spec->hide_caret ? 0 : 255},
        .selection={palette.selection_fill.r,palette.selection_fill.g,palette.selection_fill.b,160},
        .preedit={border.r,border.g,border.b,110}};
    KitUiTextPainter callbacks={&painter,paint_rect,paint_text};
    /* Native Vulkan callbacks preserve the existing enclosing pane clip. */
    pushClipRect(&painter.clip);
    (void)kit_ui_text_presentation_paint(&view,&colors,&callbacks);
    popClipRect();
}
