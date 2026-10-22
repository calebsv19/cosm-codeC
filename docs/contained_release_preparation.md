# Contained release preparation

The Registry `snapshot_sandbox_v1` producer supplies `PREPARATION_ROOT` and
`TARGET_BUILD_ROOT` as the same fresh owned directory. Ordinary preparation goals
copy selected vendored shared inputs into that directory before invoking legacy
builders. The original source snapshot remains read-only. Output-root allocation
accepts absent source-local or selected data-root job/target directories and
refuses occupied destinations and symlinks.

Compiler inputs remain pinned by `make/fisics-source-pin.txt`. The committed
`make/fisics-snapshot-inputs.json` records exact bytes/modes for compiler build
inputs. `tools/packaging/verify_compiler_snapshot.py` verifies those inputs in
metadata-free Registry copies; normal compiler checkouts additionally retain Git
revision and cleanliness checks. Updating the compiler pin requires regenerating
and reviewing this input manifest from the accepted compiler source.

C fixtures use `IDE_TEST_TMPDIR`, set from the caller's owned `TMPDIR`. Runtime
and log roots remain explicit. Generated `src/ide_files/` caches and editor-session
data are ignored source outputs and must not enter producer source inputs.

The native recipe in `make/release-native.mk` exposes separate build/audit,
signing, notarization, ticket and archive phases for the existing authentication
owner. It does not grant credentials or publication authority. Interruption and
completed-result reuse belong to that owner; do not run a second upload after an
uncertain or Accepted submission. Unsigned packages are ad-hoc local inputs, not
Developer ID authenticated releases. Neither route replaces an installed app.
