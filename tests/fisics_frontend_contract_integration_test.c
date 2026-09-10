#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fisics_frontend.h"

static int failures = 0;

static void check(bool condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
}

static const FisicsSymbol* find_symbol(const FisicsAnalysisResult* result,
                                       const char* name) {
    for (size_t i = 0; result && i < result->symbol_count; ++i) {
        if (result->symbols[i].name && strcmp(result->symbols[i].name, name) == 0) {
            return &result->symbols[i];
        }
    }
    return NULL;
}

static void check_contract_base(const FisicsAnalysisResult* result) {
    const uint64_t base = FISICS_CONTRACT_CAP_DIAGNOSTICS |
                          FISICS_CONTRACT_CAP_INCLUDES |
                          FISICS_CONTRACT_CAP_SYMBOLS |
                          FISICS_CONTRACT_CAP_TOKENS |
                          FISICS_CONTRACT_CAP_SYMBOL_PARENT_STABLE_ID |
                          FISICS_CONTRACT_CAP_DIAGNOSTIC_TAXONOMY;
    check(strcmp(result->contract.contract_id, "fisiCs.analysis.contract") == 0,
          "real frontend contract id");
    check(result->contract.contract_major == 1, "real frontend contract major");
    check((result->contract.capabilities & base) == base,
          "real frontend base capabilities");
}

int main(void) {
    char temp_template[] = "/tmp/ide-s2-frontend-XXXXXX";
    char* temp_dir = mkdtemp(temp_template);
    check(temp_dir != NULL, "temporary fixture directory");
    if (!temp_dir) return 1;

    char header_path[1024];
    char source_path[1024];
    snprintf(header_path, sizeof(header_path), "%s/resolved_dep.h", temp_dir);
    snprintf(source_path, sizeof(source_path), "%s/contract_fixture.c", temp_dir);
    FILE* header = fopen(header_path, "w");
    check(header != NULL, "resolved include fixture creation");
    if (header) {
        fputs("#define RESOLVED_VALUE 7\n", header);
        fclose(header);
    }

    const char* valid_source =
        "#include \"resolved_dep.h\"\n"
        "enum ContractStatus { CONTRACT_READY = 1, CONTRACT_PENDING = 2 };\n"
        "double speed [[fisics::dim(speed)]] [[fisics::unit(feet_per_second)]] = 1.0;\n"
        "int read_status(enum ContractStatus s) { return (int)s + RESOLVED_VALUE; }\n";
    const char* include_paths[] = {temp_dir};
    FisicsFrontendOptions options = {0};
    options.include_paths = include_paths;
    options.include_path_count = 1;
    options.lenient_mode = 1;
    options.overlay_features = FISICS_OVERLAY_PHYSICS_UNITS;

    FisicsAnalysisResult valid = {0};
    check(fisics_analyze_buffer(source_path,
                                valid_source,
                                strlen(valid_source),
                                &options,
                                &valid),
          "real frontend valid analysis");
    check_contract_base(&valid);
    check(!valid.contract.partial && !valid.contract.fatal,
          "valid result is authoritative");
    check(valid.token_count > 0, "real frontend tokens");
    check(valid.symbol_count > 0, "real frontend symbols");

    bool resolved_include = false;
    for (size_t i = 0; i < valid.include_count; ++i) {
        const FisicsInclude* include = &valid.includes[i];
        if (include->name && strcmp(include->name, "resolved_dep.h") == 0 &&
            include->resolved && include->resolved_path) {
            resolved_include = true;
        }
    }
    check(resolved_include, "real frontend resolved include");

    bool owned_symbol = false;
    for (size_t i = 0; i < valid.symbol_count; ++i) {
        const FisicsSymbol* symbol = &valid.symbols[i];
        if (symbol->parent_name && symbol->parent_name[0] &&
            symbol->parent_kind != FISICS_SYMBOL_UNKNOWN &&
            symbol->parent_stable_id != 0) {
            owned_symbol = true;
        }
    }
    check(owned_symbol, "real frontend parent stable ids");

    const FisicsSymbol* speed = find_symbol(&valid, "speed");
    check(speed && speed->stable_id != 0, "units symbol stable id");
    check(valid.units_attachment_count == 1, "real frontend units attachment");
    check((valid.contract.capabilities &
           FISICS_CONTRACT_CAP_EXTENSION_UNITS_ATTACHMENTS) != 0,
          "units attachment capability");
    check((valid.contract.capabilities &
           FISICS_CONTRACT_CAP_EXTENSION_UNITS_CONCRETE) != 0,
          "concrete units capability");
    if (speed && valid.units_attachment_count == 1) {
        const FisicsUnitsAttachment* units = &valid.units_attachments[0];
        check(units->symbol_stable_id == speed->stable_id,
              "units attachment symbol linkage");
        check(units->resolved && units->unit_resolved,
              "resolved dimension and concrete unit");
    }
    fisics_free_analysis_result(&valid);

    const char* partial_source =
        "#include \"definitely_missing_s2_header.h\"\n"
        "int broken( { return missing_s2_symbol + 1; }\n";
    FisicsFrontendOptions strict_options = options;
    strict_options.lenient_mode = -1;
    FisicsAnalysisResult partial = {0};
    check(fisics_analyze_buffer(source_path,
                                partial_source,
                                strlen(partial_source),
                                &strict_options,
                                &partial),
          "real frontend retains fatal payload");
    check_contract_base(&partial);
    check(partial.contract.partial && partial.contract.fatal,
          "real frontend partial and fatal contract flags");
    check(partial.diag_count > 0, "real frontend fatal diagnostics");
    bool unresolved_include = false;
    for (size_t i = 0; i < partial.include_count; ++i) {
        const FisicsInclude* include = &partial.includes[i];
        if (include->name && strcmp(include->name, "definitely_missing_s2_header.h") == 0 &&
            !include->resolved && include->origin == FISICS_INCLUDE_UNRESOLVED) {
            unresolved_include = true;
        }
    }
    check(unresolved_include, "real frontend unresolved include");
    fisics_free_analysis_result(&partial);

    unlink(header_path);
    rmdir(temp_dir);
    if (failures) return 1;
    puts("real fisiCs frontend contract integration test passed");
    return 0;
}
