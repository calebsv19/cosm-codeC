#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "app/GlobalInfo/project.h"
#include "ide/UI/Trees/ui_tree_node.h"

static const char* basename_from_path(const char* path) {
    if (!path) return NULL;
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static const char* kind_label(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION: return "fn";
        case FISICS_SYMBOL_STRUCT: return "struct";
        case FISICS_SYMBOL_UNION: return "union";
        case FISICS_SYMBOL_ENUM: return "enum";
        case FISICS_SYMBOL_TYPEDEF: return "typedef";
        case FISICS_SYMBOL_VARIABLE: return "var";
        case FISICS_SYMBOL_FIELD: return "field";
        case FISICS_SYMBOL_ENUM_MEMBER: return "member";
        case FISICS_SYMBOL_MACRO: return "macro";
        default: return "symbol";
    }
}

static bool text_contains_ci(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack || !haystack[0]) return false;

    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    if (nlen > hlen) return false;

    for (size_t i = 0; i + nlen <= hlen; ++i) {
        size_t j = 0;
        while (j < nlen) {
            unsigned char hc = (unsigned char)haystack[i + j];
            unsigned char nc = (unsigned char)needle[j];
            if (tolower(hc) != tolower(nc)) break;
            ++j;
        }
        if (j == nlen) return true;
    }
    return false;
}

static bool is_auto_param_name(const char* name) {
    return name && strncmp(name, "__unnamed_param", 15) == 0;
}

static bool is_ignored_name(const char* name) {
    if (!name) return false;
    return strcmp(name, ".git") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".DS_Store") == 0;
}

static bool is_allowed_root_dir(const char* name) {
    if (!name) return false;
    return strcmp(name, "src") == 0 || strcmp(name, "include") == 0;
}

typedef struct {
    char* key;
    bool expanded;
} SymbolTreeExpandEntry;

static SymbolTreeExpandEntry* g_expandCache = NULL;
static size_t g_expandCount = 0;
static size_t g_expandCap = 0;

static const char* cache_key_for_label(const char* label) {
    static char key[256];
    if (!label) return NULL;
    snprintf(key, sizeof(key), "section:%s", label);
    return key;
}

static const char* cache_key_for_path(const char* path) {
    return path;
}

static bool expansion_cache_get(const char* key, bool* outExpanded) {
    if (!key) return false;
    for (size_t i = 0; i < g_expandCount; ++i) {
        if (g_expandCache[i].key && strcmp(g_expandCache[i].key, key) == 0) {
            if (outExpanded) *outExpanded = g_expandCache[i].expanded;
            return true;
        }
    }
    return false;
}

static void expansion_cache_set(const char* key, bool expanded) {
    if (!key) return;
    for (size_t i = 0; i < g_expandCount; ++i) {
        if (g_expandCache[i].key && strcmp(g_expandCache[i].key, key) == 0) {
            g_expandCache[i].expanded = expanded;
            return;
        }
    }
    if (g_expandCount >= g_expandCap) {
        size_t newCap = (g_expandCap == 0) ? 64 : g_expandCap * 2;
        SymbolTreeExpandEntry* grown = (SymbolTreeExpandEntry*)realloc(g_expandCache, newCap * sizeof(SymbolTreeExpandEntry));
        if (!grown) return;
        g_expandCache = grown;
        g_expandCap = newCap;
    }
    g_expandCache[g_expandCount].key = strdup(key);
    g_expandCache[g_expandCount].expanded = expanded;
    if (g_expandCache[g_expandCount].key) {
        g_expandCount++;
    }
}

void symbol_tree_cache_note_node(const UITreeNode* node) {
    if (!node) return;
    if (!(node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION)) return;
    const char* key = NULL;
    if (node->fullPath && node->fullPath[0]) {
        key = cache_key_for_path(node->fullPath);
    } else if (node->label) {
        key = cache_key_for_label(node->label);
    }
    if (!key) return;
    expansion_cache_set(key, node->isExpanded);
}

