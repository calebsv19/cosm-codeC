#include "ide/Panes/ControlPanel/control_panel_units_tree.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/GlobalInfo/project.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_units_store.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ControlPanel/control_tree_payload.h"
#include "ide/UI/Trees/ui_tree_node.h"

static const char* basename_from_path(const char* path) {
    if (!path) return NULL;
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
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

static FisicsSymbol* create_location_symbol_for_units(const AnalysisUnitsAttachment* units,
                                                      const char* filePath) {
    if (!units) return NULL;
    const char* sourcePath = filePath;
    if ((!sourcePath || !sourcePath[0]) && units->source_file_path && units->source_file_path[0]) {
        sourcePath = units->source_file_path;
    }
    if (!sourcePath || !sourcePath[0]) return NULL;
    FisicsSymbol* out = (FisicsSymbol*)calloc(1, sizeof(FisicsSymbol));
    if (!out) return NULL;
    out->stable_id = units->symbol_stable_id;
    out->name = units->symbol_name ? strdup(units->symbol_name) : NULL;
    out->file_path = strdup(sourcePath);
    out->kind = FISICS_SYMBOL_VARIABLE;
    out->start_line = units->start_line;
    out->start_col = units->start_col;
    out->end_line = units->end_line;
    out->end_col = units->end_col;
    if (!out->file_path || (units->symbol_name && !out->name)) {
        free_tree_symbol_user_data(out);
        return NULL;
    }
    return out;
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

static const FisicsSymbol* find_symbol_by_stable_id(uint64_t stableId) {
    if (stableId == 0) return NULL;
    size_t count = analysis_symbols_store_file_count();
    for (size_t fi = 0; fi < count; ++fi) {
        const AnalysisFileSymbols* file = analysis_symbols_store_file_at(fi);
        if (!file || !file->symbols) continue;
        for (size_t si = 0; si < file->count; ++si) {
            if (file->symbols[si].stable_id == stableId) {
                return &file->symbols[si];
            }
        }
    }
    return NULL;
}

static void build_file_label(char* out, size_t outSize, const char* filePath, const char* projectRootPath) {
    if (!out || outSize == 0) return;
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

static const char* units_display_text(const AnalysisUnitsAttachment* units) {
    if (!units) return "";
    if (units->has_concrete_unit && units->unit_symbol && units->unit_symbol[0]) {
        return units->unit_symbol;
    }
    if (units->unit_source_text && units->unit_source_text[0]) {
        return units->unit_source_text;
    }
    if (units->unit_name && units->unit_name[0]) {
        return units->unit_name;
    }
    if (units->dim_text && units->dim_text[0]) {
        return units->dim_text;
    }
    return "";
}

static void attach_payload_or_free(UITreeNode* node, ControlTreeNodePayload* payload) {
    if (!payload) return;
    if (!control_tree_node_set_payload(node, payload)) {
        control_tree_payload_free(payload);
    }
}

static void attach_section_payload(UITreeNode* node, const char* bucket) {
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    if (!control_tree_payload_format_section_id(stableId, sizeof(stableId), "units", bucket)) {
        return;
    }
    attach_payload_or_free(node,
                           control_tree_payload_create(CONTROL_TREE_NODE_SECTION,
                                                       stableId,
                                                       node ? node->label : "",
                                                       node ? node->fullPath : NULL));
}

static void attach_file_payload(UITreeNode* node, const char* filePath) {
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    if (!control_tree_payload_format_file_id(stableId, sizeof(stableId), "units", filePath)) {
        return;
    }
    ControlTreeNodePayload* payload = control_tree_payload_create(CONTROL_TREE_NODE_FILE,
                                                                 stableId,
                                                                 node ? node->label : "",
                                                                 filePath);
    if (payload) {
        (void)control_tree_payload_set_target(payload, filePath, 0, 0);
    }
    attach_payload_or_free(node, payload);
}

static void attach_empty_payload(UITreeNode* node, const char* message) {
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    if (!control_tree_payload_format_empty_id(stableId, sizeof(stableId), "units", message)) {
        return;
    }
    attach_payload_or_free(node,
                           control_tree_payload_create(CONTROL_TREE_NODE_EMPTY,
                                                       stableId,
                                                       node ? node->label : message,
                                                       NULL));
}

static bool path_is_active(const char* path, const char* activeFilePath) {
    return path && activeFilePath && strcmp(path, activeFilePath) == 0;
}

bool control_panel_units_query_matches(const AnalysisUnitsAttachment* units,
                                       const char* query) {
    if (!query || !query[0]) return true;
    if (!units) return false;
    if (text_contains_ci(units->symbol_name, query)) return true;
    if (text_contains_ci(units->dim_text, query)) return true;
    if (text_contains_ci(units->unit_source_text, query)) return true;
    if (text_contains_ci(units->unit_name, query)) return true;
    if (text_contains_ci(units->unit_symbol, query)) return true;
    if (text_contains_ci(units->unit_family, query)) return true;
    return false;
}

static bool unit_text_matches_any(const AnalysisUnitsAttachment* units,
                                  const char* a,
                                  const char* b,
                                  const char* c) {
    if (!units) return false;
    return text_contains_ci(units->unit_family, a) ||
           text_contains_ci(units->unit_name, a) ||
           text_contains_ci(units->unit_source_text, a) ||
           text_contains_ci(units->unit_symbol, a) ||
           text_contains_ci(units->dim_text, a) ||
           (b && (text_contains_ci(units->unit_family, b) ||
                  text_contains_ci(units->unit_name, b) ||
                  text_contains_ci(units->unit_source_text, b) ||
                  text_contains_ci(units->unit_symbol, b) ||
                  text_contains_ci(units->dim_text, b))) ||
           (c && (text_contains_ci(units->unit_family, c) ||
                  text_contains_ci(units->unit_name, c) ||
                  text_contains_ci(units->unit_source_text, c) ||
                  text_contains_ci(units->unit_symbol, c) ||
                  text_contains_ci(units->dim_text, c)));
}

static bool text_equals_ci(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool control_panel_units_dimension_matches(const AnalysisUnitsAttachment* units,
                                           unsigned int unitDimensionMask) {
    if (unitDimensionMask == 0u) return true;
    if (!units) return false;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_TIME) != 0u &&
        (text_contains_ci(units->unit_family, "time") ||
         text_equals_ci(units->unit_name, "second") ||
         text_equals_ci(units->unit_source_text, "second") ||
         text_equals_ci(units->unit_symbol, "s") ||
         text_equals_ci(units->dim_text, "s"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_DISTANCE) != 0u &&
        unit_text_matches_any(units, "length", "distance", "meter")) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_SPEED) != 0u &&
        unit_text_matches_any(units, "velocity", "speed", "m/s")) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_ACCEL) != 0u &&
        unit_text_matches_any(units, "acceleration", "m/s^2", NULL)) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_MASS) != 0u &&
        (unit_text_matches_any(units, "mass", "kilogram", NULL) ||
         text_equals_ci(units->unit_symbol, "kg") ||
         text_equals_ci(units->dim_text, "kg"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_FORCE) != 0u &&
        unit_text_matches_any(units, "force", "newton", NULL)) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_ENERGY) != 0u &&
        unit_text_matches_any(units, "energy", "joule", NULL)) return true;
    return false;
}

