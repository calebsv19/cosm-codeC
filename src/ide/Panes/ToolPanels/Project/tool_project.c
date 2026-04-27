
#include "tool_project.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/editor_navigation.h"
#include "core/FileIO/file_ops.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/Clipboard/clipboard.h"
#include "core/LoopTime/loop_time.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/panel_text_edit.h"
#include "ide/UI/row_activation.h"
#include "tool_project_run_target_helpers.h"
#include "tool_project_tree_snapshot_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // for remove()
#include <dirent.h>
#include <errno.h>

typedef struct {
    char path[PROJECT_PATH_BUFFER_CAP];
    bool isExpanded;
} DirState;

typedef struct {
    bool select_all_visible;
    UIDoubleClickTracker double_click_tracker;
    int hovered_entry_depth;
    DirEntry* hovered_entry;
    DirEntry* selected_entry;
    DirEntry* selected_file;
    DirEntry* selected_directory;
    SDL_Rect hovered_entry_rect;
    int mouse_x;
    int mouse_y;
    UIPanelTaggedRect top_control_hit_storage[4];
    UIPanelTaggedRectList top_control_hits;
    DirEntry* renaming_entry;
    char rename_buffer[PROJECT_RENAME_BUFFER_CAP];
    int rename_cursor;
    char newly_created_path[PROJECT_PATH_BUFFER_CAP];
    char run_target_path[PROJECT_PATH_BUFFER_CAP];
    char selected_file_path[PROJECT_PATH_BUFFER_CAP];
    char selected_directory_path[PROJECT_PATH_BUFFER_CAP];
    PaneScrollState scroll;
    bool scroll_initialized;
    SDL_Rect scroll_track;
    SDL_Rect scroll_thumb;
    DirState temp_dir_states[2048];
    int temp_dir_state_count;
} ProjectPanelState;

static ProjectPanelState g_projectPanelBootstrapState = {0};
static bool g_projectPanelBootstrapInitialized = false;

static void project_panel_bind_state(ProjectPanelState* state) {
    if (!state) return;
    state->top_control_hits.items = state->top_control_hit_storage;
    state->top_control_hits.count = 0;
    state->top_control_hits.capacity =
        (int)(sizeof(state->top_control_hit_storage) / sizeof(state->top_control_hit_storage[0]));
}

static void project_panel_init_state(void* ptr) {
    ProjectPanelState* state = (ProjectPanelState*)ptr;
    if (!state) return;
    memset(state, 0, sizeof(*state));
    project_panel_bind_state(state);
}

static void project_panel_destroy_state(void* ptr) {
    ProjectPanelState* state = (ProjectPanelState*)ptr;
    if (!state) return;
    free(state);
}

static ProjectPanelState* project_panel_state_internal(void) {
    return (ProjectPanelState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_PROJECT,
        sizeof(ProjectPanelState),
        project_panel_init_state,
        project_panel_destroy_state,
        &g_projectPanelBootstrapState,
        &g_projectPanelBootstrapInitialized
    );
}

static UIPanelTextEditBuffer project_rename_text_edit_buffer(void) {
    ProjectPanelState* state = project_panel_state_internal();
    return (UIPanelTextEditBuffer){
        .text = state ? state->rename_buffer : NULL,
        .capacity = PROJECT_RENAME_BUFFER_CAP,
        .cursor = state ? &state->rename_cursor : NULL
    };
}

int project_mouse_x(void) { return project_panel_state_internal()->mouse_x; }
int project_mouse_y(void) { return project_panel_state_internal()->mouse_y; }

void project_set_mouse_position(int x, int y) {
    ProjectPanelState* state = project_panel_state_internal();
    state->mouse_x = x;
    state->mouse_y = y;
}

