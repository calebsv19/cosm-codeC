# Makefile Summary (Concise)

## What It Builds
- `ide`: main IDE executable.
- `idebridge`: IPC/bridge helper tool.
- Shared components are compiled into the same build:
  - `../shared/vk_renderer`
  - `../shared/timer_hud`
  - `../shared/core_base`, `../shared/core_io`, `../shared/core_theme`, `../shared/core_font`
  - `../fisiCs/libfisics_frontend.a`

## Build Profiles
- `BUILD_PROFILE=debug` (default): `-g -O0` + sanitizer link flags.
- `BUILD_PROFILE=perf`: `-O3 -DNDEBUG` and, by default, links sanitizer runtime because current `libfisics_frontend.a` is sanitizer-instrumented.

Related toggle:
- `FISICS_SANITIZED=1` (default): keeps sanitizer runtime linked in `perf`.
- `FISICS_SANITIZED=0`: true no-sanitizer perf link (works only if Fisics lib is rebuilt without sanitizer instrumentation).

## Common Commands
- `make` or `make debug`: debug build.
- `make perf`: optimized performance build.
- `make clean`: remove `build/` plus top-level binaries.
- `make run-ide-theme`: launch IDE with shared theme/font defaults.

## Notable Flags
- Vulkan debug toggles:
  - `VULKAN_RENDER_DEBUG=1`
  - `VULKAN_RENDER_DEBUG_FRAMES=1`
- Uses Homebrew and fallback include/lib paths automatically.
