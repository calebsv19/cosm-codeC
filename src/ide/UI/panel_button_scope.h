#ifndef IDE_PANEL_BUTTON_SCOPE_H
#define IDE_PANEL_BUTTON_SCOPE_H
#include "button_scope.h"
#include "kit_ui_text_presentation.h"
#include "panel_control_widgets.h"
typedef enum IDEPanelButtonGroup { IDE_PANEL_CONTROL, IDE_PANEL_ERRORS, IDE_PANEL_GIT, IDE_PANEL_LIBRARIES, IDE_PANEL_TASKS, IDE_PANEL_ASSETS, IDE_PANEL_TERMINAL, IDE_PANEL_BUTTON_GROUP_COUNT } IDEPanelButtonGroup;
/* A painted control snapshot is valid only for that pane geometry/workspace.
 * Input never reconstructs complex layout from stale cached rectangles. */
void ide_panel_buttons_domain(IDEPanelButtonGroup group, UIPane* pane, const void* owner, uint64_t revision);
void ide_panel_buttons_publish(IDEPanelButtonGroup group, UIPane* pane, const UIPanelTaggedRectList* hits, uint32_t disabled_id);
bool ide_panel_buttons_event(IDEPanelButtonGroup group, UIPane* pane, const SDL_Event* event, bool text_focused, uint32_t* activated);
enum { IDE_PANEL_TEXT_FIELD_ID=0x7fffffff };
void ide_panel_buttons_publish_field(IDEPanelButtonGroup group,UIPane* pane,const UIPanelTaggedRectList* hits,uint32_t disabled_id,SDL_Rect field);
bool ide_panel_buttons_field_focused(IDEPanelButtonGroup group,UIPane* pane);
bool ide_panel_buttons_paint_state(SDL_Rect rect, KitUiButtonState* state);
void ide_panel_text_publish(IDEPanelButtonGroup group, UIPane* pane, const KitUiTextPresentation* view,
    const char* source, KitUiTextMeasure measure, void* user);
bool ide_panel_text_pointer(IDEPanelButtonGroup group, UIPane* pane, const SDL_Event* event,
    const char* source, size_t* position, bool* extend);
#endif
