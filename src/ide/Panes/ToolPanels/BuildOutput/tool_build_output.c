#include "tool_build_output.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Clipboard/clipboard.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/editor_navigation.h"
#include "ide/UI/flat_list_hit_test.h"
#include "ide/UI/flat_list_interaction.h"
#include "ide/UI/flat_list_selection.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_input_adapter.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    PaneScrollState scroll;
    bool scroll_init;
    SDL_Rect scroll_track;
    SDL_Rect scroll_thumb;
    bool selected[1024];
    UIFlatListDragState drag_state;
    UIDoubleClickTracker double_click_tracker;
} BuildOutputInteractionState;

static BuildOutputInteractionState g_buildOutputBootstrapState = {0};
static bool g_buildOutputBootstrapInitialized = false;

static void build_output_interaction_init(void* ptr) {
    BuildOutputInteractionState* state = (BuildOutputInteractionState*)ptr;
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->drag_state.active = false;
    state->drag_state.anchor = -1;
}

static void build_output_interaction_destroy(void* ptr) {
    BuildOutputInteractionState* state = (BuildOutputInteractionState*)ptr;
    if (!state) return;
    free(state);
}

static BuildOutputInteractionState* build_output_interaction_state(void) {
    return (BuildOutputInteractionState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_BUILD_OUTPUT_UI,
        sizeof(BuildOutputInteractionState),
        build_output_interaction_init,
        build_output_interaction_destroy,
        &g_buildOutputBootstrapState,
        &g_buildOutputBootstrapInitialized
    );
}

static int build_output_rows_for_diag(const BuildDiagnostic* d) {
    if (!d) return 0;
    return d->notes[0] ? 3 : 2;
}

typedef struct {
    const BuildDiagnostic* diags;
    int lineHeight;
} BuildOutputHitTestContext;

typedef struct {
    int hit_index;
} BuildOutputRowActivationState;

PaneScrollState* build_output_get_scroll_state(void) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    if (!state->scroll_init) {
        scroll_state_init(&state->scroll, NULL);
        state->scroll_init = true;
    }
    return &state->scroll;
}

SDL_Rect build_output_get_scroll_track_rect(void) {
    return build_output_interaction_state()->scroll_track;
}

SDL_Rect build_output_get_scroll_thumb_rect(void) {
    return build_output_interaction_state()->scroll_thumb;
}

void build_output_set_scroll_rects(SDL_Rect track, SDL_Rect thumb) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    state->scroll_track = track;
    state->scroll_thumb = thumb;
}

int build_output_content_top(const UIPane* pane) {
    if (!pane) return 0;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    return pane->y + d.controls_top + 8;
}

int build_output_first_row_y(const UIPane* pane) {
    return build_output_content_top(pane) + tool_panel_content_inset_default();
}

static int build_output_row_height_for_index(int rowIndex, void* context) {
    BuildOutputHitTestContext* ctx = (BuildOutputHitTestContext*)context;
    if (!ctx || !ctx->diags || rowIndex < 0) return 0;
    return build_output_rows_for_diag(&ctx->diags[rowIndex]) * ctx->lineHeight;
}

static int hit_test_diag(int my, int lineHeight, int firstY, float offset) {
    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    BuildOutputHitTestContext ctx = {
        .diags = diags,
        .lineHeight = lineHeight
    };
    return ui_flat_list_hit_test_variable(my,
                                          firstY,
                                          offset,
                                          (int)count,
                                          build_output_row_height_for_index,
                                          &ctx);
}

static void clear_selection(void);
static void select_range(int a, int b);
static void jump_to_diag(const BuildDiagnostic* d);
static void toggle_selection(int idx, bool additive);

static void build_output_clear_selection_cb(void* context) {
    (void)context;
    clear_selection();
}

static void build_output_select_range_cb(int anchorIndex, int hitIndex, void* context) {
    (void)context;
    select_range(anchorIndex, hitIndex);
}

static void build_output_select_single_cb(void* context) {
    BuildOutputRowActivationState* state = (BuildOutputRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    toggle_selection(state->hit_index, false);
}

static void build_output_select_additive_cb(void* context) {
    BuildOutputRowActivationState* state = (BuildOutputRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    toggle_selection(state->hit_index, true);
}

static void build_output_activate_cb(void* context) {
    BuildOutputRowActivationState* state = (BuildOutputRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    if (state->hit_index >= (int)count) return;
    jump_to_diag(&diags[state->hit_index]);
}

static void build_output_drag_start_cb(void* context) {
    BuildOutputRowActivationState* state = (BuildOutputRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    ui_flat_list_drag_state_begin(&build_output_interaction_state()->drag_state, state->hit_index);
}

static void clear_selection(void) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    ui_flat_list_selection_clear(state->selected, (int)(sizeof(state->selected) / sizeof(state->selected[0])));
    setSelectedBuildDiag(-1);
}

static void toggle_selection(int idx, bool additive) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    if (!ui_flat_list_selection_toggle(state->selected,
                                       (int)(sizeof(state->selected) / sizeof(state->selected[0])),
                                       0,
                                       idx,
                                       additive)) {
        return;
    }
    setSelectedBuildDiag(idx);
}

static bool is_selected(int idx) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    return ui_flat_list_selection_contains(state->selected,
                                           (int)(sizeof(state->selected) / sizeof(state->selected[0])),
                                           idx);
}

static void select_range(int a, int b) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    if (!ui_flat_list_selection_select_range(state->selected,
                                             (int)(sizeof(state->selected) / sizeof(state->selected[0])),
                                             0,
                                             a,
                                             b)) {
        return;
    }
    setSelectedBuildDiag(b);
}

