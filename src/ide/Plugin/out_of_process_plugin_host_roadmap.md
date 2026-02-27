# Out-of-Process Plugin Host Roadmap

## Goal

Replace in-process plugin loading with an isolated plugin host process and a constrained IPC contract.

## Why

- In-process plugins currently share full trust with the IDE core.
- A bug or compromise in plugin code can directly impact IDE integrity and user files.
- Process separation is required for meaningful containment.

## Migration Phases

### 1. Stabilize Plugin API Surface

- Define a small, versioned plugin API contract.
- Split APIs into read-only vs mutating capabilities.
- Add explicit capability declarations to manifests.

### 2. Build Plugin Host Process

- Create `ide-plugin-host` executable.
- Host loads plugin dynamic libraries instead of IDE core.
- Host runs with narrowed environment and explicit working directory.

### 3. IPC Contract and Mediation

- Define structured RPC between IDE core and host.
- Enforce command authorization and argument validation in the core before forwarding.
- Add request/response timeouts and failure isolation.

### 4. Policy Enforcement

- Require signed or hash-pinned plugin manifests.
- Bind capability grants to manifest declarations.
- Add per-plugin enable/disable controls and audit logs.

### 5. Runtime Hardening

- Add OS-level sandbox controls where available.
- Limit filesystem and network access for host process.
- Add crash/restart containment so plugin failure does not crash IDE core.

## Non-Goals (Current Baseline)

- No in-process plugin sandboxing claims.
- No cross-user plugin trust model.
- No remote plugin marketplace trust chain yet.
