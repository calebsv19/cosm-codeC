#ifndef CONTROL_PANEL_UNITS_TREE_H
#define CONTROL_PANEL_UNITS_TREE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/Analysis/analysis_units_store.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"

struct DirEntry;
struct UITreeNode;

bool control_panel_units_query_matches(const AnalysisUnitsAttachment* units,
                                       const char* query);
bool control_panel_units_dimension_matches(const AnalysisUnitsAttachment* units,
                                           unsigned int unitDimensionMask);

struct UITreeNode* control_panel_build_units_tree(const struct DirEntry* projectRoot,
                                                  const char* activeFilePath);

struct UITreeNode* control_panel_clone_units_tree_filtered(const struct UITreeNode* root,
                                                           const char* query,
                                                           SymbolFilterScope scope,
                                                           unsigned int unitDimensionMask);

bool control_panel_units_tree_node_focus_query(const struct UITreeNode* node,
                                               char* outQuery,
                                               size_t outQuerySize);

#endif
