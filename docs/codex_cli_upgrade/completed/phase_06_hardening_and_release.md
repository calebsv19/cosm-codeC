# phase_06_hardening_and_release

## goal
Finalize the IDE bridge for day-to-day Codex usage by adding a broad integration regression suite, protocol compatibility policy, multi-instance operations guidance, and a concrete Codex workflow walkthrough.

## scope
- In scope: integration verification, release-facing documentation, operations guidance.
- Out of scope: new feature commands or protocol version bump.

## execution checklist
1. [x] create phase-6 integration regression test covering routing and critical commands
2. [x] wire phase-6 test target into make workflow
3. [x] run phase-6 test and baseline regression tests, fixing failures
4. [x] document protocol compatibility policy (versioning, additive changes, deprecation rules)
5. [x] document command contracts and failure behavior in one reference doc
6. [x] document multi-instance operational behavior (`MYIDE_SOCKET`, `--socket`, PTY env)
7. [x] document final Codex loop walkthrough (`diag -> edit -> apply -> verify`)
8. [x] add release checklist for bridge readiness gates
9. [x] update north star phase status and move this doc to `completed/`

## deliverable checklist
- [x] integration tests for request routing and critical commands
- [x] protocol compatibility policy documented
- [x] operational docs for running with multiple IDE instances
- [x] final walkthrough for Codex loop: diag -> patch -> apply -> verify

## test checklist
1. [x] routing coverage includes success paths for ping/diag/symbols/includes/search/build/open/edit
2. [x] error coverage includes malformed request and unknown command handling
3. [x] regression run confirms prior phase tests still pass

## release gate
- [x] test suite covers protocol, connection, and key command regressions
- [x] docs fully describe command contracts and failure behavior
- [x] release checklist completed
