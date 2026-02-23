#include "input_terminal.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/Terminal/command_terminal.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/UI/scroll_manager.h"
#include "core/Clipboard/clipboard.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Popup/popup_system.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

static bool g_terminal_drag_selecting = false;

static void terminal_send_ctrl_char(char c) {
    terminal_send_text(&c, 1);
}

static void terminal_send_esc_seq(const char* seq, size_t len) {
    terminal_send_text(seq, len);
}

static char* normalize_paste_text(const char* src, bool singleLine) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    size_t w = 0;
    bool prevSpace = false;

    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\r') continue;
        if (singleLine && c == '\n') c = ' ';

        if (singleLine && isspace(c)) {
            if (prevSpace) continue;
            out[w++] = ' ';
            prevSpace = true;
            continue;
        }

        out[w++] = (char)c;
        prevSpace = false;
    }

    if (singleLine && w > 0 && out[w - 1] == ' ') {
        --w;
    }
    out[w] = '\0';
    return out;
}

static void terminal_send_bracketed_paste(const char* text, size_t len) {
    if (!text || len == 0) return;
    static const char begin[] = "\x1b[200~";
    static const char end[] = "\x1b[201~";
    terminal_send_text(begin, sizeof(begin) - 1);
    terminal_send_text(text, len);
    terminal_send_text(end, sizeof(end) - 1);
}

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

static void terminal_scroll_history_lines(float lines) {
    PaneScrollState* scroll = terminal_get_scroll_state();
    if (!scroll) return;
    scroll_state_scroll_lines(scroll, lines);
    terminal_update_follow_flag(scroll);
}

static void terminal_scroll_history_page(int direction) {
    PaneScrollState* scroll = terminal_get_scroll_state();
    if (!scroll) return;
    float lineHeight = scroll->line_height_px > 0.0f ? scroll->line_height_px : (float)TERMINAL_LINE_HEIGHT;
    float pageLines = scroll->viewport_height_px / lineHeight;
    if (pageLines < 1.0f) pageLines = 1.0f;
    terminal_scroll_history_lines(pageLines * (float)direction);
}

static void terminal_compute_position(UIPane* pane, int mouseX, int mouseY, int* outLine, int* outColumn) {
    int cellH = terminal_cell_height();
    int lineHeight = cellH > 0 ? cellH : TERMINAL_LINE_HEIGHT;
    int padding = TERMINAL_PADDING;
    int headerH = TERMINAL_HEADER_HEIGHT;
    int totalLines = terminal_content_rows();

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

    int localY = mouseY - (pane->y + padding + headerH) + (int)intra;
    if (localY < 0) localY = 0;
    int lineOffset = (lineHeight > 0) ? localY / lineHeight : 0;

    int lineIndex = firstLine + lineOffset;
    if (lineIndex >= totalLines) lineIndex = totalLines - 1;
    if (lineIndex < 0) lineIndex = 0;

    int lineLen = terminal_line_length(lineIndex, true);
    int cellW = terminal_cell_width();
    if (cellW <= 0) cellW = 8;

    int textX = pane->x + padding;
    int localX = mouseX;
    if (localX < textX) localX = textX;

    int col = (localX - textX + (cellW / 2)) / cellW;
    if (col < 0) col = 0;
    if (col > lineLen) col = lineLen;

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
    IDECoreState* core = getCoreState();
    if (!core || core->focusedPane != pane) return;
    if (isRenaming() || core->popupPaneActive || isPopupVisible()) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    bool ctrlDown = (mod & KMOD_CTRL) != 0;
    bool guiDown = (mod & KMOD_GUI) != 0;
    bool shiftDown = (mod & KMOD_SHIFT) != 0;
    bool altDown = (mod & KMOD_ALT) != 0;

    bool cmdOrCtrlV = ((ctrlDown || guiDown) && key == SDLK_v);
    bool rawPasteShortcut = cmdOrCtrlV && shiftDown;
    bool pasteShortcut = cmdOrCtrlV || (shiftDown && key == SDLK_INSERT);
    if (pasteShortcut) {
        bool singleLinePaste = terminal_safe_paste_enabled() && !rawPasteShortcut;
        char* clip = clipboard_paste_text();
        if (clip && clip[0]) {
            char* normalized = normalize_paste_text(clip, singleLinePaste);
            if (normalized && normalized[0]) {
                terminal_send_bracketed_paste(normalized, strlen(normalized));
            }
            free(normalized);
        }
        clipboard_free_text(clip);
        return;
    }

    if (ctrlDown || guiDown) {
        switch (key) {
            case SDLK_l: CMD(COMMAND_CLEAR_TERMINAL); return;
            case SDLK_r: CMD(COMMAND_RUN_EXECUTABLE); return;
            case SDLK_a: terminal_send_ctrl_char(0x01); return;
            case SDLK_c:
                if (terminal_has_selection() && terminal_copy_selection_to_clipboard()) {
                    printf("[Terminal] Copied selection to clipboard.\n");
                } else {
                    terminal_send_ctrl_char(0x03);
                }
                return;
            case SDLK_d: terminal_send_ctrl_char(0x04); return;
            case SDLK_e: terminal_send_ctrl_char(0x05); return;
            case SDLK_k: terminal_send_ctrl_char(0x0B); return;
            case SDLK_u: terminal_send_ctrl_char(0x15); return;
            case SDLK_w: terminal_send_ctrl_char(0x17); return;
            default:
                break;
        }
    }
    if (altDown) {
        switch (key) {
            case SDLK_LEFT: {
                const char seq[] = {0x1b, 'b'};
                terminal_send_esc_seq(seq, sizeof(seq));
                return;
            }
            case SDLK_RIGHT: {
                const char seq[] = {0x1b, 'f'};
                terminal_send_esc_seq(seq, sizeof(seq));
                return;
            }
            case SDLK_BACKSPACE: {
                const char seq[] = {0x1b, 0x7f};
                terminal_send_esc_seq(seq, sizeof(seq));
                return;
            }
            default:
                break;
        }
    }

    switch (key) {
        case SDLK_PAGEUP:
            terminal_set_follow_output(false);
            terminal_scroll_history_page(+1);
            break;
        case SDLK_PAGEDOWN:
            terminal_set_follow_output(false);
            terminal_scroll_history_page(-1);
            break;
        case SDLK_HOME: {
            PaneScrollState* scroll = terminal_get_scroll_state();
            if (scroll) {
                scroll->offset_px = 0.0f;
                scroll->target_offset_px = 0.0f;
                terminal_set_follow_output(false);
            }
            break;
        }
        case SDLK_END: {
            PaneScrollState* scroll = terminal_get_scroll_state();
            if (scroll) {
                float maxOffset = scroll->content_height_px - scroll->viewport_height_px;
                if (maxOffset < 0.0f) maxOffset = 0.0f;
                scroll->offset_px = maxOffset;
                scroll->target_offset_px = maxOffset;
                terminal_set_follow_output(true);
            }
            break;
        }
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
            terminal_send_esc_seq(seq, sizeof(seq));
            break;
        }
        case SDLK_DOWN: {
            const char seq[] = {0x1b, '[', 'B'};
            terminal_send_esc_seq(seq, sizeof(seq));
            break;
        }
        case SDLK_RIGHT: {
            const char seq[] = {0x1b, '[', 'C'};
            terminal_send_esc_seq(seq, sizeof(seq));
            break;
        }
        case SDLK_LEFT: {
            const char seq[] = {0x1b, '[', 'D'};
            terminal_send_esc_seq(seq, sizeof(seq));
            break;
        }
        default:
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
                g_terminal_drag_selecting = true;
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
                g_terminal_drag_selecting = false;
            }
            break;

        default:
            break;
    }
}

