# IDE shared UI rollout — Main Edit development contract

Last updated: 2026-10-07

The retained lane `_worktrees/ide_main_edit` uses immutable shared source
`8744d0eff37c0c93b86d063f99e32db0c7f260a5`: kit_ui 0.18.0,
kit_pane 0.5.0, kit_render 0.15.0, vk_renderer 1.7.0 and vk_runtime 0.6.0.
Exact source parity is checked; version equality alone does not qualify a build.
The compiler is separately pinned in `make/fisics-source-pin.txt` to
cd1673aa05d8a1daf54bbfec37ac440ad7cb7be3 (fisiCs 0.7.0). Existing source VERSION 0.4.0,
canonical and installed/released packages retain separate identities.

## Adoption by surface

| Surface | Shared adoption now | Remaining boundary |
| --- | --- | --- |
| Menu commands | Rounded appearance; stable IDs; press/release, focus and keyboard activation | Existing app command bus/policy retained |
| Icon rail | Rounded selected/disabled/focused state; seven stable action IDs and matching release; numeric shortcuts preserved | Tool selection meaning remains local |
| Project toolbar | Fresh paint/input geometry; four command IDs; matching release; target, workspace-generation, modal/hidden and resize cancellation | Tree selection, drag/drop, file operations, rename and confirmation remain existing app owners |
| Control, Errors and Git header controls | Shared rounded state; semantic IDs, matching release and keyboard activation; painted geometry gated by pane/workspace/tool revision; disabled clears | Existing filter/batch/stage/commit bodies and continuous tree behavior remain app owned; field focus retains keyboard ownership |
| Libraries, Tasks and Assets headers | Shared release/focus semantics; view/selected-target revision gates and disabled Task Remove | Existing action bodies and continuous gestures retained |
| Terminal close/add/session tabs | Shared pointer release ownership and registry revision; measured UTF-8 captions | PTY keyboard/text/paste and app action policy retained |
| Editor tabs and close controls | Shared pointer release ownership per split leaf; file/tab, workspace, geometry and retirement cancellation | Keyboard/document selection/scroll/split semantics and command policy remain app owned |
| Fixed top-level panes | Shared kit_pane composition visible bounds, role-stable IDs, half-open hit policy; native nested clip intersection/restoration; one existing seam pass | Existing core_pane layout/topology and pointer capture remain local; generic KitPaneHost/docking not adopted |
| Native window | Shared observation; interruption cancellation and draft restoration; suspension and Vulkan recovery | macOS native source proof only; other platforms/external displays separate |
| Bounded Control/Errors search, Git message and Project inline rename | Shared KitUI UTF-8 scalar cursor, insert/delete; intact capacity/invalid-text rejection; safe rename initialization | Host focus/submit/persistence retained; retained selection/preedit and measured presentation adopted; Control/Errors/Git mixed traversal, measured caret/Shift-click/drag selection and pane/tool/workspace/window composition cancellation adopted; real native input-method review is recorded separately |
| Editor, terminal and global rename/modals | Existing document/PTY/modal policy retained; authoring takeover cancels scopes | Global rename scalar editing and measured presentation adopted; retained bounded selection/preedit adopted; document/PTY policy unchanged; human IME acceptance open |
| Font/Theme workspace authoring | Existing shared host/control contracts retained | No new authoring/persistence model introduced |

## Verification

Use the fresh retained source snapshot helper from the parent workspace; never
use cleanup as verification. `test-ide-menu-window-ui`,
`test-ide-shared-panel-ui`, `test-ide-panel-text-ui`, `test-ide-tool-chrome-ui`,
`test-ide-editor-header-ui`, `test-ide-rename-ui`, `test-ide-ui-button-adapter`
and headless smoke cover
production-linked control routing, appearance contracts, composition and existing
runtime regressions. Bridge suites retain fixed temporary fixture names and run
serially. Compiler/app/vendor outputs remain under the selected contained root.

Native opt-in `CODEWORK_WINDOW_LIFECYCLE_PROOF=<fresh directory>` drives the real
IDE window. `CODEWORK_IDE_UI_PROOF_PANELS=1` or `2` additionally selects existing
panel views for captures; it executes no build/run/file-delete actions. Isolate
HOME, cache, temp and `--workspace`; enable installed Khronos validation layers.
`CODEWORK_IDE_UI_PROOF_TEXT=1` adds isolated long UTF-8 field fixtures.
Dark/light runs each pass eight lifecycle stages and six captures with zero
validation warnings/errors and unchanged fixture documents. These are source
and native rendering checks, distinct from installed or human acceptance.

