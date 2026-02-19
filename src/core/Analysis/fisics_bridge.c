#include "core/Analysis/fisics_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/Analysis/include_path_resolver.h"
#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/workspace_prefs.h"

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
        free((char*)g_symbols.symbols[i].parent_name);
        free((char*)g_symbols.symbols[i].return_type);
        if (g_symbols.symbols[i].param_types) {
            for (size_t p = 0; p < g_symbols.symbols[i].param_count; ++p) {
                free((char*)g_symbols.symbols[i].param_types[p]);
            }
            free(g_symbols.symbols[i].param_types);
        }
        if (g_symbols.symbols[i].param_names) {
            for (size_t p = 0; p < g_symbols.symbols[i].param_count; ++p) {
                free((char*)g_symbols.symbols[i].param_names[p]);
            }
            free(g_symbols.symbols[i].param_names);
        }
    }
    free(g_symbols.symbols);
    memset(&g_symbols, 0, sizeof(g_symbols));
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
        if (symbols[i].parent_name) {
            g_symbols.symbols[i].parent_name = strdup(symbols[i].parent_name);
        }
        if (symbols[i].return_type) {
            g_symbols.symbols[i].return_type = strdup(symbols[i].return_type);
        }
        g_symbols.symbols[i].param_types = NULL;
        g_symbols.symbols[i].param_count = symbols[i].param_count;
        if (symbols[i].param_types && symbols[i].param_count > 0) {
            g_symbols.symbols[i].param_types = (const char**)calloc(symbols[i].param_count, sizeof(char*));
            if (g_symbols.symbols[i].param_types) {
                for (size_t p = 0; p < symbols[i].param_count; ++p) {
                    if (symbols[i].param_types[p]) {
                        ((char**)g_symbols.symbols[i].param_types)[p] = strdup(symbols[i].param_types[p]);
                    }
                }
            }
        }
        g_symbols.symbols[i].param_names = NULL;
        if (symbols[i].param_names && symbols[i].param_count > 0) {
            g_symbols.symbols[i].param_names = (const char**)calloc(symbols[i].param_count, sizeof(char*));
            if (g_symbols.symbols[i].param_names) {
                for (size_t p = 0; p < symbols[i].param_count; ++p) {
                    if (symbols[i].param_names[p]) {
                        ((char**)g_symbols.symbols[i].param_names)[p] = strdup(symbols[i].param_names[p]);
                    }
                }
            }
        }
    }
    g_symbols.count = count;
}

void ide_analyze_buffer_for_file(const char* filePath, const char* contents, size_t length) {
    if (!filePath || !contents) return;

    FisicsAnalysisResult result;
    memset(&result, 0, sizeof(result));

    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* flags = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    BuildFlagSet flagsSet = {0};
    gather_build_flags(projectPath, flags, &flagsSet);
    FisicsFrontendOptions opts = {0};
    opts.include_paths = (const char* const*)flagsSet.include_paths;
    opts.include_path_count = flagsSet.include_count;
    opts.macro_defines = (const char* const*)flagsSet.macro_defines;
    opts.macro_define_count = flagsSet.macro_count;

    bool ok = fisics_analyze_buffer(filePath, contents, length, &opts, &result);
    (void)ok; // ok may be false but still yield diagnostics

    analysis_store_upsert(filePath, result.diagnostics, result.diag_count);
    analysis_symbols_store_upsert(filePath, result.symbols, result.symbol_count);
    analysis_token_store_upsert(filePath, result.tokens, result.token_count);
    analysis_store_flatten_to_engine();

    cache_tokens(filePath, result.tokens, result.token_count);
    cache_symbols(filePath, result.symbols, result.symbol_count);

    fisics_free_analysis_result(&result);
    free_build_flag_set(&flagsSet);
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
