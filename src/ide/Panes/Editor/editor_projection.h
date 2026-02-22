#ifndef EDITOR_PROJECTION_H
#define EDITOR_PROJECTION_H

#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"

void editor_projection_rebuild(OpenFile* file,
                               const char* query,
                               const SymbolFilterOptions* options);

#endif // EDITOR_PROJECTION_H
