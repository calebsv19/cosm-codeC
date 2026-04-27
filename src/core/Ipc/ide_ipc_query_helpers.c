#include "core/Ipc/ide_ipc_query_helpers.h"

#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/library_index.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Diagnostics/diagnostics_engine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    DIAG_TAXONOMY_INFO = 0,
    DIAG_TAXONOMY_WARNING = 1,
    DIAG_TAXONOMY_ERROR = 2
} DiagTaxonomySeverity;

static const char* symbol_kind_name(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION: return "function";
        case FISICS_SYMBOL_STRUCT: return "struct";
        case FISICS_SYMBOL_UNION: return "union";
        case FISICS_SYMBOL_ENUM: return "enum";
        case FISICS_SYMBOL_TYPEDEF: return "typedef";
        case FISICS_SYMBOL_VARIABLE: return "variable";
        case FISICS_SYMBOL_FIELD: return "field";
        case FISICS_SYMBOL_ENUM_MEMBER: return "enum_member";
        case FISICS_SYMBOL_MACRO: return "macro";
        default: return "unknown";
    }
}

static const char* token_kind_name(FisicsTokenKind kind) {
    switch (kind) {
        case FISICS_TOK_IDENTIFIER: return "identifier";
        case FISICS_TOK_KEYWORD: return "keyword";
        case FISICS_TOK_NUMBER: return "number";
        case FISICS_TOK_STRING: return "string";
        case FISICS_TOK_CHAR: return "char";
        case FISICS_TOK_OPERATOR: return "operator";
        case FISICS_TOK_PUNCT: return "punct";
        case FISICS_TOK_COMMENT: return "comment";
        case FISICS_TOK_WHITESPACE: return "whitespace";
        default: return "unknown";
    }
}

static void symbol_stable_id_hex(uint64_t value, char out[19]) {
    if (!out) return;
    snprintf(out, 19, "0x%016llx", (unsigned long long)value);
}

static const char* normalize_or_project_path(const char* input,
                                             const char* project_root,
                                             char* out,
                                             size_t out_cap) {
    if (!input || !*input || !out || out_cap == 0) return "";
    if (input[0] == '/') {
        snprintf(out, out_cap, "%s", input);
    } else if (project_root && *project_root) {
        snprintf(out, out_cap, "%s/%s", project_root, input);
    } else {
        snprintf(out, out_cap, "%s", input);
    }
    return out;
}

static bool symbol_is_top_level(const FisicsSymbol* sym) {
    if (!sym) return false;
    return (!sym->parent_name || !sym->parent_name[0]);
}

static const char* diag_severity_legacy_name(DiagTaxonomySeverity sev) {
    switch (sev) {
        case DIAG_TAXONOMY_ERROR: return "error";
        case DIAG_TAXONOMY_WARNING: return "warn";
        default: return "info";
    }
}

static const char* diag_severity_normalized_name(DiagTaxonomySeverity sev) {
    switch (sev) {
        case DIAG_TAXONOMY_ERROR: return "error";
        case DIAG_TAXONOMY_WARNING: return "warning";
        default: return "info";
    }
}

static void add_diag_taxonomy_fields(json_object* d,
                                     DiagTaxonomySeverity sev,
                                     const char* category,
                                     const char* code_text,
                                     int code_id) {
    if (!d) return;
    const char* safe_category = (category && category[0]) ? category : "unknown";
    const char* safe_code = (code_text && code_text[0]) ? code_text : "";
    json_object_object_add(d, "severity", json_object_new_string(diag_severity_legacy_name(sev)));
    json_object_object_add(d, "severity_name", json_object_new_string(diag_severity_normalized_name(sev)));
    json_object_object_add(d, "severity_id", json_object_new_int((int)sev));
    json_object_object_add(d, "category", json_object_new_string(safe_category));
    json_object_object_add(d, "code", json_object_new_string(safe_code));
    json_object_object_add(d, "code_id", json_object_new_int(code_id));
}

