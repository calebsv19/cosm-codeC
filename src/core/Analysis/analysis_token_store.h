#ifndef ANALYSIS_TOKEN_STORE_H
#define ANALYSIS_TOKEN_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "fisics_frontend.h"

typedef struct {
    char* path;
    FisicsTokenSpan* spans;
    size_t count;
    uint64_t stamp;
} AnalysisFileTokens;

void analysis_token_store_clear(void);

void analysis_token_store_upsert(const char* filePath,
                                 const FisicsTokenSpan* spans,
                                 size_t spanCount);

size_t analysis_token_store_file_count(void);
const AnalysisFileTokens* analysis_token_store_file_at(size_t idx);

void analysis_token_store_save(const char* workspaceRoot);
void analysis_token_store_load(const char* workspaceRoot);

#endif // ANALYSIS_TOKEN_STORE_H
