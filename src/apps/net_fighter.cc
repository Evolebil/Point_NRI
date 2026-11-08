// ============================================================================
// Сеть-боец (Этап 6): рекуррентная нейросеть (эмбеддинг → GRU → три головы + ценность), обучение наградой (A2C с GAE, обратный проход по времени).
//
// Сеть выбирает на каждом своём ходу три решения из интерфейса движка (Ovr): стойку (9), режим цели (4) и предпочтительное действие (оружие или
// одна из 12 способностей). Остальное (куда именно идти, кому бить) делает движок. Скрытое состояние сети живёт весь бой: сеть помнит, что делала раньше.
// Награда на каждом решении: нанесённый урон по боссу врага минус полученный (в долях ХП) плюс ±1 за исход; дисконт γ и GAE дают «думать наперёд».
//
//   ./net_fighter train <итераций> <боёв в итерации> <зерно> <файл весов> [файл для продолжения]
//   ./net_fighter eval  <файл весов> <боёв> [greedy|sample]      — сеть (ИНТ 16) против правила-бота (ИНТ 10) на тех же сборках
//   ./net_fighter check                                          — «пустое» вмешательство (стойка 0, цель 0, действие по умолчанию) = обычный бот
//   ./net_fighter upgrade20 <старые.bin> <новые.bin>             — перенос весов v20 в формат v21 (новые признаки и стойка — с нулевыми весами)
//   ./net_fighter pinit <файл> [зерно]                           — случайные начальные веса новой сети (prim_net.h, примитивы и карта)
//   ./net_fighter psmoke <файл> [боёв]                           — бои новой сети против бота: решения и отказы исполнителя (должно быть 0)
//   ./net_fighter pbc <боёв> <проходов> <зерно> <веса.bin> [старт] — новая сеть: копирование бота (prim_train.h)
//   ./net_fighter ptrain <итераций> <боёв> <зерно> <веса.bin> [старт] — новая сеть: PPO, продолжает с <веса>.state
//   ./net_fighter peval <веса.bin> <боёв>                        — новая сеть против бота на отложенных боях
//   ./net_fighter preplay <веса.bin> <бюджет> <зерно> <файл.html> — бой новой сети в просмотрщик: шаги сети, её туман войны
//   ./net_fighter pfair <веса.bin> <пар>                         — честная проверка: та же пара боссов, сеть за каждого из двух, ИНТ 16 у обоих
// Сборка: g++ -std=c++17 -O2 -pthread -o net_fighter net_fighter.cc
// ============================================================================
#define main boss_main
#include "boss_arena_v20.cc"
#undef main
#include <cstdio>
#include <fstream>
#include "gru.h"
#include "par.h"
#include "fighter_train.h"
#include "prim_train.h"   // v21: обучение новой сети (копирование бота, PPO)

int main(int argc, char** argv) {
    if (const char* e = std::getenv("DND_TIMEPEN")) g_netTimePen = std::atof(e);
    cmb::g_capDraw = std::getenv("DND_CAPDRAW") != nullptr;
    g_rlTurn = netTurn;   // хук хода: бойцы под управлением сети (флаг rl) ходят через netTurn
    if (argc > 1 && std::string(argv[1]) == "train") return cmb::trainMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "eval") return cmb::evalMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "check") return cmb::checkMain();
    if (argc > 1 && std::string(argv[1]) == "upgrade20") return cmb::upgrade20Main(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "pinit") return cmb::pinitMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "psmoke") return cmb::psmokeMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "pbc") return pt::bcMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "ptrain") return pt::trainMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "peval") return pt::evalMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "preplay") return pt::replayMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "pfair") return pt::fairMain(argc, argv);
    std::fprintf(stderr, "использование: net_fighter train|eval|check (см. шапку файла)\n");
    return 1;
}
