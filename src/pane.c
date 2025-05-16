#include "GlobalInfo/core_state.h"
#include "pane.h"

#include "Editor/editor.h"
#include "Editor/editor_view.h"

#include "Render/render_pipeline.h"

#include "UI/ui_state.h"

#include "ToolPanels/icon_bar.h"


#include "CommandBus/Panes/command_editor.h"
#include "CommandBus/Panes/command_menu_bar.h"
#include "CommandBus/Panes/command_icon_bar.h"
#include "CommandBus/Panes/command_terminal.h"
#include "CommandBus/Panes/command_control_panel.h"
#include "CommandBus/Panes/command_popup.h"
#include "CommandBus/ToolPanels/command_tool_panel.h"
#include "CommandBus/command_metadata.h"

#include "InputManager/Panes/input_editor.h"
#include "InputManager/Panes/input_menu_bar.h"
#include "InputManager/Panes/input_terminal.h"
#include "InputManager/Panes/input_control_panel.h"
#include "InputManager/Panes/input_popup.h"

#include "InputManager/Panes/input_icon_bar.h"
#include "InputManager/InputToolPanels/input_tool_panel.h"

#include "MenuBar/menu_buttons.h"        

        
#include <stdlib.h>
#include <string.h>


//              Inits
// 	==================



UIPane* createThemedPane(int x, int y, int w, int h, UIPaneRole role,
                        UITheme* theme, const char* title){

	SDL_Color bg;
	switch (role){
		case PANE_ROLE_MENUBAR:
                        bg = theme->bgMenuBar;
                        break;
		case PANE_ROLE_EDITOR:
			bg = theme->bgEditor;
			break;
                case PANE_ROLE_ICONBAR:
                        bg = theme->bgIconBar;
                        break;
                case PANE_ROLE_TOOLPANEL:
                        bg = theme->bgToolBar;
                        break;
		case PANE_ROLE_CONTROLPANEL:
                        bg = theme->bgControlPanel;
                        break;
                case PANE_ROLE_TERMINAL:
                        bg = theme->bgTerminal;
                        break;
                case PANE_ROLE_POPUP:
                        bg = theme->bgPopup;
                        break;
                default:
                        bg = (SDL_Color) {0,0,0,255};
                        break;
	}
	UIPane* pane = createPane(x,y,w,h, bg, theme->border, title, role);
	if (!pane){	
		fprintf(stderr, "Failed to create pane\n");
    		exit(1);
	}

	if (pane){
		pane->role = role;
	}
	
	return pane;
}


UIPane* createPane(int x, int y, int w, int h, SDL_Color bg, SDL_Color border,
                   const char* title, UIPaneRole role) {
    UIPane* pane = createEmptyPane(role);
    if (!pane) return NULL;

    pane->x = x;
    pane->y = y;
    pane->w = w;
    pane->h = h;
    pane->bgColor = bg;
    pane->borderColor = border;
    pane->title = title;

    applyPaneRoleDefaults(pane);  // Optional helper
    return pane;
}


UIPane* createEmptyPane(UIPaneRole role) {
    UIPane* pane = (UIPane*)malloc(sizeof(UIPane));
    if (!pane) return NULL;

    pane->x = 0;
    pane->y = 0;
    pane->w = 100;
    pane->h = 100;
    pane->bgColor = (SDL_Color){30, 30, 30, 255};
    pane->borderColor = (SDL_Color){80, 80, 80, 255};
    pane->visible = true;
    pane->title = NULL;
    pane->role = role;

    pane->render = NULL;
    pane->editorView = NULL;

    return pane;
}







static void initEditorPane(UIPane* pane) {
    IDECoreState* core = getCoreState();
    pane->render = renderEditor;
    pane->editorView = core->persistentEditorView;
    if (pane->editorView) {
        setParentPaneForView(pane->editorView, pane);
        pane->editorView->parentPane = pane;
    }
    pane->inputHandler = &editorInputHandler;
    initEditorCommandHandler(pane);
    pane->title = "Editor";
}

static void initMenuBarPane(UIPane* pane) {
    pane->render = renderMenuBar;
    pane->inputHandler = &menuBarInputHandler;
    initMenuBarCommandHandler(pane);
    pane->title = "Menu";
}

static void initIconBarPane(UIPane* pane) {
    pane->render = renderIconBar;
    pane->inputHandler = &iconBarInputHandler;
    initIconBarCommandHandler(pane);
    pane->title = "Icons";
}

static void initToolPanelPane(UIPane* pane) {
    pane->render = renderToolPanel;
    pane->inputHandler = &toolPanelInputHandler;
    initToolPanelCommandHandler(pane);
    pane->title = "Tool Panel";
}

static void initTerminalPane(UIPane* pane) {
    pane->render = renderTerminal;
    pane->inputHandler = &terminalInputHandler;
    initTerminalCommandHandler(pane);
    pane->title = "Terminal";
}

static void initControlPanelPane(UIPane* pane) {
    pane->render = renderControlPanel;
    pane->inputHandler = &controlPanelInputHandler;
    initControlPanelCommandHandler(pane);
    pane->title = "Control";
}

static void initPopupPane(UIPane* pane) {
    pane->render = renderPopupQueue;
    pane->inputHandler = &popupInputHandler;
    initPopupCommandHandler(pane);
    pane->title = "Popup";
}



void applyPaneRoleDefaults(UIPane* pane) {
    if (!pane) return;

    switch (pane->role) {
        case PANE_ROLE_MENUBAR:      initMenuBarPane(pane); break;
        case PANE_ROLE_ICONBAR:      initIconBarPane(pane); break;
        case PANE_ROLE_TOOLPANEL:    initToolPanelPane(pane); break;
        case PANE_ROLE_EDITOR:       initEditorPane(pane); break;
        case PANE_ROLE_TERMINAL:     initTerminalPane(pane); break;
        case PANE_ROLE_CONTROLPANEL: initControlPanelPane(pane); break;
        case PANE_ROLE_POPUP:        initPopupPane(pane); break;
        default:
            pane->render = NULL;
            pane->inputHandler = NULL;
            pane->handleCommand = NULL;
            pane->title = "Unknown";
            break;
    }
}











//		Inits
//      ======================
// 		Helpers


bool isPointInsidePane(UIPane* pane, int x, int y) {
    if (!pane || !pane->visible) return false;

    return (
        x >= pane->x &&
        x <= (pane->x + pane->w) &&
        y >= pane->y &&
        y <= (pane->y + pane->h)
    );
}


// 		Helpers
// 	==========================
//              Tree Memory 






void printEditorTree(struct EditorView* view, int depth) {
    if (!view) return;
    for (int i = 0; i < depth; i++) printf("  ");

    printf("View %p | type=%s | x=%d y=%d w=%d h=%d\n",
        (void*)view,
        (view->type == VIEW_SPLIT ? "SPLIT" : "LEAF"),
        view->x, view->y, view->w, view->h
    );

    if (view->type == VIEW_SPLIT) {
        printEditorTree(view->childA, depth + 1);
        printEditorTree(view->childB, depth + 1);
    }
}




void destroyPane(UIPane* pane) {
    if (!pane) return;

    if (pane->role == PANE_ROLE_EDITOR && pane->editorView) {
        destroyEditorView(pane->editorView);
    }

    free(pane);
}









