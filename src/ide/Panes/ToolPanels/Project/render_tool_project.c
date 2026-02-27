#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/UI/ui_selection_style.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

extern int mouseX, mouseY;
extern DirEntry* hoveredEntry;
extern DirEntry* selectedEntry;
extern DirEntry* selectedFile;
extern DirEntry* selectedDirectory;
extern int hoveredEntryDepth;
extern SDL_Rect hoveredEntryRect;
extern char runTargetPath[1024];
extern DirEntry* renamingEntry;
extern char renameBuffer[256];

PaneScrollState* project_get_scroll_state(UIPane* pane);

static PaneScrollState s_projectScrollState;
static bool s_projectScrollInitialized = false;
static const int PROJECT_TREE_INDENT_WIDTH = 10;
static const int PROJECT_TREE_BOX_PAD_X = 3;
static const int PROJECT_TREE_BOX_PAD_Y = 1;
static const PaneScrollConfig s_projectScrollConfig = {
    .line_height_px = 14.0f,
    .deceleration_px = 0.0f,
    .allow_negative = false,
};
static SDL_Rect s_projectScrollTrack = {0};
static SDL_Rect s_projectScrollThumb = {0};

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

static void project_render_entry(ProjectRenderContext* ctx, DirEntry* entry, int depth) {
    const char* displayName = NULL;
    if (project_should_skip_entry(entry, &displayName)) return;

    float drawY = ctx->currentY - ctx->offset;
    ctx->currentY += (float)ctx->lineHeight;

    int indent = depth * ctx->indentWidth;
    int drawX = ctx->baseX + indent;

    const bool selectAllVisual = project_select_all_visual_active();

    const char* prefix = "";
    if (entry->type == ENTRY_FOLDER) {
        prefix = entry->isExpanded ? "[-] " : "[+] ";
    }

    char line[1024];
    if (entry == renamingEntry) {
        snprintf(line, sizeof(line), "%s%s_", prefix, renameBuffer);
    } else {
        snprintf(line, sizeof(line), "%s%s", prefix, displayName ? displayName : "");
    }

    TTF_Font* rowFont = project_entry_font();
    int textWidth = getTextWidthWithFont(line, rowFont);
    int textHeight = rowFont ? TTF_FontHeight(rowFont) : ctx->lineHeight;
    if (textHeight < 1) textHeight = ctx->lineHeight;
    int textY = (int)drawY + ((ctx->lineHeight - textHeight) / 2);
    SDL_Rect box = {
        .x = drawX - PROJECT_TREE_BOX_PAD_X,
        .y = textY - PROJECT_TREE_BOX_PAD_Y,
        .w = textWidth + (PROJECT_TREE_BOX_PAD_X * 2),
        .h = textHeight + (PROJECT_TREE_BOX_PAD_Y * 2)
    };
    SDL_Rect visibleBox = {0};
    bool insideViewport = SDL_IntersectRect(&box, &ctx->clipRect, &visibleBox);

    bool isDirectoryDropTarget = ctx->drag && ctx->drag->validDirectoryTarget &&
                                 ctx->drag->targetDirectory == entry;

    if (insideViewport) {
        if (selectAllVisual) {
            SDL_Color fill = ui_selection_fill_color();
            SDL_SetRenderDrawColor(ctx->renderer, fill.r, fill.g, fill.b, fill.a);
            SDL_RenderFillRect(ctx->renderer, &box);
            SDL_Color outline = ui_selection_outline_color();
            SDL_SetRenderDrawColor(ctx->renderer, outline.r, outline.g, outline.b, outline.a);
            SDL_RenderDrawRect(ctx->renderer, &box);
        }
        if (entry == selectedDirectory) {
            SDL_SetRenderDrawColor(ctx->renderer, 80, 160, 90, 120);
            SDL_RenderFillRect(ctx->renderer, &box);
        }

        if (entry == selectedFile) {
            SDL_SetRenderDrawColor(ctx->renderer, 70, 120, 200, 140);
            SDL_RenderFillRect(ctx->renderer, &box);
        }
        if (isDirectoryDropTarget) {
            SDL_SetRenderDrawColor(ctx->renderer, 40, 170, 120, 210);
            SDL_RenderDrawRect(ctx->renderer, &box);
        }
    }

    bool isRunTarget = (runTargetPath[0] != '\0' && strcmp(entry->path, runTargetPath) == 0);
    bool isRunAncestor = false;
    if (!isRunTarget && runTargetPath[0] != '\0' && entry->type == ENTRY_FOLDER) {
        size_t len = strlen(entry->path);
        if (strncmp(runTargetPath, entry->path, len) == 0) {
            char next = runTargetPath[len];
            if (next == '/' || next == '\\') {
                isRunAncestor = true;
            }
        }
    }

    if (insideViewport) {
        if (isRunTarget) {
            SDL_SetRenderDrawColor(ctx->renderer, 200, 60, 60, 120);
            SDL_RenderFillRect(ctx->renderer, &box);
        } else if (isRunAncestor) {
            SDL_SetRenderDrawColor(ctx->renderer, 200, 60, 60, 80);
            SDL_RenderDrawRect(ctx->renderer, &box);
        }

        drawTextUTF8WithFontColorClipped(drawX, textY, line, rowFont,
                                         project_entry_text_color(entry), false,
                                         &ctx->clipRect);

        if (mouseY >= visibleBox.y && mouseY < visibleBox.y + visibleBox.h &&
            mouseX >= visibleBox.x && mouseX < visibleBox.x + visibleBox.w) {
            hoveredEntry = entry;
            hoveredEntryDepth = depth;
            SDL_SetRenderDrawColor(ctx->renderer, 180, 180, 180, 100);
            SDL_RenderDrawRect(ctx->renderer, &box);
            hoveredEntryRect = visibleBox;
        }
    }

    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; ++i) {
            project_render_entry(ctx, entry->children[i], depth + 1);
        }
    }
}

