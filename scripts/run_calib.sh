#!/bin/sh
# Использование: ./run_calib.sh boss_arena_v20.cc   — два зерна подряд (каждое на всех ядрах, результат не зависит от числа потоков), затем сводка
g++ -std=c++17 -O2 -pthread -o arena "$1" || exit 1
./arena 0 24000 30 42  > calibA.txt
./arena 0 24000 30 777 > calibB.txt
python3 an.py calibA.txt calibB.txt > summary_calib.txt
cat summary_calib.txt
echo "--- если всё в порядке: python3 bake.py $1 calibA.txt calibB.txt ---"
