#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"

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

        UITreeNode* symNode = createTreeNode(label, TREE_NODE_SECTION, NODE_COLOR_DEFAULT, filePath, (void*)sym);
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
                UITreeNode* paramNode = createTreeNode(paramLabel, TREE_NODE_FILE, NODE_COLOR_DEFAULT, filePath, (void*)sym);
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
        UITreeNode* childNode = createTreeNode(label, TREE_NODE_FILE, NODE_COLOR_DEFAULT, filePath, (void*)sym);
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
        const AnalysisFileSymbols* entrySymbols = find_symbols_for_path(entry->path);
        append_symbols_to_file(fileNode, entrySymbols, entry->path, showAutoParamNames, showMacros);
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
        const AnalysisFileSymbols* entry = find_symbols_for_path(activeFilePath);
        append_symbols_to_file(fileNode, entry, activeFilePath, showAutoParamNames, showMacros);
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
