// ============================================================================
// ./arena: калибровка цен (по умолчанию), просмотр одного боя (replay), лестница бюджетов (ladder).
// Запуск:  ./build/arena [бюджет, 0=мульти] [боёв всего] [итераций] [зерно] [целевая_длина=14] [лимит_раундов=60]
//          ./build/arena replay <бюджет> <зерно> <файл.html>   — один бой с реплеем
//          ./build/arena ladder <бюджет1> <бюджет2> <боёв> <зерно>
// Сборка: make (программа — build/arena)
// ============================================================================
#include "engine.h"

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);   // построчно: прогресс калибровки виден в файле сразу
    if (argc > 1 && std::string(argv[1]) == "replay") {
        long long budget = argc > 2 ? std::stoll(argv[2]) : 1600;
        unsigned seed = argc > 3 ? (unsigned)std::stoul(argv[3]) : (unsigned)std::time(nullptr);
        std::string outPath = argc > 4 ? argv[4] : "battle_replay.html";
        // на огромных бюджетах (гигантские ХП) может понадобиться больше 60
        if (argc > 5) g_maxRounds = std::stoi(argv[5]);
        rng.seed(seed);
        // ВНИМАНИЕ: стартовые цены "на глаз", не откалиброванные — для просмотра боя это не важно
        Coef C;
        for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
        Boss A, B;
        makePair(budget, C, A, B);
        std::printf("=== Босс A ===\n%s\n=== Босс B ===\n%s\n", describeBoss(A).c_str(), describeBoss(B).c_str());
        std::vector<std::string> frames;
        std::string mapJson;
        int res = fight(A, B, &frames, &mapJson);
        std::printf("Победитель: %s\n", res == 0 ? "Босс A" : (res == 1 ? "Босс B" : "ничья"));
        writeViewerHTML(outPath, A, B, C, (double)budget, "Случайный генератор", "Случайный генератор", seed, mapJson, frames);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "ladder") {
        // Бюджетная лестница: как часто босс большего бюджета побеждает босса меньшего (случайные сборки, один из двух
        // обязательно с миньонами).
        // ./build/arena ladder <малый=800> <большой=1600> [боёв=6000] [зерно=42]   цель плана v19: 800 против 1600 → 75–80%
        long long lo = argc > 2 ? std::stoll(argv[2]) : 800, hi = argc > 3 ? std::stoll(argv[3]) : 1600;
        int n = argc > 4 ? std::stoi(argv[4]) : 6000;
        unsigned sd = argc > 5 ? (unsigned)std::stoul(argv[5]) : 42;
        rng.seed(sd);
        Coef C;
        for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
        double w = 0;
        g_rounds = 0;
        g_capped = 0;
        for (int i = 0; i < n; i++) {
            int fs = irand(2);
            Boss s = makeBoss(lo, C, true, fs == 0), l = makeBoss(hi, C, true, fs == 1);
            // стороны меняются
            int r = (i % 2 == 0) ? fight(l, s) : fight(s, l);
            w += r == 2 ? 0.5 : (((i % 2 == 0) == (r == 0)) ? 1.0 : 0.0);
        }
        double p = w / n;
        std::printf("Бюджет %lld против %lld: больший выигрывает %.1f%% (±%.1f п.п., %d боёв); средняя длина боя %.1f "
                    "р., в лимит %.1f%%\n",
                    lo, hi, 100 * p, 100 * 2 * std::sqrt(p * (1 - p) / n), n, (double)g_rounds / n,
                    100.0 * g_capped / n);
        std::printf("Погонь на бой %.3f, побегов %.3f, истощений %.3f, осложнений %.3f\n", (double)g_chases / n, (double)g_escapes / n,
                    (double)g_exhaustions / n, (double)g_complications / n);
        return 0;
    }
    long long budgetArg = 0;
    int fights = 14000, iters = 30;
    unsigned seed = 42;
    double targetLen = 14.0;
    if (argc > 1) budgetArg = std::stoll(argv[1]);
    if (argc > 2) fights = std::stoi(argv[2]);
    if (argc > 3) iters = std::stoi(argv[3]);
    if (argc > 4) seed = (unsigned)std::stoul(argv[4]);
    if (argc > 5) targetLen = std::stod(argv[5]);
    if (argc > 6) g_maxRounds = std::stoi(argv[6]);
    rng.seed(seed);
    loadCalibBld();
    if (g_cb.share > 0) std::printf("калибровка на смеси: %.0f%% пар с боссами сетей-Сборщиков\n", 100 * g_cb.share);
    std::vector<long long> BL =
        budgetArg > 0 ? std::vector<long long>{budgetArg} : std::vector<long long>{200, 400, 800, 1600, 3200, 6400};
    int nB = (int)BL.size(), perB = std::max(1, fights / nB);

    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    Coef start = C;
    std::printf("=== Арена Боссов v6: ОДНА таблица цен на все бюджеты ===\nБюджеты:");
    for (auto b : BL) std::printf(" %lld", b);
    std::printf(" | боёв на бюджет %d, итераций %d, зерно %u, целевая длина боя %.0f р.\n\n", perB, iters, seed,
                targetLen);

    struct PassRes {
        std::array<double, AT_N> wr{}, wlo{}, whi{};
        std::array<int, AT_N> gm{}, nv{};
        double len = 0, sumDiff = 0, cap = 0;
        int sumValid = 0;
        std::vector<double> lenB;
        double slope = 0;
        int slopeN = 0;
        std::vector<double> dB;
        std::array<double, AT_N> use{}, useDen{};   // метрики использования (№6): доля или применений за бой; den — база
        double bldW = 0;   // этап 5б: очки Сборщика против генератора (доля) и число таких пар, пар Сборщик–Сборщик
        int bldN = 0, bldBB = 0;
    };
    int passId = 0;
    // Прогон делится на задачи (бюджет × кусок боёв). У каждой задачи своё зерно из (зерно, номер прогона, бюджет,
    // кусок), а результаты складываются в фиксированном порядке: итог не зависит ни от числа потоков, ни от их
    // расписания.
    auto pass = [&](const Coef& Cc, int per) {
        PassRes R;
        R.wlo.fill(2.0);
        R.whi.fill(-1.0);
        for (int t = 0; t < DT_N; t++) g_dmgT[t] = 0;
        g_dmgNM = 0;
        const int CH = 16, nT = nB * CH, pid = ++passId;
        struct TaskRes {
            std::array<double, AT_N> wins{};
            std::array<int, AT_N> games{};
            double nw = 0, sw = 0;
            int ng = 0, sg = 0;
            long long rounds = 0, capped = 0;
            double dmgT[DT_N] = {};
            double dmgNM = 0;
            long long st[10] = {};
            std::array<double, AT_N> useNum{}, useDen{};   // метрики использования по владельцам атома (№6)
            double bldW = 0;   // очки Сборщика в парах «Сборщик против генератора» (этап 5б)
            int bldN = 0, bldBB = 0;
        };
        std::vector<TaskRes> tr(nT);
        std::atomic<int> nextTask(0);
        long long base[10] = {g_lrUsed,  g_rechUsed, g_rechRolls,  g_rechOk, g_attacksMade,
                              g_legUsed, g_concCast, g_concBroken, g_surge,  g_ripUsed};
        auto worker = [&]() {
            // этап 4 ТЗ (решение владельца): в калибровке всеми боссами ходит обученная сеть, если она загружена
            // (ИНТ — только игровая характеристика); без сети (DND_NETP=off) — бот, для сравнения цен
            g_forceNetMask = pn::primNet() ? 3 : 0;
            long long* cn[10] = {&g_lrUsed,  &g_rechUsed, &g_rechRolls,  &g_rechOk, &g_attacksMade,
                                 &g_legUsed, &g_concCast, &g_concBroken, &g_surge,  &g_ripUsed};
            for (;;) {
                int t = nextTask++;
                if (t >= nT) break;
                int bi = t / CH, ch = t % CH, n = per / CH + (ch < per % CH ? 1 : 0);
                TaskRes& T = tr[t];
                rng.seed((((uint64_t)seed * 1315423911ULL + (uint64_t)pid) * 2654435761ULL + (uint64_t)bi) * 40503ULL +
                         (uint64_t)ch);
                for (int k = 0; k < DT_N; k++) g_dmgT[k] = 0;
                g_dmgNM = 0;
                g_track = true;
                g_rounds = 0;
                g_capped = 0;
                long long s0[10];
                for (int k = 0; k < 10; k++) s0[k] = *cn[k];
                for (int i = 0; i < n; i++) {
                    Boss A, B;
                    int bside = -1;
                    const int kind = makeCalibPair(BL[bi], Cc, A, B, bside);
                    int res = fight(A, B);
                    if (kind == 1) {
                        T.bldW += res == 2 ? 0.5 : (res == bside ? 1.0 : 0.0);
                        T.bldN++;
                    } else if (kind == 2)
                        T.bldBB++;
                    for (int side = 0; side < 2; side++) {   // метрики использования (№6) — у владельцев атома
                        const Boss& bp = side == 0 ? A : B;
                        const long long* u = g_use[side];
                        auto addU = [&](int a, double num, double den) {
                            if (bp.at(a)) T.useNum[a] += num, T.useDen[a] += den;
                        };
                        addU(AT_WPNRANGE, (double)u[U_WOUT], (double)u[U_WATK]);
                        addU(AT_ARANGE, (double)u[U_SOUT], (double)u[U_SATK]);
                        addU(AT_SPEED, (double)u[U_FTAWAY], (double)u[U_FT]);
                        addU(AT_HIDE, (double)u[U_HIDE], 1.0);
                        addU(AT_INVIS, (double)u[U_INVIS], 1.0);
                        addU(AT_DARK, (double)u[U_DARK], 1.0);
                        // прочие атомы способностей: применений за бой у тех владельцев, у кого атом — в способности
                        AMask sm = 0;
                        for (auto& sp : bp.abil) markSpec(sp, sm);
                        for (int a = 0; a < AT_N; a++)
                            if (((sm >> a) & ONE) && a != AT_ARANGE && a != AT_HIDE && a != AT_INVIS && a != AT_DARK)
                                T.useNum[a] += (double)g_castAtom[side][a], T.useDen[a] += 1.0;
                    }
                    double sa = res == 0 ? 1.0 : (res == 2 ? 0.5 : 0.0);
                    for (int side = 0; side < 2; side++) {
                        const Boss& bp = side == 0 ? A : B;
                        double sc = side == 0 ? sa : 1.0 - sa;
                        if (bp.at(AT_SUMMON)) {
                            T.sw += sc;
                            T.sg++;
                        } else {
                            T.nw += sc;
                            T.ng++;
                        }
                        for (int a = 0; a < AT_N; a++)
                            if (bp.at(a)) {
                                T.wins[a] += sc;
                                T.games[a]++;
                            }
                    }
                }
                g_track = false;
                T.rounds = g_rounds;
                T.capped = g_capped;
                for (int k = 0; k < DT_N; k++) T.dmgT[k] = g_dmgT[k];
                T.dmgNM = g_dmgNM;
                for (int k = 0; k < 10; k++) T.st[k] = *cn[k] - s0[k];
            }
        };
        {
            const char* e = std::getenv("DND_THREADS");
            int nth = e ? std::max(1, std::atoi(e)) : std::max(1, (int)std::thread::hardware_concurrency() - 2);   // ТЗ v21: по умолчанию ядра − 2
            nth = std::min(nth, nT);
            std::vector<std::thread> th;
            for (int k = 1; k < nth; k++) th.emplace_back(worker);
            worker();
            for (auto& x : th) x.join();
        }
        long long* mc[10] = {&g_lrUsed,  &g_rechUsed, &g_rechRolls,  &g_rechOk, &g_attacksMade,
                             &g_legUsed, &g_concCast, &g_concBroken, &g_surge,  &g_ripUsed};
        // главный поток тоже брал задачи: возвращаем его счётчики и суммируем всё из результатов задач
        for (int k = 0; k < 10; k++) *mc[k] = base[k];
        for (int k = 0; k < DT_N; k++) g_dmgT[k] = 0;
        g_dmgNM = 0;
        std::array<double, AT_N> wsum{};
        std::vector<std::pair<double, double>> dpts;
        double diffSum = 0, lenSum = 0;
        int diffN = 0;
        long long capTot = 0, fTot = 0;
        for (int bi = 0; bi < nB; bi++) {
            std::array<double, AT_N> wins{};
            std::array<int, AT_N> games{};
            double nw = 0, sw = 0;
            int ng = 0, sg = 0;
            long long rounds = 0, capped = 0;
            for (int ch = 0; ch < CH; ch++) {
                const TaskRes& T = tr[bi * CH + ch];
                R.bldW += T.bldW;
                R.bldN += T.bldN;
                R.bldBB += T.bldBB;
                for (int a = 0; a < AT_N; a++) R.use[a] += T.useNum[a], R.useDen[a] += T.useDen[a];
                for (int a = 0; a < AT_N; a++) {
                    wins[a] += T.wins[a];
                    games[a] += T.games[a];
                }
                nw += T.nw;
                sw += T.sw;
                ng += T.ng;
                sg += T.sg;
                rounds += T.rounds;
                capped += T.capped;
                for (int k = 0; k < DT_N; k++) g_dmgT[k] += T.dmgT[k];
                g_dmgNM += T.dmgNM;
                for (int k = 0; k < 10; k++) *mc[k] += T.st[k];
            }
            double len = (double)rounds / per;
            R.lenB.push_back(len);
            lenSum += len;
            capTot += capped;
            fTot += per;
            for (int a = 0; a < AT_N; a++) {
                R.gm[a] += games[a];
                if (games[a] >= 100) {
                    double w = wins[a] / games[a];
                    wsum[a] += w;
                    R.nv[a]++;
                    R.wlo[a] = std::min(R.wlo[a], w);
                    R.whi[a] = std::max(R.whi[a], w);
                }
            }
            if (sg >= 100 && ng >= 100) {
                diffSum += sw / sg - nw / ng;
                diffN++;
                dpts.push_back({std::log2(BL[bi] / 1600.0), sw / sg - nw / ng});
            }
        }
        for (int a = 0; a < AT_N; a++) R.wr[a] = R.nv[a] ? wsum[a] / R.nv[a] : 0.5;
        // наклон «винрейт призывателя минус винрейт остальных» по бюджетам (на октаву бюджета)
        if (dpts.size() >= 3) {
            double mx = 0, my = 0;
            for (auto& p : dpts) {
                mx += p.first;
                my += p.second;
            }
            mx /= dpts.size();
            my /= dpts.size();
            double sxx = 0, sxy = 0;
            for (auto& p : dpts) {
                sxx += (p.first - mx) * (p.first - mx);
                sxy += (p.first - mx) * (p.second - my);
            }
            if (sxx > 0) {
                R.slope = sxy / sxx;
                R.slopeN = (int)dpts.size();
            }
            for (auto& p : dpts) R.dB.push_back(p.second);
        }
        for (int a = 0; a < AT_N; a++) R.use[a] = R.useDen[a] > 0 ? R.use[a] / R.useDen[a] : 0.0;
        R.len = lenSum / nB;
        R.sumDiff = diffN ? diffSum / diffN : 0;
        R.sumValid = diffN;
        R.cap = 100.0 * capTot / fTot;
        if (R.bldN > 0) R.bldW /= R.bldN;
        return R;
    };

    // ТЗ v21, №6 (пороги — решение владельца): атомом «почти не пользуются», если дальность (доля атак из-за пределов
    // досягаемости цели) и скорость (доля пройденного не на сближение) меньше 5%, а скрытность, невидимость, тьма —
    // меньше 0.1 применения за бой у владельцев. Таким атомам калибровка цену не снижает (только повышает)
    // и (решение владельца 2026-11-01: калибровать и по используемости) любой атом способности — меньше 0.1 применения за
    // бой у владельцев, у которых он в способности
    std::array<bool, AT_N> abilAtom{};
    auto useThr = [&](int a) {
        return (a == AT_WPNRANGE || a == AT_ARANGE || a == AT_SPEED) ? 0.05 : (a == AT_HIDE || a == AT_INVIS || a == AT_DARK || abilAtom[a]) ? 0.1 : -1.0;
    };
    auto useStr = [&](const PassRes& R) {
        char b[260];
        std::snprintf(b, sizeof b, "исп.: дальн. оружия %.1f%%, дальн. способн. %.1f%%, скорость %.1f%%, скрытн. %.2f, невид. %.2f, тьма %.2f за бой",
                      100 * R.use[AT_WPNRANGE], 100 * R.use[AT_ARANGE], 100 * R.use[AT_SPEED], R.use[AT_HIDE], R.use[AT_INVIS], R.use[AT_DARK]);
        return std::string(b);
    };
    std::printf("Кто ходит: %s\n", pn::primNet() ? "все боссы — обученная сеть (models/nets/net_fighter_p.bin), ИНТ только игровая характеристика" : "все — правило-бот (сети нет)");
    Coef Csum{};
    int Ccnt = 0;
    for (int it = 0; it < iters; it++) {
        PassRes R = pass(C, perB);
        for (int a = 0; a < AT_N; a++) abilAtom[a] = abilAtom[a] || (R.useDen[a] > 0 && a != AT_WPNRANGE && a != AT_SPEED);
        // цены защит следуют за реальной долей урона по типам
        {
            double tot = 0;
            for (int t = 0; t < DT_N; t++) tot += g_dmgT[t];
            if (tot > 0) {
                for (int t = 0; t < DT_N; t++) g_share[t] = 0.5 * g_share[t] + 0.5 * (g_dmgT[t] / tot);
                g_nmShare = 0.5 * g_nmShare + 0.5 * (g_dmgNM / tot);
            }
        }
        std::printf("--- Итерация %d --- длина боя по бюджетам:", it + 1);
        for (double l : R.lenB) std::printf(" %.1f", l);
        std::printf(" | в лимит %.1f%% | наклон призывателя %.3f/окт., k=%.3f", R.cap, R.slope, C[AT_MINEXP]);
        if (R.bldN > 0) std::printf(" | Сборщик против генератора %.1f%% (%d пар), Сборщик–Сборщик %d пар", 100 * R.bldW, R.bldN, R.bldBB);
        std::printf("\n");
        std::printf("    %s\n", useStr(R).c_str());
        if (it >= iters * 2 / 3) {
            for (int a = 0; a < AT_N; a++) Csum[a] += C[a];
            Ccnt++;
        }
        if (it < iters - 1) {
            for (int a = 0; a < AT_N; a++) {
                double f = 1.0;
                // показатель: слабее призыватели на больших бюджетах → цена там дешевле
                if (AD[a].mode == 5) {
                    if (R.slopeN >= 3) C[a] = std::clamp(C[a] - 2.0 * R.slope, AD[a].lo, AD[a].hi);
                    continue;
                }
                if (AD[a].mode == 1)
                    f = std::clamp(1.0 + 0.4 * (targetLen - R.len) / targetLen, 0.8, 1.25);
                else if (AD[a].mode == 3) {
                    if (R.sumValid > 0) f = std::clamp(1.0 + R.sumDiff * 1.2, 0.75, 1.3);
                } else if (AD[a].mode == 0 && R.nv[a] > 0)
                    f = std::clamp(1.0 + (R.wr[a] - 0.5) * 1.2, 0.75, 1.3);
                // возврат: чем лучше владельцы, тем МЕНЬШЕ возврат
                else if (AD[a].mode == 4 && R.nv[a] > 0)
                    f = std::clamp(1.0 - (R.wr[a] - 0.5) * 1.2, 0.75, 1.3);
                if (useThr(a) >= 0 && R.use[a] < useThr(a)) f = std::max(f, 1.0);   // №6: не пользуются — не дешевле
                C[a] = std::clamp(C[a] * f, AD[a].lo, AD[a].hi);
            }
        }
    }
    // усреднение по последней трети — гасит шум
    if (Ccnt > 0)
        for (int a = 0; a < AT_N; a++) C[a] = Csum[a] / Ccnt;
    PassRes F = pass(C, perB * 2);
    {
        std::printf("Винрейт призывателя минус остальные по бюджетам (разность, п.п.):");
        for (double d : F.dB) std::printf(" %+.1f", 100 * d);
        std::printf(" | наклон %.3f на октаву, k=%.3f\n", F.slope, C[AT_MINEXP]);
    }
    std::printf(
        "\n=== Итог (усреднённые цены, контрольный прогон x2): коэф, винрейт среднее [мин..макс по бюджетам] ===\n");
    for (int a = 0; a < AT_N; a++) {
        char us[48] = "";
        if (useThr(a) >= 0)   // используемость: доля (дальность, скорость) или применений за бой
            std::snprintf(us, sizeof us, useThr(a) < 0.08 ? "  исп. %.1f%%" : "  исп. %.2f/бой", useThr(a) < 0.08 ? 100 * F.use[a] : F.use[a]);
        std::printf("  %-52s %8.2f -> %8.2f   %5.1f%% [%5.1f..%5.1f]  n=%d%s\n", AD[a].name, start[a], C[a],
                    100 * F.wr[a], F.nv[a] ? 100 * F.wlo[a] : 0.0, F.nv[a] ? 100 * F.whi[a] : 0.0, F.gm[a], us);
    }
    std::printf("\nИспользование (№6, контрольный прогон): %s\n", useStr(F).c_str());
    for (int a = 0; a < AT_N; a++)
        if (useThr(a) >= 0 && F.use[a] < useThr(a))
            std::printf("  почти не используется: %s — цена не снижалась\n", AD[a].name);
    std::printf("\nДлина боя по бюджетам:");
    for (double l : F.lenB) std::printf(" %.1f", l);
    std::printf(" | в лимит %.1f%%\n", F.cap);
    std::printf("\nПрименения за прогон: концентрация %lld (сорвана %lld), всплеск действий %lld, контратаки %lld\n",
                g_concCast, g_concBroken, g_surge, g_ripUsed);
    std::printf("\nДоли урона по типам в боях:");
    for (int t = 0; t < DT_N; t++) std::printf(" %s %.3f;", DTN[t], g_share[t]);
    std::printf(" немагический физический %.3f\n", g_nmShare);
    std::printf("// g_share init: {");
    for (int t = 0; t < DT_N; t++) std::printf("%.3f%s", g_share[t], t + 1 < DT_N ? ", " : "}");
    std::printf("; g_nmShare = %.3f\n", g_nmShare);
    std::printf("\n// --- для вставки в AD[].init (по порядку) ---\n");
    for (int a = 0; a < AT_N; a++) std::printf("%.4f, // %s\n", C[a], AD[a].name);

    rng.seed((uint64_t)seed * 7919 + 1);
    std::printf("\n=== Примеры боссов, собранных по итоговым ценам (бюджет %lld) ===\n", BL[nB / 2]);
    for (int i = 0; i < 3; i++) {
        Boss b = makeBoss(BL[nB / 2], C, true);
        std::printf("Босс %d\n%s", i + 1, describeBoss(b).c_str());
    }
    return 0;
}
