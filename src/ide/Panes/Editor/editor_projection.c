#include "ide/Panes/Editor/editor_projection.h"

#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "ide/Panes/ControlPanel/control_panel.h"

typedef struct {
    char** lines;
    int* realLines;
    int* realCols;
    int count;
    int cap;
    int droppedRows;
} ProjectionBuilder;

enum {
    PROJECTION_MAX_ROWS = 12000,
    PROJECTION_MAX_FUNCTION_LINES = 800
};

static bool text_contains_ci(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack || !haystack[0]) return false;

    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    if (nlen > hlen) return false;

    for (size_t i = 0; i + nlen <= hlen; ++i) {
        size_t j = 0;
        while (j < nlen) {
            unsigned char hc = (unsigned char)haystack[i + j];
            unsigned char nc = (unsigned char)needle[j];
            if (tolower(hc) != tolower(nc)) break;
            ++j;
        }
        if (j == nlen) return true;
    }
    return false;
}

static const char* kind_label(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION: return "fn";
        case FISICS_SYMBOL_STRUCT: return "struct";
        case FISICS_SYMBOL_UNION: return "union";
        case FISICS_SYMBOL_ENUM: return "enum";
        case FISICS_SYMBOL_TYPEDEF: return "typedef";
        case FISICS_SYMBOL_VARIABLE: return "var";
        case FISICS_SYMBOL_FIELD: return "field";
        case FISICS_SYMBOL_ENUM_MEMBER: return "member";
        case FISICS_SYMBOL_MACRO: return "macro";
        default: return "symbol";
    }
}

static int infer_block_end_line(const OpenFile* file, int startLine, int maxLineInclusive) {
    if (!file || !file->buffer || startLine < 0) return startLine;
    if (maxLineInclusive < startLine) maxLineInclusive = startLine;
    if (maxLineInclusive >= file->buffer->lineCount) {
        maxLineInclusive = file->buffer->lineCount - 1;
    }
    if (maxLineInclusive < startLine) return startLine;

    bool opened = false;
    int braceDepth = 0;
    for (int line = startLine; line <= maxLineInclusive; ++line) {
        const char* src = file->buffer->lines[line] ? file->buffer->lines[line] : "";
        bool inString = false;
        bool inChar = false;
        bool escaped = false;
        for (const char* p = src; *p; ++p) {
            char c = *p;
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (!inChar && c == '"' ) {
                inString = !inString;
                continue;
            }
            if (!inString && c == '\'') {
                inChar = !inChar;
                continue;
            }
            if (inString || inChar) continue;

            if (c == '{') {
                opened = true;
                braceDepth++;
            } else if (c == '}' && opened) {
                braceDepth--;
                if (braceDepth <= 0) {
                    return line;
                }
            }
        }
    }
    return startLine;
}

static bool symbol_kind_matches_mode(FisicsSymbolKind kind, SymbolFilterMode mode) {
    switch (mode) {
        case SYMBOL_FILTER_MODE_METHODS:
            return kind == FISICS_SYMBOL_FUNCTION;
        case SYMBOL_FILTER_MODE_TYPES:
            return kind == FISICS_SYMBOL_STRUCT ||
                   kind == FISICS_SYMBOL_UNION ||
                   kind == FISICS_SYMBOL_ENUM ||
                   kind == FISICS_SYMBOL_TYPEDEF;
        case SYMBOL_FILTER_MODE_TAGS:
            return true;
        case SYMBOL_FILTER_MODE_SYMBOLS:
        default:
            return true;
    }
}

static uint32_t symbol_kind_mask_for_symbol(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION:
            return SYMBOL_KIND_MASK_METHODS;
        case FISICS_SYMBOL_STRUCT:
        case FISICS_SYMBOL_UNION:
        case FISICS_SYMBOL_ENUM:
        case FISICS_SYMBOL_TYPEDEF:
            return SYMBOL_KIND_MASK_TYPES;
        case FISICS_SYMBOL_VARIABLE:
        case FISICS_SYMBOL_FIELD:
        case FISICS_SYMBOL_ENUM_MEMBER:
            return SYMBOL_KIND_MASK_VARS;
        default:
            return 0u;
    }
}

