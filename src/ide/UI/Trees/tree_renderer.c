#include "tree_renderer.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/UI/scroll_manager.h"

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

void renderTreePanelWithScroll(UIPane* pane, UITreeNode* root,
                               PaneScrollState* scroll,
                               SDL_Rect* outTrack,
                               SDL_Rect* outThumb) {
    if (!pane || !root) return;
    if (!scroll) return;

    const int lineHeight = 22;
    int paddingX = 12;
    int paddingY = 30;
    int contentTop = pane->y + paddingY;
    int viewportH = pane->h - (contentTop - pane->y);
    if (viewportH < 0) viewportH = 0;

    scroll_state_set_viewport(scroll, (float)viewportH);

    // Count visible rows
    int visibleLines = 0;
    // quick DFS count
    UITreeNode* stack[1024];
    int sp = 0;
    stack[sp++] = root;
    while (sp > 0) {
        UITreeNode* n = stack[--sp];
        if (!n) continue;
        visibleLines++;
        if ((n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) && n->isExpanded) {
            for (int i = n->childCount - 1; i >= 0; --i) {
                stack[sp++] = n->children[i];
            }
        }
    }
    float contentHeight = (float)visibleLines * (float)lineHeight;
    scroll_state_set_content_height(scroll, contentHeight);
    float offset = scroll_state_get_offset(scroll);

    SDL_Rect clip = { pane->x, contentTop, pane->w - 8, viewportH };
    pushClipRect(&clip);

    int x = pane->x + paddingX;
    int y = contentTop - (int)offset;
    int maxY = contentTop + viewportH;

    hoveredNode = NULL;
    // We reuse the existing recursive renderer but adjust y/maxY and skip off-screen
    // So we inline a trimmed version here:
    // We'll perform a stack walk similar to above but drawing with offset.
    sp = 0;
    stack[sp++] = root;
    while (sp > 0) {
        UITreeNode* n = stack[--sp];
        if (!n) continue;
        int drawY = y;
        y += lineHeight;
        if (drawY + lineHeight < contentTop) {
            // Skip draw, but continue traversal (need depth info)
        } else if (drawY <= maxY) {
            int indent = n->depth * 20;
            int drawX = x + indent;

            RenderContext* ctx = getRenderContext();
            if (!ctx || !ctx->renderer) { popClipRect(); return; }
            SDL_Renderer* renderer = ctx->renderer;

            const char* prefix = "";
            if (n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) {
                prefix = n->isExpanded ? "[-] " : "[+] ";
            }
            char line[512];
            snprintf(line, sizeof(line), "%s%s", prefix, n->label);

            int textWidth = getTextWidth(line);
            SDL_Rect textBox = { drawX - 6, drawY - 1, textWidth + 12, lineHeight };

            int my = getCoreState()->mouseY;
            bool isHovered = (my >= drawY && my < drawY + lineHeight);

            if (isHovered) {
                hoveredNode = n;
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &textBox);
            }

            if (n == selectedNode && !isHovered) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
                SDL_RenderDrawRect(renderer, &textBox);
            }

            TreeNodeColor color = n->color;
            SDL_Color col = statusColors[color >= 0 && color < 8 ? color : 0];
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
            drawText(drawX, drawY, line);
        }

        if ((n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) && n->isExpanded) {
            for (int i = n->childCount - 1; i >= 0; --i) {
                stack[sp++] = n->children[i];
            }
        }
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(scroll) && viewportH > 0;
    if (showScrollbar) {
        SDL_Rect track = {
            pane->x + pane->w - 8,
            contentTop,
            4,
            viewportH
        };
        SDL_Rect thumb = scroll_state_thumb_rect(scroll,
                                                 track.x,
                                                 track.y,
                                                 track.w,
                                                 track.h);
        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(getRenderContext()->renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &track);
        SDL_SetRenderDrawColor(getRenderContext()->renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &thumb);

        if (outTrack) *outTrack = track;
        if (outThumb) *outThumb = thumb;
    } else {
        if (outTrack) *outTrack = (SDL_Rect){0};
        if (outThumb) *outThumb = (SDL_Rect){0};
    }
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

void handleTreeClickWithScroll(UIPane* pane, UITreeNode* root, PaneScrollState* scroll, int clickX, int clickY) {
    if (!pane || !root || !scroll) return;

    const int lineHeight = 22;
    int paddingX = 12;
    int paddingY = 30;
    int contentTop = pane->y + paddingY;
    float offset = scroll_state_get_offset(scroll);

    // Walk visible nodes with the same layout as renderTreePanelWithScroll
    UITreeNode* stack[1024];
    int sp = 0;
    stack[sp++] = root;
    int y = contentTop - (int)offset;

    UITreeNode* hit = NULL;
    while (sp > 0) {
        UITreeNode* n = stack[--sp];
        if (!n) continue;
        int drawY = y;
        y += lineHeight;

        if (clickY >= drawY && clickY < drawY + lineHeight) {
            hit = n;
            break;
        }

        if ((n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) && n->isExpanded) {
            for (int i = n->childCount - 1; i >= 0; --i) {
                stack[sp++] = n->children[i];
            }
        }
    }

    if (!hit) return;

    selectedNode = hit;

    if (hit->type == TREE_NODE_FOLDER || hit->type == TREE_NODE_SECTION) {
        int indent = hit->depth * 20;
        int drawX = pane->x + paddingX + indent;
        int prefixWidth = getTextWidth("[-] ") + 10;
        bool clickedPrefix = (clickX >= drawX && clickX <= drawX + prefixWidth);
        if (clickedPrefix) {
            hit->isExpanded = !hit->isExpanded;
        }
    }
}
