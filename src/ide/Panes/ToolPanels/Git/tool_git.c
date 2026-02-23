#include "tool_git.h"
#include "ide/Panes/ToolPanels/Git/render_tool_git.h"

#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GitFileEntry gitFiles[MAX_GIT_ENTRIES];
int gitFileCount = 0;
char currentGitBranch[64] = "unknown";
GitLogEntry gitLogEntries[MAX_GIT_LOG_ENTRIES];
int gitLogCount = 0;
static SDL_Rect g_addAllRect = {0};
static SDL_Rect g_commitRect = {0};
static SDL_Rect g_messageRect = {0};
static bool g_messageFocused = false;
static char g_commitMessage[256] = {0};
static int g_commitCursor = 0;
static char g_statusText[256] = {0};

static uint64_t g_lastStatusHash = 0;
static char g_lastWorkspacePath[1024] = {0};

static uint64_t fnv1a64_bytes(const unsigned char* data, size_t len, uint64_t seed) {
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static bool point_in_rect(int x, int y, SDL_Rect r) {
    return x >= r.x && x <= (r.x + r.w) &&
           y >= r.y && y <= (r.y + r.h);
}

static void sanitize_line(char* s) {
    if (!s) return;
    for (int i = 0; s[i]; ++i) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            break;
        }
    }
}

static void shell_append_quoted(char* out, size_t outCap, const char* raw) {
    if (!out || outCap == 0) return;
    size_t n = strlen(out);
    if (n + 2 >= outCap) return;
    out[n++] = '\'';
    if (raw) {
        for (size_t i = 0; raw[i] && n + 5 < outCap; ++i) {
            if (raw[i] == '\'') {
                out[n++] = '\'';
                out[n++] = '\\';
                out[n++] = '\'';
                out[n++] = '\'';
            } else {
                out[n++] = raw[i];
            }
        }
    }
    if (n + 1 < outCap) out[n++] = '\'';
    out[n] = '\0';
}

static bool run_git_command(const char* gitArgs, char* outLine, size_t outLineCap) {
    if (outLine && outLineCap > 0) outLine[0] = '\0';
    if (!projectPath[0] || !gitArgs || !gitArgs[0]) return false;

    char cmd[2048] = {0};
    snprintf(cmd, sizeof(cmd), "cd ");
    shell_append_quoted(cmd, sizeof(cmd), projectPath);
    strncat(cmd, " && git ", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, gitArgs, sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, " 2>&1", sizeof(cmd) - strlen(cmd) - 1);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        if (outLine && outLineCap > 0) snprintf(outLine, outLineCap, "Failed to start git command");
        return false;
    }

    if (outLine && outLineCap > 0) {
        if (!fgets(outLine, (int)outLineCap, pipe)) {
            outLine[0] = '\0';
        } else {
            sanitize_line(outLine);
        }
    } else {
        char sink[256];
        fgets(sink, sizeof(sink), pipe);
    }

    int rc = pclose(pipe);
    return rc == 0;
}

void resetGitStatusWatcher(void) {
    g_lastStatusHash = 0;
    g_lastWorkspacePath[0] = '\0';
}

void git_panel_set_add_all_rect(SDL_Rect rect) { g_addAllRect = rect; }
void git_panel_set_commit_rect(SDL_Rect rect) { g_commitRect = rect; }
void git_panel_set_message_rect(SDL_Rect rect) { g_messageRect = rect; }
bool git_panel_point_in_add_all(int x, int y) { return point_in_rect(x, y, g_addAllRect); }
bool git_panel_point_in_commit(int x, int y) { return point_in_rect(x, y, g_commitRect); }
bool git_panel_point_in_message(int x, int y) { return point_in_rect(x, y, g_messageRect); }
void git_panel_set_message_focus(bool focused) { g_messageFocused = focused; }
bool git_panel_is_message_focused(void) { return g_messageFocused; }
const char* git_panel_get_message(void) { return g_commitMessage; }
int git_panel_get_message_cursor(void) { return g_commitCursor; }
void git_panel_set_message(const char* text) {
    if (!text) text = "";
    snprintf(g_commitMessage, sizeof(g_commitMessage), "%s", text);
    g_commitCursor = (int)strlen(g_commitMessage);
}
const char* git_panel_get_status_text(void) { return g_statusText; }

