#ifndef IDE_WORKSPACE_AUTHORING_PRESENTATION_UI_H
#define IDE_WORKSPACE_AUTHORING_PRESENTATION_UI_H

#include <stdint.h>

#include "app/GlobalInfo/workspace_authoring_projection.h"

typedef enum IDEWorkspaceAuthoringPresentationControlId {
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_NONE = 0,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PANEL,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_CONTROL_PANEL,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TERMINAL,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PREVIOUS,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_NEXT,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_EXPORT_PROFILE,
    IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_PREVIEW_PROFILE
} IDEWorkspaceAuthoringPresentationControlId;

typedef struct IDEWorkspaceAuthoringPresentationControl {
    IDEWorkspaceAuthoringPresentationControlId id;
    int x;
    int y;
    int width;
    int height;
    const char *label;
    uint8_t active;
} IDEWorkspaceAuthoringPresentationControl;

#define IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT 7u

uint32_t ide_workspace_authoring_presentation_build_controls(
    int viewport_width,
    const IDEWorkspaceAuthoringProjection *projection,
    IDEWorkspaceAuthoringPresentationControl *out_controls,
    uint32_t capacity);
IDEWorkspaceAuthoringPresentationControlId ide_workspace_authoring_presentation_hit_test(
    const IDEWorkspaceAuthoringPresentationControl *controls,
    uint32_t count,
    int x,
    int y);
const char *ide_workspace_authoring_presentation_tool_label(IconTool tool);

#endif
