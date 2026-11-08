# DND_PRO: Арена Боссов

Босс собирается на бюджет очков из «атомов» (механик); цены атомов подбирает симуляция так, чтобы владелец атома выигрывал ~50% боёв.

## Папки
Файлы разложены по папкам (аудит кода, шаг 6, 8 ноября). В корне остались симлинки на всё, чем пользуются команды и
скрипты, поэтому команды ниже работают из корня как раньше; `g++ -Isrc/engine -Isrc/ai src/apps/arena_play.cc` тоже
собирается.

| Папка | Что там |
|---|---|
| `src/engine/` | движок: `boss_arena_v20.cc` (правила, генератор, бой, калибровка; `./arena`), карты `world_gen.h` и `custom_map.h`, игрок `char_spec.h` и `human_turn.h`, просмотр `replay_export.h`, `mini_json.h`, `par.h` (потоки), `dnd_root.h` (где искать файлы: `DND_ROOT` → текущая папка → папка программы) |
| `src/ai/` | сети: боец v21 `prim_net.h` и обучение `prim_train.h`, старый боец `fighter_net.h`/`fighter_train.h`, Сборщик `builder_net.h`, общий `gru.h` |
| `src/apps/` | программы: `arena_play.cc` (движок игры для сервера), `net_fighter.cc`, `net_builder.cc`, `net_teams.cc`, `builder.cc` |
| `web/` | игра в браузере: `play_server.py` (`python3 play_server.py`, журнал ошибок — stderr), `play_template.html`, `viewer_template.html` (просмотр боя) |
| `relay/` | связь через Cloudflare Workers (`relay/host.mjs`; секрет `host_key.txt` — не в git) |
| `tools/` | `calc.py` (цена персонажа), `sheet.py` (шпаргалка), `bake.py` / `an.py` (калибровка), `apply_tiers.py`, `apply_adv.py` |
| `scripts/` | `start_*.sh` (запуск игры и туннелей), `run_calib*.sh`, `run_ptrain.sh`, `run_builder.sh`, `live_replay.sh` |
| `tests/` | `audit_tests20.cc` (233 проверки, `./run_tests.sh audit_tests20.cc`), `test_play.py`; `tests/golden/` — эталон для уборки кода: `make_golden.sh` (снять / `check`, около часа) и `quick_check.sh` (минуты) |
| `config/` | `limity.json` — технические потолки и растяжки призыва |
| `models/nets/` | веса сетей в игре: `net_fighter_p.bin` (боец v21), `net_fighter.bin` (старый); остальное в `models/` — локальная языковая модель, не в git |
| `docs/` | правила (`pravila-v21.md`, `pravila-v20.md`), ТЗ и список работ (`TZ_backlog.md`, `TZ_atomy_posle_testa.md`, …), журнал `istoriya-razrabotki.md`, каталог атомов, памятка игрока, шпаргалка `kratko-pravila.md` |
| `results/` | итоги прогонов (калибровка, лестницы, тесты) — не в git |
| `runs/` | `runs/net/` (= `net_runs/`) — веса и журналы обучения сетей; `runs/builder/` (= `builder_runs/`) — Сборщик, резервные копии; не в git |
| `demo/`, `kampaniya/`, `Sushnost/` | примеры боёв и персонаж; кампания (указатель на Obsidian, карты мира); выгрузка данных бестиария |

## Команды
```
g++ -std=c++17 -O2 -pthread -o arena_play arena_play.cc && python3 play_server.py   # игра за персонажа: http://127.0.0.1:8770/ (журнал ошибок — stderr сервера)
python3 calc.py demo/rycar.json                    # цена персонажа по файлу
python3 sheet.py 800 > kratko-pravila.md           # шпаргалка цен
./arena replay 1600 42 battle.html                 # просмотр: бюджет, зерно, файл (viewer_template.html лежит рядом)
./net_builder replay net_runs/b7_A.bin net_runs/b7_B.bin 1600 5 battle.html   # боссов собирают нейросети-Сборщики
./run_tests.sh audit_tests20.cc                    # тесты
./run_calib.sh boss_arena_v20.cc                   # калибровка двумя зёрнами (~1.5 мин, ядра − 2)
python3 bake.py boss_arena_v20.cc calibA.txt calibB.txt   # запечь цены в движок
./builder tiers 8 1200 7                           # замер и подгонка лестниц размерных атомов
python3 apply_tiers.py boss_arena_v20.cc tiers_fit.txt
./builder train <0..5> 300 3000 <зерно>            # обучить Сборщика на бюджете 200·2^i
./builder report builder_runs/champ_*.txt          # отчёт о дырах
./net_fighter train 1200 2048 <зерно> net_runs/f.bin [стартовые_веса]   # обучить сеть-бойца (DND_SELF=60 DND_TIMEPEN=0.06)
./net_fighter eval net_fighter.bin 4000            # сеть ИНТ 16 против правило-бота ИНТ 10
./net_fighter upgrade20 net_runs/f11.bin net_runs/f11_v21.bin   # перенести веса v20 в формат v21 (новые признаки — с нулевыми весами)
./net_fighter pinit net_fighter_p.bin 1           # случайные начальные веса новой сети (пока она не обучена)
./net_fighter psmoke net_fighter_p.bin 1000       # бои новой сети против бота: решения и отказы исполнителя (должно быть 0)
./net_fighter check                               # пустая политика = бот; повтор записанных примитивов бота = тот же бой
./run_ptrain.sh net_runs/p1 1200 512              # обучение новой сети: копирование бота, затем PPO (несколько часов; продолжает с места)
./net_fighter peval net_runs/p1.bin.best.bin 4000 # лучшая версия против бота на отложенных боях
./net_fighter pfair net_runs/p1.bin.best.bin 3000 # честная проверка: та же пара боссов, сеть за каждого из двух, ИНТ 16 у обоих
./net_fighter preplay net_runs/p1.bin.best.bin 1600 42 boi.html   # бой сети в просмотрщик: её шаги, её туман войны
cp net_runs/p1.bin.best.bin net_fighter_p.bin     # поставить обученную сеть в игру (ИНТ >= 12 и соперник «нейросеть»)
```
Число потоков: переменная `DND_THREADS` (по умолчанию ядра − 2: ноутбук остаётся отзывчивым). Результат от числа потоков не зависит.

## Правила разработки
- Любая смена правил сборки обнуляет обучение Сборщиков и ботов; эталон `T126` (бой побитово как v20) тогда перезаписывается.
- Перед правкой производительности или уборкой снимайте эталон: `./tests/golden/make_golden.sh`, после — `./tests/golden/make_golden.sh check` (всё должно совпасть).
