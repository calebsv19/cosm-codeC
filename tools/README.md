# IDE Tools

This directory contains local command-line and packaging helper lanes for
codeC/IDE.

| Lane | Responsibility |
| --- | --- |
| [`idebridge/`](idebridge/README.md) | Local bridge CLI used by tests and automation to exercise editor-facing bridge behavior. |
| `packaging/` | macOS app-bundle launcher, plist, and dylib-copy helpers used by package/release targets. |

Tool behavior changes should use the relevant focused make targets before
package or release checks. Documentation-only updates to this README do not
require rebuilding the tools.
