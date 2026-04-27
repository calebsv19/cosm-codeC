#include "core/Ipc/ide_ipc_build_helpers.h"

#include "app/GlobalInfo/workspace_prefs.h"
#include "core/BuildSystem/build_diagnostics.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char* storage;
    char** argv;
    size_t argc;
} ExecCommandTokens;

typedef struct {
    char** items;
    size_t count;
    size_t cap;
} SourceFileList;

static void str_copy(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    if (!src) src = "";
    snprintf(dst, cap, "%s", src);
}

static json_object* build_error_obj_local(const char* code, const char* message, const char* details) {
    json_object* err = json_object_new_object();
    json_object_object_add(err, "code", json_object_new_string(code ? code : "error"));
    json_object_object_add(err, "message", json_object_new_string(message ? message : "Unknown error"));
    if (details && *details) {
        json_object_object_add(err, "details", json_object_new_string(details));
    }
    return err;
}

static bool has_makefile_in_dir(const char* dir) {
    if (!dir || !*dir) return false;
    struct stat st;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/Makefile", dir);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return true;
    snprintf(path, sizeof(path), "%s/makefile", dir);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return true;
    return false;
}

static bool is_valid_build_profile(const char* profile) {
    if (!profile || !*profile) return true;
    return (strcmp(profile, "debug") == 0 || strcmp(profile, "perf") == 0);
}

static bool ensure_dir_exists(const char* path, mode_t mode) {
    if (!path || !*path) return false;

    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    if (mkdir(path, mode) != 0 && errno != EEXIST) {
        return false;
    }

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

static void free_exec_command_tokens(ExecCommandTokens* tokens) {
    if (!tokens) return;
    free(tokens->storage);
    free(tokens->argv);
    memset(tokens, 0, sizeof(*tokens));
}

static bool parse_command_tokens(const char* text,
                                 ExecCommandTokens* out,
                                 char* error_out,
                                 size_t error_out_cap) {
    if (error_out && error_out_cap > 0) error_out[0] = '\0';
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!text || !text[0]) {
        str_copy(error_out, error_out_cap, "Command is empty");
        return false;
    }

    const size_t len = strlen(text);
    out->storage = (char*)malloc(len + 1);
    size_t argv_cap = (len / 2) + 4;
    out->argv = (char**)calloc(argv_cap, sizeof(char*));
    if (!out->storage || !out->argv) {
        free_exec_command_tokens(out);
        str_copy(error_out, error_out_cap, "Failed to allocate command parser state");
        return false;
    }

    const char* p = text;
    char* w = out->storage;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        if (out->argc + 1 >= argv_cap) {
            free_exec_command_tokens(out);
            str_copy(error_out, error_out_cap, "Command has too many arguments");
            return false;
        }

        out->argv[out->argc++] = w;
        char quote = '\0';
        while (*p) {
            char c = *p;
            if (quote) {
                if (c == quote) {
                    quote = '\0';
                    p++;
                    continue;
                }
                if (c == '\\' && quote == '"' && p[1]) {
                    *w++ = p[1];
                    p += 2;
                    continue;
                }
                *w++ = c;
                p++;
                continue;
            }

            if (isspace((unsigned char)c)) break;
            if (c == '"' || c == '\'') {
                quote = c;
                p++;
                continue;
            }
            if (c == '\\' && p[1]) {
                *w++ = p[1];
                p += 2;
                continue;
            }

            *w++ = c;
            p++;
        }

        if (quote) {
            free_exec_command_tokens(out);
            str_copy(error_out, error_out_cap, "Unterminated quote in command");
            return false;
        }

        *w++ = '\0';
        while (*p && isspace((unsigned char)*p)) p++;
    }

    if (out->argc == 0) {
        free_exec_command_tokens(out);
        str_copy(error_out, error_out_cap, "Command is empty");
        return false;
    }

    out->argv[out->argc] = NULL;
    return true;
}

