# Main-Thread Wake

This module lets worker threads wake the SDL event loop safely.

| File | Responsibility |
| --- | --- |
| `mainthread_wake.h/c` | Registers the custom wake event type, pushes wake events, detects received wake events, and tracks wake statistics. |

Use this when background work needs to wake the main loop without spinning.
