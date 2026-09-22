#include "idebridge_type.h"

#include <stdlib.h>
#include <string.h>

static const char** string_slot(IdebridgeTypeOptions* options, const char* argument) {
    if (strcmp(argument, "--path") == 0) return &options->path;
    if (strcmp(argument, "--expected_before_fnv64") == 0) return &options->before_hash;
    if (strcmp(argument, "--expected_after_fnv64") == 0) return &options->after_hash;
    if (strcmp(argument, "--expected_text") == 0) return &options->expected_text;
    if (strcmp(argument, "--replacement_text") == 0) return &options->replacement_text;
    return NULL;
}

static int* int_slot(IdebridgeTypeOptions* options, const char* argument) {
    if (strcmp(argument, "--start_line") == 0) return &options->start_line;
    if (strcmp(argument, "--start_column") == 0) return &options->start_column;
    if (strcmp(argument, "--end_line") == 0) return &options->end_line;
    if (strcmp(argument, "--end_column") == 0) return &options->end_column;
    if (strcmp(argument, "--cadence_ms") == 0) return &options->cadence_ms;
    return NULL;
}

int idebridge_type_parse_option(const char* argument,
                                int argc,
                                char** argv,
                                int* index,
                                IdebridgeTypeOptions* options) {
    if (!argument || !index || !options) return 0;
    const char** text = string_slot(options, argument);
    int* number = int_slot(options, argument);
    if (!text && !number) return 0;
    if (*index + 1 >= argc) return -1;
    if (text) *text = argv[++(*index)];
    else *number = atoi(argv[++(*index)]);
    return 1;
}

json_object* idebridge_type_build_args(const IdebridgeTypeOptions* options) {
    if (!options || !options->path || !options->path[0] ||
        !options->before_hash || !options->after_hash ||
        !options->expected_text || !options->replacement_text ||
        !options->replacement_text[0] || strlen(options->before_hash) != 16 ||
        strlen(options->after_hash) != 16 || options->start_line < 1 ||
        options->start_column < 1 || options->end_line < options->start_line ||
        options->end_column < 1 || options->cadence_ms < 16 ||
        options->cadence_ms > 500) return NULL;

    json_object* args = json_object_new_object();
    json_object_object_add(args, "path", json_object_new_string(options->path));
    json_object_object_add(args, "expected_before_fnv64", json_object_new_string(options->before_hash));
    json_object_object_add(args, "expected_after_fnv64", json_object_new_string(options->after_hash));
    json_object_object_add(args, "expected_text", json_object_new_string(options->expected_text));
    json_object_object_add(args, "replacement_text", json_object_new_string(options->replacement_text));
    json_object_object_add(args, "start_line", json_object_new_int(options->start_line));
    json_object_object_add(args, "start_column", json_object_new_int(options->start_column));
    json_object_object_add(args, "end_line", json_object_new_int(options->end_line));
    json_object_object_add(args, "end_column", json_object_new_int(options->end_column));
    json_object_object_add(args, "cadence_ms", json_object_new_int(options->cadence_ms));
    return args;
}
