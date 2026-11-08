#pragma once
// Сеть-Сборщик: решения генератора через рекуррентную сеть, обучение A2C. Подключается после движка, gru.h и par.h.
namespace bld {
typedef GruNet<float> BNet;
static const int NOPT = 24;   // вариантов в одном выборе (этап 5: состояния — 15)
static const int BD = BP_N + 10, BE = 32, BH = 48, BNA = NOPT + 2;   // логиты: [0] поправка да/нет, [1..NOPT] выбор из вариантов, [NOPT+1] растяжка доли
static const long long BUDGETS[6] = {200, 400, 800, 1600, 3200, 6400};

struct BStep {
    int type = 0;   // 0 да/нет, 1 выбор, 2 растяжка
    int id = 0, n = 0, res = 0;
    float p0 = 0, u = 0, v = 0, val = 0;
    float p0c[NOPT];
    float feat[10];
    float hprev[BH];
};

static inline float tanhScaled(float raw, float s) { return s * std::tanh(raw / s); }

struct BrainNet : BuildBrain {
    const BNet* net;
    std::vector<BStep>* rec;   // куда писать шаги (nullptr — не записывать)
    float h[BH];
    int t = 0;
    BrainNet(const BNet* n, std::vector<BStep>* r) : net(n), rec(r) { for (int i = 0; i < BH; i++) h[i] = 0; }

    void fillFeat(BStep& s) {
        const BuildState& b = g_bstate;
        const double B = std::max(1.0, b.budget);
        s.feat[0] = (float)(std::log2(B) / 14.0);
        s.feat[1] = (float)(b.spent / B);
        s.feat[2] = (float)(b.passLeft / B);
        s.feat[3] = (float)(b.nAbil / 20.0);
        s.feat[4] = (float)(b.depth - 1);
        s.feat[5] = s.type == 0;
        s.feat[6] = s.type == 1;
        s.feat[7] = s.type == 2;
        s.feat[8] = s.type == 0 ? s.p0 : s.n / 12.0f;
        s.feat[9] = std::min(1.0f, t / 400.0f);
    }
    // прямой проход для шага s (сеть вызывается с текущим скрытым состоянием)
    void run(BStep& s, BNet::Step& st, float* logits, float& value) {
        float x[BD];
        for (int i = 0; i < BP_N; i++) x[i] = 0;
        x[s.id] = 1;
        for (int i = 0; i < 10; i++) x[BP_N + i] = s.feat[i];
        std::copy(h, h + BH, s.hprev);
        net->forward(x, h, st, logits, value);
        std::copy(st.h.begin(), st.h.end(), h);
        s.val = value;
        t++;
    }
    bool bern(int id, double p0, double u) override {
        BStep s;
        s.type = 0; s.id = id; s.n = 0; s.p0 = (float)std::clamp(p0, 1e-4, 1 - 1e-4);
        fillFeat(s);
        BNet::Step st; float lg[BNA]; float val;
        run(s, st, lg, val);
        const double d = tanhScaled(lg[0], 6.0f), p = 1.0 / (1.0 + std::exp(-(std::log(s.p0 / (1.0 - s.p0)) + d)));
        const bool x = u < p;
        s.res = x; s.u = (float)u;
        if (rec) rec->push_back(s);
        return x;
    }
    int cat(int base, int n, const double* p0, double u) override {
        BStep s;
        s.type = 1; s.id = base; s.n = n;
        for (int j = 0; j < NOPT; j++) s.p0c[j] = j < n ? (float)p0[j] : 0.f;
        fillFeat(s);
        BNet::Step st; float lg[BNA]; float val;
        run(s, st, lg, val);
        double pj[NOPT], tot = 0;
        for (int j = 0; j < n; j++) { pj[j] = p0[j] * std::exp((double)tanhScaled(lg[1 + j], 6.0f)); tot += pj[j]; }
        double q = u * tot;
        int c = n - 1;
        for (int j = 0; j < n; j++) { q -= pj[j]; if (q <= 0 && pj[j] > 0) { c = j; break; } }
        s.res = c; s.u = (float)u;
        if (rec) rec->push_back(s);
        return c;
    }
    double warp(int id, double u) override {
        BStep s;
        s.type = 2; s.id = id; s.n = 0;
        fillFeat(s);
        BNet::Step st; float lg[BNA]; float val;
        run(s, st, lg, val);
        const double th = tanhScaled(lg[NOPT + 1], 4.0f), v = std::pow(u, std::exp(th));
        s.u = (float)u; s.v = (float)v;
        if (rec) rec->push_back(s);
        return v;
    }
};

static Coef coefNow() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    return C;
}

