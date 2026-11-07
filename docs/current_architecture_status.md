# IDE Current Architecture Status

Last updated: 2026-08-08
Audience: public/stable readers

## Summary

The IDE runtime is operating on a wrapper-owned lifecycle with an event-driven
main-thread apply model for analysis and UI-visible state.

Current architecture readers should treat `current_truth.md` as the compressed
source of public state. This file summarizes the architecture lanes that are
still current after the March scaffold/analysis baseline and the later wrapper,
bridge, cache-manifest, startup-audit, package, and release work.

## Lifecycle And Runtime

1. `main.c` delegates directly to `ide_app_main(...)`.
2. `ide_app_main.c` owns the canonical stage sequence:
   `bootstrap`, `config_load`, `state_seed`, `subsystems_init`,
   `runtime_start`, `run_loop`, and `shutdown`.
3. The wrapper records stage ownership, wrapper errors, dispatch count, and
   run-loop handoff status before shutdown.
4. The legacy editor runtime remains the fallback body behind the typed
   runtime-loop handoff.

The presentation backend is the vendored `vk_renderer 1.3.1`, whose Vulkan
instance/device/queue lifecycle is owned by vendored `vk_runtime 0.6.0`.
Compatibility remains at the renderer boundary, so IDE pane, editor, input,
and draw-command semantics do not own Vulkan lifecycle details. The IDE does
not currently adopt the runtime compute, residency, or timing APIs.

## Analysis And Main-Thread Apply

The March event-driven analysis model remains active:

1. worker jobs compute and publish results
2. the main thread is the visible apply authority for runtime state
3. stale analysis results are dropped before visible apply
4. pane updates route through runtime events and invalidation instead of
   unconditional redraw loops

Later analysis work extended this baseline with:

- cache metadata and artifact trust reporting through
  `ide_files/cache_manifest.json`
- startup audit summaries for trusted cache state, source coverage, dirty and
  removed counts, and refresh intent
- bounded optional token persistence and oversized-token pruning
- saved-file diagnostics priority/cancellation behavior for active startup or
  workspace analysis runs
- dedicated stores and IPC publication for diagnostics metadata,
  explanations, context, units attachments, build graph summaries, include
  graph data, and runtime memory-check report summaries

## UI And Bridge Consumption

The compiler bridge ingestion/publication boundary is complete for the current
contract. Current UI work should consume existing IDE-owned stores and IPC
surfaces instead of broadening the compiler ABI by default.

The Errors panel and Control panel now have richer diagnostic/context/units
consumers. The Control tree uses typed payloads, stable row IDs, precise row
hit testing, stable Active-scope file discovery, composed multi-target trees,
persisted filter/session state, and source-owned Units navigation so row
activation is independent of labels or stale user-data pointers. The Libraries
panel has include-dependency graph views backed by shared graph/viewport
helpers while keeping IDE-owned panel semantics.

Editor navigation now uses a shared goto path for file/line/column targets:
it places the cursor on the resolved location, frames long-file targets in the
upper reading band, and renders subtle cursor-row feedback in the line-number
gutter without changing the text layout.

The Git tool panel keeps its command execution app-local but no longer uses
shell-mediated `popen(...)`: branch, status, log, watcher, stage, and commit
commands are launched through a Git-panel argv process helper with child-side
project-directory switching.

## Current Validation Lanes

Vulkan-specific gates precede the broad application lanes:

1. `make -C ide vulkan-rollout-contract`
   - verifies the exact canonical shared snapshot and runtime/renderer versions
2. `make -C ide -j1 vulkan-rollout-self-test`
   - validation-enabled startup/readback/high-DPI/resize/recreate/capture/
     restart proof

Current baseline command lanes:

1. `make -C ide clean && make -C ide`
2. `make -C ide run-headless-smoke`
   - one-command headless proof; expected final line:
     `run-headless-smoke completed.`
3. `make -C ide visual-harness`
   - visual build-readiness proof; expected final line starts with:
     `visual-harness build gate ready:`
   - does not launch the interactive editor shell or capture a screenshot
4. `make -C ide visual-artifact`
   - display-backed first-frame proof; expected final line:
     `visual-artifact ready: visual_artifacts/ide_first_frame.bmp`
   - writes ignored `ide/visual_artifacts/ide_first_frame.bmp`
5. `make -C ide test-stable`
6. `make -C ide test-legacy`

Focused legacy and subsystem lanes such as `test-fast`, `test-idebridge`,
`test-extended`, and phase gates remain available for narrower diagnosis.

