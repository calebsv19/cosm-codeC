#ifndef SYMBOL_TREE_ADAPTER_H
#define SYMBOL_TREE_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/Analysis/analysis_symbols_store.h"

struct UITreeNode;
struct DirEntry;

typedef enum {
    SYMBOL_FILTER_MODE_SYMBOLS = 0,
    SYMBOL_FILTER_MODE_METHODS,
    SYMBOL_FILTER_MODE_TYPES,
    SYMBOL_FILTER_MODE_TAGS
} SymbolFilterMode;

typedef enum {
    SYMBOL_FILTER_SCOPE_ACTIVE = 0,
    SYMBOL_FILTER_SCOPE_OPEN,
    SYMBOL_FILTER_SCOPE_PROJECT
} SymbolFilterScope;

enum {
    SYMBOL_KIND_MASK_METHODS = 1u << 0,
    SYMBOL_KIND_MASK_TYPES   = 1u << 1,
    SYMBOL_KIND_MASK_VARS    = 1u << 2,
    SYMBOL_KIND_MASK_TAGS    = 1u << 3
};

typedef struct {
    SymbolFilterMode mode;
    uint32_t kind_mask;
    SymbolFilterScope scope;
    bool field_name;
    bool field_type;
    bool field_params;
    bool field_kind;
} SymbolFilterOptions;

void symbol_tree_cache_note_node(const struct UITreeNode* node);
void symbol_tree_cache_note_tree(const struct UITreeNode* root);
bool symbol_tree_query_matches_node(const struct UITreeNode* node,
                                    const char* query,
                                    const SymbolFilterOptions* options);
struct UITreeNode* symbol_tree_clone_filtered(const struct UITreeNode* root,
                                              const char* query,
                                              const SymbolFilterOptions* options);

struct UITreeNode* buildSymbolTreeForFile(const char* filePath,
                                          const AnalysisFileSymbols* entry,
                                          bool showAutoParamNames,
                                          bool showMacros);

struct UITreeNode* buildSymbolTreeForWorkspace(const struct DirEntry* projectRoot,
                                               const char* activeFilePath,
                                               bool showAutoParamNames,
                                               bool showMacros);

#endif // SYMBOL_TREE_ADAPTER_H
