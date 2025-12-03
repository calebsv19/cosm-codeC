#include "input_terminal.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/Terminal/command_terminal.h"
#include "ide/Panes/Terminal/terminal.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/scroll_manager.h"

#include <stdio.h>
#include <string.h>

static void terminal_update_follow_flag(PaneScrollState* scroll) {
    if (!scroll) return;
    float maxOffset = scroll->content_height_px - scroll->viewport_height_px;
    if (maxOffset < 0.0f) maxOffset = 0.0f;
    float offset = scroll_state_get_offset(scroll);
    if (offset >= maxOffset - 1.0f) {
        terminal_set_follow_output(true);
    } else {
        terminal_set_follow_output(false);
    }
}

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

    PaneScrollState* scroll = terminal_get_scroll_state();
    float offset = scroll_state_get_offset(scroll);
    int firstLine = (lineHeight > 0) ? (int)(offset / (float)lineHeight) : 0;
    if (firstLine < 0) firstLine = 0;
    if (firstLine > totalLines) firstLine = totalLines;
    float intra = offset - (float)firstLine * (float)lineHeight;

    int localY = mouseY - (pane->y + padding) + (int)intra;
    if (localY < 0) localY = 0;
    int lineOffset = (lineHeight > 0) ? localY / lineHeight : 0;

    int lineIndex = firstLine + lineOffset;
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

    PaneScrollState* scroll = terminal_get_scroll_state();
    if (scroll) {
        SDL_Rect track, thumb;
        terminal_get_scroll_track(&track, &thumb);
        if (scroll_state_handle_mouse_drag(scroll, event, &track, &thumb)) {
            terminal_update_follow_flag(scroll);
            return;
        }
    }

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                int line = 0, column = 0;
                terminal_compute_position(pane, event->button.x, event->button.y, &line, &column);
                terminal_begin_selection(line, column);
                terminal_set_follow_output(false);
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
    PaneScrollState* scroll = terminal_get_scroll_state();
    if (scroll && scroll_state_handle_mouse_wheel(scroll, event)) {
        terminal_set_follow_output(false);
        terminal_update_follow_flag(scroll);
    }
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
