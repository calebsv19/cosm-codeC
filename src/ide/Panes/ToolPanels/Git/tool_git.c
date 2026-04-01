#include "tool_git.h"
#include "ide/Panes/ToolPanels/Git/tree_git_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/panel_text_edit.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/text_input_focus.h"

#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GitFileEntry files[MAX_GIT_ENTRIES];
    int file_count;
    char current_branch[64];
    GitLogEntry* logs;
    int log_count;
    int log_capacity;
    FILE* log_pipe;
    bool log_loading;
    bool log_parse_error;
} GitPanelModelState;

typedef struct {
    UIPanelThreeSegmentStripLayout top_strip_layout;
    bool message_focused;
    char commit_message[256];
    int commit_cursor;
    char status_text[256];
} GitPanelUIState;

typedef struct {
    char* key;
    bool expanded;
} GitPanelExpandCacheEntry;

typedef struct {
    UITreeNode* tree;
    bool needs_refresh;
    PaneScrollState scroll;
    bool scroll_init;
    SDL_Rect scroll_track;
    SDL_Rect scroll_thumb;
    UIDoubleClickTracker double_click_tracker;
    GitPanelExpandCacheEntry* expand_cache;
    int expand_count;
    int expand_cap;
} GitPanelTreeState;

typedef struct {
    uint64_t last_status_hash;
    char last_workspace_path[1024];
} GitPanelWatcherState;

typedef struct {
    GitPanelModelState model;
    GitPanelUIState ui;
    GitPanelTreeState tree;
    GitPanelWatcherState watcher;
} GitPanelControllerState;

static GitPanelControllerState g_gitPanelBootstrapState = {
    .model = {
        .current_branch = "unknown"
    },
    .tree = {
        .needs_refresh = true
    }
};
static bool g_gitPanelBootstrapInitialized = true;

static void git_panel_init_controller_state(void* ptr) {
    GitPanelControllerState* state = (GitPanelControllerState*)ptr;
    if (!state) return;
    memset(state, 0, sizeof(*state));
    snprintf(state->model.current_branch, sizeof(state->model.current_branch), "%s", "unknown");
    state->tree.needs_refresh = true;
}

static void git_panel_destroy_controller_state(void* ptr) {
    GitPanelControllerState* state = (GitPanelControllerState*)ptr;
    if (!state) return;

    if (tree_select_all_visual_active_for(state->tree.tree)) {
        clearTreeSelectAllVisual();
    }
    if (state->tree.tree) {
        freeGitTree(state->tree.tree);
        state->tree.tree = NULL;
    }
    for (int i = 0; i < state->tree.expand_count; ++i) {
        free(state->tree.expand_cache[i].key);
        state->tree.expand_cache[i].key = NULL;
    }
    free(state->tree.expand_cache);
    state->tree.expand_cache = NULL;
    state->tree.expand_count = 0;
    state->tree.expand_cap = 0;
    if (state->model.log_pipe) {
        pclose(state->model.log_pipe);
        state->model.log_pipe = NULL;
    }
    free(state->model.logs);
    state->model.logs = NULL;
    state->model.log_count = 0;
    state->model.log_capacity = 0;
    state->model.log_loading = false;
    state->model.log_parse_error = false;

    free(state);
}

static GitPanelControllerState* git_panel_state(void) {
    return (GitPanelControllerState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_GIT,
        sizeof(GitPanelControllerState),
        git_panel_init_controller_state,
        git_panel_destroy_controller_state,
        &g_gitPanelBootstrapState,
        &g_gitPanelBootstrapInitialized
    );
}

#define gitFiles (git_panel_state()->model.files)
#define gitFileCount (git_panel_state()->model.file_count)
#define currentGitBranch (git_panel_state()->model.current_branch)
#define gitLogEntries (git_panel_state()->model.logs)
#define gitLogCount (git_panel_state()->model.log_count)
#define gitLogCapacity (git_panel_state()->model.log_capacity)
#define gitLogPipe (git_panel_state()->model.log_pipe)
#define gitLogLoading (git_panel_state()->model.log_loading)
#define gitLogParseError (git_panel_state()->model.log_parse_error)

#define g_topStripLayout (git_panel_state()->ui.top_strip_layout)
#define g_messageFocused (git_panel_state()->ui.message_focused)
#define g_commitMessage (git_panel_state()->ui.commit_message)
#define g_commitCursor (git_panel_state()->ui.commit_cursor)
#define g_statusText (git_panel_state()->ui.status_text)

