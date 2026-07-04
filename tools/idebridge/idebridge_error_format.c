#include "idebridge_error_format.h"

#include <ctype.h>

static void print_quoted(FILE* out, const char* value) {
    fputc('"', out);
    for (const char* p = value ? value : ""; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\\' || ch == '"') {
            fputc('\\', out);
            fputc(ch, out);
        } else if (ch == '\n') {
            fputs("\\n", out);
        } else if (ch == '\r') {
            fputs("\\r", out);
        } else if (ch == '\t') {
            fputs("\\t", out);
        } else if (iscntrl(ch)) {
            fprintf(out, "\\x%02x", ch);
        } else {
            fputc(ch, out);
        }
    }
    fputc('"', out);
}

void idebridge_print_cli_error(FILE* out,
                               const char* stage,
                               const char* code,
                               const char* message,
                               const char* detail) {
    FILE* target = out ? out : stderr;
    fprintf(target,
            "idebridge: stage=%s code=%s message=",
            (stage && stage[0]) ? stage : "unknown",
            (code && code[0]) ? code : "unknown");
    print_quoted(target, message && message[0] ? message : "Unknown error");
    if (detail && detail[0]) {
        fputs(" detail=", target);
        print_quoted(target, detail);
    }
    fputc('\n', target);
}

void idebridge_print_server_error(FILE* out, json_object* response_root) {
    const char* code = "server_error";
    const char* message = "Server error";
    const char* detail = NULL;
    json_object* error = NULL;
    json_object* field = NULL;

    if (response_root &&
        json_object_object_get_ex(response_root, "error", &error) &&
        error &&
        json_object_is_type(error, json_type_object)) {
        if (json_object_object_get_ex(error, "code", &field) && field) {
            const char* value = json_object_get_string(field);
            if (value && value[0]) code = value;
        }
        if (json_object_object_get_ex(error, "message", &field) && field) {
            const char* value = json_object_get_string(field);
            if (value && value[0]) message = value;
        }
        if (json_object_object_get_ex(error, "details", &field) && field) {
            const char* value = json_object_get_string(field);
            if (value && value[0]) detail = value;
        }
    }

    idebridge_print_cli_error(out, "server", code, message, detail);
}
