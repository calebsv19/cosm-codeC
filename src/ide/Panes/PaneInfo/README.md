# Pane Metadata

Lightweight abstractions shared by every pane. Keeping these definitions in
their own directory avoids circular dependencies between panes.

| File | Responsibility |
| --- | --- |
| `pane.h/c` | Defines the `UIPane` struct, constructor helpers, and role-agnostic utilities (hit testing, destruction). |
| `pane_role.h` | Enum of logical pane roles used by the command registry. |

Include these headers when you need to create or inspect panes outside their
home directories.