static bool label_matches_unit_dimension(const char* label, unsigned int unitDimensionMask) {
    if (unitDimensionMask == 0u) return true;
    if (!label || !label[0]) return false;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_TIME) != 0u &&
        (text_contains_ci(label, "time") || text_contains_ci(label, "second") ||
         text_contains_ci(label, ": s  second"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_DISTANCE) != 0u &&
        (text_contains_ci(label, "length") || text_contains_ci(label, "distance") ||
         text_contains_ci(label, "meter"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_SPEED) != 0u &&
        (text_contains_ci(label, "velocity") || text_contains_ci(label, "speed") ||
         text_contains_ci(label, "m/s"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_ACCEL) != 0u &&
        (text_contains_ci(label, "acceleration") || text_contains_ci(label, "m/s^2"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_MASS) != 0u &&
        (text_contains_ci(label, "mass") || text_contains_ci(label, "kilogram") ||
         text_contains_ci(label, ": kg  kilogram"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_FORCE) != 0u &&
        (text_contains_ci(label, "force") || text_contains_ci(label, "newton"))) return true;
    if ((unitDimensionMask & CONTROL_UNIT_DIM_ENERGY) != 0u &&
        (text_contains_ci(label, "energy") || text_contains_ci(label, "joule"))) return true;
    return false;
}

bool control_panel_units_tree_node_focus_query(const struct UITreeNode* node,
                                               char* outQuery,
                                               size_t outQuerySize) {
    return control_tree_node_marker_query(node, outQuery, outQuerySize);
}

static UITreeNode* create_units_attachment_node(const AnalysisUnitsAttachment* units,
                                                const char* filePath) {
    if (!units) return NULL;
    const char* name = units->symbol_name && units->symbol_name[0]
        ? units->symbol_name
        : "<symbol>";
    const char* unitText = units_display_text(units);
    const char* family = units->unit_family ? units->unit_family : "";
    const char* unitName = units->unit_name ? units->unit_name : "";
    const char* dim = units->dim_text ? units->dim_text : "";
    char label[512];
    if (unitText[0] && unitName[0] && family[0] && dim[0]) {
        snprintf(label, sizeof(label), "%s : %s  %s  %s  dim %s", name, unitText, unitName, family, dim);
    } else if (unitText[0] && family[0] && dim[0]) {
        snprintf(label, sizeof(label), "%s : %s  %s  dim %s", name, unitText, family, dim);
    } else if (unitText[0] && unitName[0] && family[0]) {
        snprintf(label, sizeof(label), "%s : %s  %s  %s", name, unitText, unitName, family);
    } else if (unitText[0] && unitName[0]) {
        snprintf(label, sizeof(label), "%s : %s  %s", name, unitText, unitName);
    } else if (unitText[0] && family[0]) {
        snprintf(label, sizeof(label), "%s : %s  %s", name, unitText, family);
    } else if (unitText[0] && dim[0]) {
        snprintf(label, sizeof(label), "%s : %s  dim %s", name, unitText, dim);
    } else if (unitText[0]) {
        snprintf(label, sizeof(label), "%s : %s", name, unitText);
    } else {
        snprintf(label, sizeof(label), "%s : unit", name);
    }

    FisicsSymbol* symCopy = NULL;
    if ((filePath && filePath[0]) || units->start_line > 0) {
        symCopy = create_location_symbol_for_units(units, filePath);
    }
    if (!symCopy && (units->has_symbol_stable_id || units->symbol_stable_id != 0)) {
        analysis_symbols_store_lock();
        const FisicsSymbol* sym = find_symbol_by_stable_id(units->symbol_stable_id);
        if (sym && (!filePath || !filePath[0] ||
                    (sym->file_path && strcmp(sym->file_path, filePath) == 0))) {
            symCopy = clone_symbol_for_tree(sym);
        }
        analysis_symbols_store_unlock();
    }

    UITreeNode* node = createTreeNode(label, TREE_NODE_FILE, NODE_COLOR_DEFAULT, filePath, symCopy);
    if (symCopy) {
        setTreeNodeUserDataFreeFn(node, free_tree_symbol_user_data);
    }
    if (node) {
        const char* targetPath = filePath && filePath[0] ? filePath : NULL;
        if (!targetPath && symCopy && symCopy->file_path && symCopy->file_path[0]) {
            targetPath = symCopy->file_path;
        }
        if (!targetPath && units->source_file_path && units->source_file_path[0]) {
            targetPath = units->source_file_path;
        }
        int targetLine = symCopy ? symCopy->start_line : units->start_line;
        int targetCol = symCopy ? symCopy->start_col : units->start_col;
        char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
        if (control_tree_payload_format_unit_id(stableId,
                                                sizeof(stableId),
                                                targetPath,
                                                targetLine,
                                                targetCol,
                                                name,
                                                unitText)) {
            ControlTreeNodePayload* payload = control_tree_payload_create(CONTROL_TREE_NODE_UNIT,
                                                                         stableId,
                                                                         label,
                                                                         targetPath);
            if (payload) {
                (void)control_tree_payload_set_target(payload, targetPath, targetLine, targetCol);
                (void)control_tree_payload_set_marker_query(payload, name);
            }
            attach_payload_or_free(node, payload);
        }
    }
    return node;
}

static bool append_units_for_file(UITreeNode* fileNode,
                                  const AnalysisFileUnits* file,
                                  const char* filePath,
                                  bool appendEmptyMessage) {
    if (!fileNode) return false;
    if (!file || !file->attachments || file->count == 0) {
        if (appendEmptyMessage) {
            UITreeNode* empty = createTreeNode("No units", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
            attach_empty_payload(empty, "No units");
            addChildNode(fileNode, empty);
        }
        return false;
    }
    bool addedAny = false;
    for (size_t i = 0; i < file->count; ++i) {
        UITreeNode* unitNode = create_units_attachment_node(&file->attachments[i], filePath);
        if (unitNode) {
            addChildNode(fileNode, unitNode);
            addedAny = true;
        }
    }
    if (!addedAny && appendEmptyMessage) {
        UITreeNode* empty = createTreeNode("No units", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        attach_empty_payload(empty, "No units");
        addChildNode(fileNode, empty);
    }
    return addedAny;
}

struct UITreeNode* control_panel_build_units_tree(const struct DirEntry* projectRoot,
                                                  const char* activeFilePath) {
    UITreeNode* root = createTreeNode("Units", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!root) return NULL;
    attach_section_payload(root, "root");
    root->isExpanded = true;

    const char* projectRootPath = projectRoot ? projectRoot->path : NULL;
    UITreeNode* activeSection = createTreeNode("Active File", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!activeSection) {
        freeTreeNodeRecursive(root);
        return NULL;
    }
    activeSection->isExpanded = true;
    attach_section_payload(activeSection, "active");

    bool activeAdded = false;
    UITreeNode* projectSection = createTreeNode("Project Files", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!projectSection) {
        freeTreeNodeRecursive(root);
        return NULL;
    }
    projectSection->isExpanded = true;
    attach_section_payload(projectSection, "project");

    analysis_units_store_lock();
    size_t fileCount = analysis_units_store_file_count();
    for (size_t fi = 0; fi < fileCount; ++fi) {
        const AnalysisFileUnits* file = analysis_units_store_file_at(fi);
        if (!file || !file->path || !file->path[0]) continue;

        bool isActive = path_is_active(file->path, activeFilePath);
        char fileLabel[512];
        build_file_label(fileLabel, sizeof(fileLabel), file->path, projectRootPath);

        if (isActive) {
            UITreeNode* activeFileNode = createTreeNode(fileLabel, TREE_NODE_SECTION, NODE_COLOR_SECTION, file->path, NULL);
            if (activeFileNode) {
                attach_file_payload(activeFileNode, file->path);
                activeFileNode->isExpanded = true;
                append_units_for_file(activeFileNode, file, file->path, true);
                addChildNode(activeSection, activeFileNode);
            }
            activeAdded = true;
        }

        if (file->attachments && file->count > 0) {
            UITreeNode* projectFileNode = createTreeNode(fileLabel, TREE_NODE_SECTION, NODE_COLOR_SECTION, file->path, NULL);
            if (!projectFileNode) continue;
            attach_file_payload(projectFileNode, file->path);
            projectFileNode->isExpanded = isActive;
            if (append_units_for_file(projectFileNode, file, file->path, false)) {
                addChildNode(projectSection, projectFileNode);
            } else {
                freeTreeNodeRecursive(projectFileNode);
            }
        }
    }
    analysis_units_store_unlock();

    if (!activeAdded) {
        UITreeNode* empty = createTreeNode(activeFilePath && activeFilePath[0] ? "No units" : "No active file",
                                           TREE_NODE_FILE,
                                           NODE_COLOR_DEFAULT,
                                           NULL,
                                           NULL);
        attach_empty_payload(empty, activeFilePath && activeFilePath[0] ? "No units" : "No active file");
        addChildNode(activeSection, empty);
    }
    if (projectSection->childCount == 0) {
        UITreeNode* empty = createTreeNode("No project units", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
        attach_empty_payload(empty, "No project units");
        addChildNode(projectSection, empty);
    }

    addChildNode(root, activeSection);
    addChildNode(root, projectSection);
    return root;
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

static UITreeNode* clone_units_filtered_node(const UITreeNode* node,
                                             const char* query,
                                             SymbolFilterScope scope,
                                             unsigned int unitDimensionMask,
                                             bool* outDescendantMatch) {
    if (!node) return NULL;
    if (is_scope_excluded_bucket(node, scope)) {
        if (outDescendantMatch) *outDescendantMatch = false;
        return NULL;
    }

    bool isLeafUnit = node->userData != NULL;
    bool selfMatches = text_contains_ci(node->label, query) &&
                       (!isLeafUnit || label_matches_unit_dimension(node->label, unitDimensionMask));
    FisicsSymbol* symCopy = node->userData ? clone_symbol_for_tree((const FisicsSymbol*)node->userData) : NULL;
    UITreeNode* clone = createTreeNode(node->label,
                                       node->type,
                                       node->color,
                                       node->fullPath,
                                       symCopy);
    if (!clone) {
        if (symCopy) free_tree_symbol_user_data(symCopy);
        return NULL;
    }
    if (symCopy) {
        setTreeNodeUserDataFreeFn(clone, free_tree_symbol_user_data);
    }
    const ControlTreeNodePayload* payload = control_tree_node_payload(node);
    if (payload) {
        attach_payload_or_free(clone, control_tree_payload_clone(payload));
    }

    bool kept = isLeafUnit ? selfMatches : (selfMatches || !query || !query[0]);
    bool descendantMatch = false;
    for (int i = 0; i < node->childCount; ++i) {
        UITreeNode* childClone = clone_units_filtered_node(node->children[i],
                                                           query,
                                                           scope,
                                                           unitDimensionMask,
                                                           NULL);
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
    if (query && query[0] && descendantMatch &&
        (clone->type == TREE_NODE_FOLDER || clone->type == TREE_NODE_SECTION)) {
        clone->isExpanded = true;
    }
    if (outDescendantMatch) *outDescendantMatch = selfMatches || descendantMatch;
    return clone;
}

struct UITreeNode* control_panel_clone_units_tree_filtered(const struct UITreeNode* root,
                                                           const char* query,
                                                           SymbolFilterScope scope,
                                                           unsigned int unitDimensionMask) {
    if (!root) return NULL;
    bool matched = false;
    return clone_units_filtered_node(root, query ? query : "", scope, unitDimensionMask, &matched);
}
