#include "editor_input_mouse.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "app/GlobalInfo/core_state.h"

#define CURSOR_EDGE_BIAS 2

static SDL_Rect compute_tab_header_viewport(const EditorView* view) {
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int closeReserve = EDITOR_TAB_CLOSE_BTN_SIZE + EDITOR_TAB_CLOSE_BTN_MARGIN + 6;

    SDL_Rect viewport = {
        boxX + 1,
        boxY + 1,
        boxW - closeReserve - 2,
        HEADER_HEIGHT - 2
    };
    if (viewport.w < 0) viewport.w = 0;
    if (viewport.h < 0) viewport.h = 0;
    return viewport;
}

static SDL_Rect compute_tab_close_button_rect(const EditorView* view) {
    int boxX = view->x + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;

    int xButtonSize = EDITOR_TAB_CLOSE_BTN_SIZE;
    int xButtonX = boxX + boxW - xButtonSize - EDITOR_TAB_CLOSE_BTN_MARGIN;
    int xButtonY = boxY + 4;
    return (SDL_Rect){ xButtonX, xButtonY, xButtonSize, xButtonSize };
}

static bool editor_input_projection_active(const OpenFile* file) {
    return file &&
           editor_file_projection_active(file) &&
           file->projection.lines &&
           file->projection.lineCount > 0;
}

static int editor_input_render_line_count(const OpenFile* file, const EditorBuffer* buffer) {
    if (editor_input_projection_active(file)) {
        return file->projection.lineCount;
    }
    if (!buffer) return 0;
    return buffer->lineCount;
}

static const char* editor_input_render_line_at(const OpenFile* file,
                                               const EditorBuffer* buffer,
                                               int row) {
    if (row < 0) return "";
    if (editor_input_projection_active(file)) {
        if (row >= file->projection.lineCount) return "";
        return file->projection.lines[row] ? file->projection.lines[row] : "";
    }
    if (!buffer || row >= buffer->lineCount) return "";
    return buffer->lines[row] ? buffer->lines[row] : "";
}