static ControlFilterButtonId projection_group_for_symbol(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION:
            return CONTROL_FILTER_BTN_MATCH_METHODS;
        case FISICS_SYMBOL_STRUCT:
        case FISICS_SYMBOL_UNION:
        case FISICS_SYMBOL_ENUM:
        case FISICS_SYMBOL_TYPEDEF:
            return CONTROL_FILTER_BTN_MATCH_TYPES;
        case FISICS_SYMBOL_VARIABLE:
        case FISICS_SYMBOL_FIELD:
        case FISICS_SYMBOL_ENUM_MEMBER:
            return CONTROL_FILTER_BTN_MATCH_VARS;
        default:
            return CONTROL_FILTER_BTN_MATCH_TAGS;
    }
}

static bool symbol_kind_matches_options(FisicsSymbolKind kind, const SymbolFilterOptions* options) {
    if (options && options->kind_mask != 0u) {
        if ((options->kind_mask & SYMBOL_KIND_MASK_TAGS) != 0u) {
            return true;
        }
        uint32_t symbolMask = symbol_kind_mask_for_symbol(kind);
        return (symbolMask & options->kind_mask) != 0u;
    }
    SymbolFilterMode mode = options ? options->mode : SYMBOL_FILTER_MODE_SYMBOLS;
    return symbol_kind_matches_mode(kind, mode);
}

