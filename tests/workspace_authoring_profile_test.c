#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "app/GlobalInfo/workspace_authoring_profile.h"
#include "app/GlobalInfo/workspace_authoring_profile_path.h"

static void test_export_import_round_trip(void) {
    char path[] = "/tmp/ide_wapp_profile_XXXXXX";
    IDEWorkspaceAuthoringProjection source = {false, true, false, ICON_VERSION_CONTROL};
    IDEWorkspaceAuthoringProjection imported = {true, true, true, ICON_PROJECT_FILES};
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    assert(ide_workspace_authoring_profile_export_file(path, &source) == IDE_WORKSPACE_AUTHORING_PROFILE_OK);
    assert(ide_workspace_authoring_profile_import_file(path, &imported) == IDE_WORKSPACE_AUTHORING_PROFILE_OK);
    assert(ide_workspace_authoring_projection_equal(&source, &imported));
    unlink(path);
}

static void test_invalid_input_does_not_mutate_projection(void) {
    char path[] = "/tmp/ide_wapp_invalid_XXXXXX";
    IDEWorkspaceAuthoringProjection unchanged = {true, false, true, ICON_TASKS};
    IDEWorkspaceAuthoringProjection expected = unchanged;
    const char bad[] = "not a WAPP profile";
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(write(fd, bad, sizeof(bad)) == (ssize_t)sizeof(bad));
    close(fd);
    assert(ide_workspace_authoring_profile_import_file(path, &unchanged) != IDE_WORKSPACE_AUTHORING_PROFILE_OK);
    assert(ide_workspace_authoring_projection_equal(&unchanged, &expected));
    unlink(path);
}

static void test_workspace_local_default_path(void) {
    char path[256];
    assert(ide_workspace_authoring_profile_default_path("/tmp/workspace", path, sizeof(path)));
    assert(strcmp(path, "/tmp/workspace/ide_files/workspace_authoring.wapp") == 0);
    assert(!ide_workspace_authoring_profile_default_path("", path, sizeof(path)));
}

int main(void) {
    test_export_import_round_trip();
    test_invalid_input_does_not_mutate_projection();
    test_workspace_local_default_path();
    puts("workspace_authoring_profile_test: success");
    return 0;
}