// --------------------------------------------------------------------------------------------------------------------------------
// Веса
// --------------------------------------------------------------------------------------------------------------------------------
static void initNet(BNet& n, uint64_t seed) {
    n.init(BD, BE, BH, BNA, seed);
    for (size_t i = 0; i < (size_t)BNA * BH; i++) n.p[n.oWo + i] *= 0.05f;   // поправки ≈ 0: сеть стартует как обычный генератор
    for (int i = 0; i < BNA; i++) n.p[n.obo + i] = 0.f;
}
static bool saveNet(const std::string& fn, const BNet& n) {
    std::ofstream f(fn, std::ios::binary);
    if (!f) return false;
    int hd[4] = {n.D, n.E, n.H, n.NA};
    f.write((const char*)hd, sizeof hd);
    f.write((const char*)n.p.data(), n.p.size() * sizeof(float));
    return true;
}
static bool loadNet(const std::string& fn, BNet& n) {
    std::ifstream f(fn, std::ios::binary);
    if (!f) return false;
    int hd[4];
    f.read((char*)hd, sizeof hd);
    if (hd[1] != BE || hd[2] != BH || hd[3] != BNA) return false;
    if (hd[0] != BD) {
        // этап 5б: новые решения дописаны в конец BPar — вход старых весов короче; столбцы решений остаются на месте,
        // 10 признаков сдвигаются в конец, у новых решений веса нулевые (сеть собирает как раньше)
        const int bpOld = hd[0] - 10;
        if (bpOld <= 0 || bpOld > BP_N) return false;
        BNet old;
        old.init(hd[0], BE, BH, BNA, 1);
        f.read((char*)old.p.data(), old.p.size() * sizeof(float));
        if (!f) return false;
        n.init(BD, BE, BH, BNA, 1);
        std::fill(n.p.begin(), n.p.end(), 0.f);
        for (int i = 0; i < BE; i++) {
            const float* src = &old.p[old.oWe + (size_t)i * hd[0]];
            float* dst = &n.p[n.oWe + (size_t)i * BD];
            std::copy(src, src + bpOld, dst);
            std::copy(src + bpOld, src + hd[0], dst + BP_N);
        }
        std::copy(old.p.begin() + old.obe, old.p.end(), n.p.begin() + n.obe);
        return true;
    }
    n.init(hd[0], hd[1], hd[2], hd[3], 1);
    f.read((char*)n.p.data(), n.p.size() * sizeof(float));
    return (bool)f;
}
// линейный Сборщик (builder.cc): файл «бюджет … параметров N» и N чисел
static bool loadTheta(const std::string& fn, std::vector<float>& th) {
    std::ifstream f(fn);
    if (!f) return false;
    std::string a, c, d;
    long long B; double wr; size_t n;
    f >> a >> B >> c >> wr >> d >> n;
    if (n > (size_t)BP_N) return false;
    th.assign((size_t)BP_N, 0);   // этап 5: новые решения дописаны в конец — у старых линейных Сборщиков они нулевые
    for (size_t i = 0; i < n; i++) f >> th[i];
    return (bool)f;
}

