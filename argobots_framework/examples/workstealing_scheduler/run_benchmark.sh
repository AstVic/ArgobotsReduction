#!/bin/bash

export C_INCLUDE_PATH="$HOME/local/argobots/include"
export LIBRARY_PATH="$HOME/local/argobots/lib"
export DYLD_LIBRARY_PATH="$HOME/local/argobots/lib"   

set -euo pipefail

echo "=== Бенчмарк планировщиков задач ==="
echo "Запуск сначала старого, затем нового планировщика"
echo ""

resolve_argobots_flags() {
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists argobots; then
        ABT_CFLAGS="$(pkg-config --cflags argobots)"
        ABT_LIBS="$(pkg-config --libs argobots) -lpthread"
        echo "Используем Argobots через pkg-config"
        return
    fi

    local candidates=()
    if [ -n "${ARGOBOTS_INSTALL_DIR:-}" ]; then
        candidates+=("$ARGOBOTS_INSTALL_DIR")
    fi
    candidates+=("$HOME/argobots-install" "/usr/local" "/usr")

    for dir in "${candidates[@]}"; do
        if [ -f "$dir/include/abt.h" ] && [ -d "$dir/lib" ]; then
            ABT_CFLAGS="-I$dir/include"
            ABT_LIBS="-L$dir/lib -labt -lpthread"
            echo "Используем Argobots из $dir"
            return
        fi
    done

    echo "Ошибка: не удалось найти Argobots (abt.h/libabt)." >&2
    echo "Подсказка: установите ARGOBOTS_INSTALL_DIR или настройте pkg-config для argobots." >&2
    exit 1
}

resolve_argobots_flags

# Создаем директорию для результатов
mkdir -p benchmark_results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Компилируем все
echo "1. Компиляция программ..."
make clean
make 

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
    cat > benchmark_test_${i}.c << EOF2
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <abt.h>

#define NUM_XSTREAMS ${NUM_XSTREAMS}
#define NUM_TASKS_PER_STREAM ${TASKS_PER_STREAM}
#define COMPLEXITY_MODE ${COMPLEXITY_MODE}

$(tail -n +2 compare_schedulers_real.c)
EOF2

    # Компилируем и запускаем
    gcc -O2 -Wall -Wextra $ABT_CFLAGS \
        benchmark_test_${i}.c \
        abt_workstealing_scheduler.c \
        abt_workstealing_scheduler_cost_aware.c \
        $ABT_LIBS \
        -o benchmark_test_${i}

    echo "  Запуск..."
    ./benchmark_test_${i} > benchmark_results/test_${i}_${TIMESTAMP}.log 2>&1

    # Пауза между тестами
    sleep 3
done

echo ""
echo "3. Анализ результатов..."

# Собираем все результаты в один файл
grep -hE "^(=== Тест [0-9]+ ===|OLD:|NEW:|Improvement:)" benchmark_results/*.log \
    > benchmark_results/summary_${TIMESTAMP}.txt

echo ""
echo "=== ВСЕ ТЕСТЫ ЗАВЕРШЕНЫ ==="
echo "Результаты сохранены в папке benchmark_results/"
echo "Сводка: benchmark_results/summary_${TIMESTAMP}.txt"
echo "Детальные результаты: scheduler_comparison_results.csv"