#ifndef INCLUDE_PATH_RESOLVER_H
#define INCLUDE_PATH_RESOLVER_H

#include <stddef.h>

typedef struct {
    char** include_paths;
    size_t include_count;
    char** macro_defines;
    size_t macro_count;
} BuildFlagSet;

// Build a set of include paths and macro definitions for the current project.
// - project_root: workspace root for expanding relative paths.
// - extra_flags: optional build args/command strings to scrape for -I/-isystem/-D.
// Returns count of include paths; fills out the BuildFlagSet. Caller must free via free_build_flag_set.
size_t gather_build_flags(const char* project_root,
                          const char* extra_flags,
                          BuildFlagSet* out);

void free_build_flag_set(BuildFlagSet* set);

// Persistence helpers (ide_files/build_flags.json). Safe to call when files are missing.
void save_build_flags(const BuildFlagSet* set, const char* workspace_root);
void load_build_flags(BuildFlagSet* set, const char* workspace_root);

#endif // INCLUDE_PATH_RESOLVER_H
