#ifndef EDITOR_DIAGNOSTIC_MARKERS_H
#define EDITOR_DIAGNOSTIC_MARKERS_H

#include "core/Diagnostics/diagnostics_engine.h"
#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct EditorDiagnosticMarker {
    int line;
    int column;
    int length;
    DiagnosticSeverity severity;
} EditorDiagnosticMarker;

size_t editor_diagnostic_markers_collect(const char* file_path,
                                         int line_count,
                                         EditorDiagnosticMarker* out_markers,
                                         size_t max_markers);

bool editor_diagnostic_markers_file_has_markers(const char* file_path,
                                                int line_count);

void render_editor_diagnostic_line_marker(const OpenFile* file,
                                          int line_index,
                                          const char* line_text,
                                          TTF_Font* text_font,
                                          const IDEThemePalette* palette,
                                          int text_x,
                                          int text_max_width,
                                          int y_line,
                                          int line_height,
                                          const SDL_Rect* clip);

void render_editor_diagnostic_scrollbar_ticks(const OpenFile* file,
                                              SDL_Renderer* renderer,
                                              const IDEThemePalette* palette,
                                              const SDL_Rect* scrollbar_track);

#endif // EDITOR_DIAGNOSTIC_MARKERS_H