#define g_gitTree (git_panel_state()->tree.tree)
#define g_needsRefresh (git_panel_state()->tree.needs_refresh)
#define g_gitScroll (git_panel_state()->tree.scroll)
#define g_gitScrollInit (git_panel_state()->tree.scroll_init)
#define g_gitScrollTrack (git_panel_state()->tree.scroll_track)
#define g_gitScrollThumb (git_panel_state()->tree.scroll_thumb)
#define g_expandCache (git_panel_state()->tree.expand_cache)
#define g_expandCount (git_panel_state()->tree.expand_count)
#define g_expandCap (git_panel_state()->tree.expand_cap)

#define g_lastStatusHash (git_panel_state()->watcher.last_status_hash)
#define g_lastWorkspacePath (git_panel_state()->watcher.last_workspace_path)

static bool git_tree_node_tracks_expansion(const UITreeNode* node) {
    if (!node) return false;
    if (!(node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION)) return false;
    // Track stable structural sections only; commit rows use transient hashes.
    if (node->fullPath && node->fullPath[0]) return false;
    return true;
}

static bool git_panel_tree_expansion_cache_get(const char* key, bool* outExpanded) {
    if (!key || !key[0]) return false;
    for (int i = 0; i < g_expandCount; ++i) {
        if (g_expandCache[i].key && strcmp(g_expandCache[i].key, key) == 0) {
            if (outExpanded) *outExpanded = g_expandCache[i].expanded;
            return true;
        }
    }
    return false;
}

static void git_panel_tree_expansion_cache_set(const char* key, bool expanded) {
    if (!key || !key[0]) return;
    for (int i = 0; i < g_expandCount; ++i) {
        if (g_expandCache[i].key && strcmp(g_expandCache[i].key, key) == 0) {
            g_expandCache[i].expanded = expanded;
            return;
        }
    }

    if (g_expandCount >= g_expandCap) {
        int newCap = g_expandCap > 0 ? (g_expandCap * 2) : 32;
        GitPanelExpandCacheEntry* grown =
            (GitPanelExpandCacheEntry*)realloc(g_expandCache, (size_t)newCap * sizeof(*grown));
        if (!grown) return;
        g_expandCache = grown;
        g_expandCap = newCap;
    }

    char* copied = strdup(key);
    if (!copied) return;

    g_expandCache[g_expandCount].key = copied;
    g_expandCache[g_expandCount].expanded = expanded;
    g_expandCount++;
}

static void git_panel_tree_expansion_cache_clear(void) {
    for (int i = 0; i < g_expandCount; ++i) {
        free(g_expandCache[i].key);
        g_expandCache[i].key = NULL;
    }
    free(g_expandCache);
    g_expandCache = NULL;
    g_expandCount = 0;
    g_expandCap = 0;
}

static bool git_panel_tree_expansion_key(const UITreeNode* node,
                                         const char* parentKey,
                                         char* out,
                                         size_t outCap) {
    if (!node || !out || outCap == 0) return false;
    const char* source = NULL;
    const char* prefix = NULL;

    if (node->fullPath && node->fullPath[0]) {
        source = node->fullPath;
        prefix = "path";
    } else if (node->label && node->label[0]) {
        source = node->label;
        prefix = "label";
    } else {
        source = "unknown";
        prefix = "node";
    }

    int wrote = 0;
    if (parentKey && parentKey[0]) {
        wrote = snprintf(out, outCap, "%s/%s:%s", parentKey, prefix, source);
    } else {
        wrote = snprintf(out, outCap, "%s:%s", prefix, source);
    }
    return wrote > 0 && (size_t)wrote < outCap;
}

static void git_panel_capture_tree_expansion_recursive(const UITreeNode* node, const char* parentKey) {
    if (!node) return;

    char key[2048] = {0};
    const char* nextParent = parentKey;
    if (git_tree_node_tracks_expansion(node) &&
        git_panel_tree_expansion_key(node, parentKey, key, sizeof(key))) {
        git_panel_tree_expansion_cache_set(key, node->isExpanded);
        nextParent = key;
    }

    for (int i = 0; i < node->childCount; ++i) {
        git_panel_capture_tree_expansion_recursive(node->children[i], nextParent);
    }
}

static void git_panel_apply_tree_expansion_recursive(UITreeNode* node, const char* parentKey) {
    if (!node) return;

    char key[2048] = {0};
    const char* nextParent = parentKey;
    if (git_tree_node_tracks_expansion(node) &&
        git_panel_tree_expansion_key(node, parentKey, key, sizeof(key))) {
        bool expanded = false;
        if (git_panel_tree_expansion_cache_get(key, &expanded)) {
            node->isExpanded = expanded;
        }
        nextParent = key;
    }

    for (int i = 0; i < node->childCount; ++i) {
        git_panel_apply_tree_expansion_recursive(node->children[i], nextParent);
    }
}

