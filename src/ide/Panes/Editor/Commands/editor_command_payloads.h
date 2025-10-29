#pragma once

#include <SDL2/SDL.h>

typedef struct EditorKeyCommandPayload {
    SDL_Keycode key;
    SDL_Keymod mod;
    Uint8 repeat;
} EditorKeyCommandPayload;

typedef struct EditorTextInputPayload {
    char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
} EditorTextInputPayload;

