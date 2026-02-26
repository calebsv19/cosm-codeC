#include "tree_renderer.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/ui_selection_style.h"
#include "fisics_frontend.h"

#include <stdlib.h>
#include <string.h>

static UITreeNode* hoveredNode = NULL;
static UITreeNode* selectedNode = NULL;
static UITreeNode* selectAllVisualRoot = NULL;
static const int TREE_INDENT_WIDTH = 10;

// Optional color overrides
static SDL_Color statusColors[8] = {
    { 226, 231, 238, 255 },  // DEFAULT
    { 245, 233, 206, 255 },  // MODIFIED (warm near-white)
    { 214, 241, 223, 255 },  // ADDED (green-tinted near-white)
    { 244, 214, 214, 255 },  // DELETED (red-tinted near-white)
    { 214, 223, 245, 255 },  // UNTRACKED (blue-tinted near-white)
    { 222, 228, 237, 255 },  // STAGED
    { 224, 226, 244, 255 }   // SECTION
};

static int tree_line_height_for_pane(const UIPane* pane) {
    if (pane && pane->role == PANE_ROLE_CONTROLPANEL) {
        return 18;
    }
    return 22;
}

static int control_panel_symbol_tier(const UITreeNode* node) {
    if (!node) return 1;

    // Top-level branches and directories share top tier.
    if (node->depth <= 2 || node->type == TREE_NODE_FOLDER) {
        return 0;
    }

    const FisicsSymbol* sym = (const FisicsSymbol*)node->userData;
    if (sym && sym->kind == FISICS_SYMBOL_FUNCTION) {
        // Functions and their params share the same dull tier.
        return 2;
    }

    // Files + non-function symbol rows.
    return 1;
}

static SDL_Color control_panel_symbol_tone_by_node(const UITreeNode* node) {
    int tier = control_panel_symbol_tier(node);
    if (tier == 0) {
        return (SDL_Color){242, 247, 252, 255}; // directories/top branches
    }
    if (tier == 2) {
        return (SDL_Color){132, 144, 162, 255}; // params
    }
    return (SDL_Color){186, 198, 214, 255};     // files + methods
}

void setTreeColorOverride(TreeNodeColor color, SDL_Color sdlColor) {
    if (color >= 0 && color < 8) {
        statusColors[color] = sdlColor;
    }
}

UITreeNode* getHoveredTreeNode(void) { return hoveredNode; }
UITreeNode* getSelectedTreeNode(void) { return selectedNode; }
void clearTreeSelectionState(void) {
    hoveredNode = NULL;
    selectedNode = NULL;
}

void setTreeSelectAllVisualRoot(UITreeNode* root) {
    selectAllVisualRoot = root;
}

void clearTreeSelectAllVisual(void) {
    selectAllVisualRoot = NULL;
}

bool tree_select_all_visual_active_for(const UITreeNode* root) {
    return root && root == selectAllVisualRoot;
}

void handleTreeMouseMove(int x, int y) {
    getCoreState()->mouseX = x;
    getCoreState()->mouseY = y;
}

static void renderTreeRecursive(UITreeNode* node,
                                int x,
                                int* y,
                                int maxY,
                                int mouseX,
                                int mouseY,
                                bool allowHover,
                                bool controlPanelTone,
                                bool selectAllVisual,
                                int lineHeight) {
    if (!node || *y > maxY) return;
    int indent = node->depth * TREE_INDENT_WIDTH;
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

    bool isHovered = allowHover && (mouseY >= drawY && mouseY < drawY + lineHeight);
    if (isHovered) {
        isHovered = (mouseX >= textBox.x && mouseX <= (textBox.x + textBox.w) &&
                     mouseY >= textBox.y && mouseY <= (textBox.y + textBox.h));
    }

    //  Draw hover outline
    if (isHovered) {
        hoveredNode = node;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); // light gray
        SDL_RenderDrawRect(renderer, &textBox);
    }

    bool selectedVisual = (node == selectedNode) || selectAllVisual;
    if (selectedVisual) {
        SDL_Color fill = ui_selection_fill_color();
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &textBox);
        if (!isHovered) {
            SDL_Color outline = ui_selection_outline_color();
            SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, outline.a);
            SDL_RenderDrawRect(renderer, &textBox);
        }
    }

    //  Set color by node->color enum
    TreeNodeColor color = node->color;
    SDL_Color col = statusColors[color >= 0 && color < 8 ? color : 0];
    if (controlPanelTone) {
        col = control_panel_symbol_tone_by_node(node);
    }
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
    drawTextUTF8WithFontColor(drawX, drawY, line, getActiveFont(), col, false);
    *y += lineHeight;

    // Recurse into children if expanded
    if ((node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION) && node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            renderTreeRecursive(node->children[i],
                                x,
                                y,
                                maxY,
                                mouseX,
                                mouseY,
                                allowHover,
                                controlPanelTone,
                                selectAllVisual,
                                lineHeight);
        }
    }
}