bool resetCursorPositionToMouse(UIPane* pane, int mouseX, int mouseY,
                                 EditorBuffer* buffer, EditorState* state) {
    const int lineHeight = EDITOR_LINE_HEIGHT;
    const int textX = pane->x + EDITOR_LINE_NUMBER_GUTTER_W + 6;
    const int textY = pane->y + HEADER_HEIGHT + 2 + state->verticalPadding;
    TTF_Font* textFont = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (!textFont) textFont = getActiveFont();

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
    if (mouseX < textX) mouseX = textX;

    int leftEdge = textX;

    while (line[newCol]) {
        int rightEdge = getTextWidthNWithFont(line, newCol + 1, textFont) + textX;
        int charWidth = rightEdge - leftEdge;
        if (charWidth <= 0) charWidth = 1;

        if (mouseX < rightEdge) {
            int leftDist = mouseX - leftEdge;
            int rightDist = rightEdge - mouseX;
            if (leftDist > rightDist + CURSOR_EDGE_BIAS) {
                newCol++;
            }
            break;
        }

        leftEdge = rightEdge;
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
    if (!view || view->type != VIEW_LEAF) {
        view = getHoveredEditorView();
    }

    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = getActiveOpenFile(view);
    if (!file || !file->buffer) return;
    if (editor_input_projection_active(file)) return;

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

bool handleEditorSplitDividerInteraction(SDL_Event* event, int mx, int my) {
    if (!event) return false;

    EditorViewState* vs = getCoreState()->editorViewState;
    if (!vs) return false;

    if (isEditorDraggingSplitDivider()) {
        EditorView* split = getDraggingSplitEditorView();
        if (!split || split->type != VIEW_SPLIT) {
            endEditorSplitDividerDrag();
            return false;
        }

        if (event->type == SDL_MOUSEMOTION) {
            int axisTotal = (split->splitType == SPLIT_VERTICAL) ? split->w : split->h;
            int axisStart = (split->splitType == SPLIT_VERTICAL) ? split->x : split->y;
            int axisMouse = (split->splitType == SPLIT_VERTICAL) ? event->motion.x : event->motion.y;
            int available = axisTotal - EDITOR_SPLIT_GAP;
            if (available > 0) {
                float ratio = (float)(axisMouse - axisStart) / (float)available;
                float minRatio = (split->splitType == SPLIT_VERTICAL)
                                     ? ((float)EDITOR_SPLIT_MIN_CHILD_W / (float)available)
                                     : ((float)EDITOR_SPLIT_MIN_CHILD_H / (float)available);
                if (minRatio >= 0.5f) {
                    ratio = 0.5f;
                } else {
                    if (ratio < minRatio) ratio = minRatio;
                    if (ratio > (1.0f - minRatio)) ratio = 1.0f - minRatio;
                }
                split->splitRatio = ratio;
            }
            return true;
        }

        if (event->type == SDL_MOUSEBUTTONUP) {
            endEditorSplitDividerDrag();
            return true;
        }

        return true;
    }

    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }

    EditorView* split = hitTestSplitDivider(vs, mx, my);
    if (!split) return false;

    beginEditorSplitDividerDrag(split, mx, my);
    return true;
}



//              DRAG
// ============================================
//              Click & Release



static void getClickedEditorPosition(int mouseX, int mouseY, EditorView* view,
                                     OpenFile* file, EditorBuffer* buffer, EditorState* state,
                                     int* outRow, int* outCol) {
    const int lineHeight = EDITOR_LINE_HEIGHT;
    const int textX = view->x + EDITOR_LINE_NUMBER_GUTTER_W + 6;
    const int textY = view->y + HEADER_HEIGHT + 2 + state->verticalPadding;
    TTF_Font* textFont = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (!textFont) textFont = getActiveFont();

    int totalLines = editor_input_render_line_count(file, buffer);
    if (totalLines <= 0) {
        *outRow = 0;
        *outCol = 0;
        return;
    }

    // Compute row under cursor
    int row = state->viewTopRow + (mouseY - textY) / lineHeight;
    if (row < 0) row = 0;
    if (row >= totalLines) row = totalLines - 1;

    const char* line = editor_input_render_line_at(file, buffer, row);

    int col = 0;
    if (mouseX < textX) mouseX = textX;

    int leftEdge = textX;

    while (line[col]) {
        int rightEdge = getTextWidthNWithFont(line, col + 1, textFont) + textX;
        int charWidth = rightEdge - leftEdge;
        if (charWidth <= 0) charWidth = 1;

        if (mouseX < rightEdge) {
            int leftDist = mouseX - leftEdge;
            int rightDist = rightEdge - mouseX;
            if (rightDist < leftDist - 10) {
                col++;
            }
            break;
        }

        leftEdge = rightEdge;
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
                if (thumbView->activeTab < 0 || thumbView->activeTab >= thumbView->fileCount)
                    continue;
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
    if (!pane || !pane->editorView)
        return;

    if (!clickedView || clickedView->type != VIEW_LEAF) {
        clickedView = getHoveredEditorView();
        if (!clickedView || clickedView->type != VIEW_LEAF) return;
    }

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
        SDL_Rect xRect = compute_tab_close_button_rect(clickedView);
        if (mouseX >= xRect.x && mouseX <= xRect.x + xRect.w &&
            mouseY >= xRect.y && mouseY <= xRect.y + xRect.h) {
            if (clickedView->activeTab >= 0 && clickedView->activeTab < clickedView->fileCount) {
                closeTab(clickedView, clickedView->activeTab);
            } else if (clickedView->fileCount == 0) {
                closeEmptyEditorLeaf(getCoreState()->persistentEditorView, clickedView);
            }
            return;
        }
    }

    if (handleCommandClickOnScrollbar(mouseX, mouseY, event)) return;
    if (handleCommandClickOnTabBar(clickedView, mouseX, mouseY)) return;

    OpenFile* file = getActiveOpenFile(clickedView);
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;
    bool projectionMode = editor_input_projection_active(file);

    int clickedLine, clickedCol;
    getClickedEditorPosition(mouseX, mouseY, clickedView, file, buffer, state, &clickedLine, &clickedCol);

    if (projectionMode) {
        int sourceRow = 0;
        int sourceCol = 0;
        bool mapped = false;
        if (editor_projection_map_row_to_source(file, clickedLine, &sourceRow, &sourceCol)) {
            mapped = true;
            if (buffer && sourceRow >= 0 && sourceRow < buffer->lineCount) {
                int lineLen = buffer->lines[sourceRow] ? (int)strlen(buffer->lines[sourceRow]) : 0;
                if (sourceCol > lineLen) sourceCol = lineLen;
                if (sourceCol < 0) sourceCol = 0;
            } else {
                sourceRow = 0;
                sourceCol = 0;
            }
            state->cursorRow = sourceRow;
            state->cursorCol = sourceCol;
        }
        bool isDoubleClick = (event->type == SDL_MOUSEBUTTONDOWN &&
                              event->button.button == SDL_BUTTON_LEFT &&
                              event->button.clicks >= 2);
        if (isDoubleClick && mapped) {
            control_panel_set_search_enabled(false);
            state->viewTopRow = (sourceRow > 2) ? sourceRow - 2 : 0;
        }
        state->selecting = false;
        state->draggingWithMouse = false;
        state->mouseHasMovedSinceClick = false;
        return;
    }

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
    if (!view || view->type != VIEW_LEAF) {
        view = getHoveredEditorView();
    }

    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = getActiveOpenFile(view);
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
                                      OpenFile* file, EditorView* view, int scrollDelta) {
    const int lineHeight = EDITOR_LINE_HEIGHT;
    const int contentY = view->y + HEADER_HEIGHT + 2;
    const int contentH = view->h - (contentY - view->y);
    const int maxVisibleLines = (contentH > 0) ? contentH / lineHeight : 0;
    
    state->viewTopRow -= scrollDelta;
    if (state->viewTopRow < 0) state->viewTopRow = 0;
    
    int totalLines = editor_input_render_line_count(file, buffer);
    int maxScroll = totalLines - maxVisibleLines;
    if (state->viewTopRow > maxScroll) state->viewTopRow = maxScroll;
    
    if (!editor_input_projection_active(file)) {
        // Align cursor with view only in real text mode.
        state->cursorRow -= scrollDelta;
        if (state->cursorRow < 0) state->cursorRow = 0;
        if (buffer && state->cursorRow >= buffer->lineCount) state->cursorRow = buffer->lineCount - 1;
    }
}

bool handleEditorScrollWheel(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEWHEEL)
        return false;
        
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->type != VIEW_LEAF) {
        view = getHoveredEditorView();
    }

    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0)
        return false;

    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    SDL_Point mousePoint = { mx, my };
    SDL_Rect tabViewport = compute_tab_header_viewport(view);
    if (tabViewport.w > 0 && tabViewport.h > 0 && SDL_PointInRect(&mousePoint, &tabViewport)) {
        view->tabScrollX -= event->wheel.y * EDITOR_TAB_SCROLL_STEP;
        if (view->tabScrollX < 0) view->tabScrollX = 0;
        return true;
    }
        
    OpenFile* file = getActiveOpenFile(view);
    if (!file || !file->buffer)
        return false;
        
    EditorState* state = &file->state;
    EditorBuffer* buffer = file->buffer;
    
    int scrollDelta = event->wheel.y;
    
    handleCommandEditorScroll(state, buffer, file, view, scrollDelta);
    return true;
}



