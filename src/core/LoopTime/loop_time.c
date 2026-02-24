#include "core/LoopTime/loop_time.h"

#include "core_time.h"

#include <limits.h>

uint64_t loop_time_now_ns(void) {
    return core_time_now_ns();
}

uint32_t loop_time_now_ms32(void) {
    uint64_t ns = core_time_now_ns();
    uint64_t ms = ns / 1000000ULL;
    if (ms > (uint64_t)UINT32_MAX) return UINT32_MAX;
    return (uint32_t)ms;
}

uint64_t loop_time_diff_ns(uint64_t a, uint64_t b) {
    return core_time_diff_ns(a, b);
}

double loop_time_ns_to_seconds(uint64_t ns) {
    return core_time_ns_to_seconds(ns);
}
