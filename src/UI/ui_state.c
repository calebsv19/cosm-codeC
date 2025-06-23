#include "ui_state.h"
#include "PaneInfo/pane.h"
#include "../GlobalInfo/core_state.h"


// Define the single global UIState instance
static UIState globalUIState = {
    .toolPanelVisible = true,
    .controlPanelVisible = true,
    .terminalVisible = true,

    .menuBar = NULL,
    .iconBar = NULL,
    .toolPanel = NULL,
    .editorPanel = NULL,
    .controlPanel = NULL,
    .terminalPanel = NULL,
    .popup = NULL,

    .panesInitialized = false
};

// Provide access to the shared instance
UIState* getUIState(void) {
    return &globalUIState;
}


void initializeUIPanesIfNeeded() {
    UIState* ui = getUIState();
    if (ui->panesInitialized) return;

    ui->menuBar       = createEmptyPane(PANE_ROLE_MENUBAR);
    ui->iconBar       = createEmptyPane(PANE_ROLE_ICONBAR);
    ui->toolPanel     = createEmptyPane(PANE_ROLE_TOOLPANEL);
    ui->editorPanel   = createEmptyPane(PANE_ROLE_EDITOR);
    ui->controlPanel  = createEmptyPane(PANE_ROLE_CONTROLPANEL);
    ui->terminalPanel = createEmptyPane(PANE_ROLE_TERMINAL);

    // Apply render/event behavior to each pane
    applyPaneRoleDefaults(ui->menuBar);
    applyPaneRoleDefaults(ui->iconBar);
    applyPaneRoleDefaults(ui->toolPanel);
    applyPaneRoleDefaults(ui->editorPanel);
    applyPaneRoleDefaults(ui->controlPanel);
    applyPaneRoleDefaults(ui->terminalPanel);

    if (getCoreState()->initializePopup) {
	ui->popup = createEmptyPane(PANE_ROLE_POPUP);
        applyPaneRoleDefaults(ui->popup);
    }

    ui->panesInitialized = true;
}
