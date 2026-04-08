#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/fisics_bridge.h"
#include "core/Analysis/fisics_frontend_guard.h"
#include "core/Analysis/include_graph.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/library_index.h"
#include "core/LoopEvents/event_queue.h"
#include "ide/Panes/Editor/editor_buffer.h"

DirEntry* projectRoot = NULL;
char projectPath[1024] = {0};
char projectRootPath[1024] = {0};
bool pendingProjectRefresh = false;
unsigned int pendingProjectRefreshReasonMask = 0u;

void queueProjectRefresh(unsigned int analysisReasonMask) {
    pendingProjectRefreshReasonMask |= analysisReasonMask;
}

const WorkspaceBuildConfig* getWorkspaceBuildConfig(void) {
    static WorkspaceBuildConfig cfg;
    cfg.build_args[0] = '\0';
    return &cfg;
}

size_t gather_build_flags(const char* project_root,
                          const char* extra_flags,
                          BuildFlagSet* out) {
    (void)project_root;
    (void)extra_flags;
    if (out) {
        out->include_paths = NULL;
        out->include_count = 0;
        out->macro_defines = NULL;
        out->macro_count = 0;
    }
    return 0;
}

void free_build_flag_set(BuildFlagSet* set) {
    (void)set;
}

void fisics_frontend_guard_lock(void) {}
void fisics_frontend_guard_unlock(void) {}

void include_graph_replace_from_result(const char* source_path,
                                       const FisicsInclude* includes,
                                       size_t include_count,
                                       const char* workspace_root) {
    (void)source_path;
    (void)includes;
    (void)include_count;
    (void)workspace_root;
}

void library_index_remove_source(const char* source_path) {
    (void)source_path;
}

void library_index_add_include(const char* source_path,
                               const char* include_name,
                               const char* resolved_path,
                               LibraryIncludeKind kind,
                               LibraryBucketKind origin,
                               int line,
                               int column) {
    (void)source_path;
    (void)include_name;
    (void)resolved_path;
    (void)kind;
    (void)origin;
    (void)line;
    (void)column;
}

char* getBufferSnapshot(EditorBuffer* buffer, size_t* outLength) {
    (void)buffer;
    if (outLength) *outLength = 0;
    return NULL;
}

static char* dup_cstr(const char* value) {
    if (!value) return NULL;
    size_t len = strlen(value) + 1;
    char* out = (char*)malloc(len);
    if (!out) return NULL;
    memcpy(out, value, len);
    return out;
}

bool fisics_analyze_buffer(const char* file_path,
                           const char* source,
                           size_t length,
                           const FisicsFrontendOptions* opts,
                           FisicsAnalysisResult* out) {
    (void)source;
    (void)opts;
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    snprintf(out->contract.contract_id, sizeof(out->contract.contract_id), "%s", "fisiCs.analysis.contract");
    out->contract.contract_major = 1;
    out->contract.contract_minor = 0;
    out->contract.contract_patch = 0;
    snprintf(out->contract.producer_name, sizeof(out->contract.producer_name), "%s", "fisiCs");
    snprintf(out->contract.producer_version, sizeof(out->contract.producer_version), "%s", "test");
    out->contract.mode = FISICS_ANALYSIS_MODE_LENIENT;
    out->contract.source_length = (uint64_t)length;

    out->diagnostics = (FisicsDiagnostic*)calloc(1, sizeof(FisicsDiagnostic));
    out->tokens = (FisicsTokenSpan*)calloc(1, sizeof(FisicsTokenSpan));
    out->symbols = (FisicsSymbol*)calloc(1, sizeof(FisicsSymbol));
    if (!out->diagnostics || !out->tokens || !out->symbols) {
        return false;
    }

    out->diag_count = 1;
    out->diagnostics[0].file_path = dup_cstr(file_path);
    out->diagnostics[0].line = 1;
    out->diagnostics[0].column = 1;
    out->diagnostics[0].length = 3;
    out->diagnostics[0].kind = DIAG_WARNING;
    out->diagnostics[0].code = 42;
    out->diagnostics[0].message = dup_cstr("bridge stub warning");
    out->diagnostics[0].hint = dup_cstr("stub hint");

    out->token_count = 1;
    out->tokens[0].line = 1;
    out->tokens[0].column = 1;
    out->tokens[0].length = 3;
    out->tokens[0].kind = FISICS_TOK_KEYWORD;

    out->symbol_count = 1;
    out->symbols[0].name = dup_cstr("bridge_fn");
    out->symbols[0].file_path = dup_cstr(file_path);
    out->symbols[0].start_line = 1;
    out->symbols[0].start_col = 1;
    out->symbols[0].end_line = 1;
    out->symbols[0].end_col = 8;
    out->symbols[0].kind = FISICS_SYMBOL_FUNCTION;
    out->symbols[0].is_definition = true;
    out->symbols[0].return_type = dup_cstr("int");

    out->includes = NULL;
    out->include_count = 0;
    return true;
}

void fisics_free_analysis_result(FisicsAnalysisResult* result) {
    if (!result) return;
    if (result->diagnostics) {
        for (size_t i = 0; i < result->diag_count; ++i) {
            free((char*)result->diagnostics[i].file_path);
            free(result->diagnostics[i].message);
            free(result->diagnostics[i].hint);
        }
        free(result->diagnostics);
    }
    if (result->symbols) {
        for (size_t i = 0; i < result->symbol_count; ++i) {
            free((char*)result->symbols[i].name);
            free((char*)result->symbols[i].file_path);
            free((char*)result->symbols[i].parent_name);
            free((char*)result->symbols[i].return_type);
        }
        free(result->symbols);
    }
    free(result->tokens);
    free(result->includes);
    memset(result, 0, sizeof(*result));
}

int main(void) {
    snprintf(projectPath, sizeof(projectPath), "%s", "/tmp/fisics_bridge_events_regression");

    loop_events_init();
    analysis_store_clear();
    analysis_symbols_store_clear();
    analysis_token_store_clear();

    const char* file_path = "/tmp/fisics_bridge_events_regression/src/a.c";
    const char* src = "int bridge_fn(void) { return 1; }";
    ide_analyze_buffer_for_file(file_path, src, strlen(src));

    IDEEvent symbol_event;
    IDEEvent diagnostics_event;
    memset(&symbol_event, 0, sizeof(symbol_event));
    memset(&diagnostics_event, 0, sizeof(diagnostics_event));

    assert(loop_events_pop(&symbol_event));
    assert(loop_events_pop(&diagnostics_event));
    IDEEvent trailing_event;
    memset(&trailing_event, 0, sizeof(trailing_event));
    assert(!loop_events_pop(&trailing_event));

    assert(symbol_event.type == IDE_EVENT_SYMBOL_TREE_UPDATED);
    assert(strcmp(symbol_event.payload.analysis.project_root, projectPath) == 0);
    assert(symbol_event.payload.analysis.data_stamp == analysis_symbols_store_combined_stamp());

    assert(diagnostics_event.type == IDE_EVENT_DIAGNOSTICS_UPDATED);
    assert(strcmp(diagnostics_event.payload.analysis.project_root, projectPath) == 0);
    assert(diagnostics_event.payload.analysis.data_stamp == analysis_store_combined_stamp());

    assert(symbol_event.sequence < diagnostics_event.sequence);
    assert(analysis_symbols_store_file_count() == 1);
    assert(analysis_store_file_count() == 1);

    analysis_store_clear();
    analysis_symbols_store_clear();
    analysis_token_store_clear();
    loop_events_shutdown();

    printf("fisics_bridge_events_regression_test: success\n");
    return 0;
}
