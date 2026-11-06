// ============================================================================
// Сеть-боец v21 (ТЗ v21, №4 и №5): ходит примитивами (applyPrim) и видит карту. Подключается в boss_arena_v20.cc после
// fighter_net.h. Старая сеть со стойками (fighter_net.h) по решению владельца отключена: её веса грузятся только по
// переменной DND_NET_LEGACY (для сравнения).
//
// Решение — один примитив за шаг, пока сеть не выберет «конец хода». На каждом шаге:
//   наблюдение = о себе и ходе (бюджет действия, движения, решено за ход) + до MAXE известных врагов + способности (MAXA × FA)
//                + окно карты WIN×WIN вокруг бойца (слои: стены, укрытия, местность, вода, ущелье, тьма, высота, свои,
//                  известные враги) — туман войны: врага, которого сторона не видит и не знает, в наблюдении нет;
//   эмбеддинг (tanh) → GRU (память живёт весь бой) → головы:
//     тип примитива (маска допустимых), способность (маска), указатель на врага (по признакам слотов), указатель на клетку
//     (по признакам кандидатов: куда дойти, куда прыгнуть, куда навести область), запасное оружие, ценность.
// Клетка — указатель на список кандидатов, а не координаты на сетке 100×100. Признаки клетки-кандидата для движения:
//   смещение, цена пути, расстояние до ближайшего врага, видит ли её враг, укрытие, достанет ли враг её за свой ход,
//   видно ли врага оттуда, достану ли я врага оттуда, уход из досягаемости ближника (провоцированная атака), высота, тьма.
// Обучение (копирование бота по записи PrimLog, затем PPO/A2C) — этап 3. Здесь: устройство сети, наблюдение, маски,
// прямой проход и ход по сети. Веса: net_fighter_p.bin или файл из DND_NETP ("off" — без сети).
// ============================================================================
#pragma once
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace pn {
static const int MAXE = 8, FE = 12;                // слоты известных врагов и признаков на слот
static const int GB = 48;                           // признаков о себе и ходе
static const int WIN = 15, NL = 11;                 // окно карты и слоёв
static const int FA = 16;                           // признаков на способность (13 + категория: атака, заклинание, призыв)
// Этап 5б: признаки состояний дописаны В КОНЕЦ входа (старая часть входа не сдвинулась — веса PN22 переносятся с нулями
// у новых входов, сеть сначала играет как прежде): у врага — какие состояния на нём (C_N−1), его скорость, доля иммунитетов
// к состояниям; у способности — какое состояние (C_N−1), длительность, без повторного спаса, надбавка к СЛ, шанс провала
// спаса у ближайшего врага, впустую ли (на ближайшем уже есть или иммунитет)
static const int FEX = (C_N - 1) + 2;                   // новые признаки врага (этап 5б)
static const int FAX = (C_N - 1) + 5;                   // новые признаки способности
static const int D_OLD = GB + MAXE * FE + MAXA * FA + WIN * WIN * NL;
static const int D_IN24 = D_OLD + MAXE * FEX + MAXA * FAX;   // вход PN23/PN24
// v21, механики «всё оставшееся» и команды (PN25): снова В КОНЕЦ входа — о себе GBX (размер, протискивание, высота полёта и
// потолок, лазание/плавание/копание, Увёртливость, солнце, облик, логово, команды и союзники, ярус, уровень земли), у врага
// FEX2 (размер, разница высот, высота полёта, летает, протиснут, в облике, Увёртливость, логово — и у указателя), слои окна
// NLX (закрытая и открытая дверь, настил, дерево, опасная зона, лестница), у способности FAX2 (облик, групп призыва, копий)
static const int GBX = 20, FEX2 = 8, NLX = 6, FAX2 = 3;
static const int FEP = FE + FEX + FEX2;                 // признаки указателя на врага
static const int FEP24 = FE + FEX;                      // ... в PN23/PN24
static const int OFF_X = D_IN24;                        // начало новых признаков PN25
static const int D_IN = D_IN24 + GBX + MAXE * FEX2 + WIN * WIN * NLX + MAXA * FAX2;
static const int MAXC = 96, FC = 12;                // кандидатов-клеток и признаков на клетку
static const int NSWAP = 3;
static const int EMB = 128, HID = 128, KP = 32;     // эмбеддинг, память, скрытый слой указателей
static const int TURN_CAP = 24;                     // технический предохранитель: решений сети за ход
// Ступени решения (шаг 4): тип → для «применить» категория (атака, заклинание, призыв, манёвр), затем способность;
// для «взаимодействия» — что именно (смена оружия → какое, приказ, снять концентрацию, всплеск) → цель (клетка, существо)
enum StType { ST_MOVE, ST_APPLY, ST_INTERACT, ST_END, ST_N };
enum StInter { IN_SWAP, IN_ORDER, IN_DROPCONC, IN_SURGE, IN_ALTUP, IN_ALTDOWN, IN_N };   // выше/ниже на 10 фт — PN25
static const int IN_N24 = 4;   // взаимодействий в PN22–PN24
// «выше/ниже» доступно сети: у перенесённых старых весов (ещё не учились с этим действием) — нет, иначе само открытие
// «взаимодействия» для летуна меняло бы её ходы; обучение включает снова
inline bool g_altOn = true;
const char* STN[ST_N] = {"идти", "применить", "взаимодействие", "конец хода"};
const char* INN[IN_N] = {"смена оружия", "приказ", "снять концентрацию", "всплеск действий", "выше", "ниже"};
// слоты способностей: оружие, способности сборки (MAXA), Рывок, Уклонение, Отход
static const int AB_WEAPON = 0, AB_SPEC = 1, AB_DASH = 1 + MAXA, AB_DODGE = AB_DASH + 1, AB_DISENG = AB_DASH + 2, AB_HELP = AB_DASH + 3, AB_READY = AB_DASH + 4, NAB = AB_DASH + 5;   // Помощь и Подготовка (v21)

// ---------------------------------------------------------------------------------------------------------------------
// Наблюдение
// ---------------------------------------------------------------------------------------------------------------------
struct Cand {
    int x = 0, y = 0;
    float f[FC] = {};
};
struct Obs {
    std::vector<float> x;                   // D_IN
    int enemy[MAXE];                        // индекс бойца в слоте (−1 — пусто)
    float ef[MAXE][FEP];                    // признаки слотов врагов (старые FE + новые FEX — для указателя)
    std::vector<Cand> move, spellCells[MAXA];
    bool typeMask[PR_N] = {}, abilMask[MAXA] = {}, swapMask[NSWAP] = {};
    bool atkMask[MAXE] = {}, spellTgt[MAXA][MAXE] = {}, helpTgt[MAXE] = {};   // helpTgt — враги вплотную (Помощь)
    // маски ступеней (из typeMask и abilMask): тип, категория, слоты способностей (и их категории), взаимодействие
    bool stMask[ST_N] = {}, catMask[CAT_N] = {}, abMask[NAB] = {}, inMask[IN_N] = {};
    int abCat[NAB] = {};
};

// признаки клетки (cx, cy) для бойца me, ближайший известный враг — e (может быть nullptr)
static void cellFeat(const World& w, const Fighter& me, const Fighter* e, int cx, int cy, int cost, float* f) {
    for (int k = 0; k < FC; k++) f[k] = 0;
    f[0] = (float)((cx - me.x) / 20.0);
    f[1] = (float)((cy - me.y) / 20.0);
    f[2] = (float)(cost / 60.0);
    if (e) {
        const int d = 5 * std::max(std::abs(cx - e->x), std::abs(cy - e->y));
        const bool sight = losFC(w, *e, cx, cy);
        f[3] = (float)(d / 100.0);
        f[4] = sight && !(w.dark[cy][cx] && e->b.dv < d && e->b.blindsight < d);   // враг увидит меня там
        f[5] = coverBonus(w, e->x, e->y, cx, cy) > 0 || w.climb[cy][cx];        // укрытие от врага
        f[6] = d <= meleeReach(e->b) + speedOf(*e) || (e->b.wN > 0 && e->b.wRange > 5 && d <= e->b.wRange && sight);   // достанет за ход
        f[7] = sight;                                                            // мне видно врага
        f[8] = d <= atkDist(me.b) && sight && me.b.wN > 0;                       // достану оружием
        const bool adjNow = meleeWeapon(e->b) && distFt(me, *e) <= meleeReach(e->b);
        f[9] = adjNow && d > meleeReach(e->b) && !me.disengaged;                // уход из досягаемости: провоцированная атака
    }
    f[10] = w.height[cy][cx] / 2.0f;
    f[11] = w.dark[cy][cx];
}
// клетки, куда можно дойти за budget фт (тот же поиск по цене шага, что pathTo движка; без провоцированных атак), не больше MAXC — равномерной
// выборкой по цене; текущая клетка всегда первая
static void reachable(const World& w, const Fighter& me, const Fighter* e, int budget, std::vector<Cand>& out) {
    out.clear();
    // цены по узлам (клетка, ярус) и по клеткам; метка поиска вместо очистки массивов (арена до 200×200)
    static thread_local std::vector<int> costV, bestV, stampC, stampB;
    static thread_local int stamp = 0;
    if (costV.empty()) {
        costV.assign((size_t)2 * W * W, 0);
        stampC.assign((size_t)2 * W * W, 0);
        bestV.assign((size_t)W * W, 0);
        stampB.assign((size_t)W * W, 0);
    }
    ++stamp;
    struct Lazy {
        std::vector<int>& v;
        std::vector<int>& st;
        int s;
        int& operator[](size_t i) {
            if (st[i] != s) { st[i] = s; v[i] = 1 << 29; }
            return v[i];
        }
    } cost{costV, stampC, stamp}, best{bestV, stampB, stamp};
    std::vector<std::pair<int, int>> q;   // (цена, узел) — Дейкстра на малых ценах (5..20 фт), узлы с ярусами, как в pathTo
    auto push = [&](int c, int v) {
        cost[c] = v;
        q.push_back({v, c});
        std::push_heap(q.begin(), q.end(), std::greater<>());
    };
    push(me.y * W + me.x + (me.up ? W * W : 0), 0);
    std::vector<std::pair<int, int>> all;
    while (!q.empty()) {
        std::pop_heap(q.begin(), q.end(), std::greater<>());
        const auto [v, c] = q.back();
        q.pop_back();
        if (v != cost[c]) continue;
        const int l = c / (W * W), cell = c % (W * W), x = cell % W, y = cell / W;
        if (v < best[cell]) {   // клетка-кандидат — по дешёвому ярусу (pathTo идёт к нему же)
            if (best[cell] == (1 << 29)) all.push_back({v, cell});
            best[cell] = v;
        }
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                for (int nl = 0; nl < 2; nl++) {
                    const int nx = x + dx, ny = y + dy;
                    if ((!dx && !dy) || !stepOK(w, me, x, y, l, nx, ny, nl)) continue;
                    bool foe = false;   // известные враги обходятся, как в pathTo движка
                    for (size_t i = 0; i < w.f.size(); i++) {
                        const Fighter& g = w.f[i];
                        if (g.team != me.team && touchesAt(w, me, nx, ny, nl, g) && isKnown(w, me.team, (int)i)) foe = true;
                    }
                    if (foe) continue;
                    const int n = ny * W + nx + nl * W * W, nv = v + stepCostL(w, me, x, y, l, nx, ny, nl);
                    if (nv <= budget && nv < cost[n]) push(n, nv);
                }
    }
    std::sort(all.begin(), all.end());
    const size_t n = all.size(), take = std::min<size_t>(n, MAXC);
    for (size_t k = 0; k < take; k++) {
        const size_t j = take <= 1 ? 0 : k * (n - 1) / (take - 1);
        Cand cd;
        cd.x = all[j].second % W;
        cd.y = all[j].second / W;
        cellFeat(w, me, e, cd.x, cd.y, all[j].first, cd.f);
        out.push_back(cd);
    }
}
static bool abilReady(const World& w, int idx, const TurnState& ts, int k) {
    const Fighter& me = w.f[idx];
    const Spec& s = me.b.abil[k];
    return !s.react && canPay(me, ts, specAbility(me.b, k).cost) && (s.sumN <= 0 || summonRoom(w, idx, k) > 0);
}
static void buildObs(World& w, int idx, const TurnState& ts, Obs& o) {
    Fighter& me = w.f[idx];
    o.x.assign(D_IN, 0.f);
    float* x = o.x.data();
    int q = 0;
    auto put = [&](double v) { x[q++] = (float)std::clamp(v, -3.0, 3.0); };
    // известные враги по расстоянию (туман войны)
    std::vector<std::pair<int, int>> en;
    for (size_t i = 0; i < w.f.size(); i++)
        if (w.f[i].hp > 0 && w.f[i].team != me.team && !w.f[i].fled && isKnown(w, me.team, (int)i)) en.push_back({distFt(me, w.f[i]), (int)i});
    std::sort(en.begin(), en.end());
    const Fighter* near = en.empty() ? nullptr : &w.f[en[0].second];
    // о себе и ходе
    put((double)me.hp / me.maxHp);
    put((double)me.temp / me.maxHp);
    put(std::log2((double)me.maxHp + 1) / 14.0);
    put(me.b.ac / 30.0);
    put(ts.speed / 60.0);
    put(ts.speed0 / 60.0);
    put(!ts.actionUsed);
    put(!ts.bonusUsed);
    put(me.surgeLeft / 3.0);
    put(me.concSpec >= 0);
    put(me.hidden || me.invisRounds > 0);
    put(me.reactionUsed);
    for (int k = 1; k < C_N; k++) put(me.cond[k] > 0);
    put(g_curRound / 60.0);
    put(w.aw / 100.0);
    put(w.ah / 100.0);
    put(std::min({me.x, me.y, w.aw - 1 - me.x, w.ah - 1 - me.y}) / 20.0);
    put(w.chaseOn);
    put(w.chaseOn && w.chasePrey == idx);
    put(me.exh / 5.0);
    put(me.b.wN > 0);
    put(me.b.wRange / 100.0);
    put(meleeReach(me.b) / 30.0);
    put(me.b.wAtt / 10.0);
    put(me.b.alt.size() / 3.0);
    put(me.b.disengB);
    {   // призыв — способность (v21): сколько призывающих способностей и сколько своих приспешников живо
        int nSum = 0, alive = 0;
        for (auto& sp : me.b.abil) nSum += sp.sumN > 0;
        for (auto& g : w.f) alive += g.hp > 0 && g.summoner == idx;
        put(nSum / 5.0);
        put(alive / 5.0);
    }
    put(en.size() / (double)MAXE);
    put(idx < 2);
    // ход (шаг 4): сколько решено, отдан ли приказ, сколько смен оружия, потрачено ли что-то (пустой ход запрещён)
    put(ts.steps / (double)TURN_CAP);
    put(ts.orders);
    put(ts.swaps / 3.0);
    put(turnSpent(ts, me));
    while (q < GB) put(0);
    // слоты врагов
    for (int j = 0; j < MAXE; j++) {
        o.enemy[j] = j < (int)en.size() ? en[j].second : -1;
        float* f = o.ef[j];
        for (int k = 0; k < FEP; k++) f[k] = 0;
        o.atkMask[j] = false;
        o.helpTgt[j] = false;   // пустой слот — не цель Помощи (наблюдение переиспользуется между шагами)
        if (o.enemy[j] < 0) continue;
        const Fighter& e = w.f[o.enemy[j]];
        f[0] = 1;
        f[1] = o.enemy[j] < 2;
        f[2] = (float)e.hp / e.maxHp;
        f[3] = (float)(std::log2((double)e.maxHp + 1) / 14.0);
        f[4] = e.b.ac / 30.0f;
        f[5] = distFt(me, e) / 100.0f;
        f[6] = losF(w, me, e);
        f[7] = teamSees(w, me.team, e);
        f[8] = (float)std::min(2.0, evWeapon(w, me, e) / std::max(1, e.hp));
        f[9] = (float)std::min(2.0, evWeapon(w, e, me) / std::max(1, me.hp));
        f[10] = e.b.wRange > 5;
        int nc = 0;
        for (int k = 1; k < C_N; k++) nc += e.cond[k] > 0;
        f[11] = nc / 5.0f;
        o.atkMask[j] = !ts.actionUsed && me.b.wN > 0 && distFt(me, e) <= atkDist(me.b) && f[6] > 0 && !(me.cond[C_CHARM] > 0 && me.charmSrc == o.enemy[j]);
        o.helpTgt[j] = !ts.actionUsed && distFt(me, e) <= 5 && !(me.cond[C_CHARM] > 0 && me.charmSrc == o.enemy[j]);
        for (int k = 0; k < FE; k++) put(f[k]);
        // этап 5б: какие состояния на враге, его скорость, иммунитеты к состояниям
        for (int k = 1; k < C_N; k++) f[FE + k - 1] = e.cond[k] > 0;
        f[FE + C_N - 1] = speedOf(e) / 60.0f;
        f[FE + C_N] = __builtin_popcount(e.b.immCond) / (float)(C_N - 1);
        for (int k = 0; k < FEX; k++) x[D_OLD + j * FEX + k] = std::clamp(f[FE + k], -3.0f, 3.0f);
    }
    q = GB + MAXE * FE;
    // способности
    for (int k = 0; k < MAXA; k++) {
        float* f = x + GB + MAXE * FE + k * FA;
        o.abilMask[k] = false;
        o.spellCells[k].clear();
        for (int j = 0; j < MAXE; j++) o.spellTgt[k][j] = false;
        if (k >= (int)me.b.abil.size()) continue;
        const Spec& s = me.b.abil[k];
        f[0] = 1;
        f[1] = abilReady(w, idx, ts, k);
        f[2] = s.bonus;
        f[3] = s.self;
        f[4] = (float)(specReach(me.b, s) / 100.0);
        f[5] = (float)(s.radius / 30.0);
        f[6] = s.self || !near ? 0.f : (float)std::min(2.0, evSpec(w, s, me, *near) / std::max(1, near->hp));
        f[7] = s.cond > 0;
        f[8] = s.healN > 0 || s.tempN > 0;
        f[9] = s.teleFt > 0 || s.hide || s.buffN > 0 || s.acB > 0 || s.sbAmt > 0 || s.invisN > 0 || s.darkR > 0;
        f[10] = s.ground;
        f[11] = s.react / 4.0f;
        f[12] = (float)(s.dmgParts().size() / 3.0);
        f[13 + specCategory(s)] = 1;   // категория: атака, заклинание, призыв
        {   // этап 5б: какое состояние, сколько держится, СЛ, шанс провала у ближайшего, впустую ли
            float* g = x + D_OLD + MAXE * FEX + k * FAX;
            if (s.cond > 0) {
                g[s.cond - 1] = 1;
                g[C_N - 1] = s.condRounds / 5.0f;
                g[C_N] = s.noRep;
                if (near && !s.self) {
                    const bool save = s.delivery == 1 || (s.delivery == 0 && s.cond != C_GRAPPLE);
                    g[C_N + 2] = save ? (float)pFailEst(*near, s.saveStat, me.dc() + s.dcB) : 1.0f;
                    g[C_N + 3] = near->cond[s.cond] > 0 || (near->b.immCond >> s.cond & 1u) || near->cimm[s.cond] > 0;
                }
            }
            g[C_N + 1] = s.dcB / 8.0f;
        }
        if (!f[1]) continue;
        bool any = false;
        if (s.self && s.teleFt > 0) {   // телепорт: свободные клетки в пределах прыжка
            std::vector<Cand> all;
            for (int dy = -s.teleFt / 5; dy <= s.teleFt / 5; dy++)
                for (int dx = -s.teleFt / 5; dx <= s.teleFt / 5; dx++) {
                    const int cx = me.x + dx, cy = me.y + dy;
                    if ((!dx && !dy) || !fpAll(w, me, cx, cy, [&](int ax, int ay) { return !w.wall[ay][ax] && !w.pit[ay][ax]; }) || blockedAt(w, me, cx, cy)) continue;
                    if (distFC(w, me, cx, cy) > s.teleFt) continue;   // дальность прыжка — с высотой, как в движке
                    Cand cd;
                    cd.x = cx;
                    cd.y = cy;
                    cellFeat(w, me, near, cx, cy, 5 * std::max(std::abs(dx), std::abs(dy)), cd.f);
                    all.push_back(cd);
                }
            const size_t n = all.size(), take = std::min<size_t>(n, MAXC);
            for (size_t i = 0; i < take; i++) o.spellCells[k].push_back(all[take <= 1 ? 0 : i * (n - 1) / (take - 1)]);
            any = !o.spellCells[k].empty();
        } else if (s.self)
            any = true;
        else if (s.ground) {   // по клетке: центры на известных врагах и сдвинутые к себе (как у бота)
            for (auto& pe : en) {
                const Fighter& e = w.f[pe.second];
                const int sx = (me.x > e.x) - (me.x < e.x), sy = (me.y > e.y) - (me.y < e.y);
                const int kk = std::max(0, std::min(s.radius / 5, std::max(std::abs(me.x - e.x), std::abs(me.y - e.y)) - 1));
                for (auto c : {std::make_pair(e.x, e.y), std::make_pair(e.x + sx * kk, e.y + sy * kk), std::make_pair(e.x + sx, e.y + sy)}) {
                    if (c.first < 0 || c.second < 0 || c.first >= w.aw || c.second >= w.ah || w.wall[c.second][c.first]) continue;
                    if (distFC(w, me, c.first, c.second) > s.range || !losFC(w, me, c.first, c.second)) continue;
                    if ((int)o.spellCells[k].size() >= MAXC) break;
                    Cand cd;
                    cd.x = c.first;
                    cd.y = c.second;
                    cellFeat(w, me, &e, c.first, c.second, 0, cd.f);
                    o.spellCells[k].push_back(cd);
                }
            }
            any = !o.spellCells[k].empty();
        } else
            for (int j = 0; j < MAXE; j++) {
                if (o.enemy[j] < 0) continue;
                const Fighter& e = w.f[o.enemy[j]];
                if (distFt(me, e) <= specReach(me.b, s) && losF(w, me, e) && !(me.cond[C_CHARM] > 0 && me.charmSrc == o.enemy[j]))
                    o.spellTgt[k][j] = any = true;
            }
        o.abilMask[k] = any;
    }
    // окно карты
    float* m = x + GB + MAXE * FE + MAXA * FA;
    const int h = WIN / 2;
    for (int dy = -h; dy <= h; dy++)
        for (int dx = -h; dx <= h; dx++) {
            const int cx = me.x + dx, cy = me.y + dy, base = ((dy + h) * WIN + (dx + h)) * NL;
            if (cx < 0 || cy < 0 || cx >= W || cy >= W) {
                m[base] = 1;   // за краем — стена
                continue;
            }
            m[base + 0] = w.wall[cy][cx];
            m[base + 1] = w.climb[cy][cx];
            m[base + 2] = w.diff[cy][cx] && !w.water[cy][cx];
            m[base + 3] = w.water[cy][cx];
            m[base + 4] = w.pit[cy][cx];
            m[base + 5] = w.dark[cy][cx];
            m[base + 6] = w.height[cy][cx] / 2.0f;
        }
    auto mark = [&](int fx, int fy, int layer) {
        const int dx = fx - me.x, dy = fy - me.y;
        if (std::abs(dx) <= h && std::abs(dy) <= h) m[((dy + h) * WIN + (dx + h)) * NL + layer] = 1;
    };
    for (size_t i = 0; i < w.f.size(); i++) {
        const Fighter& g = w.f[i];
        if (g.hp <= 0) continue;
        if (g.team == me.team)
            mark(g.x, g.y, i < 2 ? 7 : 8);
        else if (isKnown(w, me.team, (int)i))
            mark(g.x, g.y, i < 2 ? 9 : 10);
    }
    // PN25: новые признаки — о себе, врагах, слои окна, способности
    {
        float* g = x + OFF_X;
        auto clampf = [](double v) { return (float)std::clamp(v, -3.0, 3.0); };
        int allies = 0, foes = 0;
        for (int i = 0; i < w.nLead && i < (int)w.f.size(); i++)
            if (i != idx && w.f[i].hp > 0 && !w.f[i].fled) (w.f[i].team == me.team ? allies : foes)++;
        bool formReady = false;
        for (size_t k = 0; k < me.b.abil.size(); k++) formReady = formReady || (me.b.abil[k].formTmpl >= 0 && abilReady(w, idx, ts, (int)k));
        g[0] = fpFull(me) / 4.0f;
        g[1] = squeezed(me);
        g[2] = me.alt / 6.0f;
        g[3] = me.b.fly && canStayAloft(me);
        g[4] = maxAltAt(w, me, me.x, me.y, me.up) / 6.0f;
        g[5] = me.b.climbS;
        g[6] = me.b.swimS;
        g[7] = me.b.burrowS;
        g[8] = me.b.evasion;
        g[9] = me.b.sunSens && inSun(w, me);
        g[10] = me.inForm;
        g[11] = me.inForm ? clampf((double)me.formHpSave / std::max(1, me.formMaxSave)) : 0.f;
        g[12] = me.b.lair / 3.0f;
        g[13] = formReady;
        g[14] = clampf(w.nTeams / 8.0);
        g[15] = clampf(allies / 8.0);
        g[16] = clampf(foes / 8.0);
        g[17] = w.day;
        g[18] = me.up;
        g[19] = zAt(w, me.x, me.y) / 9.0f;
        for (int j = 0; j < MAXE; j++) {
            float* f = o.ef[j] + FE + FEX;
            float* xe = x + OFF_X + GBX + j * FEX2;
            if (o.enemy[j] < 0) continue;
            const Fighter& e = w.f[o.enemy[j]];
            f[0] = fpFull(e) / 4.0f;
            f[1] = clampf((zF(w, e) - zF(w, me)) / 6.0);
            f[2] = e.alt / 6.0f;
            f[3] = e.b.fly;
            f[4] = squeezed(e);
            f[5] = e.inForm;
            f[6] = e.b.evasion;
            f[7] = e.b.lair / 3.0f;
            for (int k = 0; k < FEX2; k++) xe[k] = f[k];
        }
        float* m2 = x + OFF_X + GBX + MAXE * FEX2;
        for (int dy = -h; dy <= h; dy++)
            for (int dx = -h; dx <= h; dx++) {
                const int cx = me.x + dx, cy = me.y + dy, base = ((dy + h) * WIN + (dx + h)) * NLX;
                if (cx < 0 || cy < 0 || cx >= W || cy >= W) continue;
                m2[base + 0] = w.door[cy][cx] == 1;
                m2[base + 1] = w.door[cy][cx] == 2;
                m2[base + 2] = w.deck[cy][cx] / 9.0f;
                m2[base + 3] = w.tree[cy][cx];
                m2[base + 4] = w.haz[cy][cx] > 0;
                m2[base + 5] = w.ladder[cy][cx];
            }
        float* a2 = x + OFF_X + GBX + MAXE * FEX2 + WIN * WIN * NLX;
        for (int k = 0; k < MAXA && k < (int)me.b.abil.size(); k++) {
            const Spec& sp = me.b.abil[k];
            a2[k * FAX2 + 0] = sp.formTmpl >= 0;
            a2[k * FAX2 + 1] = sp.sumMore.size() / 3.0f;
            a2[k * FAX2 + 2] = clampf(sp.sumTotal() / 10.0);
        }
    }
    // кандидаты движения и маска примитивов
    reachable(w, me, near, ts.speed, o.move);
    if (me.cond[C_FRIGHT] > 0 && !en.empty()) {   // испуганный не приближается к врагу добровольно (как в applyPrim)
        auto minD = [&](int cx, int cy) {
            int d = 1 << 30;
            for (auto& pe : en) d = std::min(d, 5 * std::max(std::abs(w.f[pe.second].x - cx), std::abs(w.f[pe.second].y - cy)));
            return d;
        };
        const int now = minD(me.x, me.y);
        std::vector<Cand> keep;
        for (size_t i = 0; i < o.move.size(); i++)
            if (i == 0 || minD(o.move[i].x, o.move[i].y) >= now) keep.push_back(o.move[i]);
        o.move.swap(keep);
    }
    for (int k = 0; k < PR_N; k++) o.typeMask[k] = false;
    o.typeMask[PR_END] = true;
    o.typeMask[PR_MOVE] = ts.speed >= 5 && o.move.size() > 1;
    for (int j = 0; j < MAXE; j++) o.typeMask[PR_ATTACK] = o.typeMask[PR_ATTACK] || o.atkMask[j];
    for (int k = 0; k < MAXA; k++) o.typeMask[PR_SPELL] = o.typeMask[PR_SPELL] || o.abilMask[k];
    o.typeMask[PR_DASH] = o.typeMask[PR_DODGE] = !ts.actionUsed;
    o.typeMask[PR_HELP] = false;   // Помощь — если враг вплотную; Подготовка — если есть оружие и удар ещё не готов (v21)
    for (int j = 0; j < MAXE; j++) o.typeMask[PR_HELP] = o.typeMask[PR_HELP] || o.helpTgt[j];
    o.typeMask[PR_READY] = !ts.actionUsed && me.b.wN > 0 && !me.disarmed && !me.readied;
    o.typeMask[PR_DISENGAGE] = !me.disengaged && (!ts.actionUsed || (me.b.disengB && !ts.bonusUsed));
    int alive = 0;
    for (size_t i = 2; i < w.f.size(); i++) alive += w.f[i].hp > 0 && w.f[i].team == me.team;
    o.typeMask[PR_SUMMON] = false;   // призыв — способность: сеть применяет её как «способность k» (ступени — шаг 4)
    for (int k = 0; k < NSWAP; k++) o.swapMask[k] = k < (int)me.b.alt.size();
    // смена оружия: 1-я бесплатно, 2-я бонусным действием, 3-я действием (как в applyPrim)
    o.typeMask[PR_SWAP] = !me.b.alt.empty() && (ts.swaps == 0 || (ts.swaps == 1 && !ts.bonusUsed) || (ts.swaps == 2 && !ts.actionUsed));
    o.typeMask[PR_DROPCONC] = me.concSpec >= 0;
    o.typeMask[PR_SURGE] = me.surgeLeft > 0 && ts.actionUsed && ts.lastWasAttack;
    o.typeMask[PR_ORDER] = idx < 2 && alive > 0 && !en.empty() && ts.orders == 0;   // простой приказ — раз за ход
    // ступени: способности по слотам и их категории, категории, взаимодействия, тип
    o.abMask[AB_WEAPON] = o.typeMask[PR_ATTACK];
    o.abCat[AB_WEAPON] = CAT_ATTACK;
    for (int k = 0; k < MAXA; k++) {
        o.abMask[AB_SPEC + k] = o.abilMask[k];
        o.abCat[AB_SPEC + k] = k < (int)me.b.abil.size() ? specCategory(me.b.abil[k]) : CAT_SPELL;
    }
    o.abMask[AB_DASH] = o.typeMask[PR_DASH];
    o.abMask[AB_DODGE] = o.typeMask[PR_DODGE];
    o.abMask[AB_DISENG] = o.typeMask[PR_DISENGAGE];
    o.abMask[AB_HELP] = o.typeMask[PR_HELP];
    o.abMask[AB_READY] = o.typeMask[PR_READY];
    o.abCat[AB_DASH] = o.abCat[AB_DODGE] = o.abCat[AB_DISENG] = o.abCat[AB_HELP] = o.abCat[AB_READY] = CAT_MANEUVER;
    for (int c = 0; c < CAT_N; c++) o.catMask[c] = false;
    for (int j = 0; j < NAB; j++)
        if (o.abMask[j]) o.catMask[o.abCat[j]] = true;
    o.inMask[IN_SWAP] = o.typeMask[PR_SWAP];
    o.inMask[IN_ORDER] = o.typeMask[PR_ORDER];
    o.inMask[IN_DROPCONC] = o.typeMask[PR_DROPCONC];
    o.inMask[IN_SURGE] = o.typeMask[PR_SURGE];
    // высота полёта (PN25): 10 фт движения за уровень, в пределах потолка
    o.inMask[IN_ALTUP] = g_altOn && me.b.fly && canStayAloft(me) && speedOf(me) > 0 && ts.speed >= 10 && me.alt < maxAltAt(w, me, me.x, me.y, me.up);
    o.inMask[IN_ALTDOWN] = g_altOn && me.alt > 0 && ts.speed >= 10;
    o.stMask[ST_MOVE] = o.typeMask[PR_MOVE];
    o.stMask[ST_APPLY] = o.catMask[CAT_ATTACK] || o.catMask[CAT_SPELL] || o.catMask[CAT_SUMMON] || o.catMask[CAT_MANEUVER];
    o.stMask[ST_INTERACT] = false;
    for (int k = 0; k < IN_N; k++) o.stMask[ST_INTERACT] = o.stMask[ST_INTERACT] || o.inMask[k];
    // пустой ход сети запрещён (решение владельца): ход пуст, а сдвинуться или применить атаку, заклинание, призыв можно —
    // «конец хода» закрыт; манёвры (Рывок, Уклонение, Отход) ход непустым не делают
    o.stMask[ST_END] = turnSpent(ts, me) || !(o.stMask[ST_MOVE] || o.catMask[CAT_ATTACK] || o.catMask[CAT_SPELL] || o.catMask[CAT_SUMMON]);
}
// легален только «конец хода»: движок завершает ход сам, шаг не идёт в обучение
inline bool onlyEnd(const Obs& o) {
    return !o.stMask[ST_MOVE] && !o.stMask[ST_APPLY] && !o.stMask[ST_INTERACT];
}

// ---------------------------------------------------------------------------------------------------------------------
// Сеть: эмбеддинг → GRU → головы фиксированного размера (тип примитива, способность, смена оружия, ценность) и два
// указателя (на врага и на клетку): score_i = v · tanh(U f_i + W h + b). Шаблон по типу числа: float в бою и обучении,
// double — для проверки градиентов (тест T176).
// ---------------------------------------------------------------------------------------------------------------------
// головы фиксированного размера по ступеням: тип, категория, способность (слот), взаимодействие, запасное оружие
static const int OF_TYPE = 0, OF_CAT = OF_TYPE + ST_N, OF_AB = OF_CAT + CAT_N, OF_IN = OF_AB + NAB, OF_SW = OF_IN + IN_N, NFIX = OF_SW + NSWAP;
enum PtrKind { PT_NONE, PT_ENEMY, PT_CELL };
template <class T>
struct PNetT {
    int D = D_IN, E = EMB, H = HID, NA = NFIX;
    GruNet<T> core;
    std::vector<T> ptr;
    size_t oU[3] = {}, oW[3] = {}, ob[3] = {}, ov[3] = {};   // по видам указателя (1 враг, 2 клетка)
    int nf[3] = {0, FEP, FC};
    void init(uint64_t seed) {
        core.init(D, E, H, NA, seed);
        size_t n = 0;
        for (int k = 1; k <= 2; k++) {
            oU[k] = n; n += (size_t)KP * nf[k];
            oW[k] = n; n += (size_t)KP * H;
            ob[k] = n; n += KP;
            ov[k] = n; n += KP;
        }
        ptr.assign(n, T(0));
        uint64_t s = seed * 7 + 3;
        for (auto& v : ptr) v = (T)((GruNet<T>::rnd01(s) * 2 - 1) * 0.15);
    }
    size_t size() const { return core.p.size() + ptr.size(); }
    // pre = W h + b (один раз на шаг)
    void pre(int k, const T* h, T* out) const {
        for (int q = 0; q < KP; q++) {
            T a = ptr[ob[k] + q];
            const T* wr = &ptr[oW[k] + (size_t)q * H];
            for (int j = 0; j < H; j++) a += wr[j] * h[j];
            out[q] = a;
        }
    }
    T score(int k, const T* pr, const float* f) const {
        T sc = 0;
        for (int q = 0; q < KP; q++) {
            T a = pr[q];
            const T* u = &ptr[oU[k] + (size_t)q * nf[k]];
            for (int j = 0; j < nf[k]; j++) a += u[j] * (T)f[j];
            sc += ptr[ov[k] + q] * std::tanh(a);
        }
        return sc;
    }
    // обратный проход указателя: ds[i] — градиент по оценке кандидата i; копит градиенты в gp, добавляет в dh
    void ptrBack(int k, const T* h, const T* pr, const float* feats, int n, const T* ds, std::vector<T>& gp, T* dh) const {
        T dpre[KP] = {};
        for (int i = 0; i < n; i++) {
            if (ds[i] == T(0)) continue;
            const float* f = feats + (size_t)i * nf[k];
            for (int q = 0; q < KP; q++) {
                T a = pr[q];
                const T* u = &ptr[oU[k] + (size_t)q * nf[k]];
                for (int j = 0; j < nf[k]; j++) a += u[j] * (T)f[j];
                const T t = std::tanh(a);
                gp[ov[k] + q] += ds[i] * t;
                const T da = ds[i] * ptr[ov[k] + q] * (T(1) - t * t);
                T* gu = &gp[oU[k] + (size_t)q * nf[k]];
                for (int j = 0; j < nf[k]; j++) gu[j] += da * (T)f[j];
                dpre[q] += da;
            }
        }
        for (int q = 0; q < KP; q++) {
            if (dpre[q] == T(0)) continue;
            gp[ob[k] + q] += dpre[q];
            const T* wr = &ptr[oW[k] + (size_t)q * H];
            T* gw = &gp[oW[k] + (size_t)q * H];
            for (int j = 0; j < H; j++) {
                gw[j] += dpre[q] * h[j];
                dh[j] += dpre[q] * wr[j];
            }
        }
    }
};
typedef PNetT<float> PNet;
static const int P_MAGIC = 0x504E3235;     // «PN25»: признаки механик «всё оставшееся» и команд, «выше/ниже» (v21)
static const int P_MAGIC24 = 0x504E3234;   // «PN24»: Помощь и Подготовка в слотах способностей — переносится
static const int P_MAGIC23 = 0x504E3233;   // «PN23»: этап 5б, признаки состояний — переносится
static const int P_MAGIC22 = 0x504E3232;   // «PN22»: ступени решения (шаг 4) — переносится; «PN21» не подходит
// строка выходного слоя старых весов (PN22–PN24, oldNAB слотов способностей, IN_N24 взаимодействий) -> строка PN25
inline int mapOldRow(int r, int oldNAB) {
    if (r < OF_AB + oldNAB) return r;                                     // тип, категория, слоты способностей
    if (r < OF_AB + oldNAB + IN_N24) return OF_IN + (r - OF_AB - oldNAB);   // взаимодействия
    return OF_SW + (r - OF_AB - oldNAB - IN_N24);                           // запасное оружие
}
static bool saveP(const std::string& fn, const PNet& n) {
    std::ofstream f(fn, std::ios::binary);
    if (!f) return false;
    const int hd[6] = {P_MAGIC, n.D, n.E, n.H, NFIX, (int)n.ptr.size()};
    f.write((const char*)hd, sizeof hd);
    f.write((const char*)n.core.p.data(), n.core.p.size() * sizeof(float));
    f.write((const char*)n.ptr.data(), n.ptr.size() * sizeof(float));
    return (bool)f;
}
static bool loadP(const std::string& fn, PNet& n, bool warn) {
    std::ifstream f(fn, std::ios::binary);
    if (!f) return false;
    int hd[6];
    f.read((char*)hd, sizeof hd);
    PNet probe;
    probe.init(1);
    if (f && (hd[0] == P_MAGIC22 || hd[0] == P_MAGIC23 || hd[0] == P_MAGIC24) && hd[2] == EMB && hd[3] == HID && hd[1] <= D_IN && hd[4] <= NFIX) {
        // перенос старых весов: PN22 (вход короче — новые признаки состояний в конце, указатель на врага короче) и PN23
        // (меньше слотов способностей: нет Помощи и Подготовки). У новых входов и признаков веса нулевые, у новых слотов —
        // смещение −20: пока сеть не переобучат, она ходит ровно как раньше и новых действий не выбирает
        PNetT<float> old;
        old.D = hd[1];
        old.NA = hd[4];
        old.nf[1] = hd[0] == P_MAGIC22 ? FE : FEP24;
        old.init(1);
        if (hd[5] != (int)old.ptr.size()) return false;
        f.read((char*)old.core.p.data(), old.core.p.size() * sizeof(float));
        f.read((char*)old.ptr.data(), old.ptr.size() * sizeof(float));
        if (!f) return false;
        n = probe;
        for (auto& v : n.core.p) v = 0;
        for (auto& v : n.ptr) v = 0;
        const GruNet<float>&o = old.core;
        GruNet<float>& c = n.core;
        // эмбеддинг E×D по строкам: старый вход — начало нового
        for (int i = 0; i < EMB; i++) std::copy(o.p.begin() + o.oWe + (size_t)i * old.D, o.p.begin() + o.oWe + (size_t)(i + 1) * old.D, c.p.begin() + c.oWe + (size_t)i * D_IN);
        // от смещения эмбеддинга до выходного слоя устройство то же
        std::copy(o.p.begin() + o.obe, o.p.begin() + o.oWo, c.p.begin() + c.obe);
        // выходной слой: строки до слотов способностей и сами старые слоты — на месте; взаимодействия (старых IN_N24) — с
        // начала OF_IN; запасное оружие — в OF_SW. Новые слоты и новые взаимодействия закрыты смещением −20
        const int oldNAB = old.NA - (ST_N + CAT_N + IN_N24 + NSWAP);
        for (int r = 0; r < NFIX; r++) c.p[c.obo + r] = -20.f;
        for (int r = 0; r < old.NA; r++) {
            const int nr = mapOldRow(r, oldNAB);
            std::copy(o.p.begin() + o.oWo + (size_t)r * HID, o.p.begin() + o.oWo + (size_t)(r + 1) * HID, c.p.begin() + c.oWo + (size_t)nr * HID);
            c.p[c.obo + nr] = o.p[o.obo + r];
        }
        std::copy(o.p.begin() + o.oWv, o.p.end(), c.p.begin() + c.oWv);   // ценность
        // указатели: у врага строки U длиной nf → FEP, остальное по смещениям
        for (int k = 1; k <= 2; k++) {
            for (int q = 0; q < KP; q++)
                std::copy(old.ptr.begin() + old.oU[k] + (size_t)q * old.nf[k], old.ptr.begin() + old.oU[k] + (size_t)(q + 1) * old.nf[k], n.ptr.begin() + n.oU[k] + (size_t)q * n.nf[k]);
            std::copy(old.ptr.begin() + old.oW[k], old.ptr.begin() + old.oW[k] + (size_t)KP * HID, n.ptr.begin() + n.oW[k]);
            std::copy(old.ptr.begin() + old.ob[k], old.ptr.begin() + old.ob[k] + KP, n.ptr.begin() + n.ob[k]);
            std::copy(old.ptr.begin() + old.ov[k], old.ptr.begin() + old.ov[k] + KP, n.ptr.begin() + n.ov[k]);
        }
        g_altOn = false;
        if (warn) std::fprintf(stderr, "сеть-боец %s: веса %s перенесены в PN25 (новые признаки — с нулевыми весами, новые действия закрыты до обучения)\n", fn.c_str(), hd[0] == P_MAGIC22 ? "PN22" : (hd[0] == P_MAGIC23 ? "PN23" : "PN24"));
        return true;
    }
    if (!f || hd[0] != P_MAGIC || hd[1] != D_IN || hd[2] != EMB || hd[3] != HID || hd[4] != NFIX || hd[5] != (int)probe.ptr.size()) {
        if (warn)
            std::fprintf(stderr, "ВНИМАНИЕ: веса сети-бойца %s не подходят к движку (заголовок %x/%d/%d/%d/%d/%d, нужно %x/%d/%d/%d/%d/%d) — сеть отключена, бойцы ходят обычным ботом\n",
                         fn.c_str(), hd[0], hd[1], hd[2], hd[3], hd[4], hd[5], P_MAGIC, D_IN, EMB, HID, NFIX, (int)probe.ptr.size());
        return false;
    }
    n = probe;
    f.read((char*)n.core.p.data(), n.core.p.size() * sizeof(float));
    f.read((char*)n.ptr.data(), n.ptr.size() * sizeof(float));
    return (bool)f;
}

// Одно решение сети: всё, что нужно, чтобы заново посчитать вероятность выбранного действия (обучение). Ступени: тип st;
// для «применить» — категория cat и слот ab (маска слотов abMask с их категориями abCat); для «взаимодействия» — in и,
// для смены оружия, sw; затем указатель
struct Decision {
    std::vector<float> x;                                    // наблюдение
    bool stMask[ST_N] = {}, catMask[CAT_N] = {}, abMask[NAB] = {}, inMask[IN_N] = {}, swapMask[NSWAP] = {};
    int abCat[NAB] = {};
    int st = ST_END, cat = -1, ab = -1, in = -1, sw = -1;
    int ptrKind = PT_NONE, nc = 0, c = -1;                   // указатель: вид, число кандидатов, выбранный
    std::vector<float> feats;                                // nc × (FE или FC)
    std::vector<char> pmask;                                 // допустимые кандидаты
    float logp = 0, value = 0, reward = 0;
};
static void copyMasks(const Obs& o, Decision& d) {
    std::copy(o.stMask, o.stMask + ST_N, d.stMask);
    std::copy(o.catMask, o.catMask + CAT_N, d.catMask);
    std::copy(o.abMask, o.abMask + NAB, d.abMask);
    std::copy(o.abCat, o.abCat + NAB, d.abCat);
    std::copy(o.inMask, o.inMask + IN_N, d.inMask);
    std::copy(o.swapMask, o.swapMask + NSWAP, d.swapMask);
}
// слоты способностей выбранной категории
static void abSub(const Decision& d, bool* m) {
    for (int j = 0; j < NAB; j++) m[j] = d.abMask[j] && d.abCat[j] == d.cat;
}
// маскированная вероятность и её градиент: logp выбранного c; если g != 0 — копит в dz[j] += g·(1[j=c] − p_j)
template <class T>
static T maskedLogp(const T* z, const bool* m, int n, int c, T g, T* dz, T entCoef = 0, T* entOut = nullptr) {
    T mx = -1e30;
    for (int j = 0; j < n; j++)
        if (m[j]) mx = std::max(mx, z[j]);
    T tot = 0;
    T p[256];
    for (int j = 0; j < n; j++) {
        p[j] = m[j] ? std::exp(z[j] - mx) : T(0);
        tot += p[j];
    }
    T H = 0;
    for (int j = 0; j < n; j++) {
        p[j] /= tot;
        if (p[j] > 1e-12) H -= p[j] * std::log(p[j]);
    }
    if (entOut) *entOut += H;
    if (dz)
        for (int j = 0; j < n; j++) {
            if (!m[j]) continue;
            dz[j] += g * ((j == c ? T(1) : T(0)) - p[j]);
            if (entCoef != T(0) && p[j] > 1e-12) dz[j] += entCoef * p[j] * (std::log(p[j]) + H);   // −β·H в потере
        }
    return std::log(std::max(p[c], (T)1e-30));
}
// log-вероятность решения d при памяти h и логитах lg. g — градиент потери по logp (0 — только посчитать): копит
// dlg (логиты), градиенты указателей в gp и вклад в dh. entCoef — коэффициент энтропии типа примитива.
template <class T>
static T decisionLogp(const PNetT<T>& N, const Decision& d, const T* h, const T* lg, T g, T* dlg, std::vector<T>* gp, T* dh, T entCoef = 0, T* ent = nullptr) {
    T lp = maskedLogp(lg + OF_TYPE, d.stMask, ST_N, d.st, g, dlg ? dlg + OF_TYPE : nullptr, entCoef, ent);
    if (d.st == ST_APPLY) {
        lp += maskedLogp(lg + OF_CAT, d.catMask, CAT_N, d.cat, g, dlg ? dlg + OF_CAT : nullptr);
        bool sub[NAB];
        abSub(d, sub);
        lp += maskedLogp(lg + OF_AB, sub, NAB, d.ab, g, dlg ? dlg + OF_AB : nullptr);
    } else if (d.st == ST_INTERACT) {
        lp += maskedLogp(lg + OF_IN, d.inMask, IN_N, d.in, g, dlg ? dlg + OF_IN : nullptr);
        if (d.in == IN_SWAP) lp += maskedLogp(lg + OF_SW, d.swapMask, NSWAP, d.sw, g, dlg ? dlg + OF_SW : nullptr);
    }
    if (d.ptrKind != PT_NONE && d.c >= 0) {
        const int k = d.ptrKind, n = d.nc, f = N.nf[k];
        T pr[KP];
        N.pre(k, h, pr);
        std::vector<T> sc(n), ds(n, T(0));
        std::vector<char> mk(d.pmask);
        for (int i = 0; i < n; i++) sc[i] = mk[i] ? N.score(k, pr, d.feats.data() + (size_t)i * f) : T(0);
        lp += maskedLogp(sc.data(), (const bool*)mk.data(), n, d.c, g, gp ? ds.data() : nullptr);
        if (gp) N.ptrBack(k, h, pr, d.feats.data(), n, ds.data(), *gp, dh);
    }
    return lp;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ход по сети: примитив за примитивом, пока сеть не выберет «конец хода» (не больше 24 шагов)
// ---------------------------------------------------------------------------------------------------------------------
struct Side {
    bool active = false;
    const PNet* net = nullptr;          // своя сеть стороны (соперник из пула); нет — общая
    std::vector<float> h;
    bool record = false;                // писать решения (обучение)
    std::vector<Decision> traj;
    double prevPhi = 0;                 // потенциал прошлого решения (доля своих ХП минус доля ХП врага)
    bool hasPrev = false;
};
struct Ctx {
    const PNet* net = nullptr;
    bool greedy = true;
    Rng rng;
    Side side[2];
    double fallback[2] = {0, 0};        // шанс, что боец в этот ход играет обычным ботом (ИНТ 12–15)
    long long steps = 0, illegal = 0;   // шагов и отказов исполнителя (должно быть 0: маски не пускают недопустимое)
    // метрики хода (шаг 4): ходов сети, из них с перемещением, упёрлись в предел решений, завершены движком (легален
    // только «конец хода»), решений, где «конец хода» был закрыт запретом пустого хода
    long long turns = 0, movedTurns = 0, capHits = 0, forcedEnds = 0, endBlocked = 0;
    double gamma = 0.99, timePen = 0;   // для потенциальной награды и штрафа за решение
    double roundPen = 0;                // штраф за каждый свой раунд боя (затяжка невыгодна; решение владельца 2026-10-28)
    double temp = 1.0;                  // температура выбора, когда greedy = false (1 — как в обучении)
};
static thread_local Ctx* g_pctx = nullptr;
// реплей боя сети: каждый шаг — отдельный кадр с решением сети (что выбрала, с какой уверенностью, как оценивает позицию)
static thread_local bool g_netVerbose = false;
// потенциал позиции (для награды): доля своих ХП минус доля ХП главного врага
inline double potential(const World& w, int team) {
    const Fighter& me = w.f[team];
    const Fighter& e = w.f[1 - team];
    return (double)std::max(0, me.hp) / me.maxHp - (double)std::max(0, e.hp) / e.maxHp;
}

// температура выбора по вероятностям: 1 — как в обучении, меньше — ближе к жадному (замер затяжки, 2026-11-01)
static thread_local double g_pickTemp = 1.0;
static int pick(const float* lg, const bool* mask, int n, bool greedy, Rng& r) {
    float mx = -1e30f;
    int any = -1;
    for (int j = 0; j < n; j++)
        if (mask[j]) {
            mx = std::max(mx, lg[j]);
            any = j;
        }
    if (any < 0) return -1;
    double pr[256], tot = 0;
    for (int j = 0; j < n; j++) {
        pr[j] = mask[j] ? std::exp(((double)lg[j] - mx) / (greedy ? 1.0 : g_pickTemp)) : 0.0;
        tot += pr[j];
    }
    if (greedy) {
        int b = any;
        for (int j = 0; j < n; j++)
            if (mask[j] && pr[j] > pr[b]) b = j;
        return b;
    }
    double u = r.unit() * tot;
    for (int j = 0; j < n; j++) {
        u -= pr[j];
        if (u <= 0 && mask[j]) return j;
    }
    return any;
}
// кандидаты указателя в решение d: из списка клеток (skipFirst — без своей клетки) или из слотов врагов
static void fillCells(Decision& d, const std::vector<Cand>& cs, bool skipFirst) {
    d.ptrKind = PT_CELL;
    d.nc = (int)cs.size();
    d.feats.resize((size_t)d.nc * FC);
    d.pmask.assign(d.nc, 1);
    for (int i = 0; i < d.nc; i++) std::copy(cs[i].f, cs[i].f + FC, d.feats.begin() + (size_t)i * FC);
    if (skipFirst && d.nc > 0) d.pmask[0] = 0;
}
static void fillEnemies(Decision& d, const Obs& o, const bool* mask) {
    d.ptrKind = PT_ENEMY;
    d.nc = MAXE;
    d.feats.resize((size_t)MAXE * FEP);
    d.pmask.assign(MAXE, 0);
    for (int j = 0; j < MAXE; j++) {
        std::copy(o.ef[j], o.ef[j] + FEP, d.feats.begin() + (size_t)j * FEP);
        d.pmask[j] = mask[j];
    }
}
// «Почему» для реплея (шаг 4): на ступени — выбранное, его вероятность и два следующих варианта с вероятностями
static thread_local std::string g_stages;
static std::string topJson(const char* stage, const float* z, const bool* m, int n, int c, const std::function<std::string(int)>& lab) {
    float mx = -1e30f;
    for (int j = 0; j < n; j++)
        if (m[j]) mx = std::max(mx, z[j]);
    std::vector<std::pair<double, int>> pr;
    double tot = 0;
    for (int j = 0; j < n; j++)
        if (m[j]) {
            const double e = std::exp((double)z[j] - mx);
            pr.push_back({e, j});
            tot += e;
        }
    double pc = 0;
    for (auto& q : pr) {
        q.first /= tot;
        if (q.second == c) pc = q.first;
    }
    std::sort(pr.begin(), pr.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b) { return a.first > b.first || (a.first == b.first && a.second < b.second); });
    auto jq2 = [](const std::string& t) {
        std::string r = "\"";
        for (char ch : t) r += (ch == '"' || ch == '\\') ? std::string("\\") + ch : std::string(1, ch);
        return r + "\"";
    };
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.3f", pc);
    std::string o = std::string("{\"n\":") + jq2(stage) + ",\"c\":" + jq2(lab(c)) + ",\"p\":" + buf + ",\"alt\":[";
    int k = 0;
    for (auto& q : pr) {
        if (q.second == c || k >= 2) continue;
        std::snprintf(buf, sizeof buf, "%.3f", q.first);
        o += std::string(k ? "," : "") + "[" + jq2(lab(q.second)) + "," + buf + "]";
        k++;
    }
    return o + "]}";
}
// Решение по ступеням: тип → (применить: категория → способность | взаимодействие: что → какое оружие) → указатель.
// Заполняет d, возвращает примитив (kind = PR_END — конец хода). h — память после шага, lg — логиты.
static Prim decide(const PNet& N, World& w, int idx, const TurnState& ts, const Obs& o, const float* h, const float* lg, bool greedy, Rng& rng, Decision& d) {
    Fighter& me = w.f[idx];
    Prim p;
    copyMasks(o, d);
    const bool why = g_netVerbose;
    if (why) g_stages.clear();
    auto stage = [&](const char* name, const float* z, const bool* m, int n, int c, const std::function<std::string(int)>& lab) {
        if (why) g_stages += std::string(g_stages.empty() ? "" : ",") + topJson(name, z, m, n, c, lab);
    };
    d.st = std::max(0, pick(lg + OF_TYPE, o.stMask, ST_N, greedy, rng));
    stage("тип", lg + OF_TYPE, o.stMask, ST_N, d.st, [](int j) { return std::string(STN[j]); });
    // указатель: cells — координаты кандидатов-клеток (для подписи в реплее)
    auto choosePtr = [&](const std::vector<Cand>* cells) {
        float pr[KP];
        N.pre(d.ptrKind, h, pr);
        std::vector<float> sc(d.nc);
        for (int i = 0; i < d.nc; i++) sc[i] = d.pmask[i] ? N.score(d.ptrKind, pr, d.feats.data() + (size_t)i * N.nf[d.ptrKind]) : 0.f;
        d.c = pick(sc.data(), (const bool*)d.pmask.data(), d.nc, greedy, rng);
        if (d.c >= 0)
            stage(d.ptrKind == PT_CELL ? "клетка" : "цель", sc.data(), (const bool*)d.pmask.data(), d.nc, d.c, [&](int j) {
                if (cells) return "(" + std::to_string((*cells)[j].x) + "," + std::to_string((*cells)[j].y) + ")";
                return std::string("боец ") + std::to_string(o.enemy[j]);
            });
        return d.c;
    };
    if (d.st == ST_END) return Prim{};
    if (d.st == ST_MOVE) {
        fillCells(d, o.move, true);
        if (choosePtr(&o.move) < 0) return Prim{};
        p.kind = PR_MOVE;
        p.x = o.move[d.c].x;
        p.y = o.move[d.c].y;
        return p;
    }
    if (d.st == ST_APPLY) {
        d.cat = pick(lg + OF_CAT, o.catMask, CAT_N, greedy, rng);
        if (d.cat < 0) return Prim{};
        stage("категория", lg + OF_CAT, o.catMask, CAT_N, d.cat, [](int j) { return std::string(CATN[j]); });
        bool sub[NAB];
        abSub(d, sub);
        d.ab = pick(lg + OF_AB, sub, NAB, greedy, rng);
        if (d.ab < 0) return Prim{};
        stage("способность", lg + OF_AB, sub, NAB, d.ab, [](int j) {
            if (j == AB_WEAPON) return std::string("оружие");
            if (j == AB_DASH) return std::string("Рывок");
            if (j == AB_DODGE) return std::string("Уклонение");
            if (j == AB_DISENG) return std::string("Отход");
            if (j == AB_HELP) return std::string("Помощь");
            if (j == AB_READY) return std::string("Подготовка");
            return "способность " + std::to_string(j - AB_SPEC + 1);
        });
        if (d.ab == AB_WEAPON) {
            fillEnemies(d, o, o.atkMask);
            if (choosePtr(nullptr) < 0) return Prim{};
            p.kind = PR_ATTACK;
            p.a = o.enemy[d.c];
            p.b = hasWhen(me.b, W_RECKLESS) ? 1 : 0;   // «когда → что»: безрассудство — условие правила
        } else if (d.ab == AB_DASH)
            p.kind = PR_DASH;
        else if (d.ab == AB_DODGE)
            p.kind = PR_DODGE;
        else if (d.ab == AB_DISENG) {
            p.kind = PR_DISENGAGE;
            p.a = (me.b.disengB && !ts.bonusUsed) ? 1 : 0;
        } else if (d.ab == AB_HELP) {   // Помощь: против какого врага вплотную
            fillEnemies(d, o, o.helpTgt);
            if (choosePtr(nullptr) < 0) return Prim{};
            p.kind = PR_HELP;
            p.a = o.enemy[d.c];
        } else if (d.ab == AB_READY)
            p.kind = PR_READY;
        else {
            const int k = d.ab - AB_SPEC;
            p.kind = PR_SPELL;
            p.a = k;
            const Spec& s = me.b.abil[k];
            if (!o.spellCells[k].empty()) {
                fillCells(d, o.spellCells[k], false);
                if (choosePtr(&o.spellCells[k]) < 0) return Prim{};
                p.x = o.spellCells[k][d.c].x;
                p.y = o.spellCells[k][d.c].y;
            } else if (!s.self) {
                fillEnemies(d, o, o.spellTgt[k]);
                if (choosePtr(nullptr) < 0) return Prim{};
                p.b = o.enemy[d.c];
            }
        }
        return p;
    }
    // взаимодействие
    d.in = pick(lg + OF_IN, o.inMask, IN_N, greedy, rng);
    if (d.in < 0) return Prim{};
    stage("взаимодействие", lg + OF_IN, o.inMask, IN_N, d.in, [](int j) { return std::string(INN[j]); });
    if (d.in == IN_SWAP) {
        d.sw = pick(lg + OF_SW, o.swapMask, NSWAP, greedy, rng);
        if (d.sw < 0) return Prim{};
        stage("оружие", lg + OF_SW, o.swapMask, NSWAP, d.sw, [](int j) { return "запасное " + std::to_string(j + 1); });
        p.kind = PR_SWAP;
        p.a = d.sw;
    } else if (d.in == IN_ORDER) {
        bool all[MAXE];
        for (int j = 0; j < MAXE; j++) all[j] = o.enemy[j] >= 0;
        fillEnemies(d, o, all);
        if (choosePtr(nullptr) < 0) return Prim{};
        p.kind = PR_ORDER;
        p.a = o.enemy[d.c];
    } else if (d.in == IN_DROPCONC)
        p.kind = PR_DROPCONC;
    else if (d.in == IN_ALTUP || d.in == IN_ALTDOWN) {
        p.kind = PR_ALT;
        p.a = w.f[idx].alt + (d.in == IN_ALTUP ? 1 : -1);
    } else
        p.kind = PR_SURGE;
    return p;
}
// Метка для обучения копированием: решение бота p (nullptr — конец хода) по ступеням сети. false — непредставимо
// (например, бот закончил пустой ход, а сети это запрещено, или выбранного нет в масках); cellOf — индекс клетки-кандидата
// для (x, y) (добавляет кандидата, если его нет)
static bool labelOf(const World& w, int idx, Obs& o, const Prim* p, Decision& d, const std::function<int(std::vector<Cand>&, int, int)>& cellOf) {
    const Fighter& me = w.f[idx];
    copyMasks(o, d);
    auto slotOf = [&](int i) {
        for (int j = 0; j < MAXE; j++)
            if (o.enemy[j] == i) return j;
        return -1;
    };
    const int kind = p ? p->kind : PR_END;
    if (kind == PR_END) {
        d.st = ST_END;
        return d.stMask[ST_END];
    }
    if (kind == PR_MOVE) {
        d.st = ST_MOVE;
        if (!d.stMask[ST_MOVE] || (p->x == me.x && p->y == me.y)) return false;
        d.c = cellOf(o.move, p->x, p->y);
        fillCells(d, o.move, true);
        d.pmask[d.c] = 1;
        return true;
    }
    if (kind == PR_ATTACK || kind == PR_SPELL || kind == PR_DASH || kind == PR_DODGE || kind == PR_DISENGAGE || kind == PR_HELP || kind == PR_READY) {
        d.st = ST_APPLY;
        d.ab = kind == PR_ATTACK ? AB_WEAPON : kind == PR_DASH ? AB_DASH : kind == PR_DODGE ? AB_DODGE : kind == PR_DISENGAGE ? AB_DISENG : kind == PR_HELP ? AB_HELP : kind == PR_READY ? AB_READY : AB_SPEC + p->a;
        if (kind == PR_SPELL && (p->a < 0 || p->a >= MAXA)) return false;
        if (!d.stMask[ST_APPLY] || !d.abMask[d.ab]) return false;
        d.cat = d.abCat[d.ab];
        if (kind == PR_ATTACK) {
            d.c = slotOf(p->a);
            if (d.c < 0 || !o.atkMask[d.c]) return false;
            fillEnemies(d, o, o.atkMask);
        } else if (kind == PR_SPELL) {
            const int k = p->a;
            const Spec& sp = me.b.abil[k];
            if (!o.spellCells[k].empty() || (sp.ground && !sp.self) || (sp.self && sp.teleFt > 0)) {
                d.c = cellOf(o.spellCells[k], p->x, p->y);
                fillCells(d, o.spellCells[k], false);
            } else if (!sp.self) {
                d.c = slotOf(p->b);
                if (d.c < 0 || !o.spellTgt[k][d.c]) return false;
                fillEnemies(d, o, o.spellTgt[k]);
            }
        } else if (kind == PR_HELP) {   // Помощь: враг вплотную (v21)
            d.c = slotOf(p->a);
            if (d.c < 0 || !o.helpTgt[d.c]) return false;
            fillEnemies(d, o, o.helpTgt);
        }
        return true;
    }
    d.st = ST_INTERACT;
    d.in = kind == PR_SWAP ? IN_SWAP : kind == PR_ORDER ? IN_ORDER : kind == PR_DROPCONC ? IN_DROPCONC : kind == PR_SURGE ? IN_SURGE
         : kind == PR_ALT ? (p->a > me.alt ? IN_ALTUP : IN_ALTDOWN) : -1;   // бот меняет высоту и на несколько уровней — метка «на шаг»
    if (d.in < 0 || !d.stMask[ST_INTERACT] || !d.inMask[d.in]) return false;
    if (kind == PR_SWAP) {
        d.sw = p->a;
        return d.sw >= 0 && d.sw < NSWAP && d.swapMask[d.sw];
    }
    if (kind == PR_ORDER) {
        bool all[MAXE];
        for (int j = 0; j < MAXE; j++) all[j] = o.enemy[j] >= 0;
        d.c = slotOf(p->a);
        if (d.c < 0) return false;
        fillEnemies(d, o, all);
    }
    return true;
}
static void netTurnP(World& w, int idx) {
    Ctx* c = g_pctx;
    Fighter& me = w.f[idx];
    if (!c || !c->side[me.team].active || (c->fallback[me.team] > 0 && me.ai.unit() < c->fallback[me.team])) {
        takeTurn(w, idx);
        return;
    }
    Side& sd = c->side[me.team];
    const PNet* np = sd.net ? sd.net : c->net;
    if (!np || idx >= 2) {   // приспешниками пока управляет бот (ТЗ, №18 — этап 6)
        takeTurn(w, idx);
        return;
    }
    TurnState ts;
    if (!beginTurn(w, idx, ts)) return;
    if (forcedFlee(w, idx, ts)) return;
    const PNet& N = *np;
    g_pickTemp = c->temp;
    if (sd.h.empty()) sd.h.assign(N.H, 0.f);
    Obs o;
    typename GruNet<float>::Step st;
    std::vector<float> lg(NFIX);
    c->turns++;
    bool moved = false, ended = false;
    int stepN = 0;
    for (; stepN < TURN_CAP && !over(w) && me.hp > 0; stepN++) {
        buildObs(w, idx, ts, o);
        if (onlyEnd(o)) {   // тратить нечего: ход завершает движок, в обучение шаг не идёт
            if (sd.record && stepN == 0 && sd.hasPrev && !sd.traj.empty()) sd.traj.back().reward -= (float)c->roundPen;   // раунд всё равно прошёл
            c->forcedEnds++;
            ended = true;
            break;
        }
        if (!o.stMask[ST_END]) c->endBlocked++;
        float v;
        N.core.forward(o.x.data(), sd.h.data(), st, lg.data(), v);
        sd.h = st.h;
        c->steps++;
        Decision d;
        const Prim p = decide(N, w, idx, ts, o, sd.h.data(), lg.data(), c->greedy, c->rng, d);
        if (sd.record) {   // награда прошлого решения: потенциальная разность γΦ(s') − Φ(s) и штраф за время
            const double phi = potential(w, me.team);
            if (sd.hasPrev && !sd.traj.empty())
                sd.traj.back().reward += (float)(c->gamma * phi - sd.prevPhi - c->timePen - (stepN == 0 ? c->roundPen : 0.0));
            sd.prevPhi = phi;
            sd.hasPrev = true;
            d.x = o.x;
            d.value = v;
            d.logp = decisionLogp<float>(N, d, sd.h.data(), lg.data(), 0.f, nullptr, nullptr, nullptr);
            sd.traj.push_back(std::move(d));
        }
        if (g_netVerbose) {   // решение по ступеням: что выбрано, уверенность, два запасных варианта
            const float lp = decisionLogp<float>(N, d, sd.h.data(), lg.data(), 0.f, nullptr, nullptr, nullptr);
            char buf[220];
            std::snprintf(buf, sizeof buf, "{\"k\":\"net\",\"who\":%d,\"prim\":%d,\"a\":%d,\"b\":%d,\"x\":%d,\"y\":%d,\"p\":%.3f,\"v\":%.3f,\"st\":[", idx, p.kind, p.a, p.b, p.x,
                          p.y, std::exp((double)lp), (double)v);
            evPush(buf + g_stages + "]}");
        }
        if (p.kind <= PR_END) {
            ended = true;
            break;
        }
        std::string why;
        if (!applyPrim(w, idx, ts, p, true, &why)) {
            c->illegal++;
            if (std::getenv("DND_PDEBUG")) std::fprintf(stderr, "отказ: %s %d/%d (%d,%d): %s\n", PRN[p.kind], p.a, p.b, p.x, p.y, why.c_str());
            ended = true;
            break;
        }
        moved |= p.kind == PR_MOVE;
        if (g_netVerbose && g_human.emit && !over(w)) g_human.emit(g_curRound, idx, 0);   // кадр после каждого шага сети
    }
    c->movedTurns += moved;
    if (!ended && stepN >= TURN_CAP) {   // предохранитель: ход оборван на пределе решений — в метрики и в журнал
        c->capHits++;
        tripwire("ход сети: предел решений за ход", TURN_CAP, stepN);
    }
    endOfTurn(me);
}

// Подключение к бою: если веса найдены, бойцы с ИНТ >= 12 ходят по сети (12–15: в 25% ходов — обычным ботом), иначе
// боец ходит ботом. Веса: DND_NETP (путь; "off" — без сети) или net_fighter_p.bin.
inline const PNet* primNet() {
    static const PNet* net = []() -> const PNet* {
        const char* e = std::getenv("DND_NETP");
        const std::string path = e ? e : "net_fighter_p.bin";
        if (path == "off") return nullptr;
        static PNet n;
        if (!loadP(path, n, true)) return nullptr;
        return &n;
    }();
    return net;
}
}   // namespace pn

