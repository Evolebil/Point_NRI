// Бой: логово, состав команд, расстановка, цикл боя (fightRoster, fight)
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// сеть-боец различает способности по номеру: не больше MAXA (и конструктор не даёт больше)
static const int MAXA = 12;
// принудительное управление сетью независимо от ИНТ (биты по сторонам): игра против нейросети-бойца
static thread_local int g_forceNetMask = 0;

#include "human_turn.h"
#include "prim_net.h"   // v21: сеть-боец на примитивах и карте (№4, №5)
#include "custom_map.h"
// сведения о последнем бое для просмотрщика
struct FightInfo {
    bool ctrl[2] = {false, false};   // боссом управляет нейросеть
    int rounds = 0, result = 2, reason = 0;   // reason: 0 ничья, 1 победа убийством, 2 враг вышел за арену, 3 враг оторвался, 4 враг выбился из сил
    int aw = 0, ah = 0, theme = 0;
};
thread_local FightInfo g_fightInfo;
// Действия логова (DMG, v21): в начале раунда (инициатива 20) главный босс с логовом действует логовом — без действия и
// реакции, одно и то же два раунда подряд нельзя. Центр — известный враг, рядом с которым больше всего врагов (поровну —
// у кого меньше доля хитов). Обвал: в 10 фт от центра каждый враг — спасбросок ЛОВ против СЛ босса, провал — уровень×2к10
// дробящего, успех — половина (Увёртливость действует). Трясина: клетки в 10 фт от центра — труднопроходимая местность.
void lairAction(World& w, int i) {
    Fighter& me = w.f[i];
    if (me.hp <= 0 || me.b.lair <= 0 || me.cond[C_STUN] || me.cond[C_PARAL] || me.cond[C_INCAP] || me.supp > 0) return;
    int best = -1, bn = -1;
    double bf = 2;
    for (size_t e = 0; e < w.f.size(); e++) {
        const Fighter& t = w.f[e];
        if (t.hp <= 0 || t.team == me.team || !isKnown(w, me.team, (int)e)) continue;
        int n = 0;
        for (auto& u : w.f)
            if (u.hp > 0 && u.team != me.team && distFt(t, u) <= 10) n++;
        const double fr = (double)t.hp / std::max(1, t.maxHp);
        if (n > bn || (n == bn && fr < bf)) best = (int)e, bn = n, bf = fr;
    }
    if (best < 0) return;
    const int cx = w.f[best].x, cy = w.f[best].y, kind = me.lairLast == 1 ? 2 : 1;
    me.lairLast = kind;
    evPush("{\"k\":\"lair\",\"who\":" + std::to_string(i) + ",\"kind\":" + std::to_string(kind) + ",\"x\":" + std::to_string(cx) + ",\"y\":" + std::to_string(cy) + "}");
    if (kind == 2) {
        for (int y = cy - 2; y <= cy + 2; y++)
            for (int x = cx - 2; x <= cx + 2; x++)
                if (x >= 0 && y >= 0 && x < w.aw && y < w.ah && !w.wall[y][x] && !w.diff[y][x]) {
                    w.diff[y][x] = true, w.mapVer = ++g_mapSerial;
                    dynAdd(w, x, y, 2, -1);
                }
        return;
    }
    const int src0 = g_srcIdx;
    g_srcIdx = i;
    for (auto& t : w.f) {
        if (t.hp <= 0 || t.team == me.team) continue;
        if (std::max(fpGap(t.x, fpN(t), cx, 1), fpGap(t.y, fpN(t), cy, 1)) > 2 || 10 * t.alt > 10) continue;
        int dmg = rollDice(2 * me.b.lair, 10);
        const bool ok = saveOK(t, DEX, me.dc()), evade = t.b.evasion && !t.cond[C_STUN] && !t.cond[C_PARAL];
        if (ok || evade) dmg = ok && evade ? 0 : dmg / 2;
        hurt(t, dmg, DT_BLUDGEONING, true);
    }
    g_srcIdx = src0;
}
// Состав боя (v21, команды): главные бойцы и их команды. Два бойца из команд 0 и 1 — прежний бой «один на один» (бит в бит);
// иначе — бой нескольких команд: победа — последняя команда в строю, на пределе раундов — по очкам (доля хитов команды +
// убитые враги), равные очки — ничья (−1). Результат многокомандного боя — номер команды или −1.
struct RosterEntry {
    const Boss* b;
    int team;
};
// расстановка нескольких команд: опорная точка каждой команды — как можно дальше от уже выбранных (из 300 случайных мест
// для расстановки), бойцы команды — вокруг своей опоры, квадрат целиком на местах для расстановки и не на других
void placeTeams(const World& w, const std::vector<RosterEntry>& R, std::vector<int>& px, std::vector<int>& py) {
    const int N = (int)R.size();
    std::vector<std::array<int, 3>> placed;   // x, y, сторона квадрата
    std::vector<std::pair<int, int>> anchor(MAXT, {-1, -1});
    auto nOf = [](const Boss* b) { return b->size <= 2 ? 1 : b->size - 1; };
    auto randStart = [&]() {
        int x, y;
        do {
            x = irand(w.aw);
            y = irand(w.ah);
        } while (!startCellOK(w, x, y));
        return std::make_pair(x, y);
    };
    std::vector<std::pair<int, int>> used;
    for (int i = 0; i < N; i++) {
        const int t = R[i].team;
        if (anchor[t].first >= 0) continue;
        std::pair<int, int> best = randStart();
        int bd = -1;
        for (int k = 0; k < 300 && !used.empty(); k++) {
            const auto c = randStart();
            int d = 1 << 30;
            for (auto& u : used) d = std::min(d, std::max(std::abs(u.first - c.first), std::abs(u.second - c.second)));
            if (d > bd) bd = d, best = c;
        }
        anchor[t] = best;
        used.push_back(best);
    }
    for (int i = 0; i < N; i++) {
        const int n = nOf(R[i].b), ax = anchor[R[i].team].first, ay = anchor[R[i].team].second;
        auto fit = [&](int x, int y) {
            for (int dy = 0; dy < n; dy++)
                for (int dx = 0; dx < n; dx++)
                    if (x + dx >= w.aw || y + dy >= w.ah || x + dx < 0 || y + dy < 0 || !startCellOK(w, x + dx, y + dy)) return false;
            for (auto& p : placed)
                if (fpOverlap(p[0], p[1], p[2], x, y, n)) return false;
            return true;
        };
        bool ok = false;
        for (int r = 0; r < 40 && !ok; r++)
            for (int dy = -r; dy <= r && !ok; dy++)
                for (int dx = -r; dx <= r && !ok; dx++)
                    if (std::max(std::abs(dx), std::abs(dy)) == r && fit(ax + dx, ay + dy)) px[i] = ax + dx, py[i] = ay + dy, ok = true;
        for (int k = 0; k < 4000 && !ok; k++) {
            const auto c = randStart();
            if (fit(c.first, c.second)) px[i] = c.first, py[i] = c.second, ok = true;
        }
        if (!ok) {   // негде — в любое место для расстановки (ход раздвинет)
            const auto c = randStart();
            px[i] = c.first;
            py[i] = c.second;
        }
        placed.push_back({px[i], py[i], n});
    }
}
thread_local double g_teamScore[MAXT];   // очки команд последнего многокомандного боя
int fightRoster(const std::vector<RosterEntry>& R, std::vector<std::string>* log, std::string* mapJsonOut, const Pol* polA, const Pol* polB) {
    const int N = (int)R.size();
    int nT = 0;
    for (auto& e : R) nT = std::max(nT, e.team + 1);
    nT = std::min(nT, MAXT);
    const bool duel = N == 2 && R[0].team == 0 && R[1].team == 1 && !g_human.multi;   // комната — всегда по правилам команд
    const Boss& A = *R[0].b;
    const Boss& B = *R[std::min(1, N - 1)].b;
    AutoNet autoNet(A, B);   // бойцы с ИНТ >= 12 ходят по сети, если она загружена
    for (auto& u : g_use)
        for (auto& x : u) x = 0;
    for (auto& u : g_castAtom)
        for (auto& x : u) x = 0;
    static thread_local World w;
    g_curW = &w;   // ауры союзников (v21): мир текущего боя; после боя — снова nullptr
    struct CurWReset { ~CurWReset() { g_curW = nullptr; } } curWReset;
    w.f.clear();
    w.nLead = N;
    w.nTeams = nT;
    for (int t = 0; t < MAXT; t++) w.kills[t] = 0;
    w.f.reserve(N + (size_t)N * std::max(0, limits().sumFightBoss));   // все приспешники под потолком: ссылки на бойцов не ломаются
    if (g_customMap.on) applyCustomMap(w, g_customMap);   // карта игрока
    else genMap(w);
    if (g_customMap.on) markMain(w);   // карта игрока проверена на связность — основная часть вся
    initObjects(w);   // хиты стен, деревьев, укрытий, дверей
    w.day = g_customMap.on || irand(100) < 60;   // день или ночь (солнечная чувствительность); карта игрока — днём
    {   // сила опасных зон — по боссам боя: около 15% среднего запаса хитов за срабатывание, СЛ растёт с ним
        double avgHp = 0;
        for (auto& e : R) avgHp += e.b->hp / (double)N;
        w.hazN = std::max(1, (int)std::lround(0.15 * avgHp / 5.5));
        w.hazDC = std::clamp(11 + (int)(avgHp / 60), 11, 20);
    }
    w.dyn.clear();
    for (int t = 0; t < MAXT; t++) w.cmd[t] = -1;
    w.chaseOn = false;
    w.chaseSeen = true;
    w.chasePrey = -1;
    if (mapJsonOut) *mapJsonOut = mapToJson(w);
    const Boss* src[2] = {&A, &B};
    std::vector<int> px(N), py(N);
    if (duel) {
    // начальная расстановка: случайные свободные клетки, по возможности не ближе min(30 клеток, половина длинной стороны арены) друг от друга
        const int minStart = std::min(30, std::max(1, std::max(w.aw, w.ah) / 2));
        for (int tries = 0;; tries++) {
            if (g_customMap.on && g_customMap.hasStarts()) {   // начальные клетки заданы картой
                for (int i = 0; i < 2; i++) { px[i] = g_customMap.start[i][0]; py[i] = g_customMap.start[i][1]; }
                break;
            }
            if (tries == 0 && startCellsN(w) == 0) {   // мест нет (участок мира у заданной точки, карта игрока): пустая арена
                for (int y = 0; y < w.ah; y++)
                    for (int x = 0; x < w.aw; x++) w.wall[y][x] = w.pit[y][x] = w.tree[y][x] = false, w.deck[y][x] = w.door[y][x] = 0, w.height[y][x] = 0;
                markMain(w);
                initObjects(w);
            }
            for (int i = 0; i < 2; i++) {   // квадрат бойца (по размеру) целиком на местах для расстановки, не на сопернике
                const int n = src[i]->size <= 2 ? 1 : src[i]->size - 1;
                auto fit = [&](int x, int y) {
                    for (int dy = 0; dy < n; dy++)
                        for (int dx = 0; dx < n; dx++)
                            if (x + dx >= w.aw || y + dy >= w.ah || !startCellOK(w, x + dx, y + dy)) return false;
                    if (i == 1) {
                        const int n0 = src[0]->size <= 2 ? 1 : src[0]->size - 1;
                        if (fpOverlap(px[0], py[0], n0, x, y, n)) return false;
                    }
                    return true;
                };
                bool ok = false;
                for (int k = 0; k < 400 && !ok; k++) {
                    px[i] = irand(w.aw);
                    py[i] = irand(w.ah);
                    ok = fit(px[i], py[i]);
                }
                const int off = irand(w.aw * w.ah);
                for (int k = 0; k < w.aw * w.ah && !ok; k++) {   // случайно не нашлось — первое подходящее с случайного места
                    const int c = (k + off) % (w.aw * w.ah);
                    px[i] = c % w.aw;
                    py[i] = c / w.aw;
                    ok = fit(px[i], py[i]);
                }
                if (!ok)   // квадрат не помещается нигде: ставим в место для расстановки, ход раздвинет (nudge)
                    do {
                        px[i] = irand(w.aw);
                        py[i] = irand(w.ah);
                    } while (!startCellOK(w, px[i], py[i]));
            }
            if (std::max(std::abs(px[0] - px[1]), std::abs(py[0] - py[1])) >= minStart || tries > 50) break;
        }
    } else
        placeTeams(w, R, px, py);
    for (int i = 0; i < N; i++) {
        Fighter f;
        f.b = *R[i].b;
        f.team = R[i].team;
        f.pol = duel ? (i == 0 ? (polA ? polA : &g_pol) : (polB ? polB : &g_pol)) : &g_pol;
        f.rl = duel && (g_rlTeam[i] || autoNet.controls(i));
        f.hp = f.maxHp = f.b.hp;
        f.x = px[i];
        f.y = py[i];
        f.init = (rollD20() + f.mod(DEX)) * 100 + irand(100);
        f.uses.assign(f.b.abil.size(), 0);
        f.usedRound.assign(f.b.abil.size(), -10);
        f.poolLeft = f.b.poolN;
        f.trigLeft.clear();   // «событие → эффект» (v21): применений за бой; +инициатива — сразу
        for (auto& t : f.b.trigs) {
            f.trigLeft.push_back(t.uses);
            if (t.on == T_INIT && t.what == X_INITB) f.init += 100 * t.n;
        }
        f.lrLeft = f.b.legRes;
        f.legLeft = f.b.legAct;
        f.surgeLeft = f.b.surge;
        f.dashLimit = std::max(0, 3 + f.mod(CON));   // рывки в погоне без риска: 3 + мод. ТЕЛ
        f.ai.seed(rng.next());
        for (int L = 1; L < 10; L++) f.slotLeft[L] = f.b.slots[L];
        for (size_t k = 0; k < f.b.abil.size(); k++) f.uses[k] = f.b.abil[k].recharge && !(f.b.abil[k].rechAfter && f.b.abil[k].uses < UNL) ? 1 : f.b.abil[k].uses;
        w.f.push_back(f);
    }
    for (auto& f : w.f) fireTrig(w, f, T_INIT, nullptr);   // «событие → эффект» (v21): в начале боя
    // «действует с начала боя» (пакет атомов 1): заранее наложенные способности на себя срабатывают до первого хода,
    // без действия; применение и ячейка тратятся как обычно. Раунд — уже 0: иначе отметка «применено в раунде» бралась
    // из прошлого боя потока («не два раунда подряд» зависела от того, какой бой поток считал перед этим)
    g_curRound = 0;
    {
        const World* cw0 = g_curW;
        g_curW = &w;
        for (int i = 0; i < (int)w.f.size(); i++)
            for (int k = 0; k < (int)w.f[i].b.abil.size(); k++) {
                const Spec& s = w.f[i].b.abil[k];
                if (!s.preCast || !s.self || s.react || !resReady(w.f[i], specAbility(w.f[i].b, k).cost)) continue;
                doSpec(w, w.f[i], k, i);
            }
        g_curW = cw0;
    }
    // туман войны: в начале боя стороны знают, где стоит противник (бой начался), дальше — только то, что видят
    g_curRound = 0;
    for (int t = 0; t < MAXT; t++) {
        w.known[t].assign(t < nT ? w.f.size() : 0, {-1, -1, -1});
        w.knownRound[t] = 0;
    }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (w.f[i].team != w.f[j].team) revealTo(w, w.f[i].team, j);
    g_evPending.clear();
    g_evLog = log != nullptr;
    if (g_evLog) {   // порядок ходов для показа: [боец, итог броска инициативы]
        std::vector<int> ord(N);
        for (int i = 0; i < N; i++) ord[i] = i;
        std::sort(ord.begin(), ord.end(), [&](int x, int y) { return w.f[x].init > w.f[y].init; });
        std::string o = "{\"k\":\"init\",\"order\":[";
        for (int k = 0; k < N; k++) o += (k ? "," : "") + std::string("[") + std::to_string(ord[k]) + "," + std::to_string(w.f[ord[k]].init / 100) + "]";
        evPush(o + "]}");
    }
    g_fightInfo = FightInfo{};
    g_fightInfo.ctrl[0] = w.f[0].rl;
    g_fightInfo.ctrl[1] = w.f[1].rl;
    g_fightInfo.aw = w.aw;
    g_fightInfo.ah = w.ah;
    g_fightInfo.theme = w.theme;
    // кадр для просмотрщика: состояние после действия участника actor
    auto emitFrame = [&](int round, int actor, int legendary) {
        if (!log) {
            g_evPending.clear();
            return;
        }
        std::string fr = "{\"round\":" + std::to_string(round) + ",\"actor\":" + std::to_string(actor) + ",\"act\":\"" +
                         (actor >= 0 ? g_act.type : std::string("none")) + "\",\"target\":" + std::to_string(actor >= 0 ? g_act.target : -1) +
                         ",\"abil\":" + std::to_string(actor >= 0 ? g_act.abil : -1) + (legendary ? ",\"legendary\":1" : "") +
                         ",\"chase\":" + std::to_string(w.chaseOn ? 1 : 0) + ",\"prey\":" + std::to_string(w.chasePrey) + ",\"ev\":[";
        for (size_t k = 0; k < g_evPending.size(); k++) fr += (k ? "," : "") + g_evPending[k];
        fr += "]";
        if (actor >= 0 && g_act.gx >= 0) fr += ",\"cell\":[" + std::to_string(g_act.gx) + "," + std::to_string(g_act.gy) + "]";
        if (!w.dyn.empty()) {
            fr += ",\"dyn\":[";
            for (size_t k = 0; k < w.dyn.size(); k++) fr += (k ? "," : "") + std::string("[") + std::to_string(w.dyn[k][0]) + "," + std::to_string(w.dyn[k][1]) + "," + std::to_string(w.dyn[k][2]) + "]";
            fr += "]";
        }
        fr += ",\"f\":[";
        g_evPending.clear();
        for (size_t k = 0; k < w.f.size(); k++) {
            if (k) fr += ",";
            const int ht = g_human.ask && g_human.idx >= 0 && g_human.idx < (int)w.f.size() ? w.f[g_human.idx].team : g_fogTeam;
            const int kind = duel ? ((int)k < 2 ? (int)k : 2 + w.f[k].team) : ((int)k < N ? w.f[k].team % 2 : 2 + w.f[k].team % 2);   // команды: цвет — по team
            std::string fj = fighterJson(w.f[k], kind, !g_human.multi && ht >= 0 && w.f[k].team != ht && !isKnown(w, ht, (int)k));
            if (!duel) fj.insert(fj.size() - 1, ",\"lead\":" + std::to_string((int)k < N ? (int)k : w.f[k].summoner) + ",\"ld\":" + ((int)k < N ? "1" : "0"));   // команды: чей это боец
            fr += fj;
        }
        fr += "]}";
        log->push_back(fr);
    };
    g_act = ActInfo{};
    emitFrame(0, -1, 0);
    g_human.emit = emitFrame;   // игрок пишет кадры после каждой своей команды
    int rr = 0;
    for (int r = 0; r < g_maxRounds && !over(w); r++) {
        rr = r + 1;
        g_curRound = rr;
        for (int t = 0; t < nT; t++)   // предохранитель тумана: 5 раундов не знает ни одного врага — слышит, где они
            if (rr - w.knownRound[t] >= 5)
                for (size_t k = 0; k < w.f.size(); k++)
                    if (w.f[k].hp > 0 && w.f[k].team != t) revealTo(w, t, (int)k);
        std::vector<int> order;
        for (size_t i = 0; i < w.f.size(); i++)
            if (w.f[i].hp > 0 && !w.f[i].fresh) order.push_back((int)i);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return w.f[a].init > w.f[b].init; });
        for (int i = 0; i < N && !over(w); i++) lairAction(w, i);   // логово — на инициативе 20, до всех
        for (int i : order) {
            if (over(w)) break;
            const int dist0 = duel && i < 2 ? distFt(w.f[i], w.f[1 - i]) : 0;
            if (duel && i < 2 && w.f[i].comp) {   // осложнение погони, выпавшее предыдущему участнику, достаётся этому
                if (w.chaseOn) applyComplication(w, w.f[i], w.f[i].comp);
                w.f[i].comp = 0;
            }
            w.f[i].inTurn = true;
            updateKnown(w, w.f[i].team);   // что сторона видит к началу хода бойца
            g_srcIdx = i;   // урон в этот ход наносит боец i (для реакции «мне нанесли урон»)
            // ход бойца; остановка времени (пакет атомов 2) — ещё ходы подряд, пока враги стоят
            for (int rep = 0; rep < 12; rep++) {
                if (g_primLog && i == g_primLog->who) {
                    if (g_primLog->replay) {
                        static const std::vector<Prim> none;
                        replayTurn(w, i, g_primLog->next < g_primLog->turns.size() ? g_primLog->turns[g_primLog->next++] : none);
                    } else {
                        g_primLog->turns.emplace_back();
                        g_rec = &g_primLog->turns.back();
                        g_recIdx = i;
                        takeTurn(w, i);
                        g_rec = nullptr;
                        g_recIdx = -1;
                    }
                } else if (g_human.ask && humanSeat(w, i) >= 0)   // игрок — и свои приспешники
                    humanTurn(w, i);
                else if (w.f[i].rl && pn::g_pctx)
                    pn::netTurnP(w, i);
                else
                    takeTurn(w, i);
                if (w.f[i].extraTurns <= 0 || w.f[i].hp <= 0 || over(w)) break;
                w.f[i].extraTurns--;
                emitFrame(r + 1, i, 0);
            }
            w.f[i].extraTurns = 0;
            w.f[i].inTurn = false;
            if (duel && i < 2) {
                chaseAfterTurn(w, i, dist0);
                w.f[i].speedPen = 0;
            }
            resolveReactions(w);
            g_srcIdx = -1;
            resolveConc(w);
            emitFrame(r + 1, i, 0);
            // легендарные действия противников — в конце этого хода
            for (size_t j = 0; j < w.f.size() && !over(w); j++) {
                if (w.f[j].team == w.f[i].team || w.f[j].hp <= 0 || w.f[j].b.legAct <= 0) continue;
                g_srcIdx = (int)j;
                bool lgd = legendaryTurn(w, (int)j);
                resolveReactions(w);
                g_srcIdx = -1;
                resolveConc(w);
                if (lgd) emitFrame(r + 1, (int)j, 1);
                else g_evPending.clear();
            }
        }
        if (duel) chaseEndOfRound(w);
        expireDyn(w);
        if (!g_evPending.empty()) {   // события конца раунда (отрыв от погони)
            g_act = ActInfo{};
            emitFrame(r + 1, -1, 0);
        }
        for (auto& f : w.f) f.fresh = false;
    }
    g_human.emit = nullptr;
    g_rounds += rr;
    g_lastHp[0] = (double)w.f[0].hp / std::max(1, w.f[0].maxHp);
    g_lastHp[1] = (double)w.f[1].hp / std::max(1, w.f[1].maxHp);
    g_fightInfo.rounds = rr;
    auto finish = [&](int res, int reason) {
        g_fightInfo.result = res;
        g_fightInfo.reason = reason;
        return res;
    };
    if (!duel) {   // несколько команд: последняя в строю; иначе — очки
        int last = -1, in = 0;
        for (int t = 0; t < nT; t++)
            if (teamIn(w, t)) in++, last = t;
        for (int t = 0; t < nT; t++) {
            double hp = 0, mx = 0;
            for (auto& f : w.f)
                if (f.team == t) hp += std::max(0, f.hp), mx += std::max(1, f.maxHp);
            g_teamScore[t] = (mx > 0 ? hp / mx : 0) + w.kills[t];
        }
        if (in == 1) return finish(last, 1);
        if (in == 0) return finish(-1, 0);
        g_capped++;
        int best = -1;
        double bs = -1;
        bool tie = false;
        for (int t = 0; t < nT; t++) {
            if (!teamIn(w, t)) continue;
            if (g_teamScore[t] > bs + 1e-9) best = t, bs = g_teamScore[t], tie = false;
            else if (std::fabs(g_teamScore[t] - bs) <= 1e-9) tie = true;
        }
        return finish(tie ? -1 : best, 5);   // 5 — по очкам на пределе раундов
    }
    // сбежавший (вышел за арену или оторвался от преследователя) проигрывает
    if (w.f[0].fled && !w.f[1].fled && w.f[1].hp > 0) return finish(1, w.f[0].fledWhy == 1 ? 2 : (w.f[0].fledWhy == 3 ? 4 : 3));
    if (w.f[1].fled && !w.f[0].fled && w.f[0].hp > 0) return finish(0, w.f[1].fledWhy == 1 ? 2 : (w.f[1].fledWhy == 3 ? 4 : 3));
    bool a = w.f[0].hp > 0, b = w.f[1].hp > 0;
    if (a && b) g_capped++;
    if (a && !b) return finish(0, 1);
    if (b && !a) return finish(1, 1);
    return finish(2, 0);   // оба живы после предохранителя: ничья
}
int fight(const Boss& A, const Boss& B, std::vector<std::string>* log = nullptr, std::string* mapJsonOut = nullptr,
          const Pol* polA = nullptr, const Pol* polB = nullptr) {
    return fightRoster({{&A, 0}, {&B, 1}}, log, mapJsonOut, polA, polB);
}

const char* CTN[14] = {"аберрация", "зверь", "небожитель", "конструкт", "дракон", "элементаль", "фея", "исчадие", "великан", "гуманоид", "монстр", "слизь", "растение", "нежить"};
