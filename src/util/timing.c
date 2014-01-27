/* -*- mode: C; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util/timing.h"

static int get_processor_frequency_sys(uint64_t *hzret) {
    int r;
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (!fp)  {
        r = errno;
    } else {
        unsigned int khz = 0;
        if (fscanf(fp, "%u", &khz) == 1) {
            *hzret = khz * 1000ULL;
            r = 0;
        } else {
            r = ENOENT;
        }
        fclose(fp);
    }
    return r;
}


static int get_processor_frequency_cpuinfo(uint64_t *hzret) {
    int r;
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        r = errno;
    } else {
        uint64_t maxhz = 0;
        char *buf = NULL;
        size_t n = 0;
        while (getline(&buf, &n, fp) >= 0) {
            unsigned int cpu;
            sscanf(buf, "processor : %u", &cpu);
            unsigned int ma, mb;
            if (sscanf(buf, "cpu MHz : %u.%u", &ma, &mb) == 2) {
                uint64_t hz = ma * 1000000ULL + mb * 1000ULL;
                if (hz > maxhz) {
                    maxhz = hz;
                }
            }
        }
        if (buf) {
            free(buf);
        }
        fclose(fp);
        *hzret = maxhz;
        r = maxhz == 0 ? ENOENT : 0;;
    }
    return r;
}


static int get_processor_frequency_sysctl(const char * const cmd, uint64_t *hzret) {
    int r = 0;
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        r = EINVAL;  // popen doesn't return anything useful in errno,
                     // gotta pick something
        goto exit;
    }
    r = fscanf(fp, "%" SCNu64, hzret);
    if (r != 1) {
        r = errno;
    } else {
        r = 0;
    }
    pclose(fp);

exit:
    return r;
}


int get_processor_frequency(uint64_t *hzret) {
    int r;
    r = get_processor_frequency_sys(hzret);
    if (r != 0)
        r = get_processor_frequency_cpuinfo(hzret);
    if (r != 0)
        r = get_processor_frequency_sysctl("sysctl -n hw.cpufrequency", hzret);
    if (r != 0)
        r = get_processor_frequency_sysctl("sysctl -n machdep.tsc_freq", hzret);
    return r;
}
