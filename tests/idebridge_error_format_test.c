#include <stdio.h>
#include <string.h>
#include <json-c/json.h>

#include "../tools/idebridge/idebridge_error_format.h"

static int fail(const char* msg) {
    fprintf(stderr, "idebridge_error_format_test: %s\n", msg);
    return 1;
}

static int read_file(const char* path, char* out, size_t cap) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return 1;
}

static int assert_file_contains(const char* path, const char* expected) {
    char buf[1024];
    if (!read_file(path, buf, sizeof(buf))) return fail("failed to read formatter output");
    if (!strstr(buf, expected)) {
        fprintf(stderr, "expected: %s\nactual: %s\n", expected, buf);
        return fail("formatter output mismatch");
    }
    return 0;
}

int main(void) {
    const char* path = "/tmp/idebridge_error_format_test.txt";
    FILE* f = fopen(path, "wb");
    if (!f) return fail("failed to open formatter output");
    idebridge_print_cli_error(f,
                              "response",
                              "invalid_json",
                              "Invalid \"response\"\nfrom IDE",
                              "/tmp/socket");
    fclose(f);
    if (assert_file_contains(path,
                             "idebridge: stage=response code=invalid_json message=\"Invalid \\\"response\\\"\\nfrom IDE\" detail=\"/tmp/socket\"\n") != 0) {
        return 1;
    }

    json_object* root = json_tokener_parse(
        "{\"ok\":false,\"error\":{\"code\":\"unknown_command\",\"message\":\"Unknown command\",\"details\":\"diagx\"}}");
    if (!root) return fail("failed to parse server error fixture");
    f = fopen(path, "wb");
    if (!f) {
        json_object_put(root);
        return fail("failed to reopen formatter output");
    }
    idebridge_print_server_error(f, root);
    fclose(f);
    json_object_put(root);
    if (assert_file_contains(path,
                             "idebridge: stage=server code=unknown_command message=\"Unknown command\" detail=\"diagx\"\n") != 0) {
        return 1;
    }

    printf("idebridge_error_format_test: success\n");
    return 0;
}
