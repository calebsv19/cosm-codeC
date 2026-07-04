#ifndef EDITOR_LIVE_DIAGNOSTICS_H
#define EDITOR_LIVE_DIAGNOSTICS_H

#include "ide/Panes/Editor/editor_edit_transaction_core.h"

struct OpenFile;

void editor_live_diagnostics_request_for_commit(struct OpenFile* file,
                                                EditorEditCommitReason reason);

#endif // EDITOR_LIVE_DIAGNOSTICS_H
