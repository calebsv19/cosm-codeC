#ifndef ANALYSIS_JOB_H
#define ANALYSIS_JOB_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool analysis_job_system_init(void);
void analysis_job_system_shutdown(void);

// Kick off async analysis (scan + library build + persistence). No-op if already running.
void start_async_workspace_analysis(const char* project_root, const char* build_args, uint64_t run_id);
void start_async_workspace_analysis_with_file_hints(const char* project_root,
                                                    const char* build_args,
                                                    uint64_t run_id,
                                                    const char* const* file_hints,
                                                    size_t file_hint_count);
void start_async_live_buffer_analysis(const char* project_root,
                                      const char* build_args,
                                      uint64_t run_id,
                                      const char* file_path,
                                      const char* contents,
                                      size_t content_length,
                                      uint64_t document_revision);
// Force the next async run to bypass incremental mode and execute a full rebuild.
void analysis_force_full_refresh_next_run(void);
// Controls whether the next run should be throttled (lazy background mode).
void analysis_job_set_slow_mode_next_run(bool enabled);
// Called from scan loops; sleeps briefly when slow mode is active.
void analysis_job_maybe_throttle(void);
void analysis_job_report_progress(int completed_files, int total_files);
void analysis_job_report_status_update(bool set_status,
                                       int status_value,
                                       bool set_has_cache,
                                       bool has_cache,
                                       bool set_last_error,
                                       const char* last_error);
// Cooperative cancel: current analysis run exits at safe checkpoints.
void analysis_job_request_cancel(void);
bool analysis_job_cancel_requested(void);
bool analysis_job_is_running(void);
const char* analysis_job_last_error(void);
void analysis_job_poll(void);

#endif // ANALYSIS_JOB_H
