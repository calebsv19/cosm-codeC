#include "visual_artifact_proof.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef enum VisualArtifactProofState {
    VISUAL_ARTIFACT_PROOF_UNINITIALIZED = 0,
    VISUAL_ARTIFACT_PROOF_DISABLED,
    VISUAL_ARTIFACT_PROOF_READY,
    VISUAL_ARTIFACT_PROOF_REQUESTED,
    VISUAL_ARTIFACT_PROOF_DONE,
    VISUAL_ARTIFACT_PROOF_FAILED,
} VisualArtifactProofState;

static VisualArtifactProofState s_state = VISUAL_ARTIFACT_PROOF_UNINITIALIZED;
static char s_output_path[512] = "visual_artifacts/ide_first_frame.bmp";

static bool parse_bool_env(const char* value, bool* out) {
    if (!value || !value[0] || !out) return false;
    if (strcmp(value, "1") == 0 ||
        strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 ||
        strcasecmp(value, "on") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(value, "0") == 0 ||
        strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 ||
        strcasecmp(value, "off") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static void ensure_initialized(void) {
    if (s_state != VISUAL_ARTIFACT_PROOF_UNINITIALIZED) return;

    bool enabled = false;
    const char* enabled_env = getenv("IDE_VISUAL_ARTIFACT_ONCE");
    if (!parse_bool_env(enabled_env, &enabled) || !enabled) {
        s_state = VISUAL_ARTIFACT_PROOF_DISABLED;
        return;
    }

    const char* path_env = getenv("IDE_VISUAL_ARTIFACT_PATH");
    if (path_env && path_env[0]) {
        snprintf(s_output_path, sizeof(s_output_path), "%s", path_env);
    }
    s_state = VISUAL_ARTIFACT_PROOF_READY;
}

static bool output_file_ready(void) {
    struct stat st;
    if (stat(s_output_path, &st) != 0) return false;
    return st.st_size > 0;
}

#if !USE_VULKAN
static bool capture_sdl_renderer(RenderContext* ctx) {
    if (!ctx || !ctx->renderer || !ctx->window) return false;

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(ctx->renderer, &width, &height);
    if (width <= 0 || height <= 0) {
        SDL_GetWindowSize(ctx->window, &width, &height);
    }
    if (width <= 0 || height <= 0) return false;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return false;

    int read_status = SDL_RenderReadPixels(ctx->renderer,
                                           NULL,
                                           SDL_PIXELFORMAT_ARGB8888,
                                           surface->pixels,
                                           surface->pitch);
    int save_status = -1;
    if (read_status == 0) {
        save_status = SDL_SaveBMP(surface, s_output_path);
    }
    SDL_FreeSurface(surface);
    return read_status == 0 && save_status == 0;
}
#endif

bool ide_visual_artifact_proof_enabled(void) {
    ensure_initialized();
    return s_state != VISUAL_ARTIFACT_PROOF_DISABLED;
}

bool ide_visual_artifact_proof_pending(void) {
    ensure_initialized();
    return s_state == VISUAL_ARTIFACT_PROOF_READY ||
           s_state == VISUAL_ARTIFACT_PROOF_REQUESTED;
}

void ide_visual_artifact_proof_begin_frame(RenderContext* ctx) {
    ensure_initialized();
    if (s_state != VISUAL_ARTIFACT_PROOF_READY) return;

#if USE_VULKAN
    if (!ctx || !ctx->renderer) {
        fprintf(stderr, "[VisualArtifact] renderer unavailable for %s\n", s_output_path);
        s_state = VISUAL_ARTIFACT_PROOF_FAILED;
        return;
    }
    VkResult result = vk_renderer_request_capture(ctx->renderer, s_output_path);
    if (result != VK_SUCCESS) {
        fprintf(stderr,
                "[VisualArtifact] capture request failed for %s: %d\n",
                s_output_path,
                result);
        s_state = VISUAL_ARTIFACT_PROOF_FAILED;
        return;
    }
#else
    (void)ctx;
#endif

    s_state = VISUAL_ARTIFACT_PROOF_REQUESTED;
}

void ide_visual_artifact_proof_end_frame(RenderContext* ctx, bool* running) {
    ensure_initialized();
    if (s_state == VISUAL_ARTIFACT_PROOF_FAILED) {
        if (running) *running = false;
        return;
    }
    if (s_state != VISUAL_ARTIFACT_PROOF_REQUESTED) return;

#if !USE_VULKAN
    if (!capture_sdl_renderer(ctx)) {
        fprintf(stderr, "[VisualArtifact] SDL capture failed for %s: %s\n",
                s_output_path,
                SDL_GetError());
        s_state = VISUAL_ARTIFACT_PROOF_FAILED;
        if (running) *running = false;
        return;
    }
#else
    (void)ctx;
#endif

    if (!output_file_ready()) {
        fprintf(stderr, "[VisualArtifact] capture output missing or empty: %s\n", s_output_path);
        s_state = VISUAL_ARTIFACT_PROOF_FAILED;
        if (running) *running = false;
        return;
    }

    fprintf(stderr, "[VisualArtifact] wrote %s\n", s_output_path);
    s_state = VISUAL_ARTIFACT_PROOF_DONE;
    if (running) *running = false;
}

int ide_visual_artifact_proof_exit_code(void) {
    ensure_initialized();
    return s_state == VISUAL_ARTIFACT_PROOF_FAILED ? 1 : 0;
}

const char* ide_visual_artifact_proof_path(void) {
    ensure_initialized();
    return s_output_path;
}
