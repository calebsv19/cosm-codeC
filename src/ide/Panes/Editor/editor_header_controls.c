#include "editor_header_controls.h"
#include "core/Ipc/ide_ipc_workspace.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include <string.h>
#include <stdlib.h>
typedef struct EditorHeaderModel {
    EditorView* root;
    EditorView* view;
    OpenFile** files;
    int count, active, scroll;
    SDL_Rect bounds;
} EditorHeaderModel;
typedef struct EditorHeaderScope {
    IDEButtonScope buttons;
    EditorHeaderModel painted;
    bool seen;
    OpenFile** file_snapshot;
    int file_capacity;
} EditorHeaderScope;
static EditorHeaderScope scopes[EDITOR_MAX_LEAF_VIEWS];
static UIPane* painting_pane;
static bool model(UIPane* pane, EditorView* view, EditorHeaderModel* out) {
    if (!pane || !view || view->type!=VIEW_LEAF || view->fileCount<0 || (view->fileCount && !view->openFiles)) return false;
    memset(out,0,sizeof(*out));
    out->root=pane->editorView;out->view=view;out->count=view->fileCount;
    out->active=view->activeTab;out->scroll=view->tabScrollX;
    out->bounds=(SDL_Rect){view->x,view->y,view->w,view->h};
    out->files=view->openFiles;
    return true;
}
static bool live(EditorView* root, const EditorView* wanted) {
    if(!root)return false;
    if(root->type==VIEW_LEAF)return root==wanted;
    return live(root->childA,wanted)||live(root->childB,wanted);
}
bool ide_editor_header_close_enabled(UIPane* pane, EditorView* view) {
    if (!pane || !view || view->type != VIEW_LEAF || !live(pane->editorView, view)) return false;
    if (view->activeTab >= 0 && view->activeTab < view->fileCount) return true;
    /* Retain the sole empty leaf as the workspace's file-opening target. */
    return view->fileCount == 0 && pane->editorView != view;
}
static bool eligible(void) {
    IDECoreState* core=getCoreState();
    return !isRenaming() && (!core || (!core->workspaceAuthoring.active && !core->popupPaneActive));
}
static void invalidate(EditorHeaderScope* scope) {
    kit_ui_interaction_reset(&scope->buttons.interaction);scope->buttons.count=0;
}
static bool matches(const EditorHeaderScope* scope,const EditorHeaderModel* next) {
    return !memcmp(next,&scope->painted,sizeof(*next)) && next->count<=scope->file_capacity &&
        (!next->count || (scope->file_snapshot && !memcmp(next->files,scope->file_snapshot,(size_t)next->count*sizeof(OpenFile*))));
}
void ide_editor_headers_begin(UIPane* pane) {
    painting_pane=pane;
    for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i)scopes[i].seen=false;
}
void ide_editor_header_publish(EditorView* view,const KitUiInteractionControl* controls,uint32_t count) {
    EditorHeaderModel next;
    if(!model(painting_pane,view,&next))return;
    EditorHeaderScope* scope=NULL;
    for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i)if(scopes[i].painted.view==view){scope=&scopes[i];break;}
    if(!scope)for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i)if(!scopes[i].painted.view){scope=&scopes[i];break;}
    if(!scope)return;
    if(!matches(scope,&next))invalidate(scope);
    if(next.count>scope->file_capacity){
        if((size_t)next.count>SIZE_MAX/sizeof(OpenFile*)){invalidate(scope);return;}
        OpenFile** grown=realloc(scope->file_snapshot,(size_t)next.count*sizeof(OpenFile*));
        if(!grown){invalidate(scope);return;}
        scope->file_snapshot=grown;scope->file_capacity=next.count;
    }
    if(next.count)memcpy(scope->file_snapshot,next.files,(size_t)next.count*sizeof(OpenFile*));
    scope->painted=next;scope->seen=true;
    const void* selected=(view->activeTab>=0 && view->activeTab<view->fileCount)?view->openFiles[view->activeTab]:NULL;
    ide_button_scope_sync(&scope->buttons,painting_pane,controls,count,view,selected,ide_ipc_workspace_generation(),eligible());
}
void ide_editor_headers_end(void) {
    for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i)if(!scopes[i].seen){invalidate(&scopes[i]);memset(&scopes[i].painted,0,sizeof(scopes[i].painted));}
    painting_pane=NULL;
}
static bool current(EditorHeaderScope* scope,UIPane* pane) {
    /* Compare identity against the live tree before dereferencing borrowed views. */
    if(!pane || !pane->visible || !eligible() || scope->buttons.pane!=pane || !scope->buttons.count || scope->buttons.domain_revision!=ide_ipc_workspace_generation() || !live(pane->editorView,scope->painted.view))return false;
    SDL_Rect bounds={pane->x,pane->y,pane->w,pane->h};EditorHeaderModel next;
    return !memcmp(&bounds,&scope->buttons.pane_bounds,sizeof(bounds)) && model(pane,scope->painted.view,&next) && matches(scope,&next);
}
bool ide_editor_headers_event(UIPane* pane,const SDL_Event* event,EditorView** target,uint32_t* action) {
    if(target)*target=NULL;if(action)*action=0;
    if(!pane || !event)return false;
    /* Keyboard, including Tab/Enter/Space, always belongs to the document. */
    if(event->type!=SDL_MOUSEBUTTONDOWN && event->type!=SDL_MOUSEBUTTONUP && event->type!=SDL_MOUSEMOTION)return false;
    bool consumed=false;
    for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i){
        EditorHeaderScope* scope=&scopes[i];
        if(!current(scope,pane)){invalidate(scope);continue;}
        uint32_t id=0;
        consumed=ide_button_scope_event(&scope->buttons,event,&id)||consumed;
        if(id){if(target)*target=scope->painted.view;if(action)*action=id;}
    }
    /* Invalid headers cannot fall through into document selection before repaint. */
    if(event->type==SDL_MOUSEBUTTONDOWN){
        EditorView* leaf=findLeafUnderCursor(pane->editorView,event->button.x,event->button.y);
        if(leaf && event->button.y>=editor_view_box_y(leaf) && event->button.y<editor_view_content_y(leaf))return true;
    }
    return consumed;
}
KitUiButtonState ide_editor_header_state(EditorView* view,uint32_t id,bool selected) {
    for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i)if(scopes[i].painted.view==view && current(&scopes[i],scopes[i].buttons.pane))return ide_button_scope_state(&scopes[i].buttons,id,selected);
    return (KitUiButtonState){.selected=selected};
}
void ide_editor_headers_forget_view(EditorView* view) {
    for(int i=0;i<EDITOR_MAX_LEAF_VIEWS;++i)if(scopes[i].painted.view==view){invalidate(&scopes[i]);memset(&scopes[i].painted,0,sizeof(scopes[i].painted));}
}
