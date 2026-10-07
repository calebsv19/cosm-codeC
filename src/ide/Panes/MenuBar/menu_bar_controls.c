#include "menu_bar_controls.h"

void ide_menu_bar_geometry(const UIPane* pane, MenuBarLayoutMode mode, int filename_width,
                           KitUiInteractionControl controls[IDE_MENU_CONTROL_COUNT]) {
    int width = LEFT_BUTTON_WIDTH;
    if (pane->w < 428) width = (pane->w - 20 - 6 * BUTTON_SPACING) / 6;
    if (width < 0) width = 0;
    int right_x = pane->x + pane->w - 4 * (width + BUTTON_SPACING) - 10;
    int left_x = pane->x + 8;
    int room = right_x - 10 - left_x - 2 * (width + BUTTON_SPACING) - 30;
    if (room < 0) room = 0;
    if (filename_width < 0) filename_width = 0;
    if (filename_width > room) filename_width = room;
    if (mode == MENU_BAR_MODE_STANDARD && room > 0) left_x += filename_width + 30;
    for (int i = 0; i < IDE_MENU_CONTROL_COUNT; ++i) {
        int x = i < 2 ? left_x + i * (width + BUTTON_SPACING)
                      : right_x + (i - 2) * (width + BUTTON_SPACING);
        controls[i] = (KitUiInteractionControl){
            .id = (uint32_t)i + 1,
            .bounds = {(float)x, (float)(pane->y + BUTTON_HEIGHT_PADDING / 2),
                       (float)width, (float)(pane->h - BUTTON_HEIGHT_PADDING)},
            .enabled = pane->visible && width > 0 && pane->h > BUTTON_HEIGHT_PADDING &&
                       x >= pane->x && x + width <= pane->x + pane->w
        };
    }
}

InputCommand ide_menu_bar_command(uint32_t id) {
    static const InputCommand commands[IDE_MENU_CONTROL_COUNT] = {
        COMMAND_CHOOSE_WORKSPACE, COMMAND_SAVE_FILE, COMMAND_BUILD_PROJECT,
        COMMAND_RUN_EXECUTABLE, COMMAND_DEBUG_EXECUTABLE, COMMAND_TOGGLE_CONTROL_PANEL
    };
    return id >= 1 && id <= IDE_MENU_CONTROL_COUNT ? commands[id - 1] : COMMAND_NONE;
}
