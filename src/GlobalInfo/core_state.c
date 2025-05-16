
#include "core_state.h"
#include <string.h>  // for memset if needed

static IDECoreState coreState;

IDECoreState* getCoreState(void) {
    return &coreState;
}

void initCoreState(void) {
    // Optional: zero/init struct fields here
    coreState.editorViewCount = 1;
    coreState.editorPane = NULL;
    coreState.activeEditorDragPane = NULL;
    coreState.activeMousePane = NULL;

    coreState.activeEditorView = NULL;
    coreState.persistentEditorView = NULL;

    coreState.initializePopup = false;
    coreState.popupPaneActive = false;

    // Zero UIState/LayoutDimensions
    memset(&coreState.ui, 0, sizeof(UIState));
    memset(&coreState.layout, 0, sizeof(LayoutDimensions));
}

void shutdownCoreState(void) {
    // Optional: free dynamic subsystems if any
}

