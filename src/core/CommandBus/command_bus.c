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
    struct CommandQueueItem* next;
} CommandQueueItem;

static CommandQueueItem* commandHead = NULL;
static CommandQueueItem* commandTail = NULL;




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
    dispatchInputCommandWithMetadata(item->meta);
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

void enqueueCommand(InputCommandMetadata meta) {
    CommandQueueItem* item = malloc(sizeof(CommandQueueItem));
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
        .payload = NULL
    };

    enqueueCommand(meta);
}


 
// Final Command Dispatcher

void dispatchInputCommandWithMetadata(InputCommandMetadata meta) {
    IDECoreState* core = getCoreState();
    UIPane* target = core ? core->focusedPane : NULL;

    if (!target || !target->handleCommand) {
        printf("[CommandBus] No focused pane to dispatch command: %s\n",
               getCommandName(meta.cmd));
        return;
    }

    if (!commandIsValidForRole(meta.cmd, target->role)) {
        printf("[CommandBus] Rejected: Command %s invalid for role %d\n",
               getCommandName(meta.cmd), target->role);
        return;
    }

    printf("[CommandBus] Executing command: %s (role=%d)\n",
           getCommandName(meta.cmd), target->role);

    target->handleCommand(target, meta);
}