## Preservation and next rollout slices

Pre-existing work is checkpointed at aeb2721; preparation/shared pin at cd2c021.
Current UI slices are uncommitted. No broad clean, app replacement, canonical
adoption, version bump, release or push belongs to this checkpoint. Before any
Main Edit package refresh, preserve/read back the prior bundle and qualify a
fresh contained package with its own identity; installed review stays explicit.

Retained bounded selection/preedit and Control/Errors/Git mixed traversal are
adopted. Next boundaries are shared glyph fallback, pointer hit/drag selection,
real native IME and human/contained-package comparison. Retain editor document
storage, terminal PTY input, continuous tree/scroll/selection and splitter owners.

Evidence: parent `artifacts/ide/ui-menu-window-20261006/STATUS.md` and
`artifacts/ide/ui-panel-pane-20261006/STATUS.md`.

## 2026-10-06 — Discrete panel input and bounded UTF-8 text

Retained Main Edit adds shared release/focus ownership for Control filter/search/
collapse controls, Errors filter/batch/clear controls and Git stage/commit header
buttons. Existing action implementations retain purpose. Painted control geometry
is accepted only for the current pane bounds, workspace and tool generation;
stale snapshots require a repaint. Text-focused fields retain keyboard ownership.
Disabled clears do not activate. Pane retirement forgets borrowed scope pointers;
away/back tool changes cancel held presses, including the Project toolbar.

The shared KitUI text engine now owns UTF-8 scalar cursor movement, insertion and
deletion for Control/Errors search, Git message and Project inline rename buffers.
Edits reject malformed text, newlines or insufficient capacity intact. Rename
initialization fails closed rather than reusing a prior draft. App focus, submit,
commit, document storage, tree gestures and terminal PTY policy remain local.
Selection/preedit, measured scrolling/caret presentation, mixed traversal, global
rename and native IME acceptance remain follow-on work. Other discrete Libraries,
Tasks/editor/terminal groups are not claimed fully migrated.

Fresh contained full build and focused UI tests pass. Broad stable/headless,
bridge, document/workspace IPC and immutable source contract pass; an existing
Control test link dependency was repaired, with the initial failure retained.
Dark/light native rendering/lifecycle proof passes 16 stages and 12 captures,
zero validation warnings/errors, fixture documents unchanged. Header ownership
and UTF-8 behavior have automated source proof; native captures alone do not prove
interactive stage/commit or human IME acceptance. Preservation verifies 2,537
protected files, 76 prior vendor outputs and all 811 pinned shared source files.
Shared pin/API/VERSION and IDE VERSION 0.4.0 are unchanged. UI changes remain
uncommitted; installed bundles, canonical, release and push state are unchanged.
Contract: retained Main Edit `docs/shared_ui_rollout.md`; evidence:
`artifacts/ide/ui-discrete-text-20261006/STATUS.md`.


## 2026-10-06 — Tool headers, terminal pointer controls and measured fields

Retained Main Edit adopts shared KitUI release/focus ownership for Libraries,
Tasks and Assets headers. View-mode and selected-task changes invalidate held
presses; Remove is disabled without a selected task. Continuous gestures retain
their ending release when a header snapshot is invalidated.

Terminal close/add/session tabs adopt pointer release ownership, guarded by a
session-registry generation. The app retains create, close, select and interactive
double-click rename semantics. Registry changes disqualify a pending double-click.
PTY keyboard/text/paste policy remains local. UTF-8 tab captions are clipped using
measured presentation rather than truncated at a byte boundary.

Bounded fields now use pinned KitUI measured text presentation in the native
Vulkan painter: logical font metrics, horizontal caret reveal and parent clipping.
Selection/preedit, mixed traversal, global rename and human IME acceptance remain
open. CODEWORK_IDE_UI_PROOF_TEXT=1 populates isolated long UTF-8 field fixtures;
it does not submit Git, task or build actions.

Contained full build and focused production-adapter checks pass, including 40
panel header IDs, terminal action callbacks and proportional UTF-8 caret/viewport
checks. Broad headless, bridge, terminal and document/workspace IPC checks pass.
An initial new test-stub signature failure was fixed; its failed log is retained.
Dark/light native rendering/lifecycle proof passes 16 stages and 12 captures,
zero validation warnings/errors, fixture files unchanged. Native rendering proof
is separate from human interaction and installed-package acceptance.
Preservation verifies 2,537 protected files, 76 prior vendor outputs and all 811
pinned shared source files. Shared pin/API/VERSION and IDE VERSION 0.4.0 are
unchanged. UI changes remain uncommitted; canonical and installed bundles remain
preserved. Remaining editor-specific controls and retained text/modal policy are
follow-on work. Evidence: artifacts/ide/ui-tool-chrome-20261006/STATUS.md.


