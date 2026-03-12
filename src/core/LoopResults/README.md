# Main-Thread Completed Results

This module owns per-subsystem worker-to-main-thread completed result queues.

| File | Responsibility |
| --- | --- |
| `completed_results_queue.h/c` | Accepts worker-produced result envelopes, keeps per-subsystem queues, and exposes centralized drain/stats APIs for deterministic main-thread apply. |

Workers push results only. The main thread drains and applies them in loop order.
