#ifndef TOOL_ERRORS_H
#define TOOL_ERRORS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "ide/Panes/ToolPanels/Errors/errors_context_detail.h"
#include "ide/UI/panel_control_widgets.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
    const Diagnostic* diag;
    const char* path;
    int fileIndex;
    bool isHeader;
} FlatDiagRef;

typedef struct {
    int includeCount;
    int macroCount;
    bool hasDetails;
    bool hasNavigationTarget;
    char targetPath[1024];
    int targetLine;
    int targetColumn;
    char targetKind[16];
} ErrorsDiagnosticContextSummary;

typedef enum {
    ERROR_TOP_CONTROL_NONE = 0,
    ERROR_TOP_CONTROL_FILTER_ALL = 1,
    ERROR_TOP_CONTROL_FILTER_ERRORS = 2,
    ERROR_TOP_CONTROL_FILTER_WARNINGS = 3,
    ERROR_TOP_CONTROL_OPEN_ALL = 4,
    ERROR_TOP_CONTROL_CLOSE_ALL = 5,
    ERROR_TOP_CONTROL_CLEAR_SEARCH = 6
} ErrorTopControlId;

void handleErrorsEvent(UIPane* pane, SDL_Event* event);

int  getSelectedErrorDiag(void);
void setSelectedErrorDiag(int index);
bool is_error_selected(int idx);
const Diagnostic* errors_get_selected_diagnostic_ref(void);
int flatten_diagnostics(FlatDiagRef* out, int max);
void errors_refresh_snapshot(void);
void errors_select_all_visible(void);
bool errors_copy_selection_to_clipboard(void);

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
bool errors_get_detail_panel_rect(const UIPane* pane, SDL_Rect* outRect);
bool errors_get_detail_context_line_rect(const UIPane* pane, SDL_Rect* outRect);
bool errors_get_detail_context_row_rect(const UIPane* pane, int contextRow, SDL_Rect* outRect);
bool errors_get_detail_context_row_at_point(const UIPane* pane,
                                            int x,
                                            int y,
                                            ErrorsContextDetailRow* outRow);
bool errors_get_context_summary_for_diagnostic(const Diagnostic* diag,
                                               ErrorsDiagnosticContextSummary* outSummary);
bool errors_open_selected_context_target(void);
bool errors_open_context_detail_row(const ErrorsContextDetailRow* row);
UIPanelTaggedRectList* errors_get_control_hits(void);
bool errors_filter_all_enabled(void);
bool errors_filter_errors_enabled(void);
bool errors_filter_warnings_enabled(void);
const char* errors_get_search_query(void);
int errors_get_search_cursor(void);
bool errors_is_search_focused(void);
bool errors_has_active_search_query(void);
void errors_set_search_focused(bool focused);
bool errors_clear_search_query(void);
bool errors_handle_search_text_input(const SDL_Event* event);
bool errors_handle_search_edit_key(SDL_Keycode key);
void errors_search_cursor_end(void);
void errors_set_search_strip_layout(UIPanelTextFieldButtonStripLayout layout);
UIPanelTextFieldButtonStripLayout errors_get_search_strip_layout(void);

#endif