bool build_output_is_selected(int idx) {
    return is_selected(idx);
}

static void copy_diag_to_clipboard(const BuildDiagnostic* d) {
    if (!d) return;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s %s:%d:%d\n    %s%s%s",
             d->isError ? "[E]" : "[W]",
             d->path, d->line, d->col,
             d->message,
             d->notes[0] ? "\n    note: " : "",
             d->notes[0] ? d->notes : "");
    clipboard_copy_text(buf);
}

bool build_output_copy_selection_to_clipboard(void) {
    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    size_t cap = 4096;
    size_t len = 0;
    char* out = malloc(cap);
    if (!out) return false;
    out[0] = '\0';

    bool any = false;
    for (size_t i = 0; i < count; ++i) {
        if (!is_selected((int)i)) continue;
        const BuildDiagnostic* d = &diags[i];
        any = true;
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s %s:%d:%d\n    %s",
                 d->isError ? "[E]" : "[W]",
                 d->path, d->line, d->col,
                 d->message);
        size_t add = strlen(buf);
        if (d->notes[0]) {
            snprintf(buf + add, sizeof(buf) - add, "\n    note: %s", d->notes);
            add = strlen(buf);
        }
        buf[add++] = '\n';
        buf[add] = '\0';
        if (len + add + 1 > cap) {
            cap = (len + add + 1) * 2;
            char* tmp = realloc(out, cap);
            if (!tmp) { free(out); return false; }
            out = tmp;
        }
        memcpy(out + len, buf, add);
        len += add;
        out[len] = '\0';
    }

    if (any) {
        clipboard_copy_text(out);
    } else {
        int sel = getSelectedBuildDiag();
        if (sel >= 0 && sel < (int)count) {
            copy_diag_to_clipboard(&diags[sel]);
        }
    }
    free(out);
    return any;
}

void build_output_select_all_visible(void) {
    BuildOutputInteractionState* state = build_output_interaction_state();
    size_t count = 0;
    (void)build_diagnostics_get(&count);
    int idx = ui_flat_list_selection_select_all(state->selected,
                                                (int)(sizeof(state->selected) / sizeof(state->selected[0])),
                                                (int)count);
    if (idx >= 0) {
        setSelectedBuildDiag(idx);
    }
}

static void jump_to_diag(const BuildDiagnostic* d) {
    if (!d) return;
    (void)ui_open_path_at_location_in_active_editor(d->path, d->line, d->col);
}


void handleBuildOutputEvent(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    const int lineHeight = BUILD_OUTPUT_LINE_HEIGHT;
    const int firstY = build_output_first_row_y(pane);
    PaneScrollState* scroll = build_output_get_scroll_state();
    SDL_Rect track = build_output_get_scroll_track_rect();
    SDL_Rect thumb = build_output_get_scroll_thumb_rect();
    if (ui_scroll_input_consume(scroll, event, &track, &thumb)) {
        build_output_set_scroll_rects(track, thumb);
        return;
    }

    float offset = scroll ? scroll_state_get_offset(scroll) : 0.0f;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int hit = hit_test_diag(event->button.y, lineHeight, firstY, offset);
        BuildOutputInteractionState* state = build_output_interaction_state();
        if (!ui_flat_list_drag_state_prepare_hit(&state->drag_state,
                                                 hit,
                                                 build_output_clear_selection_cb,
                                                 NULL)) {
            return;
        }

        BuildOutputRowActivationState activation = {
            .hit_index = hit
        };
        (void)ui_row_activation_handle_primary(
            &(UIRowActivationContext){
                .double_click_tracker = &state->double_click_tracker,
                .row_identity = (uintptr_t)(uint32_t)(hit + 1),
                .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
                .clicked_prefix = false,
                .additive_modifier = ui_input_is_additive_selection(SDL_GetModState()),
                .range_modifier = false,
                .wants_drag_start = true,
                .on_select_single = build_output_select_single_cb,
                .on_select_additive = build_output_select_additive_cb,
                .on_activate = build_output_activate_cb,
                .on_drag_start = build_output_drag_start_cb,
                .user_data = &activation
            });
    } else if (event->type == SDL_MOUSEMOTION && build_output_interaction_state()->drag_state.active) {
        int hit = hit_test_diag(event->motion.y, lineHeight, firstY, offset);
        (void)ui_flat_list_drag_state_apply_range(&build_output_interaction_state()->drag_state,
                                                  hit,
                                                  build_output_select_range_cb,
                                                  NULL);
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        ui_flat_list_drag_state_reset(&build_output_interaction_state()->drag_state);
    } else if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        Uint16 mod = event->key.keysym.mod;
        if (ui_input_has_primary_accel(mod) && key == SDLK_a) {
            build_output_select_all_visible();
            return;
        }
        if (ui_input_has_primary_accel(mod) && key == SDLK_c) {
            build_output_copy_selection_to_clipboard();
            return;
        }
    }
}
