#include "project_toolbar.h"
#include "core/Ipc/ide_ipc_workspace.h"
#include "tool_project.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "core/InputManager/input_macros.h"
static IDEButtonScope scope;
static uint64_t purpose_generation;
static const InputCommand commands[4] = {COMMAND_NEW_FILE, COMMAND_DELETE_FILE,
                                         COMMAND_NEW_FOLDER, COMMAND_DELETE_FOLDER};
SDL_Rect ide_project_toolbar_rect(UIPane* pane, int index) {
    if (!pane || index < 0 || index >= 4) return (SDL_Rect){0};
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    return (SDL_Rect){pane->x+d.pad_left, tool_panel_info_line_y(pane,1)+d.button_h+index*32,24,24};
}
static void prepare(UIPane* pane) {
    if (purpose_generation != getActiveIconGeneration()) kit_ui_interaction_reset(&scope.interaction);
    purpose_generation = getActiveIconGeneration();
    KitUiInteractionControl controls[4] = {0};
    for (int i = 0; i < 4; ++i) {
        SDL_Rect r = ide_project_toolbar_rect(pane,i);
        bool enabled = i == 1 ? selectedFile != NULL :
                       i == 3 ? selectedDirectory && selectedDirectory != projectRoot : true;
        controls[i] = (KitUiInteractionControl){.id = i+1,
            .bounds = {r.x,r.y,r.w,r.h}, .enabled = enabled};
    }
    IDECoreState* core = getCoreState();
    ide_button_scope_sync(&scope,pane,controls,4,projectRoot,selectedEntry,ide_ipc_workspace_generation(),
        pane && getActiveIcon()==ICON_PROJECT_FILES && !isRenaming() && !renamingEntry &&
        (!core || !core->workspaceAuthoring.active));
}
bool ide_project_toolbar_event(UIPane* pane, SDL_Event* event) {
    prepare(pane);
    uint32_t activated = 0;
    bool consumed = ide_button_scope_event(&scope,event,&activated);
    if (activated && activated<=4) { project_clear_select_all_visual(); CMD(commands[activated-1]); }
    return consumed;
}
KitUiButtonState ide_project_toolbar_state(UIPane* pane, int index) {
    prepare(pane);
    return ide_button_scope_state(&scope,index+1,false);
}
