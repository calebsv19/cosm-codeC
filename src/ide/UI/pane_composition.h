#ifndef IDE_PANE_COMPOSITION_H
#define IDE_PANE_COMPOSITION_H
#include "ide/Panes/PaneInfo/pane.h"
#include "kit_pane_composition.h"
/* Fixed IDE pane roles provide stable IDs; topology and editor leaves stay local. */
bool ide_pane_composition_build(KitPaneComposition* view, UIPane** panes, int count, int width, int height);
UIPane* ide_pane_composition_hit(const KitPaneComposition* view, UIPane** panes, int count, int x, int y);
bool ide_pane_composition_clip(const KitPaneComposition* view, UIPane* pane, SDL_Rect* clip);
#endif
