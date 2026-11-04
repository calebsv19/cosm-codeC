#include "command_bus.h"
#include "command_metadata.h"
#include "command_registry.h"

#include "save_queue.h"

#include "ide/Panes/Editor/editor_view.h"
#include "app/GlobalInfo/core_state.h"


#include <stdio.h>



#include <stdlib.h>

// Internal Command Queue Structure

typedef struct CommandQueueItem {
    InputCommandMetadata meta;
    bool editorTarget;
    uint64_t viewId;
    uint64_t fileId;
    struct CommandQueueItem* next;
} CommandQueueItem;

static CommandQueueItem* commandHead = NULL;
static CommandQueueItem* commandTail = NULL;




/* Leaf and file IDs identify lifetimes, so ordinary consecutive edits stay valid. */
static uint64_t lastTargetId;

static bool bindId(uint64_t* id) {
    if (*id) return true;
    if (lastTargetId == UINT64_MAX) return false;
    *id = ++lastTargetId;
    return true;
}

static EditorView* commandView(UIPane* pane) {
    EditorView* view = getCoreState()->activeEditorView;
    return view ? view : (pane->role == PANE_ROLE_EDITOR ? pane->editorView : NULL);
}

static bool editorTargetMatches(const CommandQueueItem* item) {
    EditorView* view = commandView(item->meta.targetPane);
    if (view ? (!item->viewId || view->commandTargetId != item->viewId) : item->viewId != 0) return false;
    if (item->meta.cmd == COMMAND_SWITCH_TAB) return true;
    OpenFile* file = getActiveOpenFile(view);
    return file ? item->fileId && file->commandTargetId == item->fileId : item->fileId == 0;
}

static void discardPayload(InputCommandMetadata meta) {
    if (meta.cmd == COMMAND_EDITOR_KEYDOWN || meta.cmd == COMMAND_EDITOR_TEXT_INPUT)
        free(meta.payload);
}

// State Tracking

static InputCommand lastCommand = COMMAND_NONE;




// Lifecycle

void initCommandBus() {
    initSaveQueue();
    commandHead = commandTail = NULL;
}

void shutdownCommandBus() {
    shutdownSaveQueue();

    while (commandHead) {
        CommandQueueItem* next = commandHead->next;
        discardPayload(commandHead->meta);
        free(commandHead);
        commandHead = next;
    }
    commandTail = NULL;
}

void tickCommandBus() {
    // Tick one save file per frame
    tickSaveQueue();

    // Tick one command per frame
    if (!commandHead) return;

    CommandQueueItem* item = commandHead;
    commandHead = item->next;
    if (!commandHead) commandTail = NULL;

    lastCommand = item->meta.cmd;
    if (item->meta.targetPane->role != item->meta.originRole ||
        (item->editorTarget && !editorTargetMatches(item))) {
        fprintf(stderr, "[CommandBus] Target changed; discarded %s\n", getCommandName(item->meta.cmd));
        discardPayload(item->meta);
    } else {
        dispatchInputCommandWithMetadata(item->meta);
    }
    free(item);
}

bool isCommandBusBusy() {
    return isSaveQueueBusy() || (commandHead != NULL);
}

InputCommand getLastCommandIssued() {
    return lastCommand;
}




// Public Save Interface

void enqueueSave(struct OpenFile* file) {
    queueSaveFromOpenFile(file);
}



// Command Queue Interface

bool enqueueCommand(InputCommandMetadata meta) {
    if (!meta.targetPane) meta.targetPane = getCoreState()->focusedPane;
    CommandQueueItem* item = meta.targetPane ? calloc(1, sizeof(*item)) : NULL;
    if (!item) {
        fprintf(stderr, "[CommandBus] Could not queue %s\n", getCommandName(meta.cmd));
        discardPayload(meta);
        return false;
    }
    meta.originRole = meta.targetPane->role;
    item->editorTarget = meta.originRole == PANE_ROLE_EDITOR || meta.cmd == COMMAND_SAVE_FILE;
    if (item->editorTarget) {
        EditorView* view = commandView(meta.targetPane);
        OpenFile* file = getActiveOpenFile(view);
        if ((view && !bindId(&view->commandTargetId)) || (file && !bindId(&file->commandTargetId))) {
            discardPayload(meta);
            free(item);
            return false;
        }
        item->viewId = view ? view->commandTargetId : 0;
        item->fileId = file ? file->commandTargetId : 0;
    }
    item->meta = meta;
    item->next = NULL;

    if (!commandTail) {
        commandHead = commandTail = item;
    } else {
        commandTail->next = item;
        commandTail = item;
    }

    printf("[CommandBus] Queued command: %s (role=%d)\n",
           getCommandName(meta.cmd), meta.originRole);
    return true;
}

// Convenience helper if only cmd is known
void enqueueCommandSimple(InputCommand cmd) {
    IDECoreState* core = getCoreState();
    UIPane* target = core ? core->focusedPane : NULL;

    InputCommandMetadata meta = {
        .cmd = cmd,
        .originRole = target ? target->role : PANE_ROLE_UNKNOWN,
        .mouseX = -1,
        .mouseY = -1,
        .keyMod = SDL_GetModState(),
        .targetPane = target,
        .payload = NULL
    };

    enqueueCommand(meta);
}


 
// Final Command Dispatcher

void dispatchInputCommandWithMetadata(InputCommandMetadata meta) {
    IDECoreState* core = getCoreState();
    UIPane* target = meta.targetPane ? meta.targetPane
                                     : (core ? core->focusedPane : NULL);

    if (!target || !target->handleCommand) {
        printf("[CommandBus] No focused pane to dispatch command: %s\n",
               getCommandName(meta.cmd));
        discardPayload(meta);
        return;
    }

    if (!commandIsValidForRole(meta.cmd, target->role)) {
        printf("[CommandBus] Rejected: Command %s invalid for role %d\n",
               getCommandName(meta.cmd), target->role);
        discardPayload(meta);
        return;
    }

    printf("[CommandBus] Executing command: %s (role=%d)\n",
           getCommandName(meta.cmd), target->role);

    target->handleCommand(target, meta);
}
