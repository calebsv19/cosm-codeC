# Diagnostics Stub

This directory reserves space for future IDE diagnostics (linting, static
analysis, feedback from compilers). The current implementation is intentionally
minimal but provides a seam for panes to query status once the engine is
fleshed out.

| File | Responsibility |
| --- | --- |
| `diagnostics_engine.h/c` | Placeholder API that will eventually collect diagnostics, cache them, and expose tick hooks. For now it provides stub functions so callers can compile even though no diagnostics are produced. |

Add new diagnostics pipelines here; keep the API UI-agnostic so panes only
need to render whatever data you expose.
