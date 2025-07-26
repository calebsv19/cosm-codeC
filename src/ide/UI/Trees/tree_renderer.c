#include "tree_renderer.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/core_state.h"

#include <stdlib.h>
#include <string.h>

static UITreeNode* hoveredNode = NULL;
static UITreeNode* selectedNode = NULL;

// Optional color overrides
static SDL_Color statusColors[8] = {
    { 220, 220, 220, 255 },  // DEFAULT
    { 255, 215, 0,   255 },  // MODIFIED
    { 0,   255, 100, 255 },  // ADDED
    { 255, 80,  80,  255 },  // DELETED
    { 100, 100, 255, 255 },  // UNTRACKED
    { 180, 180, 180, 255 },  // STAGED
    { 160, 160, 255, 255 }   // SECTION
};

void setTreeColorOverride(TreeNodeColor color, SDL_Color sdlColor) {
    if (color >= 0 && color < 8) {
        statusColors[color] = sdlColor;
    }
}

UITreeNode* getHoveredTreeNode(void) { return hoveredNode; }
UITreeNode* getSelectedTreeNode(void) { return selectedNode; }

void handleTreeMouseMove(int x, int y) {
    getCoreState()->mouseX = x;
    getCoreState()->mouseY = y;
}

static void renderTreeRecursive(UITreeNode* node, int x, int* y, int maxY) {
    if (!node || *y > maxY) return;

    int lineHeight = 22;
    int indent = node->depth * 20;
    int drawX = x + indent;
    int drawY = *y;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    // Determine prefix ([-] or [+])
    const char* prefix = "";
    if (node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION) {
        prefix = node->isExpanded ? "[-] " : "[+] ";
    }

    char line[512];
    snprintf(line, sizeof(line), "%s%s", prefix, node->label);

    int textWidth = getTextWidth(line);
    SDL_Rect textBox = {
        drawX - 6,
        drawY - 1,
        textWidth + 12,
        lineHeight
    };

    //  Use global mouse position from core state
    int mx = getCoreState()->mouseX;
    int my = getCoreState()->mouseY;
    bool isHovered = (my >= drawY && my < drawY + lineHeight);

    //  Draw hover outline
    if (isHovered) {
        hoveredNode = node;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); // light gray
        SDL_RenderDrawRect(renderer, &textBox);
    }

    //  Draw selection outline if not hovered
    if (node == selectedNode && !isHovered) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180); // white box
        SDL_RenderDrawRect(renderer, &textBox);
    }

    //  Set color by node->color enum
    TreeNodeColor color = node->color;
    SDL_Color col = statusColors[color >= 0 && color < 8 ? color : 0];
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);

    drawText(drawX, drawY, line);
    *y += lineHeight;

    // Recurse into children if expanded
    if ((node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION) && node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            renderTreeRecursive(node->children[i], x, y, maxY);
        }
    }
}


void renderTreePanel(UIPane* pane, UITreeNode* root) {
    if (!pane || !root) return;

    int x = pane->x + 12;
    int y = pane->y + 30;
    int maxY = pane->y + pane->h;

    hoveredNode = NULL;
    renderTreeRecursive(root, x, &y, maxY);
}

void handleTreeClick(UIPane* pane, int clickX, int clickY) {
    if (!hoveredNode || !pane) return;

    selectedNode = hoveredNode;

    // Only folders/sections can expand/collapse
    if (hoveredNode->type == TREE_NODE_FOLDER || hoveredNode->type == TREE_NODE_SECTION) {
        int indent = hoveredNode->depth * 20;
        int drawX = pane->x + 12 + indent;
        int prefixWidth = getTextWidth("[-] ") + 10;
        bool clickedPrefix = (clickX >= drawX && clickX <= drawX + prefixWidth);
        if (clickedPrefix) {
            hoveredNode->isExpanded = !hoveredNode->isExpanded;
        }
    }
}

