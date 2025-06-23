#include "ToolPanels/Git/render_tool_git.h"
#include "Render/render_helpers.h"


#include "ToolPanels/Git/tool_git.h"
#include <SDL2/SDL.h>
#include <string.h>



void renderGitPanel(UIPane* pane) {
    int x = pane->x + 12;
    int y = pane->y + 32;
    int maxY = pane->y + pane->h;
    int lineHeight = 20;

    // Display current branch
    if (y + lineHeight <= maxY) drawText(x, y, "[Branch] feature/editor-modes");
    y += lineHeight * 2;

    // Changes
    if (y + lineHeight <= maxY) drawText(x, y, "[Changes]");
    y += lineHeight;
    if (y + lineHeight <= maxY) drawText(x + 20, y, "M src/editor.c");
    y += lineHeight;
    if (y + lineHeight <= maxY) drawText(x + 20, y, "A src/tool_tasks.c");
    y += lineHeight;
    if (y + lineHeight <= maxY) drawText(x + 20, y, "D src/legacy_system.c");
    y += lineHeight * 2;

    // Staged
    if (y + lineHeight <= maxY) drawText(x, y, "[Staged]");
    y += lineHeight;
    if (y + lineHeight <= maxY) drawText(x + 20, y, "M include/editor.h");
    y += lineHeight * 2;

    // Untracked
    if (y + lineHeight <= maxY) drawText(x, y, "[Untracked]");
    y += lineHeight;
    if (y + lineHeight <= maxY) drawText(x + 20, y, "? assets/new_icon.png");
}

