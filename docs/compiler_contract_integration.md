# Compiler Contract Integration

This IDE consumes compiler analysis from `fisiCs` through a versioned data contract.

## Contract Boundary

- Contract ID: `fisiCs.analysis.contract`
- Producer: `fisiCs` frontend (`fisics_frontend`)
- Primary IDE ingest paths:
  - workspace/background analysis scan
  - live open-file analysis bridge

The authoritative contract specification lives in:

- `../../fisiCs/docs/compiler_ide_data_contract.md`

## Compatibility Behavior

The IDE expects contract major version `1`.

When the incoming contract is incompatible (missing/invalid id or unsupported major):

- IDE enters degraded mode for that analysis payload
- diagnostics and include graph data are still consumed
- symbols and tokens are dropped for safety
- warning is emitted to stderr

## Why Degraded Mode Exists

This protects navigation/indexing/editor semantics from silently trusting incompatible payload shapes while preserving useful diagnostics and include visibility.

## Include Dependency Lane

The IDE keeps compiler include data in two app-local analysis stores:

- `library_index`: bucketed header usage data for the Libraries panel's
  `Headers` view.
- `include_graph`: source-to-resolved-project-header dependency edges for
  invalidation and the Libraries panel's `Deps` and `Graph` views.

The `Deps` view is a first bridge surface over the existing include lane. It
does not execute manifest builds or replace makefile/build-system behavior.
The `Graph` view consumes the same `include_graph` snapshot API, translates it
into generic shared `kit_graph_struct` nodes/edges, and keeps source/header
meaning plus SDL panel drawing in IDE-owned code. This keeps the graph surface
usable without changing compiler payload shape or making the shared kit own IDE
semantics.

## Symbol Identity

For contract `1.1.x`, symbol payloads include `stable_id` (hex string) to support durable symbol cache/index matching across scans.
Missing `stable_id` is tolerated for older `1.0.x` payloads and treated as legacy data.

## Token IPC Lane

IDE IPC now exposes a `tokens` command for read-only token inspection.
Each token entry includes:

- `kind_id` (numeric `FisicsTokenKind` enum identity)
- `kind` (stable lowercase string form for JSON consumers)

This keeps enum identity stable while allowing simpler non-C consumers.

## Diagnostic Taxonomy Lane

`diag` responses include normalized taxonomy and source-span fields while
preserving legacy compatibility:

- `severity` (legacy: `info`/`warn`/`error`)
- `severity_name` (normalized: `info`/`warning`/`error`)
- `severity_id` (0/1/2)
- `category` / `category_name`
- `code` / `code_id` / `code_name`
- `stage`
- `length`, with `endCol` derived from `col + length`
- optional `hint`

Build diagnostics still report through the `build` source/category lane. Compiler
analysis diagnostics preserve the richer `fisiCs` diagnostic category/code/stage
metadata, including extension/unit diagnostics, through the IDE analysis store,
workspace persistence, and IPC `diag` responses.

## Diagnostic Explanation And Context Lanes

The IDE keeps compiler diagnostic explanation and structured context data in
app-local stores rather than changing the frontend ABI:

- `diagnostic_explanations` stores explanation catalog entries from `fisiCs`
  metadata or `--list-diagnostics --json`-shaped JSON.
- `diagnostic_context` stores optional emitted diagnostic JSON context such as
  `include_stack`, `macro_trace`, and units `details`.

When a diagnostic can be matched by stable location/code/message identity, the
`diag` IPC response may include optional `explanation`, `include_stack`,
`macro_trace`, and `details` fields. Missing optional context remains normal
for older producers and diagnostic families that do not emit it.

## Units Attachment Lane

When `FISICS_CONTRACT_CAP_EXTENSION_UNITS_ATTACHMENTS` is advertised, the IDE
stores declaration-level units attachments by `symbol_stable_id`. When
`FISICS_CONTRACT_CAP_EXTENSION_UNITS_CONCRETE` is also advertised, concrete
unit identity fields are retained.

The `symbols` IPC response can include an optional nested `units` object on
matching symbols. This is a consumer publication surface for unit/dimension
inspectors; expression-side units metadata remains outside the current public
contract.