int* project_hovered_entry_depth_ptr(void) { return &project_panel_state_internal()->hovered_entry_depth; }
DirEntry** project_hovered_entry_ptr(void) { return &project_panel_state_internal()->hovered_entry; }
DirEntry** project_selected_entry_ptr(void) { return &project_panel_state_internal()->selected_entry; }
DirEntry** project_selected_file_ptr(void) { return &project_panel_state_internal()->selected_file; }
DirEntry** project_selected_directory_ptr(void) { return &project_panel_state_internal()->selected_directory; }
SDL_Rect* project_hovered_entry_rect_ptr(void) { return &project_panel_state_internal()->hovered_entry_rect; }
UIPanelTaggedRectList* project_top_control_hits(void) { return &project_panel_state_internal()->top_control_hits; }
DirEntry** project_renaming_entry_ptr(void) { return &project_panel_state_internal()->renaming_entry; }
char* project_rename_buffer(void) { return project_panel_state_internal()->rename_buffer; }
int* project_rename_cursor_ptr(void) { return &project_panel_state_internal()->rename_cursor; }
char* project_newly_created_path_buffer(void) { return project_panel_state_internal()->newly_created_path; }
char* project_run_target_path_buffer(void) { return project_panel_state_internal()->run_target_path; }
PaneScrollState* project_panel_scroll_state(void) { return &project_panel_state_internal()->scroll; }
bool* project_panel_scroll_initialized_ptr(void) { return &project_panel_state_internal()->scroll_initialized; }
SDL_Rect* project_panel_scroll_track_ptr(void) { return &project_panel_state_internal()->scroll_track; }
SDL_Rect* project_panel_scroll_thumb_ptr(void) { return &project_panel_state_internal()->scroll_thumb; }

static void clearRunTargetSelection(void);
static void updateRunTargetSelection(DirEntry* entry);
static DirEntry* findEntryByPath(DirEntry* root, const char* targetPath);

static void setSelectedDirectory(DirEntry* entry) {
    ProjectPanelState* state = project_panel_state_internal();
    selectedDirectory = entry;
    if (entry && entry->path) {
        strncpy(state->selected_directory_path, entry->path, sizeof(state->selected_directory_path));
        state->selected_directory_path[sizeof(state->selected_directory_path) - 1] = '\0';
    } else {
        state->selected_directory_path[0] = '\0';
    }
}

static void clearRunTargetSelection(void) {
    char* runTargetPath = project_run_target_path_buffer();
    project_run_target_clear(runTargetPath, PROJECT_PATH_BUFFER_CAP);
}

static void setSelectedFile(DirEntry* entry) {
    ProjectPanelState* state = project_panel_state_internal();
    selectedFile = entry;
    if (entry && entry->path) {
        strncpy(state->selected_file_path, entry->path, sizeof(state->selected_file_path));
        state->selected_file_path[sizeof(state->selected_file_path) - 1] = '\0';
    } else {
        state->selected_file_path[0] = '\0';
    }
}

void project_select_all_visible_entries(void) {
    project_panel_state_internal()->select_all_visible = true;
}

bool project_select_all_visual_active(void) {
    return project_panel_state_internal()->select_all_visible;
}

void project_clear_select_all_visual(void) {
    project_panel_state_internal()->select_all_visible = false;
}

bool project_copy_visible_entries_to_clipboard(void) {
    ProjectPanelState* state = project_panel_state_internal();
    if (!projectRoot) return false;
    if (!state->select_all_visible && hoveredEntry) {
        const char* name = project_tree_display_name(hoveredEntry);
        char line[1024];
        if (hoveredEntry->type == ENTRY_FOLDER) {
            snprintf(line, sizeof(line), "%s%s",
                     hoveredEntry->isExpanded ? "[-] " : "[+] ",
                     name ? name : "");
        } else {
            snprintf(line, sizeof(line), "%s", name ? name : "");
        }
        return clipboard_copy_text(line);
    }

    char* snapshot = NULL;
    size_t snapshot_len = 0;
    if (!project_tree_build_visible_snapshot(projectRoot, &snapshot, &snapshot_len) || snapshot_len == 0) {
        free(snapshot);
        return false;
    }
    state->select_all_visible = false;
    bool ok = clipboard_copy_text(snapshot);
    free(snapshot);
    return ok;
}

int project_get_rename_cursor(void) {
    UIPanelTextEditBuffer buffer = project_rename_text_edit_buffer();
    return ui_panel_text_edit_clamp_cursor(&buffer);
}

void project_begin_inline_rename(DirEntry* entry) {
    renamingEntry = entry;
    UIPanelTextEditBuffer buffer = project_rename_text_edit_buffer();
    (void)ui_panel_text_edit_set_text(&buffer, (entry && entry->name) ? entry->name : "");
}

void project_end_inline_rename(void) {
    renamingEntry = NULL;
    renameCursor = 0;
}

bool project_handle_rename_text_input(const SDL_Event* event) {
    UIPanelTextEditBuffer buffer = project_rename_text_edit_buffer();
    return ui_panel_text_edit_handle_text_input(&buffer, event);
}