static bool symbol_kind_matches_mode(FisicsSymbolKind kind, SymbolFilterMode mode) {
    switch (mode) {
        case SYMBOL_FILTER_MODE_METHODS:
            return kind == FISICS_SYMBOL_FUNCTION;
        case SYMBOL_FILTER_MODE_TYPES:
            return kind == FISICS_SYMBOL_STRUCT ||
                   kind == FISICS_SYMBOL_UNION ||
                   kind == FISICS_SYMBOL_ENUM ||
                   kind == FISICS_SYMBOL_TYPEDEF;
        case SYMBOL_FILTER_MODE_TAGS:
            // Tag metadata is not wired yet; keep visibility broad for now.
            return true;
        case SYMBOL_FILTER_MODE_SYMBOLS:
        default:
            return true;
    }
}

static uint32_t symbol_kind_mask_for_symbol(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION:
            return SYMBOL_KIND_MASK_METHODS;
        case FISICS_SYMBOL_STRUCT:
        case FISICS_SYMBOL_UNION:
        case FISICS_SYMBOL_ENUM:
        case FISICS_SYMBOL_TYPEDEF:
            return SYMBOL_KIND_MASK_TYPES;
        case FISICS_SYMBOL_VARIABLE:
        case FISICS_SYMBOL_FIELD:
        case FISICS_SYMBOL_ENUM_MEMBER:
            return SYMBOL_KIND_MASK_VARS;
        default:
            return 0u;
    }
}

static bool symbol_kind_matches_options(FisicsSymbolKind kind, const SymbolFilterOptions* options) {
    if (options && options->kind_mask != 0u) {
        if ((options->kind_mask & SYMBOL_KIND_MASK_TAGS) != 0u) {
            // Tag metadata isn't wired yet; keep TAGS broad for now.
            return true;
        }
        uint32_t symbolMask = symbol_kind_mask_for_symbol(kind);
        return (symbolMask & options->kind_mask) != 0u;
    }
    SymbolFilterMode mode = options ? options->mode : SYMBOL_FILTER_MODE_SYMBOLS;
    return symbol_kind_matches_mode(kind, mode);
}

static bool is_scope_excluded_bucket(const UITreeNode* node, SymbolFilterScope scope) {
    if (!node || node->depth != 1 || !node->label) return false;
    if (strcmp(node->label, "Active File") == 0) {
        return scope == SYMBOL_FILTER_SCOPE_PROJECT;
    }
    if (strcmp(node->label, "Project Files") == 0) {
        return scope == SYMBOL_FILTER_SCOPE_ACTIVE;
    }
    return false;
}

bool symbol_tree_query_matches_node(const UITreeNode* node,
                                    const char* query,
                                    const SymbolFilterOptions* options) {
    if (!node) return false;
    if (!query || !query[0]) return true;

    bool matchName = options ? options->field_name : true;
    bool matchType = options ? options->field_type : true;
    bool matchParams = options ? options->field_params : true;
    bool matchKind = options ? options->field_kind : true;
    if (!matchName && !matchType && !matchParams && !matchKind) {
        matchName = true;
    }

    const FisicsSymbol* sym = (const FisicsSymbol*)node->userData;
    if (!sym) {
        return text_contains_ci(node->label, query);
    }

    if (!symbol_kind_matches_options(sym->kind, options)) {
        return false;
    }

    if (matchName) {
        if (text_contains_ci(sym->name, query) || text_contains_ci(node->label, query)) return true;
    }
    if (matchKind && text_contains_ci(kind_label(sym->kind), query)) return true;
    if (matchType) {
        if (text_contains_ci(sym->return_type, query) || text_contains_ci(sym->parent_name, query)) return true;
    }
    if (matchParams) {
        for (size_t i = 0; i < sym->param_count; ++i) {
            const char* pType = (sym->param_types && sym->param_types[i]) ? sym->param_types[i] : NULL;
            const char* pName = (sym->param_names && sym->param_names[i]) ? sym->param_names[i] : NULL;
            if (text_contains_ci(pType, query) || text_contains_ci(pName, query)) {
                return true;
            }
        }
    }

    return false;
}

static bool is_collapsed_active_file_bucket(const UITreeNode* node, const char* query) {
    if (!node || !query || !query[0]) return false;
    if (node->depth != 1) return false;
    if (!node->label || strcmp(node->label, "Active File") != 0) return false;
    return !node->isExpanded;
}