// --------------------------------------------------------------------------------------------------------------------------------
// Одна проба: сеть собирает босса (записывая шаги), соперник собирает босса, два боя со сменой сторон
// --------------------------------------------------------------------------------------------------------------------------------
struct Opp {
    const BNet* net = nullptr;                 // соперник — сеть
    const std::vector<float>* theta = nullptr; // соперник — линейный Сборщик
};
struct Sample {
    std::vector<BStep> steps;
    double score = 0;
    int failures = 0, guard = 0;
    long long budget = 0;
    Boss mine;
};
static int checkBoss(const Boss& b, long long B) {   // системные сбои: невозможный босс
    int bad = 0;
    if (b.cost - STAT_BASE > B) bad++;   // B — видимый бюджет
    if (b.hp < 1) bad++;
    for (int i = 0; i < STAT_N; i++) if (b.stats[i] < 1 || b.stats[i] > 60) bad++;
    if (b.wN < 0 || b.wS < 0 || b.abil.size() > 200) bad++;
    return bad;
}
static void play(const Coef& C, long long B, const BNet* mine, const Opp& opp, uint64_t seed, bool record, Sample& out) {
    // этап 5: в боях обоими боссами ходит обученная сеть-боец (если загружена; как в калибровке, температура 0.25)
    g_forceNetMask = pn::primNet() ? 3 : 0;
    rng.seed(seed);
    out.budget = B;
    out.steps.clear();
    BrainNet bm(mine, record ? &out.steps : nullptr);
    g_bb = &bm;
    out.mine = makeBoss(B, C, true, false);
    g_bb = nullptr;
    Boss other;
    if (opp.net) {
        BrainNet bo(opp.net, nullptr);
        g_bb = &bo;
        other = makeBoss(B, C, true, false);
        g_bb = nullptr;
    } else {
        g_th = opp.theta ? opp.theta->data() : nullptr;
        other = makeBoss(B, C, true, false);
        g_th = nullptr;
    }
    out.failures = checkBoss(out.mine, B) + checkBoss(other, B);
    const long long c0 = g_capped;
    const int r1 = fight(out.mine, other), r2 = fight(other, out.mine);
    out.guard = (int)(g_capped - c0);
    out.score = ((r1 == 0 ? 1.0 : (r1 == 2 ? 0.5 : 0.0)) + (r2 == 1 ? 1.0 : (r2 == 2 ? 0.5 : 0.0))) / 2;
}

// --------------------------------------------------------------------------------------------------------------------------------
// Обучение: A2C с GAE (дисконт 1, единственная награда в конце), обратный проход по времени с перевычислением активаций
// --------------------------------------------------------------------------------------------------------------------------------
struct Learner {
    BNet net;
    AdamState adam;
    std::vector<BNet> hof;
    void snapshot() { hof.push_back(net); if (hof.size() > 8) hof.erase(hof.begin()); }
};

