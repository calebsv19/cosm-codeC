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

Shared UI preparation, 2026-10-06: existing development work is preserved at
`aeb2721`, build preparation at `fe6d4f6`, and managed shared import at `0001e8a`.
The immutable source pin is `make/shared-source-pin.txt`; contained verification
checks all vendored source files against that commit. The compiler remains
`83af7ab9c984981ae1f8044dd0f5b12caecfbaa9`. The importer updates tracked source
without deleting ignored consumer outputs and refuses untracked collisions.
Build/regression qualification is source proof. The first menu/window UI pilot
has passed focused tests and native source window lifecycle/validation proof.
Broader UI migration and installed visual acceptance remain pending. Some bridge fixtures still use fixed
`/tmp/idebridge_*` paths; serialize bridge suites. The contained helper supplies
a short unique IPC cache root to avoid Unix socket path truncation.


For the menu/window pilot, include `test-ide-menu-window-ui` and
`test-ide-ui-button-adapter` in contained verification. The optional actual-app
proof uses `CODEWORK_WINDOW_LIFECYCLE_PROOF=<fresh capture directory>` with
Khronos validation layer paths, isolated HOME/cache/temp and a disposable
`--workspace` root. It changes the test window through eight lifecycle stages,
records six BMP captures, and exits with a failure on validation warnings/errors.
It never invokes menu build/run commands or replaces installed apps. Evidence:
`artifacts/ide/ui-menu-window-20261006/STATUS.md` in the parent workspace.


The next shared UI slices extend compact appearance, icon/project release/focus
routing and fixed-pane composition. Include `test-ide-shared-panel-ui` in focused
contained verification. See `docs/shared_ui_rollout.md` for the surface matrix,
proof scope and pending text/modal and installed review boundaries.

Include `test-ide-panel-text-ui` for the Control/Errors/Git release scopes and
shared bounded UTF-8 mechanics. These checks record ownership/IDs and text-buffer
changes; they do not invoke Git staging/commit or file operations.

Include `test-ide-tool-chrome-ui` for production Libraries/Tasks adapters and
terminal header/action callbacks. `test-ide-panel-text-ui` also checks measured
UTF-8 caret/viewport presentation. Opt-in `CODEWORK_IDE_UI_PROOF_TEXT=1` populates
long field fixtures in the isolated native lifecycle proof; no submit actions.

Use `test-ide-editor-header-ui` and `test-ide-rename-ui` for the editor/rename
checkpoint. They record commands/callbacks without closing real files or renaming
filesystem objects. `CODEWORK_IDE_UI_PROOF_RENAME=1` adds an isolated long UTF-8
popup fixture to the native lifecycle proof, with no completion callback.

Final capacity review (2026-10-07): existing sessions may have more than sixteen
open files. Header snapshots therefore retain every live file identity, while
visible controls stay bounded by the shared scope capacity. An 80-tab fixture
proves late-index selection/close and cancellation after an off-screen file
changes. Eight split leaves retain independent scopes. The corrected final
snapshot passes the full contained and native verification gates. Current native
font presets display missing glyph placeholders for some Chinese/emoji text;
UTF-8 editing/retention passes, but glyph coverage and human IME acceptance remain
open visual qualification work. Source VERSION remains unchanged; the release
impact dry-run recommends a minor release for eventual public adoption.

The prior field adapter caret used an unmapped SDL floating-rectangle call. The
final painter uses the actual Vulkan-compatible rectangle call, and the Vulkan
contract now rejects unmapped SDL calls in native UI adapters. A negative fixture
confirms rejection; final native rename captures force the caret visible for
render review while ordinary blink policy is retained.

Rename blink timestamps and animation advancement now use the same SDL time
origin; the previous mixed-clock call could hide the caret immediately.
Deterministic blink-transition checks pass, and the opt-in native fixture
confirms a visible caret. Ordinary blink intervals retain their existing policy.

Native field/button captions, terminal labels and rename popup sizing now use
the existing UTF-8 width helper rather than byte-oriented text measurement,
matching the UTF-8 drawing path. Font fallback remains separately unqualified.

`CODEWORK_IDE_UI_PROOF_RETAINED_TEXT=1` adds synthetic selection/preedit to the
isolated lifecycle fixture. No callbacks rename filesystem objects. Retained text
and traversal are covered by `test-ide-panel-text-ui` and `test-ide-rename-ui`.

## Guarded Desktop refresh — 2026-10-07

`tools/packaging/macos/main_edit_package.py` now supports `--output-root` and
`--build-root` for a fresh retained package/build pair. The default legacy output
path still refuses a stale existing candidate; use a new pair for every refresh.
`verify --output-root <package-root>` checks source/binary identity, deep strict
codesign, matching required icon bytes and isolated launcher namespaces.

`refresh --output-root <package-root> --backup-root <fresh-retained-root>` is the
explicit occupied-destination path. It requires an idle Main Edit app, verifies
its bundle ID, stages beside the Desktop destination, compares file/symlink
manifests, rechecks occupancy and idle state, then atomically renames the old app
into the backup and the verified candidate into place. Failed install verification
restores the prior bundle and retains the failed candidate. Release codeC.app and
preferences/projects are outside this operation. This is local development
installation; it does not bump VERSION or publish/adopt canonical source.


### Standardization closeout (2026-10-07)

The current compiler pin is `make/fisics-source-pin.txt`, fisiCs 0.7.0 at
cd1673aa05d8a1daf54bbfec37ac440ad7cb7be3, superseding the older 83af7ab9
qualification. Current shared pin includes kit_render 0.15.0 native font
fallback. Both dependency sources remain read-only during builds.
The user authorized completing this rollout, closing/refreshing Main Edit and
creating a clean source checkpoint. Existing canonical runtime files and prior
Desktop packages remain retained. This authority does not publish a release.
See `shared_ui_rollout.md` and the parent standardization completion packet.
