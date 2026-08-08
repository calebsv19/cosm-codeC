#ifndef CONTROL_TREE_PAYLOAD_H
#define CONTROL_TREE_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "ide/UI/Trees/ui_tree_node.h"

enum {
    CONTROL_TREE_PAYLOAD_STABLE_ID_MAX = 1024,
    CONTROL_TREE_PAYLOAD_PATH_MAX = 1024,
    CONTROL_TREE_PAYLOAD_LABEL_MAX = 256,
    CONTROL_TREE_PAYLOAD_MARKER_QUERY_MAX = 256
};

typedef enum ControlTreeNodeKind {
    CONTROL_TREE_NODE_SECTION = 0,
    CONTROL_TREE_NODE_FILE,
    CONTROL_TREE_NODE_SYMBOL,
    CONTROL_TREE_NODE_UNIT,
    CONTROL_TREE_NODE_EMPTY
} ControlTreeNodeKind;

typedef struct ControlTreeActivationTarget {
    bool hasTarget;
    char path[CONTROL_TREE_PAYLOAD_PATH_MAX];
    int line;
    int column;
} ControlTreeActivationTarget;

typedef struct ControlTreeNodePayload {
    ControlTreeNodeKind kind;
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    char displayPath[CONTROL_TREE_PAYLOAD_PATH_MAX];
    char label[CONTROL_TREE_PAYLOAD_LABEL_MAX];
    ControlTreeActivationTarget target;
    bool focusMarkerAfterOpen;
    char markerQuery[CONTROL_TREE_PAYLOAD_MARKER_QUERY_MAX];
} ControlTreeNodePayload;

const char* control_tree_node_kind_name(ControlTreeNodeKind kind);

bool control_tree_payload_format_section_id(char* out,
                                            size_t outSize,
                                            const char* family,
                                            const char* bucket);
bool control_tree_payload_format_file_id(char* out,
                                         size_t outSize,
                                         const char* family,
                                         const char* path);
bool control_tree_payload_format_symbol_id(char* out,
                                           size_t outSize,
                                           const char* path,
                                           int line,
                                           int column,
                                           const char* kind,
                                           const char* name);
bool control_tree_payload_format_unit_id(char* out,
                                         size_t outSize,
                                         const char* path,
                                         int line,
                                         int column,
                                         const char* name,
                                         const char* unitText);
bool control_tree_payload_format_empty_id(char* out,
                                          size_t outSize,
                                          const char* family,
                                          const char* message);

ControlTreeNodePayload* control_tree_payload_create(ControlTreeNodeKind kind,
                                                    const char* stableId,
                                                    const char* label,
                                                    const char* displayPath);
ControlTreeNodePayload* control_tree_payload_clone(const ControlTreeNodePayload* src);
void control_tree_payload_free(void* ptr);

bool control_tree_payload_set_target(ControlTreeNodePayload* payload,
                                     const char* path,
                                     int line,
                                     int column);
bool control_tree_payload_set_marker_query(ControlTreeNodePayload* payload,
                                           const char* query);

bool control_tree_node_set_payload(UITreeNode* node, ControlTreeNodePayload* payload);
const ControlTreeNodePayload* control_tree_node_payload(const UITreeNode* node);
ControlTreeNodePayload* control_tree_node_payload_mutable(UITreeNode* node);
uintptr_t control_tree_node_stable_identity(const UITreeNode* node);
bool control_tree_node_activation_target(const UITreeNode* node,
                                         ControlTreeActivationTarget* outTarget);
bool control_tree_node_marker_query(const UITreeNode* node,
                                    char* outQuery,
                                    size_t outQuerySize);

#endif
