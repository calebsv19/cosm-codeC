#ifndef TOOL_PROJECT_H
#define TOOL_PROJECT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/project.h"
#include "ide/UI/panel_control_widgets.h"
#include <SDL2/SDL.h>

#define PROJECT_RENAME_BUFFER_CAP 256
#define PROJECT_PATH_BUFFER_CAP 1024

typedef enum ProjectTopControlId {
    PROJECT_TOP_CONTROL_NONE = 0,
    PROJECT_TOP_CONTROL_ADD_FILE = 1,
    PROJECT_TOP_CONTROL_DELETE_FILE = 2,
    PROJECT_TOP_CONTROL_ADD_FOLDER = 3,
    PROJECT_TOP_CONTROL_DELETE_FOLDER = 4
} ProjectTopControlId;

int project_mouse_x(void);
int project_mouse_y(void);
void project_set_mouse_position(int x, int y);

int* project_hovered_entry_depth_ptr(void);
struct DirEntry** project_hovered_entry_ptr(void);
struct DirEntry** project_selected_entry_ptr(void);
struct DirEntry** project_selected_file_ptr(void);
struct DirEntry** project_selected_directory_ptr(void);
SDL_Rect* project_hovered_entry_rect_ptr(void);
UIPanelTaggedRectList* project_top_control_hits(void);

struct DirEntry** project_renaming_entry_ptr(void);
char* project_rename_buffer(void);
int* project_rename_cursor_ptr(void);
char* project_newly_created_path_buffer(void);
char* project_run_target_path_buffer(void);
PaneScrollState* project_panel_scroll_state(void);
bool* project_panel_scroll_initialized_ptr(void);
SDL_Rect* project_panel_scroll_track_ptr(void);
SDL_Rect* project_panel_scroll_thumb_ptr(void);

#define hoveredEntryDepth (*project_hovered_entry_depth_ptr())
#define hoveredEntry (*project_hovered_entry_ptr())
#define selectedEntry (*project_selected_entry_ptr())
#define selectedFile (*project_selected_file_ptr())
#define selectedDirectory (*project_selected_directory_ptr())
#define hoveredEntryRect (*project_hovered_entry_rect_ptr())

#define renamingEntry (*project_renaming_entry_ptr())
#define renameBuffer (project_rename_buffer())
#define renameCursor (*project_rename_cursor_ptr())

void restoreRunTargetSelection(void);
void project_select_all_visible_entries(void);
bool project_copy_visible_entries_to_clipboard(void);
bool project_select_all_visual_active(void);
void project_clear_select_all_visual(void);
int project_get_rename_cursor(void);
void project_begin_inline_rename(struct DirEntry* entry);
void project_end_inline_rename(void);
bool project_handle_rename_text_input(const SDL_Event* event);
bool project_handle_rename_edit_key(SDL_Keycode key);


void updateHoveredMousePosition(int x, int y);
void handleProjectFilesClick(UIPane* pane, int clickX, int clickY);
void handleCommandOpenFileInEditor(struct DirEntry* entry);
void updateHoveredEditorDropTarget(int mouseX, int mouseY);
void resetProjectDragState(void);
void beginProjectDrag(struct DirEntry* entry, const SDL_Rect* rect, int mouseX, int mouseY);
void updateProjectDrag(int mouseX, int mouseY);
void finalizeProjectDrag(int mouseX, int mouseY);
void renderProjectDragOverlay(void);


DirEntry* getCurrentTargetDirectory(void);
void createFileInProject(DirEntry* parent, const char* name);
void createFolderInProject(DirEntry* parent, const char* name);
void deleteSelectedFile(void);
void deleteSelectedDirectory(void);
void refreshProjectDirectory(void);
void selectDirectoryEntry(struct DirEntry* entry);
void selectFileEntry(struct DirEntry* entry);

#endif
