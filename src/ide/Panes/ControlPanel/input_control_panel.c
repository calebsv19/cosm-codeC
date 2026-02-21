#include "input_control_panel.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/Panes/Editor/Commands/editor_commands.h"
#include "app/GlobalInfo/core_state.h"
#include "fisics_frontend.h"

#include <stdio.h>
#include <string.h>

static Uint32 lastClickTime = 0;
static UITreeNode* lastClickNode = NULL;
static const Uint32 kDoubleClickMs = 350;

static bool find_open_file_in_view_tree(EditorView* view,
                                        const char* targetPath,
                                        EditorView** outView,
                                        int* outTabIndex) {
    if (!view || !targetPath || !*targetPath) return false;
    if (view->type == VIEW_LEAF) {
        for (int i = 0; i < view->fileCount; ++i) {
            OpenFile* f = view->openFiles ? view->openFiles[i] : NULL;
            if (f && f->filePath && strcmp(f->filePath, targetPath) == 0) {
                if (outView) *outView = view;
                if (outTabIndex) *outTabIndex = i;
                return true;
            }
        }
        return false;
    }
    if (find_open_file_in_view_tree(view->childA, targetPath, outView, outTabIndex)) return true;
    if (find_open_file_in_view_tree(view->childB, targetPath, outView, outTabIndex)) return true;
    return false;
}

static EditorView* resolve_target_view_for_path(const char* targetPath) {
    IDECoreState* core = getCoreState();
    if (!core) return NULL;
    EditorView* root = core->persistentEditorView;
    EditorView* active = core->activeEditorView;

    EditorView* matched = NULL;
    int matchedTab = -1;
    if (root && targetPath && targetPath[0] &&
        find_open_file_in_view_tree(root, targetPath, &matched, &matchedTab)) {
        if (matchedTab >= 0) {
            matched->activeTab = matchedTab;
        }
        setActiveEditorView(matched);
        return matched;
    }

    if (active && active->type == VIEW_LEAF) return active;
    if (root) {
        EditorView* firstLeaf = findNextLeaf(root);
        if (firstLeaf) {
            setActiveEditorView(firstLeaf);
            return firstLeaf;
        }
    }
    return NULL;
}

void handleControlPanelKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    if (control_panel_is_search_focused()) {
        bool plainEdit = !(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT));
        if (plainEdit) {
            switch (key) {
                case SDLK_BACKSPACE: control_panel_apply_search_backspace(); return;
                case SDLK_DELETE: control_panel_apply_search_delete(); return;
                case SDLK_LEFT: control_panel_search_cursor_left(); return;
                case SDLK_RIGHT: control_panel_search_cursor_right(); return;
                case SDLK_HOME: control_panel_search_cursor_home(); return;
                case SDLK_END: control_panel_search_cursor_end(); return;
                case SDLK_ESCAPE:
                    control_panel_set_search_focused(false);
                    return;
                default:
                    break;
            }
        }
    }

    if (mod & KMOD_CTRL) {
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

    if (scroll_state_handle_mouse_drag(scroll, event, track, thumb)) {
        return;
    }

    if (event->type == SDL_MOUSEWHEEL) {
        scroll_state_handle_mouse_wheel(scroll, event);
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        handleTreeMouseMove(mx, my);

        if (control_panel_point_in_search_clear_button(mx, my)) {
            control_panel_clear_search_query();
            control_panel_set_search_focused(true);
            return;
        }

        bool inSearch = control_panel_point_in_search_box(mx, my);
        control_panel_set_search_focused(inSearch);
        if (inSearch) {
            return;
        }

        if (control_panel_point_in_filter_header(mx, my)) {
            control_panel_toggle_filters_collapsed();
            return;
        }

        ControlFilterButtonId hitButton = control_panel_hit_filter_button(mx, my);
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
            handleTreeClickWithScroll(&listPane, tree, scroll, mx, my);
            UITreeNode* selected = getSelectedTreeNode();
            if (selected) {
                symbol_tree_cache_note_node(selected);
                Uint32 now = SDL_GetTicks();
                bool isDoubleClick = (selected == lastClickNode) &&
                                     (now - lastClickTime <= kDoubleClickMs);
                lastClickTime = now;
                lastClickNode = selected;

                if (isDoubleClick) {
                    const FisicsSymbol* sym = (const FisicsSymbol*)selected->userData;
                    const char* targetPath = NULL;
                    if (sym && sym->file_path) {
                        targetPath = sym->file_path;
                    } else if (selected->fullPath && selected->type != TREE_NODE_FOLDER) {
                        targetPath = selected->fullPath;
                    }
                    EditorView* view = resolve_target_view_for_path(targetPath);
                    if (view && sym && sym->file_path) {
                        editor_jump_to(view, sym->file_path, sym->start_line, sym->start_col);
                    } else if (view && selected->fullPath && selected->type != TREE_NODE_FOLDER) {
                        editor_jump_to(view, selected->fullPath, 0, 0);
                    }
                }
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

static void handleControlPanelTextInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event || event->type != SDL_TEXTINPUT) return;
    if (!control_panel_is_search_focused()) return;
    control_panel_apply_search_insert(event->text.text);
}

UIPaneInputHandler controlPanelInputHandler = {
    .onCommand = NULL,
    .onKeyboard = handleControlPanelKeyboardInput,
    .onMouse = handleControlPanelMouseInput,
    .onScroll = handleControlPanelScrollInput,
    .onHover = handleControlPanelHoverInput,
    .onTextInput = handleControlPanelTextInput,
};
