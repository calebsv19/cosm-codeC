#ifndef ANALYSIS_UNITS_STORE_H
#define ANALYSIS_UNITS_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fisics_frontend.h"

typedef struct {
    uint64_t symbol_stable_id;
    char* symbol_name;
    char* dim_text;
    int8_t dim[FISICS_UNITS_DIM_SLOTS];
    bool resolved;
    char* unit_source_text;
    char* unit_name;
    char* unit_symbol;
    char* unit_family;
    bool unit_resolved;
    bool has_concrete_unit;
} AnalysisUnitsAttachment;

typedef struct {
    char* path;
    AnalysisUnitsAttachment* attachments;
    size_t count;
    uint64_t stamp;
} AnalysisFileUnits;

void analysis_units_store_lock(void);
void analysis_units_store_unlock(void);

void analysis_units_store_clear(void);
void analysis_units_store_upsert(const char* filePath,
                                 const FisicsUnitsAttachment* attachments,
                                 size_t attachmentCount,
                                 bool concreteUnitFieldsEnabled);
void analysis_units_store_remove(const char* filePath);

size_t analysis_units_store_file_count(void);
const AnalysisFileUnits* analysis_units_store_file_at(size_t idx);
const AnalysisUnitsAttachment* analysis_units_store_find_by_symbol_id(uint64_t symbolStableId);
uint64_t analysis_units_store_combined_stamp(void);

void analysis_units_store_save(const char* workspaceRoot);
void analysis_units_store_load(const char* workspaceRoot);

#endif
