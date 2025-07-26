#ifndef TREE_RENDERER_H
#define TREE_RENDERER_H

#include "ide/UI/Trees/ui_tree_node.h"
#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>

// Draw a full UITreeNode hierarchy in a panel at x/y
void renderTreePanel(UIPane* pane, UITreeNode* root);

// Optionally expose mouse hover + selection tracking
UITreeNode* getHoveredTreeNode(void);
UITreeNode* getSelectedTreeNode(void);

// Pass mouse inputs to track hover/select states
void handleTreeMouseMove(int x, int y);
void handleTreeClick(UIPane* pane, int mouseX, int mouseY);

// Optionally customize visuals
void setTreeColorOverride(TreeNodeColor color, SDL_Color sdlColor); // optional (internal styling)

#endif

