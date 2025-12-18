#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>
#include <abt.h>

#include "abt_workstealing_scheduler.h"
#include "abt_workstealing_scheduler_cost_aware.h"

/* ============================================================
 * Конфигурация
 * ============================================================ */

#define SCHEDULER_OLD 0
#define SCHEDULER_NEW 1

/* ============================================================
 * Структуры
 * ============================================================ */

typedef struct {
    int id;
    int complexity;
    int exec_time_ms;
    int created_on;
    int executed_on;
} task_t;

typedef struct {
    int num_xstreams;
    int tasks_per_stream;
    int complexity_mode;
} test_config_t;

typedef struct {
    double total_time_ms;
    int steals;
    int imbalance;
    double efficiency;
} benchmark_stats_t;

/* ============================================================
 * Глобальные данные
 * ============================================================ */

static task_t **g_tasks = NULL;
static int g_task_count = 0;
static ABT_mutex g_task_mutex;

/* ============================================================
 * Утилиты
 * ============================================================ */

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void execute_task(int ms) {
    usleep(ms * 1000);
}

/* ============================================================
 * Тело задачи (ULT)
 * ============================================================ */

static void task_body(void *arg) {
    task_t *task = (task_t *)arg;
    int rank;

    ABT_xstream_self_rank(&rank);

    execute_task(task->exec_time_ms);

    ABT_mutex_lock(g_task_mutex);
    task->executed_on = rank;
    ABT_mutex_unlock(g_task_mutex);
}

/* ============================================================
 * Создание задач
 * ============================================================ */

static void create_tasks(const test_config_t *cfg) {
    g_task_count = cfg->num_xstreams * cfg->tasks_per_stream;
    g_tasks = malloc(sizeof(task_t *) * g_task_count);

    srand(42);

    for (int s = 0; s < cfg->num_xstreams; s++) {
        for (int i = 0; i < cfg->tasks_per_stream; i++) {
            int id = s * cfg->tasks_per_stream + i;
            task_t *t = malloc(sizeof(task_t));

            t->id = id;
            t->created_on = s;
            t->executed_on = -1;

            switch (cfg->complexity_mode) {
                case 0:
                    t->complexity = 1;
                    t->exec_time_ms = 10 + rand() % 10;
                    break;
                case 1:
                    t->complexity = 2;
                    t->exec_time_ms = 40 + rand() % 20;
                    break;
                case 2:
                    t->complexity = 3;
                    t->exec_time_ms = 80 + rand() % 40;
                    break;
                case 3:
                    t->complexity = (s % 3) + 1;
                    t->exec_time_ms = 20 * t->complexity + rand() % 20;
                    break;
                default:
                    t->complexity = 1;
                    t->exec_time_ms = 10;
            }

            g_tasks[id] = t;
        }
    }
}

/* ============================================================
 * Benchmark
 * ============================================================ */

static benchmark_stats_t run_benchmark(int scheduler, test_config_t cfg) {
    benchmark_stats_t stats = {0};

    ABT_init(0, NULL);
    ABT_mutex_create(&g_task_mutex);

    ABT_xstream *xstreams = malloc(sizeof(ABT_xstream) * cfg.num_xstreams);
    ABT_sched   *scheds   = malloc(sizeof(ABT_sched)   * cfg.num_xstreams);
    ABT_pool    *pools    = malloc(sizeof(ABT_pool)    * cfg.num_xstreams);
    ABT_thread  *threads  = malloc(sizeof(ABT_thread)  * g_task_count);

    for (int i = 0; i < cfg.num_xstreams; i++) {
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC,
                              ABT_TRUE, &pools[i]);
    }

    if (scheduler == SCHEDULER_OLD)
        ABT_create_ws_scheds(cfg.num_xstreams, pools, scheds);
    else
        ABT_create_ws_scheds_cost_aware(cfg.num_xstreams, pools, scheds);

    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched(xstreams[0], scheds[0]);

    for (int i = 1; i < cfg.num_xstreams; i++)
        ABT_xstream_create(scheds[i], &xstreams[i]);

    double start = now_ms();

    int t = 0;
    for (int s = 0; s < cfg.num_xstreams; s++) {
        for (int i = 0; i < cfg.tasks_per_stream; i++) {
            ABT_thread_create(
                pools[s],
                task_body,
                g_tasks[t],
                ABT_THREAD_ATTR_NULL,
                &threads[t]
            );
            t++;
        }
    }

    for (int i = 0; i < g_task_count; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }

    double end = now_ms();
    stats.total_time_ms = end - start;

    int *per_stream = calloc(cfg.num_xstreams, sizeof(int));
    int steals = 0;

    for (int i = 0; i < g_task_count; i++) {
        if (g_tasks[i]->executed_on != g_tasks[i]->created_on)
            steals++;
        per_stream[g_tasks[i]->executed_on]++;
    }

    int min = INT_MAX, max = 0;
    for (int i = 0; i < cfg.num_xstreams; i++) {
        if (per_stream[i] < min) min = per_stream[i];
        if (per_stream[i] > max) max = per_stream[i];
    }

    stats.steals = steals;
    stats.imbalance = max - min;
    stats.efficiency = (double)min / max;

    for (int i = 1; i < cfg.num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();

    free(per_stream);
    free(xstreams);
    free(scheds);
    free(pools);
    free(threads);

    return stats;
}

/* ============================================================
 * main
 * ============================================================ */

int main(void) {
    test_config_t tests[] = {
        {2, 20, 0},
        {4, 40, 1},
        {4, 40, 3},
        {8, 60, 2},
    };

    int ntests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < ntests; i++) {
        printf("\n=== Тест %d ===\n", i + 1);

        create_tasks(&tests[i]);
        benchmark_stats_t old = run_benchmark(SCHEDULER_OLD, tests[i]);

        create_tasks(&tests[i]);
        benchmark_stats_t nw  = run_benchmark(SCHEDULER_NEW, tests[i]);

        printf("OLD: %.2f ms, steals=%d, imbalance=%d, eff=%.3f\n",
               old.total_time_ms, old.steals, old.imbalance, old.efficiency);

        printf("NEW: %.2f ms, steals=%d, imbalance=%d, eff=%.3f\n",
               nw.total_time_ms, nw.steals, nw.imbalance, nw.efficiency);

        double imp = (old.total_time_ms - nw.total_time_ms)
                     / old.total_time_ms * 100.0;

        printf("Improvement: %.2f %%\n", imp);

        for (int t = 0; t < g_task_count; t++)
            free(g_tasks[t]);
        free(g_tasks);
    }

    return 0;
}