void git_panel_insert_text(const char* text) {
    if (!text || !text[0]) return;
    size_t curLen = strlen(g_commitMessage);
    size_t addLen = strlen(text);
    if (addLen == 0 || curLen >= sizeof(g_commitMessage) - 1) return;
    if (g_commitCursor < 0) g_commitCursor = 0;
    if ((size_t)g_commitCursor > curLen) g_commitCursor = (int)curLen;
    if (curLen + addLen >= sizeof(g_commitMessage)) addLen = sizeof(g_commitMessage) - 1 - curLen;
    memmove(g_commitMessage + g_commitCursor + addLen,
            g_commitMessage + g_commitCursor,
            curLen - (size_t)g_commitCursor + 1);
    memcpy(g_commitMessage + g_commitCursor, text, addLen);
    g_commitCursor += (int)addLen;
}

void git_panel_backspace(void) {
    size_t len = strlen(g_commitMessage);
    if (g_commitCursor <= 0 || len == 0) return;
    memmove(g_commitMessage + g_commitCursor - 1,
            g_commitMessage + g_commitCursor,
            len - (size_t)g_commitCursor + 1);
    g_commitCursor--;
}

void git_panel_delete(void) {
    size_t len = strlen(g_commitMessage);
    if ((size_t)g_commitCursor >= len) return;
    memmove(g_commitMessage + g_commitCursor,
            g_commitMessage + g_commitCursor + 1,
            len - (size_t)g_commitCursor);
}

void git_panel_move_cursor_left(void) { if (g_commitCursor > 0) g_commitCursor--; }
void git_panel_move_cursor_right(void) {
    int len = (int)strlen(g_commitMessage);
    if (g_commitCursor < len) g_commitCursor++;
}
void git_panel_move_cursor_home(void) { g_commitCursor = 0; }
void git_panel_move_cursor_end(void) { g_commitCursor = (int)strlen(g_commitMessage); }

bool git_stage_all_changes(void) {
    char line[256] = {0};
    bool ok = run_git_command("add -A", line, sizeof(line));
    if (ok) {
        snprintf(g_statusText, sizeof(g_statusText), "Staged all changes");
    } else {
        snprintf(g_statusText, sizeof(g_statusText), "git add failed: %s", line[0] ? line : "unknown error");
    }
    resetGitTree();
    return ok;
}

bool git_commit_with_message(void) {
    if (!g_commitMessage[0]) {
        snprintf(g_statusText, sizeof(g_statusText), "Commit message is empty");
        return false;
    }

    char args[1024] = {0};
    snprintf(args, sizeof(args), "commit -m ");
    shell_append_quoted(args, sizeof(args), g_commitMessage);

    char line[256] = {0};
    bool ok = run_git_command(args, line, sizeof(line));
    if (ok) {
        snprintf(g_statusText, sizeof(g_statusText), "%s", line[0] ? line : "Commit complete");
        g_commitMessage[0] = '\0';
        g_commitCursor = 0;
    } else {
        snprintf(g_statusText, sizeof(g_statusText), "git commit failed: %s", line[0] ? line : "unknown error");
    }
    resetGitTree();
    return ok;
}

