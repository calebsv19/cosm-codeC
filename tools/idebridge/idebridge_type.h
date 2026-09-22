#ifndef IDEBRIDGE_TYPE_H
#define IDEBRIDGE_TYPE_H

#include <json-c/json.h>

typedef struct {
    const char* path;
    const char* before_hash;
    const char* after_hash;
    const char* expected_text;
    const char* replacement_text;
    int start_line;
    int start_column;
    int end_line;
    int end_column;
    int cadence_ms;
} IdebridgeTypeOptions;

// Returns 1 when consumed, 0 when unrelated, and -1 for a missing value.
int idebridge_type_parse_option(const char* argument,
                                int argc,
                                char** argv,
                                int* index,
                                IdebridgeTypeOptions* options);
json_object* idebridge_type_build_args(const IdebridgeTypeOptions* options);

#endif
