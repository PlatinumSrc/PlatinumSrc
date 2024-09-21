#include "timer.h"

#include "time.h"

#include <stdlib.h>

static struct {
    struct timer* data;
    int len;
    int size;
    int index;
    int next;
} tdata = {
    .next = -1
};

void timer_init(void) {
    tdata.len = 0;
    tdata.size = 4;
    tdata.data = malloc(tdata.size * sizeof(*tdata.data));
}

int timer_new(uint64_t a, void* d) {
    int i = 0;
    for (; i < tdata.len; ++i) {
        if (!tdata.data[i].a) goto found;
    }
    if (i == tdata.size) {
        tdata.size *= 2;
        tdata.data = realloc(tdata.data, tdata.size * sizeof(*tdata.data));
    }
    ++tdata.len;
    found:;
    tdata.data[i].t = altutime() + a;
    tdata.data[i].a = a;
    tdata.data[i].d = d;
    return i;
}
void timer_delete(int i) {
    tdata.data[i].a = 0;
}

int timer_getnext(uint64_t* u, void** d) {
    if (!tdata.len) return -1;
    uint64_t m;
    int cur = tdata.next;
    int next = -1;
    if (cur == -1) {
        cur = 0;
        m = tdata.data[0].t;
        for (int i = 1; i < tdata.len; ++i) {
            if (tdata.data[i].t < m) {
                cur = i;
                m = tdata.data[i].t;
            }
        }
    }
    *u = tdata.data[cur].t;
    *d = tdata.data[cur].d;
    tdata.data[cur].t += tdata.data[cur].a;
    for (int i = 0; i < tdata.len; ++i) {
        if (next == -1 || tdata.data[i].t < m) {
            next = i;
            m = tdata.data[i].t;
        }
    }
    tdata.next = next;
    return cur;
}
