#ifndef ANALYSIS_BUILD_GRAPH_STORE_H
#define ANALYSIS_BUILD_GRAPH_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool available;
    int total;
    int errors;
    int warnings;
    int notes;
    bool partial;
    bool fatal;
} AnalysisBuildGraphDiagnosticSummary;

typedef struct {
    char* id;
    char* source;
    char* object;
    char* status;
    AnalysisBuildGraphDiagnosticSummary diagnostic_summary;
} AnalysisBuildGraphTranslationUnit;

typedef struct {
    char* id;
    char* kind;
    char* status;
    bool will_execute;
    char* source;
    char* object;
    char* output;
    AnalysisBuildGraphDiagnosticSummary diagnostic_summary;
} AnalysisBuildGraphAction;

typedef struct {
    char* path;
    char* schema;
    int version;
    char* mode;
    char* project_root;
    bool partial;
    bool fatal;
    AnalysisBuildGraphDiagnosticSummary diagnostic_summary;
    AnalysisBuildGraphTranslationUnit* translation_units;
    size_t translation_unit_count;
    AnalysisBuildGraphAction* actions;
    size_t action_count;
    uint64_t stamp;
} AnalysisBuildGraphSnapshot;

void analysis_build_graph_store_lock(void);
void analysis_build_graph_store_unlock(void);

void analysis_build_graph_store_clear(void);
bool analysis_build_graph_store_load_json_text(const char* graphPath, const char* jsonText);
bool analysis_build_graph_store_load_file(const char* graphPath);
void analysis_build_graph_store_remove(const char* graphPath);

size_t analysis_build_graph_store_snapshot_count(void);
const AnalysisBuildGraphSnapshot* analysis_build_graph_store_snapshot_at(size_t idx);
uint64_t analysis_build_graph_store_combined_stamp(void);

void analysis_build_graph_store_save(const char* workspaceRoot);
void analysis_build_graph_store_load(const char* workspaceRoot);

#endif
