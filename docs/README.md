# codeC Docs Index

Start here for public codeC documentation.
Last audited: 2026-06-09.

Repository and source-level identifiers still use `ide`.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: scaffold convergence intent and next migration phases.
- post-scaffold font-size lane is complete and tracked in `docs/current_truth.md`.
- Intel macOS `x86_64` packaging parity is complete and tracked in `docs/current_truth.md` and `docs/desktop_packaging.md`.
- migration-friendly verification gate lanes:
  - `make -C ide run-headless-smoke`:
    aggregate non-interactive smoke coverage
  - `make -C ide visual-harness`:
    build-only visual readiness, not an unattended runtime pass
  - `make -C ide test-stable`
  - `make -C ide test-legacy`

## Existing Public Docs
- `docs/keybind_reference.md`
- `docs/current_architecture_status.md`
- `docs/desktop_packaging.md`
  - includes full `package-desktop*` flow, launcher `--print-config`, and launcher logfile checks
- `docs/compiler_contract_integration.md`
  - versioned `fisiCs` compiler contract boundary, compatibility behavior,
    degraded-mode policy, enriched diagnostics publication, units attachments,
    build graph summaries, and runtime memory report sidecar publication

## Private Planning Docs
- Active private scaffold docs live in:
  - `../../docs/private_program_docs/ide/`
- historical private docs migrated from this lane are under:
  - `../../docs/private_program_docs/ide/program_docs_migrated/`
