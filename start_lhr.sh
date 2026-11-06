#!/bin/sh
# Туннель через SSH (localhost.run) с автопереподключением: упал — поднимается снова через 3 с. Ссылка при этом может
# смениться: новая печатается и пишется в lhr_url.txt. Игроки открывают новую ссылку и входят под тем же именем —
# персонаж и место в бою возвращаются. Сервер игры поднимается, если не запущен. Остановка: Ctrl+C.
cd "$(dirname "$0")" || exit 1
if ! curl -s -o /dev/null http://127.0.0.1:8770/; then
    DND_NETP=off setsid nohup python3 play_server.py 8770 > /tmp/dnd_server.log 2>&1 < /dev/null &
    sleep 1
fi
# свой SSH-ключ (зарегистрированный на admin.localhost.run — адрес держится дольше) или без ключа
USER_AT=nokey@
ls "$HOME"/.ssh/id_ed25519 "$HOME"/.ssh/id_rsa >/dev/null 2>&1 && USER_AT=
while true; do
    ssh -o StrictHostKeyChecking=accept-new -o ServerAliveInterval=30 -o ServerAliveCountMax=8 -o TCPKeepAlive=yes -o ExitOnForwardFailure=yes \
        -R 80:localhost:8770 "${USER_AT}localhost.run" 2>&1 | while IFS= read -r line; do
        url=$(printf '%s\n' "$line" | grep -o 'https://[a-z0-9.-]*\.lhr\.life' | head -1)
        if [ -n "$url" ]; then
            echo "$url" > lhr_url.txt
            echo "=== $(date +%H:%M:%S) ссылка для игроков: $url"
        fi
    done
    echo "--- $(date +%H:%M:%S) туннель упал, переподключаюсь…"
    sleep 3
done
