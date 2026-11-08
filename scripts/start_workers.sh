#!/bin/sh
# Тест с игроками через Cloudflare Workers: постоянная ссылка https://dnd-arena.<логин>.workers.dev.
# Сервер игры и связь с посредником работают на этом компьютере; связь упала — восстанавливается сама, ссылка та же.
# Первый раз (один раз): cd relay && npx wrangler login && npx wrangler deploy && npx wrangler secret put HOST_KEY < host_key.txt
# Дальше: ./scripts/start_workers.sh        (адрес берётся из relay/worker_url.txt; Ctrl+C — остановить связь)
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1   # корень проекта (скрипт лежит в scripts/)
make -s build/arena_play || exit 1
U=$(cat relay/worker_url.txt 2>/dev/null)
[ -z "$U" ] && { echo "нет relay/worker_url.txt: сначала выложите посредника (см. начало файла)"; exit 1; }
if ! curl -s -o /dev/null http://127.0.0.1:8770/; then
    mkdir -p state; setsid nohup python3 web/play_server.py 8770 > state/play_server.log 2>&1 < /dev/null &
    sleep 1
fi
echo "Ссылка для игроков: $U"
exec node relay/host.mjs "$U"
