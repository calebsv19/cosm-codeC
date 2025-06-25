#ifndef RENDER_HELPERS_H
#define RENDER_HELPERS_H

#include <SDL2/SDL.h>

// Draw text with default styling
void drawText(int x, int y, const char* text);

// Draw clipped text within a max width
void drawClippedText(int x, int y, const char* text, int maxWidth);

// UI widgets
void renderButton(SDL_Rect rect, const char* label);

#endif

