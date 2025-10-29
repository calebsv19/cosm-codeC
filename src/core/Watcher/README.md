# File Watcher

Keeps the IDE’s in-memory project tree in sync with disk by polling for
changes and signalling refreshes.

| File | Responsibility |
| --- | --- |
| `file_watcher.h/c` | Wraps platform calls to check project directories for modifications. Exposes `pollFileWatcher()` (called once per frame) and flags such as `pendingProjectRefresh`. |

The watcher is deliberately simple today; replace the polling logic with a
platform-specific watcher when needed.
