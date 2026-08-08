#include "ide/Panes/ControlPanel/control_tree_payload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* safe_text(const char* text) {
    return text ? text : "";
}

static void copy_text(char* out, size_t outSize, const char* text) {
    if (!out || outSize == 0) return;
    snprintf(out, outSize, "%s", safe_text(text));
}

static bool format_checked(char* out, size_t outSize, const char* fmt, const char* a, const char* b) {
    if (!out || outSize == 0 || !fmt) return false;
    int wrote = snprintf(out, outSize, fmt, safe_text(a), safe_text(b));
    return wrote > 0 && (size_t)wrote < outSize;
}

const char* control_tree_node_kind_name(ControlTreeNodeKind kind) {
    switch (kind) {
        case CONTROL_TREE_NODE_SECTION: return "section";
        case CONTROL_TREE_NODE_FILE: return "file";
        case CONTROL_TREE_NODE_SYMBOL: return "symbol";
        case CONTROL_TREE_NODE_UNIT: return "unit";
        case CONTROL_TREE_NODE_EMPTY: return "empty";
        default: return "unknown";
    }
}

bool control_tree_payload_format_section_id(char* out,
                                            size_t outSize,
                                            const char* family,
                                            const char* bucket) {
    return format_checked(out, outSize, "section:%s:%s", family, bucket);
}

bool control_tree_payload_format_file_id(char* out,
                                         size_t outSize,
                                         const char* family,
                                         const char* path) {
    return format_checked(out, outSize, "file:%s:%s", family, path);
}

bool control_tree_payload_format_symbol_id(char* out,
                                           size_t outSize,
                                           const char* path,
                                           int line,
                                           int column,
                                           const char* kind,
                                           const char* name) {
    if (!out || outSize == 0) return false;
    int wrote = snprintf(out,
                         outSize,
                         "symbol:%s:%d:%d:%s:%s",
                         safe_text(path),
                         line,
                         column,
                         safe_text(kind),
                         safe_text(name));
    return wrote > 0 && (size_t)wrote < outSize;
}

bool control_tree_payload_format_unit_id(char* out,
                                         size_t outSize,
                                         const char* path,
                                         int line,
                                         int column,
                                         const char* name,
                                         const char* unitText) {
    if (!out || outSize == 0) return false;
    int wrote = snprintf(out,
                         outSize,
                         "unit:%s:%d:%d:%s:%s",
                         safe_text(path),
                         line,
                         column,
                         safe_text(name),
                         safe_text(unitText));
    return wrote > 0 && (size_t)wrote < outSize;
}

bool control_tree_payload_format_empty_id(char* out,
                                          size_t outSize,
                                          const char* family,
                                          const char* message) {
    return format_checked(out, outSize, "empty:%s:%s", family, message);
}

ControlTreeNodePayload* control_tree_payload_create(ControlTreeNodeKind kind,
                                                    const char* stableId,
                                                    const char* label,
                                                    const char* displayPath) {
    ControlTreeNodePayload* payload = (ControlTreeNodePayload*)calloc(1, sizeof(ControlTreeNodePayload));
    if (!payload) return NULL;
    payload->kind = kind;
    copy_text(payload->stableId, sizeof(payload->stableId), stableId);
    copy_text(payload->label, sizeof(payload->label), label);
    copy_text(payload->displayPath, sizeof(payload->displayPath), displayPath);
    payload->target.line = 0;
    payload->target.column = 0;
    return payload;
}

ControlTreeNodePayload* control_tree_payload_clone(const ControlTreeNodePayload* src) {
    if (!src) return NULL;
    ControlTreeNodePayload* copy = (ControlTreeNodePayload*)calloc(1, sizeof(ControlTreeNodePayload));
    if (!copy) return NULL;
    *copy = *src;
    return copy;
}

void control_tree_payload_free(void* ptr) {
    free(ptr);
}

bool control_tree_payload_set_target(ControlTreeNodePayload* payload,
                                     const char* path,
                                     int line,
                                     int column) {
    if (!payload || !path || !path[0]) return false;
    payload->target.hasTarget = true;
    copy_text(payload->target.path, sizeof(payload->target.path), path);
    payload->target.line = line;
    payload->target.column = column;
    return true;
}

bool control_tree_payload_set_marker_query(ControlTreeNodePayload* payload,
                                           const char* query) {
    if (!payload || !query || !query[0]) return false;
    payload->focusMarkerAfterOpen = true;
    copy_text(payload->markerQuery, sizeof(payload->markerQuery), query);
    return true;
}

bool control_tree_node_set_payload(UITreeNode* node, ControlTreeNodePayload* payload) {
    if (!node || !payload) return false;
    setTreeNodePayload(node, payload, control_tree_payload_free);
    return true;
}

const ControlTreeNodePayload* control_tree_node_payload(const UITreeNode* node) {
    return node ? (const ControlTreeNodePayload*)node->payload : NULL;
}

ControlTreeNodePayload* control_tree_node_payload_mutable(UITreeNode* node) {
    return node ? (ControlTreeNodePayload*)node->payload : NULL;
}

uintptr_t control_tree_node_stable_identity(const UITreeNode* node) {
    const ControlTreeNodePayload* payload = control_tree_node_payload(node);
    if (!payload || !payload->stableId[0]) return 0u;

    uint64_t hash = 1469598103934665603ULL;
    const unsigned char* text = (const unsigned char*)payload->stableId;
    while (*text) {
        hash ^= (uint64_t)(*text++);
        hash *= 1099511628211ULL;
    }
    hash ^= (uint64_t)payload->kind;
    hash *= 1099511628211ULL;
    if (hash == 0u) hash = 1u;
    return (uintptr_t)hash;
}

bool control_tree_node_activation_target(const UITreeNode* node,
                                         ControlTreeActivationTarget* outTarget) {
    if (!outTarget) return false;
    memset(outTarget, 0, sizeof(*outTarget));

    const ControlTreeNodePayload* payload = control_tree_node_payload(node);
    if (!payload || !payload->target.hasTarget || !payload->target.path[0]) {
        return false;
    }
    switch (payload->kind) {
        case CONTROL_TREE_NODE_FILE:
        case CONTROL_TREE_NODE_SYMBOL:
        case CONTROL_TREE_NODE_UNIT:
            *outTarget = payload->target;
            return true;
        case CONTROL_TREE_NODE_SECTION:
        case CONTROL_TREE_NODE_EMPTY:
        default:
            return false;
    }
}

bool control_tree_node_marker_query(const UITreeNode* node,
                                    char* outQuery,
                                    size_t outQuerySize) {
    if (!outQuery || outQuerySize == 0) return false;
    outQuery[0] = '\0';

    const ControlTreeNodePayload* payload = control_tree_node_payload(node);
    if (!payload || !payload->focusMarkerAfterOpen || !payload->markerQuery[0]) {
        return false;
    }
    snprintf(outQuery, outQuerySize, "%s", payload->markerQuery);
    return outQuery[0] != '\0';
}
