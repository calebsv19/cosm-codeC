#ifndef TOOL_PROJECT_H
#define TOOL_PROJECT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/project.h"
#include <SDL2/SDL.h>


extern int mouseX;
extern int mouseY;
extern int hoveredEntryDepth; 
extern struct DirEntry* hoveredEntry; 
extern struct DirEntry* selectedEntry;
extern struct DirEntry* selectedFile;
extern struct DirEntry* selectedDirectory;
extern SDL_Rect hoveredEntryRect;

extern SDL_Rect projectBtnAddFile;
extern SDL_Rect projectBtnAddFolder;
extern SDL_Rect projectBtnDeleteFile;
extern SDL_Rect projectBtnDeleteFolder;

extern DirEntry* renamingEntry;
extern char renameBuffer[256];

extern char newlyCreatedPath[1024];
extern char runTargetPath[1024];

void restoreRunTargetSelection(void);


void updateHoveredMousePosition(int x, int y);
void handleProjectFilesClick(UIPane* pane, int clickX);
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
