#ifndef PANEL_VIEW_ADAPTER_H
#define PANEL_VIEW_ADAPTER_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/CommandBus/command_metadata.h"

#include <SDL2/SDL.h>

struct IDECoreState;

typedef struct UIPanelViewAdapter {
    const char* debug_name;
    void (*render)(UIPane* pane, bool hovered, struct IDECoreState* core);
    void (*onCommand)(UIPane* pane, InputCommandMetadata meta);
    void (*onKeyboard)(UIPane* pane, SDL_Event* event);
    void (*onMouse)(UIPane* pane, SDL_Event* event);
    void (*onScroll)(UIPane* pane, SDL_Event* event);
    void (*onHover)(UIPane* pane, int x, int y);
    void (*onTextInput)(UIPane* pane, SDL_Event* event);
} UIPanelViewAdapter;

static inline void ui_panel_view_adapter_render(const UIPanelViewAdapter* adapter,
                                                UIPane* pane,
                                                bool hovered,
                                                struct IDECoreState* core) {
    if (adapter && adapter->render) {
        adapter->render(pane, hovered, core);
    }
}

static inline void ui_panel_view_adapter_command(const UIPanelViewAdapter* adapter,
                                                 UIPane* pane,
                                                 InputCommandMetadata meta) {
    if (adapter && adapter->onCommand) {
        adapter->onCommand(pane, meta);
    }
}

static inline void ui_panel_view_adapter_keyboard(const UIPanelViewAdapter* adapter,
                                                  UIPane* pane,
                                                  SDL_Event* event) {
    if (adapter && adapter->onKeyboard) {
        adapter->onKeyboard(pane, event);
    }
}

static inline void ui_panel_view_adapter_mouse(const UIPanelViewAdapter* adapter,
                                               UIPane* pane,
                                               SDL_Event* event) {
    if (adapter && adapter->onMouse) {
        adapter->onMouse(pane, event);
    }
}

static inline void ui_panel_view_adapter_scroll(const UIPanelViewAdapter* adapter,
                                                UIPane* pane,
                                                SDL_Event* event) {
    if (adapter && adapter->onScroll) {
        adapter->onScroll(pane, event);
    }
}

static inline void ui_panel_view_adapter_hover(const UIPanelViewAdapter* adapter,
                                               UIPane* pane,
                                               int x,
                                               int y) {
    if (adapter && adapter->onHover) {
        adapter->onHover(pane, x, y);
    }
}

static inline void ui_panel_view_adapter_text_input(const UIPanelViewAdapter* adapter,
                                                    UIPane* pane,
                                                    SDL_Event* event) {
    if (adapter && adapter->onTextInput) {
        adapter->onTextInput(pane, event);
    }
}

#endif
