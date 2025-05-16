#ifndef MENU_BUTTONS_H
#define MENU_BUTTONS_H

#include <SDL2/SDL.h>

#include "../Editor/editor_view.h" 
#include "pane.h"


// Logical groups of buttons
typedef enum {
    MENU_BUTTON_LOAD = 0,
    MENU_BUTTON_SAVE,
    MENU_BUTTON_LEFT_COUNT
} MenuButtonLeft;

typedef enum {
    MENU_BUTTON_BUILD = 0,
    MENU_BUTTON_RUN,
    MENU_BUTTON_DEBUG,
    MENU_BUTTON_CONTROL_TOGGLE,
    MENU_BUTTON_RIGHT_COUNT
} MenuButtonRight;


typedef enum {
    MENU_BAR_MODE_STANDARD,
    MENU_BAR_MODE_CENTER_FILENAME
} MenuBarLayoutMode;

// Set default externally
extern MenuBarLayoutMode currentMenuBarLayout;





#define LEFT_BUTTON_WIDTH 60
#define RIGHT_BUTTON_WIDTH 60
#define BUTTON_HEIGHT_PADDING 16
#define BUTTON_SPACING 8
#define FILE_LABEL_WIDTH 80
#define LEFT_START_X(pane) ((pane)->x + 8 + FILE_LABEL_WIDTH)
    
    

SDL_Rect getLeftMenuButtonRect(UIPane* pane, int index);
SDL_Rect getRightMenuButtonRect(UIPane* pane, int index);


// API for Command Layer
void handleCommandBuild(void);
void handleCommandRun(void);
void handleCommandDebug(void);
void handleCommandToggleControlPanel(void);
void handleCommandLoad(void);
void handleCommandSave(void);

const char* getActiveFileName(void);


#endif

