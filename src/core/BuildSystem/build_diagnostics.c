#include "build_diagnostics.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <json-c/json.h>

#define MAX_BUILD_DIAGNOSTICS 256

static BuildDiagnostic g_diags[MAX_BUILD_DIAGNOSTICS];
static size_t g_diag_count = 0;
static char g_line_buffer[2048];
static size_t g_line_len = 0;
static int g_last_index_with_notes = -1;

static void strip_ansi(const char* src, char* dst, size_t dstSize) {
    size_t di = 0;
    for (size_t i = 0; src[i] && di + 1 < dstSize; ) {
        if (src[i] == '\x1b') {
            // Skip ESC sequences: ESC [ ... letter or ESC ] ... BEL/ST
            i++;
            if (src[i] == '[') {
                i++;
                while (src[i] && !((src[i] >= '@' && src[i] <= '~'))) i++;
                if (src[i]) i++;
            } else if (src[i] == ']') {
                while (src[i] && src[i] != '\a') i++;
                if (src[i]) i++;
            } else {
                // single char escapes
                i++;
            }
            continue;
        }
        dst[di++] = src[i++];
    }
    dst[di] = '\0';
}

static void try_parse_line(const char* rawLine) {
    if (g_diag_count >= MAX_BUILD_DIAGNOSTICS) return;

    char clean[1024];
    strip_ansi(rawLine, clean, sizeof(clean));
    // Trim trailing newline
    size_t len = strlen(clean);
    while (len > 0 && (clean[len - 1] == '\r' || clean[len - 1] == '\n')) {
        clean[--len] = '\0';
    }
    if (len == 0) return;

    // note lines
    if (strncmp(clean, "note:", 5) == 0 && g_last_index_with_notes >= 0) {
        BuildDiagnostic* d = &g_diags[g_last_index_with_notes];
        size_t cur = strlen(d->notes);
        const char* noteText = clean + 5;
        while (*noteText && isspace((unsigned char)*noteText)) noteText++;
        if (cur + strlen(noteText) + 2 < BUILD_DIAG_NOTES_MAX) {
            if (cur > 0) {
                d->notes[cur++] = '\n';
                d->notes[cur] = '\0';
            }
            strncat(d->notes, noteText, BUILD_DIAG_NOTES_MAX - cur - 1);
        }
        return;
    }

    // Format: path:line:col: (warning|error): message
    char path[BUILD_DIAG_PATH_MAX];
    char sev[16];
    int lineNo = 0, colNo = 0;
    char msg[BUILD_DIAG_MSG_MAX];
    path[0] = msg[0] = sev[0] = '\0';

    int matched = sscanf(clean, "%511[^:]:%d:%d: %15[^:]: %511[^\n]", path, &lineNo, &colNo, sev, msg);
    if (matched == 5) {
        bool isError = (strncmp(sev, "error", 5) == 0);
        bool isWarning = (strncmp(sev, "warning", 7) == 0);
        if (isError || isWarning) {
            BuildDiagnostic* d = &g_diags[g_diag_count++];
            snprintf(d->path, sizeof(d->path), "%s", path);
            d->line = lineNo;
            d->col = colNo;
            d->isError = isError;
            snprintf(d->message, sizeof(d->message), "%s", msg);
            d->notes[0] = '\0';
            g_last_index_with_notes = (int)(g_diag_count - 1);
        }
    }
}

/* Optional persisted diagnostics must never hold up the UI or IPC startup.
 * A bounded pair of detached readers owns only cache bytes, never app state.
 * A clear/build/workspace change invalidates older results before publication. */
typedef struct DiagnosticsCacheRead {
    bool busy, done;
    uint64_t generation;
    struct timespec started;
    char path[PATH_MAX];
    char* bytes;
} DiagnosticsCacheRead;
static DiagnosticsCacheRead cache_reads[2];
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t cache_generation;

