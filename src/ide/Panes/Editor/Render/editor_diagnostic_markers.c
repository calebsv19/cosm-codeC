#include "ide/Panes/Editor/Render/editor_diagnostic_markers.h"

#include "core/Diagnostics/diagnostics_engine.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"

#include <string.h>

enum {
    EDITOR_DIAGNOSTIC_MAX_RENDER_MARKERS = 512
};

static bool diagnostic_matches_file(const Diagnostic* diag, const char* file_path) {
    if (!diag || !file_path || !file_path[0]) return false;
    return diag->filePath && strcmp(diag->filePath, file_path) == 0;
}

static SDL_Color diagnostic_marker_color(DiagnosticSeverity severity,
                                         const IDEThemePalette* palette,
                                         Uint8 alpha) {
    SDL_Color color = {140, 180, 255, alpha};
    if (palette) {
        color = palette->accent_primary;
        if (severity == DIAG_SEVERITY_ERROR) {
            color = palette->accent_error;
        } else if (severity == DIAG_SEVERITY_WARNING) {
            color = palette->accent_warning;
        }
    } else if (severity == DIAG_SEVERITY_ERROR) {
        color = (SDL_Color){255, 96, 96, alpha};
    } else if (severity == DIAG_SEVERITY_WARNING) {
        color = (SDL_Color){232, 214, 162, alpha};
    }
    color.a = alpha;
    return color;
}

size_t editor_diagnostic_markers_collect(const char* file_path,
                                         int line_count,
                                         EditorDiagnosticMarker* out_markers,
                                         size_t max_markers) {
    if (!file_path || !file_path[0] || line_count <= 0 || !out_markers || max_markers == 0u) {
        return 0u;
    }

    size_t count = 0u;
    int diag_count = getDiagnosticCount();
    for (int i = 0; i < diag_count; ++i) {
        const Diagnostic* diag = getDiagnosticAt(i);
        if (!diagnostic_matches_file(diag, file_path)) continue;

        int line = diag->line > 0 ? diag->line - 1 : 0;
        if (line < 0 || line >= line_count) continue;

        out_markers[count].line = line;
        out_markers[count].column = diag->column > 0 ? diag->column - 1 : 0;
        out_markers[count].length = diag->length > 0 ? diag->length : 1;
        out_markers[count].severity = diag->severity;
        count++;
        if (count >= max_markers) break;
    }
    return count;
}

bool editor_diagnostic_markers_file_has_markers(const char* file_path,
                                                int line_count) {
    EditorDiagnosticMarker marker = {0};
    return editor_diagnostic_markers_collect(file_path, line_count, &marker, 1u) > 0u;
}

void render_editor_diagnostic_line_marker(const OpenFile* file,
                                          int line_index,
                                          const char* line_text,
                                          TTF_Font* text_font,
                                          const IDEThemePalette* palette,
                                          int text_x,
                                          int text_max_width,
                                          int y_line,
                                          int line_height,
                                          const SDL_Rect* clip) {
    if (!file || !file->buffer || !file->filePath || line_index < 0 || !text_font) return;
    if (text_max_width <= 0 || line_height <= 0) return;

    RenderContext* render_ctx = getRenderContext();
    if (!render_ctx || !render_ctx->renderer) return;
    SDL_Renderer* renderer = render_ctx->renderer;

    EditorDiagnosticMarker markers[EDITOR_DIAGNOSTIC_MAX_RENDER_MARKERS];
    size_t count = editor_diagnostic_markers_collect(file->filePath,
                                                     file->buffer->lineCount,
                                                     markers,
                                                     EDITOR_DIAGNOSTIC_MAX_RENDER_MARKERS);
    if (count == 0u) return;

    const char* text = line_text ? line_text : "";
    int text_len = (int)strlen(text);
    for (size_t i = 0; i < count; ++i) {
        const EditorDiagnosticMarker* marker = &markers[i];
        if (marker->line != line_index) continue;

        int col = marker->column;
        if (col < 0) col = 0;
        if (col > text_len) col = text_len;
        int end_col = col + (marker->length > 0 ? marker->length : 1);
        if (end_col < col + 1) end_col = col + 1;
        if (end_col > text_len) end_col = text_len;

        int start_x = text_x + getTextWidthNWithFont(text, col, text_font);
        int end_x = text_x + getTextWidthNWithFont(text, end_col, text_font);
        if (end_x <= start_x) {
            end_x = start_x + 8;
        }
        int max_x = text_x + text_max_width;
        if (start_x < text_x) start_x = text_x;
        if (end_x > max_x) end_x = max_x;
        if (end_x <= start_x) continue;

        int y = y_line + line_height - 3;
        if (clip) {
            if (y < clip->y || y >= clip->y + clip->h) continue;
            if (start_x < clip->x) start_x = clip->x;
            if (end_x > clip->x + clip->w) end_x = clip->x + clip->w;
            if (end_x <= start_x) continue;
        }

        SDL_Color color = diagnostic_marker_color(marker->severity, palette, 210u);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer, start_x, y, end_x, y);
        SDL_RenderDrawLine(renderer, start_x, y + 1, end_x, y + 1);
    }
}

void render_editor_diagnostic_scrollbar_ticks(const OpenFile* file,
                                              SDL_Renderer* renderer,
                                              const IDEThemePalette* palette,
                                              const SDL_Rect* scrollbar_track) {
    if (!file || !file->buffer || !file->filePath || !renderer || !scrollbar_track) return;
    int line_count = file->buffer->lineCount;
    if (line_count <= 0) return;

    EditorDiagnosticMarker markers[EDITOR_DIAGNOSTIC_MAX_RENDER_MARKERS];
    size_t count = editor_diagnostic_markers_collect(file->filePath,
                                                     line_count,
                                                     markers,
                                                     EDITOR_DIAGNOSTIC_MAX_RENDER_MARKERS);
    for (size_t i = 0; i < count; ++i) {
        const EditorDiagnosticMarker* marker = &markers[i];
        if (marker->line < 0 || marker->line >= line_count) continue;
        float ratio = line_count > 1 ? (float)marker->line / (float)(line_count - 1) : 0.0f;
        int tick_y = scrollbar_track->y + (int)(ratio * (float)(scrollbar_track->h - 2));
        SDL_Color color = diagnostic_marker_color(marker->severity, palette, 180u);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect tick = { scrollbar_track->x + 1, tick_y, scrollbar_track->w - 2, 2 };
        SDL_RenderFillRect(renderer, &tick);
    }
}
