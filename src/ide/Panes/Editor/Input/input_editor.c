#include "input_editor.h"
#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/Input/editor_input_keyboard.h"
#include "ide/Panes/Editor/Input/editor_input_mouse.h"
#include "ide/Panes/Editor/editor_view.h"
#include "app/GlobalInfo/core_state.h"
#include "core/InputManager/input_macros.h"



// === Keyboard ===
void handleEditorKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    // === CTRL shortcuts ===
    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_s: CMD(COMMAND_SAVE_FILE); return;
            case SDLK_c: CMD(COMMAND_COPY); return;
            case SDLK_x: CMD(COMMAND_CUT); return;
            case SDLK_v: CMD(COMMAND_PASTE); return;
            case SDLK_k: CMD(COMMAND_CUT); return;
            case SDLK_u: CMD(COMMAND_PASTE); return;
            case SDLK_o: CMD(COMMAND_SAVE_FILE); return;
            case SDLK_i: CMD(COMMAND_DELETE); return;
        }
    }

    // === ALT shortcuts ===
    if (mod & KMOD_ALT) {
        if (key == SDLK_MINUS)   { CMD(COMMAND_UNDO); return; }
        if (key == SDLK_EQUALS)  { CMD(COMMAND_REDO); return; }
    }

    // === Raw key behavior ===
    switch (key) {
        case SDLK_RETURN:    CMD(COMMAND_INSERT_NEWLINE); return;
        case SDLK_BACKSPACE: CMD(COMMAND_DELETE); return;
//        case SDLK_TAB:       insertCharAtCursor('\t'); return;
        case SDLK_a:
            if (mod & KMOD_CTRL) {
                CMD(COMMAND_SELECT_ALL);
                return;
            }
            break;
        default: break;
    }

    // Pass raw input to editor immediately (for characters, arrows, shift actions)
    // NOTE: We intentionally bypass the command queue for low-latency typing
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (view) {
        handleEditorKeyDown(event, view, pane);
    }
}


void handleEditorTextInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_TEXTINPUT) return;

    EditorView* view = getCoreState()->activeEditorView;
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

//    EditorBuffer* buffer = file->buffer;
//    EditorState* state = &file->state;

    const char* text = event->text.text;
    for (int i = 0; text[i] != '\0'; i++) {
        insertCharAtCursor(text[i]);  // implement this insert
    }
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
