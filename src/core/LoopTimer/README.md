# Main-Thread Timers

This module schedules delayed and repeating callbacks on the main thread.

| File | Responsibility |
| --- | --- |
| `mainthread_timer_scheduler.h/c` | Registers one-shot/repeating timers, cancels them, fires due callbacks, and exposes scheduler stats. |

This is the timed-callback side of the custom loop infrastructure. It keeps
deferred work out of pane code and tied to the central loop.