json_object* ide_ipc_build_diag_result(json_object* args, const char* project_root) {
    (void)project_root;
    int max_items = -1;
    if (args) {
        json_object* jmax = NULL;
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax &&
            json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 0) max_items = -1;
        }
    }

    json_object* result = json_object_new_object();
    json_object* summary = json_object_new_object();
    json_object* arr = json_object_new_array();

    int total = 0;
    int err = 0;
    int warn = 0;
    int info = 0;
    int returned = 0;

    size_t build_count = 0;
    const BuildDiagnostic* build = build_diagnostics_get(&build_count);
    for (size_t i = 0; i < build_count; ++i) {
        total++;
        if (build[i].isError) err++;
        else warn++;
        if (max_items >= 0 && returned >= max_items) continue;
        json_object* d = json_object_new_object();
        json_object_object_add(d, "file", json_object_new_string(build[i].path));
        json_object_object_add(d, "line", json_object_new_int(build[i].line));
        json_object_object_add(d, "col", json_object_new_int(build[i].col));
        json_object_object_add(d, "endLine", json_object_new_int(build[i].line));
        json_object_object_add(d, "endCol", json_object_new_int(build[i].col + 1));
        json_object_object_add(d, "message", json_object_new_string(build[i].message));
        add_diag_taxonomy_fields(d,
                                 build[i].isError ? DIAG_TAXONOMY_ERROR : DIAG_TAXONOMY_WARNING,
                                 "build",
                                 "",
                                 0);
        json_object_object_add(d, "source", json_object_new_string("build"));
        json_object_array_add(arr, d);
        returned++;
    }

    int diag_count = getDiagnosticCount();
    for (int i = 0; i < diag_count; ++i) {
        const Diagnostic* dg = getDiagnosticAt(i);
        if (!dg) continue;
        total++;
        if (dg->severity == DIAG_SEVERITY_ERROR) err++;
        else if (dg->severity == DIAG_SEVERITY_WARNING) warn++;
        else info++;

        if (max_items >= 0 && returned >= max_items) continue;
        DiagTaxonomySeverity sev = DIAG_TAXONOMY_INFO;
        if (dg->severity == DIAG_SEVERITY_ERROR) sev = DIAG_TAXONOMY_ERROR;
        else if (dg->severity == DIAG_SEVERITY_WARNING) sev = DIAG_TAXONOMY_WARNING;
        json_object* d = json_object_new_object();
        json_object_object_add(d, "file", json_object_new_string(dg->filePath ? dg->filePath : ""));
        json_object_object_add(d, "line", json_object_new_int(dg->line));
        json_object_object_add(d, "col", json_object_new_int(dg->column));
        json_object_object_add(d, "endLine", json_object_new_int(dg->line));
        json_object_object_add(d, "endCol", json_object_new_int(dg->column + 1));
        json_object_object_add(d, "message", json_object_new_string(dg->message ? dg->message : ""));
        char code_text[32];
        snprintf(code_text, sizeof(code_text), "%d", dg->codeId);
        add_diag_taxonomy_fields(d,
                                 sev,
                                 diagnostic_category_name(dg->category),
                                 dg->codeId ? code_text : "",
                                 dg->codeId);
        json_object_object_add(d, "source", json_object_new_string("analysis"));
        json_object_array_add(arr, d);
        returned++;
    }

    json_object_object_add(summary, "total", json_object_new_int(total));
    json_object_object_add(summary, "error", json_object_new_int(err));
    json_object_object_add(summary, "warn", json_object_new_int(warn));
    json_object_object_add(summary, "info", json_object_new_int(info));

    json_object_object_add(result, "summary", summary);
    json_object_object_add(result, "diagnostics", arr);
    json_object_object_add(result, "total_count", json_object_new_int(total));
    json_object_object_add(result, "returned_count", json_object_new_int(returned));
    json_object_object_add(result, "truncated", json_object_new_boolean(max_items >= 0 && returned < total));
    return result;
}

