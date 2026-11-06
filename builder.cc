// ============================================================================
// Умный Сборщик (Этап 4). Обучение с подкреплением (REINFORCE + Adam), не эволюция:
//   политика  — параметры θ решений генератора боссов (boss_arena_v20.cc, enum BPar), по одной политике на каждый
//   бюджет; награда   — исход боёв собранного босса против босса, собранного обычным случайным генератором (или «залом
//   славы» прошлых версий политики); обучение  — градиент логарифма вероятности собранных решений × (награда − средняя
//   награда пачки).
// Режимы:
//   ./builder train  <индекс бюджета 0..5> <итераций> <пачка> <зерно> [файл]    — обучить политику, сохранить
//   ./builder report <файл0> ... <файл5>                                        — отчёт о дырах: что берёт сильнейший
//   Сборщик и его винрейт
//   ./builder adv    <файл0> ... <файл5>                                        — множители цен для состязательной
//   калибровки (по частоте выбора)
// Сборка: g++ -std=c++17 -O2 -pthread -o builder builder.cc
// ============================================================================
#define main boss_main
#include "boss_arena_v20.cc"
#undef main
#include <fstream>
#include <sstream>
#include <map>
#include <functional>
#include <array>

static const long long BUDGETS[6] = {200, 400, 800, 1600, 3200, 6400};
typedef std::vector<float> Theta;
typedef std::vector<std::pair<int, float>> Trace;

static Coef coefNow() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    // состязательная настройка: множитель наценки за число способностей (DND_NABIL, по умолчанию 1)
    if (const char* e = std::getenv("DND_NABIL")) C[AT_NABIL] *= std::atof(e);
    return C;
}
#include "par.h"

// Одна проба: собирает босса политикой tl (если задана; иначе — обычный генератор) и босса-соперника политикой to
// (nullptr — обычный генератор); два боя со сменой сторон; результат — доля очков собранного босса (0, 0.5, 1).
struct Sample {
    double score = 0;
    Boss mine;
    Trace trace;
};
static void play(const Coef& C, long long B, const Theta* tl, const Theta* to, uint64_t seed, bool record,
                 Sample& out) {
    rng.seed(seed);
    g_th = tl ? tl->data() : nullptr;
    g_trace = record ? &out.trace : nullptr;
    out.mine = makeBoss(B, C, true, false);
    g_trace = nullptr;
    g_th = to ? to->data() : nullptr;
    Boss opp = makeBoss(B, C, true, false);
    g_th = nullptr;
    int r1 = fight(out.mine, opp), r2 = fight(opp, out.mine);
    out.score = ((r1 == 0 ? 1.0 : (r1 == 2 ? 0.5 : 0.0)) + (r2 == 1 ? 1.0 : (r2 == 2 ? 0.5 : 0.0))) / 2;
}

static void saveTheta(const std::string& fn, long long B, const Theta& th, double wr) {
    std::ofstream f(fn);
    f << "бюджет " << B << " винрейт_против_случайного " << wr << " параметров " << th.size() << "\n";
    for (size_t i = 0; i < th.size(); i++) f << th[i] << (i % 12 == 11 ? "\n" : " ");
    f << "\n";
}
static bool loadTheta(const std::string& fn, long long& B, Theta& th) {
    std::ifstream f(fn);
    if (!f) return false;
    std::string a, c, d;
    double wr;
    size_t n;
    f >> a >> B >> c >> wr >> d >> n;
    if (n != (size_t)BP_N) return false;
    th.assign(n, 0);
    for (size_t i = 0; i < n; i++) f >> th[i];
    return true;
}
static std::string parName(int i) {
    if (i < BC_SELFKIND) return BPARN[i];
    struct Blk {
        int base, n;
        const char* nm;
    };
    static const Blk bl[9] = {{BC_SELFKIND, 9, "вид способности на себя"}, {BC_DMGN, 10, "кубики урона способности"},
                              {BC_DMGS, DICE_N, "размер кубика способности"},   {BC_USES, 4, "число применений"},
                              {BC_WDIE, DICE_N, "размер кубика оружия"},        {BC_WATT, 12, "число атак оружия"},
                              {BC_MAIN, 5, "главная характеристика"},      {BC_STAT, 6, "вес характеристики"},
                              {BC_DELIV, 3, "доставка способности"}};
    for (auto& b : bl)
        if (i >= b.base && i < b.base + b.n) {
            char buf[160];
            std::snprintf(buf, sizeof buf, "%s #%d", b.nm, i - b.base);
            return buf;
        }
    static const char* bw[5] = {"доля на пассивы и способности", "доля на оружие", "доля на характеристики",
                                "доля на КД", "доля на миньонов"};
    if (i >= BW_PASS && i < BW_PASS + 5) return bw[i - BW_PASS];
    return "?";
}

