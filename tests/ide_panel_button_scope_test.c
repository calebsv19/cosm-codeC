#include "ide/UI/panel_button_scope.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include <assert.h>
#include <stdio.h>
static IDECoreState core;
static uint64_t generation=1, purpose_generation=1;
uint64_t getActiveIconGeneration(void) { return purpose_generation; }
static bool rename_active;
static IconTool icon=ICON_ERRORS;
IDECoreState* getCoreState(void) { return &core; }
uint64_t ide_ipc_workspace_generation(void) { return generation; }
bool isRenaming(void) { return rename_active; }
IconTool getActiveIcon(void) { return icon; }
static uint32_t event(IDEPanelButtonGroup group,UIPane* p,Uint32 type,int x,int y,int button,bool text) {
    SDL_Event e={0};e.type=type;e.button.x=x;e.button.y=y;e.button.button=button;
    uint32_t id=0;ide_panel_buttons_event(group,p,&e,text,&id);return id;
}
static uint32_t key(IDEPanelButtonGroup group,UIPane* p,Uint32 type,SDL_Keycode sym,int repeat,bool text) {
    SDL_Event e={0};e.type=type;e.key.keysym.sym=sym;e.key.repeat=repeat;
    uint32_t id=0;ide_panel_buttons_event(group,p,&e,text,&id);return id;
}
static CoreResult measure_ascii(void* user,const char* text,float* width) {
    (void)user;*width=(float)strlen(text)*10;return core_result_ok();
}
int main(void) {
    UIPane pane={.x=10,.y=10,.w=600,.h=180,.visible=true};core.focusedPane=&pane;
    const IconTool tools[]={ICON_COUNT,ICON_ERRORS,ICON_VERSION_CONTROL,ICON_LIBRARIES,ICON_TASKS,ICON_ASSET_MANAGER};
    for(int group=0;group<IDE_PANEL_TERMINAL;++group) {
        icon=tools[group];
        UIPanelTaggedRect items[25]={0};int count=group==IDE_PANEL_CONTROL?25:group==IDE_PANEL_ERRORS?6:group==IDE_PANEL_LIBRARIES?3:2;
        for(int i=0;i<count;++i) items[i]=(UIPanelTaggedRect){i<22?i+1:1001+i-22,{15+i*22,15,20,20}};
        UIPanelTaggedRectList hits={items,count,25};
        for(int i=0;i<count;++i) {
            ide_panel_buttons_publish(group,&pane,&hits,0);int x=items[i].rect.x+2;
            assert(!event(group,&pane,SDL_MOUSEBUTTONDOWN,x,17,SDL_BUTTON_LEFT,false));
            KitUiButtonState state={0};assert(ide_panel_buttons_paint_state(items[i].rect,&state));assert(state.pressed);
            assert(event(group,&pane,SDL_MOUSEBUTTONUP,x,17,SDL_BUTTON_LEFT,false)==(uint32_t)items[i].tag);
            assert(!event(group,&pane,SDL_MOUSEBUTTONUP,x,17,SDL_BUTTON_LEFT,false));
            event(group,&pane,SDL_MOUSEBUTTONDOWN,x,17,SDL_BUTTON_LEFT,false);
            assert(!event(group,&pane,SDL_MOUSEBUTTONUP,700,17,SDL_BUTTON_LEFT,false));
            assert(!event(group,&pane,SDL_MOUSEBUTTONDOWN,x,17,SDL_BUTTON_RIGHT,false));
            assert(!event(group,&pane,SDL_MOUSEBUTTONUP,x,17,SDL_BUTTON_RIGHT,false));
        }
        ide_button_scopes_cancel_except(NULL);ide_panel_buttons_publish(group,&pane,&hits,0);
        assert(!key(group,&pane,SDL_KEYDOWN,SDLK_TAB,0,false));
        assert(!key(group,&pane,SDL_KEYDOWN,SDLK_RETURN,0,false));
        assert(key(group,&pane,SDL_KEYUP,SDLK_RETURN,0,false)==(uint32_t)items[0].tag);
        assert(!key(group,&pane,SDL_KEYUP,SDLK_RETURN,0,false));
        key(group,&pane,SDL_KEYDOWN,SDLK_SPACE,0,false);
        assert(!key(group,&pane,SDL_KEYDOWN,SDLK_SPACE,1,false));
        assert(key(group,&pane,SDL_KEYUP,SDLK_SPACE,0,false)==(uint32_t)items[0].tag);
        key(group,&pane,SDL_KEYDOWN,SDLK_SPACE,0,false);
        assert(!key(group,&pane,SDL_KEYUP,SDLK_SPACE,0,true)); /* Text takes ownership. */
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);
        ++pane.w;assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));--pane.w;
        assert(!event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false));
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false)); /* Paint required. */
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);++generation;
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);rename_active=true;
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));rename_active=false;
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);core.workspaceAuthoring.active=true;
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));core.workspaceAuthoring.active=false;
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);ide_button_scopes_cancel_except(NULL);
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);
        purpose_generation+=2; /* Away and back, even without intermediate input. */
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);
        ide_button_scopes_forget_pane(&pane);
        KitUiButtonState retired={0};assert(!ide_panel_buttons_paint_state(items[0].rect,&retired));
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        ide_panel_buttons_domain(group,&pane,&pane,1);
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);
        ide_panel_buttons_domain(group,&pane,&core,2);
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        ide_panel_buttons_publish(group,&pane,&hits,0);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);core.popupPaneActive=true;
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));core.popupPaneActive=false;
        if(group!=IDE_PANEL_CONTROL) {
            ide_panel_buttons_publish(group,&pane,&hits,0);
            event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);icon=ICON_BUILD_OUTPUT;
            assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        }
        ide_button_scopes_cancel_except(NULL);
        icon=tools[group];
        ide_panel_buttons_publish(group,&pane,&hits,items[0].tag);
        event(group,&pane,SDL_MOUSEBUTTONDOWN,17,17,SDL_BUTTON_LEFT,false);
        assert(!event(group,&pane,SDL_MOUSEBUTTONUP,17,17,SDL_BUTTON_LEFT,false));
        KitUiButtonState disabled={0};
        assert(ide_panel_buttons_paint_state(items[0].rect,&disabled) && disabled.disabled);
        key(group,&pane,SDL_KEYDOWN,SDLK_TAB,0,false);
        key(group,&pane,SDL_KEYDOWN,SDLK_RETURN,0,false);
        assert(key(group,&pane,SDL_KEYUP,SDLK_RETURN,0,false)==(uint32_t)items[1].tag);
        ide_button_scopes_cancel_except(NULL);
    }
    icon=ICON_ERRORS;core.focusedPane=&pane;
    UIPanelTaggedRect ring_items[]={{1,{20,20,40,20}},{2,{70,20,40,20}}};
    UIPanelTaggedRectList ring={ring_items,2,2};
    ide_button_scopes_cancel_except(NULL);
    ide_panel_buttons_publish_field(IDE_PANEL_ERRORS,&pane,&ring,2,(SDL_Rect){20,60,200,25});
    event(IDE_PANEL_ERRORS,&pane,SDL_MOUSEBUTTONDOWN,30,70,SDL_BUTTON_LEFT,false);
    event(IDE_PANEL_ERRORS,&pane,SDL_MOUSEBUTTONUP,30,70,SDL_BUTTON_LEFT,true);
    assert(ide_panel_buttons_field_focused(IDE_PANEL_ERRORS,&pane));
    assert(!key(IDE_PANEL_ERRORS,&pane,SDL_KEYDOWN,SDLK_RETURN,0,true));
    assert(!key(IDE_PANEL_ERRORS,&pane,SDL_KEYUP,SDLK_RETURN,0,true));
    key(IDE_PANEL_ERRORS,&pane,SDL_KEYDOWN,SDLK_TAB,0,true);
    assert(!ide_panel_buttons_field_focused(IDE_PANEL_ERRORS,&pane));
    key(IDE_PANEL_ERRORS,&pane,SDL_KEYDOWN,SDLK_RETURN,0,false);
    assert(key(IDE_PANEL_ERRORS,&pane,SDL_KEYUP,SDLK_RETURN,0,false)==1);
    key(IDE_PANEL_ERRORS,&pane,SDL_KEYDOWN,SDLK_TAB,0,false);
    assert(ide_panel_buttons_field_focused(IDE_PANEL_ERRORS,&pane)); /* Disabled second button skipped. */
    SDL_Event reverse={0};reverse.type=SDL_KEYDOWN;reverse.key.keysym.sym=SDLK_TAB;reverse.key.keysym.mod=KMOD_SHIFT;
    uint32_t activated=0;assert(ide_panel_buttons_event(IDE_PANEL_ERRORS,&pane,&reverse,true,&activated));
    assert(!activated && !ide_panel_buttons_field_focused(IDE_PANEL_ERRORS,&pane));
    key(IDE_PANEL_ERRORS,&pane,SDL_KEYDOWN,SDLK_RETURN,0,false);
    assert(key(IDE_PANEL_ERRORS,&pane,SDL_KEYUP,SDLK_RETURN,0,false)==1);
    reverse.key.keysym.mod=KMOD_CTRL;assert(!ide_panel_buttons_event(IDE_PANEL_ERRORS,&pane,&reverse,false,&activated));
    ide_panel_buttons_publish_field(IDE_PANEL_ERRORS,&pane,&ring,0,(SDL_Rect){20,60,200,25});
    KitUiTextEdit edit={0};char value[]="abcdef";
    assert(kit_ui_text_bind(&edit,value,sizeof(value),KIT_UI_TEXT_SINGLE_LINE).code==CORE_OK);
    KitUiTextPresentation view={0};KitUiTextPresentationOptions options={.viewport={26,63,188,19},.line_height=16,.caret_width=1};
    assert(kit_ui_text_presentation_build(&view,&edit,&options,measure_ascii,NULL).code==CORE_OK);
    ide_panel_text_publish(IDE_PANEL_ERRORS,&pane,&view,value,measure_ascii,NULL);
    event(IDE_PANEL_ERRORS,&pane,SDL_MOUSEBUTTONDOWN,46,70,SDL_BUTTON_LEFT,false);
    SDL_Event pointer={.type=SDL_MOUSEBUTTONDOWN};pointer.button.button=SDL_BUTTON_LEFT;pointer.button.x=46;pointer.button.y=70;
    size_t position=999;bool extend=true;
    assert(ide_panel_text_pointer(IDE_PANEL_ERRORS,&pane,&pointer,value,&position,&extend) && position==2 && !extend);
    pointer.type=SDL_MOUSEMOTION;pointer.motion.state=SDL_BUTTON_LMASK;pointer.motion.x=66;pointer.motion.y=70;
    assert(ide_panel_text_pointer(IDE_PANEL_ERRORS,&pane,&pointer,value,&position,&extend) && position==4 && extend);
    ++generation;
    assert(!ide_panel_text_pointer(IDE_PANEL_ERRORS,&pane,&pointer,value,&position,&extend));
    assert(!ide_panel_buttons_field_focused(IDE_PANEL_ERRORS,&pane));
    puts("Mixed field/button traversal: forward/reverse, disabled skip, modifier exclusion and workspace retirement passed");
    puts("Six tool/header groups: 40 IDs, release, keyboard, text/modal/resize/workspace cancellation passed");
}
