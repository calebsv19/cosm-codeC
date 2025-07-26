#include "editor_input_mouse.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "engine/Render/render_text_helpers.h"


bool resetCursorPositionToMouse(UIPane* pane, int mouseX, int mouseY,
                                 EditorBuffer* buffer, EditorState* state) {
    const int lineHeight = 20;
    const int paddingLeft = 8;
    const int textX = pane->x + paddingLeft;
    const int textY = pane->y + HEADER_HEIGHT + 2 + state->verticalPadding;

    // If buffer is empty or not initialized
    if (buffer->lineCount <= 0 || buffer->lines == NULL) {
        state->cursorRow = 0;
        state->cursorCol = 0;
        return false;
    }

    int contentTop = textY;
    int contentBottom = pane->y + pane->h;

    if (mouseY < contentTop) mouseY = contentTop;
    if (mouseY > contentBottom - 1) mouseY = contentBottom - 1;

    int visibleLineIndex = (mouseY - textY) / lineHeight;
    int newRow = state->viewTopRow + visibleLineIndex;
    if (newRow < 0) newRow = 0;
    if (newRow >= buffer->lineCount) newRow = buffer->lineCount - 1;

    const char* line = buffer->lines[newRow];
    if (!line) line = "";

    int newCol = 0;
    int accumulatedWidth = textX;

    while (line[newCol]) {
        int charWidth = getTextWidthN(line, newCol + 1) - getTextWidthN(line, newCol);
        if (accumulatedWidth + charWidth / 2 >= mouseX) break;
        accumulatedWidth += charWidth;
        newCol++;
    }

    int lineLen = strlen(line);
    if (newCol > lineLen) newCol = lineLen;

    bool moved = (newRow != state->cursorRow || newCol != state->cursorCol);
    state->cursorRow = newRow;
    state->cursorCol = newCol;

    return moved;
}



//              helpers
// ============================================
//		DRAG


static void adjustCursorColWithinLine(EditorBuffer* buffer, EditorState* state) {
    const char* newLine = buffer->lines[state->cursorRow];
    int newLen = newLine ? strlen(newLine) : 0;
    if (state->cursorCol > newLen)
        state->cursorCol = newLen;
}



static void handleCommandMouseDragOutside(UIPane* pane, EditorBuffer* buffer,
                                          EditorState* state, int mouseY,
                                          int paneTop, int paneBottom, bool backInside) {
    if (backInside) {
        state->draggingOutsidePane = false;
        state->draggingReturnedToPane = true;

        bool moved = resetCursorPositionToMouse(pane, state->lastMouseX, state->lastMouseY, 
							buffer, state);
        if (moved && !state->selecting) {
            state->selecting = true;
            state->selStartRow = state->cursorRow;
            state->selStartCol = state->cursorCol;
        }
    } else {
        if (mouseY < paneTop && state->cursorRow > 0) {
            state->cursorRow--;
            adjustCursorColWithinLine(buffer, state);
        }
        if (mouseY > paneBottom && state->cursorRow < buffer->lineCount - 1) {
            state->cursorRow++;
            adjustCursorColWithinLine(buffer, state);
        }
    }
}

static void handleCommandMouseDragInPane(UIPane* pane, EditorBuffer* buffer,
                                         EditorState* state, int mouseX, int mouseY) {
    resetCursorPositionToMouse(pane, mouseX, mouseY, buffer, state);
}

void handleEditorMouseDrag(UIPane* pane, SDL_Event* event, EditorView* view) {
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;


    int mouseX = event->motion.x;
    int mouseY = event->motion.y;

    state->lastMouseX = mouseX;
    state->lastMouseY = mouseY;
    state->mouseHasMovedSinceClick = true;
    state->selecting = true;


    int paneTop = pane->y;
    int paneBottom = pane->y + pane->h;
    bool backInside = (mouseY >= paneTop && mouseY <= paneBottom);



    if (state->draggingOutsidePane) {
        handleCommandMouseDragOutside(pane, buffer, state, mouseY, paneTop, paneBottom, backInside);
    } else {
        handleCommandMouseDragInPane(pane, buffer, state, mouseX, mouseY);
    }
}


