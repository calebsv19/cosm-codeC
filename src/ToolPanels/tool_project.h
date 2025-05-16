#ifndef TOOL_PROJECT_H
#define TOOL_PROJECT_H

#include "../pane.h"
#include "../Render/render_pipeline.h"
#include "../GlobalInfo/project.h"
#include <SDL2/SDL.h>


extern int mouseX;
extern int mouseY;
extern int hoveredEntryDepth; 
extern struct DirEntry* hoveredEntry; 
extern struct DirEntry* selectedEntry;



void updateHoveredMousePosition(int x, int y);
void handleProjectFilesClick(UIPane* pane, int clickX);



#endif

