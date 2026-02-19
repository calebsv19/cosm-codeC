#ifndef ANALYSIS_JOB_H
#define ANALYSIS_JOB_H

#include <stdbool.h>

// Kick off async analysis (scan + library build + persistence). No-op if already running.
void start_async_workspace_analysis(const char* project_root, const char* build_args);
bool analysis_job_is_running(void);
const char* analysis_job_last_error(void);
void analysis_job_poll(void);

#endif // ANALYSIS_JOB_H
