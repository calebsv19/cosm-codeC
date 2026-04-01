#include "input_control_panel.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ControlPanel/control_panel_adapter.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/Panes/panel_view_adapter.h"
#include "ide/UI/editor_navigation.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_input_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "fisics_frontend.h"

#include <stdio.h>

static UIDoubleClickTracker s_symbolDoubleClickTracker = {0};

typedef struct {
    UITreeNode* root;
    UITreeNode* node;
} ControlPanelTreeActivationState;

static void control_panel_tree_select_single(void* user_data) {
    ControlPanelTreeActivationState* state = (ControlPanelTreeActivationState*)user_data;
    if (!state || !state->node) return;
    if (tree_select_all_visual_active_for(state->root)) {
        clearTreeSelectAllVisual();
    }
    setSelectedTreeNode(state->node);
    symbol_tree_cache_note_node(state->node);
}

static void control_panel_tree_prefix_action(void* user_data) {
    ControlPanelTreeActivationState* state = (ControlPanelTreeActivationState*)user_data;
    if (!state || !state->node) return;
    if (state->node->type == TREE_NODE_FOLDER || state->node->type == TREE_NODE_SECTION) {
        state->node->isExpanded = !state->node->isExpanded;
        symbol_tree_cache_note_node(state->node);
    }
}

static void control_panel_tree_activate(void* user_data) {
    ControlPanelTreeActivationState* state = (ControlPanelTreeActivationState*)user_data;
    if (!state || !state->node) return;

    const FisicsSymbol* sym = (const FisicsSymbol*)state->node->userData;
    if (sym && sym->file_path) {
        (void)ui_open_path_at_location_in_best_editor_view(
            sym->file_path,
            sym->start_line,
            sym->start_col
        );
    } else if (state->node->fullPath && state->node->type != TREE_NODE_FOLDER) {
        (void)ui_open_path_at_location_in_best_editor_view(state->node->fullPath, 0, 0);
    }
}

static bool is_match_sub_button(ControlFilterButtonId id) {
    return id == CONTROL_FILTER_BTN_MATCH_METHODS ||
           id == CONTROL_FILTER_BTN_MATCH_TYPES ||
           id == CONTROL_FILTER_BTN_MATCH_VARS ||
           id == CONTROL_FILTER_BTN_MATCH_TAGS;
}

void handleControlPanelKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;
    bool accel = ui_input_has_primary_accel(mod);
    IDECoreState* core = getCoreState();
    int mouseX = core ? core->mouseX : -1;
    int mouseY = core ? core->mouseY : -1;
    bool inTreeContent = control_panel_point_in_symbol_tree_content(pane, mouseX, mouseY);

    if (accel && !control_panel_is_search_focused() && inTreeContent) {
        if (key == SDLK_a) {
            control_panel_select_all_visible();
            setTreeSelectAllVisualRoot(control_panel_get_symbol_tree());
            return;
        }
        if (key == SDLK_c) {
            control_panel_copy_visible_symbol_tree();
            return;
        }
    }

    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        control_panel_clear_match_button_selection();
        return;
    }
    bool plainArrow = !(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT));
    if (plainArrow && (key == SDLK_LEFT || key == SDLK_RIGHT)) {
        int dir = (key == SDLK_LEFT) ? -1 : 1;
        bool jumpToEdge = (mod & KMOD_SHIFT) != 0;
        if (control_panel_move_selected_match_button(dir, jumpToEdge)) {
            return;
        }
    }

    if (control_panel_is_search_focused()) {
        bool plainEdit = !(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT));
        if (plainEdit) {
            if (control_panel_handle_search_edit_key(key)) {
                return;
            }
            switch (key) {
                case SDLK_ESCAPE:
                    control_panel_set_search_focused(false);
                    return;
                default:
                    break;
            }
        }
    }

    if (accel) {
        switch (key) {
            case SDLK_p: CMD(COMMAND_TOGGLE_LIVE_PARSE); return;
            case SDLK_e: CMD(COMMAND_TOGGLE_SHOW_ERRORS); return;
            case SDLK_m: CMD(COMMAND_TOGGLE_SHOW_MACROS); return;
            case SDLK_f:
                control_panel_set_search_focused(true);
                control_panel_search_cursor_end();
                return;
        }
    }

    printf("[ControlPanel] Unmapped keyboard input: %d\n", key);
}


void handleControlPanelMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    PaneScrollState* scroll = control_panel_get_symbol_scroll();
    SDL_Rect* track = control_panel_get_symbol_scroll_track();
    SDL_Rect* thumb = control_panel_get_symbol_scroll_thumb();

    if (ui_scroll_input_consume(scroll, event, track, thumb)) {
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT) {
        int mx = event->button.x;
        int my = event->button.y;
        ControlFilterButtonId hitButton = control_panel_hit_filter_button(mx, my);
        if (control_panel_select_match_button(hitButton)) {
            control_panel_set_search_focused(false);
            return;
        }
        control_panel_clear_match_button_selection();
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        UIPanelTextFieldButtonStripLayout searchLayout = control_panel_get_search_strip_layout();
        SDL_Rect filterHeaderRect = control_panel_get_filter_header_rect();
        handleTreeMouseMove(mx, my);
        ControlFilterButtonId hitButton = control_panel_hit_filter_button(mx, my);
        if (!is_match_sub_button(hitButton)) {
            control_panel_clear_match_button_selection();
        }

        if (ui_panel_rect_contains(&searchLayout.trailing_button_rect, mx, my)) {
            control_panel_clear_search_query();
            control_panel_set_search_focused(true);
            return;
        }

        if (ui_panel_rect_contains(&searchLayout.aux_button_rect, mx, my)) {
            control_panel_toggle_search_enabled();
            control_panel_set_search_focused(true);
            return;
        }

        bool inSearch = ui_panel_rect_contains(&searchLayout.text_field_rect, mx, my);
        control_panel_set_search_focused(inSearch);
        if (inSearch) {
            return;
        }

        if (ui_panel_rect_contains(&filterHeaderRect, mx, my)) {
            control_panel_toggle_filters_collapsed();
            return;
        }

        if (hitButton != CONTROL_FILTER_BTN_NONE) {
            control_panel_activate_filter_button(hitButton);
            return;
        }

        UIPane listPane = *pane;
        int listTop = control_panel_get_symbol_list_top(pane);
        int listHeight = (pane->y + pane->h) - listTop;
        if (listHeight < 0) listHeight = 0;
        listPane.y = control_panel_get_symbol_tree_origin_y(pane);
        listPane.h = (pane->y + pane->h) - listPane.y;
        if (listPane.h < 0) listPane.h = 0;

        UITreeNode* tree = control_panel_get_symbol_tree();
        if (tree) {
            UITreeNode* hit = hitTestTreeNodeWithScroll(&listPane, tree, scroll, mx, my);
            if (hit) {
                ControlPanelTreeActivationState activation = {
                    .root = tree,
                    .node = hit
                };
                (void)ui_row_activation_handle_primary(
                    &(UIRowActivationContext){
                        .double_click_tracker = &s_symbolDoubleClickTracker,
                        .row_identity = (uintptr_t)hit,
                        .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
                        .clicked_prefix = treeNodePrefixHit(&listPane, hit, mx),
                        .additive_modifier = false,
                        .range_modifier = false,
                        .wants_drag_start = false,
                        .on_select_single = control_panel_tree_select_single,
                        .on_prefix = control_panel_tree_prefix_action,
                        .on_activate = control_panel_tree_activate,
                        .user_data = &activation
                    });
            } else if (tree_select_all_visual_active_for(tree)) {
                clearTreeSelectAllVisual();
            }
        }
    }
}


void handleControlPanelScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    PaneScrollState* scroll = control_panel_get_symbol_scroll();
    scroll_state_handle_mouse_wheel(scroll, event);
}

void handleControlPanelHoverInput(UIPane* pane, int x, int y) {
    (void)pane;
    handleTreeMouseMove(x, y);
}

void handleControlPanelTextInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event || event->type != SDL_TEXTINPUT) return;
    if (!control_panel_is_search_focused()) return;
    (void)control_panel_handle_search_text_input(event);
}

static void handleControlPanelKeyboardViaAdapter(UIPane* pane, SDL_Event* event) {
    ui_panel_view_adapter_keyboard(control_panel_view_adapter(), pane, event);
}

static void handleControlPanelMouseViaAdapter(UIPane* pane, SDL_Event* event) {
    ui_panel_view_adapter_mouse(control_panel_view_adapter(), pane, event);
}

static void handleControlPanelScrollViaAdapter(UIPane* pane, SDL_Event* event) {
    ui_panel_view_adapter_scroll(control_panel_view_adapter(), pane, event);
}

static void handleControlPanelHoverViaAdapter(UIPane* pane, int x, int y) {
    ui_panel_view_adapter_hover(control_panel_view_adapter(), pane, x, y);
}

static void handleControlPanelTextInputViaAdapter(UIPane* pane, SDL_Event* event) {
    ui_panel_view_adapter_text_input(control_panel_view_adapter(), pane, event);
}

UIPaneInputHandler controlPanelInputHandler = {
    .onCommand = handleControlPanelViaAdapterCommand,
    .onKeyboard = handleControlPanelKeyboardViaAdapter,
    .onMouse = handleControlPanelMouseViaAdapter,
    .onScroll = handleControlPanelScrollViaAdapter,
    .onHover = handleControlPanelHoverViaAdapter,
    .onTextInput = handleControlPanelTextInputViaAdapter,
};
