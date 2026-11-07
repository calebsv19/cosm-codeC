/* Owned build child: exit and pipe EOF are independent; stop closes admission
 * and kills the process group before joining IPC clients. */
static atomic_bool ipc_build_stopping;
void ide_ipc_build_set_stopping(bool stopping) { atomic_store(&ipc_build_stopping, stopping); }
static int run_exec_capture(const char* working_dir, char* const argv[],
                            const char* profile, char* output, size_t output_cap,
                            size_t* out_len) {
    if (!working_dir || !*working_dir || !output || !output_cap ||
        !argv || !argv[0] || atomic_load(&ipc_build_stopping)) return -1;
    output[0] = 0; if (out_len) *out_len = 0;
    int fds[2]; if (pipe(fds)) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }
    if (!pid) {
        setpgid(0, 0);
        if (chdir(working_dir)) _exit(127);
        if (profile && *profile) setenv("PROFILE", profile, 1); else unsetenv("PROFILE");
        if (dup2(fds[1], 1) < 0 || dup2(fds[1], 2) < 0) _exit(127);
        close(fds[0]); close(fds[1]); execvp(argv[0], argv); _exit(127);
    }
    setpgid(pid, pid); close(fds[1]);
    int flags = fcntl(fds[0], F_GETFL, 0);
    if (flags < 0 || fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        kill(-pid, SIGKILL); kill(pid, SIGKILL); close(fds[0]);
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {} return -1;
    }
    bool exited = false, eof = false, cancelled = false;
    int status = 0; size_t written = 0;
    while (!exited || !eof) {
        if (atomic_load(&ipc_build_stopping)) {
            cancelled = true; kill(-pid, SIGKILL); kill(pid, SIGKILL);
        }
        char chunk[512]; ssize_t n;
        /* Bound each drain so a noisy child cannot starve cancellation. */
        for (int i = 0; i < 32; ++i) {
            n = read(fds[0], chunk, sizeof(chunk));
            if (n > 0) {
                size_t keep = (size_t)n < output_cap-1-written ? (size_t)n : output_cap-1-written;
                memcpy(output+written, chunk, keep); written += keep; output[written] = 0;
                build_diagnostics_feed_chunk(chunk, (size_t)n);
            } else { if (!n) eof = true; else if (errno != EINTR && errno != EAGAIN) eof = true; break; }
        }
        if (exited) break; /* one final nonblocking drain after direct-child exit */
        if (!exited) {
            pid_t done = waitpid(pid, &status, WNOHANG);
            if (done == pid || (done < 0 && errno == ECHILD)) {
                exited = true; kill(-pid, SIGKILL); /* descendants cannot retain the pipe */
            } else if (done < 0 && errno != EINTR) { cancelled = true; break; }
        }
        if (exited) {
            /* Read available trailing bytes once more, without waiting for an escaped descendant. */
            if (!eof) continue;
        }
        if (!exited || !eof) { struct pollfd p = {fds[0], POLLIN, 0}; poll(eof ? NULL : &p, eof ? 0 : 1, 10); }
    }
    close(fds[0]);
    if (!exited) { kill(-pid,SIGKILL);kill(pid,SIGKILL);while(waitpid(pid,NULL,0)<0&&errno==EINTR){} }
    if (out_len) *out_len = written;
    return cancelled ? -1 : (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
}
