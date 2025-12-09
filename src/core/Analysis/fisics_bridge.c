#include "core/Analysis/fisics_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_view.h"

typedef struct {
    char* filePath;
    FisicsTokenSpan* spans;
    size_t count;
} TokenCache;

typedef struct {
    char* filePath;
    FisicsSymbol* symbols;
    size_t count;
} SymbolCache;

static TokenCache g_tokens = {0};
static SymbolCache g_symbols = {0};

static void free_token_cache(void) {
    free(g_tokens.filePath);
    free(g_tokens.spans);
    memset(&g_tokens, 0, sizeof(g_tokens));
}

static void free_symbol_cache(void) {
    free(g_symbols.filePath);
    for (size_t i = 0; i < g_symbols.count; ++i) {
        free((char*)g_symbols.symbols[i].name);
        free((char*)g_symbols.symbols[i].file_path);
    }
    free(g_symbols.symbols);
    memset(&g_symbols, 0, sizeof(g_symbols));
}

static DiagnosticSeverity map_severity(DiagKind kind) {
    switch (kind) {
        case DIAG_WARNING: return DIAG_SEVERITY_WARNING;
        case DIAG_NOTE:    return DIAG_SEVERITY_INFO;
        case DIAG_ERROR:
        default:           return DIAG_SEVERITY_ERROR;
    }
}

static void cache_tokens(const char* filePath, const FisicsTokenSpan* spans, size_t count) {
    free_token_cache();
    if (!filePath || !spans || count == 0) return;

    g_tokens.filePath = strdup(filePath);
    g_tokens.spans = malloc(sizeof(FisicsTokenSpan) * count);
    if (!g_tokens.spans || !g_tokens.filePath) {
        free_token_cache();
        return;
    }
    memcpy(g_tokens.spans, spans, sizeof(FisicsTokenSpan) * count);
    g_tokens.count = count;
}

static void cache_symbols(const char* filePath, const FisicsSymbol* symbols, size_t count) {
    free_symbol_cache();
    if (!filePath || !symbols || count == 0) return;

    g_symbols.filePath = strdup(filePath);
    g_symbols.symbols = calloc(count, sizeof(FisicsSymbol));
    if (!g_symbols.symbols || !g_symbols.filePath) {
        free_symbol_cache();
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        g_symbols.symbols[i] = symbols[i];
        if (symbols[i].name) {
            g_symbols.symbols[i].name = strdup(symbols[i].name);
        }
        if (symbols[i].file_path) {
            g_symbols.symbols[i].file_path = strdup(symbols[i].file_path);
        }
    }
    g_symbols.count = count;
}

void ide_analyze_buffer_for_file(const char* filePath, const char* contents, size_t length) {
    if (!filePath || !contents) return;

    FisicsAnalysisResult result;
    memset(&result, 0, sizeof(result));

    bool ok = fisics_analyze_buffer(filePath, contents, length, &result);
    (void)ok; // ok may be false but still yield diagnostics

    analysis_store_upsert(filePath, result.diagnostics, result.diag_count);
    analysis_store_flatten_to_engine();

    cache_tokens(filePath, result.tokens, result.token_count);
    cache_symbols(filePath, result.symbols, result.symbol_count);

    fisics_free_analysis_result(&result);
}

void ide_analyze_open_file(OpenFile* file) {
    if (!file || !file->filePath || !file->buffer) return;

    size_t length = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &length);
    if (!snapshot) return;

    ide_analyze_buffer_for_file(file->filePath, snapshot, length);
    free(snapshot);
}

const FisicsTokenSpan* fisics_bridge_get_tokens(const char** filePathOut, size_t* countOut) {
    if (filePathOut) *filePathOut = g_tokens.filePath;
    if (countOut) *countOut = g_tokens.count;
    return g_tokens.spans;
}

const FisicsSymbol* fisics_bridge_get_symbols(const char** filePathOut, size_t* countOut) {
    if (filePathOut) *filePathOut = g_symbols.filePath;
    if (countOut) *countOut = g_symbols.count;
    return g_symbols.symbols;
}
