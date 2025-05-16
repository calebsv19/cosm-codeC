#include "Render/ToolPanels/render_tool_errors.h"
#include "Render/render_helpers.h"

#include "ToolPanels/tool_errors.h"
#include "Diagnostics/diagnostics_engine.h"

#include <SDL2/SDL.h>
#include <stdio.h>

void renderErrorsPanel(UIPane* pane) {
    int paddingX = 12;
    int paddingY = 32;
    int lineHeight = 20;

    int x = pane->x + paddingX;
    int y = pane->y + paddingY;
    int maxY = pane->y + pane->h;

    int count = getDiagnosticCount();

    if (count == 0) {
        drawText(x, y, "(No errors or warnings)");
        return;
    }

    for (int i = 0; i < count; i++) {
        const Diagnostic* diag = getDiagnosticAt(i);
        if (!diag) continue;

        char line[1024];
        const char* severityStr = (diag->severity == DIAG_SEVERITY_ERROR)
            ? "error"
            : (diag->severity == DIAG_SEVERITY_WARNING)
                ? "warning"
                : "info";

        snprintf(line, sizeof(line), "%s:%d:%d: %s: %s",
                 diag->filePath, diag->line, diag->column,
                 severityStr, diag->message);

        if (y + lineHeight > maxY) break;
        drawText(x, y, line);
        y += lineHeight;
    }

    // FUTURE: Add summary line (e.g., [3 errors, 1 warning])
}

