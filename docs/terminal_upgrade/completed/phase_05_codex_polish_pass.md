# phase 05 codex focused polish pass

goal: perform final codex/cli visual correctness polish, lock in regression checks, and close the terminal upgrade sequence.

## scope

- codex-like transcript validation coverage
- final interaction/visual alignment fixes discovered during usage
- final north star closure and handoff notes

## out of scope

- large architecture rewrites
- deep performance rework (beyond low-risk cleanup)

## ordered execution checklist

1. transcript-driven validation
- [x] `[done]` add codex-like transcript fixture coverage for ANSI + UTF-8 + OSC behavior
- [x] `[done]` verify wrapping behavior stability with fixture checks

2. final interaction/visual polish
- [x] `[done]` fix remaining terminal interaction mismatch found in final pass
- [x] `[done]` confirm selection/cursor/viewport behavior remains usable

3. regression verification
- [x] `[done]` run project build and terminal checks
- [x] `[done]` ensure prior phase checks still pass

4. closure
- [x] `[done]` update north star to mark phase 5 complete
- [x] `[done]` move this file into completed directory

## completion gate for this phase

- [x] `[done]` codex-like transcript validation exists and passes
- [x] `[done]` final interaction polish fix applied
- [x] `[done]` compile passes
- [x] `[done]` north star phase checklist fully complete
- [x] `[done]` this file moved to `docs/terminal_upgrade/completed/`