void renderTreePanel(UIPane* pane, UITreeNode* root) {
    if (!pane || !root) return;

    int lineHeight = tree_line_height_for_pane(pane);
    bool controlPanelTone = (pane->role == PANE_ROLE_CONTROLPANEL);
    int x = pane->x + 12;
    int y = pane->y + 30;
    int maxY = pane->y + pane->h;
    int mx = getCoreState()->mouseX;
    int my = getCoreState()->mouseY;
    bool allowHover = (mx >= pane->x && mx <= pane->x + pane->w &&
                       my >= pane->y && my <= pane->y + pane->h);
    bool selectAllVisual = tree_select_all_visual_active_for(root);

    hoveredNode = NULL;
    renderTreeRecursive(root,
                        x,
                        &y,
                        maxY,
                        mx,
                        my,
                        allowHover,
                        controlPanelTone,
                        selectAllVisual,
                        lineHeight);
}

void renderTreePanelWithScroll(UIPane* pane, UITreeNode* root,
                               PaneScrollState* scroll,
                               SDL_Rect* outTrack,
                               SDL_Rect* outThumb) {
    if (!pane || !root) return;
    if (!scroll) return;

    const int lineHeight = tree_line_height_for_pane(pane);
    const bool controlPanelTone = (pane->role == PANE_ROLE_CONTROLPANEL);
    int paddingX = 12;
    int paddingY = 30;
    int contentTop = pane->y + paddingY;
    int viewportH = pane->h - (contentTop - pane->y);
    if (viewportH < 0) viewportH = 0;

    scroll_state_set_viewport(scroll, (float)viewportH);

    // Count visible rows
    int visibleLines = 0;
    UITreeNode** stack = NULL;
    size_t sp = 0;
    size_t stackCap = 0;
    stackCap = 128;
    stack = (UITreeNode**)malloc(stackCap * sizeof(UITreeNode*));
    if (!stack) return;
    stack[sp++] = root;
    while (sp > 0) {
        UITreeNode* n = stack[--sp];
        if (!n) continue;
        visibleLines++;
        if ((n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) && n->isExpanded) {
            for (int i = n->childCount - 1; i >= 0; --i) {
                if (sp >= stackCap) {
                    size_t newCap = stackCap * 2;
                    UITreeNode** grown = (UITreeNode**)realloc(stack, newCap * sizeof(UITreeNode*));
                    if (!grown) {
                        free(stack);
                        return;
                    }
                    stack = grown;
                    stackCap = newCap;
                }
                stack[sp++] = n->children[i];
            }
        }
    }
    float contentHeight = (float)visibleLines * (float)lineHeight;
    float effectiveHeight = contentHeight;
    if (scroll->line_height_px > 0.0f && scroll->viewport_height_px > scroll->line_height_px) {
        float slack = scroll->viewport_height_px - scroll->line_height_px;
        effectiveHeight = contentHeight + slack;
    }
    scroll_state_set_content_height(scroll, effectiveHeight);
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
    if (stackCap == 0) {
        stackCap = 128;
        stack = (UITreeNode**)malloc(stackCap * sizeof(UITreeNode*));
        if (!stack) { popClipRect(); return; }
    }
    stack[sp++] = root;
    int mx = getCoreState()->mouseX;
    int my = getCoreState()->mouseY;
    bool mouseInsidePane = (mx >= pane->x && mx <= pane->x + pane->w &&
                            my >= contentTop && my <= maxY);
    bool selectAllVisual = tree_select_all_visual_active_for(root);
    while (sp > 0) {
        UITreeNode* n = stack[--sp];
        if (!n) continue;
        int drawY = y;
        y += lineHeight;
        if (drawY + lineHeight < contentTop) {
            // Skip draw, but continue traversal (need depth info)
        } else if (drawY <= maxY) {
            int indent = n->depth * TREE_INDENT_WIDTH;
            int drawX = x + indent;

            RenderContext* ctx = getRenderContext();
            if (!ctx || !ctx->renderer) {
                free(stack);
                popClipRect();
                return;
            }
            SDL_Renderer* renderer = ctx->renderer;

            const char* prefix = "";
            if (n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) {
                prefix = n->isExpanded ? "[-] " : "[+] ";
            }
            char line[512];
            snprintf(line, sizeof(line), "%s%s", prefix, n->label);

            int textWidth = getTextWidth(line);
            SDL_Rect textBox = { drawX - 6, drawY - 1, textWidth + 12, lineHeight };

            bool isHovered = mouseInsidePane &&
                             (my >= drawY && my < drawY + lineHeight) &&
                             (mx >= textBox.x && mx <= (textBox.x + textBox.w));

            if (isHovered) {
                hoveredNode = n;
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &textBox);
            }

            bool selectedVisual = (n == selectedNode) || selectAllVisual;
            if (selectedVisual) {
                SDL_Color fill = ui_selection_fill_color();
                SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
                SDL_RenderFillRect(renderer, &textBox);
                if (!isHovered) {
                    SDL_Color outline = ui_selection_outline_color();
                    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, outline.a);
                    SDL_RenderDrawRect(renderer, &textBox);
                }
            }

            TreeNodeColor color = n->color;
            SDL_Color col = statusColors[color >= 0 && color < 8 ? color : 0];
            if (controlPanelTone) {
                col = control_panel_symbol_tone_by_node(n);
            }
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
            drawTextUTF8WithFontColor(drawX, drawY, line, getActiveFont(), col, false);
        }

        if ((n->type == TREE_NODE_FOLDER || n->type == TREE_NODE_SECTION) && n->isExpanded) {
            for (int i = n->childCount - 1; i >= 0; --i) {
                if (sp >= stackCap) {
                    size_t newCap = stackCap * 2;
                    UITreeNode** grown = (UITreeNode**)realloc(stack, newCap * sizeof(UITreeNode*));
                    if (!grown) {
                        free(stack);
                        popClipRect();
                        return;
                    }
                    stack = grown;
                    stackCap = newCap;
                }
                stack[sp++] = n->children[i];
            }
        }
    }

    popClipRect();
    free(stack);

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
    clearTreeSelectAllVisual();

    // Only folders/sections can expand/collapse
    if (hoveredNode->type == TREE_NODE_FOLDER || hoveredNode->type == TREE_NODE_SECTION) {
        int indent = hoveredNode->depth * TREE_INDENT_WIDTH;
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

    const int lineHeight = tree_line_height_for_pane(pane);
    int paddingX = 12;
    int paddingY = 30;
    int contentTop = pane->y + paddingY;
    float offset = scroll_state_get_offset(scroll);

    // Walk visible nodes with the same layout as renderTreePanelWithScroll
    UITreeNode** stack = NULL;
    size_t sp = 0;
    size_t stackCap = 128;
    stack = (UITreeNode**)malloc(stackCap * sizeof(UITreeNode*));
    if (!stack) return;
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
                if (sp >= stackCap) {
                    size_t newCap = stackCap * 2;
                    UITreeNode** grown = (UITreeNode**)realloc(stack, newCap * sizeof(UITreeNode*));
                    if (!grown) {
                        free(stack);
                        return;
                    }
                    stack = grown;
                    stackCap = newCap;
                }
                stack[sp++] = n->children[i];
            }
        }
    }

    if (!hit) {
        free(stack);
        return;
    }

    selectedNode = hit;
    if (tree_select_all_visual_active_for(root)) {
        clearTreeSelectAllVisual();
    }

    if (hit->type == TREE_NODE_FOLDER || hit->type == TREE_NODE_SECTION) {
        int indent = hit->depth * TREE_INDENT_WIDTH;
        int drawX = pane->x + paddingX + indent;
        int prefixWidth = getTextWidth("[-] ") + 10;
        bool clickedPrefix = (clickX >= drawX && clickX <= drawX + prefixWidth);
        if (clickedPrefix) {
            hit->isExpanded = !hit->isExpanded;
        }
    }
    free(stack);
}
