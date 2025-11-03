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

    coreState.workspacePath[0] = '\0';
    coreState.runTargetPath[0] = '\0';

    memset(&coreState.projectDrag, 0, sizeof(ProjectDragState));
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

void setWorkspacePath(const char* path) {
    if (!path) {
        coreState.workspacePath[0] = '\0';
        return;
    }
    strncpy(coreState.workspacePath, path, sizeof(coreState.workspacePath) - 1);
    coreState.workspacePath[sizeof(coreState.workspacePath) - 1] = '\0';
}

const char* getWorkspacePath(void) {
    return coreState.workspacePath;
}

void setRunTargetPath(const char* path) {
    if (!path || !*path) {
        coreState.runTargetPath[0] = '\0';
        return;
    }
    strncpy(coreState.runTargetPath, path, sizeof(coreState.runTargetPath) - 1);
    coreState.runTargetPath[sizeof(coreState.runTargetPath) - 1] = '\0';
}

const char* getRunTargetPath(void) {
    return coreState.runTargetPath;
}