static int trainMain(int argc, char** argv) {
    int bi = argc > 2 ? std::atoi(argv[2]) : 3, iters = argc > 3 ? std::atoi(argv[3]) : 300,
        M = argc > 4 ? std::atoi(argv[4]) : 3000;
    uint64_t seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 1;
    std::string out = argc > 6 ? argv[6] : ("champ_" + std::to_string(BUDGETS[bi]) + ".txt");
    long long B = BUDGETS[bi];
    Coef C = coefNow();
    Theta th(BP_N, 0.f), m(BP_N, 0.f), v(BP_N, 0.f);
    if (argc > 7) { long long B0; if (!loadTheta(argv[7], B0, th)) { std::fprintf(stderr, "не прочитан %s\n", argv[7]); return 1; } }   // дообучение с готовой политики
    std::vector<Theta> hof;
    const double lr = 0.04, b1 = 0.9, b2 = 0.999, eps = 1e-3, reg = 0.002;
    std::vector<Sample> smp(M);
    std::vector<char> vsRandom(M);
    for (int it = 1; it <= iters; it++) {
        g_worldEpoch = it * (long long)M / 500;   // новый мир каждые 500 обучающих боёв
        parallelFor(M, [&](int t) {
            uint64_t sd = mix(mix(seed, it), t);
            const Theta* opp = nullptr;
            bool useHof = !hof.empty() && (mix(sd, 77) & 1);
            if (useHof) opp = &hof[mix(sd, 78) % hof.size()];
            vsRandom[t] = !useHof;
            smp[t].trace.clear();
            play(C, B, &th, opp, sd, true, smp[t]);
        });
        double mean = 0, meanR = 0;
        int nR = 0;
        for (int t = 0; t < M; t++) {
            mean += smp[t].score;
            if (vsRandom[t]) {
                meanR += smp[t].score;
                nR++;
            }
        }
        mean /= M;
        meanR = nR ? meanR / nR : 0;
        std::vector<double> g(BP_N, 0.0);
        for (int t = 0; t < M; t++) {
            double A = smp[t].score - mean;
            for (auto& e : smp[t].trace) g[e.first] += A * e.second;
        }
        for (int i = 0; i < BP_N; i++) {
            double gi = g[i] / M - reg * th[i];
            m[i] = (float)(b1 * m[i] + (1 - b1) * gi);
            v[i] = (float)(b2 * v[i] + (1 - b2) * gi * gi);
            double mh = m[i] / (1 - std::pow(b1, it)), vh = v[i] / (1 - std::pow(b2, it));
            th[i] = (float)std::clamp(th[i] + lr * mh / (std::sqrt(vh) + eps), -6.0, 6.0);
        }
        if (it % 50 == 0) hof.push_back(th), (hof.size() > 8 ? (void)hof.erase(hof.begin()) : (void)0);
        if (it % 10 == 0 || it == 1) {
            std::printf("бюджет %lld  итерация %d  винрейт против случайного %.1f%%  против зала славы %.1f%%\n", B, it,
                        100 * meanR, nR < M ? 100 * (mean * M - meanR * nR) / (M - nR) : 0.0);
            std::fflush(stdout);
        }
    }
    // контрольная оценка на других зёрнах: против обычного генератора
    int N = 20000;
    std::vector<Sample> ev(N);
    parallelFor(N, [&](int t) { play(C, B, &th, nullptr, mix(seed + 999, t), false, ev[t]); });
    double wr = 0;
    for (auto& s : ev) wr += s.score;
    wr /= N;
    std::printf("ИТОГ бюджет %lld: винрейт против обычного генератора %.2f%% (%d проб, ±%.1f п.п.)\n", B, 100 * wr, N,
                100 * 2 * std::sqrt(0.25 / (2.0 * N)));
    saveTheta(out, B, th, wr);
    std::printf("сохранено: %s\n", out.c_str());
    return 0;
}

