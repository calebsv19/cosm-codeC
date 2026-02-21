#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Git/tree_git_adapter.h"

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"



#include <SDL2/SDL.h>
#include <string.h>

extern int mouseX;
extern int mouseY;

static Uint8 clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

static SDL_Color darken_color(SDL_Color c, int amount) {
    return (SDL_Color){
        clamp_u8((int)c.r - amount),
        clamp_u8((int)c.g - amount),
        clamp_u8((int)c.b - amount),
        c.a
    };
}

static bool same_rgb(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static UIPane git_tree_pane(const UIPane* pane) {
    UIPane treePane = *pane;
    treePane.y = pane->y + GIT_PANEL_HEADER_HEIGHT - 30;
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

    const int controlsY = pane->y + 24;
    SDL_Rect addAllBtn = { pane->x + 10, controlsY, 70, 20 };
    SDL_Rect commitBtn = { pane->x + pane->w - 76, controlsY, 66, 20 };
    SDL_Rect msgBox = {
        addAllBtn.x + addAllBtn.w + 8,
        controlsY,
        commitBtn.x - (addAllBtn.x + addAllBtn.w + 8) - 8,
        20
    };
    if (msgBox.w < 48) msgBox.w = 48;

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
        pushClipRect(&msgClip);
        drawText(msgBox.x + 6, msgBox.y + 3, msg);
        popClipRect();
    } else if (!git_panel_is_message_focused()) {
        drawText(msgBox.x + 6, msgBox.y + 3, "commit message...");
    }

    if (git_panel_is_message_focused()) {
        int cursor = git_panel_get_message_cursor();
        if (cursor < 0) cursor = 0;
        size_t msgLen = msg ? strlen(msg) : 0;
        if ((size_t)cursor > msgLen) cursor = (int)msgLen;
        char prefix[256];
        size_t copyLen = (size_t)cursor;
        if (copyLen >= sizeof(prefix)) copyLen = sizeof(prefix) - 1;
        if (msg && copyLen > 0) memcpy(prefix, msg, copyLen);
        prefix[copyLen] = '\0';
        int caretX = msgBox.x + 6 + getTextWidth(prefix);
        if (caretX > msgBox.x + msgBox.w - 4) caretX = msgBox.x + msgBox.w - 4;
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderDrawLine(renderer, caretX, msgBox.y + 3, caretX, msgBox.y + 17);
    }

    char branchLine[96];
    snprintf(branchLine, sizeof(branchLine), "Branch: %s", currentGitBranch[0] ? currentGitBranch : "unknown");
    drawClippedText(pane->x + 10, controlsY + 22, branchLine, pane->w - 20);
    const char* status = git_panel_get_status_text();
    if (status && status[0]) {
        drawClippedText(pane->x + 10, controlsY + 34, status, pane->w - 20);
    }

    int contentTop = pane->y + GIT_PANEL_HEADER_HEIGHT;
    int contentH = pane->h - (contentTop - pane->y);
    if (contentH < 0) contentH = 0;

    SDL_Color editorBg = ide_shared_theme_background_color();
    SDL_Color listBg = editorBg;
    if (same_rgb(listBg, pane->bgColor)) {
        listBg = darken_color(pane->bgColor, 14);
    }
    SDL_Rect bodyBg = { pane->x + 1, contentTop, pane->w - 2, contentH };
    SDL_SetRenderDrawColor(renderer, listBg.r, listBg.g, listBg.b, 255);
    SDL_RenderFillRect(renderer, &bodyBg);

    handleTreeMouseMove(mouseX, mouseY);
    UIPane treePane = git_tree_pane(pane);
    renderTreePanelWithScroll(&treePane, gitTree, &gitScroll, &gitScrollTrack, &gitScrollThumb);
}
