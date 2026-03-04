#ifndef PANE_H
#define PANE_H

#include "ide/Panes/PaneInfo/pane_role.h"

#include "core/CommandBus/command_bus.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/UI/scroll_manager.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_PANES 16





// === Theme Struct ===
typedef struct UITheme {
    SDL_Color bgMenuBar;
    SDL_Color bgEditor;
    SDL_Color bgIconBar;
    SDL_Color bgToolBar;
    SDL_Color bgControlPanel;
    SDL_Color bgTerminal;
    SDL_Color bgPopup;
    SDL_Color border;
    SDL_Color text;
} UITheme;


// === Forward Declarations for Pointers ===
struct EditorView;
struct IDECoreState;
struct UIPane;
struct InputCommandMetadata;


// === Modular Input Handler Struct ===
typedef struct UIPaneInputHandler {
    void (*onCommand)(struct UIPane*, InputCommandMetadata);
    void (*onKeyboard)(struct UIPane*, SDL_Event*);  // Handles Ctrl/Alt/Shift
    void (*onMouse)(struct UIPane*, SDL_Event*);
    void (*onScroll)(struct UIPane*, SDL_Event*);
    void (*onHover)(struct UIPane*, int x, int y);
    void (*onTextInput)(struct UIPane*, SDL_Event*);  
} UIPaneInputHandler;

typedef enum PaneInvalidationReason {
    PANE_INVALIDATION_NONE       = 0u,
    PANE_INVALIDATION_INPUT      = 1u << 0,
    PANE_INVALIDATION_LAYOUT     = 1u << 1,
    PANE_INVALIDATION_THEME      = 1u << 2,
    PANE_INVALIDATION_CONTENT    = 1u << 3,
    PANE_INVALIDATION_OVERLAY    = 1u << 4,
    PANE_INVALIDATION_RESIZE     = 1u << 5,
    PANE_INVALIDATION_BACKGROUND = 1u << 6
} PaneInvalidationReason;

// === UIPane Struct ===
typedef struct UIPane {
    int x, y, w, h;   
    SDL_Color bgColor;
    SDL_Color borderColor;
    
    const char* title;
    bool visible;   
    UIPaneRole role;

    // Optional EditorView linkage
    struct EditorView* editorView;

    // Optional pane-owned controller state
    void* controllerState;
    void (*destroyControllerState)(void*);

    // Rendering and Interaction
    void (*render)(struct UIPane*, bool, struct IDECoreState* core);
    void (*handleCommand)(struct UIPane*, InputCommandMetadata meta);      // Central dispatcher

    // Modular input routing
    struct UIPaneInputHandler* inputHandler;

    PaneScrollState* scrollState;

    // Invalidation state (Phase 6.1)
    bool dirty;
    uint32_t dirtyReasons;
    bool hasDirtyRegion;
    SDL_Rect dirtyRegion;
    uint64_t lastRenderFrameId;

    // Pane cache metadata (Phase 6.3)
    bool cacheEnabled;
    bool cacheValid;
    int cacheWidth;
    int cacheHeight;
    uint64_t cacheVersion;
} UIPane;




// 	INITS

UIPane* createThemedPane(int x, int y, int w, int h, UIPaneRole role,
                        UITheme* theme, const char* title);
UIPane* createPane(int x, int y, int w, int h, 
			SDL_Color bg, SDL_Color border, const char* title, UIPaneRole role);
UIPane* createEmptyPane(UIPaneRole role);
void applyPaneRoleDefaults(UIPane* pane);



// 	HELPERS

bool isPointInsidePane(struct UIPane* pane, int x, int y);

// Render methods
void printEditorTree(struct EditorView* view, int depth);
void paneMarkDirty(UIPane* pane, uint32_t reasonBits);
void paneMarkDirtyRect(UIPane* pane, SDL_Rect rect, uint32_t reasonBits);
void paneClearDirty(UIPane* pane);
void paneReleaseCache(UIPane* pane);




// 	Pane Management
void destroyPane(UIPane* pane);



#endif
