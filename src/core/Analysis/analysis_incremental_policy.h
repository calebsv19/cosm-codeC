#ifndef ANALYSIS_INCREMENTAL_POLICY_H
#define ANALYSIS_INCREMENTAL_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#include "core/Analysis/analysis_snapshot.h"

bool analysis_incremental_policy_should_analyze_hint(const AnalysisSnapshot* cached,
                                                     const AnalysisSnapshot* current,
                                                     const char* path);

bool analysis_incremental_policy_requires_full_for_missing_include_graph(
    size_t include_graph_entry_count,
    char* const* dirty_paths,
    size_t dirty_count,
    char* const* removed_paths,
    size_t removed_count,
    const char* const* file_hints,
    size_t file_hint_count);

#endif // ANALYSIS_INCREMENTAL_POLICY_H
