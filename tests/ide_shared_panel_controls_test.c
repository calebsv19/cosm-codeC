#include "ide/UI/button_scope.h"
#include "ide/Panes/IconBar/input_icon_bar.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "ide/Panes/ToolPanels/Project/project_toolbar.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "app/GlobalInfo/core_state.h"
#include <assert.h>
#include "ide/Panes/IconBar/command_icon_bar.h"
#include "ide/UI/ui_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include <stdio.h>
static IDECoreState core;
static UIState ui;
UIState* getUIState(void){return &ui;}
void requestFullRedraw(uint32_t bits){}
static InputCommand last;
static int calls, clear_count;
static uint64_t generation = 1;
uint64_t ide_ipc_workspace_generation(void) { return generation; }
static bool renaming;
static DirEntry root, file, folder, other;
DirEntry* projectRoot = &root;
static DirEntry *entry=&file, *selected_file=&file, *directory=&folder, *rename_entry;
IDECoreState* getCoreState(void) { return &core; }
void enqueueCommandSimple(InputCommand c) { last=c; ++calls; }
bool isRenaming(void) { return renaming; }
DirEntry** project_selected_entry_ptr(void) { return &entry; }
DirEntry** project_selected_file_ptr(void) { return &selected_file; }
DirEntry** project_selected_directory_ptr(void) { return &directory; }
DirEntry** project_renaming_entry_ptr(void) { return &rename_entry; }
void project_clear_select_all_visual(void) { ++clear_count; }
ToolPanelLayoutDefaults tool_panel_layout_defaults(void) { return (ToolPanelLayoutDefaults){.pad_left=12,.button_h=24}; }
int tool_panel_info_line_y(const UIPane* p,int i) { return p->y+8+i*18; }
static void event(UIPane* p, bool icon, Uint32 type,int button,int x,int y) {
    SDL_Event e={0}; e.type=type; e.button.button=button;e.button.x=x;e.button.y=y;
    if(icon) handleIconBarMouseInput(p,&e); else (void)ide_project_toolbar_event(p,&e);
}
static void key(UIPane* p,bool icon,Uint32 type,SDL_Keycode sym,Uint16 mod,int repeat) {
    SDL_Event e={0};e.type=type;e.key.keysym.sym=sym;e.key.keysym.mod=mod;e.key.repeat=repeat;
    if(icon) handleIconBarKeyboardInput(p,&e); else (void)ide_project_toolbar_event(p,&e);
}
static void check_controls(UIPane* pane,bool icon,int count) {
    core.focusedPane=pane;
    for(int i=0;i<count;++i) {
        ide_button_scopes_cancel_except(NULL);
        SDL_Rect r=icon?getIconRect(pane,i):ide_project_toolbar_rect(pane,i);
        int before=calls;
        event(pane,icon,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        assert(calls==before);
        KitUiButtonState state=icon?ide_icon_bar_button_state(pane,i):ide_project_toolbar_state(pane,i);
        assert(state.pressed && state.focused);
        event(pane,icon,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        assert(calls==before+1);
        InputCommand expected_icon[]={COMMAND_SELECT_ICON_PROJECT_FILES,COMMAND_SELECT_ICON_LIBRARIES,
            COMMAND_SELECT_ICON_BUILD_OUTPUT,COMMAND_SELECT_ICON_ERRORS,COMMAND_SELECT_ICON_ASSET_MANAGER,
            COMMAND_SELECT_ICON_TASKS,COMMAND_SELECT_ICON_VERSION_CONTROL};
        InputCommand expected_project[]={COMMAND_NEW_FILE,COMMAND_DELETE_FILE,COMMAND_NEW_FOLDER,COMMAND_DELETE_FOLDER};
        assert(last==(icon?expected_icon[i]:expected_project[i]));
        event(pane,icon,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        assert(calls==before+1);
        event(pane,icon,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        event(pane,icon,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,pane->x+pane->w+20,r.y+2);
        assert(calls==before+1);
        event(pane,icon,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_RIGHT,r.x+2,r.y+2);
        event(pane,icon,SDL_MOUSEBUTTONUP,SDL_BUTTON_RIGHT,r.x+2,r.y+2);
        assert(calls==before+1);
        event(pane,icon,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        ide_button_scopes_cancel_except(NULL);
        event(pane,icon,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        assert(calls==before+1);
        event(pane,icon,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
        pane->w+=4;event(pane,icon,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);pane->w-=4;
        assert(calls==before+1);
    }
    ide_button_scopes_cancel_except(NULL);int before=calls;
    key(pane,icon,SDL_KEYDOWN,SDLK_TAB,0,0);
    key(pane,icon,SDL_KEYDOWN,SDLK_RETURN,0,0);
    key(pane,icon,SDL_KEYDOWN,SDLK_RETURN,0,1);
    assert(calls==before);
    key(pane,icon,SDL_KEYUP,SDLK_RETURN,0,0);assert(calls==before+1);
    key(pane,icon,SDL_KEYDOWN,SDLK_TAB,KMOD_SHIFT,0);
    key(pane,icon,SDL_KEYDOWN,SDLK_SPACE,0,0);
    key(pane,icon,SDL_KEYDOWN,SDLK_ESCAPE,0,0);
    key(pane,icon,SDL_KEYUP,SDLK_SPACE,0,0);assert(calls==before+1);
    renaming=true;key(pane,icon,SDL_KEYDOWN,SDLK_TAB,0,0);key(pane,icon,SDL_KEYDOWN,SDLK_RETURN,0,0);
    renaming=false;key(pane,icon,SDL_KEYUP,SDLK_RETURN,0,0);assert(calls==before+1);
}
int main(void) {
    assert(2*EDITOR_PADDING+EDITOR_SPLIT_GAP==0);
    InputCommand selections[]={COMMAND_SELECT_ICON_PROJECT_FILES,COMMAND_SELECT_ICON_LIBRARIES,COMMAND_SELECT_ICON_BUILD_OUTPUT,COMMAND_SELECT_ICON_ERRORS,COMMAND_SELECT_ICON_ASSET_MANAGER,COMMAND_SELECT_ICON_TASKS,COMMAND_SELECT_ICON_VERSION_CONTROL};
    for(int i=0;i<ICON_COUNT;++i){
        ui.toolPanelVisible=false;ui.controlPanelVisible=false;ui.terminalVisible=true;
        handleIconBarCommand(NULL,(InputCommandMetadata){.cmd=selections[i]});
        assert(ui.toolPanelVisible && getActiveIcon()==i && !ui.controlPanelVisible && ui.terminalVisible);
        handleIconBarCommand(NULL,(InputCommandMetadata){.cmd=selections[i]});
        assert(!ui.toolPanelVisible && getActiveIcon()==i && !ui.controlPanelVisible && ui.terminalVisible);
        handleIconBarCommand(NULL,(InputCommandMetadata){.cmd=selections[i]});
        assert(ui.toolPanelVisible && getActiveIcon()==i);
        for(int j=0;j<ICON_COUNT;++j)if(j!=i){
            handleIconBarCommand(NULL,(InputCommandMetadata){.cmd=selections[j]});
            assert(ui.toolPanelVisible && getActiveIcon()==j);
            handleIconBarCommand(NULL,(InputCommandMetadata){.cmd=selections[i]});
            assert(ui.toolPanelVisible && getActiveIcon()==i);
        }
    }

    setActiveIcon(ICON_PROJECT_FILES); /* Restore the next fixture's initial tool. */
    UIPane icon={.x=0,.y=40,.w=60,.h=600,.visible=true,.role=PANE_ROLE_ICONBAR};
    UIPane project={.x=60,.y=40,.w=260,.h=600,.visible=true,.role=PANE_ROLE_TOOLPANEL};
    check_controls(&icon,true,7);check_controls(&project,false,4);
    core.focusedPane=&icon;
    for(int i=0;i<7;++i) {int before=calls;key(&icon,true,SDL_KEYDOWN,SDLK_1+i,0,0);assert(calls==before+1);
        key(&icon,true,SDL_KEYDOWN,SDLK_1+i,0,1);assert(calls==before+1);}
    core.focusedPane=&project; SDL_Rect r=ide_project_toolbar_rect(&project,1);int before=calls;
    event(&project,false,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
    entry=&other;event(&project,false,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);assert(calls==before);
    event(&project,false,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
    ++generation;event(&project,false,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);assert(calls==before);
    selected_file=NULL;directory=projectRoot;
    assert(ide_project_toolbar_state(&project,1).disabled);
    assert(ide_project_toolbar_state(&project,3).disabled);
    event(&project,false,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,r.x+2,r.y+2);
    event(&project,false,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,r.x+2,r.y+2);assert(calls==before);
    project.h=60;assert(ide_project_toolbar_state(&project,3).disabled);
    setActiveIcon(ICON_ERRORS);assert(ide_project_toolbar_state(&project,0).disabled);
    icon.h=100;assert(ide_icon_bar_button_state(&icon,6).disabled);
    setActiveIcon(ICON_PROJECT_FILES);
    SDL_Rect project_button=ide_project_toolbar_rect(&project,0);
    int previous_calls=calls;
    event(&project,false,SDL_MOUSEBUTTONDOWN,SDL_BUTTON_LEFT,project_button.x+2,project_button.y+2);
    setActiveIcon(ICON_ERRORS);setActiveIcon(ICON_PROJECT_FILES);
    event(&project,false,SDL_MOUSEBUTTONUP,SDL_BUTTON_LEFT,project_button.x+2,project_button.y+2);
    assert(calls==previous_calls);
    printf("7 icon and 4 project commands: release/focus/modal/geometry/target/hidden cancellation pass; no filesystem actions executed\n");
}
