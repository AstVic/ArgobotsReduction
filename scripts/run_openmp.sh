#!/bin/bash

cd cg_openmp
make clean
make veryclean
make suite
cd ..

mkdir -p raw_results_openmp

THREADS=(1 2 4 8 16 32 64 128)

for num_threads in "${THREADS[@]}"
do
  for file in cg_openmp/bin/*;
  do
    if [[ -x "$file" ]]; then
        filename=$(basename "$file")
        
        for i in {1..3}; do
          OMP_NUM_THREADS=$num_threads ./"$file"  > "raw_results_openmp/${filename}_threads_${num_threads}_${i}.txt" 2>&1
        done
    else
        echo "Файл $file не является исполняемым и будет пропущен"
    fi
    echo "Файл $file обработан с num_threads=$num_threads"
  done
done