static UITreeNode* clone_filtered_node(const UITreeNode* node,
                                       const char* query,
                                       const SymbolFilterOptions* options,
                                       bool* outDescendantMatch) {
    if (!node) return NULL;
    if (is_collapsed_active_file_bucket(node, query)) {
        if (outDescendantMatch) *outDescendantMatch = false;
        return NULL;
    }
    if (options && is_scope_excluded_bucket(node, options->scope)) {
        if (outDescendantMatch) *outDescendantMatch = false;
        return NULL;
    }

    bool selfMatches = symbol_tree_query_matches_node(node, query, options);
    UITreeNode* clone = createTreeNode(node->label,
                                       node->type,
                                       node->color,
                                       node->fullPath,
                                       node->userData);
    if (!clone) return NULL;

    bool kept = selfMatches;
    bool descendantMatch = false;
    for (int i = 0; i < node->childCount; ++i) {
        UITreeNode* childClone = clone_filtered_node(node->children[i], query, options, NULL);
        if (childClone) {
            addChildNode(clone, childClone);
            kept = true;
            descendantMatch = true;
        }
    }

    if (!kept) {
        freeTreeNodeRecursive(clone);
        if (outDescendantMatch) *outDescendantMatch = false;
        return NULL;
    }

    clone->isExpanded = node->isExpanded;
    if (clone->type == TREE_NODE_FOLDER || clone->type == TREE_NODE_SECTION) {
        bool cachedExpanded = false;
        const char* key = NULL;
        if (node->fullPath && node->fullPath[0]) {
            key = cache_key_for_path(node->fullPath);
        } else if (node->label) {
            key = cache_key_for_label(node->label);
        }
        if (key && expansion_cache_get(key, &cachedExpanded)) {
            clone->isExpanded = cachedExpanded;
        }
        if (query && query[0] && descendantMatch) {
            clone->isExpanded = true;
        }
    }

    if (outDescendantMatch) *outDescendantMatch = selfMatches || descendantMatch;
    return clone;
}

struct UITreeNode* symbol_tree_clone_filtered(const struct UITreeNode* root,
                                              const char* query,
                                              const SymbolFilterOptions* options) {
    if (!root) return NULL;
    bool matched = false;
    if (!query || !query[0]) {
        return clone_filtered_node(root, "", options, &matched);
    }
    return clone_filtered_node(root, query, options, &matched);
}

