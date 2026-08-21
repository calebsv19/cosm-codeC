#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/GlobalInfo/workspace_authoring_projection.h"
#include "app/GlobalInfo/workspace_authoring_presentation_ui.h"
#include "ide/UI/ui_state.h"

static void test_defaults_and_catalog(void) {
    IDEWorkspaceAuthoringProjection projection;
    ide_workspace_authoring_projection_defaults(&projection);
    assert(ide_workspace_authoring_projection_valid(&projection));
    assert(strcmp(ide_workspace_authoring_projection_module_id(IDE_WORKSPACE_AUTHORING_MODULE_PROJECT),
                  "ide.tool.project") == 0);
    assert(strcmp(ide_workspace_authoring_projection_module_id(IDE_WORKSPACE_AUTHORING_MODULE_VERSION_CONTROL),
                  "ide.tool.version_control") == 0);
    assert(ide_workspace_authoring_projection_module_id(IDE_WORKSPACE_AUTHORING_MODULE_COUNT) == NULL);
    assert(ide_workspace_authoring_projection_icon_for_module(IDE_WORKSPACE_AUTHORING_MODULE_ERRORS) == ICON_ERRORS);
}

static void test_capture_apply_and_revert(void) {
    UIState ui = {.toolPanelVisible = true, .controlPanelVisible = false, .terminalVisible = true};
    IDEWorkspaceAuthoringProjection captured;
    IDEWorkspaceAuthoringProjection draft;
    setActiveIcon(ICON_TASKS);
    assert(ide_workspace_authoring_projection_capture(&captured, &ui, getActiveIcon()));
    draft = captured;
    draft.tool_panel_visible = false;
    draft.control_panel_visible = true;
    draft.terminal_visible = false;
    draft.active_tool = ICON_ERRORS;
    assert(ide_workspace_authoring_projection_apply(&draft, &ui));
    assert(!ui.toolPanelVisible && ui.controlPanelVisible && !ui.terminalVisible);
    assert(getActiveIcon() == ICON_ERRORS);
    assert(ide_workspace_authoring_projection_apply(&captured, &ui));
    assert(ide_workspace_authoring_projection_capture(&draft, &ui, getActiveIcon()));
    assert(ide_workspace_authoring_projection_equal(&captured, &draft));
}

static void test_invalid_projection_is_non_mutating(void) {
    UIState ui = {.toolPanelVisible = true, .controlPanelVisible = false, .terminalVisible = true};
    IDEWorkspaceAuthoringProjection invalid = {false, true, false, ICON_COUNT};
    setActiveIcon(ICON_PROJECT_FILES);
    assert(!ide_workspace_authoring_projection_apply(&invalid, &ui));
    assert(ui.toolPanelVisible && !ui.controlPanelVisible && ui.terminalVisible);
    assert(getActiveIcon() == ICON_PROJECT_FILES);
}

static void test_presentation_controls_match_draft(void) {
    IDEWorkspaceAuthoringProjection projection = {true, false, true, ICON_ERRORS};
    IDEWorkspaceAuthoringPresentationControl controls[
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT];
    uint32_t count = ide_workspace_authoring_presentation_build_controls(
        1200, &projection, controls, IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT);
    assert(count == IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT);
    assert(controls[0].active == 1u);
    assert(controls[1].active == 0u);
    assert(controls[2].active == 1u);
    assert(ide_workspace_authoring_presentation_hit_test(
               controls, count, controls[0].x + 1, controls[0].y + 1) ==
           IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PANEL);
    assert(ide_workspace_authoring_presentation_hit_test(
               controls, count, controls[4].x + 1, controls[4].y + 1) ==
           IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_NEXT);
    assert(ide_workspace_authoring_presentation_hit_test(
               controls, count, controls[6].x + 1, controls[6].y + 1) ==
           IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_PREVIEW_PROFILE);
    assert(strcmp(ide_workspace_authoring_presentation_tool_label(projection.active_tool), "Errors") == 0);
    assert(ide_workspace_authoring_presentation_build_controls(
               320, &projection, controls, IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT) == 0u);
}

int main(void) {
    test_defaults_and_catalog();
    test_capture_apply_and_revert();
    test_invalid_projection_is_non_mutating();
    test_presentation_controls_match_draft();
    puts("workspace_authoring_projection_test: success");
    return 0;
}
