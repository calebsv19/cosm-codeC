#ifndef ANALYSIS_SYMBOLS_STORE_H
#define ANALYSIS_SYMBOLS_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "fisics_frontend.h"

typedef struct {
    char* path;
    FisicsSymbol* symbols;
    size_t count;
    uint64_t stamp;
} AnalysisFileSymbols;

// Guards symbol-store read/write access across render/main and analysis threads.
void analysis_symbols_store_lock(void);
void analysis_symbols_store_unlock(void);

void analysis_symbols_store_clear(void);

void analysis_symbols_store_upsert(const char* filePath,
                                   const FisicsSymbol* symbols,
                                   size_t symbolCount);
void analysis_symbols_store_remove(const char* filePath);

size_t analysis_symbols_store_file_count(void);
const AnalysisFileSymbols* analysis_symbols_store_file_at(size_t idx);
uint64_t analysis_symbols_store_combined_stamp(void);

void analysis_symbols_store_save(const char* workspaceRoot);
void analysis_symbols_store_load(const char* workspaceRoot);

#endif // ANALYSIS_SYMBOLS_STORE_H