static void git_panel_capture_tree_expansion_state(void) {
    if (!g_gitTree) return;
    git_panel_capture_tree_expansion_recursive(g_gitTree, NULL);
}

static void git_panel_apply_tree_expansion_state(UITreeNode* root) {
    if (!root) return;
    git_panel_apply_tree_expansion_recursive(root, NULL);
}

static void git_panel_reset_tree_expansion_state(void) {
    git_panel_tree_expansion_cache_clear();
}

static UIPanelTextEditBuffer git_panel_message_buffer(void) {
    UIPanelTextEditBuffer buffer = {
        g_commitMessage,
        (int)sizeof(g_commitMessage),
        &g_commitCursor
    };
    return buffer;
}

static uint64_t fnv1a64_bytes(const unsigned char* data, size_t len, uint64_t seed) {
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
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

static bool git_panel_is_metadata_path_ignored(const char* path) {
    if (!path || !path[0]) return false;
    return strcmp(path, "ide_files") == 0 ||
           strncmp(path, "ide_files/", 10) == 0;
}

static void git_panel_log_close_stream(void) {
    if (!gitLogPipe) return;
    pclose(gitLogPipe);
    gitLogPipe = NULL;
    gitLogLoading = false;
}

static void git_panel_log_reset_entries(void) {
    gitLogCount = 0;
    gitLogParseError = false;
}

static bool git_panel_log_ensure_capacity(int desiredCount) {
    if (desiredCount <= gitLogCapacity) return true;
    int nextCap = gitLogCapacity > 0 ? gitLogCapacity : 256;
    while (nextCap < desiredCount) {
        if (nextCap > 1000000) return false;
        nextCap *= 2;
    }
    GitLogEntry* grown = (GitLogEntry*)realloc(gitLogEntries, (size_t)nextCap * sizeof(*grown));
    if (!grown) return false;
    gitLogEntries = grown;
    gitLogCapacity = nextCap;
    return true;
}

static int git_panel_log_batch_size(void) {
    const int defaultBatch = 200;
    const char* env = getenv("IDE_GIT_LOG_PAGE_SIZE");
    if (!env || !env[0]) return defaultBatch;
    char* end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env || v < 20 || v > 5000) return defaultBatch;
    return (int)v;
}

static int git_panel_log_max_commits_env(void) {
    const char* env = getenv("IDE_GIT_LOG_MAX_COMMITS");
    if (!env || !env[0]) return 0;
    char* end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env || v <= 0 || v > 2000000) return 0;
    return (int)v;
}

static bool git_panel_log_parse_line(char* line, GitLogEntry* out) {
    if (!line || !out) return false;
    char* nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    if (!line[0]) return false;

    char* f1 = strchr(line, '\x1f');
    if (!f1) return false;
    *f1 = '\0';
    char* subject = f1 + 1;

    char* f2 = strchr(subject, '\x1f');
    if (!f2) return false;
    *f2 = '\0';
    char* author = f2 + 1;

    char* f3 = strchr(author, '\x1f');
    if (!f3) return false;
    *f3 = '\0';
    char* date = f3 + 1;

    snprintf(out->hash, sizeof(out->hash), "%s", line);
    snprintf(out->message, sizeof(out->message), "%s", subject);
    snprintf(out->author, sizeof(out->author), "%s", author);
    snprintf(out->date, sizeof(out->date), "%s", date);
    return true;
}

static bool git_panel_log_start_stream(int requestedMaxCommits) {
    git_panel_log_close_stream();
    git_panel_log_reset_entries();

    if (!projectPath[0]) return false;

    int maxCommits = requestedMaxCommits > 0 ? requestedMaxCommits : git_panel_log_max_commits_env();
    char cmd[2048];
    if (maxCommits > 0) {
        snprintf(cmd,
                 sizeof(cmd),
                 "cd \"%s\" && git log -n %d --date=short --pretty=format:\"%%h%%x1f%%s%%x1f%%an%%x1f%%ad\"",
                 projectPath,
                 maxCommits);
    } else {
        snprintf(cmd,
                 sizeof(cmd),
                 "cd \"%s\" && git log --date=short --pretty=format:\"%%h%%x1f%%s%%x1f%%an%%x1f%%ad\"",
                 projectPath);
    }

    gitLogPipe = popen(cmd, "r");
    if (!gitLogPipe) {
        fprintf(stderr, "[GitError] Failed to start git log stream\n");
        gitLogLoading = false;
        return false;
    }

    gitLogLoading = true;
    return true;
}

