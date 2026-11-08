// ============================================================================
// Сеть-боец v21 (src/ai/prim_net.h): ходит примитивами и видит карту; обучение — копирование бота, затем PPO (prim_train.h).
//
//   ./net_fighter pinit <файл> [зерно]                           — случайные начальные веса
//   ./net_fighter psmoke <файл> [боёв]                           — бои сети против бота: решения и отказы исполнителя (должно быть 0)
//   ./net_fighter check                                          — повтор записанных примитивов бота даёт тот же бой до бита
//   ./net_fighter pbc <боёв> <проходов> <зерно> <веса.bin> [старт] — копирование бота
//   ./net_fighter ptrain <итераций> <боёв> <зерно> <веса.bin> [старт] — PPO, продолжает с <веса>.state
//   ./net_fighter peval <веса.bin> <боёв>                        — сеть против бота на отложенных боях
//   ./net_fighter preplay <веса.bin> <бюджет> <зерно> <файл.html> — бой сети в просмотрщик: шаги сети, её туман войны
//   ./net_fighter pfair <веса.bin> <пар>                         — честная проверка: та же пара боссов, сеть за каждого из двух, ИНТ 16 у обоих
// Сборка: make (программа — build/net_fighter)
// ============================================================================
#include "engine.h"
#include <cstdio>
#include "prim_train.h"

int main(int argc, char** argv) {
    const std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "pinit") return pt::pinitMain(argc, argv);
    if (cmd == "psmoke") return pt::psmokeMain(argc, argv);
    if (cmd == "check") return pt::checkMain();
    if (cmd == "pbc") return pt::bcMain(argc, argv);
    if (cmd == "ptrain") return pt::trainMain(argc, argv);
    if (cmd == "peval") return pt::evalMain(argc, argv);
    if (cmd == "preplay") return pt::replayMain(argc, argv);
    if (cmd == "pfair") return pt::fairMain(argc, argv);
    std::fprintf(stderr, "использование: net_fighter pinit|psmoke|check|pbc|ptrain|peval|preplay|pfair (см. шапку файла)\n");
    return 1;
}