bool handleEditorScrollbarDrag(SDL_Event* event) {
    if (!isEditorDraggingScrollbar()) return false;

    UIPane* draggingPane = getDraggingEditorPane();
    EditorView* draggingView = getDraggingEditorView();
    if (draggingPane && draggingView) {
        handleEditorScrollbarEvent(draggingPane, event);
        if (event->type == SDL_MOUSEBUTTONUP)
            endEditorScrollbarDrag();
    }
    return true;
}



//              DRAG
// ============================================
//              Click & Release



static void getClickedEditorPosition(int mouseX, int mouseY, EditorView* view,
                                     EditorBuffer* buffer, EditorState* state,
                                     int* outRow, int* outCol) {
    const int lineHeight = 20;
    const int paddingLeft = 8;
    const int textX = view->x + paddingLeft;
    const int textY = view->y + HEADER_HEIGHT + 2 + state->verticalPadding;

    // Early out if buffer has no lines
    if (buffer->lineCount <= 0 || buffer->lines == NULL) {
        *outRow = 0;
        *outCol = 0;
        return;
    }

    // Compute row under cursor
    int row = state->viewTopRow + (mouseY - textY) / lineHeight;
    if (row < 0) row = 0;
    if (row >= buffer->lineCount) row = buffer->lineCount - 1;

    const char* line = buffer->lines[row];
    if (!line) line = "";

    int col = 0;
    int accumulatedX = textX;

    while (line[col]) {
        int charWidth = getTextWidthN(line, col + 1) - getTextWidthN(line, col);
        if (accumulatedX + charWidth / 2 >= mouseX) break;
        accumulatedX += charWidth;
        col++;
    }

    *outRow = row;
    *outCol = col;
}


static void handleCommandEditorMouseClick(EditorState* state, int row, int col) {
    state->cursorRow = row;
    state->cursorCol = col;
    state->selStartRow = row;
    state->selStartCol = col;
    state->selecting = true;
    state->draggingWithMouse = true;
    state->mouseHasMovedSinceClick = false;
}


static bool handleCommandClickOnScrollbar(int mouseX, int mouseY, SDL_Event* event) {
    EditorView* view = getCoreState()->activeEditorView;
    if (!view) return false;

    EditorViewState* vs  = getCoreState()->editorViewState;
    for (int i = 0; i < vs->scrollThumbHitboxCount; i++) {   
        ScrollThumbHitbox* hit = &vs->scrollThumbHitboxes[i];
        if (SDL_PointInRect(&(SDL_Point){mouseX, mouseY}, &hit->rect)) {
            return handleEditorScrollbarEvent(hit->pane, event);
        }
    }
    return false;
}


static bool handleCommandClickOnTabBar(EditorView* view, int mouseX, int mouseY) {
    EditorViewState* vs  = getCoreState()->editorViewState;
    for (int i = 0; i < vs->viewTabHitboxCount; i++) {
        if (vs->viewTabHitboxes[i].view != view) continue;
        
        SDL_Rect tabRect = vs->viewTabHitboxes[i].rect;
        if (SDL_PointInRect(&(SDL_Point){mouseX, mouseY}, &tabRect)) {
            int tabIndex = vs->viewTabHitboxes[i].tabIndex;
            if (tabIndex >= 0 && tabIndex < view->fileCount) {
                view->activeTab = tabIndex;
                setActiveEditorView(view);
                printf("[Tab] Switched to tab %d in view %p\n", tabIndex, (void*)view);
            }
            return true;
        }
    }
    return false;
}


