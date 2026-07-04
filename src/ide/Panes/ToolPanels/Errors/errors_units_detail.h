#ifndef ERRORS_UNITS_DETAIL_H
#define ERRORS_UNITS_DETAIL_H

#include "core/Diagnostics/diagnostics_engine.h"
#include <stdbool.h>

typedef struct {
    bool hasDimensionDetail;
    char context[64];
    char leftLabel[16];
    char leftDimText[96];
    char rightLabel[16];
    char rightDimText[96];
    bool hasSymbolUnits;
    char symbolStableId[24];
    char symbolName[128];
    char unitText[128];
    char unitFamily[96];
} ErrorsUnitsDiagnosticDetail;

bool errors_units_detail_for_diagnostic(const Diagnostic* diag,
                                        ErrorsUnitsDiagnosticDetail* outDetail);

#endif
