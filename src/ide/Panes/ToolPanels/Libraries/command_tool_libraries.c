#include "ide/Panes/ToolPanels/Libraries/command_tool_libraries.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/include_path_resolver.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "ide/Panes/Editor/editor_view.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

static void clear_analysis_cache(const char* root) {
    if (!root || !*root) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files", root);
    DIR* dir = opendir(path);
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (strstr(ent->d_name, "analysis") || strstr(ent->d_name, "library_index") ||
            strstr(ent->d_name, "build_flags") || strstr(ent->d_name, "cache_meta") ||
            strstr(ent->d_name, "include_graph") || strcmp(ent->d_name, "index.json") == 0) {
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
            unlink(full);
        }
    }
    closedir(dir);
    analysis_status_set_has_cache(false);
    analysis_status_set_last_error(NULL);
}

void handleLibrariesCommand(UIPane* pane, InputCommandMetadata meta) {
    switch (meta.cmd) {
        case COMMAND_REFRESH_LIBRARY:
        {
            printf("[LibraryPanelCommand] Refreshing library index async...\n");
            analysis_scheduler_request(ANALYSIS_REASON_LIBRARY_PANEL_REFRESH, true);
            break;
        }
        case COMMAND_CLEAR_ANALYSIS_CACHE:
            printf("[LibraryPanelCommand] Clearing analysis cache...\n");
            clear_analysis_cache(projectPath);
            analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
            analysis_scheduler_request(ANALYSIS_REASON_MANUAL_REFRESH, true);
            break;

        default:
            printf("[LibraryPanelCommand] Unhandled command: %d\n", meta.cmd);
            break;
    }
}
