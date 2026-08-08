#ifndef CONTROL_PANEL_ACTIVE_FILE_H
#define CONTROL_PANEL_ACTIVE_FILE_H

struct IDECoreState;
struct OpenFile;

const struct OpenFile* control_panel_resolve_active_open_file(const struct IDECoreState* core);
const char* control_panel_resolve_active_file_path(const struct IDECoreState* core);

#endif