bool handleEditorScrollbarThumbClick(SDL_Event* event, int mx, int my) {
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT)
        return false;

    EditorView* view = getCoreState()->activeEditorView;
    if (!view) return false;

    EditorViewState* vs = getCoreState()->editorViewState;
    SDL_Point mousePoint = { mx, my };

    for (int i = 0; i < vs->scrollThumbHitboxCount; i++) {
        if (SDL_PointInRect(&mousePoint, &vs->scrollThumbHitboxes[i].rect)) {
            UIPane* thumbPane = vs->scrollThumbHitboxes[i].pane;
            EditorView* thumbView = vs->scrollThumbHitboxes[i].view;

            if (thumbView && thumbPane && thumbPane->role == PANE_ROLE_EDITOR) {
                OpenFile* file = thumbView->openFiles[thumbView->activeTab];
                if (file && file->buffer) {
                    setActiveEditorView(thumbView);
                    beginEditorScrollbarDrag(thumbPane, thumbView);
                    file->state.scrollbarDragOffsetY = my - vs->scrollThumbHitboxes[i].rect.y;
                    return true;
                }
            }
        }
    }

    return false;
}



void handleEditorMouseClick(UIPane* pane, SDL_Event* event, EditorView* clickedView) {
    if (!pane || !pane->editorView || !clickedView || clickedView->type != VIEW_LEAF)
        return;

    printf("[MouseClick] Click in view: %p | fileCount = %d | activeTab = %d\n",
           (void*)clickedView, clickedView->fileCount, clickedView->activeTab);

    setActiveEditorView(clickedView);

    IDECoreState* core = getCoreState();
    printf("[ActiveView] Now set to: %p\n", (void*)core->activeEditorView);

/*    if (clickedView->fileCount <= 0) {
        printf("[MouseClick] View is empty. Click registered, no further action.\n");
        return;
    }
*/
    int mouseX = event->button.x;
    int mouseY = event->button.y;

    // === Check for click on close "X" button
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (clickedView->activeTab >= 0 && clickedView->activeTab < clickedView->fileCount) {
            OpenFile* file = clickedView->openFiles[clickedView->activeTab];
            if (file) {
                SDL_Rect xRect = file->state.closeButtonRect;
                if (mouseX >= xRect.x && mouseX <= xRect.x + xRect.w &&
                    mouseY >= xRect.y && mouseY <= xRect.y + xRect.h) {
                    closeTab(clickedView, clickedView->activeTab);
                    return; // Skip remaining logic if tab was closed
                }
            }
        }
    }

    if (handleCommandClickOnScrollbar(mouseX, mouseY, event)) return;
    if (handleCommandClickOnTabBar(clickedView, mouseX, mouseY)) return;

    OpenFile* file = clickedView->openFiles[clickedView->activeTab];
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;

    int clickedLine, clickedCol;
    getClickedEditorPosition(mouseX, mouseY, clickedView, buffer, state, &clickedLine, &clickedCol);

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
    	SDL_Keymod mod = SDL_GetModState();
    	bool shiftHeld = (mod & KMOD_SHIFT);
	
	    state->mouseHasMovedSinceClick = false;
	    state->draggingWithMouse = false;
	
    	if (!shiftHeld) {
    	    stopSelection(state);  // Reset selection unless Shift is held
    	}

    	handleCommandEditorMouseClick(state, clickedLine, clickedCol);
    }
}


void handleEditorMouseButtonUp(UIPane* pane, SDL_Event* event, EditorView* view){
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;
     
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;
    
    EditorState* state = &file->state;

    // Stop drag state
    state->draggingWithMouse = false;
    state->draggingOutsidePane = false;

    if (state->mouseHasMovedSinceClick) {
        state->selecting = true;
    }
    else if (state->selecting &&
             state->selStartRow == state->cursorRow &&
             state->selStartCol == state->cursorCol) {
        stopSelection(state);
    }

    // Always reset drag state
    state->mouseHasMovedSinceClick = false;
}



// =====================================
// 		SCROLL


