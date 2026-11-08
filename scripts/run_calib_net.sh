#!/bin/sh
# Калибровка цен на обученном бойце — все боссы ходят сетью models/nets/net_fighter_p.bin (ИНТ только игровая
# характеристика), метрики использования (атомы, которыми почти не пользуются, не дешевеют). Для сравнения — та же
# калибровка на правиле-боте (DND_NETP=off). Цены в движок НЕ запекаются: сначала смотрим сводки.
# Калибровка на смеси (решение владельца): в доле DND_CALIB_BLD пар (по умолчанию 0.5) боссов собирают сети-Сборщики
# (DND_CALIB_BLDA / DND_CALIB_BLDB, по умолчанию models/nets/builder5_A/B.bin); бот-калибровка — без них.
# Использование: ./scripts/run_calib_net.sh [боёв за итерацию=24000] [итераций=30]      Итог — results/calib/
# прежние результаты не затираются: копия в results/calib/prev_<время>/
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1   # корень проекта
F=${1:-24000}
IT=${2:-30}
make -s build/arena || exit 1
O=results/calib; mkdir -p "$O"
P=$O/prev_$(date +%m%d_%H%M); mkdir -p "$P"; cp "$O"/calib_*.txt "$O"/summary_calib_*.txt "$P"/ 2>/dev/null
DND_NETP=off ./build/arena 0 "$F" "$IT" 42 > "$O/calib_bot_A.txt" 2>/dev/null
DND_NETP=off ./build/arena 0 "$F" "$IT" 777 > "$O/calib_bot_B.txt" 2>/dev/null
python3 tools/an.py "$O/calib_bot_A.txt" "$O/calib_bot_B.txt" > "$O/summary_calib_bot.txt"
export DND_CALIB_BLD=${DND_CALIB_BLD:-0.5}
./build/arena 0 "$F" "$IT" 42 > "$O/calib_net_A.txt" 2>/dev/null
./build/arena 0 "$F" "$IT" 777 > "$O/calib_net_B.txt" 2>/dev/null
python3 tools/an.py "$O/calib_net_A.txt" "$O/calib_net_B.txt" > "$O/summary_calib_net.txt"
echo "готово: $O/summary_calib_net.txt (сеть), $O/summary_calib_bot.txt (бот)"
