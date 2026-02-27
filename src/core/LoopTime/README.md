# Loop Time

This module centralizes monotonic time helpers used by the loop subsystems.

| File | Responsibility |
| --- | --- |
| `loop_time.h/c` | Returns nanosecond and millisecond timestamps, computes differences, and converts nanoseconds to seconds. |

Keeping these helpers here avoids ad hoc timing code scattered across the
kernel, timers, and wake paths.
