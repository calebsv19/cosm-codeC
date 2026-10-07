#include "ide/Panes/MenuBar/input_menu_bar.h"
#include "ide/Panes/MenuBar/menu_bar_controls.h"
#include "app/GlobalInfo/core_state.h"
#include <assert.h>
#include <stdio.h>
static IDECoreState core;
static InputCommand last;
static int calls, filename_width = 180;
static bool renaming;
MenuBarLayoutMode currentMenuBarLayout;
IDECoreState* getCoreState(void) { return &core; }
void enqueueCommandSimple(InputCommand cmd) { last = cmd; ++calls; }
const char* getActiveFileName(void) { return "populated-document-with-a-long-name.c"; }
int getTextWidth(const char* text) { (void)text; return filename_width; }
bool isRenaming(void) { return renaming; }
static void pointer(UIPane* pane, Uint32 type, int button, int x, int y) {
    SDL_Event e = {0}; e.type = type; e.button.button = button; e.button.x = x; e.button.y = y;
    handleMenuBarMouseInput(pane, &e);
}
static void key(UIPane* pane, Uint32 type, SDL_Keycode sym, Uint16 mod, int repeat) {
    SDL_Event e = {0}; e.type = type; e.key.keysym.sym = sym; e.key.keysym.mod = mod; e.key.repeat = repeat;
    handleMenuBarKeyboardInput(pane, &e);
}
int main(void) {
    UIPane pane = {.x = 10, .y = 20, .w = 900, .h = 40, .visible = true};
    core.focusedPane = &pane;
    for (int mode = 0; mode < 2; ++mode) {
        currentMenuBarLayout = mode;
        for (int width = 240; width <= 1000; width += 190) {
            pane.w = width;
            KitUiInteractionControl controls[6];
            ide_menu_bar_geometry(&pane, mode, 10000, controls);
            for (int i = 0; i < 6; ++i) {
                assert(controls[i].enabled);
                assert(controls[i].bounds.x >= pane.x);
                assert(controls[i].bounds.x + controls[i].bounds.width <= pane.x + pane.w);
                if (i) assert(controls[i-1].bounds.x + controls[i-1].bounds.width <= controls[i].bounds.x);
            }
        }
        pane.w = 900;
        KitUiInteractionControl controls[6];
        ide_menu_bar_geometry(&pane, mode, filename_width, controls);
        for (int i = 0; i < 6; ++i) {
            ide_menu_bar_cancel(); int before = calls;
            int x = controls[i].bounds.x + 5, y = controls[i].bounds.y + 5;
            pointer(&pane, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, x, y);
            assert(calls == before);
            assert(ide_menu_bar_button_state(&pane, i).pressed);
            pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y);
            assert(calls == before + 1 && last == ide_menu_bar_command(i+1));
            pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y);
            assert(calls == before + 1);
            pointer(&pane, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, x, y);
            pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y + 80);
            assert(calls == before + 1);
            pointer(&pane, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, x, y);
            pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, x, y);
            assert(calls == before + 1);
            pointer(&pane, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, x, y);
            ide_menu_bar_cancel();
            pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y);
            assert(calls == before + 1);
        }
        ide_menu_bar_cancel(); int before = calls;
        key(&pane, SDL_KEYDOWN, SDLK_TAB, 0, 0);
        key(&pane, SDL_KEYDOWN, SDLK_RETURN, 0, 0);
        key(&pane, SDL_KEYDOWN, SDLK_RETURN, 0, 1);
        assert(calls == before);
        key(&pane, SDL_KEYUP, SDLK_RETURN, 0, 0);
        assert(calls == before + 1 && last == COMMAND_CHOOSE_WORKSPACE);
        key(&pane, SDL_KEYDOWN, SDLK_TAB, KMOD_SHIFT, 0);
        key(&pane, SDL_KEYDOWN, SDLK_SPACE, 0, 0);
        key(&pane, SDL_KEYDOWN, SDLK_ESCAPE, 0, 0);
        key(&pane, SDL_KEYUP, SDLK_SPACE, 0, 0);
        assert(calls == before + 1);
        key(&pane, SDL_KEYDOWN, SDLK_s, KMOD_GUI, 0);
        assert(calls == before + 2 && last == COMMAND_SAVE_FILE);
        // Changed layout and modal ownership must disarm an old release.
        int x = controls[0].bounds.x+5, y = controls[0].bounds.y+5;
        pointer(&pane, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, x, y);
        pane.w += 10;
        pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y);
        assert(calls == before + 2); pane.w -= 10;
        pointer(&pane, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, x, y);
        renaming = true;
        pointer(&pane, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, x, y);
        renaming = false;
        assert(calls == before + 2);
    }
    puts("menu command parity, release ownership, focus/repeat/cancel, modal and responsive geometry passed");
    return 0;
}
