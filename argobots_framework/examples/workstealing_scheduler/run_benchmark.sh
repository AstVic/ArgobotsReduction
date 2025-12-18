#!/bin/bash

echo "=== Бенчмарк планировщиков задач ==="
echo "Запуск сначала старого, затем нового планировщика"
echo ""

# Создаем директорию для результатов
mkdir -p benchmark_results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Компилируем все
echo "1. Компиляция программ..."
make clean
make

if [ $? -ne 0 ]; then
    echo "Ошибка компиляции!"
    exit 1
fi

echo "2. Запуск тестов..."
echo "   Каждый тест будет запущен дважды:"
echo "   1) Сначала со старым планировщиком"
echo "   2) Затем с новым планировщиком"
echo ""

# Массив конфигураций для тестов
CONFIGS=(
    "2 10 0"   # 2 потока, 10 задач, легкие
    "4 25 1"   # 4 потока, 25 задач, средние
    "8 50 2"   # 8 потоков, 50 задач, тяжелые
    "4 30 3"   # 4 потока, 30 задач, смешанные
    "4 40 4"   # 4 потока, 40 задач, неравномерные
)

# Запускаем каждый тест
for i in "${!CONFIGS[@]}"; do
    CONFIG=(${CONFIGS[$i]})
    NUM_XSTREAMS=${CONFIG[0]}
    TASKS_PER_STREAM=${CONFIG[1]}
    COMPLEXITY_MODE=${CONFIG[2]}
    
    echo "Тест $((i+1)): ${NUM_XSTREAMS} потоков, ${TASKS_PER_STREAM} задач/поток, сложность ${COMPLEXITY_MODE}"
    
    # Создаем отдельный исполняемый файл для каждого теста
    cat > benchmark_test_${i}.c << EOF
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <abt.h>

#define NUM_XSTREAMS ${NUM_XSTREAMS}
#define NUM_TASKS_PER_STREAM ${TASKS_PER_STREAM}
#define COMPLEXITY_MODE ${COMPLEXITY_MODE}

$(cat compare_schedulers_real.c | tail -n +2)
EOF
    
    # Компилируем и запускаем
    gcc -O2 -Wall -Wextra -I$HOME/argobots-install/include \
        benchmark_test_${i}.c \
        abt_workstealing_scheduler.c \
        abt_workstealing_scheduler_cost_aware.c \
        -L$HOME/argobots-install/lib -labt -lpthread \
        -o benchmark_test_${i}
    
    echo "  Запуск..."
    ./benchmark_test_${i} > benchmark_results/test_${i}_${TIMESTAMP}.log 2>&1
    
    # Пауза между тестами
    sleep 3
done

echo ""
echo "3. Анализ результатов..."

# Собираем все результаты в один файл
cat benchmark_results/*.log | grep -E "(Результаты:|Улучшение:|Тест [0-9]+:)" > benchmark_results/summary_${TIMESTAMP}.txt

echo ""
echo "=== ВСЕ ТЕСТЫ ЗАВЕРШЕНЫ ==="
echo "Результаты сохранены в папке benchmark_results/"
echo "Сводка: benchmark_results/summary_${TIMESTAMP}.txt"
echo "Детальные результаты: scheduler_comparison_results.csv"