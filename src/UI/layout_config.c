#include "layout_config.h"

static LayoutDimensions globalLayout = {
    .toolWidth = 150,
    .controlWidth = 150,
    .terminalHeight = 160
};

LayoutDimensions* getLayoutDimensions(void) {
    return &globalLayout;
}

