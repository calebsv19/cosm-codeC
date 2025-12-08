#include "input_terminal.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/Terminal/command_terminal.h"
#include "ide/Panes/Terminal/terminal.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/scroll_manager.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "app/GlobalInfo/core_state.h"

#include <SDL2/SDL.h>
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
    int cellH = terminal_cell_height();
    int lineHeight = cellH > 0 ? cellH : TERMINAL_LINE_HEIGHT;
    int padding = TERMINAL_PADDING;
    TermGrid* grid = terminal_active_grid();
    int totalLines = grid ? grid->rows : 0;

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

    int lineLen = terminal_line_length(lineIndex, true);

    int textX = pane->x + padding;
    int localX = mouseX;
    if (localX < textX) localX = textX;

    int col = 0;
    int prevWidth = 0;
    char* lineBuf = NULL;
    if (lineLen > 0) {
        lineBuf = (char*)malloc((size_t)lineLen + 1);
        if (lineBuf) {
            terminal_line_to_string(lineIndex, lineBuf, lineLen + 1, true);
            while (col < lineLen) {
                int width = getTextWidthN(lineBuf, col + 1);
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
        }
    }
    if (col > lineLen) col = lineLen;
    if (lineBuf) free(lineBuf);

    *outLine = lineIndex;
    *outColumn = col;
}

// --- Rename support (double-click interactive tab) ---
static Uint32 g_lastTabClickTicks = 0;
static int g_lastTabClickIndex = -1;
static const Uint32 kTabDoubleClickMs = 400;

static void handle_tab_rename_callback(const char* oldName, const char* newName, void* context) {
    (void)oldName;
    int idx = (int)(intptr_t)context;
    if (!newName) return;
    terminal_set_name(idx, newName);
}

static void maybe_begin_tab_rename(int tabIndex) {
    const char* name = NULL;
    bool isBuild = false, isRun = false;
    if (!terminal_session_info(tabIndex, &name, &isBuild, &isRun)) return;
    if (isBuild || isRun) return; // only interactive tabs are renameable
    const char* current = name ? name : "Terminal";
    beginRenameWithPrompt(
        "Terminal Name:",
        "Enter a name",
        current,
        handle_tab_rename_callback,
        NULL, // no validation needed
        (void*)(intptr_t)tabIndex,
        true   // accept unchanged
    );
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
                if (terminal_has_selection() && terminal_copy_selection_to_clipboard()) {
                    printf("[Terminal] Copied selection to clipboard.\n");
                } else {
                    const char sig = 0x03;
                    terminal_send_text(&sig, 1);
                }
                return;
            case SDLK_d: {
                const char eof = 0x04;
                terminal_send_text(&eof, 1);
                return;
            }
            default:
                break;
        }
    }

    switch (key) {
        case SDLK_RETURN: {
            const char cr = '\r';
            terminal_send_text(&cr, 1);
            break;
        }
        case SDLK_BACKSPACE: {
            const char del = 0x7f;
            terminal_send_text(&del, 1);
            break;
        }
        case SDLK_TAB: {
            const char tab = '\t';
            terminal_send_text(&tab, 1);
            break;
        }
        case SDLK_UP: {
            const char seq[] = {0x1b, '[', 'A'};
            terminal_send_text(seq, sizeof(seq));
            break;
        }
        case SDLK_DOWN: {
            const char seq[] = {0x1b, '[', 'B'};
            terminal_send_text(seq, sizeof(seq));
            break;
        }
        case SDLK_RIGHT: {
            const char seq[] = {0x1b, '[', 'C'};
            terminal_send_text(seq, sizeof(seq));
            break;
        }
        case SDLK_LEFT: {
            const char seq[] = {0x1b, '[', 'D'};
            terminal_send_text(seq, sizeof(seq));
            break;
        }
        default:
            printf("[Terminal] Unmapped keyboard input: %s\n", SDL_GetKeyName(key));
            break;
    }
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
                int hit = terminal_tab_hit(event->button.x, event->button.y);
                if (hit >= 0) {
                    Uint32 now = SDL_GetTicks();
                    bool doubleClick = (hit == g_lastTabClickIndex) &&
                                       (now - g_lastTabClickTicks < kTabDoubleClickMs);
                    g_lastTabClickTicks = now;
                    g_lastTabClickIndex = hit;
                    if (doubleClick) {
                        maybe_begin_tab_rename(hit);
                    } else {
                        terminal_set_active(hit);
                    }
                    return;
                }
                if (terminal_plus_hit(event->button.x, event->button.y)) {
                    terminal_create_interactive(getWorkspacePath());
                    return;
                }
                if (terminal_close_hit(event->button.x, event->button.y)) {
                    if (terminal_close_active_interactive()) return;
                }
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

void handleTerminalTextInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event || event->type != SDL_TEXTINPUT) return;
    const char* text = event->text.text;
    if (!text || text[0] == '\0') return;
    terminal_send_text(text, strlen(text));
}

UIPaneInputHandler terminalInputHandler = {
    .onCommand = handleTerminalCommand,
    .onKeyboard = handleTerminalKeyboardInput,
    .onMouse = handleTerminalMouseInput,
    .onScroll = handleTerminalScrollInput,
    .onHover = handleTerminalHoverInput,
    .onTextInput = handleTerminalTextInput,
};