bool project_handle_rename_edit_key(SDL_Keycode key) {
    UIPanelTextEditBuffer buffer = project_rename_text_edit_buffer();
    return ui_panel_text_edit_handle_keydown(&buffer, key);
}

void selectDirectoryEntry(DirEntry* entry) {
    setSelectedDirectory(entry);
    selectedEntry = entry;
    setSelectedFile(NULL);
    if (entry) {
        updateRunTargetSelection(entry);
    }
}

void selectFileEntry(DirEntry* entry) {
    setSelectedFile(entry);
    if (entry) {
        selectedEntry = entry;
        if ((!selectedDirectory || selectedDirectory == projectRoot) && entry->parent) {
            setSelectedDirectory(entry->parent);
        }
        updateRunTargetSelection(entry);
    }
}

static void updateRunTargetSelection(DirEntry* entry) {
    char* runTargetPath = project_run_target_path_buffer();
    project_run_target_update_from_entry(entry, runTargetPath, PROJECT_PATH_BUFFER_CAP);
}

void restoreRunTargetSelection(void) {
    char* runTargetPath = project_run_target_path_buffer();
    const char* saved = getRunTargetPath();
    if (!saved || !saved[0]) {
        runTargetPath[0] = '\0';
        return;
    }

    DirEntry* entry = findEntryByPath(projectRoot, saved);
    if (entry) {
        snprintf(runTargetPath, sizeof(runTargetPath), "%s", entry->path);
    } else {
        clearRunTargetSelection();
    }
}

static void clearProjectDirectoryDropTarget(ProjectDragState* drag) {
    if (!drag) return;
    drag->targetDirectory = NULL;
    drag->validDirectoryTarget = false;
    drag->targetDirectoryRect = (SDL_Rect){0};
}

static void setProjectDirectoryDropTarget(ProjectDragState* drag) {
    if (!drag) return;
    drag->targetDirectory = NULL;
    drag->validDirectoryTarget = false;
    drag->targetDirectoryRect = (SDL_Rect){0};
    if (!drag->active || !drag->entry || drag->entry->type != ENTRY_FILE) return;
    if (!hoveredEntry || hoveredEntry->type != ENTRY_FOLDER) return;
    if (!hoveredEntry->path || !drag->entry->name) return;
    if (hoveredEntry == drag->entry->parent) return;

    char destPath[1024];
    snprintf(destPath, sizeof(destPath), "%s/%s", hoveredEntry->path, drag->entry->name);
    struct stat st;
    if (stat(destPath, &st) == 0) return;

    drag->targetDirectory = hoveredEntry;
    drag->validDirectoryTarget = true;
    drag->targetDirectoryRect = hoveredEntryRect;
}

static bool moveFileEntryToDirectory(DirEntry* fileEntry, DirEntry* destDir) {
    char* newlyCreatedPath = project_newly_created_path_buffer();
    if (!fileEntry || fileEntry->type != ENTRY_FILE || !fileEntry->path || !fileEntry->name) return false;
    if (!destDir || destDir->type != ENTRY_FOLDER || !destDir->path) return false;

    char destPath[1024];
    snprintf(destPath, sizeof(destPath), "%s/%s", destDir->path, fileEntry->name);
    if (strcmp(fileEntry->path, destPath) == 0) return false;
    if (!renameFileOnDisk(fileEntry->path, destPath)) return false;

    fileEntry->parent = destDir;
    free(fileEntry->path);
    fileEntry->path = strdup(destPath);

    destDir->isExpanded = true;
    strncpy(newlyCreatedPath, destPath, PROJECT_PATH_BUFFER_CAP);
    newlyCreatedPath[PROJECT_PATH_BUFFER_CAP - 1] = '\0';
    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
    return true;
}

void resetProjectDragState(void) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    drag->entry = NULL;
    drag->active = false;
    drag->validTarget = false;
    drag->targetView = NULL;
    clearProjectDirectoryDropTarget(drag);
    drag->startX = drag->startY = 0;
    drag->currentX = drag->currentY = 0;
    drag->offsetX = drag->offsetY = 0;
    drag->labelWidth = 0;
    drag->cachedLabel[0] = '\0';
    drag->startTicks = 0;
    setHoveredEditorView(NULL);
}

