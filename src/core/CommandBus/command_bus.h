#ifndef COMMAND_BUS_H
#define COMMAND_BUS_H

#include <stdbool.h>

#include "command_metadata.h"

struct OpenFile;
struct InputCommandMetadata;

// ====================================
// 	Command Bus Lifecycle
// ====================================
void initCommandBus();
void shutdownCommandBus();
void tickCommandBus();
bool isCommandBusBusy();

// ====================================
// 	Command Dispatch Logic
// ====================================
void dispatchInputCommand(InputCommand cmd);
void dispatchInputCommandWithMetadata(InputCommandMetadata meta);
InputCommand getLastCommandIssued();


// ====================================
// 	Command Queue Interface
// ====================================
void enqueueCommand(InputCommandMetadata meta);
void enqueueCommandSimple(InputCommand cmd); // Optional helper

// ====================================
// 	Save System Interface
// ====================================
void enqueueSave(struct OpenFile* file);

#endif // COMMAND_BUS_H

