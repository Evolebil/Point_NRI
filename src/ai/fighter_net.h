// ============================================================================
// Сеть-боец в движке: наблюдение, ход бойца по сети (через Ovr), контекст боя. Подключается в boss_arena_v20.cc перед fight().
// Обучение (траектории, A2C) — в net_fighter.cc. Веса: net_fighter.bin (рядом с программой) или файл из переменной DND_NET ("off" — без сети).
// ============================================================================
#pragma once
#include <cstdlib>
#include <fstream>
#include <string>
#include "gru.h"

typedef GruNet<float> Net;
static const int MAXA = 12;                    // сколько способностей сеть различает по номеру
static const int F_BASE = 72, F_ABIL = 13;     // признаков общих и на одну способность (v21: +по клетке, реакция, типов урона)
static const int D_IN = F_BASE + MAXA * F_ABIL;
static const int N_STANCE = 10, N_TGT = 4, N_PREF = 2 + MAXA;   // стойка 9 — снять концентрацию (v21); предпочтительное действие: 0 нет, 1 оружие, 2+k способность k
static const int NA = N_STANCE + N_TGT + N_PREF;
static const int EMB = 64, HID = 64;

// ---------------------------------------------------------------------------------------------------------------------------------
// Наблюдение
// ---------------------------------------------------------------------------------------------------------------------------------
static bool abilAvailable(const Fighter& me, size_t i) {
    const Spec& s = me.b.abil[i];
    return me.uses[i] > 0 && !(s.conc && me.concSpec >= 0) && !(s.slotLvl > 0 && !findSlot(me, s.slotLvl));
}

