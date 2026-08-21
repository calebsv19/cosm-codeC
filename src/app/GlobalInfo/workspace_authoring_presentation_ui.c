#include "app/GlobalInfo/workspace_authoring_presentation_ui.h"

#include "ide/Panes/IconBar/icon_bar.h"

static int presentation_contains(const IDEWorkspaceAuthoringPresentationControl *control,
                                 int x,
                                 int y) {
    return control && x >= control->x && y >= control->y &&
           x < control->x + control->width && y < control->y + control->height;
}

const char *ide_workspace_authoring_presentation_tool_label(IconTool tool) {
    const char *label = getIconLabel(tool);
    return label && label[0] ? label : "Project";
}

uint32_t ide_workspace_authoring_presentation_build_controls(
    int viewport_width,
    const IDEWorkspaceAuthoringProjection *projection,
    IDEWorkspaceAuthoringPresentationControl *out_controls,
    uint32_t capacity) {
    const int left = 16;
    const int top = 78;
    const int height = 30;
    const int gap = 8;
    const int visibility_width = 96;
    const int tool_width = 108;
    uint32_t count = 0u;
    int tool_x;

    if (!projection || !out_controls || capacity < IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT ||
        viewport_width < 420) {
        return 0u;
    }
    tool_x = left + (visibility_width + gap) * 3;
    if (tool_x + tool_width * 2 + gap > viewport_width - 16) return 0u;

    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PANEL, left, top, visibility_width, height,
        "Tool Panel", projection->tool_panel_visible ? 1u : 0u};
    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_CONTROL_PANEL, left + visibility_width + gap, top,
        visibility_width, height, "Control", projection->control_panel_visible ? 1u : 0u};
    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TERMINAL, left + (visibility_width + gap) * 2, top,
        visibility_width, height, "Terminal", projection->terminal_visible ? 1u : 0u};
    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PREVIOUS, tool_x, top, tool_width, height,
        "< Tool", 0u};
    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_NEXT, tool_x + tool_width + gap, top,
        tool_width, height, "Tool >", 0u};
    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_EXPORT_PROFILE, left, top + height + gap,
        132, height, "Save WAPP", 0u};
    out_controls[count++] = (IDEWorkspaceAuthoringPresentationControl){
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_PREVIEW_PROFILE, left + 132 + gap,
        top + height + gap, 132, height, "Preview WAPP", 0u};
    return count;
}

IDEWorkspaceAuthoringPresentationControlId ide_workspace_authoring_presentation_hit_test(
    const IDEWorkspaceAuthoringPresentationControl *controls,
    uint32_t count,
    int x,
    int y) {
    uint32_t i;
    if (!controls) return IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_NONE;
    for (i = 0u; i < count; ++i) {
        if (presentation_contains(&controls[i], x, y)) return controls[i].id;
    }
    return IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_NONE;
}
