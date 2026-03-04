#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/PaneInfo/pane.h"

#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/editor_view.h"

#include "engine/Render/render_pipeline.h"

#include "ide/UI/ui_state.h"

#include "ide/Panes/IconBar/icon_bar.h"


#include "ide/Panes/Editor/Commands/command_editor.h"
#include "ide/Panes/MenuBar/command_menu_bar.h"
#include "ide/Panes/IconBar/command_icon_bar.h"
#include "ide/Panes/Terminal/command_terminal.h"
#include "ide/Panes/ControlPanel/command_control_panel.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/Popup/command_popup.h"
#include "ide/Panes/ToolPanels/command_tool_panel.h"
#include "core/CommandBus/command_metadata.h"

#include "ide/Panes/Editor/Input/input_editor.h"
#include "ide/Panes/MenuBar/input_menu_bar.h"
#include "ide/Panes/Terminal/input_terminal.h"
#include "ide/Panes/ControlPanel/input_control_panel.h"
#include "ide/Panes/Popup/input_popup.h"
#include "ide/Panes/IconBar/input_icon_bar.h"

#include "ide/Panes/ToolPanels/input_tool_panel.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"

#include "ide/Panes/MenuBar/menu_buttons.h"        

        
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
    pane->controllerState = NULL;
    pane->destroyControllerState = NULL;
    pane->handleCommand = NULL;
    pane->inputHandler = NULL;
    pane->scrollState = NULL;
    pane->dirty = true;
    pane->dirtyReasons = PANE_INVALIDATION_LAYOUT;
    pane->hasDirtyRegion = false;
    pane->dirtyRegion = (SDL_Rect){0, 0, 0, 0};
    pane->lastRenderFrameId = 0;
    pane->cacheEnabled = true;
    pane->cacheValid = false;
    pane->cacheWidth = 0;
    pane->cacheHeight = 0;
    pane->cacheVersion = 0;

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
    tool_panel_attach_controller(pane);
    initTaskPanel();
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
    control_panel_attach_controller(pane);
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

void paneMarkDirty(UIPane* pane, uint32_t reasonBits) {
    if (!pane) return;
    pane->dirty = true;
    pane->dirtyReasons |= reasonBits;
    pane->cacheValid = false;
    pane->cacheVersion++;
}

void paneMarkDirtyRect(UIPane* pane, SDL_Rect rect, uint32_t reasonBits) {
    if (!pane) return;
    paneMarkDirty(pane, reasonBits);

    if (!pane->hasDirtyRegion) {
        pane->dirtyRegion = rect;
        pane->hasDirtyRegion = true;
        return;
    }

    int x1 = pane->dirtyRegion.x < rect.x ? pane->dirtyRegion.x : rect.x;
    int y1 = pane->dirtyRegion.y < rect.y ? pane->dirtyRegion.y : rect.y;
    int x2a = pane->dirtyRegion.x + pane->dirtyRegion.w;
    int y2a = pane->dirtyRegion.y + pane->dirtyRegion.h;
    int x2b = rect.x + rect.w;
    int y2b = rect.y + rect.h;
    int x2 = x2a > x2b ? x2a : x2b;
    int y2 = y2a > y2b ? y2a : y2b;

    pane->dirtyRegion.x = x1;
    pane->dirtyRegion.y = y1;
    pane->dirtyRegion.w = x2 - x1;
    pane->dirtyRegion.h = y2 - y1;
}

void paneClearDirty(UIPane* pane) {
    if (!pane) return;
    pane->dirty = false;
    pane->dirtyReasons = PANE_INVALIDATION_NONE;
    pane->hasDirtyRegion = false;
    pane->dirtyRegion = (SDL_Rect){0, 0, 0, 0};
    pane->cacheWidth = pane->w;
    pane->cacheHeight = pane->h;
}

void paneReleaseCache(UIPane* pane) {
    if (!pane) return;
    pane->cacheValid = false;
    pane->cacheWidth = 0;
    pane->cacheHeight = 0;
}




void destroyPane(UIPane* pane) {
    if (!pane) return;

    if (pane->role == PANE_ROLE_EDITOR && pane->editorView) {
        destroyEditorView(pane->editorView);
    }

    if (pane->destroyControllerState && pane->controllerState) {
        pane->destroyControllerState(pane->controllerState);
        pane->controllerState = NULL;
        pane->destroyControllerState = NULL;
    }

    paneReleaseCache(pane);
    free(pane);
}