static void* read_diagnostics_cache(void* argument) {
    DiagnosticsCacheRead* request = argument;
    char* bytes = NULL;
    int fd = open(request->path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    struct stat st;
    if (fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
        st.st_size > 0 && st.st_size <= (1 << 20)) {
        size_t size = (size_t)st.st_size, used = 0;
        bytes = malloc(size + 1);
        if (bytes) {
            while (used < size) {
                ssize_t got = read(fd, bytes + used, size - used);
                if (got <= 0) break;
                used += (size_t)got;
            }
            if (used != size) { free(bytes); bytes = NULL; }
            else bytes[size] = '\0';
        }
    }
    if (fd >= 0) close(fd);
    pthread_mutex_lock(&cache_lock);
    request->bytes = bytes;
    request->done = true;
    pthread_mutex_unlock(&cache_lock);
    return NULL;
}

void build_diagnostics_clear(void) {
    ++cache_generation;
    g_diag_count = 0;
    g_line_len = 0;
    g_last_index_with_notes = -1;
    g_line_buffer[0] = '\0';
}

void build_diagnostics_feed_chunk(const char* data, size_t len) {
    if (!data || len == 0) return;
    size_t i = 0;
    while (i < len) {
        char ch = data[i++];
        if (g_line_len + 1 >= sizeof(g_line_buffer)) {
            // flush oversized line
            g_line_buffer[g_line_len] = '\0';
            try_parse_line(g_line_buffer);
            g_line_len = 0;
        }
        g_line_buffer[g_line_len++] = ch;
        if (ch == '\n') {
            g_line_buffer[g_line_len] = '\0';
            try_parse_line(g_line_buffer);
            g_line_len = 0;
        }
    }
}

const BuildDiagnostic* build_diagnostics_get(size_t* count) {
    if (count) *count = g_diag_count;
    return g_diags;
}

void build_diagnostics_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files", workspaceRoot);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(path, 0755);
    }
    snprintf(path, sizeof(path), "%s/ide_files/build_output.json", workspaceRoot);

    json_object* arr = json_object_new_array();
    for (size_t i = 0; i < g_diag_count; ++i) {
        const BuildDiagnostic* d = &g_diags[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "path", json_object_new_string(d->path));
        json_object_object_add(obj, "line", json_object_new_int(d->line));
        json_object_object_add(obj, "col", json_object_new_int(d->col));
        json_object_object_add(obj, "isError", json_object_new_boolean(d->isError));
        json_object_object_add(obj, "message", json_object_new_string(d->message));
        json_object_object_add(obj, "notes", json_object_new_string(d->notes));
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
}

static void apply_diagnostics_cache(const char* bytes) {
    json_object* root = json_tokener_parse(bytes);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return;
    }

    size_t arrLen = json_object_array_length(root);
    for (size_t i = 0; i < arrLen && g_diag_count < MAX_BUILD_DIAGNOSTICS; ++i) {
        json_object* obj = json_object_array_get_idx(root, i);
        if (!obj) continue;
        BuildDiagnostic* d = &g_diags[g_diag_count];
        json_object* jpath = NULL;
        json_object* jline = NULL;
        json_object* jcol = NULL;
        json_object* jerr = NULL;
        json_object* jmsg = NULL;
        json_object* jnotes = NULL;
        if (json_object_object_get_ex(obj, "path", &jpath) &&
            json_object_object_get_ex(obj, "line", &jline) &&
            json_object_object_get_ex(obj, "col", &jcol) &&
            json_object_object_get_ex(obj, "isError", &jerr) &&
            json_object_object_get_ex(obj, "message", &jmsg)) {
            snprintf(d->path, sizeof(d->path), "%s", json_object_get_string(jpath));
            d->line = json_object_get_int(jline);
            d->col = json_object_get_int(jcol);
            d->isError = json_object_get_boolean(jerr);
            snprintf(d->message, sizeof(d->message), "%s", json_object_get_string(jmsg));
            if (json_object_object_get_ex(obj, "notes", &jnotes) && jnotes) {
                snprintf(d->notes, sizeof(d->notes), "%s", json_object_get_string(jnotes));
            } else {
                d->notes[0] = '\0';
            }
            g_diag_count++;
        }
    }
    json_object_put(root);
}

void build_diagnostics_poll(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (size_t i = 0; i < 2; ++i) {
        pthread_mutex_lock(&cache_lock);
        DiagnosticsCacheRead* request = &cache_reads[i];
        char* bytes = NULL;
        if (request->busy && request->done) {
            double elapsed = (double)(now.tv_sec - request->started.tv_sec) +
                (double)(now.tv_nsec - request->started.tv_nsec) / 1e9;
            if (request->generation == cache_generation && elapsed < 2.0)
                bytes = request->bytes;
            else free(request->bytes);
            *request = (DiagnosticsCacheRead){0};
        }
        pthread_mutex_unlock(&cache_lock);
        if (bytes) { apply_diagnostics_cache(bytes); free(bytes); }
    }
}

void build_diagnostics_load(const char* workspaceRoot) {
    build_diagnostics_clear();
    build_diagnostics_poll();
    if (!workspaceRoot || !*workspaceRoot) return;
    pthread_mutex_lock(&cache_lock);
    for (size_t i = 0; i < 2; ++i) {
        DiagnosticsCacheRead* request = &cache_reads[i];
        if (request->busy) continue;
        *request = (DiagnosticsCacheRead){.busy=true, .generation=cache_generation};
        clock_gettime(CLOCK_MONOTONIC, &request->started);
        int written = snprintf(request->path, sizeof(request->path),
            "%s/ide_files/build_output.json", workspaceRoot);
        pthread_t thread;
        if (written < 0 || (size_t)written >= sizeof(request->path) ||
            pthread_create(&thread, NULL, read_diagnostics_cache, request) != 0)
            *request = (DiagnosticsCacheRead){0};
        else pthread_detach(thread);
        break; /* Saturation skips this optional cache rather than spawning more. */
    }
    pthread_mutex_unlock(&cache_lock);
}
