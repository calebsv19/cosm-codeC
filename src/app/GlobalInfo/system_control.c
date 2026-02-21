#include "build_config.h"
#include "system_control.h"
#include "core_state.h"

#include "project.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"


#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Popup/popup_system.h"
#include "ide/Panes/ToolPanels/Tasks/task_json_helper.h"
#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Plugin/plugin_interface.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/Terminal/terminal.h"

#include "workspace_prefs.h"
#include "core/Watcher/file_watcher.h"
#include "core/BuildSystem/build_system.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/project_scan.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_snapshot.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/include_graph.h"
#include "core/Analysis/analysis_status.h"
#include "core/Ipc/ide_ipc_server.h"
#include "core/Ipc/ide_ipc_edit_apply.h"
#include "ide/Panes/Editor/Commands/editor_commands.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_session.h"
#include "ide/Panes/PaneInfo/pane.h"

#include "Parser/language_parser.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <json-c/json.h>



bool printTaskNodes = false;


static void printTaskNode(TaskNode* node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) printf("  ");
    printf("• %s (completed: %s, group: %s, expanded: %s)\n",
        node->label,
        node->completed ? "yes" : "no",
        node->isGroup ? "yes" : "no",
        node->isExpanded ? "yes" : "no"
    );

    for (int i = 0; i < node->childCount; i++) {
        printTaskNode(node->children[i], depth + 1);
    }
}

static bool ide_ipc_open_from_ui(const char* path,
                                 int line,
                                 int col,
                                 char* error_out,
                                 size_t error_out_cap,
                                 void* userdata) {
    (void)userdata;
    IDECoreState* core = getCoreState();
    if (!core) {
        if (error_out && error_out_cap) snprintf(error_out, error_out_cap, "Core state unavailable");
        return false;
    }

    EditorView* view = core->activeEditorView;
    if (!view) {
        view = core->persistentEditorView;
        if (view && view->type != VIEW_LEAF) {
            view = findNextLeaf(view);
        }
        if (view) {
            setActiveEditorView(view);
        }
    }
    if (!view) {
        if (error_out && error_out_cap) snprintf(error_out, error_out_cap, "No editor view available");
        return false;
    }

    if (!editor_jump_to(view, path, line, col)) {
        if (error_out && error_out_cap) {
            snprintf(error_out, error_out_cap, "Failed to open target: %s", path ? path : "");
        }
        return false;
    }

    if (core->editorPane) {
        core->focusedPane = core->editorPane;
    }
    return true;
}

static bool ide_ipc_edit_apply_from_ui(const char* diff_text,
                                       char* error_out,
                                       size_t error_out_cap,
                                       void* userdata,
                                       json_object** result_out) {
    (void)userdata;
    return ide_ipc_apply_unified_diff(projectPath, diff_text, result_out, error_out, error_out_cap);
}

static bool pathIsDirectory(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }

    return S_ISDIR(st.st_mode);
}

static void ensureIdeFilesDir(const char* root) {
    if (!root || !*root) return;
    char dirPath[1024];
    snprintf(dirPath, sizeof(dirPath), "%s/ide_files", root);
    struct stat st;
    if (stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode)) return;
    mkdir(dirPath, 0755);
}

static const char* getDefaultWorkspacePath(void) {
    return "/Users/calebsv16/Desktop/Project";
}