static void updateLearner(Learner& L, std::vector<Sample>& smp, int it, double& entOut, double& vlOut) {
    const double lambda = 0.97, vcoef = 0.5, beta = 0.003, lr = 5e-4;
    const int N = (int)smp.size();
    std::vector<std::vector<float>> adv(N), ret(N);
    double sumA = 0, sumA2 = 0, totSteps = 0;
    for (int i = 0; i < N; i++) {
        auto& st = smp[i].steps;
        const int T = (int)st.size();
        adv[i].assign(T, 0.f);
        ret[i].assign(T, 0.f);
        const double R = (smp[i].score - 0.5) * 2.0;   // единственная награда: исход боёв в диапазоне [-1, 1]
        double gae = 0;
        for (int t = T - 1; t >= 0; t--) {
            const double nextV = t + 1 < T ? st[t + 1].val : 0.0, r = t == T - 1 ? R : 0.0;
            const double delta = r + nextV - st[t].val;
            gae = delta + lambda * gae;
            adv[i][t] = (float)gae;
            ret[i][t] = (float)(gae + st[t].val);
            sumA += gae; sumA2 += gae * gae; totSteps += 1;
        }
    }
    const double mA = sumA / std::max(1.0, totSteps), sA = std::sqrt(std::max(1e-8, sumA2 / std::max(1.0, totSteps) - mA * mA));
    const int nChunks = std::max(1, nThreads() * 2);
    std::vector<std::vector<float>> gbuf(nChunks, std::vector<float>(L.net.size(), 0.f));
    std::vector<double> entSum(nChunks, 0.0), vlSum(nChunks, 0.0);
    parallelFor(nChunks, [&](int ch) {
        auto& g = gbuf[ch];
        for (int i = ch; i < N; i += nChunks) {
            auto& st = smp[i].steps;
            const int T = (int)st.size();
            std::vector<float> dhn(BH, 0.f), dhp(BH);
            for (int t = T - 1; t >= 0; t--) {
                BStep& s = st[t];
                float x[BD];
                for (int k = 0; k < BP_N; k++) x[k] = 0;
                x[s.id] = 1;
                for (int k = 0; k < 10; k++) x[BP_N + k] = s.feat[k];
                BNet::Step stp; float lg[BNA]; float val;
                L.net.forward(x, s.hprev, stp, lg, val);   // перевычисление активаций шага
                const double A = (adv[i][t] - mA) / sA;
                float dl[BNA];
                for (int k = 0; k < BNA; k++) dl[k] = 0;
                if (s.type == 0) {
                    const double raw = lg[0], d = 6.0 * std::tanh(raw / 6.0), p0 = s.p0;
                    const double lgt = std::log(p0 / (1.0 - p0)) + d, p = 1.0 / (1.0 + std::exp(-lgt));
                    const double dd = 1.0 - (d / 6.0) * (d / 6.0);
                    const double dlogp = (s.res ? 1.0 : 0.0) - p;                   // d ln π / dδ
                    const double dH = -p * (1 - p) * lgt;                           // d H / dδ
                    dl[0] = (float)((-A * dlogp - beta * dH) * dd);
                    entSum[ch] += -(p * std::log(std::max(p, 1e-12)) + (1 - p) * std::log(std::max(1 - p, 1e-12)));
                } else if (s.type == 1) {
                    double zj[NOPT], dz[NOPT], pj[NOPT], tot = 0;
                    for (int j = 0; j < s.n; j++) { zj[j] = 6.0 * std::tanh(lg[1 + j] / 6.0); dz[j] = 1.0 - (zj[j] / 6.0) * (zj[j] / 6.0); pj[j] = s.p0c[j] * std::exp(zj[j]); tot += pj[j]; }
                    double H = 0;
                    for (int j = 0; j < s.n; j++) { pj[j] /= tot; if (pj[j] > 1e-12) H -= pj[j] * std::log(pj[j]); }
                    entSum[ch] += H;
                    for (int j = 0; j < s.n; j++) {
                        const double lp = pj[j] > 1e-12 ? std::log(pj[j]) : -27.0;
                        const double g1 = -A * ((j == s.res ? 1.0 : 0.0) - pj[j]) + beta * pj[j] * (lp + H);
                        dl[1 + j] = (float)(g1 * dz[j]);
                    }
                } else {
                    const double th = 4.0 * std::tanh(lg[NOPT + 1] / 4.0), dd = 1.0 - (th / 4.0) * (th / 4.0);
                    const double dlogp = -1.0 - std::exp(-th) * std::log(std::max((double)s.v, 1e-12));
                    dl[NOPT + 1] = (float)(-A * dlogp * dd);
                }
                const double dv = vcoef * (val - ret[i][t]);
                vlSum[ch] += 0.5 * (val - ret[i][t]) * (val - ret[i][t]);
                L.net.backward(stp, dl, (float)dv, dhn.data(), g, dhp.data());
                dhn = dhp;
            }
        }
    });
    std::vector<float> G(L.net.size(), 0.f);
    for (int ch = 0; ch < nChunks; ch++) for (size_t k = 0; k < G.size(); k++) G[k] += gbuf[ch][k];
    const float inv = (float)(1.0 / std::max(1.0, totSteps));
    for (auto& x : G) x *= inv;
    L.adam.step(L.net.p, G, lr, 1.0);
    double ent = 0, vl = 0;
    for (int ch = 0; ch < nChunks; ch++) { ent += entSum[ch]; vl += vlSum[ch]; }
    entOut = ent / std::max(1.0, totSteps);
    vlOut = vl / std::max(1.0, totSteps);
    (void)it;
}

// оценка сети против обычного генератора и против линейных Сборщиков
struct EvalRes { double vsRandom = 0, vsLinear = 0; double failures = 0, guard = 0; double steps = 0; };
static EvalRes evalNet(const Coef& C, const BNet& net, int N, uint64_t seed, const std::vector<float> lin[6]) {
    EvalRes r;
    std::vector<Sample> a(N), b(N);
    Opp rnd;
    parallelFor(N, [&](int i) {
        const uint64_t sd = mix(seed, i);
        const int bi = (int)(mix(sd, 5) % 6);
        play(C, BUDGETS[bi], &net, rnd, sd, false, a[i]);
        Opp lo; lo.theta = lin[bi].empty() ? nullptr : &lin[bi];
        play(C, BUDGETS[bi], &net, lo, sd, false, b[i]);
    });
    for (int i = 0; i < N; i++) { r.vsRandom += a[i].score; r.vsLinear += b[i].score; r.failures += a[i].failures + b[i].failures; r.guard += a[i].guard + b[i].guard; r.steps += a[i].mine.abil.size(); }
    r.vsRandom /= N; r.vsLinear /= N; r.steps /= N;
    return r;
}
static void loadLinear(std::vector<float> lin[6]) {
    for (int i = 0; i < 6; i++) loadTheta("builder_runs/v4_champ_" + std::to_string(BUDGETS[i]) + ".txt", lin[i]);
}

