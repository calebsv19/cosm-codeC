#ifndef TOOL_PROJECT_H
#define TOOL_PROJECT_H

#include "PaneInfo/pane.h"
#include "Render/render_pipeline.h"
#include "GlobalInfo/project.h"
#include <SDL2/SDL.h>


extern int mouseX;
extern int mouseY;
extern int hoveredEntryDepth; 
extern struct DirEntry* hoveredEntry; 
extern struct DirEntry* selectedEntry;

extern SDL_Rect projectBtnAddFile;
extern SDL_Rect projectBtnAddFolder;
extern SDL_Rect projectBtnDelete;

extern DirEntry* renamingEntry;
extern char renameBuffer[256];

extern char newlyCreatedPath[1024];


void updateHoveredMousePosition(int x, int y);
void handleProjectFilesClick(UIPane* pane, int clickX);


DirEntry* getCurrentTargetDirectory(void);
void createFileInProject(DirEntry* parent, const char* name);
void createFolderInProject(DirEntry* parent, const char* name);
void deleteSelectedEntry(void);
void refreshProjectDirectory(void);

#endif

