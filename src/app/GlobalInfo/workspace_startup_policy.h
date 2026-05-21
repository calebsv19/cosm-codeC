#ifndef IDE_WORKSPACE_STARTUP_POLICY_H
#define IDE_WORKSPACE_STARTUP_POLICY_H

#include <stdbool.h>
#include <stddef.h>

bool ide_workspace_startup_build_default_root(const char* override_path,
                                              const char* home_dir,
                                              const char* cwd_path,
                                              char* out_path,
                                              size_t out_cap);

bool ide_workspace_startup_select_root(const char* stored_path,
                                       bool stored_path_valid,
                                       const char* default_path,
                                       bool default_path_valid,
                                       char* out_path,
                                       size_t out_cap,
                                       bool* out_used_fallback);

#endif