## 2026-10-07 — Editor headers and global rename

Retained Main Edit adopts pinned KitUI pointer release ownership for editor tabs
and close controls in up to eight split leaves. Painted file ordering, active tab,
tab scroll, root/leaf identity, pane/view geometry and workspace changes invalidate
held presses. Tab removal/addition, keyboard switching and view retirement revoke
borrowed snapshots before reuse. Close still uses the existing command bus; tab
activation retains app state synchronization. Keyboard Tab/Enter/Space, document
text/selection, scrollbars and split dividers retain their existing owners.

Global rename now receives whole UTF-8 text events, with shared bounded scalar
movement, insertion, backspace, forward delete and Home/End. Invalid or oversized
edits reject intact; malformed/oversized initialization fails closed rather than
truncating. Existing validate, unchanged-name, Shift-submit, complete and cancel
policies retain ownership. The popup adopts the measured field painter, with caret
blink separated from focus and horizontal reveal. Beginning rename revokes held
button scopes. Selection/preedit, mixed traversal and human IME acceptance remain
follow-on work; this is not a replacement of document or callback lifetime policy.

Contained full build, production editor/rename routing checks, prior UI suites,
broad headless/bridge/terminal and document/workspace IPC checks pass. Editor tests
cover all eight leaves and sixteen tab IDs; rename checks use recording callbacks,
not real file renames. Initial test-signature/include and font-tier compile errors
were corrected; failed attempt logs are retained. Dark/light native rendering and
lifecycle proof passes 16 stages and 12 captures, zero Vulkan validation warnings
or errors, isolated fixture documents unchanged. Opt-in
CODEWORK_IDE_UI_PROOF_RENAME=1 shows a long UTF-8 rename popup without a completion
callback, and split editor fixtures exercise both leaf header render paths.

Preservation verifies 2,537 protected files, 76 prior vendor outputs and 811 pinned
shared source files. Shared pin/API/VERSION and IDE VERSION 0.4.0 are unchanged;
source remains uncommitted, installed bundles and canonical remain preserved.
Release impact: minor release recommended for eventual public adoption; this
source checkpoint performs no version, package, canonical or publication action.
Remaining selection/preedit, mixed traversal and human/installed qualification
are explicit next boundaries. Evidence: artifacts/ide/ui-editor-rename-20261007/STATUS.md.


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


2026-10-07 retained text and mixed traversal source checkpoint

Control/Errors search, Git message, Project inline rename and global rename now
retain KitUI selection anchors and composition in their owning controllers.
Shift navigation, select all, copy/cut/paste, and scalar edit mechanics use the
pinned shared text API. Clipboard tests use a mock and never overwrite the user
clipboard; failed copy prevents cut. Composition does not mutate committed text,
refresh search results or submit rename/commit. Focus departure cancels preedit;
new drafts reset retained state. Native fields paint selection and preedit through
KitUI text presentation, with SDL candidate positioning only for the active native
window. Project inline rename now uses the measured painter instead of an underscore.

Control/Errors/Git mixed Tab/Shift-Tab traversal uses one shared interaction ring
in visual order; disabled controls are skipped. Text owns editing and Enter;
terminal PTY Tab remains local. Modified Tab does not enter the panel ring. Existing
workspace/tool/geometry/modal/retirement guards remain in force. Mouse click-to-end
is preserved; pointer drag selection is not implemented in this checkpoint.
Git metadata and ordinary tree/section text now use shared palette foregrounds;
explicit tree color overrides and status colors remain supported.

Contained full build and production UI, rename, editor, terminal ownership,
headless/bridge, document/workspace IPC and Vulkan contract pass. Initial missing
text-type include and terminal Tab regression were corrected; attempt logs retained.
Final contrast correction passes rebuilt app, shared theme/font adapter and Vulkan
contract. Dark/light native proof passes 16 stages and 12 captures with zero Vulkan
validation warnings/errors and unchanged fixture C documents. Captures visually
confirm selection, preedit underline and restored light Git caption contrast.
These synthetic preedit events do not prove a real macOS input-method session.