static int trainMain(int argc, char** argv) {
    const int iters = argc > 2 ? std::atoi(argv[2]) : 100, M = argc > 3 ? std::atoi(argv[3]) : 768;
    const uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1;
    const std::string prefix = argc > 5 ? argv[5] : "net_runs/builder";
    Coef C = coefNow();
    Learner L[2];
    for (int s = 0; s < 2; s++) {
        initNet(L[s].net, seed + 17 * s);
        L[s].adam.init(L[s].net.size());
        if (argc > 6 + s && !loadNet(argv[6 + s], L[s].net)) { std::fprintf(stderr, "не прочитан %s\n", argv[6 + s]); return 1; }
    }
    std::vector<float> lin[6];
    loadLinear(lin);
    std::vector<Sample> smp[2] = {std::vector<Sample>(M), std::vector<Sample>(M)};
    std::vector<char> kind[2] = {std::vector<char>(M), std::vector<char>(M)};
    for (int it = 1; it <= iters; it++) {
        g_worldEpoch = it * 2LL * M / 500;   // новый мир каждые 500 обучающих боёв
        parallelFor(2 * M, [&](int k) {
            const int side = k / M, t = k % M;
            const uint64_t sd = mix(mix(seed + 5 * side, it), t);
            const Learner& me = L[side];
            const Learner& other = L[1 - side];
            int r = (int)(mix(sd, 77) % 100), kd = r < 45 ? 0 : (r < 70 ? 1 : (r < 80 ? 2 : 3));
            if (kd == 1 && other.hof.empty()) kd = 0;
            if (kd == 2 && me.hof.empty()) kd = 3;
            Opp o;
            if (kd == 0) o.net = &other.net;
            else if (kd == 1) o.net = &other.hof[mix(sd, 78) % other.hof.size()];
            else if (kd == 2) o.net = &me.hof[mix(sd, 79) % me.hof.size()];
            kind[side][t] = (char)kd;
            // бюджет любой 200…6400 (логарифмически равномерно), как в обучении бойца
            const double ub = (double)(mix(sd, 5) >> 11) * (1.0 / 9007199254740992.0);
            play(C, (long long)std::llround(200.0 * std::pow(2.0, 5.0 * ub)), &me.net, o, sd, true, smp[side][t]);
        });
        double vsOther[2] = {0, 0}, vsRand[2] = {0, 0}, fails = 0, guards = 0, steps = 0;
        int nO[2] = {0, 0}, nR[2] = {0, 0};
        for (int s = 0; s < 2; s++)
            for (int t = 0; t < M; t++) {
                if (kind[s][t] == 0) { vsOther[s] += smp[s][t].score; nO[s]++; }
                if (kind[s][t] == 3) { vsRand[s] += smp[s][t].score; nR[s]++; }
                fails += smp[s][t].failures; guards += smp[s][t].guard; steps += smp[s][t].steps.size();
            }
        double ent[2], vl[2];
        for (int s = 0; s < 2; s++) updateLearner(L[s], smp[s], it, ent[s], vl[s]);
        if (it % 25 == 0) { L[0].snapshot(); L[1].snapshot(); }
        if (it % 5 == 0 || it == 1) {
            std::printf("итерация %4d: A против B %.1f%%; против случайного A %.1f%% B %.1f%%; решений на сборку %.0f; энтропия %.2f; ошибка ценности %.3f; системных сбоев %.0f; боёв до предохранителя %.0f\n", it,
                        nO[0] ? 100 * vsOther[0] / nO[0] : 0.0, nR[0] ? 100 * vsRand[0] / nR[0] : 0.0, nR[1] ? 100 * vsRand[1] / nR[1] : 0.0, steps / (2.0 * M), ent[0], vl[0], fails, guards);
            std::fflush(stdout);
        }
        if (it % 50 == 0 || it == iters) {
            saveNet(prefix + "_A.bin", L[0].net);
            saveNet(prefix + "_B.bin", L[1].net);
            EvalRes a = evalNet(C, L[0].net, 3000, 424200 + it, lin), b = evalNet(C, L[1].net, 3000, 424300 + it, lin);
            std::printf("  контроль (3000 сборок): A: против случайного %.1f%%, против линейного %.1f%%, способностей %.1f | B: против случайного %.1f%%, против линейного %.1f%% | системных сбоев %.0f\n",
                        100 * a.vsRandom, 100 * a.vsLinear, a.steps, 100 * b.vsRandom, 100 * b.vsLinear, a.failures + b.failures);
            std::fflush(stdout);
        }
    }
    saveNet(prefix + "_A.bin", L[0].net);
    saveNet(prefix + "_B.bin", L[1].net);
    return 0;
}

