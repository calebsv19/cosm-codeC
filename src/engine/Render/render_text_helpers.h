#ifndef RENDER_TEXT_HELPERS_H
#define RENDER_TEXT_HELPERS_H

#include <SDL2/SDL.h>
#include <stddef.h>

// Public helpers
int getTextWidth(const char* text);
int getTextWidthN(const char* text, int n);
size_t getTextClampedLength(const char* text, int maxWidth);


#endif
