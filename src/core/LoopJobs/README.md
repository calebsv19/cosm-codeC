# Main-Thread Jobs

This module is the generic "run this back on the UI thread" queue.

| File | Responsibility |
| --- | --- |
| `mainthread_jobs.h/c` | Queues callbacks plus user data, executes them under a time or item budget, and exposes queue statistics for instrumentation. |

Use this when a worker thread needs to hand results back to code that must run
on the main thread.
