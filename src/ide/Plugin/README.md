# Plug-in Interface (Prototype)

This directory explores a minimal plug-in API so external modules can hook
into the IDE without being compiled in. The implementation is intentionally
small while we validate use cases.

| File | Responsibility |
| --- | --- |
| `plugin_interface.h/c` | Defines the `Plugin` struct plus lifecycle helpers (`initPluginSystem`, `loadPlugin`, `unloadAllPlugins`, and lookup helpers). Platform-specific dynamic loading will be added here when the plug-in story is finalised. |

The plug-in system is off by default; it compiles so experiments can iterate
without breaking the main build.
