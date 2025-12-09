#include "engine/Render/render_helpers.h"

#include "ide/Panes/ToolPanels/BuildOutput/render_tool_build_output.h"
#include "ide/Panes/ToolPanels/BuildOutput/tool_build_output.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "engine/Render/render_pipeline.h" // getRenderContext
#include <SDL2/SDL.h>
#include <string.h>

// Forward decl from tool_build_output.c
bool build_output_is_selected(int idx);

static void renderDiagnosticsList(const BuildDiagnostic* diags, size_t count, int x, int y, int maxY, int lineHeight) {
    char line[1400];
    for (size_t i = 0; i < count && y + lineHeight <= maxY; ++i) {
        const BuildDiagnostic* d = &diags[i];
        const char* sev = d->isError ? "[E]" : "[W]";
        // Highlight selection
        if (build_output_is_selected((int)i)) {
            SDL_Rect highlight = { x - 6, y - 2, 1000, lineHeight * 2 };
            SDL_SetRenderDrawColor(getRenderContext()->renderer, 60, 80, 120, 120);
            SDL_RenderFillRect(getRenderContext()->renderer, &highlight);
        }
        // Line 1: label + location
        snprintf(line, sizeof(line), "%s %s:%d:%d", sev, d->path, d->line, d->col);
        drawText(x, y, line);
        y += lineHeight;
        // Line 2: indented message
        snprintf(line, sizeof(line), "    %s", d->message);
        drawText(x, y, line);
        y += lineHeight;
        if (d->notes[0] && y + lineHeight <= maxY) {
            snprintf(line, sizeof(line), "    note: %s", d->notes);
            drawText(x, y, line);
            y += lineHeight;
        }
    }
}

void renderBuildOutputPanel(UIPane* pane) {
    int x = pane->x + 12;
    int y = pane->y + 32;
    int maxY = pane->y + pane->h;
    int lineHeight = 20;

    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    if (!diags || count == 0) {
        drawText(x, y, "(No build diagnostics yet)");
        return;
    }

    renderDiagnosticsList(diags, count, x, y, maxY, lineHeight);
}
