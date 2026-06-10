#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "Compiler/diagnostics.h"
#include "core/Diagnostics/diagnostic_explanations.h"

static const char* k_sample_catalog =
    "{\"profile\":\"fisics_diagnostic_explanations\",\"schema_version\":1,"
    "\"diagnostics\":[{"
    "\"code_id\":4103,"
    "\"code_name\":\"extension.units.assign_dim_mismatch\","
    "\"category_id\":8,"
    "\"category_name\":\"extension\","
    "\"stage\":\"extension\","
    "\"description\":\"sample description\","
    "\"common_causes\":\"sample causes\","
    "\"next_action\":\"sample action\""
    "}]}";

int main(void) {
    const char* workspace_root = "/tmp/diagnostic_explanations_cache";
    mkdir(workspace_root, 0755);

    diagnostic_explanations_clear();
    assert(diagnostic_explanations_load_json_text(k_sample_catalog));
    assert(diagnostic_explanations_count() == 1u);

    const DiagnosticExplanation* parsed =
        diagnostic_explanations_find_by_name("extension.units.assign_dim_mismatch");
    assert(parsed);
    assert(parsed->codeId == FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH);
    assert(strcmp(parsed->description, "sample description") == 0);
    assert(strcmp(parsed->commonCauses, "sample causes") == 0);
    assert(strcmp(parsed->nextAction, "sample action") == 0);

    assert(diagnostic_explanations_save(workspace_root));
    diagnostic_explanations_clear();
    assert(diagnostic_explanations_load(workspace_root));
    parsed = diagnostic_explanations_find_by_code(FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH);
    assert(parsed);
    assert(strcmp(parsed->stage, "extension") == 0);
    assert(strcmp(parsed->nextAction, "sample action") == 0);

    diagnostic_explanations_clear();
    assert(diagnostic_explanations_refresh_from_fisics_metadata());
    const DiagnosticExplanation* metadata =
        diagnostic_explanations_find_by_code(FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH);
    assert(metadata);
    assert(strcmp(metadata->codeName, "extension.units.assign_dim_mismatch") == 0);
    assert(strcmp(metadata->categoryName, "extension") == 0);
    assert(strcmp(metadata->stage, "extension") == 0);
    assert(strstr(metadata->description, "units extension assignment") != NULL);
    assert(strstr(metadata->nextAction, "conversion") != NULL);

    diagnostic_explanations_clear();
    puts("diagnostic_explanations_cache_test: success");
    return 0;
}
