#ifndef PSRC_PROFILING_H
#define PSRC_PROFILING_H

#include "attribs.h"
#include "time.h"
#include "crc.h"

#include <stdint.h>
#include <stdlib.h>

struct profile {
    int pointct;
    int curpoint;
    unsigned iter;
    uint64_t* tmptime;
    uint64_t lasttime;
    uint64_t starttime;
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    }* colors;
    uint32_t* time;
    uint16_t* percent;
};

static inline void prof_init(struct profile* p, int pointct, const char* const* names) {
    p->pointct = pointct;
    p->tmptime = calloc(pointct, sizeof(*p->tmptime));
    p->colors = malloc(pointct * sizeof(*p->colors));
    p->time = calloc((pointct + 1), sizeof(*p->time));
    ++p->time;
    p->percent = calloc((pointct + 1), sizeof(*p->percent));
    *p->percent++ = 10000;
    //srand(altutime());
    //int r = rand();
    //printf("[ 0x%08X ]\n", r);
    for (int i = 0; i < pointct; ++i) {
        uint32_t tmp = strcrc32(names[i]) ^ 0x66BC3287;
        p->colors[i].r = tmp;
        p->colors[i].g = tmp >> 12;
        p->colors[i].b = tmp >> 24;
        uint8_t max = (p->colors[i].r > p->colors[i].g) ? p->colors[i].r : p->colors[i].g;
        max = (max > p->colors[i].b) ? max : p->colors[i].b;
        if (max < 192) {
            max = 192 - max;
            p->colors[i].r += max;
            p->colors[i].g += max;
            p->colors[i].b += max;
        }
    }
    p->curpoint = -1;
}
static inline void prof_free(struct profile* p) {
    free(p->percent - 1);
    free(p->time - 1);
    free(p->colors);
    free(p->tmptime);
}

static inline void prof_start(struct profile* p) {
    ++p->iter;
}
static inline void prof_calc(struct profile* p) {
    uint64_t t = altutime();
    if (p->iter) {
        uint64_t et = t - p->starttime;
        uint64_t other = et;
        for (int i = 0; i < p->pointct; ++i) {
            p->time[i] = p->tmptime[i] / p->iter;
            p->percent[i] = p->tmptime[i] * 10000 / et;
            other -= p->tmptime[i];
            p->tmptime[i] = 0;
        }
        p->time[-1] = other / p->iter;
        p->percent[-1] = other * 10000 / et;
        p->iter = 0;
    } else {
        for (int i = 0; i < p->pointct; ++i) {
            p->time[i] = 0;
            p->percent[i] = 0;
            p->tmptime[i] = 0;
        }
        p->time[-1] = 0;
        p->percent[-1] = 0;
    }
    p->starttime = t;
}

static inline void prof_end(struct profile* p) {
    p->tmptime[p->curpoint] += altutime() - p->lasttime;
    p->curpoint = -1;
}
static inline void prof_begin(struct profile* p, int point) {
    if (p->curpoint != -1) p->tmptime[p->curpoint] += altutime() - p->lasttime;
    p->curpoint = point;
    p->lasttime = altutime();
}

#endif