static void handleCommandEditorScroll(EditorState* state, EditorBuffer* buffer, 
					EditorView* view, int scrollDelta) {
    const int lineHeight = 20;
    const int contentY = view->y + HEADER_HEIGHT + 2;
    const int contentH = view->h - (contentY - view->y);
    const int maxVisibleLines = (contentH > 0) ? contentH / lineHeight : 0;
    
    state->viewTopRow -= scrollDelta;
    if (state->viewTopRow < 0) state->viewTopRow = 0;
    
    int maxScroll = buffer->lineCount - maxVisibleLines;
    if (state->viewTopRow > maxScroll) state->viewTopRow = maxScroll;
    
    // Align cursor with view
    state->cursorRow -= scrollDelta;
    if (state->cursorRow < 0) state->cursorRow = 0;
    if (state->cursorRow >= buffer->lineCount) state->cursorRow = buffer->lineCount - 1;
}

bool handleEditorScrollWheel(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEWHEEL)
        return false;
        
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0)
        return false;
        
    if (view->activeTab < 0 || view->activeTab >= view->fileCount)
        return false;
        
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer)
        return false;
        
    EditorState* state = &file->state;
    EditorBuffer* buffer = file->buffer;
    
    int scrollDelta = event->wheel.y;
    
    handleCommandEditorScroll(state, buffer, view, scrollDelta);
    return true;
}



//              SCROLL
// =====================================
//              SCROLL BAR




static bool handleCommandEditorScrollDrag(EditorState* state, EditorBuffer* buffer, int mouseY,
                                          int scrollbarY, int scrollbarH, int thumbH, int 
maxVisibleLines) {
    if (mouseY == state->scrollbarStartY) {
        return true;  // No movement yet   
    }
     
    int maxScroll = buffer->lineCount - maxVisibleLines;
    float maxThumbTravel = (float)(scrollbarH - thumbH);
    
    int thumbTop = mouseY - state->scrollbarDragOffsetY;
    thumbTop = clamp(thumbTop, scrollbarY, scrollbarY + scrollbarH - thumbH);
    float thumbProgress = (float)(thumbTop - scrollbarY) / maxThumbTravel;   
    
    int newTopRow = (int)(thumbProgress * maxScroll);
    newTopRow = clamp(newTopRow, -1, maxScroll);
    
    if (newTopRow != state->viewTopRow) {
        printf("[SCROLL] viewTopRow updated: %d → %d\n", state->viewTopRow, newTopRow);
    }
     
    state->viewTopRow = newTopRow;
    state->cursorRow = clamp(state->cursorRow, state->viewTopRow,
                             state->viewTopRow + maxVisibleLines - 1);
                             
    return true;             
}
 
static bool handleCommandEditorScrollRelease(void) {
    endEditorScrollbarDrag();
    return true;
}



bool handleEditorScrollbarEvent(UIPane* pane, SDL_Event* event) {
    if (!pane || !pane->editorView) return false;
    
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    
    EditorView* view = findLeafUnderCursor(pane->editorView, mouseX, mouseY);
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return false;
    
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return false;
    
    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;  
    
    const int lineHeight = 20;
    
    // 🔄 Use the actual view dimensions, not the full pane
    const int contentY = view->y;
    const int contentH = view->h;
    
    if (contentH <= 0) return false;
    
    const int maxVisibleLines = contentH / lineHeight;
    const int maxScroll = buffer->lineCount - maxVisibleLines;
    if (maxScroll <= 0) return false;
    
    const int scrollbarY = contentY;
    const int scrollbarH = contentH;
    
    int thumbH = (int)((float)maxVisibleLines / buffer->lineCount * scrollbarH);
    if (thumbH < 20) thumbH = 20;
    
    state->selecting = false;
    
    // === Drag Scroll Behavior ===
    if (event->type == SDL_MOUSEMOTION && getDraggingEditorPane() == pane) {
        return handleCommandEditorScrollDrag(state, buffer, event->motion.y,
                                             scrollbarY, scrollbarH, thumbH, maxVisibleLines);
    }
     
    // === End Drag Behavior ===
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        if (getDraggingEditorPane() == pane) {
            return handleCommandEditorScrollRelease();
        }
    }
     
    return false;
}