static int git_panel_log_pump_stream(int maxLines) {
    if (!gitLogPipe || maxLines <= 0) return 0;

    int appended = 0;
    char line[GIT_LOG_LINE_MAX];
    while (appended < maxLines && fgets(line, sizeof(line), gitLogPipe)) {
        GitLogEntry parsed = {0};
        if (!git_panel_log_parse_line(line, &parsed)) {
            gitLogParseError = true;
            continue;
        }

        if (!git_panel_log_ensure_capacity(gitLogCount + 1)) {
            fprintf(stderr, "[GitError] Unable to grow git log buffer\n");
            git_panel_log_close_stream();
            break;
        }

        gitLogEntries[gitLogCount++] = parsed;
        appended++;
    }

    if (feof(gitLogPipe)) {
        git_panel_log_close_stream();
    }

    return appended;
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

const char* git_panel_branch_name(void) {
    return currentGitBranch;
}

int git_panel_file_count(void) {
    return gitFileCount;
}

const GitFileEntry* git_panel_file_at(int index) {
    if (index < 0 || index >= gitFileCount) return NULL;
    return &gitFiles[index];
}

int git_panel_log_count(void) {
    return gitLogCount;
}

const GitLogEntry* git_panel_log_at(int index) {
    if (index < 0 || index >= gitLogCount) return NULL;
    return &gitLogEntries[index];
}

bool git_panel_log_is_loading(void) {
    return gitLogLoading;
}

void git_panel_prepare_for_render(void) {
    if (g_needsRefresh) {
        refreshGitStatus();
        refreshGitLog(0);
        if (g_gitTree) {
            git_panel_capture_tree_expansion_state();
            if (tree_select_all_visual_active_for(g_gitTree)) {
                clearTreeSelectAllVisual();
            }
            freeGitTree(g_gitTree);
            g_gitTree = NULL;
        }
        g_needsRefresh = false;
    }

    if (gitLogLoading) {
        int appended = git_panel_log_pump_stream(git_panel_log_batch_size());
        if (appended > 0 || !gitLogLoading) {
            if (g_gitTree) {
                git_panel_capture_tree_expansion_state();
                if (tree_select_all_visual_active_for(g_gitTree)) {
                    clearTreeSelectAllVisual();
                }
                freeGitTree(g_gitTree);
                g_gitTree = NULL;
            }
        }
    }

    if (!g_gitTree) {
        g_gitTree = convertGitModelToTree();
        git_panel_apply_tree_expansion_state(g_gitTree);
    }

    if (!g_gitScrollInit) {
        scroll_state_init(&g_gitScroll, NULL);
        g_gitScrollInit = true;
    }
    g_gitScroll.line_height_px = (float)IDE_UI_DENSE_ROW_HEIGHT;
}

void resetGitTree(void) {
    git_panel_capture_tree_expansion_state();
    if (tree_select_all_visual_active_for(g_gitTree)) {
        clearTreeSelectAllVisual();
    }
    if (g_gitTree) {
        freeGitTree(g_gitTree);
        g_gitTree = NULL;
    }
    git_panel_log_close_stream();
    g_needsRefresh = true;
}

UITreeNode* git_panel_tree(void) {
    return g_gitTree;
}

PaneScrollState* git_panel_scroll(void) {
    if (!g_gitScrollInit) {
        scroll_state_init(&g_gitScroll, NULL);
        g_gitScrollInit = true;
    }
    g_gitScroll.line_height_px = (float)IDE_UI_DENSE_ROW_HEIGHT;
    return &g_gitScroll;
}

SDL_Rect* git_panel_scroll_track(void) {
    return &g_gitScrollTrack;
}

SDL_Rect* git_panel_scroll_thumb(void) {
    return &g_gitScrollThumb;
}

UIDoubleClickTracker* git_panel_tree_double_click_tracker(void) {
    return &git_panel_state()->tree.double_click_tracker;
}

void resetGitStatusWatcher(void) {
    g_lastStatusHash = 0;
    g_lastWorkspacePath[0] = '\0';
    git_panel_reset_tree_expansion_state();
}

void git_panel_set_top_strip_layout(UIPanelThreeSegmentStripLayout layout) {
    g_topStripLayout = layout;
}
UIPanelThreeSegmentStripLayout git_panel_get_top_strip_layout(void) { return g_topStripLayout; }
void git_panel_set_message_focus(bool focused) { (void)ui_text_input_focus_set(&g_messageFocused, focused); }
bool git_panel_is_message_focused(void) { return g_messageFocused; }
const char* git_panel_get_message(void) { return g_commitMessage; }
int git_panel_get_message_cursor(void) { return g_commitCursor; }
void git_panel_set_message(const char* text) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_set_text(&buffer, text);
}
bool git_panel_handle_message_text_input(const SDL_Event* event) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    return ui_panel_text_edit_handle_text_input(&buffer, event);
}
bool git_panel_handle_message_edit_key(SDL_Keycode key) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    return ui_panel_text_edit_handle_keydown(&buffer, key);
}
const char* git_panel_get_status_text(void) { return g_statusText; }
int git_panel_content_top(const UIPane* pane) {
    if (!pane) return 0;

    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int controlsY = pane->y + d.controls_top - 1;
    const int controlsHeight = d.button_h;
    const int metadataGap = d.row_gap;
    const int metadataLineGap = d.info_line_gap;
    const int branchY = controlsY + controlsHeight + metadataGap;

    int infoLines = g_statusText[0] ? 2 : 1;
    int bottomPadding = g_statusText[0] ? 10 : 14;
    int minContentTop = branchY + (infoLines * metadataLineGap) + bottomPadding;
    ToolPanelHeaderMetrics metrics = {
        .controls_y = controlsY,
        .controls_h = controlsHeight,
        .info_start_y = branchY,
        .info_line_gap = metadataLineGap,
        .info_line_count = infoLines,
        .bottom_padding = bottomPadding,
        .min_content_top = minContentTop
    };
    return tool_panel_compute_content_top(&metrics);
}

