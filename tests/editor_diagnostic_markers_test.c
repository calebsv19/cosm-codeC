#include "ide/Panes/Editor/Render/editor_diagnostic_markers.h"

#include "core/Diagnostics/diagnostics_engine.h"

#include <assert.h>
#include <stdio.h>

RenderContext globalRenderContext;

RenderContext* getRenderContext(void) {
    return NULL;
}

int getTextWidthNWithFont(const char* text, int n, TTF_Font* font) {
    (void)text;
    (void)font;
    return n > 0 ? n * 8 : 0;
}

void vk_renderer_set_draw_color(VkRenderer* renderer, float r, float g, float b, float a) {
    (void)renderer;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

void vk_renderer_draw_line(VkRenderer* renderer, float x0, float y0, float x1, float y1) {
    (void)renderer;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
}

void vk_renderer_fill_rect(VkRenderer* renderer, const SDL_Rect* rect) {
    (void)renderer;
    (void)rect;
}

int main(void) {
    const char* file_path = "/tmp/editor_diagnostic_markers/src/a.c";
    const char* other_path = "/tmp/editor_diagnostic_markers/src/b.c";

    initDiagnosticsEngine();
    addDiagnosticWithDetails(file_path,
                             3,
                             5,
                             4,
                             "expected expression",
                             NULL,
                             DIAG_SEVERITY_ERROR,
                             DIAG_CATEGORY_PARSER,
                             0,
                             NULL,
                             NULL);
    addDiagnosticWithDetails(other_path,
                             2,
                             1,
                             1,
                             "other file",
                             NULL,
                             DIAG_SEVERITY_WARNING,
                             DIAG_CATEGORY_SEMANTIC,
                             0,
                             NULL,
                             NULL);
    addDiagnosticWithDetails(file_path,
                             99,
                             1,
                             1,
                             "outside buffer",
                             NULL,
                             DIAG_SEVERITY_WARNING,
                             DIAG_CATEGORY_SEMANTIC,
                             0,
                             NULL,
                             NULL);

    EditorDiagnosticMarker markers[4];
    size_t count = editor_diagnostic_markers_collect(file_path, 10, markers, 4);
    assert(count == 1u);
    assert(markers[0].line == 2);
    assert(markers[0].column == 4);
    assert(markers[0].length == 4);
    assert(markers[0].severity == DIAG_SEVERITY_ERROR);
    assert(editor_diagnostic_markers_file_has_markers(file_path, 10));
    assert(!editor_diagnostic_markers_file_has_markers(file_path, 2));

    clearDiagnostics();
    puts("editor_diagnostic_markers_test: success");
    return 0;
}
