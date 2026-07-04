#ifndef ERRORS_FILTER_H
#define ERRORS_FILTER_H

#include "core/Diagnostics/diagnostics_engine.h"
#include <stdbool.h>

bool errors_filter_diagnostic_matches_query(const Diagnostic* diag, const char* query);

#endif
