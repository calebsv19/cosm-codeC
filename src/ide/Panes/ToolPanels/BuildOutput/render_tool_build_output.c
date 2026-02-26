#include "engine/Render/render_helpers.h"

#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/ToolPanels/BuildOutput/render_tool_build_output.h"
#include "ide/Panes/ToolPanels/BuildOutput/tool_build_output.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "ide/UI/ui_selection_style.h"
#include "engine/Render/render_pipeline.h" // getRenderContext
#include <SDL2/SDL.h>
#include <string.h>

// Forward decl from tool_build_output.c
bool build_output_is_selected(int idx);

static void renderDiagnosticsList(const BuildDiagnostic* diags,
                                  size_t count,
                                  int x,
                                  int y,
                                  int maxY,
                                  int lineHeight,
                                  int highlightW) {
    char line[1400];
    for (size_t i = 0; i < count && y + lineHeight <= maxY; ++i) {
        const BuildDiagnostic* d = &diags[i];
        const char* sev = d->isError ? "[E]" : "[W]";
        // Highlight selection
        if (build_output_is_selected((int)i)) {
            SDL_Rect highlight = { x - 6, y - 2, highlightW, lineHeight * 2 };
            SDL_Color sel = ui_selection_fill_color();
            SDL_SetRenderDrawColor(getRenderContext()->renderer, sel.r, sel.g, sel.b, sel.a);
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
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    int x = pane->x + d.pad_left;
    int contentTop = pane->y + d.controls_top + 8;
    int y = contentTop + tool_panel_content_inset_default();
    int maxY = pane->y + pane->h;
    int lineHeight = 20;
    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, 14);

    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    if (!diags || count == 0) {
        drawText(x, y, "(No build diagnostics yet)");
        return;
    }

    int highlightW = pane->w - 16;
    if (highlightW < 0) highlightW = 0;
    renderDiagnosticsList(diags, count, x, y, maxY, lineHeight, highlightW);
}