static void setEditorDropTarget(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry) {
        drag->validTarget = false;
        drag->targetView = NULL;
        setHoveredEditorView(NULL);
        return;
    }

    EditorView* hovered = hitTestLeaf(core->editorViewState, mouseX, mouseY);
    drag->targetView = hovered;
    drag->validTarget = (hovered && hovered->type == VIEW_LEAF);

    setHoveredEditorView(drag->validTarget ? hovered : NULL);
}

void updateHoveredEditorDropTarget(int mouseX, int mouseY) {
    setEditorDropTarget(mouseX, mouseY);
}

void beginProjectDrag(DirEntry* entry, const SDL_Rect* rect, int mouseX, int mouseY) {
    if (!entry || !rect) return;
    resetProjectDragState();
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    drag->entry = entry;
    drag->active = false;
    drag->validTarget = false;
    drag->targetView = NULL;
    drag->startX = mouseX;
    drag->startY = mouseY;
    drag->offsetX = mouseX - rect->x;
    drag->offsetY = mouseY - rect->y;
    if (rect->w == 0 && rect->h == 0) {
        drag->offsetX = 10;
        drag->offsetY = 10;
    }
    drag->currentX = mouseX;
    drag->currentY = mouseY;
    drag->cachedLabel[0] = '\0';
    drag->labelWidth = 0;
    drag->startTicks = loop_time_now_ms32();
}

void updateProjectDrag(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry) return;

    drag->currentX = mouseX;
    drag->currentY = mouseY;

    if (!drag->active) {
        int dx = abs(mouseX - drag->startX);
        int dy = abs(mouseY - drag->startY);
        if (dx > 5 || dy > 5) {
            drag->active = true;
        } else {
            return;
        }
    }

    setEditorDropTarget(mouseX, mouseY);
    setProjectDirectoryDropTarget(drag);
}

void finalizeProjectDrag(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry) {
        resetProjectDragState();
        return;
    }

    drag->currentX = mouseX;
    drag->currentY = mouseY;

    if (drag->active) {
        setEditorDropTarget(mouseX, mouseY);
        setProjectDirectoryDropTarget(drag);
        if (drag->validTarget && drag->targetView) {
            setActiveEditorView(drag->targetView);
            handleCommandOpenFileInEditor(drag->entry);
        } else if (drag->validDirectoryTarget && drag->targetDirectory) {
            moveFileEntryToDirectory(drag->entry, drag->targetDirectory);
        }
    }

    resetProjectDragState();
}

void renderProjectDragOverlay(void) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry || !drag->active) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    if (!drag->cachedLabel[0]) {
        const char* base = strrchr(drag->entry->path, '/');
        base = base ? base + 1 : drag->entry->path;
        strncpy(drag->cachedLabel, base, sizeof(drag->cachedLabel) - 1);
        drag->cachedLabel[sizeof(drag->cachedLabel) - 1] = '\0';
        drag->labelWidth = getTextWidth(drag->cachedLabel);
    }

    SDL_Rect dragRect = {
        .x = drag->currentX - drag->offsetX,
        .y = drag->currentY - drag->offsetY,
        .w = drag->labelWidth + 24,
        .h = 28
    };

    SDL_Rect shadowRect = dragRect;
    shadowRect.x += 2;
    shadowRect.y += 3;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 110);
    SDL_RenderFillRect(renderer, &shadowRect);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 235);
    SDL_RenderFillRect(renderer, &dragRect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &dragRect);
    drawText(dragRect.x + 10, dragRect.y + 6, drag->cachedLabel);
}


bool pendingProjectRefresh = false;
unsigned int pendingProjectRefreshReasonMask = 0;

void queueProjectRefresh(unsigned int analysisReasonMask) {
    pendingProjectRefresh = true;
    pendingProjectRefreshReasonMask |= analysisReasonMask;
}



// 		INITS
// ==============================================
//              HANDLE ...


static void handleCommandFolderClick(DirEntry* entry, bool clickedPrefix, bool isDoubleClick) {
    if (clickedPrefix || isDoubleClick) {
        entry->isExpanded = !entry->isExpanded;
    }
}

static bool project_row_click_hits_prefix(UIPane* pane, int depth, int click_x) {
    if (!pane) return false;
    int indent = depth * ide_ui_tree_indent_width();
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    int drawX = pane->x + d.pad_left + indent;
    int prefixWidth = getTextWidth("[-] ");
    return (click_x >= drawX && click_x <= drawX + prefixWidth);
}

