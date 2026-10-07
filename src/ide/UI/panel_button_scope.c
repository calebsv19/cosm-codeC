#include "panel_button_scope.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "core/Ipc/ide_ipc_workspace.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include <string.h>
static IDEButtonScope scopes[IDE_PANEL_BUTTON_GROUP_COUNT];
static bool field_present[IDE_PANEL_BUTTON_GROUP_COUNT];
static uint64_t purpose_generations[IDE_PANEL_BUTTON_GROUP_COUNT];
typedef struct HeaderDomain { const void* owner; uint64_t revision; } HeaderDomain;
static HeaderDomain domains[IDE_PANEL_BUTTON_GROUP_COUNT], painted_domains[IDE_PANEL_BUTTON_GROUP_COUNT];
void ide_panel_buttons_domain(IDEPanelButtonGroup group, UIPane* pane, const void* owner, uint64_t revision) {
    if (group<0 || group>=IDE_PANEL_BUTTON_GROUP_COUNT) return;
    domains[group]=(HeaderDomain){owner,revision};
    if (scopes[group].pane==pane && (painted_domains[group].owner!=owner || painted_domains[group].revision!=revision)) {
        kit_ui_interaction_reset(&scopes[group].interaction);
        scopes[group].count=0;
    }
}
static bool eligible(IDEPanelButtonGroup group) {
    IDECoreState* core = getCoreState();
    static const IconTool tools[IDE_PANEL_BUTTON_GROUP_COUNT]={ICON_COUNT,ICON_ERRORS,ICON_VERSION_CONTROL,ICON_LIBRARIES,ICON_TASKS,ICON_ASSET_MANAGER,ICON_COUNT};
    return !isRenaming() && (!core || (!core->workspaceAuthoring.active && !core->popupPaneActive)) &&
        (tools[group]==ICON_COUNT || getActiveIcon()==tools[group]);
}
static bool current(IDEPanelButtonGroup group, UIPane* pane) {
    IDEButtonScope* scope = &scopes[group];
    SDL_Rect bounds = pane ? (SDL_Rect){pane->x,pane->y,pane->w,pane->h} : (SDL_Rect){0};
    return pane && pane->visible && scope->pane == pane && scope->count && eligible(group) &&
        scope->domain_revision == ide_ipc_workspace_generation() &&
        painted_domains[group].owner==domains[group].owner && painted_domains[group].revision==domains[group].revision &&
        purpose_generations[group] == getActiveIconGeneration() && !memcmp(&bounds,&scope->pane_bounds,sizeof(bounds));
}
void ide_panel_buttons_publish(IDEPanelButtonGroup group, UIPane* pane, const UIPanelTaggedRectList* hits, uint32_t disabled_id) {
    if (group < 0 || group >= IDE_PANEL_BUTTON_GROUP_COUNT) return;
    field_present[group]=false;
    KitUiInteractionControl controls[IDE_BUTTON_SCOPE_CAPACITY] = {0};
    uint32_t count = 0;
    if (hits && hits->count >= 0 && hits->count <= IDE_BUTTON_SCOPE_CAPACITY && hits->count <= hits->capacity && hits->items) {
        count = (uint32_t)hits->count;
        for (uint32_t i=0;i<count;++i) {
            SDL_Rect r = hits->items[i].rect;
            controls[i] = (KitUiInteractionControl){.id=hits->items[i].tag,.bounds={r.x,r.y,r.w,r.h},.enabled=(uint32_t)hits->items[i].tag != disabled_id};
        }
    }
    if (purpose_generations[group] != getActiveIconGeneration()) kit_ui_interaction_reset(&scopes[group].interaction);
    purpose_generations[group] = getActiveIconGeneration();
    painted_domains[group]=domains[group];
    ide_button_scope_sync(&scopes[group],pane,controls,count,&scopes[group],NULL,ide_ipc_workspace_generation(),eligible(group));
}
void ide_panel_buttons_publish_field(IDEPanelButtonGroup group,UIPane* pane,const UIPanelTaggedRectList* hits,uint32_t disabled_id,SDL_Rect field) {
    if(group<0 || group>=IDE_PANEL_BUTTON_GROUP_COUNT || !hits || hits->count<0 || hits->count>=IDE_BUTTON_SCOPE_CAPACITY || hits->count>hits->capacity || (hits->count && !hits->items))return;
    UIPanelTaggedRect items[IDE_BUTTON_SCOPE_CAPACITY]={0};
    for(int i=0;i<hits->count;++i)items[i]=hits->items[i];
    items[hits->count]=(UIPanelTaggedRect){IDE_PANEL_TEXT_FIELD_ID,field};
    int count=hits->count+1;
    /* Stable visual reading order, rather than separate button/field rings. */
    for(int i=1;i<count;++i){UIPanelTaggedRect item=items[i];int j=i;
        while(j>0 && (items[j-1].rect.y>item.rect.y || (items[j-1].rect.y==item.rect.y && items[j-1].rect.x>item.rect.x))){items[j]=items[j-1];--j;}
        items[j]=item;
    }
    UIPanelTaggedRectList combined={items,count,IDE_BUTTON_SCOPE_CAPACITY};
    ide_panel_buttons_publish(group,pane,&combined,disabled_id);
    field_present[group]=true;
}
bool ide_panel_buttons_field_focused(IDEPanelButtonGroup group,UIPane* pane) {
    return group>=0 && group<IDE_PANEL_BUTTON_GROUP_COUNT && field_present[group] && current(group,pane) && scopes[group].interaction.focused_id==IDE_PANEL_TEXT_FIELD_ID;
}
bool ide_panel_buttons_event(IDEPanelButtonGroup group, UIPane* pane, const SDL_Event* event, bool text_focused, uint32_t* activated) {
    if (activated) *activated=0;
    if (group < 0 || group >= IDE_PANEL_BUTTON_GROUP_COUNT || !event) return false;
    IDEButtonScope* scope = &scopes[group];
    bool keyboard = event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
    if(keyboard && event->key.keysym.sym==SDLK_TAB && (event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI|KMOD_ALT)))return false;
    bool traverse=keyboard && event->key.keysym.sym==SDLK_TAB && !(event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI|KMOD_ALT));
    if (keyboard && text_focused && (!traverse || !field_present[group])) {
        scope->interaction.key_owner_id=0;scope->interaction.armed_key=KIT_UI_INTERACTION_KEY_NONE;
        return false;
    }
    if(traverse && text_focused && field_present[group])scope->interaction.focused_id=IDE_PANEL_TEXT_FIELD_ID;
    if (!current(group,pane)) {
        kit_ui_interaction_reset(&scope->interaction);
        scope->count=0; /* Require a new paint before accepting another activation. */
        /* Existing row/graph/scroll selections must receive their ending release. */
        if (event->type==SDL_MOUSEBUTTONUP) return false;
        return event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP ||
            (keyboard && !text_focused && (event->key.keysym.sym==SDLK_TAB || event->key.keysym.sym==SDLK_RETURN || event->key.keysym.sym==SDLK_SPACE));
    }
    return ide_button_scope_event(scope,event,activated);
}
bool ide_panel_buttons_paint_state(SDL_Rect rect, KitUiButtonState* state) {
    if (!state) return false;
    for (int g=0;g<IDE_PANEL_BUTTON_GROUP_COUNT;++g) {
        IDEButtonScope* scope=&scopes[g];
        if (!current(g,scope->pane)) continue;
        for (uint32_t i=0;i<scope->count;++i) {
            KitRenderRect r=scope->controls[i].bounds;
            SDL_Rect requested={rect.x,rect.y,rect.w,rect.h},clip={0};
            SDL_IntersectRect(&requested,&scope->pane_bounds,&clip);
            if (r.x!=clip.x || r.y!=clip.y || r.width!=clip.w || r.height!=clip.h) continue;
            bool selected=state->selected,disabled=state->disabled;
            *state=ide_button_scope_state(scope,scope->controls[i].id,selected);
            state->disabled |= disabled;
            return true;
        }
    }
    return false;
}