static bool hasAtom(const Boss& b, int a) {
    return b.at(a);
}

static int reportMain(int argc, char** argv, bool adv) {
    Coef C = coefNow();
    std::ostringstream R;
    std::vector<double> freqC(AT_N, 0), freqR(AT_N, 0), winWith(AT_N, 0), nWith(AT_N, 0), winAll(1, 0), wsum(AT_N, 0);
    R << "# Отчёт о дырах: сильнейший Сборщик против обычного генератора\n\n";
    R << "Порог дыры по плану: винрейт выше 55–60% против случайного. Проб на бюджет: 20000 (по 2 боя со сменой "
         "сторон).\n\n";
    R << "| Бюджет | Винрейт Сборщика | ±2σ | Миньоны у Сборщика / у случайного | Средний ИНТ (Сборщик / случайный) | "
         "Доля ИНТ≤3 (Сборщик / случайный) | Потрачено на характеристики (Сборщик / случайный) "
         "|\n|---|---|---|---|---|---|---|\n";
    std::string detail;
    double totWr = 0;
    int nb = 0;
    std::vector<std::vector<double>> perB_ratio;
    for (int k = 0; k < 6 && 2 + k < argc; k++) {
        long long B;
        Theta th;
        if (!loadTheta(argv[2 + k], B, th)) {
            std::fprintf(stderr, "не прочитан %s\n", argv[2 + k]);
            return 1;
        }
        int N = 20000;
        std::vector<Sample> ev(N);
        parallelFor(N, [&](int t) { play(C, B, &th, nullptr, mix(31337 + k, t), false, ev[t]); });
        std::vector<Boss> rnd(N);
        parallelFor(N, [&](int t) {
            rng.seed(mix(424242 + k, t));
            g_th = nullptr;
            rnd[t] = makeBoss(B, C, true, false);
        });
        double wr = 0, sumI = 0, sumIr = 0, low = 0, lowR = 0, mn = 0, mnR = 0, statC = 0, statR = 0;
        std::vector<double> fC(AT_N, 0), fR(AT_N, 0), wW(AT_N, 0);
        for (auto& s : ev) {
            wr += s.score;
            sumI += s.mine.stats[INT];
            low += s.mine.stats[INT] <= 3;
            mn += !s.mine.tmpl.empty();
            long long st = 0;
            for (int i = 0; i < STAT_N; i++) st += tri(s.mine.stats[i]);
            statC += (double)st / B;
            for (int a = 0; a < AT_N; a++)
                if (hasAtom(s.mine, a)) {
                    fC[a]++;
                    wW[a] += s.score;
                }
        }
        for (auto& b : rnd) {
            sumIr += b.stats[INT];
            lowR += b.stats[INT] <= 3;
            mnR += !b.tmpl.empty();
            long long st = 0;
            for (int i = 0; i < STAT_N; i++) st += tri(b.stats[i]);
            statR += (double)st / B;
            for (int a = 0; a < AT_N; a++)
                if (hasAtom(b, a)) fR[a]++;
        }
        wr /= N;
        totWr += wr;
        nb++;
        char buf[400];
        std::snprintf(
            buf, sizeof buf,
            "| %lld | **%.1f%%** %s | ±%.1f | %.0f%% / %.0f%% | %.1f / %.1f | %.0f%% / %.0f%% | %.0f%% / %.0f%% |\n", B,
            100 * wr, wr > 0.60 ? "(дыра)" : (wr > 0.55 ? "(подозрение)" : ""), 100 * 2 * std::sqrt(0.25 / (2.0 * N)),
            100 * mn / N, 100 * mnR / N, sumI / N, sumIr / N, 100 * low / N, 100 * lowR / N, 100 * statC / N,
            100 * statR / N);
        R << buf;
        // детали по бюджету
        std::ostringstream D;
        D << "\n### Бюджет " << B
          << ": что берёт Сборщик (доля боссов с атомом: Сборщик / случайный генератор, винрейт боссов с атомом)\n\n| "
             "Атом | Сборщик | Случайный | Во сколько раз чаще | Винрейт с атомом |\n|---|---|---|---|---|\n";
        std::vector<int> ord;
        for (int a = 0; a < AT_N; a++)
            if (fC[a] / N > 0.02 || fR[a] / N > 0.02) ord.push_back(a);
        std::sort(ord.begin(), ord.end(),
                  [&](int x, int y) { return (fC[x] + 1) / (fR[x] + 1) > (fC[y] + 1) / (fR[y] + 1); });
        int shown = 0;
        for (int a : ord) {
            double r = (fC[a] + 1) / (fR[a] + 1);
            if (shown < 8 || r < 0.35) {
                if (!(shown >= 8 && r >= 0.35)) {
                    char b2[400];
                    std::snprintf(b2, sizeof b2, "| %s | %.1f%% | %.1f%% | ×%.2f | %.1f%% |\n", AD[a].name,
                                  100 * fC[a] / N, 100 * fR[a] / N, r, fC[a] ? 100 * wW[a] / fC[a] : 0.0);
                    D << b2;
                    shown++;
                }
            }
        }
        detail += D.str();
        for (int a = 0; a < AT_N; a++) {
            freqC[a] += fC[a];
            freqR[a] += fR[a];
            winWith[a] += wW[a];
            nWith[a] += fC[a];
        }
        // самые сдвинутые параметры политики
        std::vector<int> pi(BP_N);
        for (int i = 0; i < BP_N; i++) pi[i] = i;
        std::sort(pi.begin(), pi.end(), [&](int x, int y) { return std::fabs(th[x]) > std::fabs(th[y]); });
        D.str("");
        D << "\nСамые сдвинутые решения Сборщика на бюджете " << B << " (θ>0 — делает чаще/больше, θ<0 — реже): ";
        for (int i = 0; i < 10; i++) {
            char b3[120];
            std::snprintf(b3, sizeof b3, "%s %+.1f%s", parName(pi[i]).c_str(), th[pi[i]], i < 9 ? "; " : "");
            D << b3;
        }
        D << "\n";
        detail += D.str();
        std::vector<double> rr(AT_N, 1.0);
        for (int a = 0; a < AT_N; a++) rr[a] = (fC[a] / N > 0.02) ? (fC[a] + 1) / (fR[a] + 1) : 1.0;
        perB_ratio.push_back(rr);
    }
    R << detail;
    // множители цен: атом, который сильные Сборщики берут заметно чаще случайного (в среднем по бюджетам), дорожает
    if (adv) {
        std::printf("# атом_индекс множитель (по средней доле выбора Сборщиком / случайным генератором)\n");
        for (int a = 0; a < AT_N; a++) {
            double c = freqC[a], r = freqR[a];
            if (c / (20000.0 * nb) < 0.03) continue;
            double ratio = (c + 1) / (r + 1);
            if (ratio > 1.5)
                std::printf("ADV %d %.3f  # %s (×%.2f чаще)\n", a, std::min(1.6, 1.0 + 0.25 * (ratio - 1.0)),
                            AD[a].name, ratio);
        }
        return 0;
    }
    std::printf("%s", R.str().c_str());
    std::printf("\nСредний винрейт Сборщика по бюджетам: %.1f%%\n", 100 * totWr / std::max(1, nb));
    std::ofstream f("otchet_dyr.md");
    f << R.str();
    return 0;
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

// ./builder show <файл политики> <сколько боссов> [зерно]: описания боссов, которых собирает обученная политика (для разбора дыр)
static int showMain(int argc, char** argv) {
    long long B; Theta th; if (argc < 3 || !loadTheta(argv[2], B, th)) return 1;
    int n = argc > 3 ? std::atoi(argv[3]) : 3; uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1; Coef C = coefNow();
    double abil = 0, cost = 0, hp = 0, wdmg = 0, natt = 0; int N = 3000;
    for (int i = 0; i < N; i++) { rng.seed(mix(seed, i)); g_th = th.data(); Boss b = makeBoss(B, C, true, false); g_th = nullptr;
        abil += b.abil.size(); hp += 2.0 * b.hp / B; wdmg += b.wN * (b.wS + 1) / 2.0 * b.wAtt; natt += b.wAtt; cost += b.cost; }
    std::printf("бюджет %lld: в среднем способностей %.1f, доля бюджета в ХП %.0f%%, урон оружия за действие %.1f, атак %.1f\n\n", B, abil / N, 100 * hp / N, wdmg / N, natt / N);
    for (int i = 0; i < n; i++) { rng.seed(mix(seed + 99, i)); g_th = th.data(); Boss b = makeBoss(B, C, true, false); g_th = nullptr; std::printf("--- босс %d ---\n%s\n", i + 1, describeBoss(b).c_str()); }
    return 0;
}

// ./builder ablate <файл политики> : винрейт чемпиона, у которого одну группу решений вернули к умолчанию (и только одна группа оставлена)
static int ablateMain(int argc, char** argv) {
    long long B; Theta th; if (argc < 3 || !loadTheta(argv[2], B, th)) return 1;
    struct Grp { const char* name; std::vector<std::pair<int, int>> ranges; };   // [от, до)
    std::vector<Grp> groups = {
        {"пассивы и защиты (присутствие)", {{BP_SUPP, BP_ABSTOP}}},
        {"строение способностей", {{BP_ABSTOP, BP_WMELEE}, {BC_SELFKIND, BC_STAT}, {BC_DELIV, BC_DELIV + 3}}},
        {"оружие", {{BP_WMELEE, BC_SELFKIND}, {BC_WDIE, BC_MAIN}}},
        {"характеристики и КД (главная, веса)", {{BC_MAIN, BC_DELIV}}},
        {"доли бюджета (на способности/оружие/характеристики/КД/миньонов)", {{BW_PASS, BP_N}}},
        {"миньоны и призыв (решение)", {{BP_SUMMON, BP_SUMMON + 1}, {BP_REDIRECT, BP_REDIRECT + 1}}}};
    Coef C = coefNow(); const int N = 12000;
    auto eval = [&](const Theta& t, uint64_t sd) { std::vector<Sample> ev(N); parallelFor(N, [&](int i) { play(C, B, &t, nullptr, mix(sd, i), false, ev[i]); }); double w = 0; for (auto& s : ev) w += s.score; return 100 * w / N; };
    std::printf("бюджет %lld: полный чемпион %.1f%%\n", B, eval(th, 11));
    for (auto& g : groups) {
        Theta off = th, only(th.size(), 0.f);
        for (auto& r : g.ranges) for (int i = r.first; i < r.second; i++) { off[i] = 0; only[i] = th[i]; }
        std::printf("  %-62s без неё %.1f%% | только она %.1f%%\n", g.name, eval(off, 12), eval(only, 13));
    }
    return 0;
}

// ./builder freq <файл политики>: доля боссов с каждым атомом (Сборщик / случайный) и среднее число способностей и применений
static int freqMain(int argc, char** argv) {
    long long B; Theta th; if (argc < 3 || !loadTheta(argv[2], B, th)) return 1;
    Coef C = coefNow(); const int N = 8000; std::vector<double> fc(AT_N, 0), fr(AT_N, 0); double nac = 0, nar = 0, oneC = 0, oneR = 0, abC = 0, abR = 0, bonC = 0, bonR = 0;
    for (int i = 0; i < N; i++) {
        for (int pass = 0; pass < 2; pass++) { rng.seed(mix(555, i)); g_th = pass == 0 ? th.data() : nullptr; Boss b = makeBoss(B, C, true, false); g_th = nullptr;
            auto& f = pass == 0 ? fc : fr; for (int a = 0; a < AT_N; a++) if (b.at(a)) f[a]++;
            double& na = pass == 0 ? nac : nar; na += b.abil.size(); for (auto& s : b.abil) { (pass == 0 ? oneC : oneR) += s.uses < UNL; (pass == 0 ? bonC : bonR) += s.bonus; (pass == 0 ? abC : abR) += 1; } }
    }
    std::printf("бюджет %lld: способностей на босса %.1f / %.1f; из них одноразовых %.0f%% / %.0f%%, бонусных %.0f%% / %.0f%%\n", B, nac / N, nar / N, 100 * oneC / std::max(1.0, abC), 100 * oneR / std::max(1.0, abR), 100 * bonC / std::max(1.0, abC), 100 * bonR / std::max(1.0, abR));
    for (int a = 0; a < AT_N; a++) if (fc[a] / N > 0.15 || fr[a] / N > 0.15) std::printf("  %-70s %5.1f%% / %5.1f%%\n", AD[a].name, 100 * fc[a] / N, 100 * fr[a] / N);
    return 0;
}

// ============================================================================
// Этап 5: два соревнующихся Сборщика (./builder duel <индекс бюджета> <итераций> <пачка> <зерно> [префикс файлов]).
// Сборщики A и B растут одновременно. Соперник каждой пробы: другой Сборщик сейчас (40%), зал славы другого (30%), свой зал славы (15%), обычный
// генератор (15%). Зал славы обоих не даёт зациклиться «камень-ножницы-бумага», обычный генератор удерживает якорь.
// Итог — пара политик в равновесии (A против B около 50%); дальше ./builder duelreport показывает, какие атомы они берут чаще случайного.
// ============================================================================
struct Learner {
    Theta th, m, v;
    std::vector<Theta> hof;
    Learner() : th(BP_N, 0.f), m(BP_N, 0.f), v(BP_N, 0.f) {}
    void step(const std::vector<double>& g, int it) {
        const double lr = 0.04, b1 = 0.9, b2 = 0.999, eps = 1e-3, reg = 0.002;
        for (int i = 0; i < BP_N; i++) {
            double gi = g[i] - reg * th[i];
            m[i] = (float)(b1 * m[i] + (1 - b1) * gi);
            v[i] = (float)(b2 * v[i] + (1 - b2) * gi * gi);
            double mh = m[i] / (1 - std::pow(b1, it)), vh = v[i] / (1 - std::pow(b2, it));
            th[i] = (float)std::clamp(th[i] + lr * mh / (std::sqrt(vh) + eps), -6.0, 6.0);
        }
    }
    void snapshot() { hof.push_back(th); if (hof.size() > 12) hof.erase(hof.begin()); }
};
static int duelMain(int argc, char** argv) {
    int bi = argc > 2 ? std::atoi(argv[2]) : 3, iters = argc > 3 ? std::atoi(argv[3]) : 300, M = argc > 4 ? std::atoi(argv[4]) : 3000;
    uint64_t seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 1;
    std::string prefix = argc > 6 ? argv[6] : "duel";
    long long B = BUDGETS[bi];
    Coef C = coefNow();
    Learner L[2];
    std::vector<Sample> smp[2] = {std::vector<Sample>(M), std::vector<Sample>(M)};
    std::vector<char> kind[2] = {std::vector<char>(M), std::vector<char>(M)};   // 0 — другой сейчас, 1 — зал славы другого, 2 — свой зал, 3 — случайный
    for (int it = 1; it <= iters; it++) {
        g_worldEpoch = it * 2LL * M / 500;   // новый мир каждые 500 обучающих боёв
        parallelFor(2 * M, [&](int k) {
            int side = k / M, t = k % M;
            uint64_t sd = mix(mix(seed + 7 * side, it), t);
            const Learner& me = L[side];
            const Learner& other = L[1 - side];
            int r = (int)(mix(sd, 77) % 100), kd = r < 40 ? 0 : (r < 70 ? 1 : (r < 85 ? 2 : 3));
            if (kd == 1 && other.hof.empty()) kd = 0;
            if (kd == 2 && me.hof.empty()) kd = 3;
            const Theta* opp = kd == 0 ? &other.th : (kd == 1 ? &other.hof[mix(sd, 78) % other.hof.size()] : (kd == 2 ? &me.hof[mix(sd, 79) % me.hof.size()] : nullptr));
            kind[side][t] = (char)kd;
            smp[side][t].trace.clear();
            play(C, B, &me.th, opp, sd, true, smp[side][t]);
        });
        double vsOther[2] = {0, 0}, vsRand[2] = {0, 0}; int nO[2] = {0, 0}, nR[2] = {0, 0};
        for (int s = 0; s < 2; s++) {
            double mean = 0; for (int t = 0; t < M; t++) { mean += smp[s][t].score; if (kind[s][t] == 0) { vsOther[s] += smp[s][t].score; nO[s]++; } if (kind[s][t] == 3) { vsRand[s] += smp[s][t].score; nR[s]++; } }
            mean /= M;
            std::vector<double> g(BP_N, 0.0);
            for (int t = 0; t < M; t++) { double A = smp[s][t].score - mean; for (auto& e : smp[s][t].trace) g[e.first] += A * e.second / M; }
            L[s].step(g, it);
        }
        if (it % 25 == 0) { L[0].snapshot(); L[1].snapshot(); }
        if (it % 10 == 0 || it == 1) {
            std::printf("бюджет %lld итерация %d: A против B %.1f%% (B против A %.1f%%); против случайного A %.1f%%, B %.1f%%\n", B, it,
                        nO[0] ? 100 * vsOther[0] / nO[0] : 0.0, nO[1] ? 100 * vsOther[1] / nO[1] : 0.0, nR[0] ? 100 * vsRand[0] / nR[0] : 0.0, nR[1] ? 100 * vsRand[1] / nR[1] : 0.0);
            std::fflush(stdout);
        }
    }
    // контроль: A против B, каждый против обычного генератора
    int N = 12000; std::vector<Sample> ab(N), ar(N), br(N);
    parallelFor(N, [&](int t) { play(C, B, &L[0].th, &L[1].th, mix(seed + 999, t), false, ab[t]); play(C, B, &L[0].th, nullptr, mix(seed + 998, t), false, ar[t]); play(C, B, &L[1].th, nullptr, mix(seed + 997, t), false, br[t]); });
    double wab = 0, war = 0, wbr = 0; for (int t = 0; t < N; t++) { wab += ab[t].score; war += ar[t].score; wbr += br[t].score; }
    std::printf("ИТОГ бюджет %lld: A против B %.2f%%; A против случайного %.2f%%, B против случайного %.2f%% (±%.1f п.п.)\n", B, 100 * wab / N, 100 * war / N, 100 * wbr / N, 100 * 2 * std::sqrt(0.25 / (2.0 * N)));
    saveTheta(prefix + "_" + std::to_string(B) + "_A.txt", B, L[0].th, war / N);
    saveTheta(prefix + "_" + std::to_string(B) + "_B.txt", B, L[1].th, wbr / N);
    return 0;
}

// ./builder duelreport <префикс> [adv]: равновесие пар — какие атомы пара берёт чаще случайного генератора; с «adv» печатает множители цен
static int duelReportMain(int argc, char** argv) {
    std::string prefix = argc > 2 ? argv[2] : "duel"; bool adv = argc > 3 && std::string(argv[3]) == "adv";
    Coef C = coefNow(); std::vector<double> fm(AT_N, 0), fr(AT_N, 0); double total = 0;
    std::ostringstream R; R << "# Равновесие пар Сборщиков (Этап 5)\n\n| Бюджет | A против B | A против случайного | B против случайного | способностей на босса (пара / случайный) | средний ИНТ (пара / случайный) |\n|---|---|---|---|---|---|\n";
    for (int k = 0; k < 6; k++) {
        long long B = BUDGETS[k]; Theta ta, tb; long long b1, b2;
        if (!loadTheta(prefix + "_" + std::to_string(B) + "_A.txt", b1, ta) || !loadTheta(prefix + "_" + std::to_string(B) + "_B.txt", b2, tb)) continue;
        int N = 12000; std::vector<Sample> ab(N), ar(N), br(N); std::vector<Boss> rnd(N);
        parallelFor(N, [&](int t) { play(C, B, &ta, &tb, mix(2468 + k, t), false, ab[t]); play(C, B, &ta, nullptr, mix(1357 + k, t), false, ar[t]); play(C, B, &tb, nullptr, mix(9753 + k, t), false, br[t]);
                                    rng.seed(mix(8642 + k, t)); g_th = nullptr; rnd[t] = makeBoss(B, C, true, false); });
        double wab = 0, war = 0, wbr = 0, nab = 0, nr = 0, ia = 0, ir = 0;
        for (int t = 0; t < N; t++) {
            wab += ab[t].score; war += ar[t].score; wbr += br[t].score;
            for (const Boss* b : {&ar[t].mine, &br[t].mine}) { nab += b->abil.size(); ia += b->stats[INT]; for (int a = 0; a < AT_N; a++) if (b->at(a)) fm[a] += 1; total += 1; }
            nr += rnd[t].abil.size(); ir += rnd[t].stats[INT]; for (int a = 0; a < AT_N; a++) if (rnd[t].at(a)) fr[a] += 2;   // вес 2: случайных столько же, сколько боссов пары
        }
        char buf[300]; std::snprintf(buf, sizeof buf, "| %lld | %.1f%% | %.1f%% | %.1f%% | %.1f / %.1f | %.1f / %.1f |\n", B, 100 * wab / N, 100 * war / N, 100 * wbr / N, nab / (2.0 * N), nr / N, ia / (2.0 * N), ir / N); R << buf;
    }
    R << "\n## Атомы, которые пара берёт чаще случайного генератора (по всем бюджетам; доля боссов с атомом)\n\n| Атом | Пара | Случайный | Во сколько раз чаще |\n|---|---|---|---|\n";
    std::vector<int> ord; for (int a = 0; a < AT_N; a++) if (fm[a] / std::max(1.0, total) > 0.03 || fr[a] / std::max(1.0, total) > 0.03) ord.push_back(a);
    std::sort(ord.begin(), ord.end(), [&](int x, int y) { return (fm[x] + 1) / (fr[x] + 1) > (fm[y] + 1) / (fr[y] + 1); });
    int shown = 0;
    for (int a : ord) { double r = (fm[a] + 1) / (fr[a] + 1); if (shown++ < 14 || r < 0.3) { char b2[300]; std::snprintf(b2, sizeof b2, "| %s | %.1f%% | %.1f%% | ×%.2f |\n", AD[a].name, 100 * fm[a] / total, 100 * fr[a] / total, r); R << b2; } }
    if (adv) {
        std::printf("# атом_индекс множитель (равновесие пар Сборщиков)\n");
        for (int a : ord) { double r = (fm[a] + 1) / (fr[a] + 1); if (r > 2.0) std::printf("ADV %d %.3f  # %s (×%.2f чаще)\n", a, std::min(6.0, 1.0 + 0.5 * (r - 1.0)), AD[a].name, r); }
        return 0;
    }
    std::printf("%s", R.str().c_str());
    std::ofstream f("otchet_ravnovesie.md"); f << R.str();
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "tiers") return tiersMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "show") return showMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "duel") return duelMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "duelreport") return duelReportMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "freq") return freqMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "ablate") return ablateMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "train") return trainMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "report") return reportMain(argc, argv, false);
    if (argc > 1 && std::string(argv[1]) == "adv") return reportMain(argc, argv, true);
    std::fprintf(stderr, "использование: builder train <0..5> <итераций> <пачка> <зерно> [файл] | builder report <6 "
                         "файлов> | builder adv <6 файлов>\n");
    return 1;
}
