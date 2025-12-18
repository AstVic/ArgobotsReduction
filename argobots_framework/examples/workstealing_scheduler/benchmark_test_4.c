#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <abt.h>

#define NUM_XSTREAMS 4
#define NUM_TASKS_PER_STREAM 40
#define COMPLEXITY_MODE 4

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>

// Флаги для выбора планировщика
#define SCHEDULER_OLD 0
#define SCHEDULER_NEW 1

// Заголовочные файлы
#include "abt_workstealing_scheduler.h"
#include "abt_workstealing_scheduler_cost_aware.h"

// Структура для задачи
typedef struct {
    int id;
    int complexity;  // Сложность задачи (1-легкая, 2-средняя, 3-тяжелая)
    int execution_time_ms;  // Время выполнения в миллисекундах
    int created_on_stream;  // На каком потоке создана
    int executed_on_stream; // На каком потоке выполнена
    int stolen;  // Была ли украдена
} benchmark_task_t;

// Глобальные параметры теста
typedef struct {
    int num_xstreams;      // Количество исполнительных потоков
    int tasks_per_stream;  // Количество задач на поток
    int complexity_mode;   // Режим сложности задач
    int steal_attempts;    // Попытки кражи
} test_config_t;

// Глобальная статистика выполнения
typedef struct {
    double total_time_ms;
    int tasks_completed;
    int steals_occurred;
    int work_imbalance;    // Максимальная разница в количестве задач между потоками
    double efficiency;     // Эффективность использования потоков (0-1)
} benchmark_stats_t;

// Переменные для хранения задач между запусками
static benchmark_task_t **g_tasks = NULL;
static int g_num_tasks = 0;

// ===================== УТИЛИТЫ =====================

double get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec * 1000 + tv.tv_usec / 1000.0);
}

// Функция для создания набора задач с заданной сложностью
benchmark_task_t** create_tasks(int num_xstreams, int tasks_per_stream, int complexity_mode) {
    int total_tasks = num_xstreams * tasks_per_stream;
    benchmark_task_t **tasks = malloc(total_tasks * sizeof(benchmark_task_t*));
    
    srand(12345); // Фиксированный seed для воспроизводимости
    
    for (int stream = 0; stream < num_xstreams; stream++) {
        for (int i = 0; i < tasks_per_stream; i++) {
            int idx = stream * tasks_per_stream + i;
            tasks[idx] = malloc(sizeof(benchmark_task_t));
            tasks[idx]->id = idx;
            tasks[idx]->created_on_stream = stream;
            tasks[idx]->executed_on_stream = -1;
            tasks[idx]->stolen = 0;
            
            // Определяем сложность в зависимости от режима
            switch (complexity_mode) {
                case 0: // Все легкие
                    tasks[idx]->complexity = 1;
                    tasks[idx]->execution_time_ms = 10 + rand() % 20;
                    break;
                case 1: // Все средние
                    tasks[idx]->complexity = 2;
                    tasks[idx]->execution_time_ms = 50 + rand() % 50;
                    break;
                case 2: // Все тяжелые
                    tasks[idx]->complexity = 3;
                    tasks[idx]->execution_time_ms = 100 + rand() % 100;
                    break;
                case 3: // Смешанные (разные очереди)
                    if (stream % 3 == 0) {
                        tasks[idx]->complexity = 1;
                        tasks[idx]->execution_time_ms = 10 + rand() % 20;
                    } else if (stream % 3 == 1) {
                        tasks[idx]->complexity = 2;
                        tasks[idx]->execution_time_ms = 50 + rand() % 50;
                    } else {
                        tasks[idx]->complexity = 3;
                        tasks[idx]->execution_time_ms = 100 + rand() % 100;
                    }
                    break;
                case 4: // Сильно неравномерные
                    if (stream == 0) {
                        // Первый поток получает много тяжелых задач
                        tasks[idx]->complexity = 3;
                        tasks[idx]->execution_time_ms = 150 + rand() % 100;
                    } else {
                        tasks[idx]->complexity = 1;
                        tasks[idx]->execution_time_ms = 5 + rand() % 15;
                    }
                    break;
            }
        }
    }
    
    g_tasks = tasks;
    g_num_tasks = total_tasks;
    
    return tasks;
}

// Функция для выполнения задачи
void execute_task(benchmark_task_t *task) {
    // Имитация работы задачи
    usleep(task->execution_time_ms * 1000); // microsleep
}

// ===================== КОД ДЛЯ ТЕСТОВЫХ ЗАДАЧ =====================

#define MAX_TASKS_PER_STREAM 1000

// Структура для передачи данных в тестовую функцию
typedef struct {
    int stream_id;
    benchmark_task_t **tasks;
    int num_tasks;
    int *steal_counter;
} worker_data_t;

