#include "input_keyboard.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "core/InputManager/UserInput/rename_access.h"
#include "core/CommandBus/command_bus.h"
#include "core/CommandBus/command_metadata.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "ide/UI/layout.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This file handles SDL_KEYDOWN input → dispatchInputCommand()
// It does NOT handle behavior — that's routed by the pane’s inputHandler

static void enqueueTargetedCommand(UIPane* target, InputCommand cmd, SDL_Keymod mod) {
    InputCommandMetadata meta = {
        .cmd = cmd,
        .originRole = target ? target->role : PANE_ROLE_UNKNOWN,
        .mouseX = -1,
        .mouseY = -1,
        .keyMod = mod,
        .targetPane = target,
        .payload = NULL
    };
    enqueueCommand(meta);
}

static bool text_entry_is_active(const IDECoreState* core) {
    if (isRenaming()) return true;
    if (control_panel_is_search_focused()) return true;
    if (git_panel_is_message_focused()) return true;
    if (SDL_IsTextInputActive()) return true;
    if (!core || !core->focusedPane) return false;
    UIPaneRole role = core->focusedPane->role;
    return role == PANE_ROLE_EDITOR || role == PANE_ROLE_TERMINAL;
}

static bool global_shortcut_text_capture_active(void) {
    if (isRenaming()) return true;
    if (control_panel_is_search_focused()) return true;
    if (git_panel_is_message_focused()) return true;
    return false;
}

static void apply_font_zoom_runtime_change(void) {
    char zoom_step_buf[16];
    int zoom_step = ide_shared_font_zoom_step();

    snprintf(zoom_step_buf, sizeof(zoom_step_buf), "%d", zoom_step);
    setenv("IDE_FONT_ZOOM_STEP", zoom_step_buf, 1);
    saveFontZoomStepPreference(zoom_step);

    render_text_cache_shutdown();
    if (!initFontSystem()) {
        return;
    }

    terminal_notify_font_metrics_changed();
    requestFullRedraw(RENDER_INVALIDATION_LAYOUT |
                      RENDER_INVALIDATION_RESIZE |
                      RENDER_INVALIDATION_CONTENT |
                      RENDER_INVALIDATION_BACKGROUND);
}

void handleKeyboardInput(SDL_Event* event,
                         UIPane** panes, int* paneCount, bool* running) {
    if (!event || event->type != SDL_KEYDOWN) return;

    IDECoreState* core = getCoreState();
    UIState* uiState = getUIState();
    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;
    bool ctrl_or_cmd = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    bool shift = (mod & KMOD_SHIFT) != 0;

    // Workspace-root chooser shortcuts are global by policy:
    // they should work regardless of focused pane.
    if (ctrl_or_cmd && key == SDLK_l) {
        InputCommand cmd = shift ? COMMAND_CHOOSE_WORKSPACE_TYPED : COMMAND_CHOOSE_WORKSPACE;
        enqueueTargetedCommand(uiState ? uiState->menuBar : NULL, cmd, (SDL_Keymod)mod);
        return;
    }

    if (ctrl_or_cmd && !global_shortcut_text_capture_active()) {
        bool zoom_handled = false;
        bool zoom_changed = false;
        if (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS) {
            zoom_handled = true;
            zoom_changed = ide_shared_font_step_by(1);
        } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
            zoom_handled = true;
            zoom_changed = ide_shared_font_step_by(-1);
        } else if (key == SDLK_0 || key == SDLK_KP_0) {
            zoom_handled = true;
            zoom_changed = ide_shared_font_reset_zoom_step();
        }

        if (zoom_handled) {
            if (zoom_changed) {
                apply_font_zoom_runtime_change();
            }
            return;
        }
    }

    if (ctrl_or_cmd && shift && !global_shortcut_text_capture_active()) {
        if (key == SDLK_t) {
            if (ide_shared_theme_cycle_next()) {
                char preset_name[128] = {0};
                if (ide_shared_theme_current_preset(preset_name, sizeof(preset_name))) {
                    saveThemePresetPreference(preset_name);
                }
                ide_refresh_live_theme();
                requestFullRedraw(RENDER_INVALIDATION_THEME |
                                  RENDER_INVALIDATION_BACKGROUND |
                                  RENDER_INVALIDATION_CONTENT);
            }
            return;
        }
        if (key == SDLK_y) {
            if (ide_shared_theme_cycle_prev()) {
                char preset_name[128] = {0};
                if (ide_shared_theme_current_preset(preset_name, sizeof(preset_name))) {
                    saveThemePresetPreference(preset_name);
                }
                ide_refresh_live_theme();
                requestFullRedraw(RENDER_INVALIDATION_THEME |
                                  RENDER_INVALIDATION_BACKGROUND |
                                  RENDER_INVALIDATION_CONTENT);
            }
            return;
        }
    }

    // === GLOBAL COMMANDS (Always valid regardless of focused pane) ===

    if (isRenaming()) {
	    if (key == SDLK_RETURN) { submitRenameWithMod((SDL_Keymod)mod); return; }
	    else if (key == SDLK_ESCAPE) { cancelRename(); return; }
	    else if (key == SDLK_BACKSPACE) { handleRenameTextInput('\b'); return; }
	
	    else if (key == SDLK_LEFT) {
	        if (RENAME->cursorPosition > 0)
	            RENAME->cursorPosition--;
	        return;
	    }
	
	    else if (key == SDLK_RIGHT) {
	        if (RENAME->cursorPosition < (int)strlen(RENAME->inputBuffer))
	            RENAME->cursorPosition++;
	        return;
	    }
    }


    // Ctrl+E → Add task
/*    if ((mod & KMOD_CTRL) && key == SDLK_e) {
        CMD(COMMAND_ADD_TASK);
        return;
    }
*/
    // Ctrl+1 or Tab → Switch tab globally
    if ((mod & KMOD_CTRL) && key == SDLK_1) {
        enqueueTargetedCommand(core ? core->editorPane : NULL, COMMAND_SWITCH_TAB, (SDL_Keymod)mod);
        return;
    }

    // Ctrl+R or TAB → Toggle control/tool panel visibility
    if (mod & KMOD_CTRL && key == SDLK_r) {
        enqueueTargetedCommand(uiState ? uiState->menuBar : NULL, COMMAND_TOGGLE_CONTROL_PANEL, (SDL_Keymod)mod);
        return;
    }

    if (mod & KMOD_CTRL && key == SDLK_t) {
        enqueueTargetedCommand(uiState ? uiState->menuBar : NULL, COMMAND_TOGGLE_TOOL_PANEL, (SDL_Keymod)mod);
        return;
    }

    // Cmd/Ctrl + Shift + C → Clear analysis cache
    if ((mod & (KMOD_CTRL | KMOD_GUI)) && (mod & KMOD_SHIFT) && key == SDLK_c) {
        enqueueTargetedCommand(uiState ? uiState->menuBar : NULL, COMMAND_CLEAR_ANALYSIS_CACHE, (SDL_Keymod)mod);
        return;
    }

    // Plain P toggles search pause/resume globally, but never while typing.
    bool plainP = (key == SDLK_p) && !(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT | KMOD_SHIFT));
    if (plainP && !text_entry_is_active(core)) {
        control_panel_toggle_search_enabled();
        return;
    }

    // === FALLBACK: Send key to currently focused pane ===
    UIPane* focused = core ? core->focusedPane : NULL;
    if (focused && focused->inputHandler && focused->inputHandler->onKeyboard) {
        focused->inputHandler->onKeyboard(focused, event);
    } else {
        printf("[Keyboard] Unhandled or no focused pane: key=%d\n", key);
    }
}
