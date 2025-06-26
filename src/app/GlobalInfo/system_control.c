#include "system_control.h"
#include "core_state.h"

#include "project.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"


#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Popup/popup_system.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "Parser/language_parser.h"
#include "core/Build/build_system.h"
#include "ide/Plugin/plugin_interface.h"
#include "core/Watcher/file_watcher.h"
#include "ide/UI/ui_state.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>



static void loadTestProject() {
    const char* testPath = "/Users/calebsv16/Desktop/CodeWork/IDE/src/Project";

    printf("[Project] Loading test project from: %s\n", testPath);

    strncpy(projectPath, testPath, sizeof(projectPath));
    projectPath[sizeof(projectPath) - 1] = '\0';  // ensure null-termination

    projectRoot = loadProjectDirectory(projectPath);

    if (projectRoot) {
        printf("[Project] Project loaded successfully.\n");
    } else {
        fprintf(stderr, "[Project] Failed to load project.\n");
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

    SDL_Window* window = SDL_CreateWindow(
        "Caleb's IDE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1600, 860,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        fprintf(stderr, " Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, " Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    if (!SDL_GetKeyboardFocus()) {
        printf(" SDL window not focused!\n");
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, " Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
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
    initPluginSystem();

    initializeUIPanesIfNeeded();
    initFileWatcher();

    initProjectPaths();
    loadTestProject();

    initLibrariesPanel();

    return true;
}

void shutdownSystem(UIPane** panes, int paneCount) {
    for (int i = 0; i < paneCount; i++) {
        destroyPane(panes[i]);
    }
     
    shutdownRenderPipeline();
    unloadAllPlugins();
    
    TTF_Quit();
    
    RenderContext* ctx = getRenderContext();
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
    if (ctx->window) SDL_DestroyWindow(ctx->window);

    
    SDL_Quit();
}

