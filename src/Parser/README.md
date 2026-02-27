# Parser Scaffold

This directory holds a lightweight parser scaffold that is initialized by the
IDE at startup but remains intentionally narrow in scope.

| File | Responsibility |
| --- | --- |
| `language_parser.h/c` | Initializes/shuts down the parser state, parses a single buffer into a simple `ParsedFile` record, and exposes the last parsed result. |

This is not the main diagnostics engine. Workspace diagnostics, symbols, and
token data are currently driven by the Fisics bridge under
[`core/Analysis/`](../core/Analysis/README.md). Keep parser work here focused
on future syntax-aware editor features rather than the production analysis path.
