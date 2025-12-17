#include "abt_workstealing_scheduler_cost_aware.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t event_freq;
    int rank;
    double local_total_time;
} ws_sched_data_t;

typedef struct {
    double total_time;
} ws_global_load_t;

static ws_global_load_t *g_loads = NULL;
static ABT_mutex g_loads_mutex;
static int g_num_xstreams = 0;

/* ===================== INTERNAL HELPERS ===================== */

static int ws_find_heaviest_pool(int self, int num)
{
    int i, victim = -1;
    double max_load = 0.0;

    ABT_mutex_lock(g_loads_mutex);
    for (i = 0; i < num; i++) {
        if (i == self) continue;
        if (g_loads[i].total_time > max_load) {
            max_load = g_loads[i].total_time;
            victim = i;
        }
    }
    ABT_mutex_unlock(g_loads_mutex);
    return victim;
}

void ws_update_task_time(double elapsed, ws_sched_data_t *data)
{
    data->local_total_time += elapsed;

    ABT_mutex_lock(g_loads_mutex);
    g_loads[data->rank].total_time = data->local_total_time;
    ABT_mutex_unlock(g_loads_mutex);
}

/* ===================== SCHEDULER CALLBACKS ===================== */

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    ws_sched_data_t *p_data = (ws_sched_data_t *)calloc(1, sizeof(ws_sched_data_t));
    p_data->local_total_time = 0.0;
    ABT_sched_config_read(config, 1, &p_data->event_freq);

    ABT_xstream x;
    ABT_xstream_self(&x);
    ABT_xstream_get_rank(x, &p_data->rank);

    ABT_sched_set_data(sched, (void *)p_data);
    return ABT_SUCCESS;
}

static void sched_run(ABT_sched sched)
{
    uint32_t work_count = 0;
    ws_sched_data_t *p_data;
    int num_pools;
    ABT_pool *pools;
    ABT_bool stop;

    ABT_sched_get_data(sched, (void **)&p_data);
    ABT_sched_get_num_pools(sched, &num_pools);
    pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
    ABT_sched_get_pools(sched, num_pools, 0, pools);

    while (1) {
        ABT_thread thread;
        ABT_pool_pop_thread(pools[0], &thread);

        if (thread == ABT_THREAD_NULL) {
            int victim = ws_find_heaviest_pool(p_data->rank, num_pools);
            if (victim >= 0) {
                ABT_pool_pop_thread(pools[victim], &thread);
                if (thread != ABT_THREAD_NULL) {
                    ABT_self_schedule(thread, pools[victim]);
                }
            }
        } else {
            ABT_self_schedule(thread, ABT_POOL_NULL);
        }

        if (++work_count >= p_data->event_freq) {
            work_count = 0;
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE) break;
            ABT_xstream_check_events(sched);
        }
    }

    free(pools);
}

static int sched_free(ABT_sched sched)
{
    ws_sched_data_t *p_data;
    ABT_sched_get_data(sched, (void **)&p_data);
    free(p_data);
    return ABT_SUCCESS;
}

/* ===================== PUBLIC API ===================== */

void ABT_create_ws_scheds_cost_aware(int num, ABT_pool *pools, ABT_sched *scheds)
{
    int i, k;
    ABT_sched_config config;
    ABT_pool *sched_pools;

    ABT_sched_config_var cv_event_freq = { .idx = 0, .type = ABT_SCHED_CONFIG_INT };

    ABT_sched_def sched_def = {
        .type = ABT_SCHED_TYPE_ULT,
        .init = sched_init,
        .run  = sched_run,
        .free = sched_free,
        .get_migr_pool = NULL
    };

    g_num_xstreams = num;
    g_loads = (ws_global_load_t *)calloc(num, sizeof(ws_global_load_t));
    ABT_mutex_create(&g_loads_mutex);

    ABT_sched_config_create(&config, cv_event_freq, 10, ABT_sched_config_var_end);

    sched_pools = (ABT_pool *)malloc(num * sizeof(ABT_pool));
    for (i = 0; i < num; i++) {
        for (k = 0; k < num; k++) {
            sched_pools[k] = pools[(i + k) % num];
        }
        ABT_sched_create(&sched_def, num, sched_pools, config, &scheds[i]);
    }
    free(sched_pools);
    ABT_sched_config_free(&config);
}