bool terminal_tick_drag_autoscroll(UIPane* pane, float dt_seconds) {
    if (!pane) return false;
    if (!g_terminal_drag_selecting) return false;

    Uint32 mouseMask = SDL_GetMouseState(NULL, NULL);
    if ((mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) == 0) {
        g_terminal_drag_selecting = false;
        terminal_end_selection();
        return false;
    }

    int mx = 0;
    int my = 0;
    SDL_GetMouseState(&mx, &my);

    int viewportTop = pane->y + TERMINAL_HEADER_HEIGHT + TERMINAL_PADDING;
    int viewportBottom = pane->y + pane->h - TERMINAL_PADDING;
    int viewportH = viewportBottom - viewportTop;
    if (viewportH <= 0) return false;

    int edgeZone = viewportH / 4;
    if (edgeZone < 18) edgeZone = 18;
    if (edgeZone > 120) edgeZone = 120;

    float dir = 0.0f;
    float intensity = 0.0f;
    int topEdge = viewportTop + edgeZone;
    int bottomEdge = viewportBottom - edgeZone;
    if (my < topEdge) {
        dir = 1.0f;
        intensity = (float)(topEdge - my) / (float)edgeZone;
    } else if (my > bottomEdge) {
        dir = -1.0f;
        intensity = (float)(my - bottomEdge) / (float)edgeZone;
    }

    if (intensity > 1.0f) intensity = 1.0f;
    if (intensity < 0.0f) intensity = 0.0f;

    bool changed = false;
    if (dir != 0.0f && intensity > 0.0f) {
        // Gentle through most of the edge zone, then much faster only
        // near the extreme edge for long-range selection.
        const float logBase = logf(7.0f);
        float curved = (logBase > 0.0f) ? (logf(1.0f + 6.0f * intensity) / logBase) : intensity;
        if (curved < 0.0f) curved = 0.0f;
        if (curved > 1.0f) curved = 1.0f;

        float linesPerSecond = 1.5f + 22.0f * curved;
        if (intensity > 0.92f) {
            float t = (intensity - 0.92f) / 0.08f; // 0..1 in the last 8% near edge
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            linesPerSecond += t * t * 120.0f;
        }

        float lines = dir * linesPerSecond * (dt_seconds > 0.0f ? dt_seconds : 0.016f);
        if (lines > -0.001f && lines < 0.001f) {
            lines = dir * 0.15f;
        }
        terminal_set_follow_output(false);
        terminal_scroll_history_lines(lines);
        changed = true;
    }

    int line = 0;
    int column = 0;
    terminal_compute_position(pane, mx, my, &line, &column);
    terminal_update_selection(line, column);
    return changed;
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