static const AnalysisFileSymbols* find_symbols_for_path(const char* filePath) {
    if (!filePath) return NULL;
    size_t count = analysis_symbols_store_file_count();
    for (size_t i = 0; i < count; ++i) {
        const AnalysisFileSymbols* entry = analysis_symbols_store_file_at(i);
        if (entry && entry->path && strcmp(entry->path, filePath) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool symbol_matches_query(const FisicsSymbol* sym,
                                 const char* query,
                                 const SymbolFilterOptions* options) {
    if (!sym) return false;

    if (!symbol_kind_matches_options(sym->kind, options)) return false;

    bool matchName = options ? options->field_name : true;
    bool matchType = options ? options->field_type : true;
    bool matchParams = options ? options->field_params : true;
    bool matchKind = options ? options->field_kind : true;
    if (!matchName && !matchType && !matchParams && !matchKind) {
        matchName = true;
    }

    if (!query || !query[0]) return true;

    if (matchName && text_contains_ci(sym->name, query)) return true;
    if (matchKind && text_contains_ci(kind_label(sym->kind), query)) return true;
    if (matchType && (text_contains_ci(sym->return_type, query) ||
                      text_contains_ci(sym->parent_name, query))) {
        return true;
    }
    if (matchParams && sym->param_count > 0) {
        for (size_t i = 0; i < sym->param_count; ++i) {
            const char* pType = (sym->param_types && sym->param_types[i]) ? sym->param_types[i] : NULL;
            const char* pName = (sym->param_names && sym->param_names[i]) ? sym->param_names[i] : NULL;
            if (text_contains_ci(pType, query) || text_contains_ci(pName, query)) {
                return true;
            }
        }
    }

    return false;
}

static bool builder_push(ProjectionBuilder* b, const char* text, int realLine, int realCol) {
    if (!b) return false;
    if (b->count >= (PROJECTION_MAX_ROWS - 1)) {
        b->droppedRows++;
        return true;
    }
    if (b->count >= b->cap) {
        int newCap = (b->cap <= 0) ? 32 : (b->cap * 2);
        char** newLines = (char**)malloc(sizeof(char*) * (size_t)newCap);
        int* newRealLines = (int*)malloc(sizeof(int) * (size_t)newCap);
        int* newRealCols = (int*)malloc(sizeof(int) * (size_t)newCap);
        if (!newLines || !newRealLines || !newRealCols) {
            free(newLines);
            free(newRealLines);
            free(newRealCols);
            return false;
        }
        if (b->count > 0) {
            memcpy(newLines, b->lines, sizeof(char*) * (size_t)b->count);
            memcpy(newRealLines, b->realLines, sizeof(int) * (size_t)b->count);
            memcpy(newRealCols, b->realCols, sizeof(int) * (size_t)b->count);
        }
        free(b->lines);
        free(b->realLines);
        free(b->realCols);
        b->lines = newLines;
        b->realLines = newRealLines;
        b->realCols = newRealCols;
        b->cap = newCap;
    }

    const char* src = text ? text : "";
    b->lines[b->count] = strdup(src);
    if (!b->lines[b->count]) return false;
    b->realLines[b->count] = realLine;
    b->realCols[b->count] = realCol;
    b->count++;
    return true;
}

static void builder_free(ProjectionBuilder* b) {
    if (!b) return;
    for (int i = 0; i < b->count; ++i) {
        free(b->lines[i]);
    }
    free(b->lines);
    free(b->realLines);
    free(b->realCols);
    b->lines = NULL;
    b->realLines = NULL;
    b->realCols = NULL;
    b->count = 0;
    b->cap = 0;
}

static void mark_line(bool* flags, int lineCount, int line) {
    if (!flags || line < 0 || line >= lineCount) return;
    flags[line] = true;
}

static uint64_t compute_projection_stamp(const OpenFile* file,
                                         const char* query,
                                         const SymbolFilterOptions* options) {
    uint64_t stamp = file ? file->bufferVersion : 0;
    stamp ^= 0x9e3779b97f4a7c15ULL;

    analysis_symbols_store_lock();
    const AnalysisFileSymbols* symbols = file ? find_symbols_for_path(file->filePath) : NULL;
    if (symbols) {
        stamp ^= symbols->stamp;
        stamp ^= (uint64_t)symbols->count << 24;
    } else {
        stamp ^= 0x51ed270b4d31a6d3ULL;
    }
    analysis_symbols_store_unlock();

    const unsigned char* p = (const unsigned char*)(query ? query : "");
    while (*p) {
        unsigned char c = (unsigned char)tolower(*p++);
        stamp ^= (uint64_t)c + 0x9e3779b97f4a7c15ULL + (stamp << 6) + (stamp >> 2);
    }

    if (options) {
        stamp ^= ((uint64_t)options->mode << 32);
        stamp ^= ((uint64_t)options->kind_mask << 44);
        stamp ^= ((uint64_t)options->scope << 40);
        stamp ^= options->field_name ? (1ULL << 0) : 0;
        stamp ^= options->field_type ? (1ULL << 1) : 0;
        stamp ^= options->field_params ? (1ULL << 2) : 0;
        stamp ^= options->field_kind ? (1ULL << 3) : 0;
    }
    ControlFilterButtonId order[4] = {0};
    control_panel_get_match_button_order(order);
    for (int i = 0; i < 4; ++i) {
        stamp ^= ((uint64_t)order[i] << (uint64_t)(i * 8));
    }
    return stamp;
}

static bool build_symbol_projection(OpenFile* file,
                                    const char* query,
                                    const SymbolFilterOptions* options,
                                    ProjectionBuilder* out,
                                    bool* lineMatched) {
    if (!file || !file->buffer || !out) return false;

    analysis_symbols_store_lock();
    const AnalysisFileSymbols* symbols = find_symbols_for_path(file->filePath);
    if (!symbols || !symbols->symbols || symbols->count == 0) {
        analysis_symbols_store_unlock();
        return false;
    }

    int lineCount = file->buffer->lineCount;
    bool addedAny = false;

    ControlFilterButtonId order[4] = {0};
    control_panel_get_match_button_order(order);
    for (int orderIndex = 0; orderIndex < 4; ++orderIndex) {
        ControlFilterButtonId groupId = order[orderIndex];
        for (size_t i = 0; i < symbols->count; ++i) {
            const FisicsSymbol* sym = &symbols->symbols[i];
            if (!sym) continue;
            if (projection_group_for_symbol(sym->kind) != groupId) continue;
            if (!symbol_matches_query(sym, query, options)) continue;

            int realLine = sym->start_line > 0 ? sym->start_line - 1 : 0;
            if (realLine < 0) realLine = 0;
            if (realLine >= lineCount) realLine = lineCount - 1;
            int realCol = sym->start_col > 0 ? sym->start_col - 1 : 0;
            if (realCol < 0) realCol = 0;

            int endLine = sym->end_line > 0 ? sym->end_line - 1 : realLine;
            if (endLine < realLine) endLine = realLine;
            if (endLine >= lineCount) endLine = lineCount - 1;
            int rangeStart = realLine;
            int rangeEnd = realLine;
            if (sym->kind == FISICS_SYMBOL_FUNCTION) {
                int scanMax = realLine + PROJECTION_MAX_FUNCTION_LINES - 1;
                if (scanMax >= lineCount) scanMax = lineCount - 1;
                int inferredEnd = infer_block_end_line(file, realLine, scanMax);
                // Symbols loaded from cache/startup can under-report function end lines.
                // Always prefer the inferred closing-brace line when it extends the span.
                if (inferredEnd > endLine) endLine = inferredEnd;
                rangeStart = realLine;
                rangeEnd = endLine;
                int maxEnd = realLine + PROJECTION_MAX_FUNCTION_LINES - 1;
                if (rangeEnd > maxEnd) rangeEnd = maxEnd;
            } else {
                rangeStart = realLine;
                rangeEnd = realLine;
            }

            for (int line = rangeStart; line <= rangeEnd; ++line) {
                const char* srcLine = file->buffer->lines[line] ? file->buffer->lines[line] : "";
                if (!builder_push(out, srcLine, line, (line == realLine) ? realCol : 0)) {
                    analysis_symbols_store_unlock();
                    return false;
                }
                mark_line(lineMatched, lineCount, line);
            }

            if (sym->kind == FISICS_SYMBOL_FUNCTION && rangeEnd < endLine) {
                int skipped = endLine - rangeEnd;
                char truncated[96];
                snprintf(truncated, sizeof(truncated), "... +%d lines", skipped);
                if (!builder_push(out, truncated, rangeEnd, 0)) {
                    analysis_symbols_store_unlock();
                    return false;
                }
            }

            if (out->count > 0 && out->lines[out->count - 1] && out->lines[out->count - 1][0] != '\0') {
                // Separator row between projected symbol blocks: keep gutter blank.
                if (!builder_push(out, "", -1, -1)) {
                    analysis_symbols_store_unlock();
                    return false;
                }
            }
            addedAny = true;
        }
    }

    analysis_symbols_store_unlock();
    return addedAny;
}

static bool build_text_projection(OpenFile* file,
                                  const char* query,
                                  ProjectionBuilder* out,
                                  bool* lineMatched) {
    if (!file || !file->buffer || !query || !query[0] || !out) return false;

    bool addedAny = false;
    for (int line = 0; line < file->buffer->lineCount; ++line) {
        const char* src = file->buffer->lines[line] ? file->buffer->lines[line] : "";
        if (!text_contains_ci(src, query)) continue;
        if (lineMatched && lineMatched[line]) continue;

        if (!builder_push(out, src, line, 0)) return false;
        mark_line(lineMatched, file->buffer->lineCount, line);
        addedAny = true;
    }

    return addedAny;
}

void editor_projection_rebuild(OpenFile* file,
                               const char* query,
                               const SymbolFilterOptions* options) {
    if (!file || !file->buffer) return;

    uint64_t stamp = compute_projection_stamp(file, query, options);
    if (file->projection.buildStamp == stamp) return;

    editor_projection_free(&file->projection);

    ProjectionBuilder builder = {0};
    int lineCount = file->buffer->lineCount;
    bool* lineMatched = NULL;
    if (lineCount > 0) {
        lineMatched = (bool*)calloc((size_t)lineCount, sizeof(bool));
        if (!lineMatched) return;
    }

    bool hasRows = false;
    hasRows |= build_symbol_projection(file, query, options, &builder, lineMatched);
    bool allowTextProjection = true;
    if (options && options->kind_mask != 0u) {
        allowTextProjection = false;
    } else if (options && options->mode != SYMBOL_FILTER_MODE_SYMBOLS) {
        allowTextProjection = false;
    }
    if (allowTextProjection) {
        hasRows |= build_text_projection(file, query, &builder, lineMatched);
    }

    if (!hasRows) {
        char empty[320];
        snprintf(empty, sizeof(empty), "No matches for \"%s\"", (query && query[0]) ? query : "");
        if (!builder_push(&builder, empty, -1, -1)) {
            free(lineMatched);
            builder_free(&builder);
            return;
        }
    }

    if (builder.droppedRows > 0) {
        char summary[96];
        snprintf(summary, sizeof(summary), "... +%d more projected rows", builder.droppedRows);
        if (!builder_push(&builder, summary, -1, -1)) {
            free(lineMatched);
            builder_free(&builder);
            return;
        }
    }

    int matchCount = 0;
    if (lineMatched) {
        for (int i = 0; i < lineCount; ++i) {
            if (lineMatched[i]) matchCount++;
        }
    }

    int* realMatchLines = NULL;
    if (matchCount > 0) {
        realMatchLines = (int*)malloc(sizeof(int) * (size_t)matchCount);
        if (!realMatchLines) {
            free(lineMatched);
            builder_free(&builder);
            return;
        }
        int at = 0;
        for (int i = 0; i < lineCount; ++i) {
            if (lineMatched[i]) realMatchLines[at++] = i;
        }
    }
    free(lineMatched);

    file->projection.lines = builder.lines;
    file->projection.lineCount = builder.count;
    file->projection.projectedToRealLine = builder.realLines;
    file->projection.projectedToRealCol = builder.realCols;
    file->projection.realMatchLines = realMatchLines;
    file->projection.realMatchCount = matchCount;
    file->projection.buildStamp = stamp;
}
