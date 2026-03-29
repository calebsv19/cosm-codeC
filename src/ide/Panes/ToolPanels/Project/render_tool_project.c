#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/UI/ui_selection_style.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

PaneScrollState* project_get_scroll_state(UIPane* pane);
static const int PROJECT_TREE_BOX_PAD_X = 3;
static const int PROJECT_TREE_BOX_PAD_Y = 1;

static SDL_Color project_entry_text_color(const DirEntry* entry) {
    IDEThemePalette palette = {0};
    SDL_Color folderColor = {236, 240, 246, 255};
    SDL_Color fileColor = {174, 184, 198, 255};

    if (ide_shared_theme_resolve_palette(&palette)) {
        folderColor = palette.text_primary;
        fileColor = (SDL_Color){
            (Uint8)(((int)palette.text_primary.r * 2 + (int)palette.text_muted.r * 3) / 5),
            (Uint8)(((int)palette.text_primary.g * 2 + (int)palette.text_muted.g * 3) / 5),
            (Uint8)(((int)palette.text_primary.b * 2 + (int)palette.text_muted.b * 3) / 5),
            255
        };
    }

    if (entry && entry->type == ENTRY_FOLDER) {
        return folderColor;
    }
    return fileColor;
}

static TTF_Font* project_entry_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static const char* project_entry_display_name(const DirEntry* entry) {
    if (!entry || !entry->path) return "";
    const char* displayName = strrchr(entry->path, '/');
    return (displayName) ? displayName + 1 : entry->path;
}

static bool project_should_skip_entry(DirEntry* entry, const char** outName) {
    if (!entry) return true;
    const char* displayName = project_entry_display_name(entry);
    if (outName) *outName = displayName;
    if (!displayName) return true;
    if (entry->parent == NULL) return false;
    if (displayName[0] == '.' && strcmp(displayName, "..") != 0) return true;
    if (entry->type == ENTRY_FILE) {
        const char* ext = strrchr(displayName, '.');
        if (ext && (strcmp(ext, ".o") == 0 ||
                    strcmp(ext, ".obj") == 0 ||
                    strcmp(ext, ".out") == 0)) {
            return true;
        }
        if (strcmp(displayName, "last_build") == 0) return true;
    }
    return false;
}

static int project_count_visible_entries(DirEntry* entry) {
    if (!entry) return 0;
    const char* displayName = NULL;
    if (project_should_skip_entry(entry, &displayName)) return 0;
    int total = 1;
    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; ++i) {
            total += project_count_visible_entries(entry->children[i]);
        }
    }
    return total;
}

typedef struct ProjectRenderContext {
    UIPane* pane;
    SDL_Renderer* renderer;
    float offset;
    float currentY;
    float viewportTop;
    float viewportBottom;
    SDL_Rect clipRect;
    int baseX;
    int lineHeight;
    int indentWidth;
    ProjectDragState* drag;
} ProjectRenderContext;

typedef struct ProjectTopControlSpec {
    ProjectTopControlId id;
    const char* symbol;
    const char* label;
} ProjectTopControlSpec;

typedef struct ProjectRowVisualState {
    int draw_x;
    int text_y;
    int depth;
    bool inside_viewport;
    bool hovered;
    const char* prefix;
    char line[1024];
    SDL_Color text_color;
    UIRowSurfaceLayout visible_surface;
    UIRowSurfaceRenderSpec surface_spec;
} ProjectRowVisualState;

static const ProjectTopControlSpec s_projectTopControls[] = {
    { PROJECT_TOP_CONTROL_ADD_FILE, "+", "Add File" },
    { PROJECT_TOP_CONTROL_DELETE_FILE, "-", "Delete File" },
    { PROJECT_TOP_CONTROL_ADD_FOLDER, "+", "Add Folder" },
    { PROJECT_TOP_CONTROL_DELETE_FOLDER, "-", "Delete Folder" },
};

