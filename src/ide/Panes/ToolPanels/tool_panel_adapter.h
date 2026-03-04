#ifndef TOOL_PANEL_ADAPTER_H
#define TOOL_PANEL_ADAPTER_H

#include "ide/Panes/panel_view_adapter.h"
#include "ide/Panes/IconBar/icon_bar.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum ToolPanelStateSlot {
    TOOL_PANEL_STATE_SLOT_GIT = 0,
    TOOL_PANEL_STATE_SLOT_LIBRARIES,
    TOOL_PANEL_STATE_SLOT_ASSETS,
    TOOL_PANEL_STATE_SLOT_BUILD_OUTPUT_UI,
    TOOL_PANEL_STATE_SLOT_BUILD_OUTPUT_PANEL,
    TOOL_PANEL_STATE_SLOT_ERRORS,
    TOOL_PANEL_STATE_SLOT_TASKS,
    TOOL_PANEL_STATE_SLOT_PROJECT,
    TOOL_PANEL_STATE_SLOT_COUNT
} ToolPanelStateSlot;

void tool_panel_attach_controller(UIPane* pane);
UIPane* tool_panel_bind_dispatch_pane(UIPane* pane);
void tool_panel_restore_dispatch_pane(UIPane* pane);
void* tool_panel_ensure_state_slot_for_pane(UIPane* pane,
                                            ToolPanelStateSlot slot,
                                            size_t size,
                                            void (*init_state)(void*),
                                            void (*destroy_state)(void*));
void* tool_panel_ensure_state_slot(ToolPanelStateSlot slot,
                                   size_t size,
                                   void (*init_state)(void*),
                                   void (*destroy_state)(void*));
void* tool_panel_resolve_state_slot(ToolPanelStateSlot slot,
                                    size_t size,
                                    void (*init_state)(void*),
                                    void (*destroy_state)(void*),
                                    void* bootstrap_state,
                                    bool* bootstrap_initialized);

const UIPanelViewAdapter* tool_panel_adapter_for_icon(IconTool icon);
const UIPanelViewAdapter* tool_panel_active_adapter(void);

#endif
