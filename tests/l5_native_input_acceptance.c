#include <assert.h>

#include "core/CommandBus/command_bus.h"
#include "core/InputManager/UserInput/rename_access.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/Panes/Editor/editor_view_file_helpers.h"

#include EVENT_LOOP_SOURCE

static void input_key(FrameContext* ctx, Uint32 type, SDL_Keycode key, SDL_Keymod mod, Uint8 repeat) {
    SDL_Event event = {.type = type};
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    event.key.keysym.mod = mod;
    event.key.repeat = repeat;
    process_single_input_event(ctx, &event, false);
}

static void press(FrameContext* ctx, SDL_Keycode key, SDL_Keymod mod) {
    input_key(ctx, SDL_KEYDOWN, key, mod, 0);
    input_key(ctx, SDL_KEYUP, key, mod, 0);
}

static void text(FrameContext* ctx, Uint32 type, const char* value) {
    SDL_Event event = {.type = type};
    if (type == SDL_TEXTINPUT)
        SDL_strlcpy(event.text.text, value, sizeof(event.text.text));
    else
        SDL_strlcpy(event.edit.text, value, sizeof(event.edit.text));
    process_single_input_event(ctx, &event, false);
}

static void click(FrameContext* ctx, UIPane* pane) {
    assert(pane && pane->visible && pane->w > 20 && pane->h > 0);
    SDL_Event event = {.type = SDL_MOUSEBUTTONDOWN};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = pane->x + pane->w / 2;
    event.button.y = pane->y + (pane->h > 70 ? pane->h - 20 : pane->h / 2);
    process_single_input_event(ctx, &event, false);
    event.type = SDL_MOUSEBUTTONUP;
    process_single_input_event(ctx, &event, false);
}

static void drain_commands(void) {
    for (int i = 0; i < 100 && isCommandBusBusy(); ++i) tickCommandBus();
    assert(!isCommandBusBusy());
}

static char* snapshot(OpenFile* file) {
    size_t length = 0;
    char* bytes = getBufferSnapshot(file->buffer, &length);
    assert(bytes && length > 0);
    return bytes;
}

static void wait_for_worker(void) {
    for (int i = 0; i < 10000 && analysis_job_is_running(); ++i) {
        analysis_job_poll();
        SDL_Delay(1);
    }
    assert(!analysis_job_is_running() && completed_results_queue_analysis_terminal_pending());
}

static void analyze(OpenFile* file, uint64_t run) {
    char* bytes = snapshot(file);
    start_async_live_buffer_analysis(projectPath, "", run, file->filePath, bytes,
                                     strlen(bytes), file->documentRevision);
    free(bytes);
    assert(analysis_job_is_running());
}

static void capture(FrameContext* ctx, const char* name) {
    RenderContext* renderer = getRenderContext();
    const char* directory = getenv("CODEC_L5_CAPTURE");
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(length > 0 && (size_t)length < sizeof(path));
    assert(vk_renderer_request_capture(renderer->renderer, path) == VK_SUCCESS);
    for (int i = 0; i < 4; ++i) {
        invalidateAll(ctx->panes, *ctx->paneCount, RENDER_INVALIDATION_CONTENT);
        SDL_Delay(20);
        runFrameLoop(ctx, SDL_GetPerformanceCounter(), 0.02f);
    }
    const VkRuntimeCapabilityReport* report = vk_runtime_get_capability_report(&renderer->renderer->context.device->runtime);
    assert(report && report->validation_enabled && !report->validation_warning_count && !report->validation_error_count);
    struct stat st;
    assert(!stat(path, &st) && st.st_size > 0);
}

