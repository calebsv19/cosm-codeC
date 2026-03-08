# Security Notes

This project is currently alpha software and is not hardened for hostile multi-tenant environments.

## Security Model (Current)

- Primary model: local desktop IDE for trusted users and trusted workspaces.
- The IDE can execute shell/build/git commands from workspace context.
- Plugin support is experimental; only load trusted plugins.
- IPC/bridge functionality is intended for local same-user workflows.

## Important Trust Boundaries

- Workspace content is treated as potentially executable input in several flows.
- Build and run actions may invoke shell commands.
- Git panel features invoke local `git` commands in the workspace directory.
- Analysis/build flag discovery may invoke local tooling (`make`, `clang`).

If you open untrusted repositories, assume increased risk.

## Recommended Safe Usage

- Use this IDE with projects you trust.
- Review workspace build/run settings before executing commands.
- Keep backups and commit frequently (alpha stability).
- Do not run as root/administrator.
- Keep your OS and toolchain patched.

## Reporting Security Issues

- Prefer responsible disclosure.
- If GitHub security advisories are enabled for this repository, use them.
- Otherwise open an issue with minimal repro details and avoid posting active exploit details publicly.