int git_panel_tree_content_top(const UIPane* pane) {
    return git_panel_content_top(pane) + tool_panel_content_inset_default();
}

void git_panel_tree_viewport(const UIPane* pane, UIPane* out_pane) {
    if (!pane || !out_pane) return;
    *out_pane = *pane;
    out_pane->y = git_panel_tree_content_top(pane) - tree_panel_content_offset_y();
    out_pane->h = (pane->y + pane->h) - out_pane->y;
    if (out_pane->h < 0) out_pane->h = 0;
}

void git_panel_insert_text(const char* text) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_insert(&buffer, text);
}

void git_panel_backspace(void) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_backspace(&buffer);
}

void git_panel_delete(void) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_delete(&buffer);
}

void git_panel_move_cursor_left(void) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_move_left(&buffer);
}
void git_panel_move_cursor_right(void) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_move_right(&buffer);
}
void git_panel_move_cursor_home(void) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_move_home(&buffer);
}
void git_panel_move_cursor_end(void) {
    UIPanelTextEditBuffer buffer = git_panel_message_buffer();
    (void)ui_panel_text_edit_move_end(&buffer);
}

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
        UIPanelTextEditBuffer buffer = git_panel_message_buffer();
        (void)ui_panel_text_edit_clear(&buffer);
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

        char statusCode[3];
        strncpy(statusCode, line, 2);
        statusCode[2] = '\0';

        char* filePath = line + 3;
        filePath[strcspn(filePath, "\n")] = 0;
        if (git_panel_is_metadata_path_ignored(filePath)) {
            continue;
        }

        GitFileEntry* f = &gitFiles[gitFileCount++];

        strncpy(f->path, filePath, sizeof(f->path) - 1);
        f->path[sizeof(f->path) - 1] = '\0';

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

    pclose(pipe);
}

void refreshGitLog(int maxEntries) {
    if (!git_panel_log_start_stream(maxEntries)) {
        return;
    }
    (void)git_panel_log_pump_stream(git_panel_log_batch_size());
}

void pollGitStatusWatcher(void) {
    if (!projectPath[0]) return;

    if (strncmp(g_lastWorkspacePath, projectPath, sizeof(g_lastWorkspacePath)) != 0) {
        snprintf(g_lastWorkspacePath, sizeof(g_lastWorkspacePath), "%s", projectPath);
        g_lastWorkspacePath[sizeof(g_lastWorkspacePath) - 1] = '\0';
        g_lastStatusHash = 0;
        git_panel_reset_tree_expansion_state();
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
        char lineCopy[1024];
        snprintf(lineCopy, sizeof(lineCopy), "%s", buf);
        lineCopy[sizeof(lineCopy) - 1] = '\0';
        lineCopy[strcspn(lineCopy, "\r\n")] = '\0';

        bool include_line = true;
        if (!(lineCopy[0] == '#' && lineCopy[1] == '#')) {
            if (strlen(lineCopy) < 4) {
                include_line = false;
            } else {
                const char* path = lineCopy + 3;
                if (git_panel_is_metadata_path_ignored(path)) {
                    include_line = false;
                }
            }
        }

        if (!include_line) continue;
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
