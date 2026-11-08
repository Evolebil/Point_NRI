// ============================================================================
// Автокалибровка лестниц размерных атомов (бывший режим «builder tiers»; линейный Сборщик удалён 8 ноября — его заменила
// сеть-Сборщик net_builder).
//   ./tiers <итераций> <проб на ячейку> <зерно>   → подгонка печатается; перенос в движок: python3 tools/apply_tiers.py
// Сборка: make (программа — build/tiers)
// ============================================================================
#include "engine.h"
#include <array>
#include <cstdio>
#include <vector>

static Coef coefNow() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    return C;
}
// ============================================================================
// Блок 1б: автокалибровка лестниц размерных атомов (./builder tiers <итераций> <проб на ячейку> <зерно>).
// Для каждого атома берутся корзины значений (внутри диапазона генератора и за ним) и базовое значение n0. Две сборки
// на одном зерне: босс со значением n и босс со значением n0 (каждый платит за своё, остальной бюджет достаётся
// остальному). Если первый выигрывает >50%, значение n недооценено на этом бюджете. Цель для кумулятивной цены g(B, n)
// подтягивается к 50%, затем подбираются все шесть чисел лестницы (a, b, k2, s3, c, e): пороги a и b растут с бюджетом
// как (B/1600)^e.
// ============================================================================
struct TierProbe {
    int tier;
    std::vector<int> bins;
    int n0;
};
static const TierProbe PROBES[TR_N] = {
    {TR_RANGE, {12, 18, 23, 30, 40, 55, 70, 99}, 6}, {TR_AREA, {4, 5, 6, 8, 10, 14, 20, 30}, 3},
    {TR_PUSH, {4, 5, 6, 8, 12, 20, 30, 60}, 3},      {TR_TELE, {4, 5, 6, 8, 12, 20, 30, 50}, 3},
    {TR_SPEED, {7, 8, 10, 12, 16, 24, 40}, 6},   // v21: скорость с нуля, ступень 5 фт, база 30 фт          {TR_AURARAD, {3, 4, 5, 6, 9, 14, 20, 30}, 2},
    {TR_AURADICE, {3, 4, 5, 8, 12, 20, 30}, 2},      {TR_DMG, {6, 8, 10, 12, 16, 22, 30, 45, 60}, 5},
    {TR_TEMP, {4, 5, 6, 8, 12, 20, 30, 50}, 3},      {TR_ACB, {3, 4, 5, 6, 8, 10, 14}, 2},
    {TR_ATT, {5, 9, 14, 19, 29, 34, 44, 59, 89}, 3},
    {TR_NABIL, {4, 6, 8, 12, 16, 24, 36}, 2}};
