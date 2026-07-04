#include "ide/Panes/Editor/editor_live_diagnostics.h"

#include <stdlib.h>

#include "core/Analysis/analysis_scheduler.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_view.h"

void editor_live_diagnostics_request_for_commit(OpenFile* file,
                                                EditorEditCommitReason reason) {
    if (reason == EDIT_COMMIT_REASON_NONE) return;
    ControlPanelProjectionOptions controlOptions;
    control_panel_capture_projection_options(&controlOptions);
    if (!controlOptions.live_parse_enabled) return;
    if (!file || !file->filePath || !file->filePath[0] || !file->buffer) return;
    if (!file->isModified) return;

    size_t length = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &length);
    if (!snapshot) return;

    analysis_scheduler_request_live_buffer(file->filePath,
                                           snapshot,
                                           length,
                                           file->documentRevision,
                                           ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                           false);
    free(snapshot);
}
