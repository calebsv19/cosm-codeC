#include "core/Diagnostics/diagnostic_context.h"

#include <assert.h>
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* k_context_json =
    "{"
    "\"profile\":\"fisics_diagnostics_v1\","
    "\"diagnostics\":["
    "{"
    "\"file\":\"/tmp/diag_context/main.c\","
    "\"line\":4,\"column\":12,\"length\":5,"
    "\"code_id\":3000,\"code_name\":\"preprocessor.generic\","
    "\"message\":\"unterminated include\","
    "\"include_stack\":["
    "{\"file\":\"/tmp/diag_context/main.c\",\"origin\":\"#include \\\"bad.h\\\"\",\"resolved\":true},"
    "{\"file\":\"/tmp/diag_context/bad.h\",\"origin\":\"bad.h\",\"resolved\":true}"
    "]"
    "},"
    "{"
    "\"file\":\"/tmp/diag_context/macro.c\","
    "\"line\":3,\"column\":10,\"length\":4,"
    "\"code_id\":3000,\"code_name\":\"preprocessor.generic\","
    "\"message\":\"macro argument count mismatch\","
    "\"macro_trace\":["
    "{\"macro\":\"NEED2\",\"role\":\"call_site\",\"file\":\"/tmp/diag_context/macro.c\",\"line\":3,\"column\":12},"
    "{\"macro\":\"NEED2\",\"role\":\"definition\",\"file\":\"/tmp/diag_context/macro.c\",\"line\":1,\"column\":9}"
    "]"
    "},"
    "{"
    "\"file\":\"/tmp/diag_context/units.c\","
    "\"line\":8,\"column\":7,\"length\":6,"
    "\"code_id\":4103,\"code_name\":\"extension.units.assign_dim_mismatch\","
    "\"message\":\"assignment dimension mismatch\","
    "\"details\":{"
    "\"context\":\"assignment\","
    "\"lhs_dim_text\":\"m\","
    "\"lhs_dim\":[1,0,0,0,0,0,0,0],"
    "\"rhs_dim_text\":\"s\","
    "\"rhs_dim\":[0,0,1,0,0,0,0,0]"
    "}"
    "}"
    "]"
    "}";

static void assert_context_shape(void) {
    assert(diagnostic_context_count() == 3u);

    const DiagnosticContextRecord* include =
        diagnostic_context_find("/tmp/diag_context/main.c",
                                4,
                                12,
                                3000,
                                "preprocessor.generic",
                                "unterminated include");
    assert(include);
    assert(include->includeStackJson);
    assert(strstr(include->includeStackJson, "bad.h"));
    assert(!include->macroTraceJson);
    assert(!include->detailsJson);

    const DiagnosticContextRecord* macro =
        diagnostic_context_find("/tmp/diag_context/macro.c",
                                3,
                                10,
                                3000,
                                "preprocessor.generic",
                                "macro argument count mismatch");
    assert(macro);
    assert(macro->macroTraceJson);
    assert(strstr(macro->macroTraceJson, "call_site"));

    const DiagnosticContextRecord* units =
        diagnostic_context_find("/tmp/diag_context/units.c",
                                8,
                                7,
                                4103,
                                "extension.units.assign_dim_mismatch",
                                "assignment dimension mismatch");
    assert(units);
    assert(units->detailsJson);
    json_object* details = json_tokener_parse(units->detailsJson);
    assert(details);
    json_object* context = NULL;
    assert(json_object_object_get_ex(details, "context", &context));
    assert(strcmp(json_object_get_string(context), "assignment") == 0);
    json_object_put(details);
}

int main(void) {
    assert(diagnostic_context_load_json_text(k_context_json));
    assert_context_shape();

    const char* workspace = "/tmp/diagnostic_context_cache";
    mkdir(workspace, 0755);
    assert(diagnostic_context_save(workspace));
    diagnostic_context_clear();
    assert(diagnostic_context_load(workspace));
    assert_context_shape();

    diagnostic_context_clear();
    puts("diagnostic_context_cache_test: success");
    return 0;
}
