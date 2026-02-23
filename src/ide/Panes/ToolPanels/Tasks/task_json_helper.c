#include "task_json_helper.h"
#include "app/GlobalInfo/project.h"   // for project path
#include "core/FileIO/file_ops.h"        // for reading/writing files
#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"      // if TaskNode struct is defined elsewhere


#include <stdlib.h>
#include <string.h>

// === Public ===

#include <json-c/json.h>

bool saveTaskTreeToFile(const char* filepath, TaskNode** roots, int rootCount) {
    struct json_object* rootArray = json_object_new_array();
    if (!rootArray) return false;

    for (int i = 0; i < rootCount; i++) {
        TaskNode* root = roots[i];

        if (!root) {
            fprintf(stderr, "[TaskSave] Skipping NULL root at index %d\n", i);
            continue;
        }

        if (strlen(root->label) > 200 || strpbrk(root->label, "\n\r\t")) {
            fprintf(stderr, "[TaskSave] Warning: Suspicious label on root %d: '%s'\n", i, root->label);
        }

        struct json_object* nodeJson = serializeTaskNode(root);
        if (nodeJson) {
            json_object_array_add(rootArray, nodeJson);
        } else {
            fprintf(stderr, "[TaskSave] Failed to serialize task at index %d\n", i);
        }
    }

    const char* jsonText = json_object_to_json_string_ext(rootArray, JSON_C_TO_STRING_PRETTY);
    if (!jsonText) {
        fprintf(stderr, "[TaskSave] JSON string creation failed.\n");
        json_object_put(rootArray);
        return false;
    }

    if (!writeTextFile(filepath, jsonText, strlen(jsonText))) {
        fprintf(stderr, "[TaskSave] Could not open file: %s\n", filepath);
        json_object_put(rootArray);
        return false;
    }

    json_object_put(rootArray);  // cleanup

    return true;
}


bool loadTaskTreeFromFile(const char* filepath, TaskNode*** outRoots, int* outRootCount){
    if (!filepath || !outRoots || !outRootCount) 
	return false;

    char* buffer = NULL;
    size_t size = 0;
    if (!readTextFile(filepath, &buffer, &size)) {
        fprintf(stderr, "[TaskJSON] Failed to open file: %s\n", filepath);
        return false;
    }

    if (size <= 0) {
        free(buffer);
        fprintf(stderr, "[TaskJSON] File is empty or corrupt; resetting to empty task list.\n");
        *outRoots = NULL;
        *outRootCount = 0;
        /* Best effort: repair on disk to a valid empty JSON array. */
        (void)writeTextFile(filepath, "[]\n", 3u);
        return true;
    }
    


    struct json_object* rootArray = json_tokener_parse(buffer);
    free(buffer);

    if (!rootArray || !json_object_is_type(rootArray, json_type_array)) {
        fprintf(stderr, "[TaskJSON] JSON root is not an array or failed to parse; resetting to empty task list.\n");
        if (rootArray) json_object_put(rootArray);
        *outRoots = NULL;
        *outRootCount = 0;
        (void)writeTextFile(filepath, "[]\n", 3u);
        return true;
    }

    int count = json_object_array_length(rootArray);
    if (count == 0) {
        json_object_put(rootArray);
        *outRoots = NULL;
        *outRootCount = 0;
        return true; // Empty is valid
    }

    TaskNode** roots = (TaskNode**)calloc(count, sizeof(TaskNode*));
    if (!roots) {
        json_object_put(rootArray);
        fprintf(stderr, "[TaskJSON] Failed to allocate root task array.\n");
        return false;
    }

    int validCount = 0;
    for (int i = 0; i < count; i++) {

        struct json_object* nodeJson = json_object_array_get_idx(rootArray, i);
        TaskNode* node = deserializeTaskNode(nodeJson);

        if (node) {
            roots[validCount++] = node;
        } else {
            fprintf(stderr, "[TaskJSON] Warning: Skipping null or malformed task node.\n");
        }
    }

    json_object_put(rootArray);

    if (validCount == 0) {
        free(roots);
        *outRoots = NULL;
        *outRootCount = 0;
        fprintf(stderr, "[TaskJSON] No valid root tasks found; treating as empty task list.\n");
        return true;
    }

    *outRoots = roots;
    *outRootCount = validCount;
    return true;
}

// === Internal Helpers ===

struct json_object* serializeTaskNode(TaskNode* node) {
    if (!node) return NULL;

    struct json_object* obj = json_object_new_object();
    if (!obj) return NULL;

    json_object_object_add(obj, "label", json_object_new_string(node->label));
    json_object_object_add(obj, "completed", json_object_new_boolean(node->completed));
    json_object_object_add(obj, "isGroup", json_object_new_boolean(node->isGroup));
    json_object_object_add(obj, "isExpanded", json_object_new_boolean(node->isExpanded));

    struct json_object* childArray = json_object_new_array();
    for (int i = 0; i < node->childCount; i++) {
        struct json_object* childJson = serializeTaskNode(node->children[i]);
        if (childJson) {
            json_object_array_add(childArray, childJson);
        }
    }

    json_object_object_add(obj, "children", childArray);

    return obj;
}


TaskNode* deserializeTaskNode(struct json_object* obj) {
    if (!obj || json_object_get_type(obj) != json_type_object) {
        fprintf(stderr, "[TaskJSON] Node is NULL or not an object\n");
        return NULL;
    }

    // === Label ===
    struct json_object* labelObj = NULL;
    if (!json_object_object_get_ex(obj, "label", &labelObj) || !json_object_is_type(labelObj, json_type_string)) {
        fprintf(stderr, "[TaskJSON] Invalid or missing 'label' field.\n");
        return NULL; 
    }
    const char* label = json_object_get_string(labelObj);
    if (!label) label = "";

    // === Completed ===
    struct json_object* completedObj = NULL;
    bool completed = false;
    if (json_object_object_get_ex(obj, "completed", &completedObj) &&
        json_object_is_type(completedObj, json_type_boolean)) {
        completed = json_object_get_boolean(completedObj);
    }

    // === isGroup ===
    struct json_object* isGroupObj = NULL;
    bool isGroup = false;
    if (json_object_object_get_ex(obj, "isGroup", &isGroupObj) &&
        json_object_is_type(isGroupObj, json_type_boolean)) {
        isGroup = json_object_get_boolean(isGroupObj);
    }

    // === isExpanded ===
    struct json_object* isExpandedObj = NULL;
    bool isExpanded = true; // default
    if (json_object_object_get_ex(obj, "isExpanded", &isExpandedObj) &&
        json_object_is_type(isExpandedObj, json_type_boolean)) {
        isExpanded = json_object_get_boolean(isExpandedObj);
    }

    // === Create node ===
    TaskNode* node = createTaskNode(label, isGroup, NULL);
    if (!node) {
        fprintf(stderr, "[TaskJSON] Failed to create node for label: '%s'\n", label);
        return NULL;
    }

    node->completed = completed;
    node->isExpanded = isExpanded;

    // === Parse children ===
    struct json_object* childrenArray = NULL;
    if (json_object_object_get_ex(obj, "children", &childrenArray) &&
        json_object_is_type(childrenArray, json_type_array)) {
        
        int count = json_object_array_length(childrenArray);
        for (int i = 0; i < count; i++) {
            struct json_object* childJson = json_object_array_get_idx(childrenArray, i);
            TaskNode* child = deserializeTaskNode(childJson);
            if (child) {
                addTaskChild(node, child);
            } else {
                fprintf(stderr, "[TaskJSON] Skipping malformed child node in '%s'\n", label);
            }
        }
    }

    return node;
}
