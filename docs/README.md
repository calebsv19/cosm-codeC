# IDE Docs Index

Start here for public IDE documentation.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: scaffold convergence intent and next migration phases.
- post-scaffold font-size lane is complete and tracked in `docs/current_truth.md`.
- migration-friendly verification gate lanes:
  - `make -C ide run-headless-smoke`
  - `make -C ide visual-harness`
  - `make -C ide test-stable`
  - `make -C ide test-legacy`

## Existing Public Docs
- `docs/keybind_reference.md`
- `docs/current_architecture_status.md`
- `docs/desktop_packaging.md`

## Private Planning Docs
- Active private scaffold docs live in:
  - `../docs/private_program_docs/ide/`
- historical private docs migrated from this lane are under:
  - `../docs/private_program_docs/ide/program_docs_migrated/`
