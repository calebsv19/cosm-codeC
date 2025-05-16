#include "Render/ToolPanels/render_tool_build_output.h"
#include "Render/render_helpers.h"

#include "ToolPanels/tool_build_output.h"
#include "Build/build_system.h"   // for getBuildOutput()
#include <SDL2/SDL.h>
#include <string.h>

// Internal rendering
static void renderBuildLogLines(const char* logText, int x, int y, int maxY, int lineHeight) {
    char buffer[1024];
    char lineWithNumber[1152];  // Enough space for line number + buffer
    int lineIndex = 0;
    const char* lineStart = logText;

    while (*lineStart && y + lineHeight <= maxY) {
        const char* lineEnd = strchr(lineStart, '\n');
        if (!lineEnd) lineEnd = lineStart + strlen(lineStart);

        int len = lineEnd - lineStart;
        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        strncpy(buffer, lineStart, len);
        buffer[len] = '\0';

        // Format with line number prefix
        snprintf(lineWithNumber, sizeof(lineWithNumber), "%4d | %s", lineIndex + 1, buffer);
        drawText(x, y, lineWithNumber);
        y += lineHeight;

        lineStart = (*lineEnd == '\n') ? lineEnd + 1 : lineEnd;
        lineIndex++;
    }
}





void renderBuildOutputPanel(UIPane* pane) {
    int x = pane->x + 12;
    int y = pane->y + 32;
    int maxY = pane->y + pane->h;
    int lineHeight = 20;

    const char* log = getBuildOutput();
    if (!log || strlen(log) == 0) {
        drawText(x, y, "(No build output yet)");
        return;
    }

    renderBuildLogLines(log, x, y, maxY, lineHeight);
}

