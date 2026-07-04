#include "ide/Panes/ToolPanels/Errors/errors_filter.h"

#include <ctype.h>
#include <string.h>

static char lower_ascii(int c) {
    return (char)tolower((unsigned char)c);
}

static bool contains_query_ci(const char* haystack, const char* query) {
    if (!query || !query[0]) return true;
    if (!haystack || !haystack[0]) return false;

    size_t qlen = strlen(query);
    if (qlen == 0) return true;
    for (const char* h = haystack; *h; ++h) {
        size_t i = 0;
        while (i < qlen && h[i] &&
               lower_ascii(h[i]) == lower_ascii(query[i])) {
            i++;
        }
        if (i == qlen) return true;
    }
    return false;
}

bool errors_filter_diagnostic_matches_query(const Diagnostic* diag, const char* query) {
    if (!query || !query[0]) return true;
    if (!diag) return false;

    const char* category = diagnostic_category_name(diag->category);
    const char* codeName = (diag->codeName && diag->codeName[0])
        ? diag->codeName
        : diagnostic_code_name(diag->codeId);
    const char* stage = (diag->stage && diag->stage[0])
        ? diag->stage
        : diagnostic_stage_name(diag->codeId);

    return contains_query_ci(diag->message, query) ||
           contains_query_ci(diag->filePath, query) ||
           contains_query_ci(diag->hint, query) ||
           contains_query_ci(category, query) ||
           contains_query_ci(codeName, query) ||
           contains_query_ci(stage, query);
}
