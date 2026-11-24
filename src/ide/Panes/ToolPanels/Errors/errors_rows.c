#include "errors_rows.h"
#include "errors_filter.h"

static bool diag_visible(const ErrorsRowSource* source, const Diagnostic* diag) {
    bool severityVisible = source->showAll ||
        (diag->severity == DIAG_SEVERITY_ERROR && source->showErrors) ||
        (diag->severity == DIAG_SEVERITY_WARNING && source->showWarnings);
    return severityVisible && errors_filter_diagnostic_matches_query(diag, source->query);
}

int errors_build_rows(const ErrorsRowSource* source, FlatDiagRef* out, int max) {
    int total = 0;
    for (int fi = 0; fi < source->fileCount && total < max; ++fi) {
        const ErrorFileSnapshot* file = &source->files[fi];
        int firstVisible = 0;
        while (firstVisible < file->count && !diag_visible(source, &file->diags[firstVisible])) {
            ++firstVisible;
        }
        if (firstVisible >= file->count) continue;

        out[total++] = (FlatDiagRef){ .path = file->path, .fileIndex = fi, .isHeader = true };
        if ((size_t)fi < source->collapseCount && source->collapsed[fi]) continue;

        for (int di = firstVisible; di < file->count && total < max; ++di) {
            if (!diag_visible(source, &file->diags[di])) continue;
            out[total++] = (FlatDiagRef){
                .diag = &file->diags[di], .path = file->path, .fileIndex = fi
            };
        }
    }
    return total;
}