// Подключение к бою: новая сеть (prim_net, веса net_fighter_p.bin) или, если задана DND_NET_LEGACY, старая со стойками.
// Бойцы с ИНТ >= 12 ходят по сети (12–15: в 25% ходов — обычным ботом, 16+: всегда сеть, жадно); g_forceNetMask — сеть
// независимо от ИНТ (игра против нейросети). Нет весов — все ходят ботом.
inline bool anyFighterNet() { return pn::primNet() != nullptr || fighterNet() != nullptr; }
struct AutoNet {
    NetCtx ctx;
    pn::Ctx pctx;
    bool on = false, prim = false;
    AutoNet(const Boss& A, const Boss& B) {
        const pn::PNet* pnet = pn::primNet();
        const Net* net = pnet ? nullptr : fighterNet();
        if ((!pnet && !net) || g_ctx || pn::g_pctx) return;   // нет сети или бой ведёт обучение со своим контекстом
        const Boss* bs[2] = {&A, &B};
        for (int i = 0; i < 2; i++) {
            const int in = bs[i]->stats[INT];
            const bool forced = (g_forceNetMask >> i) & 1;
            if (in < 12 && !forced) continue;
            (pnet ? pctx.side[i].active : ctx.side[i].active) = true;
            (pnet ? pctx.fallback[i] : ctx.fallback[i]) = (in <= 15 && !forced) ? 0.25 : 0.0;
            on = true;
        }
        if (!on) return;
        if (pnet) {
            // в игре и калибровке сеть выбирает по вероятностям с температурой 0.25 (решение 2026-11-01): всегда самый
            // вероятный ход у двух одинаковых сетей давал зеркальное ожидание и затяжные бои (сама с собой 5% до предела
            // раундов против 2%), сила против бота почти та же (57.2% против 58.4%). DND_TEMP=0 — всегда самый вероятный
            const char* te = std::getenv("DND_TEMP");
            const double t = te ? std::atof(te) : 0.25;
            prim = true;
            pctx.net = pnet;
            pctx.greedy = t <= 0;
            pctx.temp = t > 0 ? t : 1.0;
            pctx.rng.seed(rng.next());   // своё зерно на бой: по зерну боя результат тот же
            pn::g_pctx = &pctx;
        } else {
            ctx.net = net;
            ctx.greedy = true;
            g_ctx = &ctx;
        }
    }
    bool controls(int team) const { return on && (prim ? pctx.side[team].active : ctx.side[team].active); }
    ~AutoNet() {
        if (on && prim) pn::g_pctx = nullptr;
        if (on && !prim) g_ctx = nullptr;
    }
};