// Функция, которая будет выполняться как задача
static void benchmark_task_function(void *arg) {
    worker_data_t *data = (worker_data_t *)arg;
    
    // Выполняем все задачи, назначенные этому потоку
    for (int i = 0; i < data->num_tasks; i++) {
        if (data->tasks[i] && data->tasks[i]->created_on_stream == data->stream_id) {
            execute_task(data->tasks[i]);
            data->tasks[i]->executed_on_stream = data->stream_id;
            
            // Проверяем, была ли задача украдена
            if (data->tasks[i]->executed_on_stream != data->tasks[i]->created_on_stream) {
                data->tasks[i]->stolen = 1;
                (*data->steal_counter)++;
            }
        }
    }
    
    free(data);
}

// Функция для создания потоков с задачами
static void create_benchmark_tasks(void *arg) {
    int stream_id = (int)(size_t)arg;
    ABT_xstream xstream;
    ABT_pool pool;
    ABT_thread thread;
    
    ABT_xstream_self(&xstream);
    ABT_xstream_get_main_pools(xstream, 1, &pool);
    
    // Создаем данные для рабочей функции
    worker_data_t *data = malloc(sizeof(worker_data_t));
    data->stream_id = stream_id;
    data->tasks = g_tasks;
    data->num_tasks = g_num_tasks;
    data->steal_counter = malloc(sizeof(int));
    *data->steal_counter = 0;
    
    // Создаем одну задачу, которая выполнит все подзадачи
    ABT_thread_create(pool, benchmark_task_function, data, ABT_THREAD_ATTR_NULL, &thread);
    
    // Ждем завершения
    ABT_thread_join(thread);
    ABT_thread_free(&thread);
    
    // Сохраняем счетчик краж
    // (здесь нужно глобально сохранить данные, но для простоты используем static)
    static int total_steals = 0;
    total_steals += *data->steal_counter;
    free(data->steal_counter);
}

// ===================== ОСНОВНОЙ БЕНЧМАРК =====================

benchmark_stats_t run_benchmark(int scheduler_type, test_config_t config) {
    benchmark_stats_t stats = {0};
    double start_time, end_time;
    
    // Сохраняем текущие задачи
    benchmark_task_t **tasks = create_tasks(config.num_xstreams, config.tasks_per_stream, config.complexity_mode);
    
    // Инициализируем Argobots
    ABT_init(0, NULL);
    
    ABT_xstream xstreams[config.num_xstreams];
    ABT_sched scheds[config.num_xstreams];
    ABT_pool pools[config.num_xstreams];
    ABT_thread threads[config.num_xstreams];
    
    // Создаем пулы
    for (int i = 0; i < config.num_xstreams; i++) {
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pools[i]);
    }
    
    // Создаем планировщики в зависимости от типа
    if (scheduler_type == SCHEDULER_OLD) {
        ABT_create_ws_scheds(config.num_xstreams, pools, scheds);
    } else {
        ABT_create_ws_scheds_cost_aware(config.num_xstreams, pools, scheds);
    }
    
    // Создаем исполнительные потоки
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched(xstreams[0], scheds[0]);
    for (int i = 1; i < config.num_xstreams; i++) {
        ABT_xstream_create(scheds[i], &xstreams[i]);
    }
    
    // Замеряем время
    start_time = get_current_time_ms();
    
    // Создаем тестовые потоки
    for (int i = 0; i < config.num_xstreams; i++) {
        size_t stream_id = (size_t)i;
        ABT_thread_create(pools[i], create_benchmark_tasks, (void *)stream_id, 
                         ABT_THREAD_ATTR_NULL, &threads[i]);
    }
    
    // Ждем завершения
    for (int i = 0; i < config.num_xstreams; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }
    
    end_time = get_current_time_ms();
    
    // Очистка
    for (int i = 1; i < config.num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }
    
    for (int i = 1; i < config.num_xstreams; i++) {
        ABT_sched_free(&scheds[i]);
    }
    
    ABT_finalize();
    
    // Собираем статистику
    stats.total_time_ms = end_time - start_time;
    stats.tasks_completed = config.num_xstreams * config.tasks_per_stream;
    
    // Подсчитываем кражи (нужно реализовать подсчет в функциях)
    int steals = 0;
    for (int i = 0; i < g_num_tasks; i++) {
        if (tasks[i]->stolen) {
            steals++;
        }
    }
    stats.steals_occurred = steals;
    
    // Вычисляем дисбаланс
    int tasks_per_stream[config.num_xstreams];
    memset(tasks_per_stream, 0, sizeof(int) * config.num_xstreams);
    
    for (int i = 0; i < g_num_tasks; i++) {
        if (tasks[i]->executed_on_stream >= 0) {
            tasks_per_stream[tasks[i]->executed_on_stream]++;
        }
    }
    
    int max_tasks = 0, min_tasks = INT_MAX;
    for (int i = 0; i < config.num_xstreams; i++) {
        if (tasks_per_stream[i] > max_tasks) max_tasks = tasks_per_stream[i];
        if (tasks_per_stream[i] < min_tasks) min_tasks = tasks_per_stream[i];
    }
    
    stats.work_imbalance = max_tasks - min_tasks;
    stats.efficiency = (double)min_tasks / max_tasks;
    
    return stats;
}

