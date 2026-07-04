
#include "ide/Panes/IconBar/render_icon_bar.h"
#include "engine/Render/render_pipeline.h"            
#include "engine/Render/render_helpers.h"    // for drawText and renderUIPane
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/panel_control_widgets.h"

#include "app/GlobalInfo/system_control.h"


#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/IconBar/icon_bar.h"   // for getActiveIcon() and ICON_COUNT

#include <SDL2/SDL.h>

void renderIconBarContents(UIPane* pane, bool hovered, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    int mouse_x = 0;
    int mouse_y = 0;
    Uint32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    bool mouse_pressed = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    IconTool active = getActiveIcon();

    (void)hovered;
    (void)core;

    SelectableTextOptions titleOpts = {
        .pane = pane,
        .owner = pane,
        .owner_role = pane->role,
        .x = pane->x + 8,
        .y = pane->y + 6,
        .maxWidth = pane->w - 16,
        .text = pane->title,
        .flags = TEXT_SELECTION_FLAG_SELECTABLE,
    };
    drawSelectableText(&titleOpts);

    int iconCount = ICON_COUNT;
    for (int i = 0; i < iconCount; i++) {
        SDL_Rect icon = getIconRect(pane, i);
        bool icon_hovered = ui_panel_rect_contains(&icon, mouse_x, mouse_y);
        const char* label = "?";
        switch (i) {
            case ICON_PROJECT_FILES: label = "P"; break;
            case ICON_LIBRARIES:     label = "L"; break;
            case ICON_BUILD_OUTPUT:  label = "B"; break;
            case ICON_ERRORS:        label = "E"; break;
            case ICON_ASSET_MANAGER:   label = "A"; break;
            case ICON_TASKS:           label = "T"; break;
            case ICON_VERSION_CONTROL: label = "G"; break; // Git
            default:                 label = "";  break;
        }
        ui_panel_compact_button_render(renderer,
                                       &(UIPanelCompactButtonSpec){
                                           .rect = icon,
                                           .label = label,
                                           .hovered = icon_hovered,
                                           .active = i == active,
                                           .pressed = icon_hovered && mouse_pressed,
                                           .disabled = false,
                                           .outlined = false,
                                           .use_custom_fill = false,
                                           .use_custom_outline = false,
                                           .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                       });
    }
}