static void free_source_file_list(SourceFileList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool append_source_file(SourceFileList* list, const char* path) {
    if (!list || !path || !path[0]) return false;
    if (list->count == list->cap) {
        size_t next_cap = list->cap ? (list->cap * 2) : 32;
        char** grown = (char**)realloc(list->items, next_cap * sizeof(char*));
        if (!grown) return false;
        list->items = grown;
        list->cap = next_cap;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

static bool should_skip_fallback_entry(const char* name) {
    if (!name || !name[0]) return true;
    return (strcmp(name, ".") == 0 ||
            strcmp(name, "..") == 0 ||
            strcmp(name, "build") == 0 ||
            strcmp(name, ".git") == 0 ||
            strcmp(name, "ide_files") == 0);
}

static bool collect_fallback_sources_recursive(const char* dir, SourceFileList* out) {
    if (!dir || !dir[0] || !out) return false;
    DIR* dp = opendir(dir);
    if (!dp) return false;

    struct dirent* ent = NULL;
    bool ok = true;
    while ((ent = readdir(dp)) != NULL) {
        if (should_skip_fallback_entry(ent->d_name)) continue;

        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name) >= (int)sizeof(path)) {
            ok = false;
            break;
        }

        struct stat st;
        if (lstat(path, &st) != 0) {
            ok = false;
            break;
        }

        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!collect_fallback_sources_recursive(path, out)) {
                ok = false;
                break;
            }
            continue;
        }

        if (!S_ISREG(st.st_mode)) continue;
        size_t name_len = strlen(ent->d_name);
        if (name_len < 3 || strcmp(ent->d_name + (name_len - 2), ".c") != 0) continue;
        if (!append_source_file(out, path)) {
            ok = false;
            break;
        }
    }

    closedir(dp);
    return ok;
}

static int run_exec_capture(const char* working_dir,
                            char* const argv[],
                            const char* profile,
                            char* output,
                            size_t output_cap,
                            size_t* out_len) {
    if (!working_dir || !*working_dir || !output || output_cap == 0) return -1;
    if (out_len) *out_len = 0;
    output[0] = '\0';
    if (!argv || !argv[0] || !argv[0][0]) return -1;

    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0) {
        if (chdir(working_dir) != 0) _exit(127);
        if (profile && *profile) {
            setenv("PROFILE", profile, 1);
        } else {
            unsetenv("PROFILE");
        }
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) _exit(127);
        if (dup2(pipe_fd[1], STDERR_FILENO) < 0) _exit(127);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipe_fd[1]);
    FILE* pipe_stream = fdopen(pipe_fd[0], "r");
    if (!pipe_stream) {
        close(pipe_fd[0]);
        (void)waitpid(pid, NULL, 0);
        return -1;
    }

    size_t written = 0;
    char line[512];
    while (fgets(line, sizeof(line), pipe_stream)) {
        size_t n = strlen(line);
        if (written + n < output_cap - 1) {
            memcpy(output + written, line, n);
            written += n;
            output[written] = '\0';
        }
        build_diagnostics_feed_chunk(line, n);
    }
    fclose(pipe_stream);
    if (out_len) *out_len = written;

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static int run_make_capture(const char* working_dir,
                            const char* profile,
                            char* output,
                            size_t output_cap,
                            size_t* out_len) {
    char* const argv[] = {"make", NULL};
    return run_exec_capture(working_dir, argv, profile, output, output_cap, out_len);
}

static int run_command_text_capture(const char* working_dir,
                                    const char* command_text,
                                    const char* profile,
                                    char* output,
                                    size_t output_cap,
                                    size_t* out_len,
                                    char* error_out,
                                    size_t error_out_cap) {
    ExecCommandTokens tokens = {0};
    if (!parse_command_tokens(command_text, &tokens, error_out, error_out_cap)) {
        return -1;
    }
    int status = run_exec_capture(working_dir, tokens.argv, profile, output, output_cap, out_len);
    free_exec_command_tokens(&tokens);
    return status;
}

