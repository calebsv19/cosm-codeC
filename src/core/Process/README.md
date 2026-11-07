# Owned synchronous process streams

App-local POSIX adapter used by Git, fallback Build/Run, analysis build-flag discovery,
and the native workspace picker. `OwnedProcess` owns one direct child, its new process
group, and a nonblocking output stream. Read through `owned_process_read_line`, not
stdio directly. The adapter checks child exit independently of EOF, terminates remaining
group members and drains buffered output without waiting for inherited pipes.

`owned_process_close` reaps the direct child exactly once. Early close before output EOF
cancels the owned group; fully drained execution preserves its ordinary exit status.
Caller-owned output buffers bound retained bytes. A running synchronous command may
still take time to finish; there is no universal deadline or daemon/session ownership.
IPC build capture retains its separate cancellation-aware adapter.

No matching subprocess stream abstraction exists in the adopted core_jobs/core_workers/
core_io APIs. Shared extraction is deferred until another consumer contract is established;
this change adds no shared dependency or version-floor change.

`tests/l3_owned_process.c` forces pipe/fork/stream admission failure, interrupted waits,
child exit before EOF, EOF before child exit, nonzero status, early close, descendant
termination and reuse. Run normally and with ASan/UBSan. The Git regression and native
frame-loop probe exercise consumers.
