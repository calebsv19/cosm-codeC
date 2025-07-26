#ifndef TASK_JSON_HELPER_H
#define TASK_JSON_HELPER_H

#include <stdbool.h>
#include <json-c/json.h>

// Forward declaration of TaskNode
struct TaskNode;

/**
 * Saves the root-level task tree to a JSON file.
 * @param filepath Destination path (e.g., project/task_tree.json)
 * @param roots Array of TaskNode* roots
 * @param rootCount Number of root nodes
 * @return true if successful, false otherwise
 */
bool saveTaskTreeToFile(const char* filepath, struct TaskNode** roots, int rootCount);

/**
 * Loads the task tree from a JSON file.
 * @param filepath Path to the JSON file
 * @param outRoots Output pointer to array of TaskNode* (allocated inside)
 * @param outRootCount Output count of root nodes
 * @return true if loading succeeded, false otherwise
 */
bool loadTaskTreeFromFile(const char* filepath, struct TaskNode*** outRoots, int* outRootCount);


// === Internal Helpers ===

/**
 * Serializes a TaskNode (including children) to a json_object*
 */
struct json_object* serializeTaskNode(struct TaskNode* node);

/**
 * Reconstructs a TaskNode tree from a json_object* recursively
 */
struct TaskNode* deserializeTaskNode(struct json_object* jsonObj);

#endif // TASK_JSON_HELPER_H

