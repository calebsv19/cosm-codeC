#include "input_tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "ide/UI/scroll_manager.h"
#include "core/Clipboard/clipboard.h"
#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"

#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

static Uint32 lastClickTicks = 0;
static int lastClickIndex = -1;
static const Uint32 doubleClickMs = 400;
static bool dragging = false;
static int dragAnchor = -1;

static void open_text_asset(const AssetEntry* e) {
    if (!e || !assets_is_text_like(e)) return;
    IDECoreState* core = getCoreState();
    if (!core || !core->activeEditorView) return;
    EditorView* view = core->activeEditorView;

    char fullPath[1024];
    if (e->absPath && e->absPath[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s", e->absPath);
    } else if (projectPath[0]) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", projectPath, e->relPath ? e->relPath : "");
    } else {
        return;
    }
    openFileInView(view, fullPath);
}

static void copy_selection(void) {
    AssetFlatRef refs[1024];
    int count = assets_flatten(refs, 1024);
    size_t cap = 2048;
    size_t len = 0;
    char* buf = malloc(cap);
    if (!buf) return;
    buf[0] = '\0';
    bool any = false;
    for (int i = 0; i < count; ++i) {
        if (!assets_is_selected(i) || refs[i].isMoreLine) continue;
        const AssetEntry* e = refs[i].entry;
        const char* label = refs[i].isHeader
            ? (const char*[]){"Images","Audio","Data","Other"}[refs[i].category]
            : (e && e->relPath) ? e->relPath : (e && e->name ? e->name : "(unknown)");
        size_t add = strlen(label) + 1;
        if (len + add + 1 > cap) {
            cap = (len + add + 1) * 2;
            char* tmp = realloc(buf, cap);
            if (!tmp) { free(buf); return; }
            buf = tmp;
        }
        memcpy(buf + len, label, add - 1);
        len += add - 1;
        buf[len++] = '\n';
        buf[len] = '\0';
        any = true;
    }
    if (any) {
        clipboard_copy_text(buf);
    }
    free(buf);
}

void handleAssetsKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event || event->type != SDL_KEYDOWN) return;
    Uint16 mod = event->key.keysym.mod;
    SDL_Keycode key = event->key.keysym.sym;
    bool ctrl = (mod & KMOD_CTRL) || (mod & KMOD_GUI);
    if (ctrl && key == SDLK_c) {
        copy_selection();
    }
}

void handleAssetsMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    const int headerHeight = 18;
    const int lineHeight = 16;
    const int startY = pane->y + 28;

    PaneScrollState* scroll = assets_get_scroll_state(pane);
    SDL_Rect track = assets_get_scroll_track_rect();
    SDL_Rect thumb = assets_get_scroll_thumb_rect();
    if (scroll_state_handle_mouse_drag(scroll, event, &track, &thumb)) {
        return;
    }

    float offset = scroll ? scroll_state_get_offset(scroll) : 0.0f;

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        AssetFlatRef refs[1024];
        int count = assets_flatten(refs, 1024);
        int my = event->button.y;
        int y = startY - (int)offset;
        int hit = -1;
        for (int i = 0; i < count; ++i) {
            int h = refs[i].isHeader ? headerHeight : lineHeight;
            if (my >= y && my < y + h) { hit = i; break; }
            y += h;
        }
        if (hit >= 0) {
            Uint32 now = SDL_GetTicks();
            bool dbl = (hit == lastClickIndex) && (now - lastClickTicks < doubleClickMs);
            lastClickIndex = hit;
            lastClickTicks = now;
            bool additive = (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI | KMOD_SHIFT)) != 0;
            if (refs[hit].isHeader) {
                assets_toggle_collapse(refs[hit].category);
                assets_clear_selection();
                assets_select_toggle(hit, false);
            } else if (!refs[hit].isMoreLine) {
                assets_select_toggle(hit, additive);
                if (dbl && refs[hit].entry && assets_is_text_like(refs[hit].entry)) {
                    open_text_asset(refs[hit].entry);
                }
            }
            dragging = true;
            dragAnchor = hit;
        } else {
            assets_clear_selection();
            dragging = false;
            dragAnchor = -1;
        }
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        dragging = false;
        dragAnchor = -1;
    } else if (event->type == SDL_MOUSEMOTION && dragging) {
        AssetFlatRef refs[1024];
        int count = assets_flatten(refs, 1024);
        int my = event->motion.y;
        int y = startY - (int)offset;
        int hit = -1;
        for (int i = 0; i < count; ++i) {
            int h = refs[i].isHeader ? headerHeight : lineHeight;
            if (my >= y && my < y + h) { hit = i; break; }
            y += h;
        }
        if (hit >= 0 && dragAnchor >= 0) {
            assets_select_range(dragAnchor, hit);
        }
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
