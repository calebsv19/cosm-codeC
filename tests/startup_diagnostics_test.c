#include "test_tmp_root.h"
#include "app/GlobalInfo/startup_diagnostics.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void test_snapshot_records_startup_boundaries(void) {
    IdeStartupDiagnosticsSnapshot snapshot;

    ide_startup_diagnostics_reset();
    ide_startup_diagnostics_record_runtime_paths(true,
                                                 "" IDE_TEST_TMPDIR "/codework",
                                                 "env",
                                                 "" IDE_TEST_TMPDIR "/codework/ide/build/bin");
    ide_startup_diagnostics_record_workspace_selection("" IDE_TEST_TMPDIR "/missing",
                                                       false,
                                                       "" IDE_TEST_TMPDIR "/codework",
                                                       true,
                                                       "" IDE_TEST_TMPDIR "/codework",
                                                       true,
                                                       true);
    ide_startup_diagnostics_record_ipc_start(true,
                                             true,
                                             "listening",
                                             "" IDE_TEST_TMPDIR "/caleb_ide.sock",
                                             "session-123");

    assert(ide_startup_diagnostics_snapshot(&snapshot));
    assert(snapshot.runtime_paths_recorded);
    assert(snapshot.runtime_paths_ok);
    assert(strcmp(snapshot.runtime_resource_root, "" IDE_TEST_TMPDIR "/codework") == 0);
    assert(strcmp(snapshot.runtime_resource_source, "env") == 0);
    assert(snapshot.workspace_recorded);
    assert(!snapshot.workspace_requested_valid);
    assert(snapshot.workspace_default_valid);
    assert(snapshot.workspace_used_fallback);
    assert(snapshot.workspace_available);
    assert(strcmp(snapshot.workspace_selected_path, "" IDE_TEST_TMPDIR "/codework") == 0);
    assert(snapshot.ipc_recorded);
    assert(snapshot.ipc_attempted);
    assert(snapshot.ipc_running);
    assert(strcmp(snapshot.ipc_status, "listening") == 0);
    assert(strcmp(snapshot.ipc_session_id, "session-123") == 0);
}

static void test_summary_is_bounded_and_omits_auth_token(void) {
    IdeStartupDiagnosticsSnapshot snapshot;
    char line[1024];

    ide_startup_diagnostics_reset();
    ide_startup_diagnostics_record_runtime_paths(true,
                                                 "" IDE_TEST_TMPDIR "/root",
                                                 "cwd_scan",
                                                 "" IDE_TEST_TMPDIR "/root/bin");
    ide_startup_diagnostics_record_workspace_selection("" IDE_TEST_TMPDIR "/root",
                                                       true,
                                                       "" IDE_TEST_TMPDIR "/default",
                                                       true,
                                                       "" IDE_TEST_TMPDIR "/root",
                                                       false,
                                                       true);
    ide_startup_diagnostics_record_ipc_start(true,
                                             false,
                                             "start_failed",
                                             NULL,
                                             NULL);

    assert(ide_startup_diagnostics_snapshot(&snapshot));
    assert(ide_startup_diagnostics_format_summary(line, sizeof(line), &snapshot));
    assert(strstr(line, "[StartupDiagnostics]"));
    assert(strstr(line, "source:cwd_scan"));
    assert(strstr(line, "fallback:0"));
    assert(strstr(line, "running:0"));
    assert(strstr(line, "status:start_failed"));
    assert(!strstr(line, "auth"));
    assert(!strstr(line, "token"));
}

static void test_reset_clears_snapshot(void) {
    IdeStartupDiagnosticsSnapshot snapshot;
    ide_startup_diagnostics_record_runtime_paths(true, "" IDE_TEST_TMPDIR "/root", "env", "" IDE_TEST_TMPDIR "/bin");
    ide_startup_diagnostics_reset();
    assert(ide_startup_diagnostics_snapshot(&snapshot));
    assert(!snapshot.runtime_paths_recorded);
    assert(!snapshot.workspace_recorded);
    assert(!snapshot.ipc_recorded);
}

int main(void) {
    test_snapshot_records_startup_boundaries();
    test_summary_is_bounded_and_omits_auth_token();
    test_reset_clears_snapshot();
    puts("startup_diagnostics_test: success");
    return 0;
}