//              SCROLL
// =====================================
//              SCROLL BAR




static bool handleCommandEditorScrollDrag(EditorState* state, int totalLines, int mouseY,
                                          int scrollbarY, int scrollbarH, int thumbH,
                                          int maxVisibleLines) {
    if (mouseY == state->scrollbarStartY) {
        return true;  // No movement yet   
    }
     
    int maxScroll = totalLines - maxVisibleLines;
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
    
    const int lineHeight = EDITOR_LINE_HEIGHT;
    
    // 🔄 Use the actual view dimensions, not the full pane
    const int contentY = view->y;
    const int contentH = view->h;
    
    if (contentH <= 0) return false;
    
    const int maxVisibleLines = contentH / lineHeight;
    const int totalLines = editor_input_render_line_count(file, buffer);
    const int maxScroll = totalLines - maxVisibleLines;
    if (maxScroll <= 0) return false;
    
    const int scrollbarY = contentY;
    const int scrollbarH = contentH;
    
    int thumbH = (int)((float)maxVisibleLines / (float)totalLines * scrollbarH);
    if (thumbH < 20) thumbH = 20;
    
    state->selecting = false;
    
    // === Drag Scroll Behavior ===
    if (event->type == SDL_MOUSEMOTION && getDraggingEditorPane() == pane) {
        return handleCommandEditorScrollDrag(state, totalLines, event->motion.y,
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
