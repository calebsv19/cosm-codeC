# Desktop Packaging

Last updated: 2026-05-04

This IDE can now be packaged as a macOS app bundle so it can be launched from Finder/Desktop without relying on repository cwd.

## Build Package

```sh
make package-desktop
```

Output:

- `dist/codeC.app`
- target-scoped package outputs are also staged under `build/targets/<target-triple>/...`

## Validate Package (Automated)

```sh
make package-desktop-self-test
```

This runs:

- bundle layout checks
- required font/shader presence checks
- launcher self-test (`ide-launcher --self-test`)

## Optional Convenience Targets

```sh
make package-desktop-copy-desktop
make package-desktop-sync
make package-desktop-remove
make package-desktop-refresh
make package-desktop-open
```

Default desktop destination:

- `$(HOME)/Desktop/codeC.app`

Optional icon inputs:

```sh
make package-desktop-refresh PACKAGE_APP_ICON_SRC="/absolute/path/to/codec.icns"
make package-desktop-refresh PACKAGE_APP_ICONSET_SRC="/absolute/path/to/codec.iconset"
```

Default local icon store:

- `ide/tools/packaging/macos/local_app_icon/AppIcon.icns`
- `ide/tools/packaging/macos/local_app_icon/AppIcon.iconset`

Plain `make -C ide package-desktop-refresh` and `package-desktop-self-test` now look in that local store first. The local icon store is gitignored so refreshed icon copies do not dirty the normal repo worktree.

## Finder Manual Verification

1. Build package with `make package-desktop-self-test`.
2. Refresh Desktop copy with `make package-desktop-refresh`.
3. Print runtime launch config:
   - `/Users/<user>/Desktop/codeC.app/Contents/MacOS/ide-launcher --print-config`
4. Launch by double-clicking `codeC.app` (or `open /Users/<user>/Desktop/codeC.app`).
4. Confirm:
   - app opens successfully
   - UI font renders correctly
   - no shader load failure on startup
   - terminal integration remains available
5. Check launcher logs:
   - `tail -n 120 ~/Library/Logs/IDE/launcher.log`

## Runtime Resource Model

Packaged launch uses `ide-launcher` to set:

- `IDE_RESOURCE_ROOT=<app>/Contents/Resources`
- `VK_RENDERER_SHADER_ROOT=<runtime-dir>`

Before launch, the launcher now ensures the packaged shader/runtime resources are copied into the writable runtime directory so Vulkan and non-Vulkan shader loads do not depend on direct bundle-relative mutable paths.

Launcher diagnostics:

- `--self-test` validates required bundle files and prints resolved roots
- `--print-config` prints resolved roots/log file without launching UI
- startup logs append to `~/Library/Logs/IDE/launcher.log` (fallback: `${TMPDIR}/ide-launcher.log`)
- when icon inputs are provided, packaging bundles `Contents/Resources/AppIcon.icns` and declares `CFBundleIconFile=AppIcon`

Note:
- a fresh clone will still need an `AppIcon.icns` copied into `tools/packaging/macos/local_app_icon/` before plain packaging picks it up, because that lane is intentionally ignored.

At runtime, IDE path resolution still supports explicit overrides (for debugging/development).

## Release Distribution Flow

Developer ID + notarized distribution lane:

```sh
make release-contract
make release-bundle-audit
make release-verify-signed APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"
make release-notarize APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"
make release-staple
make release-verify-notarized
make release-artifact
```

One-shot lane:

```sh
make release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"
```

Current release guardrails:

- `release-distribute` now requires the full notarized chain:
  `release-verify-signed -> release-notarize -> release-staple -> release-verify-notarized -> release-artifact`
- a signed-but-unnotarized `spctl` result such as `source=Unnotarized Developer ID`
  is valid only for pre-notary `release-verify`; it no longer satisfies
  `release-artifact` or `release-distribute`
- the release manifest now records `signed=1`, `notarized=1`, and the
  `notary_submit.json` evidence path for distribution-grade artifacts

Intel target packaging note:

- `make ... TARGET_ARCH=x86_64` now emits Intel artifact names in the form `codeC-<version>-macOS-x86_64-stable.*`
- dependency resolution for the Intel lane prefers `/usr/local` where appropriate during bundle closure
