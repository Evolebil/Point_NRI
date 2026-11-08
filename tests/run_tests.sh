#!/bin/sh
# Использование: ./tests/run_tests.sh   (или make test). Итог — results/tests/tests_summary.txt, полный журнал — tests_full.txt там же.
# Проверки идут параллельно: столько процессов, сколько ядер (DND_TEST_JOBS — своё число); блок проверок k выполняет
# процесс k % N со своим зерном, строки собираются по порядку блоков — результат тот же при любом числе процессов.
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1   # корень проекта
make -s build/audit_tests || exit 1
OUT=results/tests; mkdir -p "$OUT"
N=${DND_TEST_JOBS:-$(nproc)}
D=$(mktemp -d)
i=0
while [ "$i" -lt "$N" ]; do
    ./build/audit_tests "$N" "$i" > "$D/part$i" 2>&1 &
    i=$((i + 1))
done
wait
{
    printf "ID   Рез.  Требование (правила v0.5)                                      Детали\n"
    cat "$D"/part* | grep -a '^@' | sort -s -k1,1 | cut -c7-
    cat "$D"/part* | grep -a -v '^@\|^#ИТОГ' | grep -a . | sed 's/^/(вне проверок) /'
    awk '/^#ИТОГ/ {p += $2; f += $3} END {printf "\nИтого: PASS %d, FAIL %d\n", p, f}' "$D"/part*
    n=$(ls "$D"/part* | wc -l); k=$(cat "$D"/part* | grep -a -c '^#ИТОГ')
    [ "$k" -lt "$n" ] && echo "ВНИМАНИЕ: $((n - k)) процесс(ов) из $n не дошли до конца (упали) — см. выше"
} > "$OUT/tests_full.txt"
rm -rf "$D"
grep -E "FAIL|Итого|ВНИМАНИЕ" "$OUT/tests_full.txt" > "$OUT/tests_summary.txt"
cat "$OUT/tests_summary.txt"
echo "--- полный журнал: $OUT/tests_full.txt; пришли Claude $OUT/tests_summary.txt ---"
