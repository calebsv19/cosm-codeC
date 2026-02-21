#include "core_state.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <string.h>  // for memset
#include <stdlib.h>  // for malloc, free
#include <stdio.h>
#include <strings.h>

static IDECoreState coreState;
static bool s_invalidation_log_initialized = false;
static bool s_invalidation_log_enabled = false;

static bool invalidationLogEnabled(void) {
    if (!s_invalidation_log_initialized) {
        const char* env = getenv("IDE_INVALIDATION_LOG");
        s_invalidation_log_enabled = (env && env[0] &&
                                      (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0));
        s_invalidation_log_initialized = true;
    }
    return s_invalidation_log_enabled;
}

static void addReasonCounts(uint64_t counts[RENDER_INVALIDATION_REASON_BUCKETS], uint32_t reasonBits) {
    if (reasonBits & RENDER_INVALIDATION_INPUT) counts[0]++;
    if (reasonBits & RENDER_INVALIDATION_LAYOUT) counts[1]++;
    if (reasonBits & RENDER_INVALIDATION_THEME) counts[2]++;
    if (reasonBits & RENDER_INVALIDATION_CONTENT) counts[3]++;
    if (reasonBits & RENDER_INVALIDATION_OVERLAY) counts[4]++;
    if (reasonBits & RENDER_INVALIDATION_RESIZE) counts[5]++;
    if (reasonBits & RENDER_INVALIDATION_BACKGROUND) counts[6]++;
}

IDECoreState* getCoreState(void) {
    return &coreState;
}

void initCoreState(void) {
    coreState.editorViewCount = 1;
    coreState.editorPane = NULL;
    coreState.activeEditorDragPane = NULL;
    coreState.activeMousePane = NULL;

    coreState.activeEditorView = NULL;
    coreState.persistentEditorView = NULL;
    coreState.hoveredEditorView = NULL;

    // Allocate and initialize global editor view interaction state
    coreState.editorViewState = malloc(sizeof(EditorViewState));
    if (coreState.editorViewState) {
        memset(coreState.editorViewState, 0, sizeof(EditorViewState));
    }

    coreState.initializePopup = false;
    coreState.popupPaneActive = false;

    // Zero UI/layout state
    memset(&coreState.ui, 0, sizeof(UIState));
    memset(&coreState.layout, 0, sizeof(LayoutDimensions));

    // Optionally clear rename flow
    memset(&coreState.renameFlow, 0, sizeof(RenameRequest));

    coreState.workspacePath[0] = '\0';
    coreState.runTargetPath[0] = '\0';
    coreState.frameInvalidated = true;
    coreState.fullRedrawRequired = true;
    coreState.invalidationReasons = RENDER_INVALIDATION_LAYOUT;
    coreState.frameCounter = 0;
    memset(coreState.invalidationReasonCounts, 0, sizeof(coreState.invalidationReasonCounts));

    memset(&coreState.projectDrag, 0, sizeof(ProjectDragState));
}

void shutdownCoreState(void) {
    if (coreState.editorViewState) {
        free(coreState.editorViewState);
        coreState.editorViewState = NULL;
    }
}

void setTimerHudEnabled(bool enabled) {
    coreState.timerHudEnabled = enabled;
}

bool isTimerHudEnabled(void) {
    return coreState.timerHudEnabled;
}

void setHoveredEditorView(struct EditorView* view) {
    coreState.hoveredEditorView = view;
}

struct EditorView* getHoveredEditorView(void) {
    return coreState.hoveredEditorView;
}

void setWorkspacePath(const char* path) {
    if (!path) {
        coreState.workspacePath[0] = '\0';
        return;
    }
    strncpy(coreState.workspacePath, path, sizeof(coreState.workspacePath) - 1);
    coreState.workspacePath[sizeof(coreState.workspacePath) - 1] = '\0';
}

const char* getWorkspacePath(void) {
    return coreState.workspacePath;
}

void setRunTargetPath(const char* path) {
    if (!path || !*path) {
        coreState.runTargetPath[0] = '\0';
        return;
    }
    strncpy(coreState.runTargetPath, path, sizeof(coreState.runTargetPath) - 1);
    coreState.runTargetPath[sizeof(coreState.runTargetPath) - 1] = '\0';
}

const char* getRunTargetPath(void) {
    return coreState.runTargetPath;
}

void invalidatePane(struct UIPane* pane, uint32_t reasonBits) {
    if (!pane || reasonBits == RENDER_INVALIDATION_NONE) return;

    uint32_t priorReasons = pane->dirtyReasons;
    bool wasDirty = pane->dirty;
    uint32_t newReasonBits = reasonBits & ~priorReasons;

    // Avoid repeatedly dirtying with identical reasons in the same frame.
    if (wasDirty && newReasonBits == 0u) {
        coreState.frameInvalidated = true;
        return;
    }

    paneMarkDirty(pane, reasonBits);
    coreState.frameInvalidated = true;
    coreState.invalidationReasons |= reasonBits;
    addReasonCounts(coreState.invalidationReasonCounts,
                    (newReasonBits != 0u) ? newReasonBits : reasonBits);

    if (invalidationLogEnabled()) {
        fprintf(stderr, "[Invalidation] pane role=%d reasons=0x%08x\n",
                (int)pane->role, reasonBits);
    }
}

void invalidateAll(struct UIPane** panes, int paneCount, uint32_t reasonBits) {
    if (!panes || paneCount <= 0 || reasonBits == RENDER_INVALIDATION_NONE) return;

    for (int i = 0; i < paneCount; ++i) {
        invalidatePane(panes[i], reasonBits);
    }
}

void requestFullRedraw(uint32_t reasonBits) {
    if (reasonBits == RENDER_INVALIDATION_NONE) return;

    coreState.frameInvalidated = true;
    coreState.fullRedrawRequired = true;
    coreState.invalidationReasons |= reasonBits;
    addReasonCounts(coreState.invalidationReasonCounts, reasonBits);

    if (invalidationLogEnabled()) {
        fprintf(stderr, "[Invalidation] full-redraw reasons=0x%08x\n", reasonBits);
    }
}

bool consumeFrameInvalidation(uint32_t* outReasonBits, bool* outFullRedraw, uint64_t* outFrameId) {
    if (!coreState.frameInvalidated && !coreState.fullRedrawRequired) {
        return false;
    }

    if (outReasonBits) *outReasonBits = coreState.invalidationReasons;
    if (outFullRedraw) *outFullRedraw = coreState.fullRedrawRequired;

    coreState.frameCounter++;
    if (outFrameId) *outFrameId = coreState.frameCounter;

    coreState.frameInvalidated = false;
    coreState.fullRedrawRequired = false;
    coreState.invalidationReasons = RENDER_INVALIDATION_NONE;
    return true;
}

bool hasFrameInvalidation(void) {
    return coreState.frameInvalidated || coreState.fullRedrawRequired;
}
