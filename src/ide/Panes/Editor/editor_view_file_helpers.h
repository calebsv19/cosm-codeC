#ifndef EDITOR_VIEW_FILE_HELPERS_H
#define EDITOR_VIEW_FILE_HELPERS_H

#include "ide/Panes/Editor/editor_view.h"


/* Main-owner monotonic document identity, including reopened paths. */
uint64_t editor_next_document_revision(uint64_t previous);

#endif
