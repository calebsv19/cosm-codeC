#ifndef RENDER_TEXT_HELPERS_H
#define RENDER_TEXT_HELPERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>

// Public helpers
int getTextWidth(const char* text);
int getTextWidthN(const char* text, int n);
size_t getTextClampedLength(const char* text, int maxWidth);
int getTextWidthWithFont(const char* text, TTF_Font* font);
int getTextWidthNWithFont(const char* text, int n, TTF_Font* font);
int getTextWidthUTF8WithFont(const char* text, TTF_Font* font);


#endif
