#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "ide/UI/scroll_manager.h"

struct UITreeNode;
struct UIPane;

bool isLiveParseEnabled();
bool isShowInlineErrorsEnabled();
bool isShowAutoParamNamesEnabled();
bool isShowMacrosEnabled();

void toggleLiveParse();
void toggleShowInlineErrors();
void toggleShowAutoParamNames();
void toggleShowMacros();

struct DirEntry;
void control_panel_refresh_symbol_tree(const struct DirEntry* projectRoot,
                                       const char* filePath);
struct UITreeNode* control_panel_get_symbol_tree(void);
PaneScrollState* control_panel_get_symbol_scroll(void);
SDL_Rect* control_panel_get_symbol_scroll_track(void);
SDL_Rect* control_panel_get_symbol_scroll_thumb(void);
int control_panel_get_symbol_list_top(const struct UIPane* pane);
void control_panel_reset_symbol_tree(void);

#endif
