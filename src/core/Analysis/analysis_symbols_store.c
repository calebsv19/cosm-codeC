#include "core/Analysis/analysis_symbols_store.h"

#include <json-c/json.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>

static AnalysisFileSymbols* g_files = NULL;
static size_t g_file_count = 0;
static size_t g_file_cap = 0;
static uint64_t g_stamp_counter = 0;
static SDL_mutex* g_symbols_mutex = NULL;

static void ensure_symbols_mutex(void) {
    if (!g_symbols_mutex) {
        g_symbols_mutex = SDL_CreateMutex();
    }
}

void analysis_symbols_store_lock(void) {
    ensure_symbols_mutex();
    if (g_symbols_mutex) SDL_LockMutex(g_symbols_mutex);
}

void analysis_symbols_store_unlock(void) {
    if (g_symbols_mutex) SDL_UnlockMutex(g_symbols_mutex);
}

static void free_symbol_entry(AnalysisFileSymbols* f) {
    if (!f) return;
    free(f->path);
    if (f->symbols) {
        for (size_t i = 0; i < f->count; ++i) {
            free((char*)f->symbols[i].name);
            free((char*)f->symbols[i].file_path);
            free((char*)f->symbols[i].parent_name);
            free((char*)f->symbols[i].return_type);
            if (f->symbols[i].param_types) {
                for (size_t p = 0; p < f->symbols[i].param_count; ++p) {
                    free((char*)f->symbols[i].param_types[p]);
                }
                free(f->symbols[i].param_types);
            }
            if (f->symbols[i].param_names) {
                for (size_t p = 0; p < f->symbols[i].param_count; ++p) {
                    free((char*)f->symbols[i].param_names[p]);
                }
                free(f->symbols[i].param_names);
            }
        }
        free(f->symbols);
    }
    f->path = NULL;
    f->symbols = NULL;
    f->count = 0;
    f->stamp = 0;
}

void analysis_symbols_store_clear(void) {
    analysis_symbols_store_lock();
    for (size_t i = 0; i < g_file_count; ++i) {
        free_symbol_entry(&g_files[i]);
    }
    free(g_files);
    g_files = NULL;
    g_file_count = 0;
    g_file_cap = 0;
    g_stamp_counter = 0;
    analysis_symbols_store_unlock();
}

static char* dup_str(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* out = malloc(len);
    if (!out) return NULL;
    memcpy(out, s, len);
    return out;
}

static bool clone_symbol(FisicsSymbol* dst, const FisicsSymbol* src) {
    if (!dst || !src) return false;
    *dst = *src;
    dst->name = dup_str(src->name);
    dst->file_path = dup_str(src->file_path);
    dst->parent_name = dup_str(src->parent_name);
    dst->return_type = dup_str(src->return_type);

    dst->param_types = NULL;
    dst->param_names = NULL;
    if (src->param_count > 0) {
        if (src->param_types) {
            dst->param_types = (const char**)calloc(src->param_count, sizeof(char*));
            if (!dst->param_types) return false;
            for (size_t i = 0; i < src->param_count; ++i) {
                if (src->param_types[i]) {
                    ((char**)dst->param_types)[i] = dup_str(src->param_types[i]);
                    if (!dst->param_types[i]) return false;
                }
            }
        }
        if (src->param_names) {
            dst->param_names = (const char**)calloc(src->param_count, sizeof(char*));
            if (!dst->param_names) return false;
            for (size_t i = 0; i < src->param_count; ++i) {
                if (src->param_names[i]) {
                    ((char**)dst->param_names)[i] = dup_str(src->param_names[i]);
                    if (!dst->param_names[i]) return false;
                }
            }
        }
    }
    return true;
}

void analysis_symbols_store_upsert(const char* filePath,
                                   const FisicsSymbol* symbols,
                                   size_t symbolCount) {
    if (!filePath) return;
    analysis_symbols_store_lock();

    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_symbol_entry(&g_files[existing]);
        for (size_t j = existing + 1; j < g_file_count; ++j) {
            g_files[j - 1] = g_files[j];
        }
        g_file_count--;
    }

    if (g_file_count >= g_file_cap) {
        size_t newCap = g_file_cap ? g_file_cap * 2 : 8;
        AnalysisFileSymbols* tmp = realloc(g_files, newCap * sizeof(AnalysisFileSymbols));
        if (!tmp) {
            analysis_symbols_store_unlock();
            return;
        }
        g_files = tmp;
        g_file_cap = newCap;
    }

    AnalysisFileSymbols entry = {0};
    entry.path = dup_str(filePath);
    entry.count = symbolCount;
    entry.stamp = ++g_stamp_counter;

    if (symbolCount > 0 && symbols) {
        entry.symbols = (FisicsSymbol*)calloc(symbolCount, sizeof(FisicsSymbol));
        if (!entry.symbols) {
            free(entry.path);
            analysis_symbols_store_unlock();
            return;
        }
        for (size_t i = 0; i < symbolCount; ++i) {
            if (!clone_symbol(&entry.symbols[i], &symbols[i])) {
                entry.count = i + 1;
                free_symbol_entry(&entry);
                analysis_symbols_store_unlock();
                return;
            }
        }
    }

    for (size_t j = g_file_count; j > 0; --j) {
        g_files[j] = g_files[j - 1];
    }
    g_files[0] = entry;
    g_file_count++;
    analysis_symbols_store_unlock();
}