static int run_fallback_compile_capture(const char* project_root,
                                        const char* profile,
                                        char* output,
                                        size_t output_cap,
                                        size_t* out_len) {
    if (!project_root || !project_root[0] || !output || output_cap == 0) return -1;
    if (out_len) *out_len = 0;
    output[0] = '\0';

    char build_dir[PATH_MAX];
    if (snprintf(build_dir, sizeof(build_dir), "%s/build", project_root) >= (int)sizeof(build_dir)) {
        return -1;
    }
    if (!ensure_dir_exists(build_dir, 0755)) {
        str_copy(output, output_cap, "Failed to create build directory\n");
        if (out_len) *out_len = strlen(output);
        build_diagnostics_feed_chunk(output, strlen(output));
        return 1;
    }

    SourceFileList sources = {0};
    if (!collect_fallback_sources_recursive(project_root, &sources)) {
        free_source_file_list(&sources);
        str_copy(output, output_cap, "Failed to enumerate project source files\n");
        if (out_len) *out_len = strlen(output);
        build_diagnostics_feed_chunk(output, strlen(output));
        return 1;
    }

    if (sources.count == 0) {
        free_source_file_list(&sources);
        str_copy(output, output_cap, "No C source files found for fallback build\n");
        if (out_len) *out_len = strlen(output);
        build_diagnostics_feed_chunk(output, strlen(output));
        return 1;
    }

    char** argv = (char**)calloc(sources.count + 9, sizeof(char*));
    if (!argv) {
        free_source_file_list(&sources);
        return -1;
    }

    size_t argc = 0;
    argv[argc++] = "cc";
    argv[argc++] = "-std=c11";
    argv[argc++] = "-Wall";
    argv[argc++] = "-Wextra";
    argv[argc++] = "-g";
    argv[argc++] = "-Iinclude";
    argv[argc++] = "-Isrc";
    argv[argc++] = "-o";
    argv[argc++] = "build/app";
    for (size_t i = 0; i < sources.count; ++i) {
        argv[argc++] = sources.items[i];
    }
    argv[argc] = NULL;

    int status = run_exec_capture(project_root, argv, profile, output, output_cap, out_len);
    free(argv);
    free_source_file_list(&sources);
    return status;
}

static void expand_path_relative(const char* base_dir, const char* path, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || !*path) return;
    if (path[0] == '/') {
        strncpy(out, path, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) home = "";
        if (path[1] == '/' || path[1] == '\\') snprintf(out, out_size, "%s/%s", home, path + 2);
        else if (path[1] == '\0') snprintf(out, out_size, "%s", home);
        else snprintf(out, out_size, "%s/%s", home, path + 1);
        return;
    }
    if (base_dir && *base_dir) snprintf(out, out_size, "%s/%s", base_dir, path);
    else snprintf(out, out_size, "%s", path);
}

