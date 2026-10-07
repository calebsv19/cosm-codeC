#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "ide/UI/ui_state.h"
#include "window_lifecycle_proof.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "kit_ui_window_probe_sdl.h"
#include "engine/Render/render_pipeline.h"
#include <stdio.h>
#include "vk_runtime.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "ide/Panes/IconBar/input_icon_bar.h"
#include "ide/Panes/IconBar/command_icon_bar.h"
#include <stdlib.h>
#include "ide/Panes/Editor/editor_header_controls.h"
#include "ide/Panes/Editor/Input/input_editor.h"
static KitUiWindowProbe probe;
static bool initialized;
static uint64_t frames;
static int capture_frame(void* user, const char* path) {
    RenderContext* ctx = user;
    return ctx && ctx->renderer && vk_renderer_request_capture(ctx->renderer, path) == VK_SUCCESS;
}
/* Opt-in disposable-workspace proof: exercise real header input and queued close. */
static int proof_leaf_count(EditorView* view) {
    if (!view) return 0;
    if (view->type == VIEW_LEAF) return 1;
    return proof_leaf_count(view->childA) + proof_leaf_count(view->childB);
}
static bool proof_adjacent(EditorView* view) {
    if (!view || view->type == VIEW_LEAF) return true;
    EditorView* a = view->childA; EditorView* b = view->childB;
    if (!a || !b) return false;
    bool adjacent = view->splitType == SPLIT_VERTICAL ? a->x + a->w == b->x : a->y + a->h == b->y;
    return adjacent && proof_adjacent(a) && proof_adjacent(b);
}
static EditorView* proof_empty_leaf(EditorView* view) {
    if (!view) return NULL;
    if (view->type == VIEW_LEAF) return view->fileCount == 0 ? view : NULL;
    EditorView* a = proof_empty_leaf(view->childA);
    return a ? a : proof_empty_leaf(view->childB);
}
static bool proof_editor_refinement(int phase) {
    IDECoreState* core = getCoreState();
    UIPane* pane = core->editorPane;
    EditorView* root = pane->editorView;
    int expected = phase < 3 ? 3 : phase == 3 ? 2 : 1;
    bool good = proof_leaf_count(root) == expected && proof_adjacent(root);
    if (phase >= 4) good = good && root->type == VIEW_LEAF && root->fileCount == 1;
    printf("IDE_UI_EDITOR_REFINEMENT phase=%d status=%s leaves=%d adjacent=%d\n",
           phase, good ? "pass" : "fail", proof_leaf_count(root), proof_adjacent(root));
    if (!good) return false;
    return true;
}
static bool proof_click_empty_editor(void) {
    IDECoreState* core = getCoreState();
    UIPane* pane = core->editorPane;
    EditorView* empty = proof_empty_leaf(pane->editorView);
    if (!empty || !ide_editor_header_close_enabled(pane, empty)) return false;
    core->focusedPane = pane;
    SDL_Event click = {0}; click.type = SDL_MOUSEBUTTONDOWN; click.button.button = SDL_BUTTON_LEFT;
    click.button.x = editor_view_box_x(empty) + editor_view_box_w(empty) - EDITOR_TAB_CLOSE_BTN_MARGIN - EDITOR_TAB_CLOSE_BTN_SIZE / 2;
    click.button.y = editor_view_box_y(empty) + 4 + EDITOR_TAB_CLOSE_BTN_SIZE / 2;
    handleEditorMouseInput(pane, &click);
    bool pressed = ide_editor_header_state(empty, IDE_EDITOR_HEADER_CLOSE, false).pressed;
    click.type = SDL_MOUSEBUTTONUP; handleEditorMouseInput(pane, &click);
    printf("IDE_UI_EMPTY_EDITOR_CLICK pressed=%d\n", pressed);
    return pressed;
}
void ide_window_proof_tick(bool* running) {
    if (!initialized) { kit_ui_window_probe_init_sdl(&probe, "codec"); initialized = true; }
    if (!probe.directory) return;
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->window || !running) return;
    const VkRuntimeCapabilityReport* report = ctx->renderer && ctx->renderer->context.device ?
        vk_runtime_get_capability_report(&ctx->renderer->context.device->runtime) : NULL;
    if (!report || !report->validation_enabled || report->validation_warning_count || report->validation_error_count) {
        fprintf(stderr, "IDE_WINDOW_VALIDATION status=fail\n");
        probe.status = -1;
        *running = false;
        return;
    }
    const char* panels = getenv("CODEWORK_IDE_UI_PROOF_PANELS");
    static int previous_phase = -1;
    static IconTool previous_clicked=ICON_COUNT;
    static bool previous_visible;
    static int pending_close_phase = -1;
    static uint64_t pending_close_frame;
    bool refinements = getenv("CODEWORK_IDE_UI_PROOF_REFINEMENTS") != NULL;
    if (panels && panels[0] && probe.phase != previous_phase) {
        if(getenv("CODEWORK_IDE_UI_PROOF_ICON_CLICK") && previous_clicked!=ICON_COUNT){
            bool good=getUIState()->toolPanelVisible == previous_visible && getActiveIcon()==previous_clicked;
            printf("IDE_UI_ICON_CLICK_RESULT phase=%d status=%s visible=%d actual=%d\n",previous_phase,good?"pass":"fail",getUIState()->toolPanelVisible,getActiveIcon());
            if(!good){probe.status=-1;*running=false;return;}
        }
        static const IconTool first[8] = {ICON_PROJECT_FILES, ICON_ERRORS, ICON_LIBRARIES,
            ICON_VERSION_CONTROL, ICON_VERSION_CONTROL, ICON_TASKS, ICON_TASKS, ICON_BUILD_OUTPUT};
        static const IconTool second[8] = {ICON_ASSET_MANAGER, ICON_PROJECT_FILES, ICON_TASKS,
            ICON_VERSION_CONTROL, ICON_VERSION_CONTROL, ICON_LIBRARIES, ICON_LIBRARIES, ICON_ERRORS};
        int phase = probe.phase < 8 ? probe.phase : 7;
        IconTool selected=atoi(panels)==2 ? second[phase] : first[phase];
        if (refinements) {
            static const IconTool sequence[8] = {ICON_PROJECT_FILES, ICON_PROJECT_FILES, ICON_ERRORS,
                ICON_PROJECT_FILES, ICON_PROJECT_FILES, ICON_LIBRARIES, ICON_ASSET_MANAGER, ICON_ASSET_MANAGER};
            selected = sequence[phase];
        }
        if(getenv("CODEWORK_IDE_UI_PROOF_ICON_CLICK")){
            if (!refinements || phase == 0) getUIState()->toolPanelVisible=false;
            previous_visible = !(getUIState()->toolPanelVisible && getActiveIcon() == selected);
            UIPane* bar=getUIState()->iconBar;getCoreState()->focusedPane=bar;
            SDL_Rect rect=getIconRect(bar,selected);
            SDL_Event click={0};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;click.button.x=rect.x+rect.w/2;click.button.y=rect.y+rect.h/2;
            handleIconBarMouseInput(bar,&click);
            /* Include an intervening paint-state read as ordinary clicks do. */
            (void)ide_icon_bar_button_state(bar,selected);
            click.type=SDL_MOUSEBUTTONUP;handleIconBarMouseInput(bar,&click);
            previous_clicked=selected;
            printf("IDE_UI_ICON_CLICK phase=%d expected=%d\n",phase,selected);
        }else setActiveIcon(selected);
        if (refinements && !proof_editor_refinement(phase)) {
            probe.status = -1; *running = false; return;
        }
        if (refinements && (phase == 2 || phase == 3)) {
            pending_close_phase = phase; pending_close_frame = frames;
        }
        const char* text_proof=getenv("CODEWORK_IDE_UI_PROOF_TEXT");
        if (text_proof && text_proof[0]=='1') {
            (void)control_panel_set_search_query_text("Wié — a long measured UTF-8 search query whose caret must stay inside the viewport");
            bool git=getActiveIcon()==ICON_VERSION_CONTROL;
            control_panel_set_search_focused(false);
            UIPane* previous=tool_panel_bind_dispatch_pane(getUIState()->toolPanel);
            git_panel_set_message_focus(false);
            if (git) {
                git_panel_set_message("Wié — a long measured message whose caret must stay inside the viewport");
                git_panel_set_message_focus(true);
            }
            tool_panel_restore_dispatch_pane(previous);
            if (!git) control_panel_set_search_focused(true);
            IDECoreState* core=getCoreState();
            if (core) core->focusedPane=git ? getUIState()->toolPanel : getUIState()->controlPanel;
            printf("IDE_UI_TEXT_PROOF phase=%d field=%s\n",phase,git ? "git" : "control");
        }
        const char* rename_proof=getenv("CODEWORK_IDE_UI_PROOF_RENAME");
        if(rename_proof && rename_proof[0]=='1'){
            if(phase==3)beginRenameWithPrompt("Rename UTF-8 fixture","Invalid name",
                "Wié水🙂 — a long measured rename whose caret stays inside the viewport",NULL,NULL,NULL,true);
            if(phase==5)cancelRename();
            printf("IDE_UI_RENAME_PROOF phase=%d active=%d\n",phase,isRenaming());
        }
        const char* retained_proof=getenv("CODEWORK_IDE_UI_PROOF_RETAINED_TEXT");
        if(retained_proof && retained_proof[0]=='1') {
            if(isRenaming()) {
                (void)handleRenameEditingKeyWithMod(SDLK_HOME,KMOD_NONE);
                for(int i=0;i<4;++i)(void)handleRenameEditingKeyWithMod(SDLK_RIGHT,KMOD_SHIFT);
                SDL_Event preedit={0};preedit.type=SDL_TEXTEDITING;
                snprintf(preedit.edit.text,sizeof(preedit.edit.text),"Préedit");preedit.edit.start=2;preedit.edit.length=3;
                (void)handleRenameTextEvent(&preedit);
            } else if(control_panel_is_search_focused()) {
                (void)control_panel_handle_search_edit_key_mod(SDLK_HOME,KMOD_NONE);
                for(int i=0;i<4;++i)(void)control_panel_handle_search_edit_key_mod(SDLK_RIGHT,KMOD_SHIFT);
            }
            printf("IDE_UI_RETAINED_TEXT_PROOF phase=%d rename=%d\n",phase,isRenaming());
        }
        requestFullRedraw(RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_INPUT);
        printf("IDE_UI_PROOF phase=%d icon=%d\n", phase, getActiveIcon());
        previous_phase = probe.phase;
    }
    /* Window transitions update layout after this tick. Click only after repaint. */
    if (pending_close_phase == probe.phase && frames >= pending_close_frame + 2) {
        pending_close_phase = -1;
        if (!proof_click_empty_editor()) { probe.status = -1; *running = false; return; }
    }
    const char* rename_capture=getenv("CODEWORK_IDE_UI_PROOF_RENAME");
    if(rename_capture && rename_capture[0]=='1' && isRenaming()){
        /* Keep the opt-in rendering fixture's caret visible for capture. */
        caretVisible=true;lastCaretBlink=SDL_GetTicks();
    }
    int status = kit_ui_window_probe_tick_sdl(&probe, ctx->window, frames, capture_frame, ctx);
    if(status>0 && getenv("CODEWORK_IDE_UI_PROOF_ICON_CLICK") && previous_clicked!=ICON_COUNT){
        bool good=getUIState()->toolPanelVisible == previous_visible && getActiveIcon()==previous_clicked;
        printf("IDE_UI_ICON_CLICK_RESULT phase=%d status=%s visible=%d actual=%d\n",previous_phase,good?"pass":"fail",getUIState()->toolPanelVisible,getActiveIcon());
        if(!good){probe.status=-1;status=-1;}
    }
    if (status) {
        if (status < 0) fprintf(stderr, "IDE window lifecycle proof failed\n");
        if (status > 0) printf("IDE_WINDOW_VALIDATION status=pass warnings=0 errors=0\n");
        *running = false;
    }
}
void ide_window_proof_presented(void) { ++frames; }
int ide_window_proof_exit_code(void) { return initialized && probe.status < 0 ? 1 : 0; }
