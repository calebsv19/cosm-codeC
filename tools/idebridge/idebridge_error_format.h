#ifndef IDEBRIDGE_ERROR_FORMAT_H
#define IDEBRIDGE_ERROR_FORMAT_H

#include <stdio.h>
#include <json-c/json.h>

void idebridge_print_cli_error(FILE* out,
                               const char* stage,
                               const char* code,
                               const char* message,
                               const char* detail);

void idebridge_print_server_error(FILE* out, json_object* response_root);

#endif
