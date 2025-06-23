// input_project.c
#include "ToolPanels/Project/input_tool_project.h"
#include "ToolPanels/Project/tool_project.h"
#include "CommandBus/command_bus.h"
#include "InputManager/input_macros.h"


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
        }
    }

    // === Return Key opens file ===
    if (key == SDLK_RETURN) {
        CMD(COMMAND_OPEN_FILE);
        return;
    }

    printf("[ProjectInput] Unmapped key: %s\n", SDL_GetKeyName(key));
}



void handleProjectFilesMouseInput(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT) return;

    int mx = event->button.x;
    int my = event->button.y;

    if (pointInRect(mx, my, projectBtnAddFile)) {
        CMD(COMMAND_NEW_FILE);
        return;
    }

    if (pointInRect(mx, my, projectBtnAddFolder)) {
        CMD(COMMAND_NEW_FOLDER);
        return;
    }

    if (pointInRect(mx, my, projectBtnDelete)) {
        CMD(COMMAND_DELETE_ENTRY);
        return;
    }

    handleProjectFilesClick(pane, mx);
}

void handleProjectFilesScrollInput(UIPane* pane, SDL_Event* event) {
    // Optional scroll logic
}

void handleProjectFilesHoverInput(UIPane* pane, int x, int y) {
    updateHoveredMousePosition(x, y);
}

