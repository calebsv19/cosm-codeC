#include "input_editor.h"
#include "ide/Panes/Editor/Input/editor_input_keyboard.h"
#include "ide/Panes/Editor/Input/editor_input_mouse.h"
#include "ide/Panes/Editor/editor_view.h"
#include "app/GlobalInfo/core_state.h"
#include "core/CommandBus/command_bus.h"
#include "core/CommandBus/command_metadata.h"

#include <stdlib.h>
#include <string.h>

static void enqueueEditorCommand(UIPane* pane, InputCommand cmd, SDL_Keymod keyMod, void* payload) {
    InputCommandMetadata meta = {
        .cmd = cmd,
        .originRole = pane ? pane->role : PANE_ROLE_UNKNOWN,
        .mouseX = -1,
        .mouseY = -1,
        .keyMod = keyMod,
        .targetPane = pane,
        .payload = payload
    };
    enqueueCommand(meta);
}



// === Keyboard ===
void handleEditorKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    SDL_Keymod mod = (SDL_Keymod)event->key.keysym.mod;

    // === CTRL shortcuts ===
    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_s: enqueueEditorCommand(pane, COMMAND_SAVE_FILE, mod, NULL); return;
            case SDLK_c: enqueueEditorCommand(pane, COMMAND_COPY, mod, NULL); return;
            case SDLK_x: enqueueEditorCommand(pane, COMMAND_CUT, mod, NULL); return;
            case SDLK_v: enqueueEditorCommand(pane, COMMAND_PASTE, mod, NULL); return;
            case SDLK_k: enqueueEditorCommand(pane, COMMAND_CUT, mod, NULL); return;
            case SDLK_u: enqueueEditorCommand(pane, COMMAND_PASTE, mod, NULL); return;
            case SDLK_o: enqueueEditorCommand(pane, COMMAND_SAVE_FILE, mod, NULL); return;
            case SDLK_i: enqueueEditorCommand(pane, COMMAND_DELETE, mod, NULL); return;
            case SDLK_a: enqueueEditorCommand(pane, COMMAND_SELECT_ALL, mod, NULL); return;
        }
    }
    // === ALT shortcuts ===
    if (mod & KMOD_ALT) {
        if (key == SDLK_MINUS)   { enqueueEditorCommand(pane, COMMAND_UNDO, mod, NULL); return; }
        if (key == SDLK_EQUALS)  { enqueueEditorCommand(pane, COMMAND_REDO, mod, NULL); return; }
    }

    // === Raw key behavior ===
    switch (key) {
        case SDLK_RETURN:
            enqueueEditorCommand(pane, COMMAND_INSERT_NEWLINE, mod, NULL);
            return;
        case SDLK_BACKSPACE:
            enqueueEditorCommand(pane, COMMAND_DELETE, mod, NULL);
            return;
        case SDLK_DELETE:
            enqueueEditorCommand(pane, COMMAND_DELETE_FORWARD, mod, NULL);
            return;
        case SDLK_TAB:
            enqueueEditorCommand(pane, COMMAND_INSERT_TAB, mod, NULL);
            return;
        default:
            break;
    }

    EditorKeyCommandPayload* payload = malloc(sizeof(*payload));
    if (!payload) return;
    payload->key = key;
    payload->mod = mod;
    payload->repeat = event->key.repeat;

    enqueueEditorCommand(pane, COMMAND_EDITOR_KEYDOWN, mod, payload);
}


void handleEditorTextInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_TEXTINPUT) return;

    EditorTextInputPayload* payload = malloc(sizeof(*payload));
    if (!payload) return;

    memset(payload->text, 0, sizeof(payload->text));
    strncpy(payload->text, event->text.text, SDL_TEXTINPUTEVENT_TEXT_SIZE - 1);

    enqueueEditorCommand(pane, COMMAND_EDITOR_TEXT_INPUT, 0, payload);
}


// === Mouse ===
void handleEditorMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !pane->editorView) return;

    IDECoreState* core = getCoreState();

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                int mx = event->button.x;
                int my = event->button.y;
                updateActiveEditorViewFromMouse(mx, my);
                EditorView* view = core->activeEditorView;
                if (!view) return;
                if (handleEditorScrollbarEvent(pane, event)) return; // early out if scrollbar clicked
                handleEditorMouseClick(pane, event, view);
            }
            break;

        case SDL_MOUSEMOTION:
            if (event->motion.state & SDL_BUTTON_LMASK) {
                if (handleEditorScrollbarEvent(pane, event)) return; // handle drag
                EditorView* view = core->activeEditorView;
                if (!view) return;
                handleEditorMouseDrag(pane, event, view);
            } else {
                EditorView* hover = getHoveredEditorView();
                if (hover && hover->type == VIEW_LEAF) {
                    setActiveEditorView(hover);
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (handleEditorScrollbarEvent(pane, event)) return;
            EditorView* view = core->activeEditorView;
            if (!view) return;
	    handleEditorMouseButtonUp(pane, event, view);
            break;

        default:
            break;
    }
}




// === Scroll ===
void handleEditorScrollInput(UIPane* pane, SDL_Event* event) {
    handleEditorScrollWheel(pane, event);
}

// === Hover (optional) ===
void handleEditorHoverInput(UIPane* pane, int x, int y) {
    // Reserved for future hover effects
}

// === Handler Struct ===
UIPaneInputHandler editorInputHandler = {
    .onCommand = NULL,  // Uses raw event logic, not InputCommand for now
    .onKeyboard = handleEditorKeyboardInput,
    .onMouse = handleEditorMouseInput,
    .onScroll = handleEditorScrollInput,
    .onHover = handleEditorHoverInput,
    .onTextInput = handleEditorTextInput
};