typedef struct ProjectRowActivationState {
    DirEntry* entry;
    int click_x;
    int click_y;
} ProjectRowActivationState;

static void project_row_select_single(void* user_data) {
    ProjectRowActivationState* state = (ProjectRowActivationState*)user_data;
    if (!state || !state->entry) return;

    if (state->entry->type == ENTRY_FOLDER) {
        selectDirectoryEntry(state->entry);
        snprintf(renameBuffer, PROJECT_RENAME_BUFFER_CAP, "%s", state->entry->name ? state->entry->name : "");
    } else if (state->entry->type == ENTRY_FILE) {
        selectFileEntry(state->entry);
    }
}

static void project_row_prefix_action(void* user_data) {
    ProjectRowActivationState* state = (ProjectRowActivationState*)user_data;
    if (!state || !state->entry || state->entry->type != ENTRY_FOLDER) return;
    handleCommandFolderClick(state->entry, true, false);
}

static void project_row_activate(void* user_data) {
    ProjectRowActivationState* state = (ProjectRowActivationState*)user_data;
    if (!state || !state->entry) return;

    if (state->entry->type == ENTRY_FOLDER) {
        handleCommandFolderClick(state->entry, false, true);
    } else if (state->entry->type == ENTRY_FILE) {
        if (state->entry->path) {
            handleCommandOpenFileInEditor(state->entry);
        } else {
            fprintf(stderr, "[WARN] hoveredEntry has null path!\n");
        }
    }
}

static void project_row_drag_start(void* user_data) {
    ProjectRowActivationState* state = (ProjectRowActivationState*)user_data;
    if (!state || !state->entry || state->entry->type != ENTRY_FILE) return;
    beginProjectDrag(state->entry, &hoveredEntryRect, state->click_x, state->click_y);
}

void handleCommandOpenFileInEditor(DirEntry* entry) {
    if (!ui_open_path_in_active_editor(entry ? entry->path : NULL)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "[Project] No active editor view to open %s",
                    (entry && entry->path) ? entry->path : "(null)");
    }
}

void handleProjectFilesClick(UIPane* pane, int clickX, int clickY) {
    if (!hoveredEntry) return;

    bool clickedPrefix = project_row_click_hits_prefix(pane, hoveredEntryDepth, clickX);
    ProjectRowActivationState activation = {
        .entry = hoveredEntry,
        .click_x = clickX,
        .click_y = clickY
    };

    (void)ui_row_activation_handle_primary(
        &(UIRowActivationContext){
            .double_click_tracker = &project_panel_state_internal()->double_click_tracker,
            .row_identity = (uintptr_t)hoveredEntry,
            .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
            .clicked_prefix = clickedPrefix,
            .additive_modifier = false,
            .range_modifier = false,
            .wants_drag_start = (hoveredEntry->type == ENTRY_FILE),
            .on_select_single = project_row_select_single,
            .on_prefix = project_row_prefix_action,
            .on_activate = project_row_activate,
            .on_drag_start = project_row_drag_start,
            .user_data = &activation
        });
}





//		HANDLE ...
// ==============================================
// 		CREATE/DESTROY



void createFileInProject(DirEntry* parent, const char* name) {
    char* newlyCreatedPath = project_newly_created_path_buffer();
    if (!parent || !name) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", parent->path, name);

    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[ProjectPanel] Failed to create file: %s\n", path);
        return;
    }

    // Write a newline to ensure it's non-empty (optional)
    fputs("\n", f);
    fflush(f);

    // Force write to disk to avoid caching delay before directory rescan
    fsync(fileno(f));
    fclose(f);

    printf("[ProjectPanel] Created file: %s\n", path);
    parent->isExpanded = true;

    strncpy(newlyCreatedPath, path, PROJECT_PATH_BUFFER_CAP);
    newlyCreatedPath[PROJECT_PATH_BUFFER_CAP - 1] = '\0';
}


void createFolderInProject(DirEntry* parent, const char* name) {
    char* newlyCreatedPath = project_newly_created_path_buffer();
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", parent->path, name);

    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, "[ProjectPanel] Failed to create folder: %s\n", path);
        return;
    }

    printf("[ProjectPanel] Created folder: %s\n", path);
    parent->isExpanded = true;

    strncpy(newlyCreatedPath, path, PROJECT_PATH_BUFFER_CAP);
    newlyCreatedPath[PROJECT_PATH_BUFFER_CAP - 1] = '\0';
}



