#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/fisics_contract_validation.h"

static int failures = 0;

static void check(bool condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
}

static FisicsAnalysisResult contract_minor(unsigned minor, uint64_t advertised) {
    FisicsAnalysisResult result = {0};
    snprintf(result.contract.contract_id,
             sizeof(result.contract.contract_id),
             "%s",
             IDE_FISICS_CONTRACT_ID);
    result.contract.contract_major = 1;
    result.contract.contract_minor = (uint16_t)minor;
    result.contract.capabilities = advertised;
    return result;
}

int main(void) {
    const uint64_t base = FISICS_CONTRACT_CAP_DIAGNOSTICS |
                          FISICS_CONTRACT_CAP_INCLUDES |
                          FISICS_CONTRACT_CAP_SYMBOLS |
                          FISICS_CONTRACT_CAP_TOKENS;
    FisicsAnalysisResult v10 = contract_minor(0, 0);
    check(fisics_contract_effective_capabilities(&v10) == base,
          "1.0 infers base capabilities");

    FisicsAnalysisResult v12 = contract_minor(2, 0);
    check((fisics_contract_effective_capabilities(&v12) &
           FISICS_CONTRACT_CAP_SYMBOL_PARENT_STABLE_ID) != 0,
          "1.2 infers parent stable ids");

    FisicsAnalysisResult v13 = contract_minor(3, 0);
    check((fisics_contract_effective_capabilities(&v13) &
           FISICS_CONTRACT_CAP_DIAGNOSTIC_TAXONOMY) != 0,
          "1.3 infers diagnostic taxonomy");

    FisicsAnalysisResult v14 = contract_minor(4, FISICS_CONTRACT_CAP_DIAGNOSTICS);
    check(fisics_contract_effective_capabilities(&v14) ==
              FISICS_CONTRACT_CAP_DIAGNOSTICS,
          "1.4 honors advertised capabilities exactly");
    check(!fisics_contract_symbols_enabled(&v14, false),
          "missing optional symbol capability disables lane");

    FisicsAnalysisResult v17 = contract_minor(
        7,
        base | FISICS_CONTRACT_CAP_EXTENSION_UNITS_ATTACHMENTS |
            FISICS_CONTRACT_CAP_EXTENSION_UNITS_CONCRETE);
    check(fisics_contract_units_attachments_enabled(&v17, false),
          "current major-1 units attachments");
    check(fisics_contract_units_concrete_enabled(&v17, false),
          "current major-1 concrete units");

    char warning[256];
    FisicsAnalysisResult v2 = v17;
    v2.contract.contract_major = 2;
    check(fisics_contract_should_degrade(&v2, warning, sizeof(warning)),
          "unsupported major degrades");
    check(fisics_contract_effective_capabilities(&v2) == 0,
          "unsupported major exposes no optional lanes");

    if (failures) return 1;
    puts("fisiCs contract major-1 compatibility test passed");
    return 0;
}
