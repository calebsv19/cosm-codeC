#include "terminal_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

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

TerminalBackend* terminal_backend_spawn(const char* start_dir, int rows, int cols) {
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

static void append_normalized(TerminalBackend* term, const char* buf, ssize_t len) {
    // Normalize CRLF → LF, but keep lone CR so the renderer can treat it as
    // carriage return rather than newline.
    char* tmp = (char*)malloc((size_t)len);
    if (!tmp) return;
    size_t out = 0;
    for (ssize_t i = 0; i < len; ++i) {
        char c = buf[i];
        if (c == '\r') {
            if (i + 1 < len && buf[i + 1] == '\n') {
                i++;  // consume LF
                tmp[out++] = '\n';
            } else {
                tmp[out++] = '\r';
            }
        } else {
            tmp[out++] = c;
        }
    }
    append_bytes(term, tmp, out);
    free(tmp);
}

ssize_t terminal_backend_read_output(TerminalBackend* term) {
    if (!term || term->dead || term->master_fd < 0) return -1;

    ssize_t total = 0;
    char buf[1024];
    for (;;) {
        ssize_t n = read(term->master_fd, buf, sizeof(buf));
        if (n > 0) {
            append_normalized(term, buf, n);
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