PaneScrollState* project_get_scroll_state(UIPane* pane) {
    if (!pane) return NULL;
    if (!s_projectScrollInitialized) {
        scroll_state_init(&s_projectScrollState, &s_projectScrollConfig);
        s_projectScrollInitialized = true;
    }
    pane->scrollState = &s_projectScrollState;
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

    projectBtnAddFile = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(pane, projectBtnAddFile, "+");
    drawText(x + iconBtnSize + 6, y + 4, "Add File");
    y += iconBtnSize + spacing;

    projectBtnDeleteFile = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(pane, projectBtnDeleteFile, "-");
    drawText(x + iconBtnSize + 6, y + 4, "Delete File");
    y += iconBtnSize + spacing;

    projectBtnAddFolder = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(pane, projectBtnAddFolder, "+");
    drawText(x + iconBtnSize + 6, y + 4, "Add Folder");
    y += iconBtnSize + spacing;

    projectBtnDeleteFolder = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(pane, projectBtnDeleteFolder, "-");
    drawText(x + iconBtnSize + 6, y + 4, "Delete Folder");
    y += iconBtnSize + spacing;

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
                          (float)visibleLines * s_projectScrollConfig.line_height_px;
    float effectiveHeight = contentHeight;
    if (scroll->line_height_px > 0.0f && scroll->viewport_height_px > scroll->line_height_px) {
        float slack = scroll->viewport_height_px - scroll->line_height_px;
        effectiveHeight = contentHeight + slack;
    }
    scroll_state_set_content_height(scroll, effectiveHeight);

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
            .lineHeight = (int)s_projectScrollConfig.line_height_px,
            .indentWidth = PROJECT_TREE_INDENT_WIDTH,
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
        s_projectScrollTrack = (SDL_Rect){
            clipRect.x + clipRect.w + trackPadding,
            clipRect.y,
            trackWidth,
            clipRect.h
        };
        s_projectScrollThumb = scroll_state_thumb_rect(scroll,
                                                       s_projectScrollTrack.x,
                                                       s_projectScrollTrack.y,
                                                       s_projectScrollTrack.w,
                                                       s_projectScrollTrack.h);

        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(renderer, &s_projectScrollTrack);
        SDL_SetRenderDrawColor(renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(renderer, &s_projectScrollThumb);
    } else {
        s_projectScrollTrack = (SDL_Rect){0};
        s_projectScrollThumb = (SDL_Rect){0};
    }
}

SDL_Rect project_get_scroll_track_rect(void) {
    return s_projectScrollTrack;
}

SDL_Rect project_get_scroll_thumb_rect(void) {
    return s_projectScrollThumb;
}
