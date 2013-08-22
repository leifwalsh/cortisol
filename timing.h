/* -*- mode: C; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif


#include <stdint.h>

int get_processor_frequency(uint64_t *hzret);

typedef unsigned long long timestamp_t;

static inline timestamp_t now(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return (timestamp_t)hi << 32 | lo;
}

static inline double ts_to_secs(timestamp_t ts) {
    static double seconds_per_clock = -1;
    if (seconds_per_clock<0) {
	uint64_t hz;
	int r = get_processor_frequency(&hz);
	assert(r == 0);
	seconds_per_clock = 1.0/hz;
    }
    return ts * seconds_per_clock;
}

#if defined(__cplusplus)
};
#endif
