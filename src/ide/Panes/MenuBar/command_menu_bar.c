#include "command_menu_bar.h"
#include "core/CommandBus/command_metadata.h"
#include "core/CommandBus/save_queue.h"

#include "core/BuildSystem/build_system.h"
#include "core/BuildSystem/run_build.h"

#include "app/GlobalInfo/project.h" 
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "core/InputManager/UserInput/rename_access.h"
#include "core/Watcher/file_watcher.h"
#include "core/Analysis/analysis_scheduler.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/UI/ui_state.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

typedef struct WorkspaceSelectionContext {
    char resolvedPath[PATH_MAX];
    bool preserveOpenFiles;
} WorkspaceSelectionContext;

static WorkspaceSelectionContext workspaceSelectionContext = {0};

static void trimWhitespace(char* buffer) {
    if (!buffer) return;

    char* start = buffer;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    char* end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    size_t length = (size_t)(end - start);
    if (start != buffer) {
        memmove(buffer, start, length);
    }
    buffer[length] = '\0';
}

static bool normalizePathInput(const char* input, char* output, size_t outputSize) {
    if (!input || !output || outputSize == 0) return false;

    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s", input) >= (int)sizeof(temp)) {
        return false;
    }

    trimWhitespace(temp);
    if (temp[0] == '\0') {
        return false;
    }

    if (temp[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) return false;
        if (snprintf(output, outputSize, "%s%s", home, temp + 1) >= (int)outputSize) {
            return false;
        }
        return true;
    }

    if (snprintf(output, outputSize, "%s", temp) >= (int)outputSize) {
        return false;
    }
    return true;
}

static bool ensureDirectoryRecursive(const char* path) {
    if (!path || !*path) return false;

    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s", path) >= (int)sizeof(temp)) {
        return false;
    }

    size_t len = strlen(temp);
    while (len > 1 && temp[len - 1] == '/') {
        temp[--len] = '\0';
    }

    for (char* p = temp + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
            return false;
        }
        *p = '/';
    }

    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool resolveWorkspaceCandidate(const char* candidate, char* outAbs, size_t outAbsSize) {
    if (!candidate || !outAbs || outAbsSize == 0) return false;

    char normalized[PATH_MAX];
    if (!normalizePathInput(candidate, normalized, sizeof(normalized))) {
        return false;
    }

    if (normalized[0] == '/') {
        if (snprintf(outAbs, outAbsSize, "%s", normalized) >= (int)outAbsSize) return false;
        return true;
    }

    const char* base = getWorkspacePath();
    if (!base || !*base) {
        base = projectPath;
    }

    if (base && *base) {
        if (snprintf(outAbs, outAbsSize, "%s/%s", base, normalized) >= (int)outAbsSize) return false;
        return true;
    }

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) return false;
    if (snprintf(outAbs, outAbsSize, "%s/%s", cwd, normalized) >= (int)outAbsSize) return false;
    return true;
}

static bool validateWorkspacePath(const char* candidate, void* context) {
    WorkspaceSelectionContext* ctx = (WorkspaceSelectionContext*)context;
    if (!ctx) return false;
    ctx->preserveOpenFiles = RENAME->submitWithShift;

    char expanded[PATH_MAX];
    if (!resolveWorkspaceCandidate(candidate, expanded, sizeof(expanded))) {
        setRenameErrorMessage("Path is too long or empty");
        return false;
    }

    struct stat st;
    if (stat(expanded, &st) != 0) {
        if (errno == ENOENT) {
            if (!ensureDirectoryRecursive(expanded)) {
                if (errno == EACCES) {
                    setRenameErrorMessage("Permission denied creating directory");
                } else {
                    setRenameErrorMessage("Unable to create directory path");
                }
                return false;
            }
            if (stat(expanded, &st) != 0) {
                setRenameErrorMessage("Unable to access created path");
                return false;
            }
        } else if (errno == EACCES) {
            setRenameErrorMessage("Permission denied");
            return false;
        } else {
            setRenameErrorMessage("Unable to access path");
            return false;
        }
    }

    if (!S_ISDIR(st.st_mode)) {
        setRenameErrorMessage("Path is not a directory");
        return false;
    }

    char resolved[PATH_MAX];
    if (realpath(expanded, resolved)) {
        snprintf(ctx->resolvedPath, sizeof(ctx->resolvedPath), "%s", resolved);
    } else {
        snprintf(ctx->resolvedPath, sizeof(ctx->resolvedPath), "%s", expanded);
    }

    setRenameErrorMessage(NULL);
    return true;
}

static bool path_is_within_workspace(const char* filePath, const char* workspaceRoot) {
    if (!filePath || !workspaceRoot || !workspaceRoot[0]) return false;

    size_t rootLen = strlen(workspaceRoot);
    if (strncmp(filePath, workspaceRoot, rootLen) != 0) return false;
    if (filePath[rootLen] == '\0') return true;
    return filePath[rootLen] == '/';
}

static void close_tabs_outside_workspace(EditorView* view, const char* workspaceRoot) {
    if (!view || !workspaceRoot || !workspaceRoot[0]) return;

    if (view->type == VIEW_LEAF) {
        for (int i = view->fileCount - 1; i >= 0; --i) {
            OpenFile* file = view->openFiles[i];
            const char* path = (file && file->filePath) ? file->filePath : NULL;
            if (!path_is_within_workspace(path, workspaceRoot)) {
                closeTab(view, i);
            }
        }
        return;
    }

    close_tabs_outside_workspace(view->childA, workspaceRoot);
    close_tabs_outside_workspace(view->childB, workspaceRoot);
}