## Analysis Authority And Provenance Boundary

The frontend contract already emits producer name/version, contract version,
source hash/length, capability flags, and `partial` / `fatal` result state.
The IDE now retains that per-file envelope in the IDE-owned
`analysis_provenance` store alongside the existing diagnostics, symbols,
tokens, units, and graph stores. Each record carries the advertised and
effective capabilities, compatibility/degraded reason, analysis generation,
source-match result, and whether it was restored from cache.

Authority state is classified as `current`, `stale`, `partial`, `degraded`, or
`fatal`. A persisted record is deliberately restored as `stale` until a fresh
analysis result proves that its source hash and length match the analyzed
buffer. Full and incremental scans update or remove records with their other
analysis payloads, while live-buffer analysis publishes the same envelope.

The read-only IPC command `analysis_state` returns a workspace summary and
copied per-file records, with an optional `args.file` filter. The Control panel
shows the worst current authority state and per-state counts, so row presence
is no longer presented as proof that cached compiler facts are authoritative.
The persisted artifact is `ide_files/analysis_provenance.json` with schema
`ide.analysis_provenance` version 1.

This is an IDE storage/publication refinement over fields that already exist;
it did not require a compiler ABI change.

## Snapshot And Concurrency Boundary

IDEPRO-S2 makes the analysis-store lifetime rule explicit and uniform. Store
index/get/find functions expose borrowed views only while the owning store lock
is held. IPC and UI consumers copy those views into owned JSON, tree, row,
marker, or other projection records before releasing the lock; rendering and
transport never retain store pointers. Compiler diagnostics IPC now projects
directly from the guarded analysis store instead of traversing the mutable
legacy diagnostics singleton. The token store now applies the same lock to
writes, multi-step reads, and persistence, closing the one previously unlocked
compiler lane.

Provenance uses a bulk copy-out API so records, summary counts, and the store
stamp come from one lock acquisition. This is per-store snapshot consistency,
not a new cross-store transaction or cache layer: main-thread apply remains the
publication boundary and provenance remains the authority signal across a
refresh.

Contract proof is part of the stable headless gate:

- a test-only executable links the real `libfisics_frontend` archive and proves
  diagnostics, symbols, tokens, resolved and unresolved includes, parent stable
  IDs, units attachments/capabilities, and valid versus partial/fatal state;
- a synthetic compatibility matrix proves inferred capabilities for contract
  1.0, 1.2, and 1.3, explicit flags from 1.4 onward, current units capabilities,
  and safe degradation for an unsupported major;
- a concurrent writer/reader test proves guarded token views and immutable
  provenance copy-out after source-store mutation.

These fixtures compile only under `build/.../tests`. They are absent from the
application source list and package resources, and no frontend ABI changed.

## Planned Semantic Growth Boundary

Future compiler growth should be capability-gated and consumer-oriented:

1. expression units facts bound to stable file spans and optional owning
   symbol identity;
2. reference edges for read, write, call, type-use, definition, and macro-use
   relationships;
3. compact canonical type facts for symbol and expression inspection.

The compiler's internal AST/IR is not a planned IDE contract. Include edges
remain the current compiler dependency graph, while `fisiCs.build_graph`
remains a separate sidecar for translation units and planned build actions.

## Sidecar Artifact Publication Lanes

Some bridge data is intentionally outside `fisiCs.analysis.contract` and is
ingested as explicit sidecar artifacts:

- `build_graph` can ingest `fisiCs.build_graph` JSON by `args.path` and return
  normalized top-level, translation-unit, planned-action, and diagnostic summary
  state.
- `memory_reports` can ingest `memory_check_report_v1` JSON by `args.path` and
  return normalized runtime report summaries and leak allocation sites.

These lanes are separate from compiler diagnostics and do not require a
frontend contract bump. They exist so IDE panels and external `idebridge`
consumers can opt into richer project/build/runtime state when artifacts are
available.

## Contributor Notes

- Contract-shape changes should land with matching `fisiCs` docs/API updates.
- Contract changes require maintainer review.
- Avoid adding implicit assumptions in IDE code about compiler internals outside the contract.
