#include "Render/render_pipeline.h"
#include "Render/render_helpers.h"              // renderUIPane, drawText
#include "Render/render_text_helpers.h"    

#include "Editor/Render/render_editor.h"
#include "GlobalInfo/system_control.h"
#include "GlobalInfo/core_state.h"

#include "PaneInfo/pane.h"
#include "Editor/editor_view.h"    // editorView, layout, render
#include "Editor/editor_core.h"

#include <SDL2/SDL.h>

void renderEditorViewContents(UIPane* pane, bool hovered, IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
//    SDL_Renderer* renderer = ctx->renderer;

// renderUIPane(pane, hovered, core);

    if (!pane->editorView) {
        drawText(pane->x + 8, pane->y + 30, "(empty editor)");
        return;
    }

    // Reset shared render counters (tab hitboxes, leaf regions, etc.)
    resetViewCounters();

    // Layout and render
    performEditorLayout(pane->editorView, pane->x, pane->y, pane->w, pane->h);
    renderEditorEntry(pane->editorView);  // uses core->activeEditorView internally
}


void renderEditorEntry(EditorView* view){
     viewTabHitboxCount = 0;
    
     if (!isEditorDraggingScrollbar()) {
           scrollThumbHitboxCount = 0;
     }
     renderEditorViewRecursive(view);
}
        
    
void renderEditorViewRecursive(EditorView* view) {
    if (!view) return;
        
    if (view->type == VIEW_SPLIT) {
        layoutSplitChildren(view);
        renderEditorViewRecursive(view->childA);
        renderEditorViewRecursive(view->childB);
    } else if (view->type == VIEW_LEAF) {
        renderLeafEditorView(view);
    }
}


void renderLeafEditorView(EditorView* view) {
            
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
         
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxH = view->h - 2 * EDITOR_PADDING;
        
    // Register clickable region for this leaf editor
    if (leafHitboxCount < MAX_LEAF_HITBOXES) {
        leafHitboxes[leafHitboxCount].rect = (SDL_Rect){ boxX, boxY, boxW, boxH };
        leafHitboxes[leafHitboxCount].view = view;
        leafHitboxCount++;
    }
     
    // Highlight active view
    if (view == getCoreState()->activeEditorView) {
        SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255); // Blue border
        SDL_Rect highlight = { boxX - 2, boxY - 2, boxW + 4, boxH + 4 };
        SDL_RenderDrawRect(renderer, &highlight);
    }
        
    // Draw editor outline
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
    SDL_Rect outline = { boxX, boxY, boxW, boxH };
    SDL_RenderDrawRect(renderer, &outline);


                
    // Draw tab bar  
    int tabX = boxX;
         
    for (int i = 0; i < view->fileCount; i++) {
        OpenFile* file = view->openFiles[i];
        if (!file || !file->filePath) continue;
     
        const char* label = getFileName(file->filePath);
        int tabW = getTextWidth(label) + 16;
        int tabH = HEADER_HEIGHT;

        SDL_Rect tabRect = { tabX, boxY, tabW, tabH };
    
        if (viewTabHitboxCount < MAX_TAB_HITBOXES) {
            viewTabHitboxes[viewTabHitboxCount].rect = tabRect;
            viewTabHitboxes[viewTabHitboxCount].tabIndex = i;
            viewTabHitboxes[viewTabHitboxCount].view = view;
            viewTabHitboxCount++;
        }   
            
        // Render tab background
        if (i == view->activeTab) {
            SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, HEADER_BG_R, HEADER_BG_G, HEADER_BG_B, 255);
        }
         
        SDL_RenderFillRect(renderer, &tabRect);
        drawText(tabX + 6, boxY + 2, label);   
        tabX += tabW + 4;
    }

    // === Render corner "X" tab close button ===
    int xButtonSize = 18;
    int xButtonX = boxX + boxW - xButtonSize - 4;
    int xButtonY = boxY + 4;

    SDL_Rect xButtonRect = { xButtonX, xButtonY, xButtonSize, xButtonSize };

    // Save into active file's editor state for input handling
    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* file = view->openFiles[view->activeTab];
        if (file) {
            file->state.closeButtonRect = xButtonRect;
        }
    }

    // Background
    SDL_SetRenderDrawColor(renderer, 100, 60, 60, 255);
    SDL_RenderFillRect(renderer, &xButtonRect);

    // Border
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &xButtonRect);
 
    // Draw "X" text
    drawText(xButtonX + 5, xButtonY + 2, "X");


     
    // Render buffer contents
    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* active = view->openFiles[view->activeTab];
        if (active && active->buffer) {
            setEditorVerticalPaddingIfUnset(&active->state, 30);
            
            renderEditorBuffer(
                active->buffer,
                &active->state,
                boxX,
                boxY + HEADER_HEIGHT,
                boxW,
                boxH - HEADER_HEIGHT);
        }
        renderEditorScrollbar(view, active);
    }           
}       



