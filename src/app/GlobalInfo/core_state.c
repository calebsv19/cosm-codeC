#include "core_state.h"
#include "ide/Panes/Editor/editor_view_state.h"

#include <string.h>  // for memset
#include <stdlib.h>  // for malloc, free

static IDECoreState coreState;

IDECoreState* getCoreState(void) {
    return &coreState;
}

void initCoreState(void) {
    coreState.editorViewCount = 1;
    coreState.editorPane = NULL;
    coreState.activeEditorDragPane = NULL;
    coreState.activeMousePane = NULL;

    coreState.activeEditorView = NULL;
    coreState.persistentEditorView = NULL;
    coreState.hoveredEditorView = NULL;

    // Allocate and initialize global editor view interaction state
    coreState.editorViewState = malloc(sizeof(EditorViewState));
    if (coreState.editorViewState) {
        memset(coreState.editorViewState, 0, sizeof(EditorViewState));
    }

    coreState.initializePopup = false;
    coreState.popupPaneActive = false;

    // Zero UI/layout state
    memset(&coreState.ui, 0, sizeof(UIState));
    memset(&coreState.layout, 0, sizeof(LayoutDimensions));

    // Optionally clear rename flow
    memset(&coreState.renameFlow, 0, sizeof(RenameRequest));
}

void shutdownCoreState(void) {
    if (coreState.editorViewState) {
        free(coreState.editorViewState);
        coreState.editorViewState = NULL;
    }
}

void setTimerHudEnabled(bool enabled) {
    coreState.timerHudEnabled = enabled;
}

bool isTimerHudEnabled(void) {
    return coreState.timerHudEnabled;
}

void setHoveredEditorView(struct EditorView* view) {
    coreState.hoveredEditorView = view;
}

struct EditorView* getHoveredEditorView(void) {
    return coreState.hoveredEditorView;
}
