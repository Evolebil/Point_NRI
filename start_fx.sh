#!/bin/sh
# Тест с игроками через fxTunnel (серверы в России): ссылка https://<имя>.fxtun.ru.
# Один раз: скачать клиент и войти (fxtunnel login). Дальше: ./start_fx.sh [поддомен]   (по умолчанию dnd-arena)
# Туннель упал — поднимается снова через 3 с; с поддоменом ссылка та же. Остановка: Ctrl+C.
cd "$(dirname "$0")" || exit 1
SUB=${1:-dnd-arena}
FX=$(command -v fxtunnel || echo "$HOME/.local/opt/fxtunnel/fxtunnel")
[ -x "$FX" ] || { echo "нет клиента fxtunnel: скачайте его (см. инструкцию)"; exit 1; }
if ! curl -s -o /dev/null http://127.0.0.1:8770/; then
    DND_NETP=off setsid nohup python3 play_server.py 8770 > /tmp/dnd_server.log 2>&1 < /dev/null &
    sleep 1
fi
while true; do
    "$FX" http 8770 --domain "$SUB"
    echo "--- $(date +%H:%M:%S) туннель упал, переподключаюсь…"
    sleep 3
done
