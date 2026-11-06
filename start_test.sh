#!/bin/sh
# Запуск игры для теста с другими игроками: сервер игры + туннель Cloudflare (ссылка https://….trycloudflare.com).
# Использование: ./start_test.sh        Остановка: ./start_test.sh stop
# Ссылка меняется при каждом запуске туннеля — после перезапуска разошлите новую. Компьютер должен быть включён и в сети.
cd "$(dirname "$0")" || exit 1
CF="$HOME/.local/opt/cloudflared/cloudflared"
LOG=/tmp/dnd_tunnel.log
if [ "$1" = "stop" ]; then
    [ -f /tmp/dnd_tunnel.pid ] && kill "$(cat /tmp/dnd_tunnel.pid)" 2>/dev/null
    [ -f /tmp/dnd_server.pid ] && kill "$(cat /tmp/dnd_server.pid)" 2>/dev/null
    rm -f /tmp/dnd_tunnel.pid /tmp/dnd_server.pid
    echo "остановлено"
    exit 0
fi
[ -x ./arena_play ] || g++ -std=c++17 -O2 -pthread -o arena_play arena_play.cc || exit 1
# сервер: боссами управляет правило-бот (сеть p6 на картах мира слабее бота — доучим к релизу)
if ! curl -s -o /dev/null http://127.0.0.1:8770/; then
    DND_NETP=off setsid nohup python3 play_server.py 8770 > /tmp/dnd_server.log 2>&1 < /dev/null &
    echo $! > /tmp/dnd_server.pid
    sleep 1
fi
# туннель: HTTP/2 поверх TCP (QUIC/UDP у многих провайдеров не проходит); до 3 попыток (сервис ссылок иногда не отвечает)
[ -f /tmp/dnd_tunnel.pid ] && kill "$(cat /tmp/dnd_tunnel.pid)" 2>/dev/null
for try in 1 2 3; do
    rm -f "$LOG"
    setsid nohup "$CF" tunnel --no-autoupdate --protocol http2 --url http://127.0.0.1:8770 > "$LOG" 2>&1 < /dev/null &
    echo $! > /tmp/dnd_tunnel.pid
    i=0
    while [ $i -lt 45 ]; do
        grep -aq "Registered tunnel connection" "$LOG" && break
        grep -aq "failed to request quick Tunnel" "$LOG" && break
        sleep 1
        i=$((i + 1))
    done
    grep -aq "Registered tunnel connection" "$LOG" && break
    kill "$(cat /tmp/dnd_tunnel.pid)" 2>/dev/null
    echo "попытка $try не удалась, ещё раз…"
done
U=$(grep -ao "https://[a-z0-9-]*\.trycloudflare\.com" "$LOG" | head -1)
if [ -z "$U" ] || ! grep -aq "Registered tunnel connection" "$LOG"; then
    echo "туннель не поднялся, журнал: $LOG"
    grep -a "ERR\|failed" "$LOG" | tail -3
    exit 1
fi
echo "Игра доступна по ссылке: $U   (заработает через ~30 секунд)"
echo "(у себя: http://127.0.0.1:8770/ ; остановить всё: ./start_test.sh stop — гасит и сервер, и туннель)"
