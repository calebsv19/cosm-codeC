#ifndef IDE_PANEL_TEXT_PRESENTATION_H
#define IDE_PANEL_TEXT_PRESENTATION_H
#include "kit_ui_text_presentation.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
/* Logical UI coordinates; source text is borrowed read-only, never rewritten. */
bool ide_panel_text_presentation_build(KitUiTextPresentation* view, const char* text,
    int cursor, SDL_Rect viewport, float line_height, bool focused, KitUiTextMeasure measure, void* user);
#endif