static void applyWorkspaceSelection(const char* oldValue, const char* newValue, void* context) {
    (void)oldValue;
    WorkspaceSelectionContext* ctx = (WorkspaceSelectionContext*)context;
    const char* finalPath = (ctx && ctx->resolvedPath[0] != '\0') ? ctx->resolvedPath : newValue;

    if (!finalPath) return;

    IDECoreState* core = getCoreState();
    if (core && core->persistentEditorView && (!ctx || !ctx->preserveOpenFiles)) {
        close_tabs_outside_workspace(core->persistentEditorView, finalPath);
        if (!core->activeEditorView) {
            EditorView* next = findNextLeaf(core->persistentEditorView);
            if (next) setActiveEditorView(next);
        }
    }

    setWorkspacePath(finalPath);
    snprintf(projectPath, sizeof(projectPath), "%s", getWorkspacePath());
    setWorkspaceWatchPath(projectPath);
    suppressWorkspaceWatchRefreshForMs(4000);
    setRunTargetPath(NULL);
    saveRunTargetPreference(NULL);
    saveWorkspacePreference(projectPath);
    queueProjectRefresh(ANALYSIS_REASON_WORKSPACE_RELOAD);
    printf("[Workspace] Reloading workspace: %s\n", projectPath);
}

static void promptWorkspaceSelection(void) {
    workspaceSelectionContext.resolvedPath[0] = '\0';
    workspaceSelectionContext.preserveOpenFiles = false;
    const char* current = getWorkspacePath();
    beginRenameWithPrompt(
        "Workspace Directory:",
        "Provide a valid directory path",
        current && current[0] ? current : projectPath,
        applyWorkspaceSelection,
        validateWorkspacePath,
        &workspaceSelectionContext,
        true
    );
}

void handleMenuBarCommand(UIPane* pane, InputCommandMetadata meta) {
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;

    switch (meta.cmd) {
        case COMMAND_BUILD_PROJECT:
	    triggerBuild();
            break;

        case COMMAND_RUN_EXECUTABLE: {
            char exePath[PATH_MAX];
            exePath[0] = '\0';

            const char* resolvedPath = NULL;
            const char* runTarget = getRunTargetPath();
            if (runTarget && runTarget[0]) {
                resolvedPath = runTarget;
            } else {
                BuildOutputPanelState* buildState = getBuildOutputPanelState();
                if (buildState && buildState->selectedRunTarget && buildState->selectedRunTarget->fullPath) {
                    resolvedPath = buildState->selectedRunTarget->fullPath;
                } else {
                    const char* lastBuilt = getLastBuiltExecutablePath();
                    if (lastBuilt && lastBuilt[0]) {
                        resolvedPath = lastBuilt;
                    } else {
                        snprintf(exePath, sizeof(exePath), "%s/BuildOutputs/last_build/app", projectPath);
                        resolvedPath = exePath;
                    }
                }
            }

            if (resolvedPath != exePath) {
                snprintf(exePath, sizeof(exePath), "%s", resolvedPath);
            }

            runExecutableAndStreamOutput(exePath);
            break;
        }

        case COMMAND_DEBUG_EXECUTABLE:
            printf("[MenuBarCommand] Debug\n");
            break;

	case COMMAND_TOGGLE_CONTROL_PANEL: {
	    UIState* ui = getUIState();
	    ui->controlPanelVisible = !ui->controlPanelVisible;
	    printf("[MenuBarCommand] Toggle Control Panel: %s\n",
	           ui->controlPanelVisible ? "VISIBLE" : "HIDDEN");
	    break;
	}

	case COMMAND_TOGGLE_TOOL_PANEL: {
	    UIState* ui = getUIState();
	    ui->toolPanelVisible = !ui->toolPanelVisible;
	    printf("[MenuBarCommand] Toggle Tool Panel: %s\n",
	           ui->toolPanelVisible ? "VISIBLE" : "HIDDEN");
	    break;
	}
	    	    

        case COMMAND_OPEN_BUILD_LOG:     // or new Command
        {
            UIState* ui = getUIState();
            ui->controlPanelVisible = !ui->controlPanelVisible;
            printf("[MenuBarCommand] Toggle Control Panel: %s\n",
                   ui->controlPanelVisible ? "VISIBLE" : "HIDDEN");
            break;
        }

        case COMMAND_OPEN_FILE:
            printf("[MenuBarCommand] Load (unimplemented)\n");
            break;

        case COMMAND_CHOOSE_WORKSPACE:
            promptWorkspaceSelection();
            break;

        case COMMAND_RELOAD_WORKSPACE:
            queueProjectRefresh(ANALYSIS_REASON_WORKSPACE_RELOAD);
            printf("[Workspace] Queued workspace refresh\n");
            break;

        case COMMAND_SAVE_FILE:
            if (view && view->type == VIEW_LEAF &&
                view->activeTab >= 0 && view->activeTab < view->fileCount) {

                OpenFile* file = view->openFiles[view->activeTab];
                if (file && file->isModified) {
                    enqueueSave(file);
                    printf("[MenuBarCommand] Save queued: %s\n", file->filePath);
                } else {
                    printf("[MenuBarCommand] Save skipped — not modified\n");
                }
            }
            break;

        default:
            printf("[MenuBarCommand] Unhandled: %d\n", meta.cmd);
            break;
    }
}



void initMenuBarCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleMenuBarCommand;
    }
}
