#ifndef CORE_STATE_H
#define CORE_STATE_H

#include <stdbool.h>

#include "ide/UI/ui_state.h"
#include "ide/UI/layout_config.h"

#include "core/InputManager/UserInput/rename_flow.h"


struct EditorView;
struct EditorViewState;
struct UIPane;

// All core state lives here
typedef struct IDECoreState {
    // Global UI/Editor infrastructure
    struct UIPane* editorPane;
    struct UIPane* activeEditorDragPane;
    struct UIPane* activeMousePane;

    struct UIPane* focusedPane;

    struct EditorView* activeEditorView;
    struct EditorView* persistentEditorView;
    struct EditorViewState* editorViewState;

    // Counters and toggles
    int editorViewCount;

    bool initializePopup;
    bool popupPaneActive;
    bool timerHudEnabled;

    // Modular subsystems
    UIState ui;
    LayoutDimensions layout;


    int mouseX;
    int mouseY;

    // Track rename flow globally
    RenameRequest renameFlow;

    // Add more as needed (e.g., diagnostics, plugin state, build info...)
} IDECoreState;

// Accessor to global state
IDECoreState* getCoreState(void);

void setTimerHudEnabled(bool enabled);
bool isTimerHudEnabled(void);

// Lifecycle (optional for future init/cleanup)
void initCoreState(void);
void shutdownCoreState(void);

#endif // CORE_STATE_H
