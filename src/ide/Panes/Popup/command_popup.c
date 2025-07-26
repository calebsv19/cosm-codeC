#include "command_popup.h"
#include "ide/Panes/Popup/popup_system.h"
#include <stdio.h>

void initPopupCommandHandler(UIPane* pane) {
    pane->handleCommand = handlePopupCommand;
}

void handlePopupCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_DISMISS_POPUP:
            printf("[PopupCommand] Dismissing all popups\n");
            hideAllPopups();
            break;

        default:
            printf("[PopupCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}