json_object* ide_ipc_build_symbol_result(json_object* args, const char* project_root) {
    int max_items = -1;
    bool top_only = false;
    char file_filter[1024] = {0};
    if (args) {
        json_object* jmax = NULL;
        json_object* jtop = NULL;
        json_object* jfile = NULL;
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax && json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 0) max_items = -1;
        }
        if (json_object_object_get_ex(args, "top_level_only", &jtop) &&
            jtop && json_object_is_type(jtop, json_type_boolean)) {
            top_only = json_object_get_boolean(jtop);
        }
        if (json_object_object_get_ex(args, "file", &jfile) &&
            jfile && json_object_is_type(jfile, json_type_string)) {
            normalize_or_project_path(json_object_get_string(jfile), project_root, file_filter, sizeof(file_filter));
        }
    }

    json_object* result = json_object_new_object();
    json_object* arr = json_object_new_array();

    int total = 0;
    int returned = 0;
    analysis_symbols_store_lock();
    size_t file_count = analysis_symbols_store_file_count();
    for (size_t fi = 0; fi < file_count; ++fi) {
        const AnalysisFileSymbols* file_entry = analysis_symbols_store_file_at(fi);
        if (!file_entry) continue;
        if (file_filter[0]) {
            const char* path = file_entry->path ? file_entry->path : "";
            if (strcmp(path, file_filter) != 0) continue;
        }
        for (size_t si = 0; si < file_entry->count; ++si) {
            const FisicsSymbol* sym = &file_entry->symbols[si];
            if (top_only && !symbol_is_top_level(sym)) continue;
            total++;
            if (max_items >= 0 && returned >= max_items) continue;

            json_object* s = json_object_new_object();
            json_object_object_add(s, "kind", json_object_new_string(symbol_kind_name(sym->kind)));
            json_object_object_add(s, "kind_id", json_object_new_int((int)sym->kind));
            json_object_object_add(s, "name", json_object_new_string(sym->name ? sym->name : ""));
            char stable_id_hex[19];
            symbol_stable_id_hex(sym->stable_id, stable_id_hex);
            json_object_object_add(s, "stable_id", json_object_new_string(stable_id_hex));
            char parent_stable_id_hex[19];
            symbol_stable_id_hex(sym->parent_stable_id, parent_stable_id_hex);
            json_object_object_add(s, "parent_stable_id", json_object_new_string(parent_stable_id_hex));
            json_object_object_add(s, "file", json_object_new_string(sym->file_path ? sym->file_path :
                                                                       (file_entry->path ? file_entry->path : "")));
            json_object_object_add(s, "start_line", json_object_new_int(sym->start_line));
            json_object_object_add(s, "start_col", json_object_new_int(sym->start_col));
            json_object_object_add(s, "end_line", json_object_new_int(sym->end_line));
            json_object_object_add(s, "end_col", json_object_new_int(sym->end_col));
            json_object_object_add(s, "return_type", json_object_new_string(sym->return_type ? sym->return_type : ""));
            json_object_object_add(s, "owner", json_object_new_string(sym->parent_name ? sym->parent_name : ""));
            json_object_object_add(s, "parent_kind", json_object_new_string(symbol_kind_name(sym->parent_kind)));
            json_object_object_add(s, "is_definition", json_object_new_boolean(sym->is_definition));
            json_object_object_add(s, "is_variadic", json_object_new_boolean(sym->is_variadic));

            char signature[1024];
            signature[0] = '\0';
            if (sym->return_type && sym->return_type[0]) {
                snprintf(signature, sizeof(signature), "%s %s(", sym->return_type, sym->name ? sym->name : "");
            } else {
                snprintf(signature, sizeof(signature), "%s(", sym->name ? sym->name : "");
            }
            for (size_t pi = 0; pi < sym->param_count; ++pi) {
                const char* ptype = (sym->param_types && sym->param_types[pi]) ? sym->param_types[pi] : "";
                const char* pname = (sym->param_names && sym->param_names[pi]) ? sym->param_names[pi] : "";
                char piece[128];
                if (pi > 0) strncat(signature, ", ", sizeof(signature) - strlen(signature) - 1);
                if (ptype[0] && pname[0]) snprintf(piece, sizeof(piece), "%s %s", ptype, pname);
                else if (ptype[0]) snprintf(piece, sizeof(piece), "%s", ptype);
                else snprintf(piece, sizeof(piece), "%s", pname);
                strncat(signature, piece, sizeof(signature) - strlen(signature) - 1);
            }
            strncat(signature, ")", sizeof(signature) - strlen(signature) - 1);
            json_object_object_add(s, "signature", json_object_new_string(signature));

            json_object_array_add(arr, s);
            returned++;
        }
    }
    analysis_symbols_store_unlock();

    json_object_object_add(result, "symbols", arr);
    json_object_object_add(result, "total_count", json_object_new_int(total));
    json_object_object_add(result, "returned_count", json_object_new_int(returned));
    json_object_object_add(result, "truncated", json_object_new_boolean(max_items >= 0 && returned < total));
    return result;
}

