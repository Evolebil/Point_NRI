#!/bin/sh
# Быстрая сверка (минуты, не час): сборка всех программ и короткие прогоны. Для перестроек кода:
# ./tests/golden/quick_check.sh > до.txt; …правка…; ./tests/golden/quick_check.sh > после.txt; diff до.txt после.txt
cd "$(dirname "$(readlink -f "$0")")/../.." || exit 1
for p in arena arena_play audit_tests net_fighter net_builder tiers; do
    if make -s "build/$p" 2>/dev/null; then echo "сборка $p: да"; else echo "сборка $p: НЕТ"; fi
done
echo "бои с сетью: $(./build/arena 0 600 6 42 2>/dev/null | md5sum)"
echo "бои без сети: $(DND_NETP=off ./build/arena 0 600 6 42 2>/dev/null | md5sum)"
T=$(mktemp -d); ./build/arena replay 1600 42 "$T/r.html" > /dev/null 2>&1; echo "просмотр: $(md5sum < "$T/r.html")"; rm -rf "$T"
echo "калькулятор: $(python3 tools/calc.py demo/rycar.json 2>&1 | md5sum)"
echo "протокол игры: $(python3 tests/test_play.py 2>&1 | md5sum)"
echo "шпаргалка: $(python3 tools/sheet.py 800 2>&1 | md5sum)"
