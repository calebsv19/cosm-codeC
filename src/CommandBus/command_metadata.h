#pragma once

#include "../InputManager/input_command_enums.h"
#include "PaneInfo/pane_role.h"



#include <SDL2/SDL_keycode.h>      // for SDL_Keymod



typedef struct InputCommandMetadata{
    InputCommand cmd;        // The actual command (cut, paste, etc.)
    UIPaneRole originRole;   // Where the command came from (focused or hovered pane)
    int mouseX, mouseY;      // Mouse position if relevant
    SDL_Keymod keyMod;       // Held modifier keys at time of dispatch
    void* payload;           // Optional extra data (cast per use case)
} InputCommandMetadata;

