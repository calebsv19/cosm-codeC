// input_project.c
#include "ide/Panes/ToolPanels/Project/input_tool_project.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/Project/rename_callbacks.h"
#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/CommandBus/command_bus.h"
#include "core/InputManager/input_macros.h"
#include "core/InputManager/UserInput/rename_flow.h"

#include <stdlib.h>

static bool pointInRect(int x, int y, SDL_Rect r) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}


void handleProjectFilesKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    // === Rename mode input ===
    if (renamingEntry) {
        if (key == SDLK_RETURN) {
            CMD(COMMAND_CONFIRM_RENAME);
            return;
        } else if (key == SDLK_ESCAPE) {
            CMD(COMMAND_CANCEL_RENAME);
            return;
        } else if (key == SDLK_BACKSPACE) {
            int len = strlen(renameBuffer);
            if (len > 0) renameBuffer[len - 1] = '\0';
            return;
        } else {
            char ch = event->key.keysym.sym;
            if (ch >= 32 && ch < 127 && strlen(renameBuffer) < sizeof(renameBuffer) - 1) {
                int len = strlen(renameBuffer);
                renameBuffer[len] = ch;
                renameBuffer[len + 1] = '\0';
            }
            return;
        }
    }

    // === CTRL Shortcuts ===
    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_n: CMD(COMMAND_NEW_FILE); return;
            case SDLK_r: CMD(COMMAND_RENAME_FILE); return;
            case SDLK_o: CMD(COMMAND_OPEN_FILE); return;
            case SDLK_d:
                if (selectedFile) { CMD(COMMAND_DELETE_FILE); }
                else if (selectedDirectory && selectedDirectory != projectRoot) { CMD(COMMAND_DELETE_FOLDER); }
                return;
        }
    }

    // === Return Key opens file ===
    if (key == SDLK_RETURN) {
        CMD(COMMAND_OPEN_FILE);
        return;
    }

    if (key == SDLK_DELETE) {
        if (selectedFile) {
            CMD(COMMAND_DELETE_FILE);
        } else if (selectedDirectory && selectedDirectory != projectRoot) {
            CMD(COMMAND_DELETE_FOLDER);
        }
        return;
    }

    printf("[ProjectInput] Unmapped key: %s\n", SDL_GetKeyName(key));
}



void handleProjectFilesMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane) return;

    int mx = 0;
    int my = 0;
    if (event->type == SDL_MOUSEMOTION) {
        mx = event->motion.x;
        my = event->motion.y;
    } else {
        mx = event->button.x;
        my = event->button.y;
    }

    // ⏱ Right-click triggers rename
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT) {
        if (hoveredEntry) {
            if (hoveredEntry->type == ENTRY_FOLDER) {
                selectDirectoryEntry(hoveredEntry);
            } else {
                selectFileEntry(hoveredEntry);
            }
            printf("starting rename\n");
            beginRenameWithPrompt(
                "Rename Entry:",
                "Name already exists",
                hoveredEntry->name,
                handleProjectFileRenameCallback,
                (RenameValidateFn)isRenameValid,
                hoveredEntry,
                false
            );

            return;
        }
    }

    // Left click handling (existing)
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (pointInRect(mx, my, projectBtnAddFile)) {
            CMD(COMMAND_NEW_FILE);
            return;
        }

        if (pointInRect(mx, my, projectBtnDeleteFile)) {
            CMD(COMMAND_DELETE_FILE);
            return;
        }

        if (pointInRect(mx, my, projectBtnAddFolder)) {
            CMD(COMMAND_NEW_FOLDER);
            return;
        }

        if (pointInRect(mx, my, projectBtnDeleteFolder)) {
            CMD(COMMAND_DELETE_FOLDER);
            return;
        }

        if (hoveredEntry && hoveredEntry->type == ENTRY_FILE) {
            beginProjectDrag(hoveredEntry, &hoveredEntryRect, mx, my);
        }
        handleProjectFilesClick(pane, mx);
    }
}


void handleProjectFilesScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_MOUSEWHEEL) return;
    PaneScrollState* scroll = project_get_scroll_state(pane);
    if (!scroll) return;
    float lines = (float)event->wheel.y;
    if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
        lines = -lines;
    }
    scroll_state_scroll_lines(scroll, lines);
}

void handleProjectFilesHoverInput(UIPane* pane, int x, int y) {
    updateHoveredMousePosition(x, y);
}