int main(int argc, char** argv) {
    assert(argc == 1 && initializeSystem(argv[0]));
    setTimerHudEnabled(false);
    analysis_job_system_shutdown();
    analysis_scheduler_init();
    completed_results_queue_reset();
    analysis_job_system_init();

    IDECoreState* core = getCoreState();
    if (!core->persistentEditorView) core->persistentEditorView = createEditorView();
    EditorView* view = core->persistentEditorView;
    setActiveEditorView(view);
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s/main.c", projectPath);
    assert(length > 0 && (size_t)length < sizeof(path));
    OpenFile* file = openFileInView(view, path);
    assert(file);

    UIPane* panes[MAX_PANES];
    int count = 0;
    layout_static_panes(panes, &count);
    ResizeZone zones[5];
    int zone_count = 0, w = 0, h = 0;
    bool running = true;
    SDL_Event event;
    Uint64 last = SDL_GetPerformanceCounter();
    FrameContext ctx = {panes, &count, zones, &zone_count, &running, &event, &w, &h, 0.016f, &last};
    UIPane* editor = core->editorPane;
    UIPane* other = getUIState()->menuBar;
    assert(editor && other && other->visible);
    core->focusedPane = editor;

    press(&ctx, SDLK_a, KMOD_GUI);
    text(&ctx, SDL_TEXTINPUT, "int main(void) { return 0; }");
    text(&ctx, SDL_TEXTINPUT, " /* café */");
    press(&ctx, SDLK_s, KMOD_GUI);
    drain_commands();
    assert(!file->isModified && file->showSavedTag);
    char* saved = snapshot(file);
    assert(strstr(saved, "/* café */"));
    char disk[256] = {0};
    FILE* f = fopen(path, "rb");
    assert(f && fread(disk, 1, sizeof(disk) - 1, f) > 0 && fclose(f) == 0);
    assert(!strcmp(disk, saved));
    assert(!strcmp(saved, "int main(void) { return 0; } /* café */\n"));
    uint64_t revision = file->documentRevision;

    beginRename("draft", NULL, NULL, NULL);
    text(&ctx, SDL_TEXTEDITING, "preedit");
    assert(!strcmp(RENAME->retainedEdit.composition, "preedit"));
    press(&ctx, SDLK_RETURN, KMOD_NONE);
    assert(isRenaming());
    text(&ctx, SDL_TEXTINPUT, "-é");
    assert(!strcmp(RENAME->inputBuffer, "draft-é"));
    // An outside press and release cannot change focus while Rename owns input.
    SDL_Event outside = {.type = SDL_MOUSEBUTTONDOWN};
    outside.button.button = SDL_BUTTON_LEFT;
    outside.button.x = other->x + 4;
    outside.button.y = other->y + 4;
    process_single_input_event(&ctx, &outside, false);
    outside.type = SDL_MOUSEBUTTONUP;
    process_single_input_event(&ctx, &outside, false);
    bool modal_pointer = core->focusedPane == editor;
    core->focusedPane = editor;
    drain_commands();
    press(&ctx, SDLK_l, KMOD_GUI | KMOD_SHIFT);
    bool modal_shortcut = !isCommandBusBusy();
    // Discard a leaked chooser before it can open and change the fixture workspace.
    shutdownCommandBus();
    initCommandBus();

    input_key(&ctx, SDL_KEYDOWN, SDLK_RETURN, KMOD_NONE, 0);
    assert(!isRenaming());
    input_key(&ctx, SDL_KEYDOWN, SDLK_RETURN, KMOD_NONE, 1);
    input_key(&ctx, SDL_KEYUP, SDLK_RETURN, KMOD_NONE, 0);
    drain_commands();
    bool modal_repeat = file->documentRevision == revision;

    // A key held in the menu cannot begin editing after a normal editor click.
    core->focusedPane = other;
    input_key(&ctx, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, 0);
    click(&ctx, editor);
    assert(core->focusedPane == editor);
    uint64_t before_repeat = file->documentRevision;
    // Put the cursor at a position where a leaked Backspace would mutate text.
    press(&ctx, SDLK_END, KMOD_GUI);
    drain_commands();
    input_key(&ctx, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, 1);
    input_key(&ctx, SDL_KEYUP, SDLK_BACKSPACE, KMOD_NONE, 0);
    drain_commands();
    bool pane_repeat = file->documentRevision == before_repeat;

    input_key(&ctx, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, 0);
    drain_commands();
    assert(file->documentRevision > before_repeat);
    click(&ctx, other);
    assert(core->focusedPane == other);
    click(&ctx, editor);
    press(&ctx, SDLK_END, KMOD_GUI);
    drain_commands();
    uint64_t after_focus_return = file->documentRevision;
    input_key(&ctx, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, 1);
    input_key(&ctx, SDL_KEYUP, SDLK_BACKSPACE, KMOD_NONE, 0);
    drain_commands();
    assert(file->documentRevision == after_focus_return);
    input_key(&ctx, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, 0);
    drain_commands();
    uint64_t after_fresh_press = file->documentRevision;
    assert(after_fresh_press > after_focus_return);
    input_key(&ctx, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, 1);
    input_key(&ctx, SDL_KEYUP, SDLK_BACKSPACE, KMOD_NONE, 0);
    drain_commands();
    assert(file->documentRevision > after_fresh_press);

    // Force a genuine compiler result to complete before the main thread accepts it.
    press(&ctx, SDLK_a, KMOD_GUI);
    text(&ctx, SDL_TEXTINPUT, "int n = stale_missing;");
    drain_commands();
    uint64_t before_stamp = analysis_store_combined_stamp();
    analyze(file, 5000);
    wait_for_worker();
    press(&ctx, SDLK_a, KMOD_GUI);
    text(&ctx, SDL_TEXTINPUT, "int n = fresh_missing;");
    drain_commands();
    CompletedResultsQueueStats before, after;
    completed_results_queue_snapshot(&before);
    while (drain_completed_results(&ctx) > 0) {}
    completed_results_queue_snapshot(&after);
    assert(after.results_stale_dropped > before.results_stale_dropped);
    bool stale_store = analysis_store_combined_stamp() == before_stamp;
    bool stale_status = analysis_status_get() != ANALYSIS_STATUS_REFRESHING;

    fprintf(stderr, "L5 codeC modal-pointer=%d modal-shortcut=%d modal-repeat=%d pane-repeat=%d stale-store=%d stale-status=%d\n",
            modal_pointer, modal_shortcut, modal_repeat, pane_repeat, stale_store, stale_status);
    assert(modal_pointer && modal_shortcut && modal_repeat && pane_repeat && stale_store && stale_status);
    analyze(file, 5001);
    wait_for_worker();
    while (drain_completed_results(&ctx) > 0) {}
    process_events_bounded(&ctx);
    assert(analysis_status_get() == ANALYSIS_STATUS_FRESH && getDiagnosticCount() > 0);
    assert(analysis_store_published_stamp() == analysis_store_combined_stamp());
    const Diagnostic* diag = getDiagnosticAt(0);
    assert(diag && !strcmp(diag->filePath, file->filePath));

    press(&ctx, SDLK_a, KMOD_GUI);
    text(&ctx, SDL_TEXTINPUT, "int main(void) { return 0; }");
    press(&ctx, SDLK_s, KMOD_GUI);
    drain_commands();
    assert(!file->isModified && file->showSavedTag);
    analyze(file, 5002);
    wait_for_worker();
    while (drain_completed_results(&ctx) > 0) {}
    process_events_bounded(&ctx);
    assert(analysis_status_get() == ANALYSIS_STATUS_FRESH && getDiagnosticCount() == 0);
    char* clean = snapshot(file);
    assert(!strcmp(clean, "int main(void) { return 0; }\n"));
    free(clean);

    beginRename("long-project-name-é", NULL, NULL, NULL);
    text(&ctx, SDL_TEXTEDITING, "preview");
    capture(&ctx, "rename.bmp");
    press(&ctx, SDLK_ESCAPE, KMOD_NONE);
    assert(!isRenaming());
    free(saved);
    shutdownSystem(panes, count);
    puts("PASS L5 codeC: UTF-8 text, pane and modal edges, save and asynchronous diagnostics");
    return 0;
}
