#!/usr/bin/env bash
# Один проход: боец (до плато) → Сборщики A/B → калибровка. Можно останавливать и запускать снова:
# этапы с флагом .done пропускаются, боец продолжает с контрольной точки.
set -euo pipefail
cd "$(dirname "$0")/.."
D=runs/net/full-20261125
S=runs/net/cycle-20261125-srd/round-3
mkdir -p "$D"
st() { echo "$(date -Is) $1" | tee -a "$D/stages.log"; }
export DND_GAMMA=0.998 DND_LAMBDA=0.97 DND_ROUNDPEN=0.005 DND_TRADE_D=1 DND_TRADE_T=0.5
unset DND_TRAIN_PRICES

if [ ! -f "$D/fighter.done" ]; then
  st 'боец: PPO до плато (с контрольной точки, если есть)'
  [ -f "$D/fighter.bc.bin" ] || cp "$S/fighter.bin.best.bin" "$D/fighter.bc.bin"
  [ -f "$D/fighter.bin" ]    || cp "$S/fighter.bin.best.bin" "$D/fighter.bin"
  ./scripts/run_ptrain.sh "$D/fighter" 800 512
  test -s "$D/fighter.bin.best.bin"
  touch "$D/fighter.done"
fi
export DND_NETP="$PWD/$D/fighter.bin.best.bin"

if [ ! -f "$D/builder.done" ]; then
  st 'сборщики A и B (с начала)'
  rm -f "$D/builder.log"          # run_builder.sh не запускается при существующем логе
  ./scripts/run_builder.sh "$D/builder" 200 768 "$S/builder_A.bin" "$S/builder_B.bin"
  test -s "$D/builder_A.bin" && test -s "$D/builder_B.bin"
  touch "$D/builder.done"
fi
export DND_CALIB_BLDA="$PWD/$D/builder_A.bin" DND_CALIB_BLDB="$PWD/$D/builder_B.bin"

if [ ! -f "$D/calib.done" ]; then
  st 'калибровка'
  ./scripts/run_calib_net.sh
  touch "$D/calib.done"
fi
st 'готово: сводки в results/calib/'
