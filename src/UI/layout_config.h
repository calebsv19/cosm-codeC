#ifndef LAYOUT_CONFIG_H
#define LAYOUT_CONFIG_H

#define PANE_MENU_HEIGHT 40
#define PANE_ICON_WIDTH 60

typedef struct LayoutDimensions{
    int toolWidth;
    int controlWidth;
    int terminalHeight;
} LayoutDimensions;

LayoutDimensions* getLayoutDimensions(void);

#endif

