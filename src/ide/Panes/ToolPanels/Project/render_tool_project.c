#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "core/UI/scroll_manager.h"
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
static const PaneScrollConfig s_projectScrollConfig = {
    .line_height_px = 22.0f,
    .deceleration_px = 0.0f,
    .allow_negative = false,
};

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

static bool project_entry_in_build_outputs(const DirEntry* entry) {
    if (!entry || !entry->path) return false;
    const char* workspace = getWorkspacePath();
    if (!workspace || !workspace[0]) return false;
    size_t workspaceLen = strlen(workspace);
    if (strncmp(entry->path, workspace, workspaceLen) != 0) return false;
    const char* relative = entry->path + workspaceLen;
    if (*relative == '/' || *relative == '\\') relative++;
    return strncmp(relative, "BuildOutputs", strlen("BuildOutputs")) == 0;
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
    int baseX;
    int lineHeight;
    int indentWidth;
} ProjectRenderContext;

static void project_render_entry(ProjectRenderContext* ctx, DirEntry* entry, int depth) {
    const char* displayName = NULL;
    if (project_should_skip_entry(entry, &displayName)) return;

    float drawY = ctx->currentY - ctx->offset;
    ctx->currentY += (float)ctx->lineHeight;

    int indent = depth * ctx->indentWidth;
    int drawX = ctx->baseX + indent;

    bool insideViewport = (drawY >= ctx->viewportTop) &&
                          (drawY <= ctx->viewportBottom - ctx->lineHeight);

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

    int textWidth = getTextWidth(line);
    SDL_Rect box = {
        .x = drawX - 6,
        .y = (int)(drawY - 1.0f),
        .w = textWidth + 12,
        .h = ctx->lineHeight
    };

    if (insideViewport) {
        if (entry == selectedDirectory) {
            SDL_SetRenderDrawColor(ctx->renderer, 80, 160, 90, 120);
            SDL_RenderFillRect(ctx->renderer, &box);
        }

        if (entry == selectedFile) {
            SDL_SetRenderDrawColor(ctx->renderer, 70, 120, 200, 140);
            SDL_RenderFillRect(ctx->renderer, &box);
        }
    }

    bool entryInBuildOutputs = project_entry_in_build_outputs(entry);
    bool isRunTarget = (entryInBuildOutputs && runTargetPath[0] != '\0' &&
                        strcmp(entry->path, runTargetPath) == 0);
    bool isRunAncestor = false;
    if (!isRunTarget && entryInBuildOutputs && runTargetPath[0] != '\0') {
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

        drawText(drawX, (int)drawY, line);

        if (mouseY >= box.y && mouseY < box.y + box.h &&
            mouseX >= box.x && mouseX < box.x + box.w) {
            hoveredEntry = entry;
            hoveredEntryDepth = depth;
            SDL_SetRenderDrawColor(ctx->renderer, 180, 180, 180, 100);
            SDL_RenderDrawRect(ctx->renderer, &box);
            hoveredEntryRect = box;
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

    int x = pane->x + 12;
    const char* workspace = getWorkspacePath();
    char workspaceLabel[512];
    if (workspace && workspace[0]) {
        snprintf(workspaceLabel, sizeof(workspaceLabel), "Workspace: %s", workspace);
    } else {
        snprintf(workspaceLabel, sizeof(workspaceLabel), "Workspace: (sample project)");
    }
    drawClippedText(x, pane->y + 8, workspaceLabel, pane->w - 24);

    const char* runTarget = getRunTargetPath();
    char runLabel[512];
    if (runTarget && runTarget[0]) {
        snprintf(runLabel, sizeof(runLabel), "Run target: %s", runTarget);
    } else {
        snprintf(runLabel, sizeof(runLabel), "Run target: (auto-select on build)");
    }
    drawClippedText(x, pane->y + 22, runLabel, pane->w - 24);

    int y = pane->y + 42;
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

    int contentTop = y;
    SDL_Rect viewport = {
        .x = pane->x,
        .y = contentTop,
        .w = pane->w,
        .h = maxY - contentTop
    };
    if (viewport.h < 0) viewport.h = 0;

    PaneScrollState* scroll = project_get_scroll_state(pane);
    scroll_state_set_viewport(scroll, (float)viewport.h);

    int visibleLines = project_count_visible_entries(projectRoot);
    float contentHeight = (float)visibleLines * s_projectScrollConfig.line_height_px;
    float effectiveHeight = contentHeight;
    if (scroll->line_height_px > 0.0f && scroll->viewport_height_px > scroll->line_height_px) {
        float slack = scroll->viewport_height_px - scroll->line_height_px;
        effectiveHeight = contentHeight + slack;
    }
    scroll_state_set_content_height(scroll, effectiveHeight);

    pushClipRect(&viewport);

    if (projectRoot && visibleLines > 0) {
        ProjectRenderContext renderCtx = {
            .pane = pane,
            .renderer = renderer,
            .offset = scroll_state_get_offset(scroll),
            .currentY = (float)contentTop,
            .viewportTop = (float)viewport.y,
            .viewportBottom = (float)(viewport.y + viewport.h),
            .baseX = x,
            .lineHeight = (int)s_projectScrollConfig.line_height_px,
            .indentWidth = 20,
        };
        project_render_entry(&renderCtx, projectRoot, 0);
    } else if (viewport.h > 0) {
        drawText(x, contentTop, "(No project loaded)");
    }

    popClipRect();
}
