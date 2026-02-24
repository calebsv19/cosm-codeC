#ifndef LOOP_TIME_H
#define LOOP_TIME_H

#include <stdint.h>

uint64_t loop_time_now_ns(void);
uint32_t loop_time_now_ms32(void);
uint64_t loop_time_diff_ns(uint64_t a, uint64_t b);
double loop_time_ns_to_seconds(uint64_t ns);

#endif // LOOP_TIME_H
