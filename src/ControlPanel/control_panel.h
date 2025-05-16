#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include <stdbool.h>

bool isLiveParseEnabled();
bool isShowInlineErrorsEnabled();

void toggleLiveParse();
void toggleShowInlineErrors();

#endif

