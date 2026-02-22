#ifndef CORE_STATE_H
#define CORE_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#include "ide/UI/ui_state.h"
#include "ide/UI/layout_config.h"

#include "core/InputManager/UserInput/rename_flow.h"


struct EditorView;
struct EditorViewState;
struct UIPane;
struct DirEntry;

typedef enum RenderInvalidationReason {
    RENDER_INVALIDATION_NONE       = 0u,
    RENDER_INVALIDATION_INPUT      = 1u << 0,
    RENDER_INVALIDATION_LAYOUT     = 1u << 1,
    RENDER_INVALIDATION_THEME      = 1u << 2,
    RENDER_INVALIDATION_CONTENT    = 1u << 3,
    RENDER_INVALIDATION_OVERLAY    = 1u << 4,
    RENDER_INVALIDATION_RESIZE     = 1u << 5,
    RENDER_INVALIDATION_BACKGROUND = 1u << 6
} RenderInvalidationReason;

enum { RENDER_INVALIDATION_REASON_BUCKETS = 7 };

typedef struct ProjectDragState {
    struct DirEntry* entry;
    bool active;
    bool validTarget;
    struct EditorView* targetView;
    struct DirEntry* targetDirectory;
    bool validDirectoryTarget;
    SDL_Rect targetDirectoryRect;
    int startX;
    int startY;
    int offsetX;
    int offsetY;
    int currentX;
    int currentY;
    int labelWidth;
    char cachedLabel[256];
    Uint32 startTicks;
} ProjectDragState;

// All core state lives here
typedef struct IDECoreState {
    // Global UI/Editor infrastructure
    struct UIPane* editorPane;
    struct UIPane* activeEditorDragPane;
    struct UIPane* activeMousePane;

    struct UIPane* focusedPane;

    struct EditorView* activeEditorView;
    struct EditorView* persistentEditorView;
    struct EditorViewState* editorViewState;
    struct EditorView* hoveredEditorView;

    // Counters and toggles
    int editorViewCount;

    bool initializePopup;
    bool popupPaneActive;
    bool timerHudEnabled;

    // Modular subsystems
    UIState ui;
    LayoutDimensions layout;


    int mouseX;
    int mouseY;

    ProjectDragState projectDrag;

    // Track rename flow globally
    RenameRequest renameFlow;

    // Add more as needed (e.g., diagnostics, plugin state, build info...)

    char workspacePath[1024];
    char runTargetPath[1024];

    // Phase 6.1 invalidation state
    bool frameInvalidated;
    bool fullRedrawRequired;
    uint32_t invalidationReasons;
    uint64_t frameCounter;
    uint64_t invalidationReasonCounts[RENDER_INVALIDATION_REASON_BUCKETS];
} IDECoreState;

// Accessor to global state
IDECoreState* getCoreState(void);

void setTimerHudEnabled(bool enabled);
bool isTimerHudEnabled(void);

void setHoveredEditorView(struct EditorView* view);
struct EditorView* getHoveredEditorView(void);

// Lifecycle (optional for future init/cleanup)
void initCoreState(void);
void shutdownCoreState(void);

void setWorkspacePath(const char* path);
const char* getWorkspacePath(void);
void setRunTargetPath(const char* path);
const char* getRunTargetPath(void);

void invalidatePane(struct UIPane* pane, uint32_t reasonBits);
void invalidateAll(struct UIPane** panes, int paneCount, uint32_t reasonBits);
void requestFullRedraw(uint32_t reasonBits);
bool consumeFrameInvalidation(uint32_t* outReasonBits, bool* outFullRedraw, uint64_t* outFrameId);
bool hasFrameInvalidation(void);

#endif // CORE_STATE_H