json_object* ide_ipc_build_token_result(json_object* args, const char* project_root) {
    int max_items = -1;
    char file_filter[1024] = {0};
    if (args) {
        json_object* jmax = NULL;
        json_object* jfile = NULL;
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax && json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 0) max_items = -1;
        }
        if (json_object_object_get_ex(args, "file", &jfile) &&
            jfile && json_object_is_type(jfile, json_type_string)) {
            normalize_or_project_path(json_object_get_string(jfile), project_root, file_filter, sizeof(file_filter));
        }
    }

    json_object* result = json_object_new_object();
    json_object* arr = json_object_new_array();

    int total = 0;
    int returned = 0;
    size_t file_count = analysis_token_store_file_count();
    for (size_t fi = 0; fi < file_count; ++fi) {
        const AnalysisFileTokens* file_entry = analysis_token_store_file_at(fi);
        if (!file_entry) continue;
        const char* file_path = file_entry->path ? file_entry->path : "";
        if (file_filter[0] && strcmp(file_path, file_filter) != 0) continue;
        for (size_t ti = 0; ti < file_entry->count; ++ti) {
            const FisicsTokenSpan* tok = &file_entry->spans[ti];
            total++;
            if (max_items >= 0 && returned >= max_items) continue;

            json_object* t = json_object_new_object();
            json_object_object_add(t, "file", json_object_new_string(file_path));
            json_object_object_add(t, "line", json_object_new_int(tok->line));
            json_object_object_add(t, "column", json_object_new_int(tok->column));
            json_object_object_add(t, "length", json_object_new_int(tok->length));
            json_object_object_add(t, "kind", json_object_new_string(token_kind_name(tok->kind)));
            json_object_object_add(t, "kind_id", json_object_new_int((int)tok->kind));
            json_object_array_add(arr, t);
            returned++;
        }
    }

    json_object_object_add(result, "tokens", arr);
    json_object_object_add(result, "total_count", json_object_new_int(total));
    json_object_object_add(result, "returned_count", json_object_new_int(returned));
    json_object_object_add(result, "truncated", json_object_new_boolean(max_items >= 0 && returned < total));
    return result;
}

static const char* bucket_kind_name(LibraryBucketKind kind) {
    switch (kind) {
        case LIB_BUCKET_PROJECT: return "project";
        case LIB_BUCKET_SYSTEM: return "system";
        case LIB_BUCKET_EXTERNAL: return "external";
        case LIB_BUCKET_UNRESOLVED: return "unresolved";
        default: return "unknown";
    }
}

