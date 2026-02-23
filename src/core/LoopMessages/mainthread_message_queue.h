#ifndef MAINTHREAD_MESSAGE_QUEUE_H
#define MAINTHREAD_MESSAGE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum MainThreadMessageType {
    MAINTHREAD_MSG_NONE = 0,
    MAINTHREAD_MSG_ANALYSIS_FINISHED = 1,
    MAINTHREAD_MSG_PROGRESS = 2,
    MAINTHREAD_MSG_GIT_SNAPSHOT = 3
} MainThreadMessageType;

typedef struct MainThreadAnalysisFinishedPayload {
    bool cancelled;
    bool had_error;
    char project_root[1024];
} MainThreadAnalysisFinishedPayload;

typedef struct MainThreadProgressPayload {
    int job_id;
    int percent;
    char text[128];
} MainThreadProgressPayload;

typedef struct MainThreadGitSnapshotPayload {
    uint64_t hash;
    char project_root[1024];
} MainThreadGitSnapshotPayload;

typedef struct MainThreadMessage {
    MainThreadMessageType type;
    uint64_t seq;
    union {
        MainThreadAnalysisFinishedPayload analysis_finished;
        MainThreadProgressPayload progress;
        MainThreadGitSnapshotPayload git_snapshot;
    } payload;
    void* owned_ptr;
    void (*free_owned_ptr)(void*);
} MainThreadMessage;

typedef struct MainThreadMessageQueueStats {
    uint32_t depth;
    uint32_t high_watermark;
    uint64_t pushed;
    uint64_t popped;
    uint64_t dropped_progress;
    uint64_t replaced_git_snapshots;
} MainThreadMessageQueueStats;

void mainthread_message_queue_init(void);
void mainthread_message_queue_shutdown(void);
void mainthread_message_queue_reset(void);

bool mainthread_message_queue_push(const MainThreadMessage* msg);
bool mainthread_message_queue_push_progress_throttled(const MainThreadMessage* msg,
                                                      uint32_t min_interval_ms);
bool mainthread_message_queue_pop(MainThreadMessage* out);
int mainthread_message_queue_drain(MainThreadMessage* out, int max_count);
void mainthread_message_release(MainThreadMessage* msg);
void mainthread_message_queue_snapshot(MainThreadMessageQueueStats* out);

#endif // MAINTHREAD_MESSAGE_QUEUE_H

