#include "tool_assets.h"
#include "app/GlobalInfo/project.h"

DirEntry* assetRoot = NULL;

void initAssetManagerPanel() {
    if (assetRoot) freeDirectory(assetRoot);
    assetRoot = scanDirectory("src/Assets", 0);
}


void handleAssetManagerEvent(UIPane* pane, SDL_Event* event) {
    // Future: handle asset click to preview
}

