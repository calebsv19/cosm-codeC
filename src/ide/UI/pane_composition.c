#include "pane_composition.h"
static CorePaneId pane_id(const UIPane* pane) { return pane ? (CorePaneId)pane->role+1 : 0; }
bool ide_pane_composition_build(KitPaneComposition* view, UIPane** panes, int count, int width, int height) {
    if (!view || !panes || count < 0 || count > KIT_PANE_COMPOSITION_MAX || width < 0 || height < 0) return false;
    KitPaneCompositionSpec specs[KIT_PANE_COMPOSITION_MAX] = {0};
    uint32_t visible = 0;
    for (int i = 0; i < count; ++i) {
        UIPane* pane = panes[i];
        if (!pane || !pane->visible) continue;
        specs[visible++] = (KitPaneCompositionSpec){.id = pane_id(pane),
            .bounds = {pane->x,pane->y,pane->w,pane->h}, .enabled = 1};
    }
    return kit_pane_composition_build(view,specs,visible,(CorePaneRect){0,0,width,height}).code == CORE_OK;
}
UIPane* ide_pane_composition_hit(const KitPaneComposition* view, UIPane** panes, int count, int x, int y) {
    CorePaneId id = kit_pane_composition_hit(view,x,y).id;
    for (int i = 0; id && i < count; ++i) if (panes[i] && pane_id(panes[i]) == id) return panes[i];
    return NULL;
}
bool ide_pane_composition_clip(const KitPaneComposition* view, UIPane* pane, SDL_Rect* clip) {
    const KitPaneCompositionEntry* entry = kit_pane_composition_find(view,pane_id(pane));
    if (!entry || !clip || entry->visible_content.width <= 0 || entry->visible_content.height <= 0) return false;
    *clip = (SDL_Rect){entry->visible_content.x,entry->visible_content.y,
                      entry->visible_content.width,entry->visible_content.height};
    return true;
}
