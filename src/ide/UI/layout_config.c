#include "layout_config.h"

static LayoutDimensions globalLayout = {
    .toolWidth = 250,
    .controlWidth = 250,
    .terminalHeight = 160
};

LayoutDimensions* getLayoutDimensions(void) {
    return &globalLayout;
}

