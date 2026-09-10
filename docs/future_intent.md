# ide Future Intent

Last updated: 2026-08-25

## Current Direction

IDE/codeC is past its baseline scaffold, wrapper, packaging, and release
migration lanes. Future work should treat those lanes as maintained contracts,
not as open migration projects.

Current public direction:

- preserve the required scaffold floor (`docs/`, `src/`, `include/`, `tests/`,
  `build/`) and local README coverage
- keep `main.c` as a thin delegator into `ide_app_main(...)`
- keep `ide_app_main.c` as the canonical lifecycle wrapper for `bootstrap`
  through `shutdown`
- keep target-aware build, package, release, and app-bundle commands stable
- keep workspace-root-first startup and `ide_files/` runtime artifact policy
  stable
- keep the compiler bridge ingestion/publication boundary stable and plan new
  work as UI/presentation or focused analysis-cache slices rather than broad
  ABI churn

## Professionalization And Compiler Intelligence Direction

The next IDE cycle is a staged professionalization program, not another
scaffold migration. Work should proceed in this order:

1. Preserve the compiler result trust envelope in IDE-owned state. Per-file
   analysis must retain producer/contract identity, source hash and length,
   capability flags, `partial`/`fatal` state, generation, and source/cache
   match so UI and IPC consumers can distinguish current, stale, partial,
   degraded, and fatal results.
2. Maintain the completed immutable/copy-out analysis publication boundary,
   real frontend-to-IDE fixture, and contract compatibility matrix before
   widening semantic data.
3. Finish presentation over data that already exists: build-graph summaries,
   runtime memory reports, and clearly separated include/build graph modes.
4. Improve daily-driver durability through crash recovery/autosave policy,
   dirty-buffer/tab-close behavior, session restoration, and closure of the
   remaining packaged terminal-readability acceptance boundary.
5. Add compiler semantics only through narrow additive capabilities with real
   consumers. Expression units facts come first, followed by reference edges
   and compact type facts; exporting the compiler's internal AST is not a goal.
6. Reconcile source, docs, package, compiler identity, signing, and release
   receipts before presenting a new public release, then maintain a curated
   demonstration workspace for diagnostics, units, and dependency workflows.

Each item is a separate implementation and verification boundary. Compiler
contract changes require matching `fisiCs` source/API/docs work; release,
publication, and deployment remain separately authorized actions.

## Maintenance And Refinement Boundaries

Near-term IDE work should be planned as bounded subsystem slices:

- analysis provenance/result-state plus snapshot/concurrency and real-contract
  proof are implemented. The proof exposed no need for token sharding or a
  broad cache-format rewrite; the next compiler-intelligence slice should
  present existing build-graph and memory-report data before adding semantics
- bridge UI follow-up should consume existing diagnostics, explanation,
  context, units, build-graph, and memory-report stores rather than extending
  the compiler ABI
- completed `idebridge` and terminal decompositions remain historical
  boundaries; future extraction should be behavior-focused and only occur as
  an affected owner is touched
- remaining Errors-panel or IPC decomposition belongs to the implementation
  slice that needs it and should not become a broad rewrite
- IPC security review is a separate future security pass, not part of routine
  structure calibration

## Verification Contract Target

Keep these command lanes stable:

- scaffold/current aliases:
  - `run-headless-smoke`
  - `visual-harness`
  - `test-stable`
  - `test-legacy`
- legacy and focused lanes:
  - `test-fast`
  - `test-idebridge`
  - `test-extended`
  - phase gates (`test-phase1` through `test-phase5`)
- packaging/release lanes:
  - `package-desktop*`
  - `release-contract`
  - `release-bundle-audit`
  - `release-sign`
  - `release-notarize`
  - `release-staple`
  - `release-verify-notarized`
  - `release-artifact`
  - `release-distribute`

## Release Lane Follow-Up

The repository source version is `0.4.0`. That source identity must not be
treated as a fresh release-candidate or installed-artifact proof without a
current package/sign/notarize/staple/readback pass. Routine release-lane work
should preserve notarized app-bundle distribution, target-scoped build
outputs, and transitive `@rpath` dylib handling. Version bumps, release
artifacts, publication, and deployment remain explicit actions and are not
authorized by documentation or implementation work alone.
