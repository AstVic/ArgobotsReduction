#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "abt_workstealing_scheduler_cost_aware.h"
#include <abt.h>

#define NUM_XSTREAMS 4
#define NUM_THREADS 4

static void create_threads(void *arg);
static void thread_hello(void *arg);

int main(int argc, char *argv[])
{
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_sched scheds[NUM_XSTREAMS];
    ABT_pool pools[NUM_XSTREAMS];
    ABT_thread threads[NUM_XSTREAMS];
    int i;

    ABT_init(argc, argv);

    for (i = 0; i < NUM_XSTREAMS; i++)
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pools[i]);

    ABT_create_ws_scheds_cost_aware(NUM_XSTREAMS, pools, scheds);

    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched(xstreams[0], scheds[0]);
    for (i = 1; i < NUM_XSTREAMS; i++)
        ABT_xstream_create(scheds[i], &xstreams[i]);

    for (i = 0; i < NUM_XSTREAMS; i++) {
        size_t tid = (size_t)i;
        ABT_thread_create(pools[i], create_threads, (void *)tid, ABT_THREAD_ATTR_NULL, &threads[i]);
    }

    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    for (i = 1; i < NUM_XSTREAMS; i++)
        ABT_sched_free(&scheds[i]);

    ABT_finalize();
    return 0;
}

static void create_threads(void *arg)
{
    int i, tid = (int)(size_t)arg;
    ABT_xstream xstream;
    ABT_pool pool;
    ABT_thread *threads;

    ABT_xstream_self(&xstream);
    ABT_xstream_get_main_pools(xstream, 1, &pool);

    threads = (ABT_thread *)malloc(sizeof(ABT_thread) * NUM_THREADS);
    for (i = 0; i < NUM_THREADS; i++) {
        size_t id = tid * 10 + i;
        ABT_thread_create(pool, thread_hello, (void *)id, ABT_THREAD_ATTR_NULL, &threads[i]);
    }

    for (i = 0; i < NUM_THREADS; i++)
        ABT_thread_free(&threads[i]);
    free(threads);
}

static void thread_hello(void *arg)
{
    int tid = (int)(size_t)arg;
    int old_rank, cur_rank;
    char *msg;

    ABT_xstream_self_rank(&cur_rank);
    printf("[U%d:E%d] Hello, world!\n", tid, cur_rank);
    ABT_thread_yield();

    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    printf("[U%d:E%d][oldE=%d] Hello again.%s\n", tid, cur_rank, old_rank, msg);

    ABT_thread_yield();
    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    printf("[U%d:E%d][oldE=%d] Goodbye, world!%s\n", tid, cur_rank, old_rank, msg);
}
