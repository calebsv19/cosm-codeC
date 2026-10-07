#include "terminal_header_controls.h"
#include "terminal.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "app/GlobalInfo/core_state.h"
static Uint32 last_click_ticks;
static int last_click_index=-1;
static uint64_t last_click_generation;
static void rename_callback(const char* old_name, const char* new_name, void* context) {
    (void)old_name;
    if (new_name) (void)terminal_set_name((int)(intptr_t)context,new_name);
}
bool ide_terminal_header_activate(uint32_t id) {
    if (id==IDE_TERMINAL_HEADER_CLOSE) return terminal_close_active_interactive();
    if (id==IDE_TERMINAL_HEADER_ADD) return terminal_create_interactive(getWorkspacePath())>=0;
    if (id<IDE_TERMINAL_HEADER_TAB_BASE) return false;
    int index=(int)(id-IDE_TERMINAL_HEADER_TAB_BASE);
    const char* name=NULL; bool build=false,run=false;
    if (!terminal_session_info(index,&name,&build,&run)) return false;
    Uint32 now=SDL_GetTicks();
    bool double_click=index==last_click_index && terminal_session_generation()==last_click_generation &&
                       now-last_click_ticks<400;
    last_click_ticks=now; last_click_index=index;
    if (double_click) {
        if (!build && !run) beginRenameWithPrompt("Terminal Name:","Enter a name",name ? name : "Terminal",
                              rename_callback,NULL,(void*)(intptr_t)index,true);
    } else (void)terminal_set_active(index);
    last_click_generation=terminal_session_generation();
    return true;
}
