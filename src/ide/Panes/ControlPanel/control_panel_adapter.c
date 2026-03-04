#include "ide/Panes/ControlPanel/control_panel_adapter.h"

#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ControlPanel/render_control_panel.h"
#include "ide/Panes/ControlPanel/input_control_panel.h"
#include "ide/Panes/ControlPanel/command_control_panel.h"

static const UIPanelViewAdapter s_controlPanelAdapter = {
    .debug_name = "control_panel",
    .render = renderControlPanelContents,
    .onCommand = handleControlPanelCommand,
    .onKeyboard = handleControlPanelKeyboardInput,
    .onMouse = handleControlPanelMouseInput,
    .onScroll = handleControlPanelScrollInput,
    .onHover = handleControlPanelHoverInput,
    .onTextInput = handleControlPanelTextInput,
};

const UIPanelViewAdapter* control_panel_view_adapter(void) {
    return &s_controlPanelAdapter;
}

void renderControlPanelViaAdapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    control_panel_prepare_for_render(core);
    ui_panel_view_adapter_render(control_panel_view_adapter(), pane, hovered, core);
}

void handleControlPanelViaAdapterCommand(UIPane* pane, InputCommandMetadata meta) {
    ui_panel_view_adapter_command(control_panel_view_adapter(), pane, meta);
}