typedef struct PanelTextSnapshot {
    KitUiTextPresentation view;
    float advances[KIT_UI_TEXT_PRESENTATION_BYTES];
    char source[KIT_UI_TEXT_PRESENTATION_BYTES];
    UIPane* pane;
    bool valid, dragging;
} PanelTextSnapshot;
static PanelTextSnapshot text_snapshots[IDE_PANEL_BUTTON_GROUP_COUNT];
static CoreResult snapshot_measure(void* user, const char* prefix, float* width) {
    PanelTextSnapshot* snapshot = user;
    size_t length = strlen(prefix);
    if (length >= KIT_UI_TEXT_PRESENTATION_BYTES) return (CoreResult){.code=CORE_ERR_INVALID_ARG};
    *width = snapshot->advances[length];
    return core_result_ok();
}
void ide_panel_text_publish(IDEPanelButtonGroup group, UIPane* pane, const KitUiTextPresentation* view,
    const char* source, KitUiTextMeasure measure, void* user) {
    if (group < 0 || group >= IDE_PANEL_BUTTON_GROUP_COUNT || !view || !source || !measure) return;
    PanelTextSnapshot* snapshot = &text_snapshots[group];
    snapshot->valid = false;
    if (strlen(source) >= sizeof(snapshot->source) || view->count != 1) return;
    snapshot->view = *view;
    snapshot->pane = pane;
    strcpy(snapshot->source, source);
    const char* row = view->row_text + view->rows[0].text_offset;
    size_t length = strlen(row);
    char prefix[KIT_UI_TEXT_PRESENTATION_BYTES];
    for (size_t i = 0; i <= length; ++i) {
        if (i < length && (((unsigned char)row[i] & 0xc0) == 0x80)) continue;
        memcpy(prefix, row, i); prefix[i] = '\0';
        if (measure(user, prefix, &snapshot->advances[i]).code != CORE_OK) return;
    }
    snapshot->valid = true;
}
bool ide_panel_text_pointer(IDEPanelButtonGroup group, UIPane* pane, const SDL_Event* event,
    const char* source, size_t* position, bool* extend) {
    if (group < 0 || group >= IDE_PANEL_BUTTON_GROUP_COUNT || !event || !position || !extend) return false;
    PanelTextSnapshot* snapshot = &text_snapshots[group];
    bool down = event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT;
    bool motion = event->type == SDL_MOUSEMOTION && (event->motion.state & SDL_BUTTON_LMASK);
    bool up = event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT;
    if (!snapshot->valid || snapshot->pane != pane || !current(group, pane) || !source ||
        strcmp(source, snapshot->source)) { snapshot->dragging = false; return false; }
    if ((!down && !motion && !up) || (!down && !snapshot->dragging)) return false;
    if (down && !ide_panel_buttons_field_focused(group, pane)) return false;
    int x = motion ? event->motion.x : event->button.x;
    int y = motion ? event->motion.y : event->button.y;
    if (kit_ui_text_presentation_hit(&snapshot->view, x, y, snapshot_measure, snapshot, position).code != CORE_OK)
        return false;
    *extend = !down || (SDL_GetModState() & KMOD_SHIFT);
    snapshot->dragging = !up;
    return true;
}
