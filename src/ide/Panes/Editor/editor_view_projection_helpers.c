#include "ide/Panes/Editor/editor_view_projection_helpers.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/GlobalInfo/project.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/Editor/editor_projection.h"

static bool g_projection_scope_all_open_files = true;

void editor_projection_reset(SearchProjection* projection) {
    if (!projection) return;
    projection->lines = NULL;
    projection->lineCount = 0;
    projection->projectedToRealLine = NULL;
    projection->projectedToRealCol = NULL;
    projection->realMatchLines = NULL;
    projection->realMatchCount = 0;
    projection->buildStamp = 0;
}

void editor_projection_free(SearchProjection* projection) {
    if (!projection) return;
    const int maxReasonableRows = 200000;
    int lineCount = projection->lineCount;
    if (lineCount < 0 || lineCount > maxReasonableRows) {
        fprintf(stderr, "[Projection] Ignoring suspicious lineCount=%d during free\n", projection->lineCount);
        lineCount = 0;
    }

    uintptr_t linesPtr = (uintptr_t)projection->lines;
    if (projection->lines && linesPtr >= 4096u) {
        for (int i = 0; i < lineCount; ++i) {
            uintptr_t rowPtr = (uintptr_t)projection->lines[i];
            if (rowPtr >= 4096u) {
                free(projection->lines[i]);
            }
        }
        free(projection->lines);
    } else if (projection->lines) {
        fprintf(stderr, "[Projection] Ignoring suspicious lines pointer=%p during free\n",
                (void*)projection->lines);
    }

    uintptr_t realLinePtr = (uintptr_t)projection->projectedToRealLine;
    if (projection->projectedToRealLine && realLinePtr >= 4096u) {
        free(projection->projectedToRealLine);
    } else if (projection->projectedToRealLine) {
        fprintf(stderr, "[Projection] Ignoring suspicious realLine pointer=%p during free\n",
                (void*)projection->projectedToRealLine);
    }

    uintptr_t realColPtr = (uintptr_t)projection->projectedToRealCol;
    if (projection->projectedToRealCol && realColPtr >= 4096u) {
        free(projection->projectedToRealCol);
    } else if (projection->projectedToRealCol) {
        fprintf(stderr, "[Projection] Ignoring suspicious realCol pointer=%p during free\n",
                (void*)projection->projectedToRealCol);
    }

    uintptr_t matchesPtr = (uintptr_t)projection->realMatchLines;
    if (projection->realMatchLines && matchesPtr >= 4096u) {
        free(projection->realMatchLines);
    } else if (projection->realMatchLines) {
        fprintf(stderr, "[Projection] Ignoring suspicious realMatch pointer=%p during free\n",
                (void*)projection->realMatchLines);
    }
    editor_projection_reset(projection);
}

void editor_invalidate_file_projection(OpenFile* file) {
    if (!file) return;
    editor_projection_free(&file->projection);
}

void editor_set_file_render_source(OpenFile* file, EditorRenderSource source) {
    if (!file) return;
    file->renderSource = source;
}

bool editor_file_projection_active(const OpenFile* file) {
    return file && file->renderSource == EDITOR_RENDER_PROJECTION;
}

bool editor_projection_map_row_to_source(const OpenFile* file,
                                         int projectedRow,
                                         int* outSourceRow,
                                         int* outSourceCol) {
    if (!file || !editor_file_projection_active(file)) return false;
    if (!file->projection.projectedToRealLine || file->projection.lineCount <= 0) return false;
    if (projectedRow < 0 || projectedRow >= file->projection.lineCount) return false;

    int row = file->projection.projectedToRealLine[projectedRow];
    int col = (file->projection.projectedToRealCol && projectedRow < file->projection.lineCount)
                  ? file->projection.projectedToRealCol[projectedRow]
                  : 0;

    if (row < 0) {
        int above = projectedRow - 1;
        while (above >= 0 && file->projection.projectedToRealLine[above] < 0) {
            above--;
        }
        if (above >= 0) {
            row = file->projection.projectedToRealLine[above];
            col = (file->projection.projectedToRealCol && above < file->projection.lineCount)
                      ? file->projection.projectedToRealCol[above]
                      : 0;
        }
    }
    if (row < 0) {
        int below = projectedRow + 1;
        while (below < file->projection.lineCount &&
               file->projection.projectedToRealLine[below] < 0) {
            below++;
        }
        if (below < file->projection.lineCount) {
            row = file->projection.projectedToRealLine[below];
            col = (file->projection.projectedToRealCol && below < file->projection.lineCount)
                      ? file->projection.projectedToRealCol[below]
                      : 0;
        }
    }

    if (row < 0) return false;
    if (col < 0) col = 0;
    if (outSourceRow) *outSourceRow = row;
    if (outSourceCol) *outSourceCol = col;
    return true;
}

void editor_projection_set_scope_all_open_files(bool enabled) {
    g_projection_scope_all_open_files = enabled;
}