static UIRowSurfaceRenderSpec project_row_surface_spec(const DirEntry* entry,
                                                       const ProjectRenderContext* ctx,
                                                       bool hovered,
                                                       bool selectAllVisual,
                                                       bool isDirectoryDropTarget,
                                                       bool isRunTarget,
                                                       bool isRunAncestor) {
    UIRowSurfaceRenderSpec spec = {0};
    (void)ctx;

    spec.draw_selection_fill = selectAllVisual;
    spec.draw_selection_outline = selectAllVisual;
    spec.draw_hover_outline = hovered;

    if (entry == selectedDirectory) {
        spec.use_primary_fill = true;
        spec.primary_fill = (SDL_Color){80, 160, 90, 120};
    }
    if (entry == selectedFile) {
        spec.use_primary_fill = true;
        spec.primary_fill = (SDL_Color){70, 120, 200, 140};
    }
    if (isRunTarget) {
        spec.use_secondary_fill = true;
        spec.secondary_fill = (SDL_Color){200, 60, 60, 120};
    }
    if (isDirectoryDropTarget) {
        spec.use_primary_outline = true;
        spec.primary_outline = (SDL_Color){40, 170, 120, 210};
    }
    if (isRunAncestor) {
        spec.use_secondary_outline = true;
        spec.secondary_outline = (SDL_Color){200, 60, 60, 80};
    }

    return spec;
}

static const char* project_row_prefix(const DirEntry* entry) {
    if (!entry || entry->type != ENTRY_FOLDER) return "";
    return entry->isExpanded ? "[-] " : "[+] ";
}

static void project_row_build_line(char* out_line,
                                   size_t out_cap,
                                   const char* prefix,
                                   const char* display_name,
                                   const DirEntry* entry) {
    if (!out_line || out_cap == 0) return;
    if (!prefix) prefix = "";
    if (!display_name) display_name = "";

    if (entry == renamingEntry) {
        int cursor = project_get_rename_cursor();
        int renameLen = (int)strlen(renameBuffer);
        if (cursor < 0) cursor = 0;
        if (cursor > renameLen) cursor = renameLen;
        snprintf(out_line,
                 out_cap,
                 "%s%.*s_%s",
                 prefix,
                 cursor,
                 renameBuffer,
                 renameBuffer + cursor);
        return;
    }

    snprintf(out_line, out_cap, "%s%s", prefix, display_name);
}

static void project_row_run_target_flags(const DirEntry* entry,
                                         const char* run_target_path,
                                         bool* out_is_run_target,
                                         bool* out_is_run_ancestor) {
    bool isRunTarget = false;
    bool isRunAncestor = false;

    if (entry && entry->path && run_target_path && run_target_path[0] != '\0') {
        isRunTarget = (strcmp(entry->path, run_target_path) == 0);
        if (!isRunTarget && entry->type == ENTRY_FOLDER) {
            size_t len = strlen(entry->path);
            if (strncmp(run_target_path, entry->path, len) == 0) {
                char next = run_target_path[len];
                if (next == '/' || next == '\\') {
                    isRunAncestor = true;
                }
            }
        }
    }

    if (out_is_run_target) *out_is_run_target = isRunTarget;
    if (out_is_run_ancestor) *out_is_run_ancestor = isRunAncestor;
}

static ProjectRowVisualState project_build_row_visual_state(ProjectRenderContext* ctx,
                                                            const DirEntry* entry,
                                                            const char* display_name,
                                                            int depth,
                                                            float draw_y) {
    ProjectRowVisualState state = {0};
    if (!ctx || !entry) return state;

    const char* runTargetPath = project_run_target_path_buffer();
    TTF_Font* rowFont = project_entry_font();
    int textHeight = rowFont ? TTF_FontHeight(rowFont) : ctx->lineHeight;
    if (textHeight < 1) textHeight = ctx->lineHeight;

    state.depth = depth;
    state.draw_x = ctx->baseX + (depth * ctx->indentWidth);
    state.text_y = (int)draw_y + ((ctx->lineHeight - textHeight) / 2);
    state.prefix = project_row_prefix(entry);
    state.text_color = project_entry_text_color(entry);
    project_row_build_line(state.line, sizeof(state.line), state.prefix, display_name, entry);

    int textWidth = getTextWidthWithFont(state.line, rowFont);
    SDL_Rect box = {
        .x = state.draw_x - PROJECT_TREE_BOX_PAD_X,
        .y = state.text_y - PROJECT_TREE_BOX_PAD_Y,
        .w = textWidth + (PROJECT_TREE_BOX_PAD_X * 2),
        .h = textHeight + (PROJECT_TREE_BOX_PAD_Y * 2)
    };

    UIRowSurfaceLayout rowSurface = ui_row_surface_layout_from_rect(box);
    state.inside_viewport = ui_row_surface_clip(&rowSurface, &ctx->clipRect, &state.visible_surface);
    if (!state.inside_viewport) {
        return state;
    }

    bool isDirectoryDropTarget = ctx->drag && ctx->drag->validDirectoryTarget &&
                                 ctx->drag->targetDirectory == entry;
    bool isRunTarget = false;
    bool isRunAncestor = false;
    project_row_run_target_flags(entry, runTargetPath, &isRunTarget, &isRunAncestor);

    state.hovered = ui_row_surface_contains(&state.visible_surface, project_mouse_x(), project_mouse_y());
    state.surface_spec = project_row_surface_spec(entry,
                                                  ctx,
                                                  state.hovered,
                                                  project_select_all_visual_active(),
                                                  isDirectoryDropTarget,
                                                  isRunTarget,
                                                  isRunAncestor);
    return state;
}

