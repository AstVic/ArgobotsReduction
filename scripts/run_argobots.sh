#!/bin/bash

cd cg_argobots
make clean
make veryclean
make suite
cd ..

mkdir -p raw_results_argobots

XSTREAMS=(1 2 4 8 16)
THREADS=(1 2 4 8 16 32 64 128)

for num_xstreams in "${XSTREAMS[@]}"
do
  for num_threads in "${THREADS[@]}"
  do
    for file in cg_argobots/bin/*;
    do
      if [[ -x "$file" ]]; then
          filename=$(basename "$file")
          
          for i in {1..3}; do
            ./"$file" -s $num_xstreams -t $num_threads > "raw_results_argobots/${filename}_xstreams_${num_xstreams}_threads_${num_threads}_${i}.txt" 2>&1
          done
      else
          echo "Файл $file не является исполняемым и будет пропущен"
      fi
      echo "Файл $file обработан с num_xstreams=$num_xstreams num_threads=$num_threads"
    done
  done
done
