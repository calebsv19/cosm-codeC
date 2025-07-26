#ifndef PANE_H
#define PANE_H

#include "ide/Panes/PaneInfo/pane_role.h"

#include "core/CommandBus/command_bus.h"
#include "core/CommandBus/command_metadata.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

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

    // Rendering and Interaction
    void (*render)(struct UIPane*, bool, struct IDECoreState* core);
    void (*handleCommand)(struct UIPane*, InputCommandMetadata meta);      // Central dispatcher

    // Modular input routing
    struct UIPaneInputHandler* inputHandler;
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




// 	Pane Management
void destroyPane(UIPane* pane);



#endif