static void encodeObs(const World& w, int idx, float* x, bool* mask) {
    const Fighter& me = w.f[idx];
    const Fighter& E = w.f[1 - me.team];
    int q = 0;
    auto put = [&](double v) { x[q++] = (float)std::clamp(v, -3.0, 3.0); };
    // я
    put((double)me.hp / me.maxHp);
    put(std::log2((double)me.maxHp + 1) / 14.0);
    put((double)me.temp / me.maxHp);
    put(me.b.ac / 30.0);
    put(speedOf(me) / 60.0);
    put(me.concSpec >= 0);
    put(me.supp > 0);
    put(me.hidden || me.invisRounds > 0);
    for (int k = 1; k < C_N; k++) put(me.cond[k] > 0);
    // главный враг
    put((double)E.hp / E.maxHp);
    put(std::log2((double)E.maxHp + 1) / 14.0);
    put(E.b.ac / 30.0);
    put(speedOf(E) / 60.0);
    const double dist = distFt(me, E);
    put(dist / 500.0);
    put(los(w, me.x, me.y, E.x, E.y));
    put(E.b.wRange > 5);
    put(std::max(E.b.wRange, E.b.reach) / 100.0);
    put(E.b.wN * (E.b.wS + 1) / 2.0 * E.b.wAtt / me.maxHp);
    for (int k = 1; k < C_N; k++) put(E.cond[k] > 0);
    // миньоны
    int enemyMin = 0, allyMin = 0;
    double nearMin = 100.0;
    for (size_t i = 2; i < w.f.size(); i++) {
        if (w.f[i].hp <= 0) continue;
        if (w.f[i].team == me.team) allyMin++;
        else { enemyMin++; nearMin = std::min(nearMin, (double)distFt(me, w.f[i])); }
    }
    put(enemyMin / 5.0);
    put(allyMin / 5.0);
    put(nearMin / 100.0);
    put(g_curRound / 60.0);
    // моё оружие
    put(me.b.wN > 0 ? std::min(2.0, evWeapon(w, me, E) / std::max(1, E.hp)) : 0.0);
    put(me.b.wRange / 100.0);
    put(me.b.reach / 30.0);
    put(me.b.wAtt / 10.0);
    // арена и погоня: размер арены, расстояние до края (выйти за арену = проиграть), признаки погони, оставшиеся безрисковые рывки, истощение
    put(w.aw / 100.0);
    put(w.ah / 100.0);
    put(std::min({me.x, me.y, w.aw - 1 - me.x, w.ah - 1 - me.y}) / 20.0);
    put(w.chaseOn);
    put(w.chaseOn && w.chasePrey == idx);
    put(std::max(0, me.dashLimit - me.dashes) / 6.0);
    put(me.exh / 5.0);
    put(E.exh / 5.0);
    put(w.chaseSeen);
    put(w.theme);
    while (q < F_BASE) put(0);
    if (q > F_BASE) std::abort();
    // способности
    for (int k = 0; k < MAXA; k++) {
        float* f = x + F_BASE + k * F_ABIL;
        for (int j = 0; j < F_ABIL; j++) f[j] = 0;
        if (k >= (int)me.b.abil.size()) continue;
        const Spec& s = me.b.abil[k];
        f[0] = 1;
        f[1] = abilAvailable(me, k);
        f[2] = s.bonus;
        f[3] = s.self;
        f[4] = (float)(s.range / 100.0);
        f[5] = (float)(s.radius / 30.0);
        f[6] = s.self ? 0.f : (float)std::min(2.0, evSpec(w, s, me, E) / std::max(1, E.hp));
        f[7] = s.cond > 0;
        f[8] = s.healN > 0 || s.tempN > 0;
        f[9] = s.teleFt > 0 || s.hide || s.buffN > 0 || s.acB > 0 || s.sbAmt > 0 || s.invisN > 0 || s.darkR > 0;
        f[10] = s.ground;
        f[11] = s.react / 4.0f;
        f[12] = (float)(s.dmgParts().size() / 3.0);
    }
    // маски допустимых действий
    for (int j = 0; j < NA; j++) mask[j] = true;
    const int pb = N_STANCE + N_TGT;
    mask[pb + 1] = me.b.wN > 0;
    for (int k = 0; k < MAXA; k++) mask[pb + 2 + k] = k < (int)me.b.abil.size() && abilAvailable(me, k) && !me.b.abil[k].react;   // реакция срабатывает сама
    mask[9] = me.concSpec >= 0;   // снять концентрацию — только если она есть
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Траектории и ход бойца под управлением сети
// ---------------------------------------------------------------------------------------------------------------------------------
struct TStep {
    Net::Step st;
    float logits[NA];
    bool mask[NA];
    int act[3];
    float value = 0, reward = 0;
};
struct Side {
    bool active = false;
    std::vector<float> h;
    std::vector<TStep> steps;
    bool hasPrev = false;
    double prevMy = 1, prevOpp = 1;
};
struct NetCtx {
    const Net* net = nullptr;
    const Net* sideNet[2] = {nullptr, nullptr};   // своя сеть у стороны (команды); если нет — общая net
    bool greedy = false, forceDefault = false;
    Rng rng;
    Side side[2];
    double fallback[2] = {0, 0};   // шанс, что боец в этот ход играет обычным ботом (ИНТ 12–15: хорошая, но не полная тактика)
};
static thread_local NetCtx* g_ctx = nullptr;
// Последнее решение сети (стойка, режим цели, предпочтительное действие) — для кадров просмотрщика: что «задумала» нейросеть в этот ход
struct NetDecision { bool valid = false; int st = 0, tg = 0, pf = -1; };
static thread_local NetDecision g_netDec;
// Принудительное управление сетью независимо от ИНТ (биты по сторонам): игра против нейросети-бойца
static thread_local int g_forceNetMask = 0;
static double g_netTimePen = 0.0;   // штраф за каждое решение (обучение): не затягивать бой

static int pickAction(const float* logits, const bool* mask, int n, bool greedy, Rng& rng) {
    float mx = -1e30f;
    for (int j = 0; j < n; j++) if (mask[j]) mx = std::max(mx, logits[j]);
    double pr[NA], tot = 0;
    for (int j = 0; j < n; j++) { pr[j] = mask[j] ? std::exp((double)logits[j] - mx) : 0.0; tot += pr[j]; }
    if (greedy) { int b = 0; for (int j = 1; j < n; j++) if (pr[j] > pr[b]) b = j; return b; }
    double r = rng.unit() * tot;
    for (int j = 0; j < n; j++) { r -= pr[j]; if (r <= 0 && mask[j]) return j; }
    for (int j = n - 1; j >= 0; j--) if (mask[j]) return j;
    return 0;
}

static void netTurn(World& w, int idx) {
    NetCtx* c = g_ctx;
    Fighter& me = w.f[idx];
    if (!c || !c->side[me.team].active || (c->fallback[me.team] > 0 && rand01() < c->fallback[me.team])) { takeTurn(w, idx); return; }
    Side& sd = c->side[me.team];
    const Fighter& E = w.f[1 - me.team];
    const double myFrac = (double)me.hp / me.maxHp, oppFrac = (double)E.hp / E.maxHp;
    if (sd.hasPrev && !sd.steps.empty()) sd.steps.back().reward = (float)((sd.prevOpp - oppFrac) - (sd.prevMy - myFrac) - g_netTimePen);   // пряник за урон врагу, кнут за урон мне
    sd.prevMy = myFrac;
    sd.prevOpp = oppFrac;
    sd.hasPrev = true;
    int a[3];
    if (c->forceDefault) {
        a[0] = 0; a[1] = 0; a[2] = 0;
        g_ovr.active = true;
    } else {
        sd.steps.emplace_back();
        TStep& ts = sd.steps.back();
        float x[D_IN];
        encodeObs(w, idx, x, ts.mask);
        if (sd.h.empty()) sd.h.assign(HID, 0.f);
        (c->sideNet[me.team] ? c->sideNet[me.team] : c->net)->forward(x, sd.h.data(), ts.st, ts.logits, ts.value);
        sd.h = ts.st.h;
        a[0] = pickAction(ts.logits, ts.mask, N_STANCE, c->greedy, c->rng);
        a[1] = pickAction(ts.logits + N_STANCE, ts.mask + N_STANCE, N_TGT, c->greedy, c->rng);
        a[2] = pickAction(ts.logits + N_STANCE + N_TGT, ts.mask + N_STANCE + N_TGT, N_PREF, c->greedy, c->rng);
        ts.act[0] = a[0]; ts.act[1] = a[1]; ts.act[2] = a[2];
        g_ovr.active = true;
    }
    g_ovr.stance = a[0];
    g_ovr.tgt = a[1];
    g_ovr.pref = a[2] == 0 ? -1 : a[2] - 1;   // 1 → оружие (0), 2+k → способность k (1+k)
    g_netDec = NetDecision{true, g_ovr.stance, g_ovr.tgt, g_ovr.pref};
    takeTurn(w, idx);
    g_ovr = Ovr();
}


// ---------------------------------------------------------------------------------------------------------------------------------
// Старая сеть (стойки) отключена в v21 по решению владельца: её веса грузятся только из DND_NET_LEGACY (путь) — для сравнения
// с новой сетью (prim_net.h). Подключение к бою (AutoNet) — в prim_net.h.
// ---------------------------------------------------------------------------------------------------------------------------------
inline const Net* fighterNet() {
    static const Net* net = []() -> const Net* {
        const char* e = std::getenv("DND_NET_LEGACY");
        if (!e || std::string(e) == "off") return nullptr;
        const std::string path = e;
        static Net n;
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "ВНИМАНИЕ: DND_NET_LEGACY=%s — файл не найден, старая сеть не подключена\n", path.c_str());
            return nullptr;
        }
        int hd[4];
        f.read((char*)hd, sizeof hd);
        if (!f || hd[0] != D_IN || hd[1] != EMB || hd[2] != HID || hd[3] != NA) {
            // ТЗ v21: несовпадение заголовка не должно молча отключать сеть
            std::fprintf(stderr, "ВНИМАНИЕ: веса старой сети-бойца %s не подходят к движку (в файле %d/%d/%d/%d, нужно %d/%d/%d/%d: вход/эмбеддинг/память/действия) — сеть отключена; веса v20 переводит ./net_fighter upgrade20\n",
                         path.c_str(), f ? hd[0] : -1, f ? hd[1] : -1, f ? hd[2] : -1, f ? hd[3] : -1, D_IN, EMB, HID, NA);
            return nullptr;
        }
        n.init(hd[0], hd[1], hd[2], hd[3], 1);
        f.read((char*)n.p.data(), n.p.size() * sizeof(float));
        if (!f) return nullptr;
        g_rlTurn = netTurn;
        return &n;
    }();
    return net;
}