json_object* ide_ipc_build_build_result(json_object* args,
                                        const char* project_root,
                                        json_object** error_out) {
    *error_out = NULL;
    if (!project_root || !*project_root) {
        *error_out = build_error_obj_local("bad_state", "Project root unavailable", NULL);
        return NULL;
    }

    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* profile = NULL;
    if (args) {
        json_object* jprofile = NULL;
        if (json_object_object_get_ex(args, "profile", &jprofile) &&
            jprofile && json_object_is_type(jprofile, json_type_string)) {
            profile = json_object_get_string(jprofile);
        }
    }
    if (!is_valid_build_profile(profile)) {
        *error_out = build_error_obj_local("bad_request", "Invalid profile. Expected debug or perf.", profile);
        return NULL;
    }

    char working_dir[PATH_MAX];
    snprintf(working_dir, sizeof(working_dir), "%s", project_root);
    if (cfg && cfg->build_working_dir[0]) {
        expand_path_relative(project_root, cfg->build_working_dir, working_dir, sizeof(working_dir));
    }

    char command_text[2048];
    command_text[0] = '\0';
    char command_display[2048];
    command_display[0] = '\0';
    bool use_make_exec = false;
    bool use_fallback_exec = false;
    if (cfg && cfg->build_command[0]) {
        snprintf(command_text,
                 sizeof(command_text),
                 "%s%s%s",
                 cfg->build_command,
                 cfg->build_args[0] ? " " : "",
                 cfg->build_args[0] ? cfg->build_args : "");
        snprintf(command_display,
                 sizeof(command_display),
                 "%s%s%s",
                 cfg->build_command,
                 cfg->build_args[0] ? " " : "",
                 cfg->build_args[0] ? cfg->build_args : "");
    } else if (has_makefile_in_dir(project_root)) {
        use_make_exec = true;
        if (profile && *profile) {
            snprintf(command_display, sizeof(command_display), "make (PROFILE=%s)", profile);
        } else {
            snprintf(command_display, sizeof(command_display), "make");
        }
    } else {
        snprintf(command_display,
                 sizeof(command_display),
                 "cc -std=c11 -Wall -Wextra -g -Iinclude -Isrc -o build/app <project sources>");
        use_fallback_exec = true;
    }

    build_diagnostics_clear();

    char output[32768];
    size_t out_len = 0;
    output[0] = '\0';
    int exit_code = 1;

    if (use_make_exec) {
        int make_status = run_make_capture(project_root, profile, output, sizeof(output), &out_len);
        if (make_status < 0) {
            *error_out = build_error_obj_local("build_failed", "Failed to execute make command", command_display);
            return NULL;
        }
        exit_code = make_status;
    } else if (use_fallback_exec) {
        int fallback_status = run_fallback_compile_capture(project_root, profile, output, sizeof(output), &out_len);
        if (fallback_status < 0) {
            *error_out = build_error_obj_local("build_failed", "Failed to execute fallback build command", command_display);
            return NULL;
        }
        exit_code = fallback_status;
    } else {
        char parse_err[256] = {0};
        int cmd_status = run_command_text_capture(working_dir,
                                                  command_text,
                                                  profile,
                                                  output,
                                                  sizeof(output),
                                                  &out_len,
                                                  parse_err,
                                                  sizeof(parse_err));
        if (cmd_status < 0) {
            *error_out = build_error_obj_local("build_failed",
                                               parse_err[0] ? parse_err : "Failed to execute build command",
                                               command_display);
            return NULL;
        }
        exit_code = cmd_status;
    }

    size_t diag_count = 0;
    const BuildDiagnostic* build_diags = build_diagnostics_get(&diag_count);
    int errors = 0;
    int warnings = 0;
    for (size_t i = 0; i < diag_count; ++i) {
        if (build_diags[i].isError) errors++;
        else warnings++;
    }

    json_object* result = json_object_new_object();
    json_object_object_add(result, "ok", json_object_new_boolean(exit_code == 0));
    json_object_object_add(result, "status", json_object_new_string(exit_code == 0 ? "success" : "failed"));
    json_object_object_add(result, "exit_code", json_object_new_int(exit_code));
    json_object_object_add(result, "command", json_object_new_string(command_display));
    json_object_object_add(result, "working_dir", json_object_new_string(working_dir));
    json_object_object_add(result, "profile", json_object_new_string(profile ? profile : ""));
    json_object_object_add(result, "output", json_object_new_string(output));
    json_object_object_add(result, "output_truncated", json_object_new_boolean(out_len >= sizeof(output) - 1));

    json_object* diag_summary = json_object_new_object();
    json_object_object_add(diag_summary, "total", json_object_new_int((int)diag_count));
    json_object_object_add(diag_summary, "error", json_object_new_int(errors));
    json_object_object_add(diag_summary, "warn", json_object_new_int(warnings));
    json_object_object_add(result, "diagnostics_summary", diag_summary);
    return result;
}
