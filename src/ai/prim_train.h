// ============================================================================
// Обучение сети-бойца v21 (prim_net.h), ТЗ v21 этап 3. Подключается в net_fighter.cc после fighter_train.h.
//
//   1) Копирование бота: ходы правила-бота пишутся примитивами (PrimLog), повторяются, и перед каждым повтором крючок
//      g_replayHook строит наблюдение сети и метку — какой примитив, способность, враг или клетка выбраны. Сеть учится
//      повторять бота (перекрёстная энтропия по всей цепочке решений боя, обратный проход по времени), ценность — на
//      отдачу. Без этого случайная сеть почти не встречает награду.
//   2) PPO (кнут и пряник, без эволюции): награда ±1 за победу или поражение (ничья, побег за арену и отрыв от погони —
//      поражение, решение владельца), промежуточная — потенциальная (γΦ(s') − Φ(s), Φ = доля своих ХП − доля ХП врага),
//      маленький штраф за решение. Соперники — пул: правило-бот и прошлые версии сети. Бюджет — любой от 200 до 6400
//      (логарифмически равномерно), карта — новая в каждом бою. Сбор опыта во многих потоках одной сетью.
//   Проверка на отложенных боях (свои зёрна, сеть выбирает жадно), контрольные точки, «странные победы» (победа побегом
//   врага, в лимит раундов, при ХП < 10%) — в файл с зерном для реплея. Веса и состояние Adam сохраняются по ходу:
//   падение не стоит часов работы, обучение продолжается с места.
//
//   ./net_fighter pbc <боёв> <проходов> <зерно> <веса.bin> [стартовые.bin]              — копирование бота
//   ./net_fighter ptrain <итераций> <боёв в итерации> <зерно> <веса.bin> [стартовые.bin]  — PPO (продолжает с <веса>.state)
//   ./net_fighter peval <веса.bin> <боёв>                                                — против бота на отложенных боях
// ============================================================================
#pragma once
#include <map>
#include <mutex>

