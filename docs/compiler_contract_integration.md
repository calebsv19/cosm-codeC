# Compiler Contract Integration

This IDE consumes compiler analysis from `fisiCs` through a versioned data contract.

## Contract Boundary

- Contract ID: `fisiCs.analysis.contract`
- Producer: `fisiCs` frontend (`fisics_frontend`)
- Primary IDE ingest paths:
  - workspace/background analysis scan
  - live open-file analysis bridge

The authoritative contract specification lives in:

- `../fisiCs/docs/compiler_ide_data_contract.md`

## Compatibility Behavior

The IDE expects contract major version `1`.

When the incoming contract is incompatible (missing/invalid id or unsupported major):

- IDE enters degraded mode for that analysis payload
- diagnostics and include graph data are still consumed
- symbols and tokens are dropped for safety
- warning is emitted to stderr

## Why Degraded Mode Exists

This protects navigation/indexing/editor semantics from silently trusting incompatible payload shapes while preserving useful diagnostics and include visibility.

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

`diag` responses now include normalized taxonomy fields while preserving legacy compatibility:

- `severity` (legacy: `info`/`warn`/`error`)
- `severity_name` (normalized: `info`/`warning`/`error`)
- `severity_id` (0/1/2)
- `category` (current values: `build`, `analysis`)
- `code_id` (reserved numeric lane; `0` when unavailable)

## Contributor Notes

- Contract-shape changes should land with matching `fisiCs` docs/API updates.
- Contract changes require maintainer review.
- Avoid adding implicit assumptions in IDE code about compiler internals outside the contract.
