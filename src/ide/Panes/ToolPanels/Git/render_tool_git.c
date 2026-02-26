#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Git/tree_git_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/scroll_manager.h"

#include <SDL2/SDL.h>
#include <string.h>

extern int mouseX;
extern int mouseY;

typedef struct {
    int controlsY;
    int controlsHeight;
    int branchY;
    int statusY;
    int contentTop;
} GitPanelLayout;

static GitPanelLayout git_panel_layout(const UIPane* pane) {
    GitPanelLayout l = {0};
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    l.controlsY = pane->y + d.controls_top - 1;
    l.controlsHeight = d.button_h;

    const int metadataGap = d.row_gap;
    const int metadataLineGap = 12;
    l.branchY = l.controlsY + l.controlsHeight + metadataGap;
    l.statusY = l.branchY + metadataLineGap;
    l.contentTop = git_panel_content_top(pane);

    return l;
}

static UIPane git_tree_pane(const UIPane* pane, int firstRowY) {
    UIPane treePane = *pane;
    treePane.y = firstRowY - 30;
    treePane.h = (pane->y + pane->h) - treePane.y;
    if (treePane.h < 0) treePane.h = 0;
    return treePane;
}

// === Static Tree Cache ===
UITreeNode* gitTree = NULL;
bool needsRefresh = true;
PaneScrollState gitScroll;
bool gitScrollInit = false;
SDL_Rect gitScrollTrack = {0};
SDL_Rect gitScrollThumb = {0};

void resetGitTree(void) {
    if (tree_select_all_visual_active_for(gitTree)) {
        clearTreeSelectAllVisual();
    }
    if (gitTree) {
        freeGitTree(gitTree);
        gitTree = NULL;
    }
    needsRefresh = true; // mark for refresh
}

void renderGitPanel(UIPane* pane) {
    if (needsRefresh) {
        refreshGitStatus();  // Now runs before the tree is built
        refreshGitLog(20);
        resetGitTree();      // Clears any existing tree
        needsRefresh = false;
    }

    if (!gitTree) {
        gitTree = convertGitModelToTree();
    }

    if (!gitScrollInit) {
        scroll_state_init(&gitScroll, NULL);
        gitScrollInit = true;
    }

    RenderContext* rctx = getRenderContext();
    if (!rctx || !rctx->renderer) return;
    SDL_Renderer* renderer = rctx->renderer;

    GitPanelLayout layout = git_panel_layout(pane);
    tool_panel_render_split_background(renderer, pane, layout.contentTop, 14);

    ToolPanelControlRow row = tool_panel_control_row_with(pane, layout.controlsY, 10, 10, layout.controlsHeight, 8);
    SDL_Rect addAllBtn = tool_panel_row_take_left(&row, 70);
    SDL_Rect commitBtn = tool_panel_row_take_right(&row, 66);
    SDL_Rect msgBox = tool_panel_row_take_fill(&row, 48);

    git_panel_set_add_all_rect(addAllBtn);
    git_panel_set_commit_rect(commitBtn);
    git_panel_set_message_rect(msgBox);

    renderButton(pane, addAllBtn, "Add All");
    renderButton(pane, commitBtn, "Commit");

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20);
    SDL_RenderFillRect(renderer, &msgBox);
    if (git_panel_is_message_focused()) {
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
    }
    SDL_RenderDrawRect(renderer, &msgBox);

    TTF_Font* uiFont = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (!uiFont) uiFont = getActiveFont();
    SDL_Color commitTextColor = {238, 243, 252, 255};
    SDL_Color placeholderTextColor = {176, 186, 204, 255};

    const char* msg = git_panel_get_message();
    if (msg && msg[0]) {
        SDL_Rect msgClip = {
            msgBox.x + 4,
            msgBox.y + 1,
            msgBox.w - 8,
            msgBox.h - 2
        };
        if (msgClip.w < 0) msgClip.w = 0;
        if (msgClip.h < 0) msgClip.h = 0;
        drawTextUTF8WithFontColorClipped(msgBox.x + 6,
                                         msgBox.y + 3,
                                         msg,
                                         uiFont,
                                         commitTextColor,
                                         false,
                                         &msgClip);
    } else if (!git_panel_is_message_focused()) {
        SDL_Rect msgClip = {
            msgBox.x + 4,
            msgBox.y + 1,
            msgBox.w - 8,
            msgBox.h - 2
        };
        if (msgClip.w < 0) msgClip.w = 0;
        if (msgClip.h < 0) msgClip.h = 0;
        drawTextUTF8WithFontColorClipped(msgBox.x + 6,
                                         msgBox.y + 3,
                                         "commit message...",
                                         uiFont,
                                         placeholderTextColor,
                                         false,
                                         &msgClip);
    }

    if (git_panel_is_message_focused()) {
        int cursor = git_panel_get_message_cursor();
        if (cursor < 0) cursor = 0;
        size_t msgLen = msg ? strlen(msg) : 0;
        if ((size_t)cursor > msgLen) cursor = (int)msgLen;
        int caretAdvance = (msg && cursor > 0) ? getTextWidthNWithFont(msg, cursor, uiFont) : 0;
        int caretX = msgBox.x + 6 + caretAdvance;
        int caretMinX = msgBox.x + 4;
        if (caretX > msgBox.x + msgBox.w - 4) caretX = msgBox.x + msgBox.w - 4;
        if (caretX < caretMinX) caretX = caretMinX;
        SDL_Rect caretClip = { msgBox.x + 4, msgBox.y + 2, msgBox.w - 8, msgBox.h - 4 };
        if (caretClip.w > 0 && caretClip.h > 0) {
            pushClipRect(&caretClip);
            SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
            SDL_RenderDrawLine(renderer, caretX, msgBox.y + 3, caretX, msgBox.y + 17);
            popClipRect();
        }
    }

    char branchLine[96];
    snprintf(branchLine, sizeof(branchLine), "Branch: %s", currentGitBranch[0] ? currentGitBranch : "unknown");
    SDL_Rect branchClip = { pane->x + 10, layout.branchY, pane->w - 20, 14 };
    if (branchClip.w < 0) branchClip.w = 0;
    SDL_Color branchColor = {232, 238, 248, 255};
    drawTextUTF8WithFontColorClipped(branchClip.x,
                                     branchClip.y,
                                     branchLine,
                                     uiFont,
                                     branchColor,
                                     false,
                                     &branchClip);

    const char* status = git_panel_get_status_text();
    if (status && status[0]) {
        SDL_Rect statusClip = { pane->x + 10, layout.statusY, pane->w - 20, 14 };
        if (statusClip.w < 0) statusClip.w = 0;
        SDL_Color statusColor = {213, 220, 233, 255};
        drawTextUTF8WithFontColorClipped(statusClip.x,
                                         statusClip.y,
                                         status,
                                         uiFont,
                                         statusColor,
                                         false,
                                         &statusClip);
    }

    handleTreeMouseMove(mouseX, mouseY);
    UIPane treePane = git_tree_pane(pane, git_panel_tree_content_top(pane));
    renderTreePanelWithScroll(&treePane, gitTree, &gitScroll, &gitScrollTrack, &gitScrollThumb);
}
