# codeC persistent Main Edit development lane

This is the registered source lane for ordinary IDE feature and fix work.

- Canonical: `/Users/calebsv/Desktop/CodeWork/ide`, branch `main`.
- Main Edit: `/Users/calebsv/Desktop/CodeWork/_worktrees/ide_main_edit`, branch `codex/ide-main-edit`.
- Accepted entry base: `86017ba1f2e373ccdb3fdb3b5566b92b399cf305`, `VERSION` 0.4.0.
- Entry audit, 2026-09-13: canonical clean, no IDE specialist worktree or competing writer, divergence 0/0. `versionctl status --program ide` reported clean release-planning readiness. The older scaffold matrix's dirty-canonical exception is historical.

Before each edit, inspect both checkout heads/status, branch, VERSION, divergence,
worktree inventory and current writer. Keep one writer here. Never reset, clean,
recycle, or repurpose this retained lane to bypass unrelated work.

The lane passed an isolated MEW1 package self-test and live GUI/MCP proof on
2026-09-14. Use only
`package-desktop-main-edit`, `package-desktop-main-edit-self-test`, and
`package-desktop-main-edit-refresh` for the development bundle. The profile
uses `com.cosm.codec.mainedit`, `build/main-edit-dist/`, a distinct launcher
home/cache/log/runtime namespace, and an embedded source/binary identity.
The refresh target refuses an already occupied Desktop destination; it never
replaces the release `codeC.app`. A standard `package-desktop` cannot be
represented as Main Edit GUI proof.

The Main Edit launcher exports a private `agent-session.json` under its own
runtime root while an IDE session is live. An external MCP client must pin
this session file and the exact workspace; it should restart on a changed
session ID. The release app's IPC and preference namespaces remain separate.

Startup repair, 2026-09-30: Main Edit resolves `IDE_DEFAULT_WORKSPACE` from
the real user home before isolating preferences, preserving explicit overrides.
Workspace selection uses Cmd/Ctrl+L (native chooser) or Cmd/Ctrl+Shift+L
(typed path). Error popups own their overlay geometry and do not paint a
placeholder pane at the top left. Project loading does not follow directory
symlinks or descend into hidden metadata, build, dist, node_modules,
_worktrees, artifacts, or ide_files folders; these folders remain listed and
can be selected explicitly as a workspace root. Regular file links remain
visible. `test-project-directory-scan` covers source retention, generated
folders, and a link cycle.

Source verification for IDE bridge edits: `make debug` with the pinned compiler, focused
`test-idebridge-phase4`, `test-idebridge-regression`, then
`run-headless-smoke` at a meaningful checkpoint. `git diff --check` applies
to each checkpoint. Label these source commands `sandbox_safe`; interactive
IDE and ScreenCaptureKit proof are `gui_hidden_window`/`host_required`.

Current development candidate: one-file visible editor apply and saved readback,
caller-bound bridge hash, nine-tool local MCP adapter, source-bound C division
lesson step, bounded `build/app` execution with exact executable SHA-256, and
a main-thread Run-terminal presentation command for the last matching run.
The MCP adapter also binds a selected compiler-output view to the last build
hash and same IDE session, including an intentionally failed overlay build.
The Run task grid is sized for the visible pane before the first result is
fed, so its initial render does not drop a line during resize.
The separate bundle passed codesign/identity/launcher self-test and live
window review. The lesson edit, MCP connection, build, and before/after Run
terminal outputs passed. A 30-second exact-window Desktop Capture session
was finalized and imported into Capture; its four-clip, 13-second cropped
project `rev-CrxKvU1w` rendered and passed loader-reopen and full-decode
verification. The project and preview are under
`artifacts/ide/lesson-control-20260913/`. Human editorial acceptance and
voiceover remain open; they are not implied by decode verification.

A pinned fisiCs physics-units lesson is under
`artifacts/ide/lesson-units-20260914/`. The isolated Main Edit session showed
the actual `m/s` versus `s` assignment diagnostic, applied a checked one-line
correction, built with the opt-in units overlay, and presented an exact-hash
`speed: 5.0 m/s` run. Its second exact-window take became a 12-second editable
Capture cut at `rev-0azCBdFn`; the first take was rejected after frame review
showed the edit happened outside the useful recording interval. The second
project reopened, fully decoded, and had sampled code/error/result crops.

Main Edit source work does not authorize canonical adoption, VERSION changes,
package publication, Registry mutation, release-app installation or refresh.
The Main Edit refresh target is a separate development-app action. Preserve
both source lanes and the released 0.4.0 application.

## Main Edit icon packaging

The existing product icon is retained at
`tools/packaging/macos/local_app_icon/AppIcon.icns`; this file is deliberately
excluded from the local-icon ignore rule so future source checkpoints and
worktrees retain it. Main Edit packaging requires an icon input. Its self-test
requires the bundled icon, the matching `CFBundleIconFile`, and byte equality
with the selected `.icns` input. Missing icons must fail rather than silently
produce a generic Desktop icon. Other local icon experiments remain ignored.

## Contained source verification

Use `make verify-contained` for a fresh build and source regression check. The
workspace helper `bin/verify_ide_contained.py` copies tracked and non-ignored
source inputs into a new retained `/private/tmp/ide-contained-*` directory.
It excludes prior outputs and runtime `src/ide_files/`, uses isolated HOME/TMPDIR,
and keeps source hashes, command results, and logs beside the new outputs.
The existing clean pinned compiler checkout is read-only; its frontend archive
and objects are built inside this new output root. `--output-root` refuses an
existing directory; only reviewed source build/test targets are accepted.

Broad `make clean` refuses deletion. Ordinary builds no longer invoke dependency
clean targets. For qualification after shared/header/compiler changes, use a
fresh contained build to avoid stale dependency objects. Existing `build/`,
package checkpoints, Desktop bundles and user projects remain retained.
Neither source qualification nor this preparation authorizes app replacement.