struct Cell {
    double w = 0;
    int n = 0, skip = 0;
};
// один кусок проб (однопоточно: параллелит вызывающий)
static Cell measureChunk(const Coef& C, long long B, int tier, int n, int n0, int i0, int i1, uint64_t seed) {
    Cell c;
    for (int i = i0; i < i1; i++) {
        uint64_t sd = mix(seed, i);
        rng.seed(sd);
        g_probe = Probe{tier, (double)n, false, false};
        Boss A = makeBoss(B, C, false, false);
        bool fa = g_probe.fail;
        rng.seed(sd);
        g_probe = Probe{tier, (double)n0, false, false};
        Boss Bb = makeBoss(B, C, false, false);
        bool fb = g_probe.fail;
        g_probe = Probe();
        if (fa || fb) {
            c.skip++;
            continue;
        }
        int r1 = fight(A, Bb), r2 = fight(Bb, A);
        c.w += ((r1 == 0 ? 1.0 : (r1 == 2 ? 0.5 : 0.0)) + (r2 == 1 ? 1.0 : (r2 == 2 ? 0.5 : 0.0))) / 2;
        c.n++;
    }
    return c;
}
struct Pt {
    double B, n, target, wt;
};
// возвращает множитель единицы u (его вносят в коэффициенты атомов лестницы)
static double fitTier(int t, const std::vector<Pt>& pts) {
    TierDef& d = TD[t];
    double ubest = 1.0, ucur = 1.0;
    auto obj = [&](const TierDef& x, double u) {
        TierDef sv = d;
        d = x;
        double f = 0, gb = g_B;
        for (auto& p : pts) {
            g_B = p.B;
            double g = std::max(1e-9, u * tierN(t, p.n));
            double e = std::log(g) - std::log(p.target);
            f += p.wt * e * e;
        }
        g_B = gb;
        d = sv;
        f += 0.02 * (std::pow(std::log(x.b / sv.b), 2) + std::pow(std::log(x.k2 / sv.k2), 2) +
                     std::pow(std::log(x.s3 / sv.s3), 2) + std::pow(std::log(x.c / sv.c), 2) +
                     std::pow(std::log(x.a / sv.a), 2) + std::pow(x.e - sv.e, 2) + std::pow(x.q - sv.q, 2) +
                     std::pow(std::log(u), 2));
        return f;
    };
    auto clampX = [&](TierDef& x) {
        x.a = std::clamp(x.a, 1.0, 120.0);
        x.k2 = std::clamp(x.k2, 0.02, 4.0);
        x.b = std::clamp(x.b, std::max(x.a + 1, x.a + 0.5 / x.k2), x.a + 120);
        // после b рост цены ступени замедляется: s3 <= P (и s3 >= 1.5)
        double P = 1 + x.k2 * (x.b - x.a);
        x.s3 = std::clamp(x.s3, 1.5, std::min(80.0, P));
        x.c = std::clamp(x.c, 1.0, 90.0);
        x.e = std::clamp(x.e, -3.0, 3.0);
        x.q = std::clamp(x.q, -2.0, 2.5);
    };
    TierDef best = d;
    double bf = obj(best, ubest);
    uint64_t st = 12345 + t;
    (void)ucur;
    for (int round = 0; round < 6000; round++) {
        st = mix(st, round);
        double sc = 0.35 * std::pow(0.9988, round);
        auto u = [&]() {
            st = mix(st, 1);
            return ((st >> 11) * (1.0 / 9007199254740992.0)) * 2 - 1;
        };
        TierDef x = best;
        double ba = best.b - best.a;
        x.a = best.a * std::exp(sc * u());
        ba *= std::exp(sc * u());
        x.b = x.a + ba;
        x.k2 = best.k2 * std::exp(sc * u());
        x.s3 = best.s3 * std::exp(sc * u());
        x.c = best.c * std::exp(sc * u());
        x.e = best.e + 0.5 * sc * u();
        x.q = best.q + 0.5 * sc * u();
        clampX(x);
        double nu = std::clamp(ubest * std::exp(sc * u()), 0.05, 20.0);
        double f = obj(x, nu);
        if (f < bf) {
            bf = f;
            best = x;
            ubest = nu;
        }
    }
    d = best;
    return ubest;
}
static std::vector<int> tierAtoms(int t) {
    switch (t) {
        case TR_RANGE:
            return {AT_ARANGE, AT_WPNRANGE};
        case TR_AREA:
            return {AT_AREA};
        case TR_PUSH:
            return {AT_PUSH};
        case TR_TELE:
            return {AT_TELE};
        case TR_SPEED:
            return {AT_SPEED};
        case TR_AURARAD:
            return {AT_AURARAD};
        case TR_AURADICE:
            return {AT_AURA};
        case TR_DMG:
            return {AT_DMG};
        case TR_TEMP:
            return {AT_TEMP};
        case TR_ACB:
            return {AT_ACB};
        case TR_NABIL:
            return {AT_NABIL};
        default:
            return {AT_WPNATT};
    }
}
static int tiersMain(int argc, char** argv) {
    int iters = argc > 2 ? std::atoi(argv[2]) : 4, samples = argc > 3 ? std::atoi(argv[3]) : 1500;
    uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 5;
    const long long BU[5] = {400, 800, 1600, 3200, 6400};
    Coef C = coefNow();
    const Coef C0 = C;
    const double kappa = 4.0;
    TierDef before[TR_N];
    for (int t = 0; t < TR_N; t++) before[t] = TD[t];
    // среднее |винрейт − 50%| по ячейкам: было / стало
    std::vector<double> dev0(TR_N, 0), devN(TR_N, 0);
    for (int it = 0; it <= iters; it++) {
        std::printf("=== Замер %d%s ===\n", it, it == 0 ? " (до подгонки)" : (it == iters ? " (итоговый)" : ""));
        // весь замер итерации — одним пулом задач (лестница × корзина × бюджет × кусок проб)
        std::vector<std::array<int, 3>> keys;
        for (int t = 0; t < TR_N; t++)
            for (size_t j = 0; j < PROBES[t].bins.size(); j++)
                for (int bi = 0; bi < 5; bi++) keys.push_back({t, (int)j, bi});
        // ранние итерации грубее, две последние — полные
        const int smp = (it < iters - 1) ? std::max(300, samples / 2) : samples;
        const int CHK = 12, per = (smp + CHK - 1) / CHK;
        std::vector<Cell> part(keys.size() * CHK);
        parallelFor((int)part.size(), [&](int k) {
            auto& ky = keys[k / CHK];
            int ch = k % CHK, i0 = ch * per, i1 = std::min(smp, i0 + per);
            if (i0 < i1)
                part[k] = measureChunk(C, BU[ky[2]], ky[0], PROBES[ky[0]].bins[ky[1]], PROBES[ky[0]].n0, i0, i1,
                                       mix(seed + it * 17, ky[0] * 100 + ky[1] * 10 + ky[2]));
        });
        auto cellOf = [&](int t, int j, int bi) {
            int base = 0;
            for (size_t q = 0; q < keys.size(); q++)
                if (keys[q][0] == t && keys[q][1] == j && keys[q][2] == bi) {
                    base = (int)q;
                    break;
                }
            Cell c;
            for (int ch = 0; ch < CHK; ch++) {
                const Cell& x = part[base * CHK + ch];
                c.w += x.w;
                c.n += x.n;
                c.skip += x.skip;
            }
            if (c.n) c.w /= c.n;
            return c;
        };
        std::vector<std::vector<Pt>> allPts(TR_N);
        for (int t = 0; t < TR_N; t++) {
            const TierProbe& P = PROBES[t];
            std::vector<Pt> pts;
            double dsum = 0;
            int dn = 0;
            std::printf("%s  [a=%.1f b=%.1f k2=%.2f s3=%.2f c=%.1f e=%.2f q=%.2f]\n", TD[t].name, TD[t].a, TD[t].b,
                        TD[t].k2, TD[t].s3, TD[t].c, TD[t].e, TD[t].q);
            for (size_t j = 0; j < P.bins.size(); j++) {
                int n = P.bins[j];
                std::string line;
                for (int bi = 0; bi < 5; bi++) {
                    Cell c = cellOf(t, (int)j, bi);
                    char b[60];
                    if (c.n < 200)
                        std::snprintf(b, sizeof b, "   -  ");
                    else
                        std::snprintf(b, sizeof b, " %4.1f%%", 100 * c.w);
                    line += b;
                    if (c.n >= 200) {
                        dsum += std::fabs(c.w - 0.5) * c.n;
                        dn += c.n;
                        double gb = g_B;
                        g_B = (double)BU[bi];
                        double g = tierN(t, n);
                        g_B = gb;
                        pts.push_back({(double)BU[bi], (double)n, g * std::clamp(1 + kappa * (c.w - 0.5), 0.4, 3.0),
                                       std::min(1.0, c.n / 1500.0)});
                    }
                }
                std::printf("   n=%-3d бюджеты 400/800/1600/3200/6400:%s\n", n, line.c_str());
            }
            double dev = dn ? dsum / dn : 0;
            if (it == 0) dev0[t] = dev;
            if (it == iters) devN[t] = dev;
            std::printf("   среднее отклонение от 50%%: %.1f п.п.\n", 100 * dev);
            allPts[t] = pts;
        }
        // подгонка лестниц независима: по ядру на лестницу
        if (it < iters) {
            std::vector<double> us(TR_N, 1.0);
            parallelFor(TR_N, [&](int t) {
                if (allPts[t].size() >= 8) us[t] = fitTier(t, allPts[t]);
            });
            for (int t = 0; t < TR_N; t++) {
                for (int a : tierAtoms(t)) C[a] *= us[t];
                std::printf("%s: единица цены ×%.3f\n", TD[t].name, us[t]);
            }
        }
        std::fflush(stdout);
    }
    std::printf("\n=== Было → стало ===\n");
    std::ostringstream blk;
    blk << "TierDef TD[TR_N] = {\n";
    for (int t = 0; t < TR_N; t++) {
        std::printf("%-52s a %.1f→%.1f  b %.1f→%.1f  k2 %.2f→%.2f  s3 %.2f→%.2f  c %.1f→%.1f  e %.2f→%.2f  q %.2f→%.2f "
                    "| отклонение %.1f→%.1f п.п.\n",
                    TD[t].name, before[t].a, TD[t].a, before[t].b, TD[t].b, before[t].k2, TD[t].k2, before[t].s3,
                    TD[t].s3, before[t].c, TD[t].c, before[t].e, TD[t].e, before[t].q, TD[t].q, 100 * dev0[t],
                    100 * devN[t]);
        char b[300];
        std::snprintf(b, sizeof b, "    {\"%s\", %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f}%s\n", TD[t].name, TD[t].a,
                      TD[t].b, TD[t].k2, TD[t].s3, TD[t].c, TD[t].e, TD[t].q, t + 1 < TR_N ? "," : "};");
        blk << b;
    }
    for (int t = 0; t < TR_N; t++)
        for (int a : tierAtoms(t)) {
            char b[200];
            std::snprintf(b, sizeof b, "// SCALE %d %.5f  %s\n", a, C[a] / C0[a], AD[a].name);
            blk << b;
            std::printf("коэффициент «%s»: %.4g → %.4g\n", AD[a].name, C0[a], C[a]);
        }
    std::ofstream f("tiers_fit.txt");
    f << blk.str();
    std::printf("\nновый блок TD[] записан в tiers_fit.txt\n");
    return 0;
}


int main(int argc, char** argv) {
    // прежний вызов «./builder tiers …»: аргументы с тем же сдвигом
    std::vector<char*> a(argv, argv + argc);
    a.insert(a.begin() + 1, (char*)"tiers");
    return tiersMain((int)a.size(), a.data());
}
