#include "core/Ipc/ide_ipc_session_export.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    char root[] = "/tmp/ide-agent-session-test.XXXXXX";
    assert(mkdtemp(root));
    assert(chmod(root, 0700) == 0);
    assert(setenv("IDE_PROFILE", "main-edit", 1) == 0);
    assert(setenv("IDE_RUNTIME_DIR", root, 1) == 0);
    assert(ide_ipc_agent_session_write("/tmp/lesson", "/tmp/ide.sock",
                                       "session-one", "private-token"));
    char path[256];
    snprintf(path, sizeof(path), "%s/agent-session.json", root);
    struct stat st;
    assert(stat(path, &st) == 0 && (st.st_mode & 0777) == 0600);
    ide_ipc_agent_session_clear("other-session");
    assert(stat(path, &st) == 0);
    ide_ipc_agent_session_clear("session-one");
    assert(access(path, F_OK) != 0);
    assert(chmod(root, 0755) == 0);
    assert(!ide_ipc_agent_session_write("/tmp/lesson", "/tmp/ide.sock",
                                        "session-two", "private-token"));
    assert(chmod(root, 0700) == 0);
    assert(rmdir(root) == 0);
    puts("ide_ipc_session_export_test: ok");
    return 0;
}
