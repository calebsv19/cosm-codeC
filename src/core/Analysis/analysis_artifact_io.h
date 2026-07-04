#ifndef ANALYSIS_ARTIFACT_IO_H
#define ANALYSIS_ARTIFACT_IO_H

#include <stdbool.h>
#include <stddef.h>

#define ANALYSIS_ARTIFACT_IO_DEFAULT_MAX_BYTES (32u * 1024u * 1024u)

bool analysis_artifact_io_ide_dir(const char* workspace_root,
                                  char* out_path,
                                  size_t out_path_size);
bool analysis_artifact_io_path(const char* workspace_root,
                               const char* artifact_name,
                               char* out_path,
                               size_t out_path_size);
bool analysis_artifact_io_ensure_dir(const char* workspace_root);
char* analysis_artifact_io_read_text(const char* workspace_root,
                                     const char* artifact_name,
                                     size_t max_bytes,
                                     long* out_len);
bool analysis_artifact_io_write_text(const char* workspace_root,
                                     const char* artifact_name,
                                     const char* text);
bool analysis_artifact_io_write_text_atomic(const char* workspace_root,
                                            const char* artifact_name,
                                            const char* text);

#endif // ANALYSIS_ARTIFACT_IO_H
