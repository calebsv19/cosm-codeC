#include "terminal_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <util.h>
#else
#include <pty.h>
#endif

static bool get_executable_dir(char* out, size_t out_cap) {
    if (!out || out_cap == 0) return false;
    out[0] = '\0';

    char exe_path[PATH_MAX];
    exe_path[0] = '\0';

#if defined(__APPLE__)
    uint32_t size = (uint32_t)sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        return false;
    }
#else
    ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n <= 0 || n >= (ssize_t)sizeof(exe_path)) {
        return false;
    }
    exe_path[n] = '\0';
#endif

    char resolved[PATH_MAX];
    const char* path_in_use = exe_path;
    if (realpath(exe_path, resolved)) {
        path_in_use = resolved;
    }

    const char* slash = strrchr(path_in_use, '/');
    if (!slash || slash == path_in_use) return false;
    size_t dir_len = (size_t)(slash - path_in_use);
    if (dir_len + 1 > out_cap) return false;
    memcpy(out, path_in_use, dir_len);
    out[dir_len] = '\0';
    return true;
}

static void prepend_dir_to_path_if_needed(const char* dir) {
    if (!dir || !*dir) return;

    char idebridge_path[PATH_MAX];
    snprintf(idebridge_path, sizeof(idebridge_path), "%s/idebridge", dir);
    if (access(idebridge_path, X_OK) != 0) {
        return;
    }

    const char* old_path = getenv("PATH");
    if (old_path && *old_path) {
        size_t dir_len = strlen(dir);
        const char* p = old_path;
        while (p && *p) {
            const char* colon = strchr(p, ':');
            size_t seg_len = colon ? (size_t)(colon - p) : strlen(p);
            if (seg_len == dir_len && strncmp(p, dir, dir_len) == 0) {
                return;
            }
            p = colon ? colon + 1 : NULL;
        }
    }

    char new_path[8192];
    if (old_path && *old_path) {
        snprintf(new_path, sizeof(new_path), "%s:%s", dir, old_path);
    } else {
        snprintf(new_path, sizeof(new_path), "%s", dir);
    }
    setenv("PATH", new_path, 1);
    setenv("MYIDE_BIN_DIR", dir, 1);
}

static void append_bytes(TerminalBackend* term, const char* data, size_t len) {
    if (!term || !data || len == 0) return;

    if (term->scrollback_cap == 0) {
        term->scrollback_cap = 4096;
        term->scrollback = (char*)malloc(term->scrollback_cap);
        if (!term->scrollback) {
            term->scrollback_cap = 0;
            return;
        }
    }

    if (term->scrollback_len + len >= term->scrollback_cap) {
        size_t newCap = term->scrollback_cap * 2;
        while (newCap <= term->scrollback_len + len) {
            newCap *= 2;
        }
        char* newBuf = (char*)realloc(term->scrollback, newCap);
        if (!newBuf) return;
        term->scrollback = newBuf;
        term->scrollback_cap = newCap;
    }

    memcpy(term->scrollback + term->scrollback_len, data, len);
    term->scrollback_len += len;
}

