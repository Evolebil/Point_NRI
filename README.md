# DND_PRO: Арена Боссов

Босс собирается на бюджет очков из «атомов» (механик); цены атомов подбирает симуляция так, чтобы владелец атома
выигрывал ~50% боёв. Игрок собирает своего персонажа в браузере и бьётся с боссом, которого собирает сеть-Сборщик, а
ведёт сеть-боец или правило-бот.

## Папки
| Папка | Что там |
|---|---|
| `src/engine/` | движок: `engine.h` подключает части по порядку — `core` (случайные числа, политика бота, типы урона) → `atoms` (атомы и цены `AD[]`, лестницы `TD[]`) → `spec` (способность и её цена) → `gen_spec`, `gen_boss` (генератор) → `world` (бойцы, клетки, путь) → `rules` (броски, состояния, урон) → `eval` → `actions` → `turn` (примитивы хода) → `bot` (правило-бот) → `arena_map` → `chase` → `fight` → `describe` → `calib`; плюс `world_gen.h` (мир из шума), `custom_map.h`, `char_spec.h` (персонаж игрока), `human_turn.h`, `replay_export.h`, `mini_json.h`, `par.h`, `dnd_root.h` |
| `src/ai/` | сети: боец `prim_net.h` и его обучение `prim_train.h`, Сборщик `builder_net.h`, общий `gru.h` |
| `src/apps/` | программы: `arena.cc` (калибровка, просмотр боя, лестница), `arena_play.cc` (движок игры для сервера), `net_fighter.cc`, `net_builder.cc`, `tiers.cc` (лестницы атомов) |
| `web/` | игра в браузере: сервер `play_server.py`; страница `play.html` + `css/play.css` + `js/*.js` (сервер склеивает их в один ответ); `viewer_template.html` — просмотр боя |
| `relay/` | связь через Cloudflare Workers (`host.mjs`; секрет `host_key.txt` — не в git) |
| `tools/` | `calc.py` (цена персонажа), `sheet.py` (шпаргалка), `an.py` (сводка калибровки), `bake.py` (запечь цены), `apply_tiers.py` (запечь лестницы) |
| `scripts/` | `start_*.sh` — запуск игры и туннелей; `run_calib*.sh`, `run_ptrain.sh`, `run_builder.sh`, `live_replay.sh` — прогоны |
| `tests/` | `audit_tests.cc` + `audit/*.inc` (проверки по диапазонам номеров), `run_tests.sh`, `test_play.py`; `golden/` — эталон для перестроек кода |
| `config/` | `limity.json` — технические потолки и растяжки призыва |
| `models/nets/` | веса сетей игры: `net_fighter_p.bin` (боец), `builder5_A/B.bin` (Сборщики); остальное в `models/` — локальная языковая модель, не в git |
| `docs/` | правила `pravila-v21.md`, ТЗ и список работ (`TZ_backlog.md`, …), журнал `istoriya-razrabotki.md`, каталог атомов, памятка игрока, шпаргалка |
| `demo/` | примеры боёв и персонаж `rycar.json` |
| `kampaniya/` | кампания (указатель на Obsidian, карты мира) |

Не в git (создаются при работе): `build/` — программы, `state/` — игроки сервера, адрес туннеля, журнал, `results/` —
итоги прогонов (`calib/`, `tiers/`, `tests/`, `perf/`), `runs/net/` — обучение сетей, `Sushnost/` — выгрузка бестиария.

Файлы ищутся от корня проекта: из текущей папки, из `DND_ROOT` или от папки программы (`build/..`).

## Команды (из корня проекта)
```
make                                               # собрать всё в build/
make test                                          # тесты → results/tests/tests_summary.txt
python3 web/play_server.py                         # игра: http://127.0.0.1:8770/ (журнал ошибок — stderr)
./scripts/start_workers.sh                         # игра для друзей через Cloudflare Workers (ссылка в relay/worker_url.txt)
python3 tools/calc.py demo/rycar.json              # цена персонажа по файлу
python3 tools/sheet.py 800 > docs/kratko-pravila.md   # шпаргалка цен
./build/arena replay 1600 42 results/battle.html   # просмотр одного боя: бюджет, зерно, файл
./build/arena ladder 800 1600 6000 42              # лестница: насколько больший бюджет сильнее
./scripts/run_calib.sh                             # калибровка на боте: два зерна → results/calib/
./scripts/run_calib_net.sh                         # калибровка на сети и смеси со Сборщиками
python3 tools/bake.py results/calib/calibA.txt results/calib/calibB.txt   # запечь цены в src/engine/atoms.h и core.h
./build/tiers 8 1200 7 > results/tiers/tiers_fit.txt   # замер лестниц размерных атомов
python3 tools/apply_tiers.py results/tiers/tiers_fit.txt
./scripts/run_ptrain.sh runs/net/p1 1200 512       # обучение сети-бойца: копирование бота, затем PPO (часы; продолжает с места)
./build/net_fighter peval runs/net/p1.bin.best.bin 4000   # лучшая версия против бота
./build/net_fighter pfair runs/net/p1.bin.best.bin 3000   # честная проверка: та же пара боссов, сеть за каждого
./build/net_fighter preplay runs/net/p1.bin.best.bin 1600 42 results/boi.html   # бой сети в просмотрщик
./build/net_fighter check                          # повтор записанных примитивов бота = тот же бой
cp runs/net/p1.bin.best.bin models/nets/net_fighter_p.bin   # поставить обученную сеть в игру
./scripts/run_builder.sh runs/net/builder6         # обучение Сборщиков (в игру — копией в models/nets/)
```
Число потоков: переменная `DND_THREADS` (по умолчанию ядра − 2: ноутбук остаётся отзывчивым). Результат от числа потоков не зависит.

## Правила разработки
- Любая смена правил сборки обнуляет обучение Сборщиков и ботов; эталон `T126` тогда перезаписывается.
- Перед перестройкой кода снимайте эталон: `./tests/golden/make_golden.sh`, после — `./tests/golden/make_golden.sh check`
  (всё должно совпасть; около часа). Быстрая сверка за минуты: `./tests/golden/quick_check.sh` до и после, затем `diff`.
- Бои детерминированы: одно зерно — тот же бой при любом числе потоков и любом порядке боёв.
