#include "core/Ipc/ide_ipc_session_export.h"

#include <fcntl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SESSION_NAME "agent-session.json"

static bool is_main_edit(void) {
    const char* profile = getenv("IDE_PROFILE");
    return profile && strcmp(profile, "main-edit") == 0;
}

static int private_runtime_fd(void) {
    const char* root = getenv("IDE_RUNTIME_DIR");
    if (!root || root[0] != '/') return -1;
    int fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool ide_ipc_agent_session_write_generation(const char* project_root,
                                 const char* socket_path,
                                 const char* session_id,
                                 const char* auth_token, uint64_t generation) {
    if (!is_main_edit()) return true;
    if (!project_root || !socket_path || !session_id || !auth_token) return false;
    int dirfd = private_runtime_fd();
    if (dirfd < 0) return false;

    json_object* payload = json_object_new_object();
    json_object_object_add(payload, "workspace_generation", json_object_new_int64(generation));
    json_object_object_add(payload, "schema", json_object_new_string("ide_agent_session_v1"));
    json_object_object_add(payload, "project_root", json_object_new_string(project_root));
    json_object_object_add(payload, "socket_path", json_object_new_string(socket_path));
    json_object_object_add(payload, "session_id", json_object_new_string(session_id));
    json_object_object_add(payload, "auth_token", json_object_new_string(auth_token));
    const char* body = json_object_to_json_string_ext(payload, JSON_C_TO_STRING_PLAIN);
    char tmp[80];
    snprintf(tmp, sizeof(tmp), ".agent-session.%ld.tmp", (long)getpid());
    int fd = openat(dirfd, tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    bool ok = fd >= 0;
    if (ok) {
        size_t size = strlen(body);
        size_t written = 0;
        while (written < size) {
            ssize_t count = write(fd, body + written, size - written);
            if (count <= 0) { ok = false; break; }
            written += (size_t)count;
        }
        if (ok) ok = write(fd, "\n", 1) == 1;
        if (ok) ok = fsync(fd) == 0;
        if (close(fd) != 0) ok = false;
    }
    if (ok) ok = renameat(dirfd, tmp, dirfd, SESSION_NAME) == 0;
    if (ok) ok = fsync(dirfd) == 0;
    if (!ok) unlinkat(dirfd, tmp, 0);
    json_object_put(payload);
    close(dirfd);
    return ok;
}

bool ide_ipc_agent_session_write(const char* root, const char* socket, const char* session, const char* token) {
    return ide_ipc_agent_session_write_generation(root,socket,session,token,1);
}

void ide_ipc_agent_session_clear(const char* session_id) {
    if (!is_main_edit() || !session_id || !*session_id) return;
    int dirfd = private_runtime_fd();
    if (dirfd < 0) return;
    int fd = openat(dirfd, SESSION_NAME, O_RDONLY | O_NOFOLLOW);
    if (fd >= 0) {
        struct stat st;
        char body[2048];
        ssize_t count = read(fd, body, sizeof(body) - 1);
        bool owned = fstat(fd, &st) == 0 && st.st_uid == geteuid() &&
                     (st.st_mode & (S_IRWXG | S_IRWXO)) == 0;
        close(fd);
        if (owned && count > 0) {
            body[count] = '\0';
            json_object* payload = json_tokener_parse(body);
            json_object* value = NULL;
            if (payload && json_object_object_get_ex(payload, "session_id", &value) &&
                strcmp(json_object_get_string(value), session_id) == 0) {
                unlinkat(dirfd, SESSION_NAME, 0);
                fsync(dirfd);
            }
            if (payload) json_object_put(payload);
        }
    }
    close(dirfd);
}