TerminalBackend* terminal_backend_spawn(const char* start_dir, int rows, int cols,
                                        const char* ide_socket_path,
                                        const char* project_root_path,
                                        const char* ide_auth_token) {
    struct winsize ws = {
        .ws_row = (unsigned short)(rows > 0 ? rows : 24),
        .ws_col = (unsigned short)(cols > 0 ? cols : 80),
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };

    int master_fd = -1;
    pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);
    if (pid < 0) {
        perror("forkpty");
        return NULL;
    }

    if (pid == 0) {
        // Child: start shell
        if (start_dir && *start_dir) {
            chdir(start_dir);
        }
        // Ensure full terminal capabilities for interactive TUIs.
        const char* term = getenv("TERM");
        if (!term || !term[0] || strcmp(term, "dumb") == 0) {
            setenv("TERM", "xterm-256color", 1);
        }
        if (!getenv("COLORTERM")) {
            setenv("COLORTERM", "truecolor", 1);
        }
        setenv("TERM_PROGRAM", "caleb_ide", 1);
        char exe_dir[PATH_MAX];
        if (get_executable_dir(exe_dir, sizeof(exe_dir))) {
            prepend_dir_to_path_if_needed(exe_dir);
        }
        if (ide_socket_path && *ide_socket_path) {
            setenv("MYIDE_SOCKET", ide_socket_path, 1);
        }
        if (project_root_path && *project_root_path) {
            setenv("MYIDE_PROJECT_ROOT", project_root_path, 1);
        }
        if (ide_auth_token && *ide_auth_token) {
            setenv("MYIDE_AUTH_TOKEN", ide_auth_token, 1);
        }
        execl("/bin/zsh", "zsh", "-l", (char*)NULL);
        execl("/bin/bash", "bash", "-l", (char*)NULL);
        perror("execl shell");
        _exit(1);
    }

    // Parent
    int flags = fcntl(master_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);
    }

    TerminalBackend* term = (TerminalBackend*)calloc(1, sizeof(TerminalBackend));
    if (!term) {
        close(master_fd);
        kill(pid, SIGHUP);
        return NULL;
    }

    term->master_fd = master_fd;
    term->child_pid = pid;
    term->rows = ws.ws_row;
    term->cols = ws.ws_col;
    term->scrollback = NULL;
    term->scrollback_len = 0;
    term->scrollback_cap = 0;
    term->dead = false;
    return term;
}

void terminal_backend_destroy(TerminalBackend* term) {
    if (!term) return;

    if (term->master_fd >= 0) {
        close(term->master_fd);
    }

    if (!term->dead && term->child_pid > 0) {
        pid_t pid = term->child_pid;
        // Try SIGHUP, then SIGTERM, then SIGKILL, without blocking.
        kill(pid, SIGHUP);
        if (waitpid(pid, NULL, WNOHANG) == 0) {
            kill(pid, SIGTERM);
            if (waitpid(pid, NULL, WNOHANG) == 0) {
                kill(pid, SIGKILL);
                waitpid(pid, NULL, WNOHANG);
            }
        }
    }
    free(term->scrollback);
    free(term);
}

ssize_t terminal_backend_read_output(TerminalBackend* term) {
    if (!term || term->dead || term->master_fd < 0) return -1;

    ssize_t total = 0;
    char buf[1024];
    for (;;) {
        ssize_t n = read(term->master_fd, buf, sizeof(buf));
        if (n > 0) {
            append_bytes(term, buf, (size_t)n);
            total += n;
            continue;
        }
        if (n == 0) {
            term->dead = true;
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        term->dead = true;
        break;
    }
    return total;
}

bool terminal_backend_send_input(TerminalBackend* term, const char* buf, size_t len) {
    if (!term || term->dead || term->master_fd < 0 || !buf || len == 0) return false;

    size_t written = 0;
    while (written < len) {
        ssize_t n = write(term->master_fd, buf + written, len - written);
        if (n > 0) {
            written += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // Non-blocking; caller should retry later.
            break;
        }
        term->dead = true;
        return false;
    }
    return true;
}

int terminal_backend_poll_child(TerminalBackend* term) {
    if (!term || term->dead || term->child_pid <= 0) return -1;
    int status = 0;
    pid_t res = waitpid(term->child_pid, &status, WNOHANG);
    if (res > 0) {
        term->dead = true;
    }
    return (int)res;
}

bool terminal_backend_resize(TerminalBackend* term, int rows, int cols) {
    if (!term || term->dead || term->master_fd < 0) return false;
    if (rows <= 0 || cols <= 0) return false;
    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };
    if (ioctl(term->master_fd, TIOCSWINSZ, &ws) == -1) {
        return false;
    }
    term->rows = rows;
    term->cols = cols;
    kill(term->child_pid, SIGWINCH);
    return true;
}

const char* terminal_backend_buffer(const TerminalBackend* term, size_t* out_len) {
    if (!term) return NULL;
    if (out_len) *out_len = term->scrollback_len;
    return term->scrollback;
}
