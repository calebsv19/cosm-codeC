#include "button_scope.h"
#include "app/GlobalInfo/core_state.h"
#include <string.h>
static IDEButtonScope* scopes[24];
static uint32_t scope_count;
static void reset(IDEButtonScope* scope) {
    kit_ui_interaction_reset(&scope->interaction);
    scope->count = 0;
}
void ide_button_scope_sync(IDEButtonScope* scope, UIPane* pane,
    const KitUiInteractionControl* controls, uint32_t count,
    const void* domain_owner, const void* selection_owner, uint64_t domain_revision, bool eligible) {
    if (!scope) return;
    uint32_t index = 0;
    while (index < scope_count && scopes[index] != scope) ++index;
    if (index == scope_count) {
        if (scope_count == 24) { reset(scope); return; }
        scopes[scope_count++] = scope;
    }
    if (!pane || !pane->visible || !eligible || count > IDE_BUTTON_SCOPE_CAPACITY || (count && !controls)) {
        reset(scope); return;
    }
    KitUiInteractionControl clipped[IDE_BUTTON_SCOPE_CAPACITY] = {0};
    for (uint32_t i = 0; i < count; ++i) {
        clipped[i] = controls[i];
        SDL_Rect requested = {(int)controls[i].bounds.x, (int)controls[i].bounds.y,
            (int)controls[i].bounds.width, (int)controls[i].bounds.height};
        SDL_Rect bounds = {pane->x, pane->y, pane->w, pane->h}, visible = {0};
        SDL_IntersectRect(&requested, &bounds, &visible);
        clipped[i].bounds = (KitRenderRect){visible.x, visible.y, visible.w, visible.h};
        clipped[i].enabled = controls[i].enabled && visible.w > 0 && visible.h > 0;
    }
    SDL_Rect pane_bounds = {pane->x,pane->y,pane->w,pane->h};
    if (memcmp(&scope->pane_bounds,&pane_bounds,sizeof(pane_bounds)) || scope->pane != pane || scope->count != count || scope->domain_owner != domain_owner ||
        scope->selection_owner != selection_owner || scope->domain_revision != domain_revision || memcmp(scope->controls, clipped, count*sizeof(*clipped))) {
        kit_ui_interaction_reset(&scope->interaction);
    }
    scope->pane_bounds = pane_bounds;
    scope->pane = pane; scope->domain_owner = domain_owner; scope->selection_owner = selection_owner;
    scope->domain_revision = domain_revision;
    scope->count = count;
    memcpy(scope->controls, clipped, count*sizeof(*clipped));
}
bool ide_button_scope_event(IDEButtonScope* scope, const SDL_Event* event, uint32_t* activated) {
    if (activated) *activated = 0;
    if (!scope || !event || !scope->count) return false;
    KitUiInteractionEvent input;
    KitUiInteractionResult result = {0};
    if (!kit_ui_interaction_event_from_sdl(event, &input)) return false;
    if (kit_ui_interaction_route(&scope->interaction, scope->controls, scope->count,
                                 &input, &result).code != CORE_OK) { reset(scope); return false; }
    if (activated) *activated = result.activated_id;
    return result.consumed != 0;
}
KitUiButtonState ide_button_scope_state(IDEButtonScope* scope, uint32_t id, bool selected) {
    if (!scope) return (KitUiButtonState){.disabled = 1};
    for (uint32_t i = 0; i < scope->count; ++i) {
        if (scope->controls[i].id != id) continue;
        KitUiButtonState state = kit_ui_interaction_button_state(&scope->interaction, &scope->controls[i], selected);
        IDECoreState* core = getCoreState();
        if (!core || core->focusedPane != scope->pane) state.focused = 0;
        return state;
    }
    return (KitUiButtonState){.disabled = 1};
}
void ide_button_scopes_cancel_except(UIPane* pane) {
    for (uint32_t i = 0; i < scope_count; ++i) if (!pane || scopes[i]->pane != pane) reset(scopes[i]);
}
void ide_button_scopes_forget_pane(UIPane* pane) {
    for (uint32_t i=0;i<scope_count;++i) if (scopes[i]->pane==pane) {
        reset(scopes[i]); scopes[i]->pane=NULL;
    }
}
void ide_button_scopes_pointer_move(int x, int y) {
    SDL_Event event = {0}; event.type = SDL_MOUSEMOTION; event.motion.x = x; event.motion.y = y;
    for (uint32_t i = 0; i < scope_count; ++i) (void)ide_button_scope_event(scopes[i], &event, NULL);
}
