#ifndef IDE_EDITOR_HEADER_CONTROLS_H
#define IDE_EDITOR_HEADER_CONTROLS_H
#include "editor_view.h"
#include "ide/UI/button_scope.h"
/* Pointer-only controls. Editor keyboard/text/selection retain their owners. */
bool ide_editor_header_close_enabled(UIPane* pane, EditorView* view);
void ide_editor_headers_begin(UIPane* pane);
void ide_editor_header_publish(EditorView* view, const KitUiInteractionControl* controls, uint32_t count);
void ide_editor_headers_end(void);
bool ide_editor_headers_event(UIPane* pane, const SDL_Event* event, EditorView** target, uint32_t* action);
KitUiButtonState ide_editor_header_state(EditorView* view, uint32_t id, bool selected);
void ide_editor_headers_forget_view(EditorView* view);
#define IDE_EDITOR_HEADER_CLOSE 1u
#define IDE_EDITOR_HEADER_TAB_BASE 100u
#endif
