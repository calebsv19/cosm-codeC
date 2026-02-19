#ifndef SYMBOL_TREE_ADAPTER_H
#define SYMBOL_TREE_ADAPTER_H

#include <stdbool.h>

#include "core/Analysis/analysis_symbols_store.h"

struct UITreeNode;
struct DirEntry;

void symbol_tree_cache_note_node(const struct UITreeNode* node);

struct UITreeNode* buildSymbolTreeForFile(const char* filePath,
                                          const AnalysisFileSymbols* entry,
                                          bool showAutoParamNames,
                                          bool showMacros);

struct UITreeNode* buildSymbolTreeForWorkspace(const struct DirEntry* projectRoot,
                                               const char* activeFilePath,
                                               bool showAutoParamNames,
                                               bool showMacros);

#endif // SYMBOL_TREE_ADAPTER_H
