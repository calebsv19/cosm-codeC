#pragma once

#include "ide/Panes/PaneInfo/pane_role.h"
#include "command_metadata.h"

#include <stdbool.h>

#define MAX_ALLOWED_ROLES 4

typedef struct {
    InputCommand cmd;
    UIPaneRole allowedRoles[MAX_ALLOWED_ROLES];  // Ends with -1 sentinel
} CommandRule;

// Function to check if a role is allowed to handle a command
bool commandIsValidForRole(InputCommand cmd, UIPaneRole role);

// Optional: Get string version of a command name (for debugging)
const char* getCommandName(InputCommand cmd);

