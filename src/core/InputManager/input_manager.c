#include "input_mouse.h"
#include "input_manager.h"
#include "input_keyboard.h"
#include "input_resize.h"
#include "input_hover.h"
#include "input_global.h"
#include "ide/Panes/MenuBar/input_menu_bar.h"
#include "ide/Panes/IconBar/input_icon_bar.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "ide/Panes/ToolPanels/Project/project_toolbar.h"
#include "ide/UI/button_scope.h"
#include "ide/UI/panel_field_focus.h"
#include "ide/UI/window_lifecycle.h"


#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"

#include "core/CommandBus/command_bus.h"
#include "core/InputManager/UserInput/rename_flow.h"

#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/editor_view.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/layout_config.h"
#include "ide/UI/layout.h"

#include <stdio.h>
#include <string.h>

typedef struct HeldKeyOwner {
    bool pressed;
    bool rename;
    UIPane* pane;
    uint64_t tool_generation;
    EditorView* view;
    OpenFile* file;
    uint64_t file_lifetime;
} HeldKeyOwner;

static HeldKeyOwner held_keys[SDL_NUM_SCANCODES];

void input_cancel_held_keys(void) {
    memset(held_keys, 0, sizeof(held_keys));
}

static HeldKeyOwner key_owner(bool rename) {
    HeldKeyOwner owner = {.pressed = true, .rename = rename};
    if (rename) return owner;
    const IDECoreState* core = getCoreState();
    owner.pane = core->focusedPane;
    if (owner.pane && owner.pane->role == PANE_ROLE_TOOLPANEL) {
        owner.tool_generation = getActiveIconGeneration();
    } else if (owner.pane && owner.pane->role == PANE_ROLE_EDITOR) {
        owner.view = core->activeEditorView ? core->activeEditorView : owner.pane->editorView;
        owner.file = getActiveOpenFile(owner.view);
        owner.file_lifetime = owner.file ? owner.file->commandTargetId : 0;
    }
    return owner;
}

static bool same_key_scope(HeldKeyOwner a, HeldKeyOwner b) {
    return a.rename == b.rename && a.pane == b.pane && a.tool_generation == b.tool_generation &&
           a.view == b.view && a.file == b.file;
}

static bool owns_key_edge(const SDL_KeyboardEvent* key, bool rename) {
    SDL_Scancode scan = key->keysym.scancode;
    if (scan <= SDL_SCANCODE_UNKNOWN || scan >= SDL_NUM_SCANCODES)
        return key->type == SDL_KEYDOWN && !key->repeat;

    HeldKeyOwner* held = &held_keys[scan];
    if (key->type == SDL_KEYDOWN && !key->repeat) return true;
    HeldKeyOwner owner = key_owner(rename);
    bool owned = held->pressed && same_key_scope(*held, owner) && held->file_lifetime == owner.file_lifetime;
    if (key->type == SDL_KEYUP || !owned) *held = (HeldKeyOwner){0};
    return owned;
}

static void dispatch_key_press(SDL_Event* event, bool rename, UIPane** panes, int* count, bool* running) {
    const SDL_KeyboardEvent* key = &event->key;
    if (!owns_key_edge(key, rename)) return;
    HeldKeyOwner before = key_owner(rename);
    handleKeyboardInput(event, panes, count, running);
    SDL_Scancode scan = key->keysym.scancode;
    if (!key->repeat && scan > SDL_SCANCODE_UNKNOWN && scan < SDL_NUM_SCANCODES) {
        // Command admission may bind an editor file ID on its first key press.
        HeldKeyOwner after = key_owner(isRenaming());
        held_keys[scan] = same_key_scope(before, after) ? after : (HeldKeyOwner){0};
    }
}

void handleInput(SDL_Event* event,
                 UIPane** panes, int paneCount,
                 ResizeZone* zones, int zoneCount,
                 int* paneCountRef, bool* running) {

    static bool previous_rename;
    bool rename = isRenaming();
    if (rename != previous_rename) {
        input_cancel_held_keys();
        previous_rename = rename;
        if (rename) ide_window_cancel_pointer_drafts(panes, paneCountRef);
    }
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        input_cancel_held_keys();

    if (rename) {
        switch (event->type) {
        case SDL_KEYDOWN:
            dispatch_key_press(event, true, panes, paneCountRef, running);
            return;
        case SDL_KEYUP:
            (void)owns_key_edge(&event->key, true);
            return;
        case SDL_TEXTINPUT: case SDL_TEXTEDITING:
            (void)handleRenameTextEvent(event);
            return;
        case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
        case SDL_DROPBEGIN: case SDL_DROPCOMPLETE:
            return;
        case SDL_DROPFILE: case SDL_DROPTEXT:
            SDL_free(event->drop.file);
            return;
        default:
            break; // Window lifecycle and quit still reach their owners.
        }
    }

    handleWindowGlobalEvents(event, panes, paneCountRef, running);  // simplified

    if (event->type == SDL_TEXTINPUT || event->type == SDL_TEXTEDITING) {
        ide_panel_fields_reconcile();
        UIPane* focused = getCoreState()->focusedPane;
        if (focused && focused->inputHandler && focused->inputHandler->onTextInput) {
            focused->inputHandler->onTextInput(focused, event);
        }
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        dispatch_key_press(event, false, panes, paneCountRef, running);
    }

    if (event->type == SDL_KEYUP) {
        if (!owns_key_edge(&event->key, false)) return;
        UIPane* focused = getCoreState()->focusedPane;
        if (focused && focused->role == PANE_ROLE_MENUBAR) handleMenuBarKeyboardInput(focused, event);
        if (focused && focused->role == PANE_ROLE_ICONBAR) handleIconBarKeyboardInput(focused, event);
        if (focused && (focused->role == PANE_ROLE_CONTROLPANEL ||
            (focused->role == PANE_ROLE_TOOLPANEL && (getActiveIcon()==ICON_PROJECT_FILES || getActiveIcon()==ICON_ERRORS || getActiveIcon()==ICON_VERSION_CONTROL || getActiveIcon()==ICON_LIBRARIES || getActiveIcon()==ICON_TASKS || getActiveIcon()==ICON_ASSET_MANAGER))))
            if (focused->inputHandler && focused->inputHandler->onKeyboard)
                focused->inputHandler->onKeyboard(focused, event);
    }
    bool was_resizing = gResizeDrag.active;
    handleResizeDragging(event, zones, zoneCount, panes, paneCountRef);
    if (gResizeDrag.active || was_resizing) {
        ide_button_scopes_cancel_except(NULL);
        cancelMouseInputCapture();
        ide_menu_bar_cancel();
        return;
    }

    if (event->type == SDL_MOUSEMOTION ||
        event->type == SDL_MOUSEBUTTONDOWN ||
        event->type == SDL_MOUSEBUTTONUP ||
        event->type == SDL_MOUSEWHEEL) {

        HeldKeyOwner before = key_owner(false);
        handleMouseInput(event, panes, paneCount);
        if (!same_key_scope(before, key_owner(isRenaming()))) input_cancel_held_keys();
        handleHoverUpdate(event, panes, paneCount);
    }
}

                            
