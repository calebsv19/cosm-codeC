#include "render_tool_tasks.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"

#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>




static void renderTaskTreeRecursive(TaskNode* node, int x, int* y, int maxY) {
    if (!node) return;


    if (*y > maxY) return;

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    int lineHeight = TASK_LINE_HEIGHT;
    int indent = node->depth * TASK_INDENT_WIDTH;
    int drawX = x + indent;
    int drawY = *y;

    // Fixed UI segment widths
    const int expandWidth  = 28;
    const int checkboxWidth = 28;
    const int spacing = 6;

    bool hasExpandIcon = (node->isGroup || node->childCount > 0);
    int usedExpandWidth = (hasExpandIcon && includeNestLabel) ? expandWidth : 0;

    int labelWidth = getTextWidth(node->label);
    int totalWidth = usedExpandWidth + checkboxWidth + labelWidth + spacing * 2;

    SDL_Rect box = {
    	drawX + expandWidth - usedExpandWidth - spacing,
    	drawY - 1,
    	totalWidth,
    	lineHeight
    };

    if (node->isHovered) {
   	 SDL_SetRenderDrawColor(renderer, 60, 60, 80, 50);
   	 SDL_RenderDrawRect(renderer, &box);
    }
    if (node->isSelected) {
    	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
    	SDL_RenderDrawRect(renderer, &box);
    }


    // Draw expand [+]/[-]
    if (node->isGroup || node->childCount > 0) {
        drawText(drawX, drawY, node->isExpanded ? "[-]" : "[+]");
    }

    // Draw checkbox
    int checkX = drawX + expandWidth;
    drawText(checkX, drawY, node->completed ? "[x]" : "[ ]");

    // Draw label or editable text field
    int labelX = checkX + checkboxWidth;

    if (node == editingTask) {
	    drawText(labelX, drawY, taskEditingBuffer);
	    // Optional: add cursor or box
    } else {
	    drawText(labelX, drawY, node->label);
    }


    *y += lineHeight;

    // Recurse into children if expanded
    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            renderTaskTreeRecursive(node->children[i], x, y, maxY);
        }
    }
}


void renderTasksPanel(UIPane* pane) {
    int x = pane->x + TASK_PANEL_LEFT_PADDING;
    int y = pane->y + TASK_PANEL_TOP_PADDING;
    int maxY = pane->y + pane->h;

    const int iconBtnSize = 24;

    // --- [+] Add Task Button ---
    SDL_Rect addBtn = { x, y, iconBtnSize, iconBtnSize };
    renderButton(addBtn, "+");
    drawText(addBtn.x + iconBtnSize + 6, y + 4, "Add Task");

    y += iconBtnSize + TASK_BUTTON_SPACING;

    // --- [-] Remove Task Button ---
    SDL_Rect removeBtn = { x, y, iconBtnSize, iconBtnSize };
    renderButton(removeBtn, "-");
    drawText(removeBtn.x + iconBtnSize + 6, y + 4, "Remove Task");

    y += iconBtnSize + TASK_BUTTON_SPACING;

    for (int i = 0; i < taskRootCount; i++) {
        renderTaskTreeRecursive(taskRoots[i], x, &y, maxY);
    }
}

