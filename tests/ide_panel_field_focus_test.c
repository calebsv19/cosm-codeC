#include "ide/UI/panel_field_focus.h"
#include "ide/UI/ui_state.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "app/GlobalInfo/core_state.h"
#include <assert.h>
#include <stdio.h>
static IDECoreState core;
static UIState ui;
static IconTool icon;
static uint64_t generation=1;
static bool control, errors, git, modal;
static int cancelled;
IDECoreState* getCoreState(void){return &core;}
UIState* getUIState(void){return &ui;}
IconTool getActiveIcon(void){return icon;}
uint64_t ide_ipc_workspace_generation(void){return generation;}
bool isRenaming(void){return modal;}
bool control_panel_is_search_focused(void){return control;}
bool errors_is_search_focused(void){return errors;}
bool git_panel_is_message_focused(void){return git;}
void control_panel_set_search_focused(bool value){if(control&&!value)++cancelled;control=value;}
void errors_set_search_focused(bool value){if(errors&&!value)++cancelled;errors=value;}
void git_panel_set_message_focus(bool value){if(git&&!value)++cancelled;git=value;}
int main(void){
    UIPane cp={.visible=true},tp={.visible=true},editor={.visible=true};
    ui.controlPanel=&cp;ui.toolPanel=&tp;ui.controlPanelVisible=ui.toolPanelVisible=true;
    core.focusedPane=&cp;control=true;errors=true;git=true;icon=ICON_ERRORS;
    ide_panel_fields_reconcile();assert(control&&!errors&&!git&&cancelled==2);
    core.focusedPane=&editor;ide_panel_fields_reconcile();assert(!control&&cancelled==3);
    core.focusedPane=&tp;errors=true;icon=ICON_ERRORS;ide_panel_fields_reconcile();assert(errors);
    icon=ICON_VERSION_CONTROL;ide_panel_fields_reconcile();assert(!errors);
    git=true;ui.toolPanelVisible=false;ide_panel_fields_reconcile();assert(!git);
    ui.toolPanelVisible=true;git=true;modal=true;ide_panel_fields_reconcile();assert(!git);modal=false;
    core.focusedPane=&cp;control=true;ide_panel_fields_reconcile();assert(control);
    ++generation;ide_panel_fields_reconcile();assert(!control);
    control=true;ide_panel_fields_blur_except(NULL);assert(!control);
    puts("Field ownership: pane, tool, hidden, modal, workspace and native interruption cancellation passed");
}
