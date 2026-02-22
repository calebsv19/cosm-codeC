#include "input_mouse.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/Input/editor_input_mouse.h"
#include "ide/UI/ui_state.h"
#include "app/GlobalInfo/core_state.h"  // Add this to top of input_mouse.c
#include "ide/Panes/ToolPanels/Project/tool_project.h"

#include <stdio.h>

// Used to track fallback pane for motion/up events
static UIPane* activeMousePane = NULL;




// ─────────────────────────────────────────────────────────
//  Routing Mouse Input to Panes
// ─────────────────────────────────────────────────────────

static void routeMouseToTargetPane(SDL_Event* event, UIPane** panes, int paneCount, int mx, int my) {
    IDECoreState* core = getCoreState();  // Get core state

    for (int i = 0; i < paneCount; i++) {
        UIPane* pane = panes[i];
        if (isPointInsidePane(pane, mx, my)) {
            activeMousePane = pane;
            core->activeMousePane = pane;

            // Set focus on click
            if (event->type == SDL_MOUSEBUTTONDOWN) {
                core->focusedPane = pane;
            }

	    if (event->type == SDL_MOUSEWHEEL) {
		if (pane->inputHandler && pane->inputHandler->onScroll)
		        pane->inputHandler->onScroll(pane, event);
	    } else {
		if (pane->inputHandler && pane->inputHandler->onMouse)
		        pane->inputHandler->onMouse(pane, event);
	    }

            break;
        }
    }
}


static void routeMouseDragToPane(SDL_Event* event, UIPane** panes, int paneCount, int mx, int my) {
    for (int i = 0; i < paneCount; i++) {
        UIPane* pane = panes[i];
        if (isPointInsidePane(pane, mx, my)) {
            if (pane->inputHandler && pane->inputHandler->onMouse)
                pane->inputHandler->onMouse(pane, event);
            break;
        }
    }
}

static void routeMouseMotionToLastPane(SDL_Event* event) {
    if (activeMousePane && activeMousePane->inputHandler && activeMousePane->inputHandler->onMouse) {
        activeMousePane->inputHandler->onMouse(activeMousePane, event);
    }
}

static void routeMouseReleaseToLastPane(SDL_Event* event) {
    if (activeMousePane && activeMousePane->inputHandler && activeMousePane->inputHandler->onMouse) {
        activeMousePane->inputHandler->onMouse(activeMousePane, event);
    }
    activeMousePane = NULL;
    getCoreState()->activeMousePane = NULL;
}





// ─────────────────────────────────────────────────────────
//  Mouse Handling Entry Point
// ─────────────────────────────────────────────────────────
void handleMouseInput(SDL_Event* event, UIPane** panes, int paneCount) {
    int mx = 0, my = 0;

    switch (event->type) {
        case SDL_MOUSEMOTION:         mx = event->motion.x; my = event->motion.y; break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:       mx = event->button.x; my = event->button.y; break;
        case SDL_MOUSEWHEEL:          SDL_GetMouseState(&mx, &my); break;
        default:                      return;
    }

    ProjectDragState* drag = &getCoreState()->projectDrag;
    bool projectDragActive = (drag->entry != NULL);

    if (projectDragActive) {
        if (event->type == SDL_MOUSEMOTION) {
            updateProjectDrag(mx, my);
            return;
        } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
            finalizeProjectDrag(event->button.x, event->button.y);
            return;
        }
        return;
    }

    if (handleEditorSplitDividerInteraction(event, mx, my)) return;
    if (handleEditorScrollbarThumbClick(event, mx, my)) return;
    if (handleEditorScrollbarDrag(event)) return;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            
        case SDL_MOUSEWHEEL:
            routeMouseToTargetPane(event, panes, paneCount, mx, my);
            break;

        case SDL_MOUSEMOTION:
            if (event->motion.state & SDL_BUTTON_LMASK)
                routeMouseDragToPane(event, panes, paneCount, mx, my);
            else
                routeMouseMotionToLastPane(event);
            break;

        case SDL_MOUSEBUTTONUP:
            routeMouseReleaseToLastPane(event);
            break;
    }
}
