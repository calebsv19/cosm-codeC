#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "fisics_frontend.h"

#define IDE_FISICS_CONTRACT_ID "fisiCs.analysis.contract"
#define IDE_FISICS_CONTRACT_SUPPORTED_MAJOR 1
#define IDE_FISICS_CONTRACT_PARENT_LINK_MINOR 2
#define IDE_FISICS_CONTRACT_DIAG_TAXONOMY_MINOR 3
#define IDE_FISICS_CONTRACT_CAPABILITY_FLAGS_MINOR 4

static inline bool fisics_contract_should_degrade(const FisicsAnalysisResult* result,
                                                  char* warning_out,
                                                  size_t warning_out_cap) {
    if (warning_out && warning_out_cap > 0) warning_out[0] = '\0';
    if (!result) {
        if (warning_out && warning_out_cap > 0) {
            snprintf(warning_out, warning_out_cap, "missing analysis result");
        }
        return true;
    }

    if (result->contract.contract_id[0] == '\0') {
        if (warning_out && warning_out_cap > 0) {
            snprintf(warning_out, warning_out_cap, "missing contract id");
        }
        return true;
    }

    if (strcmp(result->contract.contract_id, IDE_FISICS_CONTRACT_ID) != 0) {
        if (warning_out && warning_out_cap > 0) {
            snprintf(warning_out,
                     warning_out_cap,
                     "unexpected contract id '%s' (expected '%s')",
                     result->contract.contract_id,
                     IDE_FISICS_CONTRACT_ID);
        }
        return true;
    }

    if ((int)result->contract.contract_major != IDE_FISICS_CONTRACT_SUPPORTED_MAJOR) {
        if (warning_out && warning_out_cap > 0) {
            snprintf(warning_out,
                     warning_out_cap,
                     "unsupported contract major %u (supported major %d)",
                     (unsigned)result->contract.contract_major,
                     IDE_FISICS_CONTRACT_SUPPORTED_MAJOR);
        }
        return true;
    }

    return false;
}

static inline uint64_t fisics_contract_effective_capabilities(const FisicsAnalysisResult* result) {
    if (!result) return 0;
    if ((int)result->contract.contract_major != IDE_FISICS_CONTRACT_SUPPORTED_MAJOR) return 0;

    if ((int)result->contract.contract_minor >= IDE_FISICS_CONTRACT_CAPABILITY_FLAGS_MINOR) {
        return result->contract.capabilities;
    }

    uint64_t caps = 0;
    caps |= FISICS_CONTRACT_CAP_DIAGNOSTICS;
    caps |= FISICS_CONTRACT_CAP_INCLUDES;
    caps |= FISICS_CONTRACT_CAP_SYMBOLS;
    caps |= FISICS_CONTRACT_CAP_TOKENS;
    if ((int)result->contract.contract_minor >= IDE_FISICS_CONTRACT_PARENT_LINK_MINOR) {
        caps |= FISICS_CONTRACT_CAP_SYMBOL_PARENT_STABLE_ID;
    }
    if ((int)result->contract.contract_minor >= IDE_FISICS_CONTRACT_DIAG_TAXONOMY_MINOR) {
        caps |= FISICS_CONTRACT_CAP_DIAGNOSTIC_TAXONOMY;
    }
    return caps;
}

static inline bool fisics_contract_has_capability(const FisicsAnalysisResult* result, uint64_t capability) {
    return (fisics_contract_effective_capabilities(result) & capability) != 0;
}

static inline bool fisics_contract_symbols_enabled(const FisicsAnalysisResult* result, bool degraded_contract) {
    if (degraded_contract) return false;
    return fisics_contract_has_capability(result, FISICS_CONTRACT_CAP_SYMBOLS);
}

static inline bool fisics_contract_tokens_enabled(const FisicsAnalysisResult* result, bool degraded_contract) {
    if (degraded_contract) return false;
    return fisics_contract_has_capability(result, FISICS_CONTRACT_CAP_TOKENS);
}

static inline bool fisics_contract_should_warn_missing_capability(const FisicsAnalysisResult* result,
                                                                  uint64_t capability,
                                                                  const char* capability_name,
                                                                  char* warning_out,
                                                                  size_t warning_out_cap) {
    if (warning_out && warning_out_cap > 0) warning_out[0] = '\0';
    if (!result) return false;
    if ((int)result->contract.contract_major != IDE_FISICS_CONTRACT_SUPPORTED_MAJOR) return false;
    if ((int)result->contract.contract_minor < IDE_FISICS_CONTRACT_CAPABILITY_FLAGS_MINOR) return false;
    if (fisics_contract_has_capability(result, capability)) return false;

    if (warning_out && warning_out_cap > 0) {
        snprintf(warning_out,
                 warning_out_cap,
                 "contract 1.%u does not advertise '%s' capability; consumer lane disabled",
                 (unsigned)result->contract.contract_minor,
                 capability_name ? capability_name : "unknown");
    }
    return true;
}

static inline bool fisics_contract_should_warn_parent_link_fallback(const FisicsAnalysisResult* result,
                                                                    char* warning_out,
                                                                    size_t warning_out_cap) {
    if (warning_out && warning_out_cap > 0) warning_out[0] = '\0';
    if (!result) return false;
    if ((int)result->contract.contract_major != IDE_FISICS_CONTRACT_SUPPORTED_MAJOR) return false;
    if (!fisics_contract_has_capability(result, FISICS_CONTRACT_CAP_SYMBOL_PARENT_STABLE_ID)) return false;
    if (!result->symbols || result->symbol_count == 0) return false;

    size_t owned_count = 0;
    size_t missing_link_count = 0;
    for (size_t i = 0; i < result->symbol_count; ++i) {
        const FisicsSymbol* sym = &result->symbols[i];
        if (!sym->parent_name || !sym->parent_name[0]) continue;
        if (sym->parent_kind == FISICS_SYMBOL_UNKNOWN) continue;
        owned_count++;
        if (sym->parent_stable_id == 0) {
            missing_link_count++;
        }
    }

    if (missing_link_count == 0) return false;

    if (warning_out && warning_out_cap > 0) {
        snprintf(warning_out,
                 warning_out_cap,
                 "contract 1.%u symbol graph lane missing parent_stable_id for %zu/%zu owned symbols; using owner-name fallback",
                 (unsigned)result->contract.contract_minor,
                 missing_link_count,
                 owned_count);
    }
    return true;
}