Remaining: glyph fallback for mixed CJK/emoji fonts, real native IME candidate/
commit/cancel acceptance, pointer hit/drag selection, and human/contained-package
comparison. core_font fallback_path selects an alternate file if the primary is
unavailable; it is not a glyph cascade. KitUI measurement/presentation accepts a
host callback, so a shared font-run adapter must keep glyph resolution, advances,
cache/DPI and painting consistent. Do not hand-edit the pinned vendor to simulate
adoption. Colored status rows still need light-theme semantic contrast acceptance.
Editor document and terminal text engines keep their existing owners.

Shared SDK pin and APIs, IDE VERSION 0.4.0, canonical checkout and installed
bundles remain unchanged. No clean, commit, app replacement or publication occurred.
Evidence: artifacts/ide/ui-retained-text-20261007/STATUS.md.


2026-10-07 installed review correction: icon commands now reveal a hidden tool
panel and request layout redraw, preserving unrelated pane visibility. The prior
fixture always began visible and missed the user's saved hidden presentation.
Editor padding is 2 pixels and split gap 2 pixels (6-pixel combined seam). The
editor close button paints a fixed geometric X to avoid clipping at font zoom +2.
Native opt-in CODEWORK_IDE_UI_PROOF_ICON_CLICK=1 exercises queued icon selection
from a hidden panel and asserts visible/selected state for every lifecycle phase.
The corrected development candidate is retained separately under parent artifacts/
ide/ui-visual-repair-20261007; running app replacement requires a normal close.

## Tool toggles and editor collapse refinement — 2026-10-07

Selecting the visible active tool collapses the tool panel. Selecting a different
tool opens or switches it; selecting the hidden active tool reopens it. Control
and Terminal visibility are preserved. The existing KitUI button scopes retain
release and cancellation ownership; this selection policy remains IDE-local.

Empty split leaves now publish an enabled close action routed through the existing
empty-leaf collapse operation. The sole empty root remains a valid file-opening
target. Editor padding and split gap are zero, so adjacent leaf bounds meet exactly
on both axes. No user split ratios, documents or preferences are reset.


## Standardization closeout, 2026-10-07

This entry supersedes the earlier remaining-source-work lists above. All seven
tool icons toggle the visible selected panel, reopen a hidden panel and switch
to another panel. Editor leaves meet at exact pixel seams (padding and split
gap zero); empty split leaves close through the same geometric X and queued
close command as populated views. The sole empty root remains an open-file target.

Control/Errors/Git bounded fields now publish copied, premeasured KitUI
presentations for pointer caret, Shift-click and drag selection. No borrowed
font or stack pointer survives paint. Geometry, workspace, selected tool, modal
and owner guards invalidate old controls; pane/tool/workspace/window transitions
blur obsolete fields and cancel retained composition without committing preedit.
The native candidate rectangle is emitted only by the actual focused panel.
Tree status text resolves shared warning/error/accent/text colors in light and
dark presets, retaining explicit host color overrides.

kit_render 0.15.0 provides the optional macOS native font adapter. IDE uses the
same CoreText line for missing CJK/color emoji measurement and 1x/2x rasterization;
all field prefix hits use that line's font path. Existing ASCII and non-native
paths remain. Font role/zoom/cache/clip and editor/PTY behavior stay app owned.

Persisted build and analysis diagnostics are optional derived data. Each uses a bounded pair of
background readers handling at most 1 MiB from regular, non-symlink cache files.
Reads cannot block UI startup; results older than two seconds, a newer build
or a workspace generation are discarded. Saturation skips the optional cache.
No retained output or user workspace is deleted to recover startup.

Compiler 0.7.0 is qualified from its clean immutable source revision; all
compiler and SDK outputs remain inside fresh task-owned build roots. The full
source gate includes cache safety, field geometry/ownership, stable/headless,
IDE bridge, document/workspace IPC and Vulkan source parity checks. See the
parent completion packet `artifacts/ide/ui-standardization-closeout-20261007`
for package/native/runtime receipts, preservation hashes and checkpoint identity.
Source VERSION remains 0.4.0. Public release, version selection, signing,
notarization and publication remain the release-control workflow.

Final native review corrected raster orientation and added an asymmetric-glyph
regression. Window interruption clears panel pointer drafts and rename preedit
while retaining committed rename text. The synchronous diagnostics report API
remains available to headless callers; startup uses background byte reads and
UI-thread parsing. Analysis updates invalidate pending cached reports.