json_object* ide_ipc_build_includes_result(json_object* args) {
    bool want_graph = false;
    if (args) {
        json_object* jgraph = NULL;
        if (json_object_object_get_ex(args, "graph", &jgraph) &&
            jgraph && json_object_is_type(jgraph, json_type_boolean)) {
            want_graph = json_object_get_boolean(jgraph);
        }
    }

    json_object* result = json_object_new_object();
    json_object* buckets_arr = json_object_new_array();
    json_object* summary = json_object_new_object();
    json_object* edges = want_graph ? json_object_new_array() : NULL;

    int total_headers = 0;
    int total_usages = 0;
    int bucket_headers[LIB_BUCKET_COUNT] = {0};

    library_index_lock();
    size_t bucket_count = library_index_bucket_count();
    for (size_t bi = 0; bi < bucket_count; ++bi) {
        const LibraryBucket* bucket = library_index_get_bucket(bi);
        if (!bucket) continue;

        json_object* jb = json_object_new_object();
        json_object_object_add(jb, "kind", json_object_new_string(bucket_kind_name(bucket->kind)));
        json_object_object_add(jb, "kind_id", json_object_new_int((int)bucket->kind));

        json_object* headers_arr = json_object_new_array();
        size_t header_count = library_index_header_count(bucket);
        for (size_t hi = 0; hi < header_count; ++hi) {
            const LibraryHeader* header = library_index_get_header(bucket, hi);
            if (!header) continue;
            total_headers++;
            if (bucket->kind >= 0 && bucket->kind < LIB_BUCKET_COUNT) {
                bucket_headers[bucket->kind]++;
            }

            json_object* jh = json_object_new_object();
            json_object_object_add(jh, "name", json_object_new_string(header->name ? header->name : ""));
            json_object_object_add(jh, "resolved_path", json_object_new_string(header->resolved_path ? header->resolved_path : ""));
            json_object_object_add(jh,
                                   "include_kind",
                                   json_object_new_string(header->kind == LIB_INCLUDE_KIND_SYSTEM ? "system" : "local"));
            json_object_object_add(jh, "origin", json_object_new_string(bucket_kind_name(header->origin)));
            json_object_object_add(jh, "origin_id", json_object_new_int((int)header->origin));

            json_object* usages_arr = json_object_new_array();
            size_t usage_count = library_index_usage_count(header);
            json_object_object_add(jh, "usage_count", json_object_new_int((int)usage_count));
            for (size_t ui = 0; ui < usage_count; ++ui) {
                const LibraryUsage* usage = library_index_get_usage(header, ui);
                if (!usage) continue;
                total_usages++;
                json_object* ju = json_object_new_object();
                json_object_object_add(ju, "source", json_object_new_string(usage->source_path ? usage->source_path : ""));
                json_object_object_add(ju, "line", json_object_new_int(usage->line));
                json_object_object_add(ju, "col", json_object_new_int(usage->column));
                json_object_array_add(usages_arr, ju);

                if (want_graph && edges) {
                    json_object* edge = json_object_new_object();
                    json_object_object_add(edge, "source", json_object_new_string(usage->source_path ? usage->source_path : ""));
                    json_object_object_add(edge, "target", json_object_new_string(header->name ? header->name : ""));
                    json_object_object_add(edge, "resolved_path", json_object_new_string(header->resolved_path ? header->resolved_path : ""));
                    json_object_object_add(edge, "line", json_object_new_int(usage->line));
                    json_object_object_add(edge, "col", json_object_new_int(usage->column));
                    json_object_object_add(edge, "bucket", json_object_new_string(bucket_kind_name(bucket->kind)));
                    json_object_array_add(edges, edge);
                }
            }

            json_object_object_add(jh, "usages", usages_arr);
            json_object_array_add(headers_arr, jh);
        }

        json_object_object_add(jb, "headers", headers_arr);
        json_object_object_add(jb, "header_count", json_object_new_int((int)header_count));
        json_object_array_add(buckets_arr, jb);
    }
    library_index_unlock();

    json_object_object_add(summary, "headers_total", json_object_new_int(total_headers));
    json_object_object_add(summary, "usages_total", json_object_new_int(total_usages));
    json_object_object_add(summary, "project_headers", json_object_new_int(bucket_headers[LIB_BUCKET_PROJECT]));
    json_object_object_add(summary, "system_headers", json_object_new_int(bucket_headers[LIB_BUCKET_SYSTEM]));
    json_object_object_add(summary, "external_headers", json_object_new_int(bucket_headers[LIB_BUCKET_EXTERNAL]));
    json_object_object_add(summary, "unresolved_headers", json_object_new_int(bucket_headers[LIB_BUCKET_UNRESOLVED]));

    json_object_object_add(result, "summary", summary);
    json_object_object_add(result, "buckets", buckets_arr);
    json_object_object_add(result, "graph", json_object_new_boolean(want_graph));
    if (want_graph && edges) {
        json_object_object_add(result, "edges", edges);
        json_object_object_add(result, "edge_count", json_object_new_int((int)json_object_array_length(edges)));
    }
    return result;
}
