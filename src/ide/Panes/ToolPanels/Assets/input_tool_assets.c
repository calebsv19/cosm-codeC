#include "input_tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/editor_navigation.h"
#include "ide/UI/flat_list_hit_test.h"
#include "ide/UI/flat_list_interaction.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_input_adapter.h"
#include "ide/UI/scroll_manager.h"
#include "core/Clipboard/clipboard.h"

#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    const AssetFlatRef* refs;
    int headerHeight;
    int rowHeight;
} AssetHitTestContext;

typedef struct {
    const AssetFlatRef* refs;
    int hit_index;
} AssetRowActivationState;

static int assets_row_height_for_index(int rowIndex, void* context) {
    AssetHitTestContext* ctx = (AssetHitTestContext*)context;
    if (!ctx || !ctx->refs || rowIndex < 0) return 0;
    return ctx->refs[rowIndex].isHeader ? ctx->headerHeight : ctx->rowHeight;
}

static int assets_hit_test_row(const AssetFlatRef* refs,
                               int count,
                               int mouseY,
                               int firstY,
                               float offset,
                               int headerHeight,
                               int rowHeight) {
    AssetHitTestContext ctx = {
        .refs = refs,
        .headerHeight = headerHeight,
        .rowHeight = rowHeight
    };
    return ui_flat_list_hit_test_variable(mouseY,
                                          firstY,
                                          offset,
                                          count,
                                          assets_row_height_for_index,
                                          &ctx);
}

static void open_text_asset(const AssetEntry* e) {
    if (!e || !assets_is_text_like(e)) return;
    const char* path = (e->absPath && e->absPath[0]) ? e->absPath : e->relPath;
    (void)ui_open_path_in_active_editor(path);
}

static void assets_clear_selection_cb(void* context) {
    (void)context;
    assets_clear_selection();
}

static void assets_select_range_cb(int anchorIndex, int hitIndex, void* context) {
    (void)context;
    assets_select_range(anchorIndex, hitIndex);
}

static void assets_row_select_single(void* context) {
    AssetRowActivationState* state = (AssetRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    assets_select_toggle(state->hit_index, false);
}

static void assets_row_select_additive(void* context) {
    AssetRowActivationState* state = (AssetRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    assets_select_toggle(state->hit_index, true);
}

static void assets_row_prefix_action(void* context) {
    AssetRowActivationState* state = (AssetRowActivationState*)context;
    if (!state || !state->refs || state->hit_index < 0) return;
    assets_toggle_collapse(state->refs[state->hit_index].category);
}

static void assets_row_activate(void* context) {
    AssetRowActivationState* state = (AssetRowActivationState*)context;
    if (!state || !state->refs || state->hit_index < 0) return;
    const AssetFlatRef* ref = &state->refs[state->hit_index];
    if (ref->entry && assets_is_text_like(ref->entry)) {
        open_text_asset(ref->entry);
    }
}

static void assets_row_drag_start(void* context) {
    AssetRowActivationState* state = (AssetRowActivationState*)context;
    if (!state || state->hit_index < 0) return;
    ui_flat_list_drag_state_begin(assets_get_drag_state(), state->hit_index);
}

static bool assets_handle_top_control_click(int x, int y) {
    switch ((AssetTopControlId)ui_panel_tagged_rect_list_hit_test(assets_get_control_hits(), x, y)) {
        case ASSET_TOP_CONTROL_OPEN_ALL:
            assets_set_all_collapsed(false);
            return true;
        case ASSET_TOP_CONTROL_CLOSE_ALL:
            assets_set_all_collapsed(true);
            return true;
        case ASSET_TOP_CONTROL_NONE:
        default:
            return false;
    }
}

void handleAssetsKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event || event->type != SDL_KEYDOWN) return;
    Uint16 mod = event->key.keysym.mod;
    SDL_Keycode key = event->key.keysym.sym;
    if (ui_input_has_primary_accel(mod) && key == SDLK_a) {
        assets_select_all_visible();
        return;
    }
    if (ui_input_has_primary_accel(mod) && key == SDLK_c) {
        assets_copy_selection_to_clipboard();
        return;
    }
}

void handleAssetsMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    const int headerHeight = ASSET_PANEL_HEADER_HEIGHT;
    const int lineHeight = ASSET_PANEL_ROW_HEIGHT;
    const int contentTop = tool_panel_single_row_content_top(pane);
    const int startY = contentTop;

    PaneScrollState* scroll = assets_get_scroll_state(pane);
    SDL_Rect track = assets_get_scroll_track_rect();
    SDL_Rect thumb = assets_get_scroll_thumb_rect();
    if (ui_scroll_input_consume(scroll, event, &track, &thumb)) {
        return;
    }

    float offset = scroll ? scroll_state_get_offset(scroll) : 0.0f;

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (assets_handle_top_control_click(event->button.x, event->button.y)) {
            return;
        }

        AssetFlatRef refs[1024];
        int count = assets_flatten(refs, 1024);
        int hit = assets_hit_test_row(refs,
                                      count,
                                      event->button.y,
                                      startY,
                                      offset,
                                      headerHeight,
                                      lineHeight);
        UIFlatListDragState* dragState = assets_get_drag_state();
        if (!ui_flat_list_drag_state_prepare_hit(dragState,
                                                 hit,
                                                 assets_clear_selection_cb,
                                                 NULL)) {
            return;
        }

        if (refs[hit].isMoreLine) {
            ui_flat_list_drag_state_reset(dragState);
            return;
        }

        Uint16 modifiers = SDL_GetModState();
        AssetRowActivationState activation = {
            .refs = refs,
            .hit_index = hit
        };
        (void)ui_row_activation_handle_primary(
            &(UIRowActivationContext){
                .double_click_tracker = assets_get_double_click_tracker(),
                .row_identity = (uintptr_t)(uint32_t)(hit + 1),
                .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
                .clicked_prefix = refs[hit].isHeader,
                .additive_modifier = ui_input_is_additive_selection(modifiers),
                .range_modifier = false,
                .wants_drag_start = !refs[hit].isHeader,
                .on_select_single = assets_row_select_single,
                .on_select_additive = assets_row_select_additive,
                .on_prefix = assets_row_prefix_action,
                .on_activate = assets_row_activate,
                .on_drag_start = assets_row_drag_start,
                .user_data = &activation
            });
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        ui_flat_list_drag_state_reset(assets_get_drag_state());
    } else if (event->type == SDL_MOUSEMOTION && assets_get_drag_state()->active) {
        AssetFlatRef refs[1024];
        int count = assets_flatten(refs, 1024);
        int hit = assets_hit_test_row(refs,
                                      count,
                                      event->motion.y,
                                      startY,
                                      offset,
                                      headerHeight,
                                      lineHeight);
        (void)ui_flat_list_drag_state_apply_range(assets_get_drag_state(),
                                                  hit,
                                                  assets_select_range_cb,
                                                  NULL);
    }
}

void handleAssetsScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    PaneScrollState* scroll = assets_get_scroll_state(pane);
    if (!scroll) return;
    scroll_state_handle_mouse_wheel(scroll, event);
}

void handleAssetsHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}
