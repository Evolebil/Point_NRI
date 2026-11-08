#pragma once
// Обучение сети-бойца (A2C): эпизоды, обновление, оценка. Подключается после движка (boss_arena_v20.cc), gru.h и par.h.
namespace cmb {
static const long long BUDGETS[6] = {200, 400, 800, 1600, 3200, 6400};

static bool g_capDraw = false;   // обучение: бой, упёршийся в лимит раундов, считается ничьёй
struct Traj {
    std::vector<TStep> steps;
    int outcome = 2;   // с точки зрения этой стороны: 1 победа, 0 поражение, 2 ничья
};
struct Episode {
    std::vector<Traj> traj;   // одна (сеть против правила-бота) или две (самоигра) траектории
    long long budget = 0, rounds = 0, capped = 0;
    bool selfPlay = false;
    int learnerOutcome = 2;   // исход боя для стороны сети (1 победа, 0 поражение, 2 ничья), даже если траектория не записывалась
};

// Один бой на одних и тех же сборках. selfPlay = false: сеть (ИНТ 16) на одной стороне против правила-бота (ИНТ 10); selfPlay = true: сеть на обеих
// сторонах (ИНТ 16), обе траектории идут в обучение. forceDefault — «пустое» вмешательство без сети; netOff — вмешательства нет вообще.
static Episode runEpisode(const Coef& C, const Net* net, uint64_t seed, bool greedy, bool selfPlay, bool forceDefault, bool netOff) {
    Episode ep;
    ep.selfPlay = selfPlay;
    const long long B = BUDGETS[mix(seed, 5) % 6];
    ep.budget = B;
    rng.seed(seed);
    Boss A, Bo;
    makePair(B, C, A, Bo);
    const int learner = (int)(mix(seed, 6) & 1);
    for (int t = 0; t < 2; t++) {
        Boss& b = t == 0 ? A : Bo;
        b.stats[INT] = (t == learner || selfPlay) ? 16 : 10;
        b.stats[WIS] = 10;
        b.stats[CHA] = 10;
        for (auto& m : b.tmpl) { m.stats[INT] = 10; m.stats[WIS] = 10; m.stats[CHA] = 10; }
    }
    NetCtx ctx;
    ctx.net = net;
    ctx.greedy = greedy;
    ctx.forceDefault = forceDefault;
    ctx.rng.seed(mix(seed, 12345));
    for (int t = 0; t < 2; t++) ctx.side[t].active = !netOff && (t == learner || selfPlay);
    g_ctx = &ctx;
    for (int t = 0; t < 2; t++) g_rlTeam[t] = ctx.side[t].active;
    const long long r0 = g_rounds, c0 = g_capped;
    const int r = fight(A, Bo);
    ep.learnerOutcome = r == 2 ? 2 : (r == learner ? 1 : 0);
    ep.rounds = g_rounds - r0;
    ep.capped = g_capped - c0;
    g_rlTeam[0] = g_rlTeam[1] = false;
    g_ctx = nullptr;
    for (int t = 0; t < 2; t++) {
        if (!ctx.side[t].active) continue;
        Traj tr;
        tr.outcome = r == 2 ? 2 : (r == t ? 1 : 0);
        if (g_capDraw && ep.capped > 0) tr.outcome = 2;   // обучение: победа по очкам на лимите раундов не награждается (не тянуть бой)
        Side& sd = ctx.side[t];
        if (!sd.steps.empty()) {
            const double term = tr.outcome == 1 ? 1.0 : (tr.outcome == 0 ? -1.0 : 0.0);
            sd.steps.back().reward = (float)((sd.prevOpp - g_lastHp[1 - t]) - (sd.prevMy - g_lastHp[t]) + term - g_netTimePen);
        }
        tr.steps = std::move(sd.steps);
        ep.traj.push_back(std::move(tr));
    }
    return ep;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Файл весов
// ---------------------------------------------------------------------------------------------------------------------------------
static bool saveNet(const std::string& fn, const Net& n) {
    std::ofstream f(fn, std::ios::binary);
    if (!f) return false;
    int hd[4] = {n.D, n.E, n.H, n.NA};
    f.write((const char*)hd, sizeof hd);
    f.write((const char*)n.p.data(), n.p.size() * sizeof(float));
    return true;
}
static bool loadNet(const std::string& fn, Net& n) {
    std::ifstream f(fn, std::ios::binary);
    if (!f) return false;
    int hd[4];
    f.read((char*)hd, sizeof hd);
    if (hd[0] != D_IN || hd[1] != EMB || hd[2] != HID || hd[3] != NA) {
        std::fprintf(stderr, "ВНИМАНИЕ: %s — веса другой сети (в файле %d/%d/%d/%d, нужно %d/%d/%d/%d)\n", fn.c_str(), hd[0], hd[1], hd[2], hd[3], D_IN, EMB, HID, NA);
        return false;
    }
    n.init(hd[0], hd[1], hd[2], hd[3], 1);
    f.read((char*)n.p.data(), n.p.size() * sizeof(float));
    return (bool)f;
}

static Coef coefNow() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    return C;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Обучение
// ---------------------------------------------------------------------------------------------------------------------------------
static void softmaxMasked(const float* lg, const bool* m, int n, double* p) {
    float mx = -1e30f;
    for (int j = 0; j < n; j++) if (m[j]) mx = std::max(mx, lg[j]);
    double tot = 0;
    for (int j = 0; j < n; j++) { p[j] = m[j] ? std::exp((double)lg[j] - mx) : 0.0; tot += p[j]; }
    for (int j = 0; j < n; j++) p[j] /= tot;
}

struct EvalResult { double win = 0, draw = 0, loss = 0, meanSteps = 0, meanRounds = 0, capped = 0; int n = 0; };
static EvalResult evaluate(const Coef& C, const Net* net, int N, uint64_t seed, bool greedy, bool selfPlay, bool forceDefault, bool netOff) {
    std::vector<Episode> eps(N);
    parallelFor(N, [&](int i) { eps[i] = runEpisode(C, net, mix(seed, i), greedy, selfPlay, forceDefault, netOff); });
    EvalResult r;
    r.n = N;
    for (auto& e : eps) {
        const int oc = e.traj.empty() ? 2 : e.traj[0].outcome;
        r.win += oc == 1; r.draw += oc == 2; r.loss += oc == 0;
        r.meanSteps += e.traj.empty() ? 0 : e.traj[0].steps.size();
        r.meanRounds += e.rounds; r.capped += e.capped > 0;
    }
    r.win /= N; r.draw /= N; r.loss /= N; r.meanSteps /= N; r.meanRounds /= N; r.capped /= N;
    return r;
}

struct UpdStats { double totSteps = 0, meanRet = 0, entropy = 0, valueLoss = 0; };
// Один шаг A2C по набору траекторий (GAE, обратный проход по времени, Adam). Используется и обучением бойца в одиночку, и командным обучением.
static UpdStats applyUpdate(Net& net, AdamState& adam, std::vector<Traj*>& trajs) {
    const double gamma = 0.99, lambda = 0.95, vcoef = 0.5, beta = 0.01, lr = 5e-4;
    const int nChunks = std::max(1, nThreads() * 2);
    const int NT = (int)trajs.size();
    std::vector<std::vector<float>> adv(NT), ret(NT);
    double sumA = 0, sumA2 = 0, totSteps = 0, meanRet = 0;
    for (int i = 0; i < NT; i++) {
        auto& st = trajs[i]->steps;
        const int T = (int)st.size();
        adv[i].assign(T, 0.f);
        ret[i].assign(T, 0.f);
        double gae = 0, R0 = 0;
        for (int t = T - 1; t >= 0; t--) {
            const double nextV = t + 1 < T ? st[t + 1].value : 0.0;
            const double delta = st[t].reward + gamma * nextV - st[t].value;
            gae = delta + gamma * lambda * gae;
            adv[i][t] = (float)gae;
            ret[i][t] = (float)(gae + st[t].value);
            sumA += gae; sumA2 += gae * gae; totSteps += 1;
            R0 = st[t].reward + gamma * R0;
        }
        meanRet += R0;
    }
    const double mA = sumA / std::max(1.0, totSteps), sA = std::sqrt(std::max(1e-8, sumA2 / std::max(1.0, totSteps) - mA * mA));
    std::vector<std::vector<float>> gbuf(nChunks, std::vector<float>(net.size(), 0.f));
    std::vector<double> entSum(nChunks, 0.0), vlSum(nChunks, 0.0);
    parallelFor(nChunks, [&](int ch) {
        auto& g = gbuf[ch];
        for (int i = ch; i < NT; i += nChunks) {
            auto& st = trajs[i]->steps;
            const int T = (int)st.size();
            std::vector<float> dhn(HID, 0.f), dhp(HID);
            for (int t = T - 1; t >= 0; t--) {
                TStep& s = st[t];
                const double A = (adv[i][t] - mA) / sA;
                float dl[NA];
                for (int j = 0; j < NA; j++) dl[j] = 0;
                const int off[3] = {0, N_STANCE, N_STANCE + N_TGT}, len[3] = {N_STANCE, N_TGT, N_PREF};
                for (int hd = 0; hd < 3; hd++) {
                    double p[NA];
                    softmaxMasked(s.logits + off[hd], s.mask + off[hd], len[hd], p);
                    double H = 0;
                    for (int j = 0; j < len[hd]; j++) if (p[j] > 1e-12) H -= p[j] * std::log(p[j]);
                    entSum[ch] += H;
                    for (int j = 0; j < len[hd]; j++) {
                        if (!s.mask[off[hd] + j]) continue;
                        const double lp = p[j] > 1e-12 ? std::log(p[j]) : -27.0;
                        dl[off[hd] + j] = (float)(A * (p[j] - (j == s.act[hd] ? 1.0 : 0.0)) + beta * p[j] * (lp + H));
                    }
                }
                const double dv = vcoef * (s.value - ret[i][t]);
                vlSum[ch] += 0.5 * (s.value - ret[i][t]) * (s.value - ret[i][t]);
                net.backward(s.st, dl, (float)dv, dhn.data(), g, dhp.data());
                dhn = dhp;
            }
        }
    });
    std::vector<float> G(net.size(), 0.f);
    for (int ch = 0; ch < nChunks; ch++) for (size_t k = 0; k < G.size(); k++) G[k] += gbuf[ch][k];
    const float inv = (float)(1.0 / std::max(1.0, totSteps));
    for (auto& x : G) x *= inv;
    adam.step(net.p, G, lr, 1.0);
    UpdStats us;
    us.totSteps = totSteps;
    us.meanRet = meanRet / std::max(1, NT);
    for (int ch = 0; ch < nChunks; ch++) { us.entropy += entSum[ch]; us.valueLoss += vlSum[ch]; }
    us.entropy /= std::max(1.0, 3 * totSteps);
    us.valueLoss /= std::max(1.0, totSteps);
    return us;

}

static int trainMain(int argc, char** argv) {
    const int iters = argc > 2 ? std::atoi(argv[2]) : 200, N = argc > 3 ? std::atoi(argv[3]) : 1536;
    const uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1;
    const std::string out = argc > 5 ? argv[5] : "net_fighter.bin";
    const int selfPct = std::getenv("DND_SELF") ? std::atoi(std::getenv("DND_SELF")) : 50;   // доля боёв-самоигр в обучении, %
    Coef C = coefNow();
    Net net;
    if (argc > 6) {
        if (!loadNet(argv[6], net)) { std::fprintf(stderr, "не прочитан %s\n", argv[6]); return 1; }
    } else {
        net.init(D_IN, EMB, HID, NA, seed);
        // начальное смещение: «обычная стойка, авто-цель, действие по умолчанию» — сеть стартует как обычный бот и учится отклоняться от него
        net.p[net.obo + 0] += 2.5f;
        net.p[net.obo + N_STANCE + 0] += 2.5f;
        net.p[net.obo + N_STANCE + N_TGT + 0] += 2.5f;
    }
    AdamState adam;
    adam.init(net.size());
    for (int it = 1; it <= iters; it++) {
        std::vector<Episode> eps(N);
        parallelFor(N, [&](int i) {
            const uint64_t sd = mix(mix(seed, it), i);
            eps[i] = runEpisode(C, &net, sd, false, (int)(mix(sd, 7) % 100) < selfPct, false, false);
        });
        std::vector<Traj*> trajs;
        for (auto& e : eps) for (auto& t : e.traj) trajs.push_back(&t);
        double winRule = 0, nRule = 0, roundsSelf = 0, nSelf = 0, cappedSelf = 0;
        for (auto& e : eps) {
            if (e.selfPlay) { roundsSelf += e.rounds; nSelf++; cappedSelf += e.capped > 0; }
            else if (!e.traj.empty()) { winRule += e.traj[0].outcome == 1; nRule++; }
        }
        const UpdStats us = applyUpdate(net, adam, trajs);
        const double totSteps = us.totSteps, meanRet = us.meanRet;
        if (it % 5 == 0 || it == 1) {
            const double ent = us.entropy, vl = us.valueLoss;
            std::printf("итерация %4d: против бота победы %.1f%%; самоигра: %.1f раундов, в лимит %.1f%%; средняя награда %.3f, решений %.0f, энтропия %.2f, ошибка ценности %.3f\n", it,
                        nRule ? 100 * winRule / nRule : 0.0, nSelf ? roundsSelf / nSelf : 0.0, nSelf ? 100 * cappedSelf / nSelf : 0.0, meanRet, totSteps,
                        ent, vl);
            std::fflush(stdout);
        }
        if (it % 50 == 0 || it == iters) {
            saveNet(out, net);
            EvalResult r = evaluate(C, &net, 4000, 777000 + it, true, false, false, false);
            EvalResult s2 = evaluate(C, &net, 2000, 888000 + it, true, true, false, false);
            std::printf("  контроль (жадно): сеть ИНТ 16 против правила-бота ИНТ 10: победы %.1f%% ничьи %.1f%% поражения %.1f%%; самоигра ИНТ 16 против ИНТ 16: %.1f раундов, в лимит %.1f%%\n",
                        100 * r.win, 100 * r.draw, 100 * r.loss, s2.meanRounds, 100 * s2.capped);
            std::fflush(stdout);
        }
    }
    saveNet(out, net);
    return 0;
}

static int evalMain(int argc, char** argv) {
    if (argc < 4) return 1;
    Net net;
    if (!loadNet(argv[2], net)) { std::fprintf(stderr, "не прочитан %s\n", argv[2]); return 1; }
    const int N = std::atoi(argv[3]);
    const bool greedy = !(argc > 4 && std::string(argv[4]) == "sample");
    Coef C = coefNow();
    EvalResult r = evaluate(C, &net, N, 4242, greedy, false, false, false);
    EvalResult s2 = evaluate(C, &net, N, 4343, greedy, true, false, false);
    EvalResult b = evaluate(C, nullptr, N, 4343, greedy, true, false, true);   // без сети: оба ИНТ 16 играют правилами
    std::printf("сеть (ИНТ 16, %s) против правила-бота (ИНТ 10), %d боёв: победы %.2f%% ничьи %.2f%% поражения %.2f%% (±%.1f п.п.), решений на бой %.1f\n", greedy ? "жадно" : "с выбором", N,
                100 * r.win, 100 * r.draw, 100 * r.loss, 100 * 2 * std::sqrt(0.25 / N), r.meanSteps);
    std::printf("самоигра сеть против сети: %.1f раундов на бой, в лимит %.1f%%; для сравнения правила против правил: %.1f раундов, в лимит %.1f%%\n", s2.meanRounds, 100 * s2.capped, b.meanRounds, 100 * b.capped);
    return 0;
}

// Перенос весов сети v20 (вход 72 + 12×10, стоек 9) в формат v21 (вход 72 + 12×13, стоек 10): новые признаки способности
// (по клетке, реакция, число типов урона) получают нулевые веса, новая стойка 9 «снять концентрацию» — нулевые веса и
// смещение −2. На прежних входах сеть решает так же, как раньше. ./net_fighter upgrade20 <старые.bin> <новые.bin>
static int upgrade20Main(int argc, char** argv) {
    if (argc < 4) return 1;
    std::ifstream f(argv[2], std::ios::binary);
    int hd[4];
    f.read((char*)hd, sizeof hd);
    const int oldFA = 10, oldNS = 9;
    if (!f || hd[0] != F_BASE + MAXA * oldFA || hd[1] != EMB || hd[2] != HID || hd[3] != oldNS + N_TGT + N_PREF) {
        std::fprintf(stderr, "%s — не веса v20 (заголовок %d/%d/%d/%d)\n", argv[2], hd[0], hd[1], hd[2], hd[3]);
        return 1;
    }
    GruNet<float> o;
    o.init(hd[0], hd[1], hd[2], hd[3], 1);
    f.read((char*)o.p.data(), o.p.size() * sizeof(float));
    if (!f) return 1;
    Net n;
    n.init(D_IN, EMB, HID, NA, 1);
    std::fill(n.p.begin(), n.p.end(), 0.f);
    auto col = [&](int j) { return j < F_BASE ? j : F_BASE + (j - F_BASE) / oldFA * F_ABIL + (j - F_BASE) % oldFA; };
    for (int e = 0; e < EMB; e++)
        for (int j = 0; j < o.D; j++) n.p[n.oWe + (size_t)e * n.D + col(j)] = o.p[o.oWe + (size_t)e * o.D + j];
    for (size_t k = o.obe; k < o.oWo; k++) n.p[n.obe + (k - o.obe)] = o.p[k];   // эмбеддинг-смещение и вся GRU: размеры те же
    auto row = [&](int r) { return r < oldNS ? r : r + 1; };
    for (int r = 0; r < o.NA; r++) {
        for (int j = 0; j < HID; j++) n.p[n.oWo + (size_t)row(r) * HID + j] = o.p[o.oWo + (size_t)r * HID + j];
        n.p[n.obo + row(r)] = o.p[o.obo + r];
    }
    n.p[n.obo + oldNS] = -2.f;
    for (int j = 0; j < HID; j++) n.p[n.oWv + j] = o.p[o.oWv + j];
    n.p[n.obv] = o.p[o.obv];
    if (!saveNet(argv[3], n)) return 1;
    std::printf("веса v20 %s перенесены в формат v21: %s\n", argv[2], argv[3]);
    return 0;
}

// Новая сеть (prim_net.h): pinit — сохранить случайные начальные веса; psmoke — бои сети (ИНТ 16) против бота (ИНТ 10): сколько
// решений, сколько отказов исполнителя (должно быть 0: маски не пускают недопустимое), сколько боёв дошло до конца.
static int pinitMain(int argc, char** argv) {
    if (argc < 3) return 1;
    pn::PNet n;
    n.init(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1);
    if (!pn::saveP(argv[2], n)) return 1;
    std::printf("случайные веса новой сети записаны: %s (вход %d, параметров %zu)\n", argv[2], pn::D_IN, n.core.p.size() + n.ptr.size());
    return 0;
}
static int psmokeMain(int argc, char** argv) {
    if (argc < 3) return 1;
    pn::PNet net;
    if (!pn::loadP(argv[2], net, true)) return 1;
    const int N = argc > 3 ? std::atoi(argv[3]) : 400;
    Coef C = coefNow();
    std::vector<long long> steps(N), illegal(N);
    std::vector<int> res(N), rounds(N);
    parallelFor(N, [&](int i) {
        const uint64_t sd = mix(4321, i);
        rng.seed(sd);
        Boss A, Bo;
        makePair(BUDGETS[mix(sd, 5) % 6], C, A, Bo);
        const int learner = (int)(mix(sd, 6) & 1);
        (learner ? Bo : A).stats[INT] = 16;
        (learner ? A : Bo).stats[INT] = 10;
        pn::Ctx ctx;
        ctx.net = &net;
        ctx.greedy = false;
        ctx.rng.seed(mix(sd, 99));
        ctx.side[learner].active = true;
        pn::g_pctx = &ctx;
        g_rlTeam[learner] = true;
        const long long r0 = g_rounds;
        const int r = fight(A, Bo);
        g_rlTeam[learner] = false;
        pn::g_pctx = nullptr;
        res[i] = r == 2 ? 2 : (r == learner ? 1 : 0);
        rounds[i] = (int)(g_rounds - r0);
        steps[i] = ctx.steps;
        illegal[i] = ctx.illegal;
    });
    long long st = 0, il = 0, wins = 0, draws = 0, rr = 0;
    for (int i = 0; i < N; i++) st += steps[i], il += illegal[i], wins += res[i] == 1, draws += res[i] == 2, rr += rounds[i];
    std::printf("новая сеть (выбор с вероятностями) против бота, %d боёв: побед %.1f%%, ничьих %.1f%%, раундов %.1f, решений на бой %.1f, отказов исполнителя %lld\n", N,
                100.0 * wins / N, 100.0 * draws / N, (double)rr / N, (double)st / N, il);
    return il == 0 ? 0 : 2;
}

static int checkMain() {
    Coef C = coefNow();
    const int N = 4000;
    std::vector<Episode> a(N), b(N);
    parallelFor(N, [&](int i) { a[i] = runEpisode(C, nullptr, mix(99, i), false, false, false, true); b[i] = runEpisode(C, nullptr, mix(99, i), false, false, true, false); });
    int same = 0;
    for (int i = 0; i < N; i++) same += a[i].learnerOutcome == b[i].learnerOutcome;
    std::printf("«пустое» вмешательство (стойка 0, цель 0, действие по умолчанию) даёт тот же исход, что и обычный бот: %d из %d боёв\n", same, N);
    // v21, №4: ход из примитивов. Бой, где ходы босса записаны примитивами, и тот же бой, где эти примитивы повторяет
    // исполнитель (applyPrim), совпадают до бита: исход, длина, ХП обоих боссов
    std::vector<int> okP(N, 0);
    std::vector<long long> nPrim(N, 0);
    parallelFor(N, [&](int i) {
        const uint64_t sd = mix(77, i);
        const long long B = BUDGETS[mix(sd, 5) % 6];
        auto run = [&](PrimLog& L, double* hp, long long& rounds) {
            rng.seed(sd);
            Boss A, Bo;
            makePair(B, C, A, Bo);
            A.stats[INT] = Bo.stats[INT] = 10;
            const long long r0 = g_rounds;
            g_primLog = &L;
            const int r = fight(A, Bo);
            g_primLog = nullptr;
            rounds = g_rounds - r0;
            hp[0] = g_lastHp[0];
            hp[1] = g_lastHp[1];
            return r;
        };
        PrimLog rec;
        rec.who = (int)(mix(sd, 6) & 1);
        double h1[2], h2[2];
        long long n1 = 0, n2 = 0;
        const int r1 = run(rec, h1, n1);
        for (auto& t : rec.turns) nPrim[i] += (long long)t.size();
        rec.replay = true;
        const int r2 = run(rec, h2, n2);
        okP[i] = r1 == r2 && n1 == n2 && h1[0] == h2[0] && h1[1] == h2[1] && rec.next == rec.turns.size();
    });
    int sameP = 0;
    long long prims = 0;
    for (int i = 0; i < N; i++) sameP += okP[i], prims += nPrim[i];
    std::printf("ход из примитивов: бой с повтором записанных примитивов бота совпадает с исходным до бита: %d из %d боёв (примитивов %.1f на бой)\n", sameP, N, (double)prims / N);
    return same == N && sameP == N ? 0 : 2;
}

}  // namespace cmb
