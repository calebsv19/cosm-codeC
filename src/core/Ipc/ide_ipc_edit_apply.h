#ifndef IDE_IPC_EDIT_APPLY_H
#define IDE_IPC_EDIT_APPLY_H

#include <stdbool.h>
#include <stddef.h>

struct json_object;

bool ide_ipc_apply_unified_diff(const char* project_root,
                                const char* diff_text,
                                struct json_object** result_out,
                                char* error_out,
                                size_t error_out_cap);

bool ide_ipc_compute_file_hash_hex(const char* path,
                                   char* out_hex,
                                   size_t out_hex_cap);

#endif