static void project_render_entry(ProjectRenderContext* ctx, DirEntry* entry, int depth) {
    const char* displayName = NULL;
    if (project_should_skip_entry(entry, &displayName)) return;

    float drawY = ctx->currentY - ctx->offset;
    ctx->currentY += (float)ctx->lineHeight;
    ProjectRowVisualState row = project_build_row_visual_state(ctx, entry, displayName, depth, drawY);

    if (row.inside_viewport) {
        TTF_Font* rowFont = project_entry_font();
        ui_row_surface_render(ctx->renderer, &row.visible_surface, &row.surface_spec);

        drawTextUTF8WithFontColorClipped(row.draw_x, row.text_y, row.line, rowFont,
                                         row.text_color, false,
                                         &ctx->clipRect);

        if (row.hovered) {
            hoveredEntry = entry;
            hoveredEntryDepth = row.depth;
            hoveredEntryRect = row.visible_surface.bounds;
        }
    }

    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; ++i) {
            project_render_entry(ctx, entry->children[i], depth + 1);
        }
    }
}

PaneScrollState* project_get_scroll_state(UIPane* pane) {
    const float row_height_px = (float)IDE_UI_DENSE_ROW_HEIGHT;
    if (!pane) return NULL;
    if (!*project_panel_scroll_initialized_ptr()) {
        PaneScrollConfig cfg = {
            .line_height_px = row_height_px,
            .deceleration_px = 0.0f,
            .allow_negative = false,
        };
        scroll_state_init(project_panel_scroll_state(), &cfg);
        *project_panel_scroll_initialized_ptr() = true;
    }
    project_panel_scroll_state()->line_height_px = row_height_px;
    pane->scrollState = project_panel_scroll_state();
    return pane->scrollState;
}

