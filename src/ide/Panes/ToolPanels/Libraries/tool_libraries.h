#ifndef TOOL_LIBRARIES_H
#define TOOL_LIBRARIES_H

#include "ide/Panes/PaneInfo/pane.h"
#include "app/GlobalInfo/project.h" 
#include <SDL2/SDL.h>


extern DirEntry* libraryRoot;
extern DirEntry* hoveredLibraryEntry;
extern DirEntry* selectedLibraryEntry;
extern int hoveredLibraryDepth;
extern int libraryMouseX;
extern int libraryMouseY;


void initLibrariesPanel(void);

void handleLibraryEntryClick(UIPane* pane, int clickX);
void updateHoveredLibraryMousePosition(int x, int y);


#endif