static bool deletePathRecursive(const char* path) {
    if (!path) return false;

    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "[Delete] Failed to stat %s: %s\n", path, strerror(errno));
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(path);
        if (!dir) {
            fprintf(stderr, "[Delete] Failed to open dir %s: %s\n", path, strerror(errno));
            return false;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char childPath[1024];
            snprintf(childPath, sizeof(childPath), "%s/%s", path, entry->d_name);
            if (!deletePathRecursive(childPath)) {
                closedir(dir);
                return false;
            }
        }

        closedir(dir);
        if (rmdir(path) != 0) {
            fprintf(stderr, "[Delete] Failed to remove dir %s: %s\n", path, strerror(errno));
            return false;
        }
        return true;
    }

    if (remove(path) != 0) {
        fprintf(stderr, "[Delete] Failed to delete %s: %s\n", path, strerror(errno));
        return false;
    }
    return true;
}

static void closeFilesRecursive(EditorView* view, DirEntry* entry) {
    if (!view || !entry) return;

    if (entry->type == ENTRY_FILE) {
        if (entry->path) {
            closeFileInAllViews(view, entry->path);
        }
        return;
    }

    for (int i = 0; i < entry->childCount; ++i) {
        closeFilesRecursive(view, entry->children[i]);
    }
}

void deleteSelectedFile(void) {
    char* runTargetPath = project_run_target_path_buffer();
    if (!selectedFile || !selectedFile->path) return;

    DirEntry* fileEntry = selectedFile;
    DirEntry* parent = fileEntry->parent;

    if (remove(fileEntry->path) != 0) {
        fprintf(stderr, "[Delete] Failed to delete file: %s (%s)\n", fileEntry->path, strerror(errno));
        return;
    }

    if (runTargetPath[0] && strcmp(runTargetPath, fileEntry->path) == 0) {
        clearRunTargetSelection();
    }

    IDECoreState* core = getCoreState();
    if (core->persistentEditorView) {
        closeFileInAllViews(core->persistentEditorView, fileEntry->path);
    }

    setSelectedFile(NULL);
    if (parent) {
        selectDirectoryEntry(parent);
    } else {
        selectedEntry = NULL;
    }

    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
}

void deleteSelectedDirectory(void) {
    char* runTargetPath = project_run_target_path_buffer();
    if (!selectedDirectory || selectedDirectory == projectRoot || !selectedDirectory->path) {
        fprintf(stderr, "[Delete] No deletable directory selected.\n");
        return;
    }

    DirEntry* dirEntry = selectedDirectory;
    DirEntry* parent = dirEntry->parent;

    IDECoreState* core = getCoreState();
    if (core->persistentEditorView) {
        closeFilesRecursive(core->persistentEditorView, dirEntry);
    }

    if (!deletePathRecursive(dirEntry->path)) {
        fprintf(stderr, "[Delete] Failed to remove directory tree: %s\n", dirEntry->path);
        return;
    }

    if (runTargetPath[0]) {
        size_t dirLen = strlen(dirEntry->path);
        if (strncmp(runTargetPath, dirEntry->path, dirLen) == 0) {
            char next = runTargetPath[dirLen];
            if (next == '\0' || next == '/' || next == '\\') {
                clearRunTargetSelection();
            }
        }
    }

    setSelectedFile(NULL);
    if (parent) {
        selectDirectoryEntry(parent);
    } else {
        selectDirectoryEntry(projectRoot);
    }

    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
}





//              CREATE/DESTROY   
//   ===================================================
// 		REFRESHING PROJECT







static void cacheExpandedStateRecursive(DirEntry* entry) {
    ProjectPanelState* state = project_panel_state_internal();
    if (entry->type != ENTRY_FOLDER) return;

    if (state->temp_dir_state_count < 2048) {
        strncpy(state->temp_dir_states[state->temp_dir_state_count].path,
                entry->path,
                sizeof(state->temp_dir_states[0].path));
        state->temp_dir_states[state->temp_dir_state_count].path[sizeof(state->temp_dir_states[0].path) - 1] = '\0';
        state->temp_dir_states[state->temp_dir_state_count].isExpanded = entry->isExpanded;
        state->temp_dir_state_count++;
    }

    for (int i = 0; i < entry->childCount; i++) {
        cacheExpandedStateRecursive(entry->children[i]);
    }
}