bool editor_projection_scope_all_open_files(void) {
    return g_projection_scope_all_open_files;
}

static bool find_file_revision_in_view(const EditorView* view,
                                       const char* filePath,
                                       uint64_t* out_revision) {
    if (!view || !filePath || !filePath[0]) return false;
    if (view->type == VIEW_SPLIT) {
        if (find_file_revision_in_view(view->childA, filePath, out_revision)) return true;
        return find_file_revision_in_view(view->childB, filePath, out_revision);
    }
    if (view->type != VIEW_LEAF || !view->openFiles || view->fileCount <= 0) return false;
    for (int i = 0; i < view->fileCount; ++i) {
        OpenFile* file = view->openFiles[i];
        if (!file || !file->filePath) continue;
        if (strcmp(file->filePath, filePath) == 0) {
            if (out_revision) *out_revision = file->documentRevision;
            return true;
        }
    }
    return false;
}

bool editor_find_open_file_revision_by_path(const char* filePath, uint64_t* out_revision) {
    IDECoreState* core = getCoreState();
    if (!core || !filePath || !filePath[0]) return false;
    EditorView* root = core->persistentEditorView ? core->persistentEditorView : core->activeEditorView;
    return find_file_revision_in_view(root, filePath, out_revision);
}

static void sync_projection_for_file(OpenFile* file,
                                     bool applySearchMode,
                                     bool projectionRenderEnabled,
                                     const char* query,
                                     const SymbolFilterOptions* options) {
    if (!file) return;
    if (!applySearchMode) {
        editor_set_file_render_source(file, EDITOR_RENDER_REAL);
        editor_invalidate_file_projection(file);
        return;
    }
    editor_set_file_render_source(file,
                                  projectionRenderEnabled
                                      ? EDITOR_RENDER_PROJECTION
                                      : EDITOR_RENDER_REAL);
    editor_projection_rebuild(file, query, options);
}

static bool path_is_in_active_project(const char* filePath) {
    if (!filePath || !filePath[0] || !projectPath[0]) return false;
    size_t rootLen = strlen(projectPath);
    if (rootLen == 0) return false;
    if (strncmp(filePath, projectPath, rootLen) != 0) return false;
    char boundary = filePath[rootLen];
    return boundary == '\0' || boundary == '/';
}

static void sync_projection_for_view_tree(EditorView* view,
                                          OpenFile* activeFile,
                                          bool enableEditorTarget,
                                          bool queryActive,
                                          bool scopeProjectFiles,
                                          bool projectionRenderEnabled,
                                          const char* query,
                                          const SymbolFilterOptions* options) {
    if (!view) return;
    if (view->type == VIEW_SPLIT) {
        sync_projection_for_view_tree(view->childA,
                                      activeFile,
                                      enableEditorTarget,
                                      queryActive,
                                      scopeProjectFiles,
                                      projectionRenderEnabled,
                                      query,
                                      options);
        sync_projection_for_view_tree(view->childB,
                                      activeFile,
                                      enableEditorTarget,
                                      queryActive,
                                      scopeProjectFiles,
                                      projectionRenderEnabled,
                                      query,
                                      options);
        return;
    }
    if (view->type != VIEW_LEAF || !view->openFiles || view->fileCount <= 0) return;

    for (int i = 0; i < view->fileCount; ++i) {
        OpenFile* file = view->openFiles[i];
        bool inScope = scopeProjectFiles ? path_is_in_active_project(file ? file->filePath : NULL)
                                         : (file == activeFile);
        bool applySearchMode = enableEditorTarget && queryActive && inScope;
        sync_projection_for_file(file,
                                 applySearchMode,
                                 projectionRenderEnabled,
                                 query,
                                 options);
    }
}

void editor_sync_active_file_projection_mode(void) {
    IDECoreState* core = getCoreState();
    if (!core) {
        return;
    }

    const char* query = control_panel_get_search_query();
    bool queryActive = control_panel_is_search_enabled() && (query && query[0] != '\0');
    bool editorTargetEnabled = control_panel_target_editor_enabled();
    bool projectionRenderEnabled =
        control_panel_get_editor_view_mode() == CONTROL_EDITOR_VIEW_PROJECTION;
    bool scopeProjectFiles =
        control_panel_get_search_scope() == CONTROL_SEARCH_SCOPE_PROJECT_FILES;
    SymbolFilterOptions options = {0};
    control_panel_get_search_filter_options(&options);

    OpenFile* activeFile = NULL;
    if (core->activeEditorView && core->activeEditorView->type == VIEW_LEAF) {
        activeFile = getActiveOpenFile(core->activeEditorView);
    }

    EditorView* root = core->persistentEditorView ? core->persistentEditorView : core->activeEditorView;
    sync_projection_for_view_tree(root,
                                  activeFile,
                                  editorTargetEnabled,
                                  queryActive,
                                  scopeProjectFiles,
                                  projectionRenderEnabled,
                                  query,
                                  &options);
}
