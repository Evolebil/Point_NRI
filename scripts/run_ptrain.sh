#!/bin/sh
# Обучение сети-бойца: копирование бота, затем PPO. Несколько часов на ядрах − 2.
# Использование: ./scripts/run_ptrain.sh [префикс=runs/net/p1] [итераций PPO=800] [боёв в итерации=512]
# Обучение само останавливается на плато: 6 проверок подряд (300 итераций) без роста лучшего результата на 0.5 п.п.
# (DND_PATIENCE — число проверок); энтропия 0.1 (DND_ENT). В конце — проверка на 4000 боёв и честная (зеркальная) на 3000 пар.
# Повторный запуск продолжает PPO с последней контрольной точки (<префикс>.bin.state); копирование не повторяется,
# если уже есть <префикс>.bc.bin. Журнал — <префикс>.log, странные победы — <префикс>.bin.strange.txt,
# лучшая версия на отложенных боях — <префикс>.bin.best.bin (в игру её ставят копированием в models/nets/net_fighter_p.bin).
cd "$(dirname "$(readlink -f "$0")")/.." || exit 1   # корень проекта
P=${1:-runs/net/p1}
IT=${2:-800}
N=${3:-512}
make -s build/net_fighter || exit 1
mkdir -p "$(dirname "$P")"
export DND_NETP=off   # бои учителя и соперников-ботов — без готовой сети
if [ ! -f "$P.bc.bin" ]; then
    ./build/net_fighter pbc 16384 8 1 "$P.bc.bin" >> "$P.log" 2>&1 || exit 1
fi
[ -f "$P.bin" ] || cp "$P.bc.bin" "$P.bin"
./build/net_fighter ptrain "$IT" "$N" 7 "$P.bin" >> "$P.log" 2>&1
./build/net_fighter peval "$P.bin.best.bin" 4000 >> "$P.log" 2>&1
./build/net_fighter pfair "$P.bin.best.bin" 3000 >> "$P.log" 2>&1
