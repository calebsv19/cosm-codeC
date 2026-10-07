#ifndef IDE_BUTTON_SCOPE_H
#define IDE_BUTTON_SCOPE_H
#include "ide/Panes/PaneInfo/pane.h"
#include "kit_ui_interaction_sdl.h"
#define IDE_BUTTON_SCOPE_CAPACITY 64
/* App bridge only: shared KitUI owns button behavior; hosts own actions/eligibility. */
typedef struct IDEButtonScope {
    KitUiInteractionContext interaction;
    KitUiInteractionControl controls[IDE_BUTTON_SCOPE_CAPACITY];
    UIPane* pane;
    SDL_Rect pane_bounds;
    const void* domain_owner;
    const void* selection_owner;
    uint32_t count;
    uint64_t domain_revision;
} IDEButtonScope;
void ide_button_scope_sync(IDEButtonScope* scope, UIPane* pane,
    const KitUiInteractionControl* controls, uint32_t count,
    const void* domain_owner, const void* selection_owner, uint64_t domain_revision, bool eligible);
bool ide_button_scope_event(IDEButtonScope* scope, const SDL_Event* event, uint32_t* activated);
KitUiButtonState ide_button_scope_state(IDEButtonScope* scope, uint32_t id, bool selected);
void ide_button_scopes_cancel_except(UIPane* pane);
void ide_button_scopes_forget_pane(UIPane* pane);
void ide_button_scopes_pointer_move(int x, int y);
#endif
