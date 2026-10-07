#ifndef IDE_MENU_BAR_CONTROLS_H
#define IDE_MENU_BAR_CONTROLS_H
#include "menu_buttons.h"
#include "kit_ui_interaction.h"
#define IDE_MENU_CONTROL_COUNT 6
/* Shared paint/input geometry, in logical window coordinates. */
void ide_menu_bar_geometry(const UIPane* pane, MenuBarLayoutMode mode, int filename_width,
                           KitUiInteractionControl controls[IDE_MENU_CONTROL_COUNT]);
InputCommand ide_menu_bar_command(uint32_t id);
#endif