void renderEditorScrollbar(EditorView* view, OpenFile* file) {
    if (!view || !file || !file->buffer) return;
    
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    
    EditorState* state = &file->state;
    EditorBuffer* buffer = file->buffer;
    
    int totalLines = buffer->lineCount;
    int lineHeight = 20;
    
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxH = view->h - 2 * EDITOR_PADDING;
    
    int contentHeight = boxH - HEADER_HEIGHT;
    int visibleLines = contentHeight / lineHeight;
    
    if (totalLines <= visibleLines) return;
    
    int maxScroll = totalLines - visibleLines;
    float scrollYRatio = (float)state->viewTopRow / (float)(maxScroll > 0 ? maxScroll : 1);
    float visibleRatio = (float)visibleLines / (float)totalLines;
    
    // Clamp scrollYRatio
    if (scrollYRatio < 0.0f) scrollYRatio = 0.0f;
    if (scrollYRatio > 1.0f) scrollYRatio = 1.0f;
    
    int scrollbarWidth = 16;
    int scrollbarX = boxX + boxW - scrollbarWidth - 2;
    
    int scrollThumbHeight = (int)(visibleRatio * contentHeight);
    if (scrollThumbHeight < 20) scrollThumbHeight = 20;
    
    int scrollThumbY = boxY + HEADER_HEIGHT +
                                (int)(scrollYRatio * (contentHeight - scrollThumbHeight));
                                
    SDL_Rect scrollbarTrack = { 
        scrollbarX, boxY + HEADER_HEIGHT,
        scrollbarWidth, contentHeight
    };

    SDL_Rect scrollbarThumb = {
        scrollbarX, scrollThumbY,
        scrollbarWidth, scrollThumbHeight
    };
      
    // Only add thumb hotbox if NOT currently dragging
    if (!isEditorDraggingScrollbar()) {
        addScrollThumbHitbox(scrollbarThumb, view, view->parentPane);
    } else {
        // Optional: Update dragging thumb rect position if needed during drag
        // e.g. updateScrollThumbPosition(view, scrollbarThumb);
    }
     
    // Render track background
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 100);
    SDL_RenderFillRect(renderer, &scrollbarTrack);
    
    // Render thumb
    SDL_SetRenderDrawColor(renderer, 160, 160, 160, 180);
    SDL_RenderFillRect(renderer, &scrollbarThumb);
}










void renderEditorBuffer(EditorBuffer* buffer, EditorState* state,
                        int x, int y, int w, int h){
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
            
         
    const int lineHeight = 20;
    const int textX = x + 8;
    const int startY = y + state->verticalPadding;  // <-- Changed here
    const int contentHeight = h - (startY - y);
    const int maxVisibleLines = (contentHeight > 0) ? contentHeight / lineHeight : 0;

    // Scroll bounding
    if (state->cursorRow < state->viewTopRow) {
        state->viewTopRow = state->cursorRow; 
    } else if (state->cursorRow >= state->viewTopRow + maxVisibleLines) {
        state->viewTopRow = state->cursorRow - maxVisibleLines + 1;
    }

    if (state->viewTopRow < 0) state->viewTopRow = 0;
    if (buffer->lineCount < maxVisibleLines) state->viewTopRow = 0;
    if (state->viewTopRow > buffer->lineCount - maxVisibleLines)
        state->viewTopRow = buffer->lineCount - maxVisibleLines;
    if (state->viewTopRow < 0) state->viewTopRow = 0;

    // Draw visible lines
    for (int i = 0; i < maxVisibleLines; i++) {
        int bufferLineIndex = state->viewTopRow + i;
        if (bufferLineIndex >= buffer->lineCount) break;

        const char* line = buffer->lines[bufferLineIndex];
        int yLine = startY + i * lineHeight;  
        int maxWidth = w - 16;
    
        int selStart, selEnd;
        if (isLineInSelection(bufferLineIndex, &selStart, &selEnd, buffer, state)) {
            int selXStart = textX + getTextWidthN(line, selStart);
            int selXEnd = textX + getTextWidthN(line, selEnd);
            SDL_Rect highlight = { selXStart, yLine, selXEnd - selXStart, lineHeight };
            SDL_SetRenderDrawColor(renderer, 80, 120, 200, 100);
            SDL_RenderFillRect(renderer, &highlight);
        }
        
        drawClippedText(textX, yLine, line, maxWidth);
         
        if (bufferLineIndex == state->cursorRow) {
            int cursorX = textX + getTextWidthN(line, state->cursorCol);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, cursorX, yLine, cursorX, yLine + lineHeight);
        }
    }
}
