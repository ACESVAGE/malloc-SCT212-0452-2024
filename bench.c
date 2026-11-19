/*
 * bench.c - Task 8 experiment.
 *
 * The same random workload (fixed seed) is run three times: my allocator with
 * first-fit, my allocator with best-fit, and the C library's malloc/free.
 * Each run happens in its own forked process so that one run's heap does not
 * leak into the next.
 */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "myalloc.h"

#define OPS     50000
#define SLOTS   1000       /* how many blocks can be live at once */
#define MAX_SZ  4096
#define SEED    12345

typedef enum { MODE_FIRST, MODE_BEST, MODE_LIBC } mode_t_;

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void run(mode_t_ mode)
{
    static void *slot[SLOTS];
    int use_mine = (mode != MODE_LIBC);

    if (mode == MODE_BEST)
        set_policy(FIT_BEST);
    else if (mode == MODE_FIRST)
        set_policy(FIT_FIRST);

    srand(SEED);
    double t0 = now();

    for (int i = 0; i < OPS; i++) {
        int s = rand() % SLOTS;
        size_t size = 1 + (size_t)(rand() % MAX_SZ);

        if (slot[s]) {
            if (use_mine) my_free(slot[s]); else free(slot[s]);
            slot[s] = NULL;
        } else {
            slot[s] = use_mine ? my_malloc(size) : malloc(size);
        }
    }

    double t1 = now();

    if (use_mine) {
        heap_stats_t st;
        heap_stats(&st);
        double frag = st.free_bytes ? 1.0 - (double)st.largest_free / st.free_bytes : 0.0;
        printf("%-10s heap=%-9zu free=%-9zu free_blocks=%-5zu largest_free=%-8zu ext_frag=%.4f time=%.4fs  consistent=%d\n",
               mode == MODE_BEST ? "best-fit" : "first-fit",
               st.heap_bytes, st.free_bytes, st.free_blocks, st.largest_free,
               frag, t1 - t0, heap_check());
    } else {
        printf("%-10s (no stats available)                                                    time=%.4fs\n",
               "libc", t1 - t0);
    }
}

int main(void)
{
    mode_t_ modes[] = { MODE_FIRST, MODE_BEST, MODE_LIBC };

    for (int i = 0; i < 3; i++) {
        fflush(stdout);
        pid_t pid = fork();
        if (pid == 0) {
            run(modes[i]);
            fflush(stdout);
            _exit(0);
        }
        waitpid(pid, NULL, 0);
    }
    return 0;
}