void analysis_symbols_store_remove(const char* filePath) {
    if (!filePath) return;
    analysis_symbols_store_lock();
    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing == (size_t)-1) {
        analysis_symbols_store_unlock();
        return;
    }
    free_symbol_entry(&g_files[existing]);
    for (size_t j = existing + 1; j < g_file_count; ++j) {
        g_files[j - 1] = g_files[j];
    }
    g_file_count--;
    analysis_symbols_store_unlock();
}

size_t analysis_symbols_store_file_count(void) {
    return g_file_count;
}

const AnalysisFileSymbols* analysis_symbols_store_file_at(size_t idx) {
    if (idx >= g_file_count) return NULL;
    return &g_files[idx];
}

uint64_t analysis_symbols_store_combined_stamp(void) {
    analysis_symbols_store_lock();
    uint64_t stamp = (uint64_t)g_file_count;
    for (size_t i = 0; i < g_file_count; ++i) {
        stamp ^= g_files[i].stamp;
    }
    analysis_symbols_store_unlock();
    return stamp;
}

static void ensure_cache_dir(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspaceRoot);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

static json_object* json_string_or_empty(const char* s) {
    return json_object_new_string(s ? s : "");
}

void analysis_symbols_store_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    analysis_symbols_store_lock();
    ensure_cache_dir(workspaceRoot);
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_symbols.json", workspaceRoot);

    json_object* arr = json_object_new_array();
    for (size_t i = 0; i < g_file_count; ++i) {
        AnalysisFileSymbols* f = &g_files[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "path", json_string_or_empty(f->path));
        json_object_object_add(obj, "stamp", json_object_new_int64((long long)f->stamp));

        json_object* syms = json_object_new_array();
        for (size_t s = 0; s < f->count; ++s) {
            const FisicsSymbol* sym = &f->symbols[s];
            json_object* js = json_object_new_object();
            json_object_object_add(js, "name", json_string_or_empty(sym->name));
            json_object_object_add(js, "file_path", json_string_or_empty(sym->file_path));
            json_object_object_add(js, "parent_name", json_string_or_empty(sym->parent_name));
            json_object_object_add(js, "start_line", json_object_new_int(sym->start_line));
            json_object_object_add(js, "start_col", json_object_new_int(sym->start_col));
            json_object_object_add(js, "end_line", json_object_new_int(sym->end_line));
            json_object_object_add(js, "end_col", json_object_new_int(sym->end_col));
            json_object_object_add(js, "kind", json_object_new_int(sym->kind));
            json_object_object_add(js, "parent_kind", json_object_new_int(sym->parent_kind));
            json_object_object_add(js, "is_definition", json_object_new_int(sym->is_definition ? 1 : 0));
            json_object_object_add(js, "is_variadic", json_object_new_int(sym->is_variadic ? 1 : 0));
            json_object_object_add(js, "return_type", json_string_or_empty(sym->return_type));

            json_object* ptypes = json_object_new_array();
            json_object* pnames = json_object_new_array();
            for (size_t p = 0; p < sym->param_count; ++p) {
                const char* pt = sym->param_types ? sym->param_types[p] : NULL;
                const char* pn = sym->param_names ? sym->param_names[p] : NULL;
                json_object_array_add(ptypes, json_string_or_empty(pt));
                json_object_array_add(pnames, json_string_or_empty(pn));
            }
            json_object_object_add(js, "param_types", ptypes);
            json_object_object_add(js, "param_names", pnames);
            json_object_object_add(js, "param_count", json_object_new_int((int)sym->param_count));

            json_object_array_add(syms, js);
        }
        json_object_object_add(obj, "symbols", syms);
        json_object_array_add(arr, obj);
    }

    const char* serialized = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    json_object_put(arr);
    analysis_symbols_store_unlock();
}