Packaged proof is available through `make -C ide package-desktop-self-test`;
the expected final line is `package-desktop-self-test passed.` Other
packaging validation remains available through `package-desktop*`, and release
validation through the notarized release target chain described in
`current_truth.md`.

## Analysis worker lifetime (L3)

The main owner retains a joinable analysis thread for workspace and live-buffer
runs. The worker publishes an atomic finished predicate only after its final
cleanup, result notification and wake. Poll joins finished threads before reusing
request storage. Shutdown closes admission, requests cooperative cancellation and
joins before shutdown persistence, UI, wake and result-queue teardown. Repeated
shutdown is harmless; explicit subsystem initialization enables a new lifetime.
A thread-creation failure releases the owned live-buffer copy.

Cancellation does not forcibly interrupt a compiler or filesystem call: shutdown
waits for that operation to return to a cancellation checkpoint. This is not a
hard real-time shutdown bound. The combined finite L3 review now covers build/Git/IPC ownership and native
stale-owner/result acceptance; installation and release remain separate.

`tests/test_analysis_shutdown.py` links the actual analysis owner with a blocked
scan-entry fixture and contained app objects. It checks workspace/live-buffer join,
closed admission, launch failure, completion polling and reinitialization. Its
`--sanitize` flag instruments the analysis owner with ASan/UBSan; it does not imply
that the reused compiler frontend is fully instrumented.

## Completion pressure and cache-reader shutdown (L3)

One allocation-free terminal slot is reserved for the single admitted analysis
owner. Ordinary progress/projection hints retain their bounded queues and may be
refused under pressure. Push consumes its owned payload on acceptance or refusal.
Sequence assignment and publication are serialized. A finished worker is joined,
but scheduler admission stays busy until its terminal result is consumed; queue
initialization failure refuses analysis admission. Thread-creation failure emits
an error terminal. The accepted success terminal refreshes the current projection
and status even if ordinary hints were dropped. Existing stale-result guards apply.

Analysis and build diagnostics-cache readers retain joinable pthread handles.
Poll joins a completed reader before reclaiming its request. Shutdown closes
admission and joins both bounded reader slots outside the publication mutex before
releasing their buffers and dependent application resources. Reinitialization
starts a new lifetime. A stalled filesystem read must return before join completes;
this establishes quiescence, not a hard shutdown deadline.

`tests/test_l3_results.py --output <fresh-directory>` exercises all three full
queues, allocation refusal, held terminal ownership, and both real cache readers
with controlled read/join barriers, normally and under ASan/UBSan.
`tests/test_analysis_shutdown.py --terminal-pressure` verifies real-worker terminal
accounting, successor refusal until consumption, and launch failure; its existing
shutdown mode remains the retained worker-lifetime regression.

## Subprocess and result acceptance lifetimes (L3)

The terminal owns its direct child independently of PTY EOF. Shutdown kills its
owned shell and observed foreground process group and waits for the direct child;
interrupted waits retry. Terminal owner allocation precedes spawning. The Git argv
runner retries interrupted waits and kills/reaps a child if stream admission fails.

IPC admits at most sixteen joinable clients. Stop closes admission, interrupts
blocked socket reads/writes, cancels an active build process group, wakes pending
main-owner requests and joins clients before dependent application teardown.
Completed clients are joined before slot reuse. Build requests hold the workspace
transaction exclusively. The build capture loop distinguishes direct-child exit
from output EOF, bounds each drain, and stops descendants retaining output pipes.
Private `ide_ipc_clients.inc` and `ide_ipc_capture.inc` keep those lifetimes out of
the already large server/router implementation.

Document revisions use a main-owner monotonic clock across close/reopen and slot
reuse. Analysis messages bind the captured workspace generation; live terminal
messages also carry the captured document revision. The real event-loop acceptance
path rejects changed/closed/reopened documents, old workspace generations,
out-of-order runs and duplicate terminal delivery. Process-local identities are
not durable job/retry receipts.

The contained native first-frame proof exercises startup analysis and application
shutdown. Native result application under full completion pressure and same-path/workspace
lifetime replacement passed the actual frame loop and renderer. The remaining
synchronous Git/build, build-flag and picker capture paths use `OwnedProcess`,
which separates EOF from child exit, kills the owned process group after direct
child exit, preserves its status and reaps the direct child. Early stream close
cancels the owned group. Normal synchronous execution waits for its direct child;
there is no universal hard runtime deadline or control of escaped daemon sessions.
The combined finite L3 source review is complete. No installed-app or packaged
release acceptance is implied.
