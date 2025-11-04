#include "input_terminal.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/Terminal/command_terminal.h"
#include "ide/Panes/Terminal/terminal.h"
#include "engine/Render/render_text_helpers.h"

#include <stdio.h>
#include <string.h>

static void terminal_compute_position(UIPane* pane, int mouseX, int mouseY, int* outLine, int* outColumn) {
    int lineHeight = TERMINAL_LINE_HEIGHT;
    int padding = TERMINAL_PADDING;
    int totalLines = getTerminalLineCount();
    const char** lines = getTerminalBuffer();

    if (totalLines <= 0) {
        *outLine = 0;
        *outColumn = 0;
        return;
    }

    int visibleLines = (pane->h - 2 * padding) / lineHeight;
    if (visibleLines <= 0) visibleLines = 1;

    int start = (totalLines > visibleLines) ? totalLines - visibleLines : 0;

    int localY = mouseY - (pane->y + padding);
    if (localY < 0) localY = 0;
    int lineOffset = localY / lineHeight;
    if (lineOffset >= visibleLines) lineOffset = visibleLines - 1;

    int lineIndex = start + lineOffset;
    if (lineIndex >= totalLines) lineIndex = totalLines - 1;
    if (lineIndex < 0) lineIndex = 0;

    const char* text = lines[lineIndex];
    if (!text) text = "";
    int lineLen = (int)strlen(text);

    int textX = pane->x + padding;
    int localX = mouseX;
    if (localX < textX) localX = textX;

    int col = 0;
    int prevWidth = 0;
    while (col < lineLen) {
        int width = getTextWidthN(text, col + 1);
        int charLeft = textX + prevWidth;
        int charRight = textX + width;
        if (localX < charRight) {
            int leftDist = localX - charLeft;
            int rightDist = charRight - localX;
            if (rightDist < leftDist) {
                col++;
            }
            break;
        }
        prevWidth = width;
        col++;
    }
    if (col > lineLen) col = lineLen;

    *outLine = lineIndex;
    *outColumn = col;
}


void handleTerminalKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    bool ctrlDown = (mod & KMOD_CTRL) != 0;
    bool guiDown = (mod & KMOD_GUI) != 0;

    if (ctrlDown || guiDown) {
        switch (key) {
            case SDLK_l: CMD(COMMAND_CLEAR_TERMINAL); return;
            case SDLK_r: CMD(COMMAND_RUN_EXECUTABLE); return;
            case SDLK_c:
                if (terminal_copy_selection_to_clipboard()) {
                    printf("[Terminal] Copied selection to clipboard.\n");
                }
                return;
            default:
                break;
        }
    }

    printf("[Terminal] Unmapped keyboard input: %s\n", SDL_GetKeyName(key));
}


void handleTerminalMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane) return;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                int line = 0, column = 0;
                terminal_compute_position(pane, event->button.x, event->button.y, &line, &column);
                terminal_begin_selection(line, column);
            }
            break;

        case SDL_MOUSEMOTION:
            if (event->motion.state & SDL_BUTTON_LMASK) {
                int line = 0, column = 0;
                terminal_compute_position(pane, event->motion.x, event->motion.y, &line, &column);
                terminal_update_selection(line, column);
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                terminal_end_selection();
            }
            break;

        default:
            break;
    }
}


void handleTerminalScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleTerminalHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}

UIPaneInputHandler terminalInputHandler = {
    .onCommand = handleTerminalCommand,
    .onKeyboard = handleTerminalKeyboardInput,
    .onMouse = handleTerminalMouseInput,
    .onScroll = handleTerminalScrollInput,
    .onHover = handleTerminalHoverInput,
};
