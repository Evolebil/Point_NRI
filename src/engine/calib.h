// Калибровка на смеси: боссы от сетей-Сборщиков
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

#include "replay_export.h"
#include "par.h"
#include "builder_net.h"   // этап 5б: калибровка на смеси — часть боссов собирают сети-Сборщики

// Калибровка на смеси (этап 5б, решение владельца): доля пар DND_CALIB_BLD (по умолчанию 0 — только генератор), где
// боссов собирают сети-Сборщики (веса DND_CALIB_BLDA и DND_CALIB_BLDB, по умолчанию models/nets/builder_A/B.bin). Из
// таких пар треть — Сборщик против Сборщика (A против B), две трети — Сборщик (A или B) против генератора
struct CalibBld {
    bld::BNet net[2];
    bool ok = false;
    double share = 0;
} g_cb;
void loadCalibBld() {
    const char* e = std::getenv("DND_CALIB_BLD");
    g_cb.share = e ? std::clamp(std::atof(e), 0.0, 1.0) : 0.0;
    if (g_cb.share <= 0) return;
    const char* pa = std::getenv("DND_CALIB_BLDA");
    const char* pb = std::getenv("DND_CALIB_BLDB");
    const std::string fa = pa ? pa : dndPath("models/nets/builder_A.bin"), fb = pb ? pb : dndPath("models/nets/builder_B.bin");
    g_cb.ok = bld::loadNet(fa, g_cb.net[0]) && bld::loadNet(fb, g_cb.net[1]);
    if (!g_cb.ok) {
        std::fprintf(stderr, "ВНИМАНИЕ: веса Сборщиков %s / %s не загрузились — калибровка только на генераторе\n", fa.c_str(), fb.c_str());
        g_cb.share = 0;
    }
}
// босс от сети-Сборщика k (0 — A, 1 — B)
Boss makeBossBld(long long B, const Coef& C, int k) {
    bld::BrainNet br(&g_cb.net[k], nullptr);
    g_bb = &br;
    Boss b = makeBoss(B, C, true, false);
    g_bb = nullptr;
    return b;
}
// пара для калибровки: kind 0 — генератор против генератора (makePair), 1 — Сборщик против генератора, 2 — Сборщик A против
// Сборщика B; side — какая сторона у Сборщика (для kind 1)
int makeCalibPair(long long B, const Coef& C, Boss& A, Boss& Bo, int& side) {
    side = -1;
    if (g_cb.share <= 0 || rand01() >= g_cb.share) {
        makePair(B, C, A, Bo);
        return 0;
    }
    const int k = irand(3);
    if (k == 2) {
        A = makeBossBld(B, C, 0);
        Bo = makeBossBld(B, C, 1);
        return 2;
    }
    side = irand(2);
    Boss nb = makeBossBld(B, C, k), gb = makeBoss(B, C, true, false);
    A = side == 0 ? std::move(nb) : std::move(gb);
    Bo = side == 0 ? std::move(gb) : std::move(nb);
    return 1;
}
