# Desktop Packaging

This IDE can now be packaged as a macOS app bundle so it can be launched from Finder/Desktop without relying on repository cwd.

## Build Package

```sh
make package-desktop
```

Output:

- `dist/IDE.app`

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
make package-desktop-open
```

## Finder Manual Verification

1. Build package with `make package-desktop`.
2. Copy `dist/IDE.app` to Desktop.
3. Launch by double-clicking `IDE.app`.
4. Confirm:
   - app opens successfully
   - UI font renders correctly
   - no shader load failure on startup
   - terminal integration remains available

## Runtime Resource Model

Packaged launch uses `ide-launcher` to set:

- `IDE_RESOURCE_ROOT=<app>/Contents/Resources`
- `VK_RENDERER_SHADER_ROOT=<app>/Contents/Resources/vk_renderer`

At runtime, IDE path resolution still supports explicit overrides (for debugging/development).