void refreshGitStatus() {
    gitFileCount = 0;

    // Get branch name
    char branchCmd[1024];
    snprintf(branchCmd, sizeof(branchCmd), "cd %s && git branch --show-current", projectPath);
    printf("[Git] Using projectPath: %s\n", projectPath);

    FILE* bpipe = popen(branchCmd, "r");
    if (bpipe) {
        fgets(currentGitBranch, sizeof(currentGitBranch), bpipe);
        currentGitBranch[strcspn(currentGitBranch, "\n")] = 0;
        pclose(bpipe);
    }

    // Get file status
    char statusCmd[1024];
    snprintf(statusCmd, sizeof(statusCmd), "cd %s && git status --porcelain", projectPath);
    FILE* pipe = popen(statusCmd, "r");
    if (!pipe) {
        fprintf(stderr, "[GitError] Failed to run git status\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), pipe) && gitFileCount < MAX_GIT_ENTRIES) {
        if (strlen(line) < 4) continue;

        GitFileEntry* f = &gitFiles[gitFileCount++];

        char statusCode[3];
        strncpy(statusCode, line, 2);
        statusCode[2] = '\0';

        char* filePath = line + 3;
        filePath[strcspn(filePath, "\n")] = 0;

        strncpy(f->path, filePath, sizeof(f->path) - 1);
        f->path[sizeof(f->path) - 1] = '\0';

        printf("[GitDebug] Raw line: %s", line);
        printf("[GitDebug] Parsed status: '%s', file: '%s'\n", statusCode, f->path);

        // Parse status into enum
        if (statusCode[0] == 'M') {
            f->status = GIT_STATUS_STAGED;
            f->staged = true;
        } else if (statusCode[1] == 'M') {
            f->status = GIT_STATUS_MODIFIED;
            f->staged = false;
        } else if (statusCode[1] == 'A') {
            f->status = GIT_STATUS_ADDED;
            f->staged = false;
        } else if (statusCode[1] == 'D') {
            f->status = GIT_STATUS_DELETED;
            f->staged = false;
        } else if (statusCode[1] == '?') {
            f->status = GIT_STATUS_UNTRACKED;
            f->staged = false;
        } else {
            f->status = GIT_STATUS_MODIFIED;  // Fallback
            f->staged = false;
        }
    }

    printf("[GitDebug] Final file count: %d\n", gitFileCount);
    pclose(pipe);
}

void refreshGitLog(int maxEntries) {
    gitLogCount = 0;
    if (maxEntries <= 0 || maxEntries > MAX_GIT_LOG_ENTRIES) {
        maxEntries = MAX_GIT_LOG_ENTRIES;
    }

    char cmd[512];
    // Tab-delimited: hash<TAB>subject<TAB>author<TAB>short-date
    snprintf(cmd,
             sizeof(cmd),
             "cd \"%s\" && git log -n %d --date=short --pretty=format:\"%%h\t%%s\t%%an\t%%ad\"",
             projectPath,
             maxEntries);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "[GitError] Failed to run git log\n");
        return;
    }

    char line[GIT_LOG_LINE_MAX];
    while (fgets(line, sizeof(line), pipe) && gitLogCount < maxEntries) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (line[0] == '\0') continue;

        char* t1 = strchr(line, '\t');
        if (!t1) continue;
        *t1 = '\0';
        char* subject = t1 + 1;

        char* t2 = strchr(subject, '\t');
        if (!t2) continue;
        *t2 = '\0';
        char* author = t2 + 1;

        char* t3 = strchr(author, '\t');
        if (!t3) continue;
        *t3 = '\0';
        char* date = t3 + 1;

        GitLogEntry* e = &gitLogEntries[gitLogCount++];
        strncpy(e->hash, line, sizeof(e->hash) - 1);
        e->hash[sizeof(e->hash) - 1] = '\0';
        snprintf(e->message, sizeof(e->message), "%s", subject);
        snprintf(e->author, sizeof(e->author), "%s", author);
        snprintf(e->date, sizeof(e->date), "%s", date);
    }

    pclose(pipe);
}

void pollGitStatusWatcher(void) {
    if (!projectPath[0]) return;

    if (strncmp(g_lastWorkspacePath, projectPath, sizeof(g_lastWorkspacePath)) != 0) {
        snprintf(g_lastWorkspacePath, sizeof(g_lastWorkspacePath), "%s", projectPath);
        g_lastWorkspacePath[sizeof(g_lastWorkspacePath) - 1] = '\0';
        g_lastStatusHash = 0;
        resetGitTree();
    }

    char cmd[1200];
    snprintf(cmd,
             sizeof(cmd),
             "cd \"%s\" && git status --porcelain=v1 --branch 2>/dev/null",
             projectPath);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return;

    uint64_t hash = 0;
    char buf[1024];
    bool sawAny = false;
    while (fgets(buf, sizeof(buf), pipe)) {
        sawAny = true;
        hash = fnv1a64_bytes((const unsigned char*)buf, strlen(buf), hash);
    }
    int rc = pclose(pipe);
    if (rc != 0) return;
    if (!sawAny) {
        hash = fnv1a64_bytes((const unsigned char*)"clean", 5, 0);
    }

    if (g_lastStatusHash != 0 && hash != g_lastStatusHash) {
        resetGitTree();
    }
    g_lastStatusHash = hash;
}

Uint32 gitStatusWatchIntervalMs(void) {
    const Uint32 defaultMs = 1500;
    const char* env = getenv("IDE_GIT_WATCHER_POLL_MS");
    if (!env || !env[0]) return defaultMs;
    char* end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env || v < 200 || v > 10000) return defaultMs;
    return (Uint32)v;
}
