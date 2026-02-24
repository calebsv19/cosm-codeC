#ifndef MAINTHREAD_KERNEL_H
#define MAINTHREAD_KERNEL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct MainThreadKernelStats {
    bool initialized;
    bool render_requested;
    uint64_t last_tick_work_units;
} MainThreadKernelStats;

bool mainthread_kernel_init(void);
void mainthread_kernel_shutdown(void);
void mainthread_kernel_tick(uint64_t now_ns);
void mainthread_kernel_snapshot(MainThreadKernelStats* out);

#endif // MAINTHREAD_KERNEL_H
