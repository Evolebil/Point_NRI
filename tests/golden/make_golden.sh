#!/bin/sh
# Эталон для уборки кода: ./tests/golden/make_golden.sh — снять (около часа), ./tests/golden/make_golden.sh check — сравнить
# (вывод должен совпасть побитово).
cd "$(dirname "$(readlink -f "$0")")/../.." || exit 1
OUT=tests/golden; [ "$1" = check ] && OUT=$(mktemp -d)
make -s build/arena build/arena_play || exit 1
./build/arena 0 12000 6 42 2>/dev/null | md5sum > $OUT/fights_net.md5
DND_NETP=off ./build/arena 0 12000 6 42 2>/dev/null | md5sum > $OUT/fights_nonet.md5
./build/arena replay 1600 42 $OUT/replay.html > /dev/null 2>&1; md5sum < $OUT/replay.html > $OUT/replay.md5; rm -f $OUT/replay.html
./build/arena ladder 800 1600 6000 42 2>/dev/null | grep -v '^WARN' > $OUT/ladder.txt
python3 tools/calc.py demo/rycar.json > $OUT/calc_rycar.txt 2>&1
python3 tests/test_play.py > $OUT/test_play.txt 2>&1
if [ "$1" = check ]; then
    bad=0
    for f in fights_net.md5 fights_nonet.md5 replay.md5 ladder.txt calc_rycar.txt test_play.txt; do
        if cmp -s tests/golden/$f $OUT/$f; then echo "совпало: $f"; else echo "РАЗНИЦА: $f"; diff tests/golden/$f $OUT/$f | head -5; bad=1; fi
    done
    rm -rf "$OUT"; exit $bad
fi
echo "эталон снят в $OUT"
