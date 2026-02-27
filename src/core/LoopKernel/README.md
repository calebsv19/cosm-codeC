# Main-Thread Kernel

This is the coordination layer for the IDE's custom main-thread loop.

| File | Responsibility |
| --- | --- |
| `mainthread_kernel.h/c` | Initializes/shuts down the loop kernel, performs per-frame ticks, and tracks whether queued work requested another render/update pass. |

The kernel is the glue between the wake system, timers, queued jobs, and any
other work that needs to re-enter the frame loop predictably.
