#!/bin/sh
# Калибровка на правиле-боте: два зерна подряд (каждое на всех ядрах, результат не зависит от числа потоков), затем сводка.
# Использование: ./scripts/run_calib.sh        Итог — results/calib/
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1   # корень проекта
make -s build/arena || exit 1
O=results/calib; mkdir -p "$O"
./build/arena 0 24000 30 42  > "$O/calibA.txt"
./build/arena 0 24000 30 777 > "$O/calibB.txt"
python3 tools/an.py "$O/calibA.txt" "$O/calibB.txt" > "$O/summary_calib.txt"
cat "$O/summary_calib.txt"
echo "--- если всё в порядке: python3 tools/bake.py $O/calibA.txt $O/calibB.txt ---"