static const AnalysisFileSymbols* find_symbols_for_path(const char* filePath) {
    if (!filePath) return NULL;
    size_t count = analysis_symbols_store_file_count();
    for (size_t i = 0; i < count; ++i) {
        const AnalysisFileSymbols* entry = analysis_symbols_store_file_at(i);
        if (entry && entry->path && strcmp(entry->path, filePath) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void free_tree_symbol_user_data(void* ptr) {
    FisicsSymbol* sym = (FisicsSymbol*)ptr;
    if (!sym) return;
    free((char*)sym->name);
    free((char*)sym->file_path);
    free((char*)sym->parent_name);
    free((char*)sym->return_type);
    if (sym->param_types) {
        for (size_t i = 0; i < sym->param_count; ++i) {
            free((char*)sym->param_types[i]);
        }
        free((void*)sym->param_types);
    }
    if (sym->param_names) {
        for (size_t i = 0; i < sym->param_count; ++i) {
            free((char*)sym->param_names[i]);
        }
        free((void*)sym->param_names);
    }
    free(sym);
}

static FisicsSymbol* clone_symbol_for_tree(const FisicsSymbol* src) {
    if (!src) return NULL;
    FisicsSymbol* out = (FisicsSymbol*)calloc(1, sizeof(FisicsSymbol));
    if (!out) return NULL;
    *out = *src;
    out->name = src->name ? strdup(src->name) : NULL;
    out->file_path = src->file_path ? strdup(src->file_path) : NULL;
    out->parent_name = src->parent_name ? strdup(src->parent_name) : NULL;
    out->return_type = src->return_type ? strdup(src->return_type) : NULL;
    out->param_types = NULL;
    out->param_names = NULL;
    if (src->param_count > 0) {
        if (src->param_types) {
            out->param_types = (const char**)calloc(src->param_count, sizeof(char*));
            if (!out->param_types) {
                free_tree_symbol_user_data(out);
                return NULL;
            }
            for (size_t i = 0; i < src->param_count; ++i) {
                if (src->param_types[i]) {
                    ((char**)out->param_types)[i] = strdup(src->param_types[i]);
                }
            }
        }
        if (src->param_names) {
            out->param_names = (const char**)calloc(src->param_count, sizeof(char*));
            if (!out->param_names) {
                free_tree_symbol_user_data(out);
                return NULL;
            }
            for (size_t i = 0; i < src->param_count; ++i) {
                if (src->param_names[i]) {
                    ((char**)out->param_names)[i] = strdup(src->param_names[i]);
                }
            }
        }
    }
    return out;
}

static void build_symbol_label(char* out, size_t outSize, const FisicsSymbol* sym) {
    const char* name = (sym && sym->name) ? sym->name : "<anonymous>";
    const char* kind = kind_label(sym ? sym->kind : FISICS_SYMBOL_UNKNOWN);
    if (sym && sym->kind == FISICS_SYMBOL_FUNCTION) {
        const char* ret = sym->return_type ? sym->return_type : "void";
        snprintf(out, outSize, "%s %s : %s", kind, name, ret);
    } else if (sym && sym->return_type && sym->return_type[0]) {
        snprintf(out, outSize, "%s %s : %s", kind, name, sym->return_type);
    } else {
        snprintf(out, outSize, "%s %s", kind, name);
    }
}

static void build_file_label(char* out, size_t outSize, const char* filePath, const char* projectRootPath) {
    if (!filePath || !filePath[0]) {
        snprintf(out, outSize, "<unknown>");
        return;
    }
    if (projectRootPath && projectRootPath[0]) {
        size_t rootLen = strlen(projectRootPath);
        if (strncmp(filePath, projectRootPath, rootLen) == 0) {
            const char* rel = filePath + rootLen;
            if (rel[0] == '/') rel++;
            if (rel[0]) {
                snprintf(out, outSize, "%s", rel);
                return;
            }
        }
    }
    const char* base = basename_from_path(filePath);
    snprintf(out, outSize, "%s", base ? base : filePath);
}

static void append_symbols_to_file(UITreeNode* fileNode,
                                   const AnalysisFileSymbols* entry,
                                   const char* filePath,
                                   bool showAutoParamNames,
                                   bool showMacros) {
    if (!fileNode) return;
    if (!entry || entry->count == 0 || !entry->symbols) {
        UITreeNode* empty = createTreeNode("No symbols", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        addChildNode(fileNode, empty);
        return;
    }

    typedef struct {
        const char* name;
        FisicsSymbolKind kind;
        UITreeNode* node;
    } ParentEntry;

    ParentEntry* parents = NULL;
    size_t parentCount = 0;
    size_t parentCap = 0;

    for (size_t i = 0; i < entry->count; ++i) {
        const FisicsSymbol* sym = &entry->symbols[i];
        if (!sym || (sym->parent_name && sym->parent_name[0])) continue;
        if (!showMacros && sym->kind == FISICS_SYMBOL_MACRO) continue;

        char label[512];
        build_symbol_label(label, sizeof(label), sym);

        FisicsSymbol* symCopy = clone_symbol_for_tree(sym);
        UITreeNode* symNode = createTreeNode(label, TREE_NODE_SECTION, NODE_COLOR_DEFAULT, filePath, (void*)symCopy);
        if (symCopy) {
            setTreeNodeUserDataFreeFn(symNode, free_tree_symbol_user_data);
        }
        symNode->isExpanded = false;

        if (sym->kind == FISICS_SYMBOL_FUNCTION && sym->param_count > 0) {
            for (size_t p = 0; p < sym->param_count; ++p) {
                const char* paramName = sym->param_names ? sym->param_names[p] : NULL;
                const char* paramType = sym->param_types ? sym->param_types[p] : NULL;
                if (!showAutoParamNames && is_auto_param_name(paramName)) {
                    paramName = NULL;
                }
                char paramLabel[512];
                if (paramName && paramName[0]) {
                    if (paramType && paramType[0]) {
                        snprintf(paramLabel, sizeof(paramLabel), "%s : %s", paramName, paramType);
                    } else {
                        snprintf(paramLabel, sizeof(paramLabel), "%s", paramName);
                    }
                } else if (paramType && paramType[0]) {
                    snprintf(paramLabel, sizeof(paramLabel), "%s", paramType);
                } else {
                    snprintf(paramLabel, sizeof(paramLabel), "<param>");
                }
                UITreeNode* paramNode = createTreeNode(paramLabel, TREE_NODE_FILE, NODE_COLOR_DEFAULT, filePath, NULL);
                addChildNode(symNode, paramNode);
            }
        }

        if (parentCount == parentCap) {
            size_t nextCap = parentCap ? parentCap * 2 : 8;
            ParentEntry* grown = realloc(parents, nextCap * sizeof(ParentEntry));
            if (grown) {
                parents = grown;
                parentCap = nextCap;
            }
        }
        if (parents && parentCount < parentCap) {
            parents[parentCount++] = (ParentEntry){ .name = sym->name, .kind = sym->kind, .node = symNode };
        }

        addChildNode(fileNode, symNode);
    }

    for (size_t i = 0; i < entry->count; ++i) {
        const FisicsSymbol* sym = &entry->symbols[i];
        if (!sym || !sym->parent_name || !sym->parent_name[0]) continue;
        if (!showMacros && sym->kind == FISICS_SYMBOL_MACRO) continue;

        UITreeNode* parentNode = NULL;
        for (size_t p = 0; p < parentCount; ++p) {
            if (parents[p].kind == sym->parent_kind &&
                parents[p].name &&
                strcmp(parents[p].name, sym->parent_name) == 0) {
                parentNode = parents[p].node;
                break;
            }
        }
        if (!parentNode) {
            continue;
        }

        char label[512];
        build_symbol_label(label, sizeof(label), sym);
        FisicsSymbol* symCopy = clone_symbol_for_tree(sym);
        UITreeNode* childNode = createTreeNode(label, TREE_NODE_FILE, NODE_COLOR_DEFAULT, filePath, (void*)symCopy);
        if (symCopy) {
            setTreeNodeUserDataFreeFn(childNode, free_tree_symbol_user_data);
        }
        addChildNode(parentNode, childNode);
    }

    free(parents);
}

static UITreeNode* build_project_tree_node(const DirEntry* entry,
                                           const char* activeFilePath,
                                           bool showAutoParamNames,
                                           bool showMacros,
                                           int depth) {
    if (!entry) return NULL;
    if (entry->name && is_ignored_name(entry->name)) {
        return NULL;
    }
    if (depth == 1 && entry->name && !is_allowed_root_dir(entry->name)) {
        return NULL;
    }

    if (entry->type == ENTRY_FILE) {
        UITreeNode* fileNode = createTreeNode(entry->name, TREE_NODE_SECTION, NODE_COLOR_DEFAULT, entry->path, NULL);
        if (!fileNode) return NULL;
        bool cachedExpanded = false;
        const char* key = cache_key_for_path(entry->path);
        if (key && expansion_cache_get(key, &cachedExpanded)) {
            fileNode->isExpanded = cachedExpanded;
        } else {
            fileNode->isExpanded = false;
        }
        analysis_symbols_store_lock();
        const AnalysisFileSymbols* entrySymbols = find_symbols_for_path(entry->path);
        append_symbols_to_file(fileNode, entrySymbols, entry->path, showAutoParamNames, showMacros);
        analysis_symbols_store_unlock();
        return fileNode;
    }

    UITreeNode* dirNode = createTreeNode(entry->name, TREE_NODE_FOLDER, NODE_COLOR_SECTION, entry->path, NULL);
    if (!dirNode) return NULL;
    bool cachedExpanded = false;
    const char* key = cache_key_for_path(entry->path);
    if (key && expansion_cache_get(key, &cachedExpanded)) {
        dirNode->isExpanded = cachedExpanded;
    } else {
        dirNode->isExpanded = entry->isExpanded;
    }

    for (int i = 0; i < entry->childCount; ++i) {
        UITreeNode* child = build_project_tree_node(entry->children[i], activeFilePath, showAutoParamNames, showMacros, depth + 1);
        if (child) {
            addChildNode(dirNode, child);
        }
    }

    if (dirNode->childCount == 0) {
        freeTreeNodeRecursive(dirNode);
        return NULL;
    }
    return dirNode;
}

struct UITreeNode* buildSymbolTreeForFile(const char* filePath,
                                          const AnalysisFileSymbols* entry,
                                          bool showAutoParamNames,
                                          bool showMacros) {
    UITreeNode* root = createTreeNode("Symbols", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!root) return NULL;
    root->isExpanded = true;

    if (!filePath || !filePath[0]) {
        UITreeNode* empty = createTreeNode("No active file", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        addChildNode(root, empty);
        return root;
    }

    const char* base = basename_from_path(filePath);
    char fileLabel[256];
    snprintf(fileLabel, sizeof(fileLabel), "%s", base ? base : filePath);
    UITreeNode* fileNode = createTreeNode(fileLabel, TREE_NODE_SECTION, NODE_COLOR_SECTION, filePath, NULL);
    fileNode->isExpanded = true;

    if (!entry || entry->count == 0 || !entry->symbols) {
        UITreeNode* empty = createTreeNode("No symbols", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        addChildNode(fileNode, empty);
        addChildNode(root, fileNode);
        return root;
    }

    append_symbols_to_file(fileNode, entry, filePath, showAutoParamNames, showMacros);

    addChildNode(root, fileNode);
    return root;
}

struct UITreeNode* buildSymbolTreeForWorkspace(const DirEntry* projectRoot,
                                               const char* activeFilePath,
                                               bool showAutoParamNames,
                                               bool showMacros) {
    UITreeNode* root = createTreeNode("Symbols", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!root) return NULL;
    root->isExpanded = true;

    const char* projectRootPath = projectRoot ? projectRoot->path : NULL;
    UITreeNode* activeSection = createTreeNode("Active File", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!activeSection) {
        freeTreeNodeRecursive(root);
        return NULL;
    }
    {
        bool cachedExpanded = false;
        const char* key = cache_key_for_label("Active File");
        if (key && expansion_cache_get(key, &cachedExpanded)) {
            activeSection->isExpanded = cachedExpanded;
        } else {
            activeSection->isExpanded = true;
        }
    }

    if (activeFilePath && activeFilePath[0]) {
        char label[512];
        build_file_label(label, sizeof(label), activeFilePath, projectRootPath);
        UITreeNode* fileNode = createTreeNode(label, TREE_NODE_SECTION, NODE_COLOR_SECTION, activeFilePath, NULL);
        {
            bool cachedExpanded = false;
            const char* key = cache_key_for_path(activeFilePath);
            if (key && expansion_cache_get(key, &cachedExpanded)) {
                fileNode->isExpanded = cachedExpanded;
            } else {
                fileNode->isExpanded = true;
            }
        }
        analysis_symbols_store_lock();
        const AnalysisFileSymbols* entry = find_symbols_for_path(activeFilePath);
        append_symbols_to_file(fileNode, entry, activeFilePath, showAutoParamNames, showMacros);
        analysis_symbols_store_unlock();
        addChildNode(activeSection, fileNode);
    } else {
        UITreeNode* empty = createTreeNode("No active file", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        addChildNode(activeSection, empty);
    }
    addChildNode(root, activeSection);

    UITreeNode* projectSection = createTreeNode("Project Files", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!projectSection) {
        freeTreeNodeRecursive(root);
        return NULL;
    }
    {
        bool cachedExpanded = false;
        const char* key = cache_key_for_label("Project Files");
        if (key && expansion_cache_get(key, &cachedExpanded)) {
            projectSection->isExpanded = cachedExpanded;
        } else {
            projectSection->isExpanded = true;
        }
    }

    bool addedAny = false;
    if (projectRoot) {
        for (int i = 0; i < projectRoot->childCount; ++i) {
            const DirEntry* childEntry = projectRoot->children[i];
            UITreeNode* childNode = build_project_tree_node(childEntry, activeFilePath, showAutoParamNames, showMacros, 1);
            if (childNode) {
                addChildNode(projectSection, childNode);
                addedAny = true;
            }
        }
    }

    if (!addedAny) {
        UITreeNode* empty = createTreeNode("No project files", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        addChildNode(projectSection, empty);
    }
    addChildNode(root, projectSection);

    return root;
}
