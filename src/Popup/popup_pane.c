#include "popup_pane.h"
#include "popup_system.h"


#include "../GlobalInfo/core_state.h"
#include "../UI/ui_state.h"
#include "Popup/render_popup.h"
#include <string.h>

void syncPopupPane(UIPane** panes, int* paneCount) {
    UIState* ui = getUIState();

    bool needsPopup = isPopupVisible();

    if (needsPopup && ui->popup == NULL) {
        ui->popup = createEmptyPane(PANE_ROLE_POPUP);
        applyPaneRoleDefaults(ui->popup);
    }

    // If it should be visible but isn't in the pane list
    bool popupInList = false;
    for (int i = 0; i < *paneCount; i++) {
        if (panes[i] == ui->popup) {
            popupInList = true;
            break;
        }
    }

    if (needsPopup && ui->popup && !popupInList) {
        panes[(*paneCount)++] = ui->popup;
    }

    if (!needsPopup && ui->popup != NULL) {
        // Remove it from the pane list
        for (int i = 0; i < *paneCount; i++) {
            if (panes[i] == ui->popup) {
                for (int j = i; j < *paneCount - 1; j++) {
                    panes[j] = panes[j + 1];
                }
                (*paneCount)--;
                break;
            }
        }
    }
}

