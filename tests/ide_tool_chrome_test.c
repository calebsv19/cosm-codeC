#include "ide/UI/panel_button_scope.h"
#include "ide/Panes/ToolPanels/Libraries/input_tool_libraries.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/Panes/ToolPanels/Tasks/input_tool_tasks.h"
#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"
#include "ide/Panes/Terminal/terminal_header_controls.h"
#include "ide/Panes/Terminal/terminal.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static IDECoreState core;
static LibraryPanelState library;
static TaskNode task,other,*target=&task;
static uint64_t terminal_generation=1;
static bool close_allowed=true;
static IconTool icon=ICON_LIBRARIES;
static int calls,last,legacy;
static int activates,adds,closes,renames,renamed_index;
static int active_index;
static RenameCallback captured_rename;
static void* rename_context;
const char* getWorkspacePath(void) { return "/fixture/no-shell"; }
int terminal_create_interactive(const char* path) { assert(!strcmp(path,"/fixture/no-shell"));++adds;++terminal_generation;return 1; }
bool terminal_close_active_interactive(void) { if (!close_allowed) return false;++closes;++terminal_generation;return true; }
bool terminal_session_info(int i,const char** name,bool* build,bool* run) {
    if (i<0 || i>=8) return false;
    if(name)*name="Term";if(build)*build=i==6;if(run)*run=i==7;return true;
}
bool terminal_set_active(int index) { if(active_index!=index)++terminal_generation;active_index=index;++activates;return true; }
bool terminal_set_name(int index,const char* name) { assert(!strcmp(name,"renamed"));renamed_index=index;++terminal_generation;return true; }
void beginRenameWithPrompt(const char* prompt,const char* error,const char* old,RenameCallback callback,RenameValidateFn validate,void* context,bool unchanged) {
    assert(!strcmp(prompt,"Terminal Name:") && unchanged && !validate);++renames;captured_rename=callback;rename_context=context;
}
IDECoreState* getCoreState(void) { return &core; }
bool isRenaming(void) { return false; }
IconTool getActiveIcon(void) { return icon; }
uint64_t getActiveIconGeneration(void) { return 1; }
uint64_t ide_ipc_workspace_generation(void) { return 1; }
uint64_t terminal_session_generation(void) { return terminal_generation; }
bool terminal_can_close_active_interactive(void) { return close_allowed; }
LibraryPanelState* libraries_panel_state(void) { return &library; }
bool library_activate_top_control(LibraryTopControlId id) { ++calls;last=id;return true; }
void select_all_library_rows(void) { ++legacy; }
void copy_selected_rows(void) { ++legacy; }
void handleLibraryEntryClick(UIPane* p,int x,int y,Uint16 mod) { ++legacy; }
bool handleLibraryGraphMouseDown(UIPane* p,int x,int y) { ++legacy;return false; }
bool handleLibraryGraphWheel(UIPane* p,SDL_Event* e) { return false; }
void updateLibraryGraphMouseMotion(int x,int y) {}
void updateLibraryDragSelection(UIPane* p,int y) {}
void endLibraryGraphDrag(void) {}
void endLibrarySelectionDrag(void) {}
void updateHoveredLibraryMousePosition(int x,int y) {}
TaskNode** task_panel_selected_ptr(void) { return &target; }
bool task_activate_top_control(TaskTopControlId id) { ++calls;last=id;return true; }
PaneScrollState* task_panel_scroll_state(void) { return NULL; }
SDL_Rect task_panel_scroll_track_rect(void) { return (SDL_Rect){0}; }
SDL_Rect task_panel_scroll_thumb_rect(void) { return (SDL_Rect){0}; }
void handleTaskLeftClick(UIPane* p,SDL_Event* e) { ++legacy; }
void handleTaskHoverAt(UIPane* p,int x,int y) {}
void enqueueCommandSimple(InputCommand command) { ++legacy; }
bool scroll_state_handle_mouse_drag(PaneScrollState* s,const SDL_Event* e,const SDL_Rect* r,const SDL_Rect* t) { return false; }
bool scroll_state_handle_mouse_wheel(PaneScrollState* s,const SDL_Event* e) { return false; }
SDL_Rect scroll_state_thumb_rect(const PaneScrollState* s,int x,int y,int w,int h) { return (SDL_Rect){0}; }
static void mouse(UIPane* pane,bool tasks,Uint32 type,int x,int y) {
    SDL_Event e={0};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
    if(tasks)handleTasksMouseInput(pane,&e);else handleLibrariesMouseInput(pane,&e);
}
static void keyboard(UIPane* pane,bool tasks,Uint32 type,SDL_Keycode sym) {
    SDL_Event e={0};e.type=type;e.key.keysym.sym=sym;
    if(tasks)handleTasksKeyboardInput(pane,&e);else handleLibrariesKeyboardInput(pane,&e);
}
static uint32_t term_mouse(UIPane* pane,Uint32 type,int x,int y,bool* consumed) {
    SDL_Event e={0};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;uint32_t id=0;
    bool c=ide_terminal_header_event(pane,&e,&id);if(consumed)*consumed=c;return id;
}
static void term_publish(UIPane* pane) {
    ide_terminal_header_begin(pane);
    ide_terminal_header_add(IDE_TERMINAL_HEADER_CLOSE,(SDL_Rect){16,16,20,20});
    ide_terminal_header_add(IDE_TERMINAL_HEADER_ADD,(SDL_Rect){42,16,20,20});
    for(int i=0;i<8;++i)ide_terminal_header_add(IDE_TERMINAL_HEADER_TAB_BASE+i,(SDL_Rect){68+i*40,16,36,20});
    ide_terminal_header_publish(pane);
}
int main(void) {
    UIPane pane={.x=10,.y=10,.w=600,.h=300,.visible=true};core.focusedPane=&pane;
    for(int tasks=0;tasks<2;++tasks) {
        icon=tasks?ICON_TASKS:ICON_LIBRARIES;
        int count=tasks?2:3;
        UIPanelTaggedRect items[]={{1,{16,16,20,20}},{2,{42,16,20,20}},{3,{68,16,20,20}}};
        UIPanelTaggedRectList hits={items,count,3};IDEPanelButtonGroup group=tasks?IDE_PANEL_TASKS:IDE_PANEL_LIBRARIES;
        ide_panel_buttons_domain(group,&pane,tasks?(void*)target:(void*)&library,0);
        for(int i=0;i<count;++i) {
            ide_panel_buttons_publish(group,&pane,&hits,0);int before=calls,row_calls=legacy;
            mouse(&pane,tasks,SDL_MOUSEBUTTONDOWN,items[i].rect.x+2,18);assert(calls==before && legacy==row_calls);
            mouse(&pane,tasks,SDL_MOUSEBUTTONUP,items[i].rect.x+2,18);assert(calls==before+1 && last==i+1);
        }
        ide_button_scopes_cancel_except(NULL);ide_panel_buttons_publish(group,&pane,&hits,0);
        keyboard(&pane,tasks,SDL_KEYDOWN,SDLK_TAB);int before=calls;
        keyboard(&pane,tasks,SDL_KEYDOWN,SDLK_RETURN);assert(calls==before);
        keyboard(&pane,tasks,SDL_KEYUP,SDLK_RETURN);assert(calls==before+1 && last==1);
        ide_panel_buttons_publish(group,&pane,&hits,0);
        mouse(&pane,tasks,SDL_MOUSEBUTTONDOWN,18,18);before=calls;
        if(tasks)target=&other;else library.viewMode=LIB_PANEL_VIEW_GRAPH;
        mouse(&pane,tasks,SDL_MOUSEBUTTONUP,18,18);assert(calls==before);
        ide_button_scopes_cancel_except(NULL);
    }
    icon=ICON_BUILD_OUTPUT;term_publish(&pane);
    for(int i=0;i<10;++i) {
        int x=i==0?18:i==1?44:70+(i-2)*40;
        assert(!term_mouse(&pane,SDL_MOUSEBUTTONDOWN,x,18,NULL));
        assert(term_mouse(&pane,SDL_MOUSEBUTTONUP,x,18,NULL)==(uint32_t)(i<2?i+1:100+i-2));
    }
    term_mouse(&pane,SDL_MOUSEBUTTONDOWN,18,18,NULL);++terminal_generation;
    assert(!term_mouse(&pane,SDL_MOUSEBUTTONUP,18,18,NULL));
    term_publish(&pane);close_allowed=false;term_publish(&pane);
    term_mouse(&pane,SDL_MOUSEBUTTONDOWN,18,18,NULL);assert(!term_mouse(&pane,SDL_MOUSEBUTTONUP,18,18,NULL));
    SDL_Event e={0};uint32_t id=999;
    Uint32 types[]={SDL_KEYDOWN,SDL_KEYUP,SDL_TEXTINPUT,SDL_TEXTEDITING};
    for(int i=0;i<4;++i) {
        e.type=types[i];e.key.keysym.sym=SDLK_TAB;
        assert(!ide_terminal_header_event(&pane,&e,&id) && id==0);
    }
    bool consumed=true;
    ++terminal_generation;
    assert(!term_mouse(&pane,SDL_MOUSEBUTTONDOWN,18,100,&consumed) && !consumed);
    assert(!term_mouse(&pane,SDL_MOUSEBUTTONUP,18,100,&consumed) && !consumed);
    assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_ADD) && adds==1);
    assert(!ide_terminal_header_activate(IDE_TERMINAL_HEADER_CLOSE) && closes==0);
    close_allowed=true;assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_CLOSE) && closes==1);
    assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_TAB_BASE+2));assert(active_index==2 && activates==1);
    assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_TAB_BASE+2));assert(renames==1);
    assert(captured_rename);captured_rename("Term","renamed",rename_context);assert(renamed_index==2);
    ++terminal_generation;int before=renames;
    assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_TAB_BASE+2));assert(renames==before && activates==2);
    assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_TAB_BASE+6));
    assert(ide_terminal_header_activate(IDE_TERMINAL_HEADER_TAB_BASE+6));assert(renames==before);
    assert(!ide_terminal_header_activate(IDE_TERMINAL_HEADER_TAB_BASE+99));
    puts("Library/Tasks production input dispatch and terminal header ownership passed; no task/shell actions executed");
}
