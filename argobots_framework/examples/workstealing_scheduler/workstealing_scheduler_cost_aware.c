#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "abt_workstealing_scheduler_cost_aware.h"
#include <abt.h>

#define NUM_XSTREAMS 4
#define NUM_THREADS 8

/* Прототипы внешних функций (реализованы в abt_workstealing_scheduler_cost_aware.c) */
extern void ws_push_task_estimate(int rank, double est);
extern void ws_update_task_time(double elapsed, int rank);
extern void ws_print_global_stats(void);

// Обертка для задачи с измерением времени
typedef struct {
    void (*task_func)(void*);
    void *arg;
    int expected_stream;  // На каком потоке ожидалось выполнение
} timed_task_t;

static void timed_task_wrapper(void *arg);
static void create_timed_threads(void *arg);
static void complex_task(void *arg);

int main(int argc, char *argv[]) {
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_sched scheds[NUM_XSTREAMS];
    ABT_pool pools[NUM_XSTREAMS];
    ABT_thread threads[NUM_XSTREAMS];
    int i;

    ABT_init(argc, argv);

    // Создаем пулы
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pools[i]);
    }

    // Создаем cost-aware планировщики
    ABT_create_ws_scheds_cost_aware(NUM_XSTREAMS, pools, scheds);

    // Создаем исполнительные потоки
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched(xstreams[0], scheds[0]);
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_create(scheds[i], &xstreams[i]);
    }

    // Создаем задачи
    for (i = 0; i < NUM_XSTREAMS; i++) {
        size_t stream_id = (size_t)i;
        ABT_thread_create(pools[i], create_timed_threads, (void *)stream_id, 
                         ABT_THREAD_ATTR_NULL, &threads[i]);
    }

    // Ждем завершения
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }
    
    // Выводим статистику
    ws_print_global_stats();

    // Очистка
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_sched_free(&scheds[i]);
    }

    ABT_finalize();
    return 0;
}

// Обертка для задачи с измерением времени
static void timed_task_wrapper(void *arg) {
    timed_task_t *task = (timed_task_t *)arg;
    ABT_xstream x;
    int actual_stream = -1;
    
    /* Получаем текущий поток выполнения */
    ABT_xstream_self(&x);
    ABT_xstream_get_rank(x, &actual_stream);
    
    /* Измеряем время начала */
    double start_time = ABT_get_wtime();
    
    /* Выполняем реальную задачу */
    task->task_func(task->arg);
    
    /* Измеряем время окончания */
    double end_time = ABT_get_wtime();
    double elapsed = end_time - start_time;
    
    /* Обновляем глобальную историческую статистику */
    ws_update_task_time(elapsed, actual_stream);
    
    /* Выводим информацию о задаче */
    printf("Задача выполнена на потоке %d (ожидался %d), время: %.6f сек\n",
           actual_stream, task->expected_stream, elapsed);
    
    /* Освобождаем память */
    free(task);
}

// Создание задач с измерением времени
static void create_timed_threads(void *arg) {
    int stream_id = (int)(size_t)arg;
    ABT_xstream xstream;
    ABT_pool pool;
    ABT_thread threads[NUM_THREADS];
    
    ABT_xstream_self(&xstream);
    ABT_xstream_get_main_pools(xstream, 1, &pool);
    
    /* Создаем задачи разной сложности */
    for (int i = 0; i < NUM_THREADS; i++) {
        timed_task_t *task = malloc(sizeof(timed_task_t));
        
        size_t iterations;
        /* Задачи разной сложности в зависимости от номера */
        if (i % 3 == 0) {
            task->task_func = complex_task;
            iterations = (size_t)(1000000);  /* Лёгкая */
        } else if (i % 3 == 1) {
            task->task_func = complex_task;
            iterations = (size_t)(5000000);  /* Средняя */
        } else {
            task->task_func = complex_task;
            iterations = (size_t)(10000000); /* Тяжёлая */
        }
        
        task->arg = (void *)iterations;
        task->expected_stream = stream_id;

        /* Оценка стоимости задачи — используем число итераций как относительную меру */
        double est = (double)iterations; /* можно масштабировать при необходимости */
        ws_push_task_estimate(stream_id, est);

        ABT_thread_create(pool, timed_task_wrapper, task, 
                         ABT_THREAD_ATTR_NULL, &threads[i]);
    }
    
    /* Ждем завершения всех задач */
    for (int i = 0; i < NUM_THREADS; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }
}

// Пример "сложной" задачи (имитация вычислений)
static void complex_task(void *arg) {
    size_t iterations = (size_t)arg;
    volatile double result = 1.0;
    
    for (size_t i = 0; i < iterations; i++) {
        result = result * 1.000001;
        if (result > 1000000.0) result = 1.0;
    }
}
