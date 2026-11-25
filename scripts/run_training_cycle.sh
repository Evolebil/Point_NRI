#!/usr/bin/env bash
# Последовательное обучение в отдельной папке. Тот же каталог продолжает прерванный круг.
set -euo pipefail
cd "$(dirname "$0")/.."
run=${1:?Укажите каталог запуска}
mkdir -p "$run"
run=$(realpath "$run")
exec 9>"$run/cycle.lock"
flock -n 9 || { echo 'Цикл уже запущен в этом каталоге' >&2; exit 1; }
# Все три этапа должны использовать одну версию атомов, даже если проект пересобирают.
if [[ ! -d "$run/bin" ]]; then
    staging=$(mktemp -d "$run/bin.XXXXXX")
    cp build/net_fighter build/net_builder build/arena "$staging/"
    mv "$staging" "$run/bin"
fi
cores=$(nproc)
export DND_THREADS=${DND_THREADS:-$((cores > 2 ? cores - 2 : 1))}
export DND_CALIB_BLD=0.5
export DND_GAMMA=0.998 DND_LAMBDA=0.97 DND_ROUNDPEN=0.005 DND_TRADE_D=1 DND_TRADE_T=0.5
export DND_CALIB_BLDA="${DND_START_BLDA:-$PWD/models/nets/builder_A.bin}" DND_CALIB_BLDB="${DND_START_BLDB:-$PWD/models/nets/builder_B.bin}"
fighter="${DND_START_FIGHTER:-$PWD/models/nets/net_fighter_p.bin}"
unset DND_TRAIN_PRICES
stage() { echo "$(date -Is) круг $round — $1" | tee -a "$run/cycle.log"; }
for ((round=1; ; round++)); do
    dir="$run/round-$round"
    mkdir -p "$dir"
    if [[ ! -f "$dir/fighter.done" ]]; then
        stage 'боец'
        DND_NETP=off "$run/bin/net_fighter" ptrain 800 512 "$((7000+round))" "$dir/fighter.bin" "$fighter" >>"$dir/fighter.log" 2>&1
        test -s "$dir/fighter.bin.best.bin"
        touch "$dir/fighter.done"
    fi
    fighter="$dir/fighter.bin.best.bin"
    export DND_NETP="$fighter"
    if [[ ! -f "$dir/builder.done" ]]; then
        stage 'сборщик'
        "$run/bin/net_builder" train 200 768 "$((3000+round))" "$dir/builder" "$DND_CALIB_BLDA" "$DND_CALIB_BLDB" >>"$dir/builder.log" 2>&1
        test -s "$dir/builder_A.bin" && test -s "$dir/builder_B.bin" || exit 1
        touch "$dir/builder.done"
    fi
    export DND_CALIB_BLDA="$dir/builder_A.bin" DND_CALIB_BLDB="$dir/builder_B.bin"
    if [[ ! -f "$dir/prices.tsv" ]]; then
        stage 'калибровщик'
        DND_TRAIN_PRICES_OUT="$dir/prices.tmp" "$run/bin/arena" 0 6000 8 "$((9000+round))" >"$dir/calibration.log" 2>&1
        test -s "$dir/prices.tmp"
        mv "$dir/prices.tmp" "$dir/prices.tsv"
        stage 'завершён'
    fi
    export DND_TRAIN_PRICES="$dir/prices.tsv"
done