static int evalMain(int argc, char** argv) {
    BNet net;
    if (argc < 4 || !loadNet(argv[2], net)) return 1;
    const int N = std::atoi(argv[3]);
    Coef C = coefNow();
    std::vector<float> lin[6];
    loadLinear(lin);
    EvalRes r = evalNet(C, net, N, 99, lin);
    std::printf("сеть-Сборщик, %d сборок: против обычного генератора %.2f%%, против линейных Сборщиков %.2f%% (±%.1f п.п.), способностей на босса %.1f, системных сбоев %.0f, боёв до предохранителя %.0f\n", N,
                100 * r.vsRandom, 100 * r.vsLinear, 100 * 2 * std::sqrt(0.25 / (2.0 * N)), r.steps, r.failures, r.guard);
    return 0;
}

static int showMain(int argc, char** argv) {
    BNet net;
    if (argc < 4 || !loadNet(argv[2], net)) return 1;
    const int n = std::atoi(argv[3]);
    Coef C = coefNow();
    for (int i = 0; i < n; i++) {
        rng.seed(mix(31, i));
        BrainNet b(&net, nullptr);
        g_bb = &b;
        Boss boss = makeBoss(BUDGETS[i % 6], C, true, false);
        g_bb = nullptr;
        std::printf("--- босс %d (бюджет %lld) ---\n%s\n", i + 1, BUDGETS[i % 6], describeBoss(boss).c_str());
    }
    return 0;
}

// ./net_builder replay <A.bin> <B.bin> <бюджет> <зерно> <файл.html>: боссов собирают нейросети-Сборщики, бой ведут сети-бойцы (если ИНТ >= 12); запись для просмотрщика
static int replayMain(int argc, char** argv) {
    BNet na, nb;
    if (argc < 7 || !loadNet(argv[2], na) || !loadNet(argv[3], nb)) {
        std::fprintf(stderr, "использование: net_builder replay <A.bin> <B.bin> <бюджет> <зерно> <файл.html>\n");
        return 1;
    }
    const long long budget = std::atoll(argv[4]);
    const unsigned seed = (unsigned)std::strtoul(argv[5], nullptr, 10);
    Coef C = coefNow();
    anyFighterNet();   // загружает сеть-бойца (новую или старую по DND_NET_LEGACY)
    Boss boss[2];
    for (int k = 0; k < 2; k++) {
        rng.seed(mix(seed, 100 + k));
        BrainNet b(k == 0 ? &na : &nb, nullptr);
        g_bb = &b;
        boss[k] = makeBoss(budget, C, true, false);
        g_bb = nullptr;
    }
    rng.seed(mix(seed, 777));
    std::vector<std::string> frames;
    std::string mapJson;
    const int res = fight(boss[0], boss[1], &frames, &mapJson);
    std::printf("Победитель: %s\n", res == 0 ? "Босс A" : (res == 1 ? "Босс B" : "ничья"));
    writeViewerHTML(argv[6], boss[0], boss[1], C, (double)budget, std::string("нейросеть-Сборщик A (") + argv[2] + ")", std::string("нейросеть-Сборщик B (") + argv[3] + ")", seed, mapJson, frames);
    return 0;
}

