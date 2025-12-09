#include "ide/Panes/ToolPanels/Errors/render_tool_errors.h"
#include "engine/Render/render_helpers.h"

#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "engine/Render/render_pipeline.h"

#include <SDL2/SDL.h>
#include <stdio.h>

// Forward from tool_errors.c
bool is_error_selected(int idx);

void renderErrorsPanel(UIPane* pane) {
    int paddingX = 12;
    int paddingY = 32;
    int lineHeight = 20;

    int x = pane->x + paddingX;
    int y = pane->y + paddingY;
    int maxY = pane->y + pane->h;

    size_t files = analysis_store_file_count();
    if (files == 0) {
        drawText(x, y, "(No errors or warnings)");
        return;
    }

    int flatIndex = 0;
    for (size_t fi = 0; fi < files; ++fi) {
        const AnalysisFileDiagnostics* f = analysis_store_file_at(fi);
        if (!f || f->count <= 0) continue;

        // File header
        if (y + lineHeight > maxY) break;
        drawText(x, y, f->path ? f->path : "(unknown file)");
        y += lineHeight;

        // Diagnostics for this file
        for (int di = 0; di < f->count; ++di, ++flatIndex) {
            if (y + lineHeight * 2 > maxY) break;

            const Diagnostic* diag = &f->diags[di];
            const char* sev = (diag->severity == DIAG_SEVERITY_ERROR)
                ? "[E]" : (diag->severity == DIAG_SEVERITY_WARNING) ? "[W]" : "[I]";

            if (is_error_selected(flatIndex)) {
                SDL_Rect highlight = { x - 6, y - 2, 1000, lineHeight * 2 };
                SDL_SetRenderDrawColor(getRenderContext()->renderer, 60, 80, 120, 120);
                SDL_RenderFillRect(getRenderContext()->renderer, &highlight);
            }

            char line[1024];
            snprintf(line, sizeof(line), "  %s %d:%d", sev, diag->line, diag->column);
            drawText(x, y, line);
            y += lineHeight;

            snprintf(line, sizeof(line), "      %s", diag->message ? diag->message : "(no message)");
            drawText(x, y, line);
            y += lineHeight;
        }
    }

    // FUTURE: Add summary line (e.g., [3 errors, 1 warning])
}
