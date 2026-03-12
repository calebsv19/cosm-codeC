#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/PaneInfo/pane.h"

void invalidatePane(struct UIPane* pane, uint32_t reasonBits) {
    if (!pane) return;
    pane->dirty = true;
    pane->dirtyReasons |= reasonBits;
}

void invalidateAll(struct UIPane** panes, int paneCount, uint32_t reasonBits) {
    if (!panes || paneCount <= 0) return;
    for (int i = 0; i < paneCount; ++i) {
        invalidatePane(panes[i], reasonBits);
    }
}

void requestFullRedraw(uint32_t reasonBits) {
    (void)reasonBits;
}
