#ifndef TOOL_ERRORS_H
#define TOOL_ERRORS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

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
void errors_refresh_snapshot(void);

// Scroll helpers
struct PaneScrollState;
struct SDL_Rect;
struct PaneScrollState* errors_get_scroll_state(void);
struct SDL_Rect errors_get_scroll_track_rect(void);
struct SDL_Rect errors_get_scroll_thumb_rect(void);
void errors_set_scroll_rects(struct SDL_Rect track, struct SDL_Rect thumb);

// Shared layout helpers so rendering and input stay in sync.
TTF_Font* get_error_font(void);
void errors_get_layout_metrics(const UIPane* pane,
                               int* contentTop,
                               int* headerHeight,
                               int* diagHeight,
                               int* lineHeight);
void errors_set_control_button_rects(SDL_Rect allRect,
                                     SDL_Rect errorsRect,
                                     SDL_Rect warningsRect,
                                     SDL_Rect openAllRect,
                                     SDL_Rect closeAllRect);
bool errors_filter_all_enabled(void);
bool errors_filter_errors_enabled(void);
bool errors_filter_warnings_enabled(void);

#endif