namespace pt {
using pn::Decision;
using pn::PNet;

struct Traj {
    std::vector<Decision> d;
    int outcome = 0;   // 1 победа, 0 нет (поражение, ничья, побег)
};
struct Hyper {
    // энтропия 0.1 (решение владельца 2026-10-27, было 0.01: сеть рано переставала пробовать новое); лучшая версия
    // по отложенным боям сохраняется отдельно, так что «забыть лучшее» нельзя
    // штраф за время — за раунд боя, а не за решение (решение владельца 2026-10-28: сети против сети тянули бой, а
    // штраф за решение затяжку не наказывал); DND_ROUNDPEN
    double gamma = 0.99, lambda = 0.95, clip = 0.2, vcoef = 0.5, ent = 0.1, lr = 3e-4, timePen = 0.0, roundPen = 0.005;
    int epochs = 3;
};

static long long budgetOf(uint64_t sd) {   // бюджет 200…6400, логарифмически равномерно
    const double u = (double)(mix(sd, 5) >> 11) * (1.0 / 9007199254740992.0);
    return (long long)std::llround(200.0 * std::pow(2.0, 5.0 * u));
}
static void prepBosses(Boss& A, Boss& Bo, int learner, int oppInt) {
    for (int t = 0; t < 2; t++) {
        Boss& b = t == 0 ? A : Bo;
        b.stats[INT] = t == learner ? 16 : oppInt;
        b.stats[WIS] = 10;
        b.stats[CHA] = 10;
        for (auto& m : b.tmpl) { m.stats[INT] = 10; m.stats[WIS] = 10; m.stats[CHA] = 10; }
    }
}
// «Странная победа»: враг сбежал или оторвался, бой упёрся в лимит раундов, победа при ХП < 10%
static std::string strangeWin(int r, int learner, uint64_t seed, long long B) {
    if (r != learner) return "";
    const int opp = 1 - learner;
    const FightInfo& fi = g_fightInfo;
    std::string why;
    if (fi.reason >= 2) why = fi.reason == 2 ? "враг вышел за арену" : (fi.reason == 3 ? "враг оторвался от погони" : "враг выбился из сил");
    else if (fi.rounds >= g_maxRounds) why = "лимит раундов";
    else if (g_lastHp[learner] < 0.1) why = "победа при ХП < 10%";
    (void)opp;
    if (why.empty()) return "";
    char buf[200];
    std::snprintf(buf, sizeof buf, "зерно %llu бюджет %lld: %s", (unsigned long long)seed, B, why.c_str());
    return buf;
}

// ---------------------------------------------------------------------------------------------------------------------
// Эпизод: сеть (ИНТ 16) против бота (ИНТ 10) или против сети из пула (ИНТ 16)
// ---------------------------------------------------------------------------------------------------------------------
struct Episode {
    Traj tr;
    int result = 0;   // 1 победа сети
    long long rounds = 0, steps = 0, illegal = 0;
    long long turns = 0, movedTurns = 0, capHits = 0, forcedEnds = 0, endBlocked = 0;   // метрики хода (шаг 4)
    std::string strange;
    Traj tr2;   // бой сети против самой себя: решения второй стороны (тоже учатся)
    int bcat = 0;   // чьи боссы: 0 оба от генератора, 1 у сети босс Сборщика, 2 у соперника босс Сборщика, 3 оба от Сборщиков
};
// победы по группам боёв (для журнала): соперник — бот / прошлые версии / сама; боссы — по Episode::bcat
struct WinSplit {
    double w[4] = {}, n[4] = {};
    void add(int k, int res) { w[k] += res, n[k]++; }
    std::string str(int k) const {
        char b[48];
        if (n[k] > 0) std::snprintf(b, sizeof b, "%.1f%% (%d)", 100 * w[k] / n[k], (int)n[k]);
        else std::snprintf(b, sizeof b, "— (0)");
        return b;
    }
    std::string bosses() const {   // одна строка по видам боссов
        return "обычные " + str(0) + ", у сети босс Сборщика " + str(1) + ", у соперника босс Сборщика " + str(2) + ", оба от Сборщиков " + str(3);
    }
};
static Episode runEpisode(const Coef& C, const PNet& net, const PNet* opp, uint64_t seed, bool greedy, bool record, const Hyper& hp) {
    Episode ep;
    const long long B = budgetOf(seed);
    rng.seed(seed);
    Boss A, Bo;
    int bside = -1;
    const int kind = makeCalibPair(B, C, A, Bo, bside);   // этап 5б: с DND_CALIB_BLD часть боссов собирают сети-Сборщики (иначе как makePair)
    const int learner = (int)(mix(seed, 6) & 1);
    ep.bcat = kind == 2 ? 3 : (kind == 1 ? (bside == learner ? 1 : 2) : 0);
    prepBosses(A, Bo, learner, opp ? 16 : 10);
    pn::Ctx ctx;
    ctx.net = &net;
    ctx.greedy = greedy;
    ctx.rng.seed(mix(seed, 12345));
    ctx.gamma = hp.gamma;
    ctx.timePen = hp.timePen;
    ctx.roundPen = hp.roundPen;
    ctx.side[learner].active = true;
    ctx.side[learner].record = record;
    const bool self = opp == &net;   // сеть против самой себя (текущей версии): учатся обе стороны
    if (opp) {
        ctx.side[1 - learner].active = true;
        ctx.side[1 - learner].net = opp;
        ctx.side[1 - learner].record = record && self;
    }
    pn::g_pctx = &ctx;
    g_rlTeam[learner] = true;
    g_rlTeam[1 - learner] = opp != nullptr;
    const long long r0 = g_rounds;
    const int r = fight(A, Bo);
    g_rlTeam[0] = g_rlTeam[1] = false;
    pn::g_pctx = nullptr;
    ep.result = r == learner ? 1 : 0;
    ep.rounds = g_rounds - r0;
    ep.steps = ctx.steps;
    ep.illegal = ctx.illegal;
    ep.turns = ctx.turns;
    ep.movedTurns = ctx.movedTurns;
    ep.capHits = ctx.capHits;
    ep.forcedEnds = ctx.forcedEnds;
    ep.endBlocked = ctx.endBlocked;
    ep.strange = strangeWin(r, learner, seed, B);
    if (record) {
        pn::Side& sd = ctx.side[learner];
        ep.tr.d = std::move(sd.traj);
        ep.tr.outcome = ep.result;
        if (!ep.tr.d.empty()) ep.tr.d.back().reward += (float)((ep.result ? 1.0 : -1.0) - sd.prevPhi);   // Φ(конец) = 0, исход ±1
        if (self) {   // вторая сторона: ничья (в том числе предел раундов) — поражение и для неё
            pn::Side& s2 = ctx.side[1 - learner];
            ep.tr2.d = std::move(s2.traj);
            ep.tr2.outcome = r == 1 - learner ? 1 : 0;
            if (!ep.tr2.d.empty()) ep.tr2.d.back().reward += (float)((ep.tr2.outcome ? 1.0 : -1.0) - s2.prevPhi);
        }
    }
    return ep;
}

// ---------------------------------------------------------------------------------------------------------------------
// Копирование бота: решение бота → метка решения сети
// ---------------------------------------------------------------------------------------------------------------------
struct BCSink {
    Traj tr;
    int team = 0;
    double prevPhi = 0;
    bool hasPrev = false;
    double gamma = 0.99;
    long long skipped = 0, skipKind[PR_N] = {};
};
static thread_local BCSink* g_bc = nullptr;
static long long g_skipKind[PR_N] = {};   // сводка непредставимых меток по видам (для отчёта)
static std::mutex g_skipMu;
// клетка (x, y) в списке кандидатов; нет — добавляется (или заменяет последнюю), чтобы метка была представима
static int cellIndex(const World& w, const Fighter& me, const Fighter* e, std::vector<pn::Cand>& cs, int x, int y) {
    for (size_t i = 0; i < cs.size(); i++)
        if (cs[i].x == x && cs[i].y == y) return (int)i;
    pn::Cand cd;
    cd.x = x;
    cd.y = y;
    pn::cellFeat(w, me, e, x, y, 5 * std::max(std::abs(x - me.x), std::abs(y - me.y)), cd.f);
    if ((int)cs.size() < pn::MAXC) cs.push_back(cd);
    else cs.back() = cd;
    return (int)(std::find_if(cs.begin(), cs.end(), [&](const pn::Cand& c) { return c.x == x && c.y == y; }) - cs.begin());
}
static void bcHook(World& w, int idx, const TurnState& ts, const Prim* p) {
    BCSink* s = g_bc;
    if (!s || idx >= 2) return;
    pn::Obs o;
    pn::buildObs(w, idx, ts, o);
    if (pn::onlyEnd(o)) return;   // легален только «конец хода»: его делает движок, в обучение не идёт (как у сети)
    const Fighter& me = w.f[idx];
    const Fighter* near = o.enemy[0] >= 0 ? &w.f[o.enemy[0]] : nullptr;
    Decision d;
    const bool ok = pn::labelOf(w, idx, o, p, d, [&](std::vector<pn::Cand>& cs, int x, int y) { return cellIndex(w, me, near, cs, x, y); });
    if (!ok) {   // непредставимо (например, бот закончил пустой ход — сети это запрещено)
        s->skipped++;
        s->skipKind[p ? p->kind : PR_END]++;
        return;
    }
    const double phi = pn::potential(w, me.team);
    if (s->hasPrev && !s->tr.d.empty()) s->tr.d.back().reward += (float)(s->gamma * phi - s->prevPhi);
    s->prevPhi = phi;
    s->hasPrev = true;
    d.x = std::move(o.x);
    s->tr.d.push_back(std::move(d));
}
// бой бота против бота: ходы стороны learner пишутся примитивами, затем повтор с крючком — цепочка меток
static Traj bcEpisode(const Coef& C, uint64_t seed, long long& skipped) {
    const long long B = budgetOf(seed);
    const int learner = (int)(mix(seed, 6) & 1);
    PrimLog L;
    L.who = learner;
    auto run = [&]() {
        rng.seed(seed);
        Boss A, Bo;
        makePair(B, C, A, Bo);
        prepBosses(A, Bo, learner, 10);
        (learner ? Bo : A).stats[INT] = 10;   // учитель — правило-бот среднего ИНТ
        g_primLog = &L;
        const int r = fight(A, Bo);
        g_primLog = nullptr;
        return r;
    };
    run();
    BCSink sink;
    sink.team = learner;
    g_bc = &sink;
    g_replayHook = bcHook;
    L.replay = true;
    const int r = run();
    g_replayHook = nullptr;
    g_bc = nullptr;
    sink.tr.outcome = r == learner ? 1 : 0;
    if (!sink.tr.d.empty()) sink.tr.d.back().reward += (float)((sink.tr.outcome ? 1.0 : -1.0) - sink.prevPhi);
    skipped += sink.skipped;
    {
        std::lock_guard<std::mutex> lk(g_skipMu);
        for (int k = 0; k < PR_N; k++) g_skipKind[k] += sink.skipKind[k];
    }
    return std::move(sink.tr);
}

// ---------------------------------------------------------------------------------------------------------------------
// Обновление: обратный проход по времени по каждой цепочке; bc — копирование (−log p метки), иначе PPO
// ---------------------------------------------------------------------------------------------------------------------
struct Grad {
    std::vector<float> core, ptr;
};
struct UpdStats {
    double loss = 0, ent = 0, vloss = 0, clipFrac = 0, acc = 0;
    long long steps = 0;
};
static UpdStats update(PNet& net, AdamState& aCore, AdamState& aPtr, std::vector<Traj*>& trs, const Hyper& hp, bool bc) {
    const int NT = (int)trs.size();
    // отдача и преимущество (GAE) по старым ценностям
    std::vector<std::vector<float>> adv(NT), ret(NT);
    double sA = 0, sA2 = 0, nS = 0;
    for (int i = 0; i < NT; i++) {
        auto& d = trs[i]->d;
        const int T = (int)d.size();
        adv[i].assign(T, 0.f);
        ret[i].assign(T, 0.f);
        double gae = 0, R = 0;
        for (int t = T - 1; t >= 0; t--) {
            const double nv = t + 1 < T ? d[t + 1].value : 0.0;
            gae = d[t].reward + hp.gamma * nv - d[t].value + hp.gamma * hp.lambda * gae;
            R = d[t].reward + hp.gamma * R;
            adv[i][t] = (float)gae;
            ret[i][t] = bc ? (float)R : (float)(gae + d[t].value);
            sA += gae;
            sA2 += gae * gae;
            nS += 1;
        }
    }
    const double mA = sA / std::max(1.0, nS), sdA = std::sqrt(std::max(1e-8, sA2 / std::max(1.0, nS) - mA * mA));
    UpdStats us;
    us.steps = (long long)nS;
    const int epochs = bc ? 1 : hp.epochs;
    for (int ep = 0; ep < epochs; ep++) {
        // куски градиента — постоянное число (не от числа потоков): порядок сложения, а значит и веса, одинаковы при любом
        // DND_THREADS (ТЗ: воспроизводимость по зерну)
        const int nCh = std::max(1, std::min(NT, 32));
        std::vector<Grad> G(nCh);
        std::vector<UpdStats> part(nCh);
        parallelFor(nCh, [&](int ch) {
            Grad& g = G[ch];
            g.core.assign(net.core.p.size(), 0.f);
            g.ptr.assign(net.ptr.size(), 0.f);
            UpdStats& u = part[ch];
            for (int i = ch; i < NT; i += nCh) {
                auto& d = trs[i]->d;
                const int T = (int)d.size();
                std::vector<typename GruNet<float>::Step> st(T);
                std::vector<std::vector<float>> lg(T, std::vector<float>(pn::NFIX));
                std::vector<float> val(T), h(net.H, 0.f);
                for (int t = 0; t < T; t++) {
                    net.core.forward(d[t].x.data(), h.data(), st[t], lg[t].data(), val[t]);
                    h = st[t].h;
                }
                std::vector<float> dhn(net.H, 0.f), dhp(net.H);
                for (int t = T - 1; t >= 0; t--) {
                    float lp = pn::decisionLogp<float>(net, d[t], st[t].h.data(), lg[t].data(), 0.f, nullptr, nullptr, nullptr);
                    float gl;
                    if (bc) {
                        gl = -1.f;   // потеря −log p метки
                        u.loss -= lp;
                        u.acc += std::exp(lp);
                    } else {
                        const double A = (adv[i][t] - mA) / sdA, ratio = std::exp((double)lp - d[t].logp);
                        const bool clipped = (A > 0 && ratio > 1 + hp.clip) || (A < 0 && ratio < 1 - hp.clip);
                        gl = clipped ? 0.f : (float)(-A * ratio);
                        u.clipFrac += clipped;
                        u.loss += -std::min(ratio * A, std::clamp(ratio, 1 - hp.clip, 1 + hp.clip) * A);
                    }
                    std::vector<float> dlg(pn::NFIX, 0.f), dh(dhn);
                    float ent = 0;
                    pn::decisionLogp<float>(net, d[t], st[t].h.data(), lg[t].data(), gl, dlg.data(), &g.ptr, dh.data(), bc ? 0.f : (float)hp.ent, &ent);
                    u.ent += ent;
                    const float dv = (float)(hp.vcoef * (val[t] - ret[i][t]));
                    u.vloss += 0.5 * (val[t] - ret[i][t]) * (val[t] - ret[i][t]);
                    net.core.backward(st[t], dlg.data(), dv, dh.data(), g.core, dhp.data());
                    dhn = dhp;
                }
            }
        });
        Grad tot;
        tot.core.assign(net.core.p.size(), 0.f);
        tot.ptr.assign(net.ptr.size(), 0.f);
        for (int ch = 0; ch < nCh; ch++) {
            for (size_t k = 0; k < tot.core.size(); k++) tot.core[k] += G[ch].core[k];
            for (size_t k = 0; k < tot.ptr.size(); k++) tot.ptr[k] += G[ch].ptr[k];
            us.loss += part[ch].loss;
            us.ent += part[ch].ent;
            us.vloss += part[ch].vloss;
            us.clipFrac += part[ch].clipFrac;
            us.acc += part[ch].acc;
        }
        const float inv = (float)(1.0 / std::max(1.0, nS));
        for (auto& v : tot.core) v *= inv;
        for (auto& v : tot.ptr) v *= inv;
        aCore.step(net.core.p, tot.core, hp.lr, 1.0);
        aPtr.step(net.ptr, tot.ptr, hp.lr, 1.0);
    }
    const double div = std::max(1.0, nS * epochs);
    us.loss /= div;
    us.ent /= div;
    us.vloss /= div;
    us.clipFrac /= div;
    us.acc /= div;
    return us;
}

// ---------------------------------------------------------------------------------------------------------------------
// Сохранение состояния обучения (веса, Adam, итерация, пул): падение не стоит часов работы
// ---------------------------------------------------------------------------------------------------------------------
static bool saveState(const std::string& fn, const AdamState& a, const AdamState& b, int iter) {
    std::ofstream f(fn, std::ios::binary);
    if (!f) return false;
    const long long hd[4] = {iter, (long long)a.m.size(), (long long)b.m.size(), a.t};
    f.write((const char*)hd, sizeof hd);
    for (const AdamState* s : {&a, &b}) {
        f.write((const char*)s->m.data(), s->m.size() * sizeof(double));
        f.write((const char*)s->v.data(), s->v.size() * sizeof(double));
    }
    return (bool)f;
}
static bool loadState(const std::string& fn, AdamState& a, AdamState& b, int& iter) {
    std::ifstream f(fn, std::ios::binary);
    if (!f) return false;
    long long hd[4];
    f.read((char*)hd, sizeof hd);
    if (!f || hd[1] != (long long)a.m.size() || hd[2] != (long long)b.m.size()) return false;
    for (AdamState* s : {&a, &b}) {
        f.read((char*)s->m.data(), s->m.size() * sizeof(double));
        f.read((char*)s->v.data(), s->v.size() * sizeof(double));
        s->t = hd[3];
    }
    iter = (int)hd[0];
    return (bool)f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Проверка на отложенных боях: свои зёрна (не встречаются в обучении), сеть выбирает жадно
// ---------------------------------------------------------------------------------------------------------------------
struct EvalRes {
    double win = 0, rounds = 0, steps = 0;
    long long illegal = 0;
    long long turns = 0, movedTurns = 0, capHits = 0, forcedEnds = 0, endBlocked = 0;
    int n = 0;
    std::vector<std::string> strange;
    WinSplit byBoss;
};
static EvalRes evaluate(const Coef& C, const PNet& net, int N, uint64_t base) {
    std::vector<Episode> eps(N);
    Hyper hp;
    parallelFor(N, [&](int i) { eps[i] = runEpisode(C, net, nullptr, mix(base, i), true, false, hp); });
    EvalRes r;
    r.n = N;
    for (auto& e : eps) {
        r.win += e.result;
        r.rounds += e.rounds;
        r.steps += e.steps;
        r.illegal += e.illegal;
        r.turns += e.turns;
        r.movedTurns += e.movedTurns;
        r.capHits += e.capHits;
        r.forcedEnds += e.forcedEnds;
        r.endBlocked += e.endBlocked;
        r.byBoss.add(e.bcat, e.result);
        if (!e.strange.empty()) r.strange.push_back(e.strange);
    }
    r.win /= N;
    r.rounds /= N;
    r.steps /= N;
    return r;
}
static const uint64_t HELD_OUT = 0xE7A1D0E7ULL;   // зерно отложенных боёв

static void logStrange(const std::string& fn, int iter, const std::vector<std::string>& v) {
    if (v.empty()) return;
    std::ofstream f(fn, std::ios::app);
    for (auto& s : v) f << "итерация " << iter << ": " << s << "\n";
}

static int bcMain(int argc, char** argv) {
    if (argc < 6) return 1;
    const int fights = std::atoi(argv[2]), passes = std::atoi(argv[3]);
    const uint64_t seed = std::strtoull(argv[4], nullptr, 10);
    const std::string out = argv[5];
    PNet net;
    if (argc > 6) {
        if (!pn::loadP(argv[6], net, true)) return 1;
    } else
        net.init(seed);
    AdamState aC, aP;
    aC.init(net.core.p.size());
    aP.init(net.ptr.size());
    Coef C = cmb::coefNow();
    Hyper hp;
    hp.lr = 1e-3;
    const int chunk = 256;
    for (int pass = 0; pass < passes; pass++) {
        double acc = 0, loss = 0;
        long long steps = 0, skipped = 0;
        for (int b0 = 0; b0 < fights; b0 += chunk) {
            const int n = std::min(chunk, fights - b0);
            std::vector<Traj> trs(n);
            std::vector<long long> sk(n, 0);
            parallelFor(n, [&](int i) { trs[i] = bcEpisode(C, mix(mix(seed, 1000 + b0 + i), 77), sk[i]); });
            std::vector<Traj*> ptrs;
            for (auto& t : trs)
                if (!t.d.empty()) ptrs.push_back(&t);
            for (auto v : sk) skipped += v;
            const UpdStats us = update(net, aC, aP, ptrs, hp, true);
            acc += us.acc * us.steps;
            loss += us.loss * us.steps;
            steps += us.steps;
        }
        pn::saveP(out, net);
        const EvalRes ev = evaluate(C, net, 1000, HELD_OUT);
        std::printf("копирование бота, проход %d/%d: решений %lld (непредставимых меток %lld), потеря %.3f, вероятность метки %.2f; против бота (жадно, отложенные бои): победы %.1f%%, раундов %.1f, решений %.1f, отказов %lld\n",
                    pass + 1, passes, steps, skipped, loss / std::max(1LL, steps), acc / std::max(1LL, steps), 100 * ev.win, ev.rounds, ev.steps, ev.illegal);
        std::string sk;
        for (int k = 0; k < PR_N; k++)
            if (g_skipKind[k]) sk += std::string(sk.empty() ? "" : ", ") + PRN[k] + " " + std::to_string(g_skipKind[k]);
        if (!sk.empty()) std::printf("  непредставимые метки по видам (нарастающим итогом): %s\n", sk.c_str());
        std::fflush(stdout);
    }
    return 0;
}

static int trainMain(int argc, char** argv) {
    if (argc < 6) return 1;
    const int iters = std::atoi(argv[2]), N = std::atoi(argv[3]);
    const uint64_t seed = std::strtoull(argv[4], nullptr, 10);
    const std::string out = argv[5];
    PNet net;
    if (!pn::loadP(argc > 6 ? argv[6] : out, net, true)) {
        if (argc > 6) return 1;
        net.init(seed);
    }
    AdamState aC, aP;
    aC.init(net.core.p.size());
    aP.init(net.ptr.size());
    int it0 = 0;
    if (loadState(out + ".state", aC, aP, it0)) {
        pn::loadP(out, net, true);
        std::printf("продолжаю обучение с итерации %d (%s)\n", it0, (out + ".state").c_str());
    }
    loadCalibBld();   // этап 5б: боссы сетей-Сборщиков в обучении (DND_CALIB_BLD — доля пар)
    if (g_cb.share > 0) std::printf("обучение на смеси: %.0f%% пар с боссами сетей-Сборщиков\n", 100 * g_cb.share);
    Coef C = cmb::coefNow();
    Hyper hp;
    if (const char* e = std::getenv("DND_LR")) hp.lr = std::atof(e);
    if (const char* e = std::getenv("DND_ENT")) hp.ent = std::atof(e);
    if (const char* e = std::getenv("DND_ROUNDPEN")) hp.roundPen = std::atof(e);
    // соперники (решение владельца 2026-10-28: сети учатся друг против друга): доли «сама против себя, пул прошлых
    // версий, правило-бот»; DND_MIX="0.4,0.4,0.2". Пул пуст — его доля идёт на бои против себя
    double mixSelf = 0.4, mixPool = 0.4;
    if (const char* e = std::getenv("DND_MIX")) std::sscanf(e, "%lf,%lf", &mixSelf, &mixPool);
    auto oppKind = [&](uint64_t sd, bool havePool) {   // 0 бот, 1 пул, 2 сама
        const double u = (double)(mix(sd, 9) >> 11) * (1.0 / 9007199254740992.0);
        if (u < mixSelf) return 2;
        if (u < mixSelf + mixPool) return havePool ? 1 : 2;
        return 0;
    };
    // ранняя остановка на плато: проверка раз в 50 итераций; если лучший результат не вырос хотя бы на 0.5 п.п. за
    // DND_PATIENCE проверок подряд (по умолчанию 6 = 300 итераций) — обучение заканчивается
    const char* pe = std::getenv("DND_PATIENCE");
    const int patience = pe ? std::max(1, std::atoi(pe)) : 6;
    std::printf("обучение: энтропия %.3f, скорость %.0e, штраф %.4f за раунд, соперники: сама %.0f%%, пул %.0f%%, бот %.0f%%; до %d итераций, остановка после %d проверок без роста\n",
                hp.ent, hp.lr, hp.roundPen, 100 * mixSelf, 100 * mixPool, 100 * (1 - mixSelf - mixPool), iters, patience);
    std::vector<PNet> pool;   // прошлые версии сети — соперники
    for (int k = 0; k < 8; k++) {
        PNet p;
        if (pn::loadP(out + ".pool" + std::to_string(k), p, false)) pool.push_back(p);
    }
    double best = -1, bestMark = -1;   // bestMark — лучший результат, от которого считается плато (рост ≥ 0.5 п.п.)
    int markIt = it0;
    {
        std::ifstream bf(out + ".best");
        if (bf) bf >> best >> bestMark >> markIt;
        if (bestMark < 0) bestMark = best;
    }
    WinSplit winOpp, winBoss;   // накопление между проверками: по соперникам (0 бот, 1 прошлые версии, 2 сама) и по боссам
    pn::g_altOn = true;   // учится и «выше/ниже» (у перенесённых весов оно было выключено)
    for (int it = it0 + 1; it <= iters; it++) {
        g_worldEpoch = (long long)it * N / 500;   // новый мир каждые 500 обучающих боёв
        std::vector<Episode> eps(N);
        parallelFor(N, [&](int i) {
            const uint64_t sd = mix(mix(seed, it), i);
            const int k = oppKind(sd, !pool.empty());
            const PNet* opp = k == 2 ? &net : (k == 1 ? &pool[mix(sd, 10) % pool.size()] : nullptr);
            eps[i] = runEpisode(C, net, opp, sd, false, true, hp);
        });
        std::vector<Traj*> trs;
        WinSplit opp1;   // эта итерация, по соперникам
        double winBot = 0, nBot = 0, rounds = 0, selfRounds = 0, nSelf = 0, selfCap = 0;
        long long illegal = 0;
        std::vector<std::string> strange;
        for (auto& e : eps) {
            if (!e.tr.d.empty()) trs.push_back(&e.tr);
            if (!e.tr2.d.empty()) trs.push_back(&e.tr2);
            rounds += e.rounds;
            illegal += e.illegal;
            if (!e.strange.empty()) strange.push_back(e.strange);
        }
        for (int i = 0; i < N; i++) {
            const int k = oppKind(mix(mix(seed, it), i), !pool.empty());
            if (k == 0) winBot += eps[i].result, nBot++;
            opp1.add(k == 0 ? 0 : (k == 1 ? 1 : 2), eps[i].result);
            if (k != 2) winBoss.add(eps[i].bcat, eps[i].result);   // против себя счёт всегда около половины — в разбивку по боссам не идёт
            if (k == 2) selfRounds += eps[i].rounds, nSelf++, selfCap += eps[i].rounds >= g_maxRounds;
        }
        const UpdStats us = update(net, aC, aP, trs, hp, false);
        logStrange(out + ".strange.txt", it, strange);
        for (int k = 0; k < 3; k++) winOpp.w[k] += opp1.w[k], winOpp.n[k] += opp1.n[k];
        std::printf("итерация %4d: против бота %.1f%% (%d боёв), против прошлых версий %s, раундов %.1f, сама с собой раундов %.1f (до предела %.1f%%), решений %lld, потеря %.3f, энтропия %.2f, ошибка ценности %.3f, срезано %.0f%%, отказов %lld, странных побед %zu\n",
                    it, nBot ? 100 * winBot / nBot : 0.0, (int)nBot, opp1.str(1).c_str(), rounds / N, nSelf ? selfRounds / nSelf : 0.0, nSelf ? 100 * selfCap / nSelf : 0.0, us.steps, us.loss, us.ent, us.vloss,
                    100 * us.clipFrac, illegal, strange.size());
        std::fflush(stdout);
        if (it % 10 == 0 || it == iters) {   // контрольная точка: веса, состояние Adam, пул
            pn::saveP(out, net);
            saveState(out + ".state", aC, aP, it);
        }
        if (it % 25 == 0) {
            const int slot = (int)(pool.size() < 8 ? pool.size() : (it / 25) % 8);
            if ((int)pool.size() < 8) pool.push_back(net); else pool[slot] = net;
            pn::saveP(out + ".pool" + std::to_string(slot), net);
        }
        if (it % 50 == 0 || it == iters) {
            const EvalRes ev = evaluate(C, net, 2000, HELD_OUT);
            logStrange(out + ".strange.txt", it, ev.strange);
            {   // затяжка: 500 отложенных боёв сети против самой себя (жадно) — раунды и доля до предела
                std::vector<Episode> se(500);
                Hyper h0 = hp;
                parallelFor(500, [&](int i) { se[i] = runEpisode(C, net, &net, mix(HELD_OUT ^ 0x5E1F, i), true, false, h0); });
                double rr = 0, cap = 0;
                for (auto& e : se) rr += e.rounds, cap += e.rounds >= g_maxRounds;
                std::printf("  проверка «сама с собой» (500 отложенных боёв, жадно): раундов %.1f, до предела %d раундов %.1f%%\n", rr / 500, g_maxRounds, 100 * cap / 500);
            }
            std::printf("  проверка (отложенные бои, жадно, против бота ИНТ 10): победы %.1f%%, раундов %.1f, решений на бой %.1f, отказов %lld; ходов с перемещением %.1f%%, упоров в предел %lld, «конец хода» закрыт %lld раз\n",
                        100 * ev.win, ev.rounds, ev.steps, ev.illegal, 100.0 * ev.movedTurns / std::max(1LL, ev.turns), ev.capHits, ev.endBlocked);
            std::printf("    по боссам: %s\n", ev.byBoss.bosses().c_str());
            std::printf("  обучающие бои с прошлой проверки: против бота %s, против прошлых версий %s, сама с собой %s\n",
                        winOpp.str(0).c_str(), winOpp.str(1).c_str(), winOpp.str(2).c_str());
            std::printf("    по боссам (бои с ботом и прошлыми версиями): %s\n", winBoss.bosses().c_str());
            winOpp = WinSplit(), winBoss = WinSplit();
            if (ev.win > best) {
                best = ev.win;
                pn::saveP(out + ".best.bin", net);
                std::printf("  лучшая версия: %s\n", (out + ".best.bin").c_str());
            }
            if (ev.win >= bestMark + 0.005) bestMark = ev.win, markIt = it;
            std::ofstream(out + ".best") << best << " " << bestMark << " " << markIt;
            std::fflush(stdout);
            if (it - markIt >= patience * 50) {
                std::printf("плато: %d итераций без роста лучшего результата на 0.5 п.п. (лучший %.1f%%) — обучение остановлено\n", it - markIt, 100 * best);
                break;
            }
        }
    }
    return 0;
}

// Реплей боя сети (ИНТ 16) против бота (ИНТ 10) в просмотрщик: каждый шаг сети — кадр с её решением, туман войны её
// стороны (враги, которых она не знает, полупрозрачные). ./net_fighter preplay <веса> <бюджет> <зерно> <файл.html>
static int replayMain(int argc, char** argv) {
    if (argc < 6) return 1;
    PNet net;
    if (!pn::loadP(argv[2], net, true)) return 1;
    const long long B = std::atoll(argv[3]);
    const uint64_t seed = std::strtoull(argv[4], nullptr, 10);
    Coef C = cmb::coefNow();
    rng.seed(seed);
    Boss A, Bo;
    makePair(B, C, A, Bo);
    prepBosses(A, Bo, 0, 10);
    pn::Ctx ctx;
    ctx.net = &net;
    ctx.greedy = true;
    ctx.side[0].active = true;
    pn::g_pctx = &ctx;
    g_rlTeam[0] = true;
    pn::g_netVerbose = true;
    g_fogTeam = 0;
    std::vector<std::string> frames;
    std::string mapJson;
    const int r = fight(A, Bo, &frames, &mapJson);
    g_fogTeam = -1;
    pn::g_netVerbose = false;
    g_rlTeam[0] = false;
    pn::g_pctx = nullptr;
    const std::string st = strangeWin(r, 0, seed, B);
    std::printf("бой: %s за %d раундов, решений сети %lld%s\n", r == 0 ? "победа сети" : (r == 1 ? "победа бота" : "ничья"), g_fightInfo.rounds, ctx.steps,
                st.empty() ? "" : (" — странная победа: " + st).c_str());
    writeViewerHTML(argv[5], A, Bo, C, (double)B, std::string("нейросеть-боец v21 (") + argv[2] + ")", "правило-бот ИНТ 10", (unsigned)seed, mapJson, frames);
    return 0;
}

// Честная проверка: одна пара боссов и одна карта, бой дважды — сеть за первого босса, затем за второго (бот — за другого);
// у обоих ИНТ 16 (у бота ИНТ 16 играет как ИНТ 10, а характеристика одинакова). Удача сборки сокращается: бот на месте
// сети дал бы ровно 50%. ./net_fighter pfair <веса> <пар>
static int fairMain(int argc, char** argv) {
    if (argc < 4) return 1;
    PNet net;
    if (!pn::loadP(argv[2], net, true)) return 1;
    const int N = std::atoi(argv[3]);
    Coef C = cmb::coefNow();
    std::vector<std::array<int, 2>> res(N);
    std::vector<long long> bud(N);
    parallelFor(N, [&](int i) {
        const uint64_t sd = mix(HELD_OUT ^ 0x5A5A, i);
        bud[i] = budgetOf(sd);
        for (int side = 0; side < 2; side++) {
            rng.seed(sd);
            Boss A, Bo;
            makePair(bud[i], C, A, Bo);
            prepBosses(A, Bo, side, 16);   // у обоих ИНТ 16
            pn::Ctx ctx;
            ctx.net = &net;
            ctx.greedy = true;
            ctx.side[side].active = true;
            pn::g_pctx = &ctx;
            g_rlTeam[side] = true;
            const int r = fight(A, Bo);
            g_rlTeam[side] = false;
            pn::g_pctx = nullptr;
            res[i][side] = r == 2 ? 0 : (r == side ? 1 : -1);   // 1 сеть победила, −1 бот, 0 ничья
        }
    });
    double pts = 0;
    int both = 0, none = 0, split = 0;
    std::map<int, std::array<double, 2>> byB;   // по бюджетам: очки сети, пар
    for (int i = 0; i < N; i++) {
        const int w0 = res[i][0] == 1, w1 = res[i][1] == 1;
        pts += (w0 + w1) / 2.0;
        both += w0 && w1;
        none += !w0 && !w1;
        split += w0 != w1;
        const int k = bud[i] < 400 ? 200 : (bud[i] < 800 ? 400 : (bud[i] < 1600 ? 800 : (bud[i] < 3200 ? 1600 : 3200)));
        byB[k][0] += (w0 + w1) / 2.0;
        byB[k][1] += 1;
    }
    const double p = pts / N;
    std::printf("честная проверка, %d пар (бой дважды, сеть за каждого из двух боссов, ИНТ 16 у обоих): очки сети %.1f%% (±%.1f п.п.); сеть выиграла за обоих боссов в %d парах, проиграла за обоих в %d, по одному разу в %d (тут решила сборка)\n",
                N, 100 * p, 100 * 2 * std::sqrt(p * (1 - p) / (2.0 * N)), both, none, split);
    for (auto& kv : byB) std::printf("  бюджет от %d: очки сети %.1f%% (%d пар)\n", kv.first, 100 * kv.second[0] / kv.second[1], (int)kv.second[1]);
    return 0;
}

static int evalMain(int argc, char** argv) {
    if (argc < 4) return 1;
    PNet net;
    if (!pn::loadP(argv[2], net, true)) return 1;
    loadCalibBld();   // DND_CALIB_BLD > 0 — часть боссов от сетей-Сборщиков (по умолчанию только генератор)
    const EvalRes ev = evaluate(cmb::coefNow(), net, std::atoi(argv[3]), HELD_OUT);
    std::printf("сеть (ИНТ 16, жадно) против правила-бота (ИНТ 10), %d отложенных боёв: победы %.1f%% (±%.1f п.п.), раундов %.1f, решений на бой %.1f, отказов %lld, странных побед %zu\n",
                ev.n, 100 * ev.win, 100 * 2 * std::sqrt(0.25 / ev.n), ev.rounds, ev.steps, ev.illegal, ev.strange.size());
    std::printf("  ход: ходов сети %lld, с перемещением %.1f%%, упёрлись в предел %d решений %lld (%.2f на бой), завершены движком %lld, решений с закрытым «концом хода» (пустой ход) %lld\n",
                ev.turns, 100.0 * ev.movedTurns / std::max(1LL, ev.turns), pn::TURN_CAP, ev.capHits, (double)ev.capHits / std::max(1, ev.n), ev.forcedEnds, ev.endBlocked);
    std::printf("  по боссам: %s\n", ev.byBoss.bosses().c_str());
    for (size_t i = 0; i < std::min<size_t>(ev.strange.size(), 10); i++) std::printf("  %s\n", ev.strange[i].c_str());
    return 0;
}
}   // namespace pt
