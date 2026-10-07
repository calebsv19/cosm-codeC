#include "panel_field_focus.h"
#include "core/Ipc/ide_ipc_workspace.h"
#include "ui_state.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "core/InputManager/UserInput/rename_flow.h"
void ide_panel_fields_blur_except(UIPane* pane) {
    UIState* ui = getUIState();
    if ((!pane || pane != ui->controlPanel) && control_panel_is_search_focused())
        control_panel_set_search_focused(false);
    if ((!pane || pane != ui->toolPanel) && errors_is_search_focused())
        errors_set_search_focused(false);
    if ((!pane || pane != ui->toolPanel) && git_panel_is_message_focused())
        git_panel_set_message_focus(false);
}
void ide_panel_fields_reconcile(void) {
    static uint64_t previous_workspace_generation;
    uint64_t generation = ide_ipc_workspace_generation();
    if (previous_workspace_generation && generation != previous_workspace_generation)
        ide_panel_fields_blur_except(NULL);
    previous_workspace_generation = generation;
    IDECoreState* core = getCoreState();
    UIState* ui = getUIState();
    UIPane* owner = core ? core->focusedPane : NULL;
    if (!core || isRenaming() || core->popupPaneActive || core->workspaceAuthoring.active)
        owner = NULL;
    ide_panel_fields_blur_except(owner);
    if ((!ui->controlPanelVisible || !ui->controlPanel || !ui->controlPanel->visible) && control_panel_is_search_focused())
        control_panel_set_search_focused(false);
    bool tool_visible = ui->toolPanelVisible && ui->toolPanel && ui->toolPanel->visible;
    if ((!tool_visible || getActiveIcon() != ICON_ERRORS) && errors_is_search_focused())
        errors_set_search_focused(false);
    if ((!tool_visible || getActiveIcon() != ICON_VERSION_CONTROL) && git_panel_is_message_focused())
        git_panel_set_message_focus(false);
}