static void loadInitialWorkspace(void) {
    const char* requestedPath = getWorkspacePath();
    const char* defaultPath = getDefaultWorkspacePath();
    const char* finalPath = NULL;
    bool pathChanged = false;

    if (pathIsDirectory(requestedPath)) {
        finalPath = requestedPath;
    } else {
        if (requestedPath && requestedPath[0]) {
            fprintf(stderr, "[Workspace] Stored workspace unavailable: %s\n", requestedPath);
        }

        if (pathIsDirectory(defaultPath)) {
            finalPath = defaultPath;
            pathChanged = true;
        }
    }

    if (!finalPath) {
        fprintf(stderr, "[Workspace] No valid workspace directory available.\n");
        projectRoot = NULL;
        return;
    }

    snprintf(projectPath, sizeof(projectPath), "%s", finalPath);
    ensureIdeFilesDir(projectPath);
    setWorkspacePath(projectPath);
    setWorkspaceWatchPath(projectPath);
    resetGitStatusWatcher();
    build_diagnostics_load(projectPath);
    diagnostics_load(projectPath);

    const char* savedRunTarget = loadRunTargetPreference();
    if (savedRunTarget && savedRunTarget[0]) {
        setRunTargetPath(savedRunTarget);
    } else {
        setRunTargetPath(NULL);
    }

    if (pathChanged) {
        saveWorkspacePreference(projectPath);
        if (savedRunTarget && savedRunTarget[0]) {
            saveRunTargetPreference(savedRunTarget);
        } else {
            saveRunTargetPreference(NULL);
        }
    }

    printf("[Project] Loading workspace from: %s\n", projectPath);
    projectRoot = loadProjectDirectory(projectPath);
    if (projectRoot) {
        printf("[Project] Project loaded successfully.\n");
        restoreRunTargetSelection();
    } else {
        fprintf(stderr, "[Project] Failed to load project.\n");
    }
        
    // Clear old task roots before attempting to load.
    if (!taskRoots) {
        taskRoots = calloc(MAX_TASK_ROOTS, sizeof(TaskNode*));
        if (!taskRoots) {
            fprintf(stderr, "[TaskLoad] Failed to allocate task root array.\n");
            taskRootCount = 0;
            return;
        }
    } else {
        for (int i = 0; i < taskRootCount; i++) {
            if (taskRoots[i]) {
                freeTaskTree(taskRoots[i]);
                taskRoots[i] = NULL;
            }
        }
    }
    for (int i = 0; i < MAX_TASK_ROOTS; i++) {
        taskRoots[i] = NULL;
    }
    taskRootCount = 0;
        
    // 📦 Load task tree from file
    ensureIdeFilesDir(projectPath);
    char taskPath[1024];
    snprintf(taskPath, sizeof(taskPath), "%s/ide_files/task_tree.json", projectPath);
    bool loaded = loadTaskTreeFromFile(taskPath, &taskRoots, &taskRootCount);

    if (loaded && taskRootCount > 0) {
        printf("[TaskLoad] Loaded %d root task(s):\n", taskRootCount);
        
	if (printTaskNodes){
		for (int i = 0; i < taskRootCount; i++) {
        	    printTaskNode(taskRoots[i], 0);
        	}
	}
    } else {
        printf("[TaskLoad] No valid task tree found in file. Task tree will remain empty.\n");
    }
}


bool initializeSystem() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("Current working directory: %s\n", cwd);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, " SDL not initialized! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#if USE_VULKAN
    windowFlags |= SDL_WINDOW_VULKAN;
