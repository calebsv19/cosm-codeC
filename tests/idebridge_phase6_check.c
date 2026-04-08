#include "core/Ipc/ide_ipc_server.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/fisics_contract_validation.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static bool open_stub(const char* path,
                      int line,
                      int col,
                      char* error_out,
                      size_t error_out_cap,
                      void* userdata) {
    (void)userdata;
    if (!path || !*path || line <= 0 || col <= 0) {
        snprintf(error_out, error_out_cap, "Invalid open request");
        return false;
    }
    return true;
}

static bool edit_stub(const char* diff_text,
                      char* error_out,
                      size_t error_out_cap,
                      void* userdata,
                      json_object** result_out) {
    (void)userdata;
    if (!diff_text || !strstr(diff_text, "@@")) {
        snprintf(error_out, error_out_cap, "Malformed diff");
        return false;
    }
    json_object* result = json_object_new_object();
    json_object* touched = json_object_new_array();
    json_object_array_add(touched, json_object_new_string("src/main.c"));
    json_object_object_add(result, "touched_files", touched);
    json_object* summary = json_object_new_object();
    json_object_object_add(summary, "total", json_object_new_int(0));
    json_object_object_add(summary, "error", json_object_new_int(0));
    json_object_object_add(summary, "warn", json_object_new_int(0));
    json_object_object_add(summary, "info", json_object_new_int(0));
    json_object_object_add(result, "diagnostics_summary", summary);
    *result_out = result;
    return true;
}

static int connect_socket(const char* socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int send_then_recv(const char* socket_path,
                          const char* req,
                          bool pump,
                          char* out,
                          size_t out_cap) {
    int fd = connect_socket(socket_path);
    if (fd < 0) return -1;
    if (write(fd, req, strlen(req)) < 0 || write(fd, "\n", 1) != 1) {
        close(fd);
        return -1;
    }

    if (pump) {
        for (int i = 0; i < 200; ++i) {
            ide_ipc_pump();
            usleep(5000);
        }
    }

    size_t used = 0;
    while (used + 1 < out_cap) {
        ssize_t n = read(fd, out + used, out_cap - used - 1);
        if (n > 0) {
            used += (size_t)n;
            continue;
        }
        if (n == 0) break;
        close(fd);
        return -1;
    }
    close(fd);
    while (used > 0 && (out[used - 1] == '\n' || out[used - 1] == '\r')) used--;
    out[used] = '\0';
    return 0;
}

static int expect_ok_with_result(const char* response, const char* result_field) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }
    json_object *ok=NULL,*result=NULL,*field=NULL;
    int rc = -1;
    if (json_object_object_get_ex(root, "ok", &ok) && json_object_get_boolean(ok) &&
        json_object_object_get_ex(root, "result", &result) && result &&
        json_object_object_get_ex(result, result_field, &field) && field) {
        rc = 0;
    }
    json_object_put(root);
    return rc;
}

static int expect_error_code(const char* response, const char* code) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }
    json_object *ok=NULL,*err=NULL,*jcode=NULL;
    int rc = -1;
    if (json_object_object_get_ex(root, "ok", &ok) && !json_object_get_boolean(ok) &&
        json_object_object_get_ex(root, "error", &err) && err &&
        json_object_object_get_ex(err, "code", &jcode) && jcode) {
        const char* got = json_object_get_string(jcode);
        if (got && strcmp(got, code) == 0) rc = 0;
    }
    json_object_put(root);
    return rc;
}

static int expect_symbol_parent_stable_id(const char* response,
                                          const char* symbol_name,
                                          const char* parent_stable_id) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }
    json_object *ok=NULL,*result=NULL,*symbols=NULL;
    int rc = -1;
    if (json_object_object_get_ex(root, "ok", &ok) &&
        json_object_get_boolean(ok) &&
        json_object_object_get_ex(root, "result", &result) &&
        result &&
        json_object_object_get_ex(result, "symbols", &symbols) &&
        symbols &&
        json_object_is_type(symbols, json_type_array)) {
        size_t len = json_object_array_length(symbols);
        for (size_t i = 0; i < len; ++i) {
            json_object* sym = json_object_array_get_idx(symbols, i);
            if (!sym) continue;
            json_object* jname = NULL;
            json_object* jparent = NULL;
            json_object_object_get_ex(sym, "name", &jname);
            json_object_object_get_ex(sym, "parent_stable_id", &jparent);
            const char* got_name = jname ? json_object_get_string(jname) : NULL;
            const char* got_parent = jparent ? json_object_get_string(jparent) : NULL;
            if (got_name && got_parent &&
                strcmp(got_name, symbol_name) == 0 &&
                strcmp(got_parent, parent_stable_id) == 0) {
                rc = 0;
                break;
            }
        }
    }
    json_object_put(root);
    return rc;
}