void renderProjectFilesPanel(UIPane* pane) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    IDECoreState* coreState = getCoreState();
    bool paneHovered = coreState && coreState->activeMousePane == pane;
    bool paneActive = coreState && coreState->focusedPane == pane;

    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int rowHeight = IDE_UI_DENSE_ROW_HEIGHT;
    int x = pane->x + d.pad_left;
    const char* workspace = getWorkspacePath();
    char workspaceLabel[512];
    if (workspace && workspace[0]) {
        snprintf(workspaceLabel, sizeof(workspaceLabel), "Workspace: %s", workspace);
    } else {
        snprintf(workspaceLabel, sizeof(workspaceLabel), "Workspace: (sample project)");
    }
    drawClippedText(x, tool_panel_info_line_y(pane, 0), workspaceLabel, pane->w - (d.pad_left + d.pad_right));

    const char* runTarget = getRunTargetPath();
    char runLabel[512];
    if (runTarget && runTarget[0]) {
        snprintf(runLabel, sizeof(runLabel), "Run target: %s", runTarget);
    } else {
        snprintf(runLabel, sizeof(runLabel), "Run target: (auto-select on build)");
    }
    drawClippedText(x, tool_panel_info_line_y(pane, 1), runLabel, pane->w - (d.pad_left + d.pad_right));

    int y = tool_panel_info_line_y(pane, 1) + d.button_h;
    int maxY = pane->y + pane->h;
    hoveredEntry = NULL;
    hoveredEntryRect = (SDL_Rect){0};
    hoveredEntryDepth = 0;

    const int iconBtnSize = 24;
    const int spacing = 8;
    const int topControlCount = (int)(sizeof(s_projectTopControls) / sizeof(s_projectTopControls[0]));
    UIPanelVerticalButtonStackLayout topControls =
        ui_panel_vertical_button_stack_layout(x, y, iconBtnSize, iconBtnSize, spacing, topControlCount);
    UIPanelTaggedRectList* topControlHits = project_top_control_hits();
    ui_panel_tagged_rect_list_reset(topControlHits);

    for (int i = 0; i < topControls.count; ++i) {
        SDL_Rect buttonRect = topControls.button_rects[i];
        ui_panel_compact_button_render(renderer,
                                       &(UIPanelCompactButtonSpec){
                                           .rect = buttonRect,
                                           .label = s_projectTopControls[i].symbol,
                                           .active = false,
                                           .outlined = false,
                                           .use_custom_fill = false,
                                           .use_custom_outline = false,
                                           .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                       });
        (void)ui_panel_tagged_rect_list_add(topControlHits, s_projectTopControls[i].id, buttonRect);
        drawText(buttonRect.x + buttonRect.w + 6, buttonRect.y + 4, s_projectTopControls[i].label);
    }
    y += (topControls.count * iconBtnSize) + ((topControls.count > 0 ? topControls.count - 1 : 0) * spacing);

    const int treeTopGap = 8;
    int contentTop = y;
    int treeStartY = contentTop + treeTopGap;

    tool_panel_render_split_background(renderer, pane, contentTop, 14);

    const int trackWidth = 6;
    const int trackPadding = 4;

    SDL_Rect viewport = {
        .x = pane->x,
        .y = contentTop,
        .w = pane->w,
        .h = maxY - contentTop
    };
    if (viewport.h < 0) viewport.h = 0;
    if (viewport.w < 0) viewport.w = 0;

    PaneScrollState* scroll = project_get_scroll_state(pane);
    scroll_state_set_viewport(scroll, (float)viewport.h);

    int visibleLines = project_count_visible_entries(projectRoot);
    float contentHeight = (float)treeTopGap +
                          (float)visibleLines * (float)rowHeight;
    scroll_state_set_content_height(scroll,
                                    scroll_state_top_anchor_content_height(scroll, contentHeight));

    SDL_Rect clipRect = {
        .x = x - 6,
        .y = contentTop,
        .w = pane->w - (trackWidth + trackPadding + (x - pane->x)),
        .h = viewport.h
    };
    if (clipRect.w < 0) clipRect.w = 0;

    pushClipRect(&clipRect);

    if (projectRoot && visibleLines > 0) {
        ProjectRenderContext renderCtx = {
            .pane = pane,
            .renderer = renderer,
            .offset = scroll_state_get_offset(scroll),
            .currentY = (float)treeStartY,
            .viewportTop = (float)viewport.y,
            .viewportBottom = (float)(viewport.y + viewport.h),
            .clipRect = clipRect,
            .baseX = x,
            .lineHeight = rowHeight,
            .indentWidth = IDE_UI_TREE_INDENT_WIDTH,
            .drag = &coreState->projectDrag,
        };
        project_render_entry(&renderCtx, projectRoot, 0);
    } else if (viewport.h > 0) {
        drawText(x, treeStartY, "(No project loaded)");
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(scroll) && viewport.h > 0 &&
                         (paneHovered || paneActive || scroll_state_is_dragging_thumb(scroll));

    if (showScrollbar) {
        *project_panel_scroll_track_ptr() = (SDL_Rect){
            clipRect.x + clipRect.w + trackPadding,
            clipRect.y,
            trackWidth,
            clipRect.h
        };
        *project_panel_scroll_thumb_ptr() = scroll_state_thumb_rect(scroll,
                                                                    project_panel_scroll_track_ptr()->x,
                                                                    project_panel_scroll_track_ptr()->y,
                                                                    project_panel_scroll_track_ptr()->w,
                                                                    project_panel_scroll_track_ptr()->h);

        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(renderer, project_panel_scroll_track_ptr());
        SDL_SetRenderDrawColor(renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(renderer, project_panel_scroll_thumb_ptr());
    } else {
        *project_panel_scroll_track_ptr() = (SDL_Rect){0};
        *project_panel_scroll_thumb_ptr() = (SDL_Rect){0};
    }
}

SDL_Rect project_get_scroll_track_rect(void) {
    return *project_panel_scroll_track_ptr();
}

SDL_Rect project_get_scroll_thumb_rect(void) {
    return *project_panel_scroll_thumb_ptr();
}
