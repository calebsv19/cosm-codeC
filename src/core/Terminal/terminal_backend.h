#ifndef TERMINAL_BACKEND_H
#define TERMINAL_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

// Minimal PTY-backed shell instance. Output is stored as a raw byte stream
// (with CRLF normalized) for the UI layer to split into lines.
typedef struct TerminalBackend {
    int master_fd;
    pid_t child_pid;
    int rows;
    int cols;

    char* scrollback;
    size_t scrollback_len;
    size_t scrollback_cap;

    bool dead;
} TerminalBackend;

TerminalBackend* terminal_backend_spawn(const char* start_dir, int rows, int cols,
                                        const char* ide_socket_path,
                                        const char* project_root_path);
void terminal_backend_destroy(TerminalBackend* term);

// Returns total bytes read; -1 on fatal error. Non-blocking; safe to call when
// poll/select reports readability.
ssize_t terminal_backend_read_output(TerminalBackend* term);

// Returns false on fatal error (e.g., closed FD).
bool terminal_backend_send_input(TerminalBackend* term, const char* buf, size_t len);

// Check if the child exited; returns child's waitpid result (0 = still running).
int terminal_backend_poll_child(TerminalBackend* term);

// Update PTY size (rows/cols). Returns true on success.
bool terminal_backend_resize(TerminalBackend* term, int rows, int cols);

// Accessors for the raw scrollback buffer.
const char* terminal_backend_buffer(const TerminalBackend* term, size_t* out_len);

#endif
