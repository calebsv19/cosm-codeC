#include "build_config.h"
#include "system_control.h"
#include "core_state.h"

#include "project.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"


#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Popup/popup_system.h"
#include "ide/Panes/ToolPanels/Tasks/task_json_helper.h"
#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "ide/Plugin/plugin_interface.h"
#include "ide/UI/ui_state.h"


#include "core/Watcher/file_watcher.h"
#include "core/BuildSystem/build_system.h"
#include "core/Diagnostics/diagnostics_engine.h"

#include "Parser/language_parser.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>



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

static void loadTestProject() {
    const char* testPath = "/Users/calebsv16/Desktop/Project";
    printf("[Project] Loading test project from: %s\n", testPath);
        
    strncpy(projectPath, testPath, sizeof(projectPath));
    projectPath[sizeof(projectPath) - 1] = '\0';
    
    projectRoot = loadProjectDirectory(projectPath);
    if (projectRoot) {
        printf("[Project] Project loaded successfully.\n");
    } else {
        fprintf(stderr, "[Project] Failed to load project.\n");
    }
        
    // 🧼 Clear old task roots before attempting to load
    for (int i = 0; i < MAX_TASK_ROOTS; i++) {
        taskRoots[i] = NULL;
    }
    taskRootCount = 0;
        
    // 📦 Load task tree from file
    char taskPath[1024];
    snprintf(taskPath, sizeof(taskPath), "%s/task_tree.json", projectPath);
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

    initPluginSystem();

    initializeUIPanesIfNeeded();
    initFileWatcher();

    initProjectPaths();
    loadTestProject();

    initLibrariesPanel();

    return true;
}

void shutdownSystem(UIPane** panes, int paneCount) {
    // === Destroy UI panes ===
    for (int i = 0; i < paneCount; i++) {
        destroyPane(panes[i]);
    }

    shutdownRenderPipeline();
    unloadAllPlugins();

    // === Save current task tree to JSON ===
    char taskPath[1024];
    snprintf(taskPath, sizeof(taskPath), "%s/task_tree.json", projectPath);
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
