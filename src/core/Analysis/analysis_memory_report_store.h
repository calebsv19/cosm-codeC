#ifndef ANALYSIS_MEMORY_REPORT_STORE_H
#define ANALYSIS_MEMORY_REPORT_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int active;
    uint64_t leaked_bytes;
    int allocs;
    int frees;
    int double_free;
    int unknown_free;
    int tracker_failures;
} AnalysisMemoryReportSummary;

typedef struct {
    uint64_t size;
    char* file;
    int line;
} AnalysisMemoryReportLeak;

typedef struct {
    char* path;
    char* profile;
    int schema_version;
    char* runtime;
    char* trigger;
    AnalysisMemoryReportSummary summary;
    AnalysisMemoryReportLeak* leaks;
    size_t leak_count;
    uint64_t stamp;
} AnalysisMemoryReportSnapshot;

void analysis_memory_report_store_lock(void);
void analysis_memory_report_store_unlock(void);

void analysis_memory_report_store_clear(void);
bool analysis_memory_report_store_load_json_text(const char* reportPath, const char* jsonText);
bool analysis_memory_report_store_load_file(const char* reportPath);
void analysis_memory_report_store_remove(const char* reportPath);

size_t analysis_memory_report_store_snapshot_count(void);
const AnalysisMemoryReportSnapshot* analysis_memory_report_store_snapshot_at(size_t idx);
uint64_t analysis_memory_report_store_combined_stamp(void);

void analysis_memory_report_store_save(const char* workspaceRoot);
void analysis_memory_report_store_load(const char* workspaceRoot);

#endif
