#ifndef TOOL_ERRORS_H
#define TOOL_ERRORS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include <SDL2/SDL.h>

typedef struct {
    const Diagnostic* diag;
    const char* path;
    int fileIndex;
    bool isHeader;
} FlatDiagRef;

void handleErrorsEvent(UIPane* pane, SDL_Event* event);

int  getSelectedErrorDiag(void);
void setSelectedErrorDiag(int index);
bool is_error_selected(int idx);
int flatten_diagnostics(FlatDiagRef* out, int max);

// Scroll helpers
struct PaneScrollState;
struct SDL_Rect;
struct PaneScrollState* errors_get_scroll_state(void);
struct SDL_Rect errors_get_scroll_track_rect(void);
struct SDL_Rect errors_get_scroll_thumb_rect(void);
void errors_set_scroll_rects(struct SDL_Rect track, struct SDL_Rect thumb);

#endif