static bool findCachedExpandedState(const char* path) {
    ProjectPanelState* state = project_panel_state_internal();
    for (int i = 0; i < state->temp_dir_state_count; i++) {
        if (strcmp(state->temp_dir_states[i].path, path) == 0) {
            return state->temp_dir_states[i].isExpanded;
        }
    }
    return false;
}


static DirEntry* findEntryByPath(DirEntry* root, const char* targetPath) {
    if (!root || !targetPath || strlen(targetPath) == 0) {
        fprintf(stderr, "[findEntryByPath] Invalid inputs — root or path was NULL or empty.\n");
        return NULL;
    }

    if (!root->path) {
        fprintf(stderr, "[findEntryByPath] Root DirEntry has null path (corrupted entry?)\n");
        return NULL;
    }

    if (strcmp(root->path, targetPath) == 0) {
        return root;
    }

    for (int i = 0; i < root->childCount; i++) {
        DirEntry* found = findEntryByPath(root->children[i], targetPath);
        if (found) return found;
    }

    return NULL;
}


static void restoreExpandedStateRecursive(DirEntry* entry) {
    if (entry->type == ENTRY_FOLDER) {
        if (entry->parent == NULL) {
            entry->isExpanded = true;
        } else {
            entry->isExpanded = findCachedExpandedState(entry->path);
        }
        for (int i = 0; i < entry->childCount; i++) {
            restoreExpandedStateRecursive(entry->children[i]);
        }
    }
}


void refreshProjectDirectory(void) {
    ProjectPanelState* state = project_panel_state_internal();
    char* newlyCreatedPath = project_newly_created_path_buffer();
    state->temp_dir_state_count = 0;

    // Invalidate all UI pointers before refreshing
    selectedEntry = NULL;
    selectedFile = NULL;
    selectedDirectory = NULL;
    hoveredEntry = NULL;
    renamingEntry = NULL;

    if (projectRoot) {
        cacheExpandedStateRecursive(projectRoot);
        freeDirectory(projectRoot);
    }

    projectRoot = loadProjectDirectory(projectPath);
    restoreExpandedStateRecursive(projectRoot);

    // === Restore selections ===
    if (newlyCreatedPath[0] != '\0') {
        DirEntry* restored = findEntryByPath(projectRoot, newlyCreatedPath);
        if (restored) {
            if (restored->type == ENTRY_FOLDER) {
                setSelectedDirectory(restored);
            } else {
                setSelectedFile(restored);
                if (restored->parent) setSelectedDirectory(restored->parent);
            }
            selectedEntry = restored;
        } else {
            fprintf(stderr, "[WARN] Could not restore created entry: %s\n", newlyCreatedPath);
        }
        newlyCreatedPath[0] = '\0';
    } else {
        if (state->selected_file_path[0] != '\0') {
            DirEntry* restoredFile = findEntryByPath(projectRoot, state->selected_file_path);
            if (restoredFile && restoredFile->type == ENTRY_FILE) {
                setSelectedFile(restoredFile);
            } else {
                state->selected_file_path[0] = '\0';
            }
        }

        if (state->selected_directory_path[0] != '\0') {
            DirEntry* restoredDir = findEntryByPath(projectRoot, state->selected_directory_path);
            if (restoredDir && restoredDir->type == ENTRY_FOLDER) {
                setSelectedDirectory(restoredDir);
            } else {
                state->selected_directory_path[0] = '\0';
            }
        }

        if (!selectedDirectory && projectRoot) {
            setSelectedDirectory(projectRoot);
        }

        if (selectedFile) {
            selectedEntry = selectedFile;
        } else {
            selectedEntry = selectedDirectory;
        }
    }

    // Do not restart terminals here; we want build/run sessions to keep their scrollback.
    // If we ever need to start a shell on first load, do it from initTerminal instead.

    restoreRunTargetSelection();
}





//              REFRESHING PROJECT
// ==============================================
// 		HELPERS





void updateHoveredMousePosition(int x, int y) {
    project_set_mouse_position(x, y);
}
         



DirEntry* getCurrentTargetDirectory(void) {
    if (selectedDirectory) {
        return selectedDirectory;
    }

    if (selectedFile && selectedFile->parent) {
        return selectedFile->parent;
    }

    return projectRoot;
}