// ./net_builder report <A.bin> <B.bin> [N]: что собирают сети — доли атомов против обычного генератора, способности, ИНТ, винрейты по бюджетам
static int reportMain(int argc, char** argv) {
    BNet na, nb;
    if (argc < 4 || !loadNet(argv[2], na) || !loadNet(argv[3], nb)) return 1;
    const int N = argc > 4 ? std::atoi(argv[4]) : 6000;
    Coef C = coefNow();
    std::vector<double> fm(AT_N, 0), fr(AT_N, 0);
    double total = 0;
    std::printf("| Бюджет | A против B | A против случайного | B против случайного | способностей (сети / случайный) | средний ИНТ (сети / случайный) | доля ИНТ>=12 (сети / случайный) | сбоев |\n|---|---|---|---|---|---|---|---|\n");
    for (int k = 0; k < 6; k++) {
        const long long B = BUDGETS[k];
        std::vector<Sample> ab(N), ar(N), br(N);
        std::vector<Boss> rnd(N);
        Opp rnd0, oppB, oppA;
        oppB.net = &nb;
        oppA.net = &na;
        parallelFor(N, [&](int t) {
            play(C, B, &na, oppB, mix(2468 + k, t), false, ab[t]);
            play(C, B, &na, rnd0, mix(1357 + k, t), false, ar[t]);
            play(C, B, &nb, rnd0, mix(9753 + k, t), false, br[t]);
            rng.seed(mix(8642 + k, t));
            rnd[t] = makeBoss(B, C, true, false);
        });
        double wab = 0, war = 0, wbr = 0, nab = 0, nr = 0, ia = 0, ir = 0, hi = 0, hir = 0, fails = 0;
        for (int t = 0; t < N; t++) {
            wab += ab[t].score; war += ar[t].score; wbr += br[t].score; fails += ab[t].failures + ar[t].failures + br[t].failures;
            for (const Boss* b : {&ar[t].mine, &br[t].mine}) {
                nab += b->abil.size(); ia += b->stats[INT]; hi += b->stats[INT] >= 12; total += 1;
                for (int a2 = 0; a2 < AT_N; a2++) if (b->at(a2)) fm[a2] += 1;
            }
            nr += rnd[t].abil.size(); ir += rnd[t].stats[INT]; hir += rnd[t].stats[INT] >= 12;
            for (int a2 = 0; a2 < AT_N; a2++) if (rnd[t].at(a2)) fr[a2] += 2;
        }
        std::printf("| %lld | %.1f%% | %.1f%% | %.1f%% | %.1f / %.1f | %.1f / %.1f | %.0f%% / %.0f%% | %.0f |\n", B, 100 * wab / N, 100 * war / N, 100 * wbr / N, nab / (2.0 * N), nr / N, ia / (2.0 * N), ir / N,
                    100 * hi / (2.0 * N), 100 * hir / N, fails);
    }
    std::vector<int> ord;
    for (int a2 = 0; a2 < AT_N; a2++) if (fm[a2] / std::max(1.0, total) > 0.03 || fr[a2] / std::max(1.0, total) > 0.03) ord.push_back(a2);
    std::sort(ord.begin(), ord.end(), [&](int x, int y) { return (fm[x] + 1) / (fr[x] + 1) > (fm[y] + 1) / (fr[y] + 1); });
    if (argc > 5 && std::string(argv[5]) == "adv") {   // множители цен для apply_adv.py (возврат за уязвимость уменьшается, остальные растут)
        std::printf("# атом_индекс множитель (равновесие сетей-Сборщиков)\n");
        for (int a2 : ord) {
            const double r = (fm[a2] + 1) / (fr[a2] + 1);
            if (r > 2.5) std::printf("ADV %d %.3f  # %s (×%.2f чаще)\n", a2, std::min(6.0, 1.0 + 0.5 * (r - 1.0)), AD[a2].name, r);
        }
        return 0;
    }
    std::printf("\n| Атом | Сети | Случайный | Во сколько раз чаще |\n|---|---|---|---|\n");
    int shown = 0;
    for (int a2 : ord) {
        const double r = (fm[a2] + 1) / (fr[a2] + 1);
        if (shown++ < 14 || r < 0.3) std::printf("| %s | %.1f%% | %.1f%% | ×%.2f |\n", AD[a2].name, 100 * fm[a2] / total, 100 * fr[a2] / total, r);
    }
    return 0;
}

}  // namespace bld
