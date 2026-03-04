#ifndef CONTROL_PANEL_ADAPTER_H
#define CONTROL_PANEL_ADAPTER_H

#include "ide/Panes/panel_view_adapter.h"

const UIPanelViewAdapter* control_panel_view_adapter(void);
void renderControlPanelViaAdapter(UIPane* pane, bool hovered, struct IDECoreState* core);
void handleControlPanelViaAdapterCommand(UIPane* pane, InputCommandMetadata meta);

#endif