static int write_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t len = strlen(text);
    if (fwrite(text, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int send_json_with_token(const char* socket_path,
                                const char* request_id,
                                const char* cmd,
                                const char* auth_token,
                                const char* args_json,
                                bool pump,
                                char* out,
                                size_t out_cap) {
    char req[65536];
    snprintf(req,
             sizeof(req),
             "{\"id\":\"%s\",\"proto\":1,\"cmd\":\"%s\",\"auth_token\":\"%s\",\"args\":%s}",
             request_id ? request_id : "req",
             cmd ? cmd : "ping",
             auth_token ? auth_token : "",
             args_json ? args_json : "{}");
    return send_then_recv(socket_path, req, pump, out, out_cap);
}

int main(void) {
    const char* workspace = "/tmp/idebridge_phase6_workspace";
    mkdir(workspace, 0755);

    char src_dir[1024];
    snprintf(src_dir, sizeof(src_dir), "%s/src", workspace);
    mkdir(src_dir, 0755);

    char main_c[1024];
    snprintf(main_c, sizeof(main_c), "%s/main.c", src_dir);
    if (write_file(main_c, "#include <stdio.h>\nint main(){return 0;}\n") != 0) return 1;

    char makefile_path[1024];
    snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", workspace);
    if (write_file(makefile_path, "all:\n\t@echo ok\n") != 0) return 1;

    clearDiagnostics();
    addDiagnostic(main_c, 1, 1, "phase6 warn", DIAG_SEVERITY_WARNING);

    FisicsSymbol phase6_symbols[2];
    memset(phase6_symbols, 0, sizeof(phase6_symbols));

    phase6_symbols[0].name = "MyStruct";
    phase6_symbols[0].file_path = main_c;
    phase6_symbols[0].start_line = 1;
    phase6_symbols[0].start_col = 1;
    phase6_symbols[0].end_line = 4;
    phase6_symbols[0].end_col = 1;
    phase6_symbols[0].kind = FISICS_SYMBOL_STRUCT;
    phase6_symbols[0].stable_id = 0x1111111111111111ULL;
    phase6_symbols[0].is_definition = true;

    phase6_symbols[1].name = "value";
    phase6_symbols[1].file_path = main_c;
    phase6_symbols[1].start_line = 2;
    phase6_symbols[1].start_col = 5;
    phase6_symbols[1].end_line = 2;
    phase6_symbols[1].end_col = 10;
    phase6_symbols[1].kind = FISICS_SYMBOL_FIELD;
    phase6_symbols[1].parent_name = "MyStruct";
    phase6_symbols[1].parent_kind = FISICS_SYMBOL_STRUCT;
    phase6_symbols[1].stable_id = 0x2222222222222222ULL;
    phase6_symbols[1].parent_stable_id = 0x1111111111111111ULL;
    phase6_symbols[1].is_definition = true;

    analysis_symbols_store_upsert(main_c, phase6_symbols, 2);

    FisicsSymbol fallback_symbol;
    memset(&fallback_symbol, 0, sizeof(fallback_symbol));
    fallback_symbol.name = "field_a";
    fallback_symbol.kind = FISICS_SYMBOL_FIELD;
    fallback_symbol.parent_name = "MyStruct";
    fallback_symbol.parent_kind = FISICS_SYMBOL_STRUCT;

    FisicsAnalysisResult fallback_result;
    memset(&fallback_result, 0, sizeof(fallback_result));
    snprintf(fallback_result.contract.contract_id,
             sizeof(fallback_result.contract.contract_id),
             "%s",
             IDE_FISICS_CONTRACT_ID);
    fallback_result.contract.contract_major = 1;
    fallback_result.contract.contract_minor = 2;
    fallback_result.contract.contract_patch = 0;
    fallback_result.symbols = &fallback_symbol;
    fallback_result.symbol_count = 1;

    char fallback_warning[256];
    if (!fisics_contract_should_warn_parent_link_fallback(&fallback_result,
                                                          fallback_warning,
                                                          sizeof(fallback_warning))) {
        fprintf(stderr, "expected parent link fallback warning for missing parent_stable_id\n");
        return 1;
    }
    fallback_symbol.parent_stable_id = 0x1111111111111111ULL;
    if (fisics_contract_should_warn_parent_link_fallback(&fallback_result,
                                                         fallback_warning,
                                                         sizeof(fallback_warning))) {
        fprintf(stderr, "unexpected parent link fallback warning when parent_stable_id present\n");
        return 1;
    }

    library_index_begin(workspace);
    library_index_add_include(main_c, "stdio.h", "/usr/include/stdio.h", LIB_INCLUDE_KIND_SYSTEM, LIB_BUCKET_SYSTEM, 1, 1);
    library_index_finalize();

    if (!ide_ipc_start(workspace)) return 1;
    ide_ipc_set_open_handler(open_stub, NULL);
    ide_ipc_set_edit_handler(edit_stub, NULL);

    const char* socket_path = ide_ipc_socket_path();
    const char* auth_token = ide_ipc_auth_token();
    if (!socket_path || !*socket_path) {
        ide_ipc_stop();
        return 1;
    }
    if (!auth_token || !*auth_token) {
        ide_ipc_stop();
        return 1;
    }

    char response[65536];

    if (send_then_recv(socket_path, "{\"id\":\"p1\",\"proto\":1,\"cmd\":\"ping\",\"args\":{}}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "session_id") != 0) {
        fprintf(stderr, "ping failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{\"id\":\"d1\",\"proto\":1,\"cmd\":\"diag\",\"args\":{}}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "diagnostics") != 0) {
        fprintf(stderr, "diag failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{\"id\":\"s1\",\"proto\":1,\"cmd\":\"symbols\",\"args\":{}}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "symbols") != 0) {
        fprintf(stderr, "symbols failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    if (expect_symbol_parent_stable_id(response, "value", "0x1111111111111111") != 0) {
        fprintf(stderr, "symbols missing expected parent_stable_id: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{\"id\":\"t1\",\"proto\":1,\"cmd\":\"tokens\",\"args\":{}}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "tokens") != 0) {
        fprintf(stderr, "tokens failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{\"id\":\"i1\",\"proto\":1,\"cmd\":\"includes\",\"args\":{\"graph\":true}}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "buckets") != 0) {
        fprintf(stderr, "includes failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{\"id\":\"f1\",\"proto\":1,\"cmd\":\"search\",\"args\":{\"pattern\":\"main\",\"max\":10}}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "matches") != 0) {
        fprintf(stderr, "search failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_json_with_token(socket_path, "b1", "build", auth_token, "{}", false, response, sizeof(response)) != 0 ||
        expect_ok_with_result(response, "exit_code") != 0) {
        fprintf(stderr, "build failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_json_with_token(socket_path,
                             "o1",
                             "open",
                             auth_token,
                             "{\"path\":\"src/main.c\",\"line\":1,\"col\":1}",
                             true,
                             response,
                             sizeof(response)) != 0 ||
        expect_ok_with_result(response, "applied") != 0) {
        fprintf(stderr, "open failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_json_with_token(socket_path,
                             "e1",
                             "edit",
                             auth_token,
                             "{\"op\":\"apply\",\"diff\":\"--- a/src/main.c\\n+++ b/src/main.c\\n@@ -1,2 +1,2 @@\\n-#include <stdio.h>\\n+#include <stdlib.h>\\n int main(){return 0;}\\n\",\"check_hash\":false,\"hashes\":{}}",
                             true,
                             response,
                             sizeof(response)) != 0 ||
        expect_ok_with_result(response, "touched_files") != 0) {
        fprintf(stderr, "edit failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{", false, response, sizeof(response)) != 0 ||
        expect_error_code(response, "bad_json") != 0) {
        fprintf(stderr, "bad_json check failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    if (send_then_recv(socket_path, "{\"id\":\"u1\",\"proto\":1,\"cmd\":\"nope\",\"args\":{}}", false, response, sizeof(response)) != 0 ||
        expect_error_code(response, "unknown_cmd") != 0) {
        fprintf(stderr, "unknown_cmd check failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    ide_ipc_stop();
    library_index_reset();
    printf("idebridge_phase6_check: ok\n");
    return 0;
}
