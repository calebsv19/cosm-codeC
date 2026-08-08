#ifndef EDITOR_SESSION_CONTROL_PANEL_H
#define EDITOR_SESSION_CONTROL_PANEL_H

#include <stdbool.h>
#include <json-c/json.h>

#include "ide/Panes/ControlPanel/control_panel.h"

json_object* editor_session_control_panel_serialize(const ControlPanelPersistState* state);
bool editor_session_control_panel_deserialize(json_object* payload,
                                              ControlPanelPersistState* inout_state);

#endif