void analysis_symbols_store_load(const char* workspaceRoot) {
    analysis_symbols_store_clear();
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_symbols.json", workspaceRoot);
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        return;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return;
    }
    size_t arrLen = json_object_array_length(root);
    for (size_t i = 0; i < arrLen; ++i) {
        json_object* obj = json_object_array_get_idx(root, i);
        if (!obj) continue;
        json_object* jpath = NULL;
        json_object* jstamp = NULL;
        json_object* jsyms = NULL;
        if (!json_object_object_get_ex(obj, "path", &jpath) ||
            !json_object_object_get_ex(obj, "symbols", &jsyms)) {
            continue;
        }
        const char* pathStr = json_object_get_string(jpath);
        size_t scount = json_object_array_length(jsyms);
        FisicsSymbol* tmp = calloc(scount, sizeof(FisicsSymbol));
        if (!tmp) continue;
        for (size_t s = 0; s < scount; ++s) {
            json_object* js = json_object_array_get_idx(jsyms, s);
            if (!js) continue;
            json_object* jname=NULL,* jfile=NULL,* jparent=NULL,* jret=NULL;
            json_object* jsl=NULL,* jsc=NULL,* jel=NULL,* jec=NULL;
            json_object* jkind=NULL,* jparentKind=NULL,* jdef=NULL,* jvar=NULL;
            json_object* jptypes=NULL,* jpnames=NULL,* jpcount=NULL;
            json_object_object_get_ex(js, "name", &jname);
            json_object_object_get_ex(js, "file_path", &jfile);
            json_object_object_get_ex(js, "parent_name", &jparent);
            json_object_object_get_ex(js, "return_type", &jret);
            json_object_object_get_ex(js, "start_line", &jsl);
            json_object_object_get_ex(js, "start_col", &jsc);
            json_object_object_get_ex(js, "end_line", &jel);
            json_object_object_get_ex(js, "end_col", &jec);
            json_object_object_get_ex(js, "kind", &jkind);
            json_object_object_get_ex(js, "parent_kind", &jparentKind);
            json_object_object_get_ex(js, "is_definition", &jdef);
            json_object_object_get_ex(js, "is_variadic", &jvar);
            json_object_object_get_ex(js, "param_types", &jptypes);
            json_object_object_get_ex(js, "param_names", &jpnames);
            json_object_object_get_ex(js, "param_count", &jpcount);

            tmp[s].name = dup_str(jname ? json_object_get_string(jname) : NULL);
            tmp[s].file_path = dup_str(jfile ? json_object_get_string(jfile) : NULL);
            tmp[s].parent_name = dup_str(jparent ? json_object_get_string(jparent) : NULL);
            tmp[s].return_type = dup_str(jret ? json_object_get_string(jret) : NULL);
            tmp[s].start_line = jsl ? json_object_get_int(jsl) : 0;
            tmp[s].start_col = jsc ? json_object_get_int(jsc) : 0;
            tmp[s].end_line = jel ? json_object_get_int(jel) : 0;
            tmp[s].end_col = jec ? json_object_get_int(jec) : 0;
            tmp[s].kind = jkind ? (FisicsSymbolKind)json_object_get_int(jkind) : FISICS_SYMBOL_UNKNOWN;
            tmp[s].parent_kind = jparentKind ? (FisicsSymbolKind)json_object_get_int(jparentKind) : FISICS_SYMBOL_UNKNOWN;
            tmp[s].is_definition = jdef ? (json_object_get_int(jdef) != 0) : false;
            tmp[s].is_variadic = jvar ? (json_object_get_int(jvar) != 0) : false;
            tmp[s].param_count = jpcount ? (size_t)json_object_get_int(jpcount) : 0;

            size_t pcount = tmp[s].param_count;
            if (pcount > 0 && jptypes && json_object_is_type(jptypes, json_type_array)) {
                tmp[s].param_types = (const char**)calloc(pcount, sizeof(char*));
                size_t arr = json_object_array_length(jptypes);
                size_t limit = arr < pcount ? arr : pcount;
                for (size_t p = 0; p < limit; ++p) {
                    json_object* jt = json_object_array_get_idx(jptypes, p);
                    const char* val = jt ? json_object_get_string(jt) : NULL;
                    tmp[s].param_types[p] = dup_str(val);
                }
            }
            if (pcount > 0 && jpnames && json_object_is_type(jpnames, json_type_array)) {
                tmp[s].param_names = (const char**)calloc(pcount, sizeof(char*));
                size_t arr = json_object_array_length(jpnames);
                size_t limit = arr < pcount ? arr : pcount;
                for (size_t p = 0; p < limit; ++p) {
                    json_object* jn = json_object_array_get_idx(jpnames, p);
                    const char* val = jn ? json_object_get_string(jn) : NULL;
                    tmp[s].param_names[p] = dup_str(val);
                }
            }
        }
        analysis_symbols_store_upsert(pathStr, tmp, scount);
        for (size_t s = 0; s < scount; ++s) {
            free((char*)tmp[s].name);
            free((char*)tmp[s].file_path);
            free((char*)tmp[s].parent_name);
            free((char*)tmp[s].return_type);
            if (tmp[s].param_types) {
                for (size_t p = 0; p < tmp[s].param_count; ++p) {
                    free((char*)tmp[s].param_types[p]);
                }
                free(tmp[s].param_types);
            }
            if (tmp[s].param_names) {
                for (size_t p = 0; p < tmp[s].param_count; ++p) {
                    free((char*)tmp[s].param_names[p]);
                }
                free(tmp[s].param_names);
            }
        }
        free(tmp);
        if (json_object_object_get_ex(obj, "stamp", &jstamp)) {
            long long s = json_object_get_int64(jstamp);
            if (s > 0 && (uint64_t)s > g_stamp_counter) g_stamp_counter = (uint64_t)s;
        }
    }
    json_object_put(root);
}
