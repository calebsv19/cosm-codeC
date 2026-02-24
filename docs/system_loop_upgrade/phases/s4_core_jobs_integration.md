# S4 - `core_jobs` Main-Thread Job Budgeting

Status: Completed

## Goal

Introduce `core_jobs` as the bounded main-thread job runner and use it for loop-safe command bus ticking.

## Scope

- Add a `mainthread_jobs` wrapper module.
- Initialize/shutdown with system lifecycle.
- Queue command-bus tick as a main-thread job and execute with bounded per-frame work.

## Checklist

- [x] Add `mainthread_jobs` wrapper.
- [x] Integrate in startup/shutdown lifecycle.
- [x] Route command-bus ticking through `core_jobs`.
- [x] Build and verify compile.
- [x] Mark phase complete.