#endif

    SDL_Window* window = SDL_CreateWindow(
        "Caleb's IDE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1600, 860,
        windowFlags
    );
    if (!window) {
        fprintf(stderr, " Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    SDL_Renderer* renderer = NULL;
#if USE_VULKAN
    renderer = calloc(1, sizeof(*renderer));
    if (!renderer) {
        fprintf(stderr, " Failed to allocate Vulkan renderer.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    VkRendererConfig rendererConfig;
    vk_renderer_config_set_defaults(&rendererConfig);
    rendererConfig.enable_validation = VK_FALSE;
    rendererConfig.clear_color[0] = 30.0f / 255.0f;
    rendererConfig.clear_color[1] = 30.0f / 255.0f;
    rendererConfig.clear_color[2] = 30.0f / 255.0f;
    rendererConfig.clear_color[3] = 1.0f;

    VkResult vkInitResult = vk_renderer_init(renderer, window, &rendererConfig);
    if (vkInitResult != VK_SUCCESS) {
        fprintf(stderr, " Vulkan renderer initialization failed: %d\n", vkInitResult);
        free(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
#else
    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, " Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
#endif

    if (!SDL_GetKeyboardFocus()) {
        printf(" SDL window not focused!\n");
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, " Failed to initialize SDL_ttf: %s\n", TTF_GetError());
#if USE_VULKAN
        vk_renderer_shutdown(renderer);
        free(renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }



    if (!initRenderPipeline()) return false;

    setRenderContext(renderer, window, STARTING_WIDTH, STARTING_HEIGHT);




    initCoreState();
    initPopupSystem();
    initDiagnosticsEngine();
    initLanguageParser();
    initBuildSystem();
    initBuildOutputPanelState();
    analysis_status_init();

    initProjectPaths();
    loadInitialWorkspace();

    IDECoreState* core = getCoreState();
    if (core) {
        EditorView* restoredRoot = NULL;
        EditorView* restoredActive = NULL;
        if (editor_session_load(projectPath, &restoredRoot, &restoredActive) && restoredRoot) {
            core->persistentEditorView = restoredRoot;
            core->activeEditorView = restoredActive ? restoredActive : findNextLeaf(restoredRoot);
            core->editorViewCount = editor_session_count_leaves(restoredRoot);
            if (core->editorViewCount <= 0) core->editorViewCount = 1;
            if (core->activeEditorView) {
                setActiveEditorView(core->activeEditorView);
            }
        }
    }

    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* buildArgs = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    bool loadedCache = analysis_cache_load_errors(projectPath, buildArgs);
    if (!loadedCache) {
        analysis_store_load(projectPath);
    }
    bool loadedSymbols = analysis_cache_load_symbols(projectPath, buildArgs);
    if (!loadedSymbols) {
        analysis_symbols_store_load(projectPath);
    }
    bool loadedTokens = analysis_cache_load_tokens(projectPath, buildArgs);
    if (!loadedTokens) {
        analysis_token_store_load(projectPath);
    }
    if (analysis_cache_load_library(projectPath, buildArgs)) {
        loadedCache = true;
    }
    include_graph_load(projectPath);
    analysis_status_set_has_cache(loadedCache || loadedSymbols || loadedTokens);

    initPluginSystem();

    initializeUIPanesIfNeeded();
    initFileWatcher();

    analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
    analysis_request_refresh();
    initAssetManagerPanel();

    if (!ide_ipc_start(projectPath)) {
        fprintf(stderr, "[IPC] Warning: failed to start IDE IPC server. idebridge will be unavailable.\n");
    } else {
        ide_ipc_set_open_handler(ide_ipc_open_from_ui, NULL);
        ide_ipc_set_edit_handler(ide_ipc_edit_apply_from_ui, NULL);
    }

    initTerminal();

    const char* workspace = getWorkspacePath();
    terminal_spawn_shell((workspace && *workspace) ? workspace : NULL, 0, 0);

    initLibrariesPanel();

    return true;
}

void shutdownSystem(UIPane** panes, int paneCount) {
    // Persist last build diagnostics for next session.
    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* buildArgs = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    build_diagnostics_save(projectPath);
    diagnostics_save(projectPath);
    analysis_store_save(projectPath);
    analysis_symbols_store_save(projectPath);
    analysis_token_store_save(projectPath);
    library_index_save(projectPath);
    BuildFlagSet tmpFlags = {0};
    gather_build_flags(projectPath, buildArgs, &tmpFlags);
    analysis_cache_save_build_flags(&tmpFlags, projectPath);
    free_build_flag_set(&tmpFlags);
    analysis_cache_save_metadata(projectPath, buildArgs);
    analysis_cache_save_symbols(projectPath);
    analysis_cache_save_tokens(projectPath);
    analysis_snapshot_refresh_and_save(projectPath);
    include_graph_save(projectPath);
    IDECoreState* core = getCoreState();
    if (core && core->persistentEditorView) {
        editor_session_save(projectPath, core->persistentEditorView, core->activeEditorView);
    }

    ide_ipc_stop();

    terminal_shutdown_shell();

    // === Destroy UI panes ===
    for (int i = 0; i < paneCount; i++) {
        destroyPane(panes[i]);
    }

    shutdownRenderPipeline();
    unloadAllPlugins();

    // === Save current task tree to JSON (in ide_files) ===
    ensureIdeFilesDir(projectPath);
    char taskPath[1024];
    snprintf(taskPath, sizeof(taskPath), "%s/ide_files/task_tree.json", projectPath);
    if (!saveTaskTreeToFile(taskPath, taskRoots, taskRootCount)) {
        fprintf(stderr, "[Shutdown] Failed to save task tree to %s\n", taskPath);
    }

    // === Free all task tree memory ===
    if (taskRoots) {
        for (int i = 0; i < taskRootCount; i++) {
            if (taskRoots[i]) {
                freeTaskTree(taskRoots[i]);
            }
        }
        free(taskRoots);
        taskRoots = NULL;
    }
    taskRootCount = 0;

    // === Clean up SDL systems ===
    freeBuildOutputPanelState();
    TTF_Quit();

    RenderContext* ctx = getRenderContext();
#if USE_VULKAN
    if (ctx->renderer) {
        vk_renderer_shutdown(ctx->renderer);
        free(ctx->renderer);
    }
#else
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
#endif
    if (ctx->window) SDL_DestroyWindow(ctx->window);

    SDL_Quit();
}
