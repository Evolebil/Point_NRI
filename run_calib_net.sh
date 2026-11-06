#!/bin/sh
# Этап 4 ТЗ: калибровка цен на обученном бойце — все боссы ходят сетью net_fighter_p.bin (ИНТ только игровая
# характеристика), метрики использования №6 (атомы, которыми почти не пользуются, не дешевеют). Для сравнения — та же
# калибровка на правиле-боте (DND_NETP=off, ~2 минуты). Цены в движок НЕ запекаются: сначала смотрим сводки.
# Этап 5б (решение владельца): калибровка на смеси — в доле DND_CALIB_BLD пар (по умолчанию 0.5) боссов собирают
# сети-Сборщики (DND_CALIB_BLDA / DND_CALIB_BLDB, по умолчанию net_runs/builder5_A/B.bin); бот-калибровка — без них.
# Использование: ./run_calib_net.sh [боёв за итерацию=24000] [итераций=30]
F=${1:-24000}
IT=${2:-30}
g++ -std=c++17 -O2 -pthread -o arena boss_arena_v20.cc || exit 1
# прежние результаты не затираются: копия в builder_runs/calib_prev_<время>/
D=builder_runs/calib_prev_$(date +%m%d_%H%M); mkdir -p "$D"; cp calib_*.txt summary_calib_*.txt "$D"/ 2>/dev/null
DND_NETP=off ./arena 0 "$F" "$IT" 42 > calib_bot_A.txt 2>/dev/null
DND_NETP=off ./arena 0 "$F" "$IT" 777 > calib_bot_B.txt 2>/dev/null
python3 an.py calib_bot_A.txt calib_bot_B.txt > summary_calib_bot.txt
export DND_CALIB_BLD=${DND_CALIB_BLD:-0.5}
./arena 0 "$F" "$IT" 42 > calib_net_A.txt 2>/dev/null
./arena 0 "$F" "$IT" 777 > calib_net_B.txt 2>/dev/null
python3 an.py calib_net_A.txt calib_net_B.txt > summary_calib_net.txt
echo "готово: summary_calib_net.txt (сеть), summary_calib_bot.txt (бот)"
