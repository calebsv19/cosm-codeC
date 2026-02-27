# Main-Thread Messages

This queue carries structured messages from worker systems back to the main
thread.

| File | Responsibility |
| --- | --- |
| `mainthread_message_queue.h/c` | Pushes/pops typed messages, throttles noisy progress updates, replaces stale git snapshots, and tracks queue stats. |

The current message types cover analysis completion, progress text, and git
snapshot updates.
