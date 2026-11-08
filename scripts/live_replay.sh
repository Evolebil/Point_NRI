#!/bin/sh
# Живой просмотр обучения: раз в MIN минут — бой ТЕКУЩИХ весов сети против бота ИНТ 10 (новая пара боссов, бюджет и
# карта) в один и тот же файл <префикс>_live.html. Открытая страница сама идёт к бою, а после его конца ждёт 15 с и
# перезагружается — уже с новым боем. Обучению не мешает (берёт копию весов, один поток на бой).
# Использование: ./scripts/live_replay.sh [префикс=runs/net/p2] [минут между боями=3]
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1   # корень проекта
P=${1:-runs/net/p2}
MIN=${2:-3}
OUT="${P}_live.html"
export DND_NETP=off DND_LIVE=15 DND_THREADS=1
# своя копия программы: build/net_fighter может пересобираться идущим обучением
make -s build/net_fighter && cp build/net_fighter build/net_fighter_live || exit 1
echo "живой просмотр: открой $OUT в браузере (Ctrl+C — остановить)"
while :; do
    if [ -f "$P.bin" ] && cp "$P.bin" "$P.live.bin"; then
        B=$((200 << $(od -An -N1 -tu1 /dev/urandom | tr -d ' ') % 6))
        S=$(od -An -N4 -tu4 /dev/urandom | tr -d ' ')
        if ./build/net_fighter_live preplay "$P.live.bin" "$B" "$S" "$OUT.tmp" > "$P.live.txt" 2>&1; then
            mv "$OUT.tmp" "$OUT"
            echo "$(date +%H:%M) бюджет $B, зерно $S: $(head -1 "$P.live.txt")"
        fi
    fi
    sleep $((MIN * 60))
done
