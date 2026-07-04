# ide Future Intent

Last updated: 2026-06-17

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

## Maintenance And Refinement Boundaries

Near-term IDE work should be planned as bounded subsystem slices:

- analysis/cache follow-up should stay under the active cache/artifact plan
  and avoid token-sharding or cache-format rewrites unless source evidence
  requires them
- bridge UI follow-up should consume existing diagnostics, explanation,
  context, units, build-graph, and memory-report stores rather than extending
  the compiler ABI
- `ide/tools/idebridge/idebridge.c` extraction belongs to the active
  idebridge large-file decomposition plan
- Errors-panel decomposition belongs to the active bridge UI plan and should
  not be bundled into scaffold/docs refinement
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

The current public release is `0.3.0`. Routine release-lane work should
preserve notarized app-bundle distribution, target-scoped build outputs, and
transitive `@rpath` dylib handling. Version bumps remain explicit release
actions and should not be folded into docs-only refinement passes.