// ===================== ГЛАВНАЯ ФУНКЦИЯ =====================

int main(int argc, char *argv[]) {
    printf("=== СРАВНЕНИЕ ПЛАНИРОВЩИКОВ ЗАДАЧ ===\n\n");
    
    // Конфигурации тестов
    test_config_t test_configs[] = {
        // num_xstreams, tasks_per_stream, complexity_mode, steal_attempts
        {2, 10, 0, 5},    // Мало задач, легкие
        {4, 25, 1, 10},   // Среднее количество, средние задачи
        {8, 50, 2, 20},   // Много задач, тяжелые
        {4, 30, 3, 15},   // Смешанные очереди
        {4, 40, 4, 20},   // Сильно неравномерные
    };
    
    int num_tests = sizeof(test_configs) / sizeof(test_configs[0]);
    
    // Создаем CSV файл для результатов
    FILE *csv = fopen("scheduler_comparison_results.csv", "w");
    if (!csv) {
        perror("Ошибка создания файла результатов");
        return 1;
    }
    
    // Заголовок CSV
    fprintf(csv, "TestID,Streams,TasksPerStream,ComplexityMode,");
    fprintf(csv, "OldTimeMs,NewTimeMs,OldSteals,NewSteals,");
    fprintf(csv, "OldImbalance,NewImbalance,OldEfficiency,NewEfficiency,Improvement%%\n");
    
    printf("Запуск тестов (сначала старый планировщик, затем новый)...\n\n");
    
    for (int test_id = 0; test_id < num_tests; test_id++) {
        test_config_t config = test_configs[test_id];
        
        printf("Тест %d: %d потоков, %d задач/поток, сложность: %d\n",
               test_id + 1, config.num_xstreams, config.tasks_per_stream, config.complexity_mode);
        
        // 1. Запускаем старый планировщик
        printf("  Запуск старого планировщика...\n");
        benchmark_stats_t old_stats = run_benchmark(SCHEDULER_OLD, config);
        
        // Даем системе отдохнуть между тестами
        sleep(1);
        
        // 2. Запускаем новый планировщик
        printf("  Запуск нового планировщика...\n");
        benchmark_stats_t new_stats = run_benchmark(SCHEDULER_NEW, config);
        
        // Вычисляем улучшение
        double improvement = 0;
        if (old_stats.total_time_ms > 0) {
            improvement = ((old_stats.total_time_ms - new_stats.total_time_ms) / 
                          old_stats.total_time_ms) * 100;
        }
        
        // Выводим результаты
        printf("  Результаты:\n");
        printf("    Старый: %.2f ms, %d краж, дисбаланс: %d, эффективность: %.3f\n",
               old_stats.total_time_ms, old_stats.steals_occurred, 
               old_stats.work_imbalance, old_stats.efficiency);
        printf("    Новый:  %.2f ms, %d краж, дисбаланс: %d, эффективность: %.3f\n",
               new_stats.total_time_ms, new_stats.steals_occurred,
               new_stats.work_imbalance, new_stats.efficiency);
        printf("    Улучшение: %.2f%%\n\n", improvement);
        
        // Записываем в CSV
        fprintf(csv, "%d,%d,%d,%d,%.2f,%.2f,%d,%d,%d,%d,%.3f,%.3f,%.2f\n",
                test_id + 1, config.num_xstreams, config.tasks_per_stream, config.complexity_mode,
                old_stats.total_time_ms, new_stats.total_time_ms,
                old_stats.steals_occurred, new_stats.steals_occurred,
                old_stats.work_imbalance, new_stats.work_imbalance,
                old_stats.efficiency, new_stats.efficiency,
                improvement);
        
        // Очищаем задачи для следующего теста
        if (g_tasks) {
            for (int i = 0; i < g_num_tasks; i++) {
                free(g_tasks[i]);
            }
            free(g_tasks);
            g_tasks = NULL;
            g_num_tasks = 0;
        }
        
        sleep(2); // Пауза между тестами
    }
    
    fclose(csv);
    
    printf("=== ТЕСТЫ ЗАВЕРШЕНЫ ===\n");
    printf("Результаты сохранены в scheduler_comparison_results.csv\n\n");
    
    // Генерируем простой отчет
    printf("Краткий отчет:\n");
    printf("--------------\n");
    
    FILE *csv_read = fopen("scheduler_comparison_results.csv", "r");
    if (csv_read) {
        char line[256];
        fgets(line, sizeof(line), csv_read); // Пропускаем заголовок
        
        double total_improvement = 0;
        int count = 0;
        
        while (fgets(line, sizeof(line), csv_read)) {
            double improvement;
            sscanf(line, "%*d,%*d,%*d,%*d,%*f,%*f,%*d,%*d,%*d,%*d,%*f,%*f,%lf", &improvement);
            total_improvement += improvement;
            count++;
        }
        
        if (count > 0) {
            printf("Среднее улучшение производительности: %.2f%%\n", total_improvement / count);
        }
        
        fclose(csv_read);
    }
    
    return 0;
}
