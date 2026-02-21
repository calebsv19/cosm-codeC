# protocol_compatibility_policy

## protocol baseline
- Transport: Unix domain socket.
- Envelope: JSON object with `id`, `proto`, `cmd`, and `args`.
- Current protocol version: `proto=1`.

## compatibility guarantees
- `proto=1` requests must remain supported until an explicit major migration plan is published.
- Existing command names and required argument fields remain stable.
- Existing response fields are not removed or renamed inside `proto=1`.
- Error envelope shape remains stable: `ok=false`, `error.code`, `error.message`, optional `error.details`.

## allowed changes in proto=1
- Add new optional request args.
- Add new optional fields to success `result` payloads.
- Add new commands without changing existing command behavior.
- Expand error codes with new specific values while preserving existing semantics.

## disallowed changes in proto=1
- Changing type of an existing field.
- Removing required request fields.
- Reinterpreting existing command meaning.
- Returning non-JSON output on stdout for `--json` mode.

## protocol evolution policy
- Breaking changes require `proto=2` introduction.
- During transition, server should reject unsupported proto with `bad_proto` and clear message.
- CLI keeps default proto pinned to latest stable supported by server branch.

## validation policy
- Every new command must include:
  - schema notes in command contracts doc
  - one success-path runtime test
  - one deterministic failure-path runtime test
