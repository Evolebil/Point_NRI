// Ход: ум бойца (Brain), состояние хода, примитивы и их исполнитель (applyPrim), запись и повтор ходов
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Ум бойца. Решения бота спрашивают у Brain, что бойцу доступно; Pol задаёт, как он взвешивает варианты; сеть-боец —
// вход для обучаемого бота. ИНТ: <=3 инстинкт (бьёт ближайшего, без тактик, часто ошибается), 4-7 простая тактика
// (лечится, но без кайтинга, отхода, баффов и фокуса), 8-11 средний (вся прежняя тактика, без случайных чисел), 12-15
// хорошая и 16+ полная (сейчас играют как средний: ступени заняты под обучаемого бота). МДР: дальность, на которой боец
// замечает врагов (МДР>=10 — всю карту; дальше добавляется 60 фт за раунд боя). ХАР главного босса: послушание
// миньонов.
// ---------------------------------------------------------------------------
const int SENSE_INF = 1 << 28;
struct Brain {
    int lvl = 2;
    double noise = 0;
    bool focus = true, selfSimple = true, selfTactics = true, kite = true, disengage = true, dodge = true,
         evalSummon = true;
    int sense = SENSE_INF;
    double obey = 1.0;
};
Brain brainOf(const World& w, const Fighter& me) {
    Brain b;
    int in = me.b.stats[INT];
    b.lvl = in <= 3 ? 0 : (in <= 7 ? 1 : (in <= 11 ? 2 : (in <= 15 ? 3 : 4)));
    if (b.lvl <= 1) {
        b.focus = b.selfTactics = b.kite = b.disengage = b.dodge = b.evalSummon = false;
        b.noise = b.lvl == 0 ? 0.35 : 0.15;
        if (b.lvl == 0) b.selfSimple = false;
    }
    int wis = me.b.stats[WIS];
    if (wis < 10) b.sense = 60 * std::max(1, wis - 3);
    if (me.team >= 0 && me.team < (int)w.f.size()) {
        int cha = w.f[me.team].b.stats[CHA];
        if (cha < 10) b.obey = std::max(0.2, 1.0 - 0.1 * (10 - cha));
    }
    return b;
}
// Состояние хода: оставшееся движение и потраченные действие/бонусное действие
struct TurnState {
    int speed = 0, speed0 = 0;
    bool actionUsed = false, bonusUsed = false;
    bool lastWasAttack = false;   // последнее действие — Атака или способность (для всплеска действий)
    int swaps = 0, orders = 0;    // смен оружия и приказов за ход (их цена растёт со счётчиком)
    bool spent = false;           // за ход применены атака, заклинание или призыв (пустой ход сети запрещён; манёвры — Рывок,
                                  // Уклонение, Отход — и взаимодействия ход непустым не делают, решение владельца)
    int x0 = -1, y0 = -1;         // клетка в начале хода: шаг, после которого боец вернулся туда же, тратой не считается
    int steps = 0;                // выполнено решений за ход
    int atkLeft = 0;              // игрок бьёт атаки действия Атака по одной (между ними можно идти и менять цель): сколько ещё осталось
    int altLeft[8] = {};          // мультиатака из разных оружий: сколько ударов ещё у запасного оружия k в этом действии «Атака»
};
// начало действия «Атака»: у оружия мультиатаки (запасное с флагом multi) — свои удары
inline void initAltLeft(TurnState& ts, const Fighter& me) {
    for (int k = 0; k < 8; k++) ts.altLeft[k] = k < (int)me.b.alt.size() && me.b.alt[k].multi ? me.b.alt[k].att : 0;
}
inline bool attackStarted(const TurnState& ts) {
    if (ts.atkLeft > 0) return true;
    for (int k = 0; k < 8; k++) if (ts.altLeft[k] > 0) return true;
    return false;
}
// отложенное от событий: атака оружием бонусным действием (если оно свободно в этот ход) и добавка скорости
void resolvePending(World& w, int idx, TurnState& ts) {
    Fighter& me = w.f[idx];
    if (me.pendSpeed > 0) {
        ts.speed += me.pendSpeed;
        me.pendSpeed = 0;
    }
    if (!me.pendBonus) return;
    me.pendBonus = false;
    if (ts.bonusUsed || me.b.wN <= 0 || me.disarmed || me.hp <= 0 || over(w)) return;
    int t = me.pendTgt;
    if (t < 0 || t >= (int)w.f.size() || w.f[t].hp <= 0 || distFt(me, w.f[t]) > atkDist(me.b)) t = nearestEnemyInRange(w, me, atkDist(me.b));
    if (t < 0) return;
    ts.bonusUsed = true;
    const ActInfo saved = g_act;
    doWeapon(w, me, t, 1);
    g_act = saved;
}
// Можно ли заплатить цену: ресурсы способности (применения, концентрация, ячейка), затем экономика действия. why —
// причина отказа для игрока и журнала
bool canPay(const Fighter& me, const TurnState& ts, const AbCost& c, const char** why = nullptr) {
    auto no = [&](const char* m) {
        if (why) *why = m;
        return false;
    };
    if (!resReady(me, c, why)) return false;
    if (c.econ == EC_ACTION && ts.actionUsed) return no("Действие уже потрачено.");
    if (c.econ == EC_BONUS && ts.bonusUsed) return no("Бонусное действие уже потрачено.");
    if (c.econ == EC_REACTION && me.reactionUsed) return no("Реакция уже потрачена.");
    if (c.econ == EC_ATTACK && ts.actionUsed && ts.atkLeft <= 0) return no("Атаки действия «Атака» уже потрачены.");
    return true;
}
// Непустой ли ход (запрет пустого хода сети): применены атака, заклинание или призыв, или боец стоит не там, где начал
// ход (шаг туда и обратно не в счёт); Рывок, Уклонение, Отход и взаимодействия сами по себе ход непустым не делают
inline bool turnSpent(const TurnState& ts, const Fighter& me) {
    return ts.spent || (ts.x0 >= 0 && (me.x != ts.x0 || me.y != ts.y0));
}
// Списать экономику действия (ресурсы самой способности списывает paySpec при её применении)
inline void payEcon(TurnState& ts, Fighter& me, int econ) {
    if (econ == EC_ATTACK) {   // вместо одной атаки: начинает действие «Атака» (остальные атаки — оружием) или тратит одну из оставшихся
        if (ts.atkLeft > 0) ts.atkLeft--;
        else {
            ts.actionUsed = true;
            ts.atkLeft = std::max(0, me.b.wAtt - 1);
            initAltLeft(ts, me);
        }
    } else if (econ == EC_ACTION) ts.actionUsed = true;
    else if (econ == EC_BONUS) ts.bonusUsed = true;
    else if (econ == EC_REACTION) spendReaction(me);
}
// ---- Примитивы хода (v21, №4): ход бота, игрока и сети — последовательность таких шагов. Движок следит за бюджетом:
// оставшаяся скорость, действие, бонусное действие, всплеск. Двигаться можно до и после действия.
enum PrimKind {
    PR_END,         // закончить ход
    PR_MOVE,        // идти в клетку (x, y); a — предел движения в фт (−1 — вся оставшаяся скорость)
    PR_ATTACK,      // атака оружием по a; b = 1 — безрассудно; x = 1 — одна атака (игрок, TurnState::atkLeft)
    PR_SPELL,       // способность a: по цели b или по клетке (x, y) (телепорт и «по клетке»)
    PR_DASH,        // Рывок (действие): движение +скорость
    PR_DODGE,       // Уклонение (действие)
    PR_DISENGAGE,   // Отход: a = 0 действием, 1 бонусным (если умеет)
    PR_SUMMON,      // призвать a приспешников (действие)
    PR_SWAP,        // сменить оружие на запасное a (бесплатно)
    PR_DROPCONC,    // прекратить концентрацию (бесплатно)
    PR_SURGE,       // всплеск действий: ещё одно действие после Атаки или способности
    PR_ORDER,       // приказ приспешникам: бить a (бесплатно, главный босс)
    PR_HELP,        // Помощь (действие, v21): враг a рядом со мной — первая атака союзника по нему с преимуществом
    PR_READY,       // Подготовка (действие, v21): враг войдёт в досягаемость — удар оружием реакцией
    PR_ALT,         // высота полёта a (уровни по 10 фт): 10 фт движения за уровень; уход из досягаемости ближника — провоцирует
    PR_N
};
const char* PRN[PR_N] = {"конец хода", "идти", "атака", "способность", "Рывок", "Уклонение", "Отход", "призыв", "смена оружия", "снять концентрацию", "всплеск", "приказ", "Помощь", "Подготовка", "высота"};
struct Prim {
    int kind = PR_END, a = -1, b = -1, x = -1, y = -1;
};
void recordMove(int idx, int x, int y, int budget) {
    if (g_rec && idx == g_recIdx) g_rec->push_back({PR_MOVE, budget, -1, x, y});
}
inline void recordPrim(int idx, Prim p) {
    if (g_rec && idx == g_recIdx) g_rec->push_back(p);
}
// Начало хода (общее для бота и игрока): сбросы, регенерация, яд, урон по времени, ауры, оглушение, захват, бюджет движения.
// false — ход уже закончился (боец пал, оглушён и т.п.)
bool beginTurn(World& w, int idx, TurnState& ts) {
    Fighter& me = w.f[idx];
    if (me.hp <= 0 || over(w)) return false;
    g_act = ActInfo{};
    me.turnX0 = me.x;   // наскок (пакет атомов 2): разбег считается от клетки начала хода
    me.turnY0 = me.y;
    if (me.banished > 0) {   // изгнан: ход пропускается; изгнавший пал — возвращается сразу
        const bool srcGone = me.banishSrc < 0 || me.banishSrc >= (int)w.f.size() || w.f[me.banishSrc].hp <= 0;
        me.banished = srcGone ? 0 : me.banished - 1;
        if (me.banished > 0) return false;
    }
    // сброс в начале своего хода (§10, §12: реакция восстанавливается)
    me.dodging = false;
    me.disengaged = false;
    me.struckFoe = false;
    me.riderUsed = false;
    me.onceUsed = 0;
    me.reactionUsed = false;
    me.wasReckless = false;
    me.wAtkTurn = false;
    me.readied = false;   // Подготовка — до начала моего следующего хода
    me.pendBonus = false;
    me.pendSpeed = 0;
    fallCheck(me);   // не может держаться в воздухе (скорость 0 и т. п.) — падает
    me.alt = std::min(me.alt, maxAltAt(w, me, me.x, me.y, me.up));
    for (auto& g : w.f) {
        if (g.helpBy == idx) g.helpBy = -1;   // и Помощь тоже
        if (g.noOAvs == idx) g.noOAvs = -1;   // и «не провоцирует» (Подвижный)
    }
    if (me.disarmed) {   // обезоружен: подбирает оружие — тратит взаимодействие хода
        me.disarmed = false;
        ts.swaps++;
    }
    // очки легендарных действий восстанавливаются в начале своего хода
    me.legLeft = me.b.legAct;
    // бросок перезарядки в начале своего хода
    rechargeStart(me);
    // концентрация закончилась сама, если эффект уже снят
    if (me.concSpec >= 0) {
        const Spec& cs = me.b.abil[me.concSpec];
        bool act = (cs.buffN && me.buffRounds > 0) || (cs.sbAmt && me.sbRounds > 0) ||
                   (cs.acB && me.acBuffRounds > 0) || (cs.invisN && me.invisRounds > 0);
        for (auto& f : w.f)
            if (cs.sumN > 0 && f.summoner == idx && f.sumSpec == me.concSpec && f.hp > 0) act = true;
        for (auto& f : w.f) {
            for (int k = 1; k < C_N; k++)
                if (f.cond[k] > 0 && f.condConc[k] == idx + 1) act = true;
            if (f.dotRounds > 0 && f.dotConc == idx + 1) act = true;
        }
        if (!act) me.concSpec = -1;
    }
    if (me.b.regenN && !(me.supp > 0 && (me.b.suppTgt & 1)))
        me.hp = std::min(me.maxHp, me.hp + rollDice(me.b.regenN, 6));
    g_turnSerial++;   // опасная зона: новый ход — может задеть снова; начал ход в зоне — урон
    hazardHit(w, me);
    if (me.hp <= 0) return false;
    // тик урона от яда, повторный спас по ТЕЛ уже обрабатывает endOfTurn
    if (me.cond[C_POISON] > 0 && me.hp > 0) {
        // яд: считается типом «яд» (иммунным и резистным он нипочём не наносится)
        hurt(me, rollDie(4), DT_POISON, true);
        if (me.hp <= 0) return false;
    }
    // урон по времени
    if (me.dotRounds > 0 && me.hp > 0) {
        hurt(me, rollDice(me.dotN, me.dotS), me.dotType, true);
        me.dotRounds--;
        if (me.hp <= 0) return false;
    }
    // ауры врагов
    if (applyAuras(w, idx)) return false;
    if (me.cond[C_STUN] || me.cond[C_PARAL]) {
        endOfTurn(me);
        g_act.type = "stunned";
        return false;
    }
    // захват кончается, если схвативший пал или недееспособен
    if (me.cond[C_GRAPPLE] > 0 && me.grapSrc >= 0) {
        const Fighter& gr = w.f[me.grapSrc];
        if (gr.hp <= 0 || gr.cond[C_STUN] || gr.cond[C_PARAL] || gr.cond[C_INCAP]) me.cond[C_GRAPPLE] = 0;
    }
    grabSync(me);
    if (me.cond[C_GRAPPLE] > 0 && me.grabDotN > 0) {   // захват с уроном (пакет атомов 2): каждый ход, пока держат
        const int s0 = g_srcIdx;
        g_srcIdx = me.grabDotSrc;
        hurt(me, rollDice(me.grabDotN, me.grabDotS), me.grabDotType, false);
        g_srcIdx = s0;
        if (me.hp <= 0) return false;
    }
    // схваченный, которому нечем атаковать (нет оружия или враг вне досягаемости), тратит действие на побег (СЛ из способности)
    bool escapeTried = false;
    if (me.cond[C_GRAPPLE] > 0 && !me.cond[C_DISTR] && !me.cond[C_INCAP] &&
        (me.b.wN <= 0 || nearestEnemyInRange(w, me, atkDist(me.b)) < 0)) {
        escapeTried = true;
        const bool sa = me.skill(SK_ATHL) >= me.skill(SK_ACRO);
        if (d20(skillAdv(me, sa ? SK_ATHL : SK_ACRO), false) + std::max(me.skill(SK_ATHL), me.skill(SK_ACRO)) >= me.cdc[C_GRAPPLE]) me.cond[C_GRAPPLE] = 0;   // Атлетика или Акробатика
        grabSync(me);
    }
    int speed = speedOf(me);
    if (me.cond[C_PRONE]) {
        me.cond[C_PRONE] = 0;
        speed /= 2;
    }
    // общий бюджет движения на ход (отход + сближение + бег)
    ts.speed = ts.speed0 = speed;
    ts.x0 = me.x;
    ts.y0 = me.y;
    // недееспособный не использует действия
    ts.actionUsed = me.cond[C_DISTR] > 0 || me.cond[C_INCAP] > 0 || escapeTried;
    ts.bonusUsed = me.cond[C_INCAP] > 0;
    fireTrig(w, me, T_TURN, nullptr);   // «событие → эффект» (v21): в начале моего хода
    resolvePending(w, idx, ts);
    return true;
}
// Страх с бегством (общее правило для бота, игрока и сети): действие — Рывок прочь от ближайшего известного врага
// (движение вдвое), провоцированные атаки действуют; дошёл до края арены — шагнул за него: побег и поражение.
// true — боец сбежал, ход окончен.
bool forcedFlee(World& w, int idx, TurnState& ts) {
    Fighter& me = w.f[idx];
    if (!(me.cond[C_FLEE] > 0 && !me.cond[C_INCAP] && !ts.actionUsed)) return false;
    int focus = -1, fd = 1 << 30;
    for (int pass = 0; pass < 2 && focus < 0; pass++)   // сначала известные, если таких нет — любые
        for (size_t e = 0; e < w.f.size(); e++)
            if (w.f[e].hp > 0 && w.f[e].team != me.team && (pass || isKnown(w, me.team, (int)e)) && distFt(me, w.f[e]) < fd) {
                fd = distFt(me, w.f[e]);
                focus = (int)e;
            }
    if (focus < 0) return false;
    auto* rec0 = g_rec;   // вынужденное бегство — правило, а не решение: в запись хода не идёт
    g_rec = nullptr;
    payEcon(ts, w.f[idx], EC_ACTION);
    g_act.type = "flee";
    const Fighter& F = w.f[focus];
    const int dx = me.x - F.x, dy = me.y - F.y, speed = ts.speed;
    const int sx = dx == 0 ? (irand(2) * 2 - 1) : (dx > 0 ? 1 : -1), sy = dy == 0 ? (irand(2) * 2 - 1) : (dy > 0 ? 1 : -1), steps = 2 * speed / 5 + 1;
    me.dashedNow = true;
    const int bx = sx > 0 ? w.aw - 1 - me.x : me.x, by = sy > 0 ? w.ah - 1 - me.y : me.y;
    if ((std::min(bx, by) + 1) * 5 <= 2 * speed) {
        me.fled = true;
        me.fledWhy = 1;
        evPush("{\"k\":\"fled\",\"who\":" + std::to_string(idx) + ",\"why\":1}");
        endOfTurn(me);
        g_rec = rec0;
        return true;
    }
    Fighter away;   // +1 клетка: moveToward не заходит в клетку самой «цели»
    away.x = std::clamp(me.x + sx * steps, 0, w.aw - 1);
    away.y = std::clamp(me.y + sy * steps, 0, w.ah - 1);
    moveToward(w, me, away, 0, 2 * speed, false);
    ts.speed = 0;
    g_rec = rec0;
    return false;
}
// Исполнитель примитива (v21, №4). check — проверить правила и вернуть причину отказа в msg (игрок, сеть); без проверки —
// повтор записанного хода бота. true — примитив выполнен.
bool applyPrim0(World& w, int idx, TurnState& ts, const Prim& p, bool check, std::string* msg);
// исполнитель с подсчётом шагов хода (ts.steps — признак сети «сколько уже решено за ход»)
bool applyPrim(World& w, int idx, TurnState& ts, const Prim& p, bool check, std::string* msg = nullptr) {
    const bool ok = applyPrim0(w, idx, ts, p, check, msg);
    if (ok) ts.steps++;
    if (ok) {   // «событие → эффект» (v21): после Рывка и Уклонения — события; отложенное (атака бонусом, скорость)
        if (p.kind == PR_DASH) fireTrig(w, w.f[idx], T_DASH, nullptr);
        if (p.kind == PR_DODGE) fireTrig(w, w.f[idx], T_DODGE, nullptr);
        resolvePending(w, idx, ts);
    }
    return ok;
}
bool applyPrim0(World& w, int idx, TurnState& ts, const Prim& p, bool check, std::string* msg) {
    Fighter& me = w.f[idx];
    auto no = [&](const char* why) {
        if (msg) *msg = why;
        return false;
    };
    auto enemyOk = [&](int t) {
        return t >= 0 && t < (int)w.f.size() && w.f[t].hp > 0 && w.f[t].team != me.team && !w.f[t].fled && isKnown(w, me.team, t) &&
               !(me.cond[C_CHARM] > 0 && me.charmSrc == t);
    };
    switch (p.kind) {
        case PR_END:
            return true;
        case PR_MOVE: {
            if (check) {
                if (p.x < 0 || p.y < 0 || p.x >= w.aw || p.y >= w.ah) return no("Туда нельзя: это за пределами арены.");
                if (w.wall[p.y][p.x] || w.pit[p.y][p.x]) return no("Клетка непроходима (стена или ущелье).");
                if (ts.speed < 5) return no("Движения не осталось.");
                // опутан, схвачен или парализован посреди хода (реакция, ловушка): бюджет хода уже выдан, но скорость — 0
                if (speedOf(me) <= 0) return no("Вы не можете двигаться: опутаны, схвачены или парализованы.");
                if (me.cond[C_FRIGHT] > 0) {   // испуганный не приближается добровольно
                    int nowD = 1 << 30, newD = 1 << 30;
                    for (size_t i = 0; i < w.f.size(); i++) {
                        const Fighter& g = w.f[i];
                        if (g.hp > 0 && g.team != me.team && isKnown(w, me.team, (int)i)) {   // только известные (туман войны)
                            nowD = std::min(nowD, 5 * std::max(std::abs(g.x - me.x), std::abs(g.y - me.y)));   // по плану (как у сети):
                            newD = std::min(newD, 5 * std::max(std::abs(g.x - p.x), std::abs(g.y - p.y)));     // ярус клетки ещё неизвестен
                        }
                    }
                    if (newD < nowD) return no("Вы испуганы: приближаться к врагу нельзя.");
                }
                // каждое решение что-то тратит: шаг, который никуда не ведёт (своя клетка, путь сразу упирается во врага
                // или не хватает скорости на первую клетку), не тратит скорость — его нет
                if (p.x == me.x && p.y == me.y) return no("Вы уже стоите в этой клетке.");
                std::vector<PStep> path;
                pathTo(w, me, p.x, p.y, path);
                const int budget = p.a >= 0 ? std::min(ts.speed, p.a) : ts.speed;
                bool knownFoe = false;   // в известного врага не шагнуть; в невидимого — можно упереться (он раскроется, 5 фт)
                for (size_t i = 0; !path.empty() && i < w.f.size(); i++) {
                    const Fighter& g = w.f[i];
                    if (g.team != me.team && touchesAt(w, me, path[0].first, path[0].second, path[0].l, g) && isKnown(w, me.team, (int)i)) knownFoe = true;
                }
                if (path.empty() || knownFoe || stepCostL(w, me, me.x, me.y, me.up, path[0].first, path[0].second, path[0].l) > budget)
                    return no("Туда не сдвинуться: путь закрыт или не хватает скорости на первый шаг.");
            }
            const int budget = p.a >= 0 ? std::min(ts.speed, p.a) : ts.speed;
            const int moved = moveToCell(w, me, p.x, p.y, budget);
            ts.speed -= moved;
            g_act = ActInfo{};
            g_act.type = "move";
            return true;
        }
        case PR_ATTACK: {   // x == 1 — одна атака (игрок): первая тратит действие, остальные атаки этого действия — без платы
            const Ability ab = builtinAbility(EF_WEAPON);
            // y >= 1 — удар запасным оружием мультиатаки y−1 (укус + когти в одном действии «Атака», пакет атомов 1)
            if (p.y >= 1) {
                const int k = p.y - 1;
                if (k >= (int)me.b.alt.size() || k >= 8 || !me.b.alt[k].multi) return no("Это оружие не бьёт в мультиатаке.");
                const bool started = attackStarted(ts);
                if (check) {
                    if (started ? ts.altLeft[k] <= 0 : ts.actionUsed) return no(started ? "Удары этим оружием в этом действии уже потрачены." : "Действие уже потрачено.");
                    if (me.disarmed) return no("Вы обезоружены.");
                    if (!enemyOk(p.a)) return no("Нужна живая известная цель-враг (не очаровавшая вас).");
                    swapWeapon(me.b, (size_t)k);
                    const bool far = distFt(me, w.f[p.a]) > atkDist(me.b);
                    swapWeapon(me.b, (size_t)k);
                    if (far) return no("Цель дальше дальности этого оружия.");
                    if (!losF(w, me, w.f[p.a])) return no("Цель не видна: путь закрыт стеной.");
                }
                if (!started) {   // действие «Атака» начато этим оружием: все удары основного ещё впереди
                    payEcon(ts, me, EC_ACTION);
                    ts.atkLeft = me.b.wAtt;
                    initAltLeft(ts, me);
                }
                const int nNow = std::clamp(p.x, 1, ts.altLeft[k]);   // сколько ударов этим оружием — по этой цели сразу
                ts.altLeft[k] -= nNow;
                g_act = ActInfo{};
                swapWeapon(me.b, (size_t)k);
                doWeapon(w, me, p.a, nNow);
                swapWeapon(me.b, (size_t)k);
                ts.lastWasAttack = true;
                ts.spent = true;
                return true;
            }
            // x >= 1 — игрок: столько атак действия «Атака» по этой цели сразу (остальные — потом, по другим целям; 8 ноября);
            // cont — атаки действия уже начаты (игрок; способность «вместо одной атаки» — и бот)
            const bool one = p.x >= 1, cont = ts.atkLeft > 0;
            const int nNow = !one ? 0 : std::clamp(p.x, 1, cont ? ts.atkLeft : std::max(1, me.b.wAtt));
            if (check) {
                const char* why = nullptr;
                if (!cont && !canPay(me, ts, ab.cost, &why)) return no(why);
                if (me.b.wN <= 0) return no("У вас нет оружия: атаковать нечем.");
                if (!enemyOk(p.a)) return no("Нужна живая известная цель-враг (не очаровавшая вас).");
                if (distFt(me, w.f[p.a]) > atkDist(me.b)) return no("Цель дальше дальности оружия.");
                if (!losF(w, me, w.f[p.a])) return no("Цель не видна: путь закрыт стеной.");
            }
            if (p.b == 1 && hasWhen(me.b, W_RECKLESS) && me.b.wRange <= 5) me.wasReckless = true;
            g_act = ActInfo{};
            doWeapon(w, me, p.a, one ? nNow : (cont ? ts.atkLeft : -1));
            if (cont) ts.atkLeft = one ? ts.atkLeft - nNow : 0;
            else {
                payEcon(ts, me, ab.cost.econ);
                ts.atkLeft = one ? std::max(0, me.b.wAtt - nNow) : 0;
                initAltLeft(ts, me);
            }
            // целое действие (бот): удары оружием мультиатаки уже сделаны в doWeapon(−1); после «вместо одной атаки» — здесь
            if (!one) {
                if (cont && !me.b.alt.empty()) altMultiAttacks(w, me, p.a);
                for (int k = 0; k < 8; k++) ts.altLeft[k] = 0;
            }
            ts.lastWasAttack = true;
            ts.spent = true;
            return true;
        }
        case PR_SPELL: {
            if (p.a < 0 || p.a >= (int)me.b.abil.size()) return no("Нет такой способности.");
            const Spec s = me.b.abil[p.a];   // копия: превращение меняет тело (me.b) посреди примитива
            const Ability ab = specAbility(me.b, p.a);
            if (check) {
                if (s.react) return no("Это реакция: она срабатывает сама.");
                const char* why = nullptr;
                if (!canPay(me, ts, ab.cost, &why)) return no(why);
                if (s.sumN > 0 && summonRoom(w, idx, p.a) <= 0) return no("Технический потолок призыва (limity.json): больше не призвать.");
            }
            g_act = ActInfo{};
            if (s.self && s.teleFt > 0) {   // телепорт в клетку
                if (check) {
                    if (!fpAll(w, me, p.x, p.y, [&](int cx, int cy) { return !w.wall[cy][cx] && !w.pit[cy][cx]; }) || blockedAt(w, me, p.x, p.y))
                        return no("Туда не прыгнуть: клетка закрыта или занята.");
                    if (distFC(w, me, p.x, p.y) > s.teleFt) return no("Дальше дальности прыжка.");
                    if (s.jump && distFC(w, me, p.x, p.y) > ts.speed) return no("Прыжок тратит движение: его не хватает.");
                    if (s.jump && !losFC(w, me, p.x, p.y)) return no("Прыжок: путь закрыт стеной.");
                }
                if (s.jump) ts.speed = std::max(0, ts.speed - distFC(w, me, p.x, p.y));   // прыжок (пакет атомов 2) — за движение
                me.x = p.x;
                me.y = p.y;
                me.up = !nodeOK(w, me, me.x, me.y, 0) && w.deck[me.y][me.x] > 0;
                hazardHit(w, me);
                paySpec(w, me, p.a, false);
                g_act.type = "teleport";
                g_act.abil = p.a;
            } else if (s.self) {
                // «и на союзника»: цель — союзник в пределах дальности и на виду (иначе — на себя)
                if (check && p.b >= 0 && p.b != idx) {
                    if (!s.ally) return no("Эта способность только на себя.");
                    if (p.b >= (int)w.f.size() || w.f[p.b].hp <= 0 || w.f[p.b].team != me.team || w.f[p.b].fled) return no("Нужен живой союзник.");
                    if (distFt(me, w.f[p.b]) > s.range) return no("Союзник дальше дальности способности.");
                    if (!losF(w, me, w.f[p.b])) return no("Союзник не виден: путь закрыт стеной.");
                }
                const bool form = s.formTmpl >= 0;
                doSpec(w, me, p.a, p.b >= 0 ? p.b : idx);
                if (form && me.inForm) {   // скорость облика: потраченное движение остаётся потраченным
                    const int used = ts.speed0 - ts.speed;
                    ts.speed0 = speedOf(me);
                    ts.speed = std::max(0, ts.speed0 - used);
                }
            } else if (s.ground) {
                if (check) {
                    if (p.x < 0 || p.y < 0 || p.x >= w.aw || p.y >= w.ah) return no("Выберите клетку на арене.");
                    if (distFC(w, me, p.x, p.y) > s.range) return no("Клетка дальше дальности способности.");
                    if (!losFC(w, me, p.x, p.y)) return no("Клетка не видна: путь закрыт стеной.");
                }
                doSpec(w, me, p.a, -1, p.x, p.y);
            } else {
                if (check) {
                    // лечит союзников (пакет атомов 2): целью может быть и союзник
                    const bool allyOk = s.healAlly && p.b >= 0 && p.b < (int)w.f.size() && w.f[p.b].hp > 0 && w.f[p.b].team == me.team;
                    if (!allyOk && !enemyOk(p.b)) return no("Нужна живая известная цель-враг (не очаровавшая вас).");
                    if (distFt(me, w.f[p.b]) > specReach(me.b, s)) return no("Цель дальше дальности способности.");
                    if (!losF(w, me, w.f[p.b])) return no("Цель не видна: путь закрыт стеной.");
                }
                doSpec(w, me, p.a, p.b);
            }
            payEcon(ts, me, ab.cost.econ);
            if (ab.cost.econ == EC_ACTION) ts.lastWasAttack = !s.self;
            ts.spent = true;
            return true;
        }
        case PR_DASH: {
            const Ability ab = builtinAbility(EF_DASH);
            const char* why = nullptr;
            if (check && !canPay(me, ts, ab.cost, &why)) return no(why);
            payEcon(ts, me, ab.cost.econ);
            ts.speed += ts.speed0;
            me.dashedNow = true;
            g_act = ActInfo{};
            g_act.type = "dash";
            return true;
        }
        case PR_HELP: {   // Помощь (v21): враг рядом со мной, первая атака союзника по нему — с преимуществом
            const Ability ab = builtinAbility(EF_HELP);
            const char* why = nullptr;
            if (check) {
                if (!canPay(me, ts, ab.cost, &why)) return no(why);
                if (!enemyOk(p.a)) return no("Нужна живая известная цель-враг.");
                if (distFt(me, w.f[p.a]) > 5) return no("Помочь можно только против врага вплотную к вам.");
            }
            payEcon(ts, me, ab.cost.econ);
            w.f[p.a].helpBy = idx;
            g_act = ActInfo{};
            g_act.type = "help";
            g_act.target = p.a;
            return true;
        }
        case PR_ALT: {   // высота полёта (v21)
            const int cost = 10 * std::abs(p.a - me.alt);
            if (check) {
                if (!me.b.fly) return no("Вы не умеете летать.");
                if (!canStayAloft(me) || speedOf(me) <= 0) return no("Сейчас не взлететь: вы сбиты с ног, опутаны, схвачены или не можете двигаться.");
                if (p.a < 0 || p.a > maxAltAt(w, me, me.x, me.y, me.up)) return no(p.a < 0 ? "Ниже земли не опуститься." : "Выше не подняться: потолок или предел высоты.");
                if (p.a == me.alt) return no("Вы уже на этой высоте.");
                if (cost > ts.speed) return no("Не хватает движения: уровень высоты — 10 фт.");
            }
            ts.speed -= cost;
            setAlt(w, me, p.a);
            g_act = ActInfo{};
            g_act.type = "alt";
            return true;
        }
        case PR_READY: {   // Подготовка (v21): до моего следующего хода — удар реакцией по врагу, вошедшему в досягаемость
            const Ability ab = builtinAbility(EF_READY);
            const char* why = nullptr;
            if (check) {
                if (!canPay(me, ts, ab.cost, &why)) return no(why);
                if (p.a >= (int)me.b.abil.size()) return no("Нет такой способности.");
                if (p.a >= 0 && (me.b.abil[p.a].react || !resReady(me, specAbility(me.b, p.a).cost))) return no("Эту способность сейчас не подготовить.");
                if (p.a < 0 && (me.b.wN <= 0 || me.disarmed)) return no("Подготовить удар нечем: нет оружия.");
            }
            payEcon(ts, me, ab.cost.econ);
            me.readied = true;
            me.readyAbil = p.a >= 0 && p.a < (int)me.b.abil.size() ? p.a : -1;   // 8 ноября: что подготовлено
            me.readyTrig = p.b == 2 ? 2 : 1;                                      // и при каком условии
            g_act = ActInfo{};
            g_act.type = "ready";
            return true;
        }
        case PR_DODGE: {
            const Ability ab = builtinAbility(EF_DODGE);
            const char* why = nullptr;
            if (check && !canPay(me, ts, ab.cost, &why)) return no(why);
            payEcon(ts, me, ab.cost.econ);
            me.dodging = true;
            g_act = ActInfo{};
            g_act.type = "dodge";
            return true;
        }
        case PR_DISENGAGE: {
            // a = 1 — Отход бонусным действием (способность есть, только если боец умеет)
            const Ability ab = builtinAbility(EF_DISENGAGE, p.a == 1 ? EC_BONUS : EC_ACTION);
            const char* why = nullptr;
            if (check && p.a == 1 && (!me.b.disengB || !canPay(me, ts, ab.cost))) return no("Отход бонусным действием недоступен.");
            if (check && !canPay(me, ts, ab.cost, &why)) return no(why);
            payEcon(ts, me, ab.cost.econ);
            if (ab.cost.econ == EC_ACTION) {
                g_act = ActInfo{};
                g_act.type = "disengage";
            }
            me.disengaged = true;
            return true;
        }
        case PR_SUMMON:   // призыв — способность: a — её номер (то же, что «способность a» на себя)
            if (p.a < 0 || p.a >= (int)me.b.abil.size() || me.b.abil[p.a].sumN <= 0) return no("Нет такой призывающей способности.");
            return applyPrim0(w, idx, ts, Prim{PR_SPELL, p.a, idx}, check, msg);
        case PR_SWAP:
            // смена оружия: 1-я за ход бесплатно, 2-я — бонусное действие, 3-я — действие, дальше платить нечем
            if (p.a < 0 || p.a >= (int)me.b.alt.size()) return no("Нет такого запасного оружия.");
            if (ts.swaps >= 3) return no("Оружие уже сменено трижды за ход: платить нечем.");
            if (ts.swaps == 1) {
                if (check && ts.bonusUsed) return no("Вторая смена оружия — бонусное действие, а оно потрачено.");
                payEcon(ts, me, EC_BONUS);
            } else if (ts.swaps == 2) {
                if (check && ts.actionUsed) return no("Третья смена оружия — действие, а оно потрачено.");
                payEcon(ts, me, EC_ACTION);
                ts.lastWasAttack = false;
            }
            ts.swaps++;
            swapWeapon(me.b, (size_t)p.a);
            return true;
        case PR_DROPCONC:
            if (me.concSpec < 0) return no("Вы ни на чём не концентрируетесь.");
            breakConc(w, idx);
            g_act = ActInfo{};
            g_act.type = "dropconc";
            return true;
        case PR_SURGE:
            if (check && (me.surgeLeft <= 0 || !ts.actionUsed || !ts.lastWasAttack))
                return no("Всплеск действий недоступен: нужен остаток и уже потраченное действие Атака или способность.");
            me.surgeLeft--;
            g_surge++;
            ts.actionUsed = false;
            return true;
        case PR_ORDER:
            // простой приказ (одна цель всем приспешникам): бесплатно, раз за ход; дорогие приказы — после №18
            if (ts.orders >= 1) return no("Приказ уже отдан в этот ход.");
            // цель приказа — живой известный враг (очаровавший босса тоже: бить его могут приспешники)
            if (check && (idx >= w.nLead || p.a < 0 || p.a >= (int)w.f.size() || w.f[p.a].hp <= 0 || w.f[p.a].team == me.team ||
                          w.f[p.a].fled || !isKnown(w, me.team, p.a)))
                return no("Приказ отдаёт главный босс, цель — живой известный враг.");
            ts.orders++;
            if (idx < w.nLead && p.a >= 0) {
                w.cmd[me.team] = p.a;
                evPush("{\"k\":\"order\",\"who\":" + std::to_string(idx) + ",\"tgt\":" + std::to_string(p.a) + "}");
            }
            return true;
    }
    return no("Неизвестный примитив.");
}
// Запись и повтор ходов бойца who примитивами: проверка «пустой политики» (повтор хода бота даёт тот же бой) и
// обучение сети копированием бота. replay = false — ходы бота пишутся в turns, true — ходы берутся из turns.
struct PrimLog {
    int who = -1;
    bool replay = false;
    std::vector<std::vector<Prim>> turns;
    size_t next = 0;
};
thread_local PrimLog* g_primLog = nullptr;
// Повтор записанного хода: начало хода и бегство — по правилам, дальше записанные примитивы (без проверки)
// g_replayHook — посмотреть решение перед его повтором (обучение сети копированием бота); p == nullptr — конец хода
thread_local void (*g_replayHook)(World&, int, const TurnState&, const Prim*) = nullptr;
void replayTurn(World& w, int idx, const std::vector<Prim>& prims) {
    TurnState ts;
    if (!beginTurn(w, idx, ts)) return;
    if (forcedFlee(w, idx, ts)) return;
    for (auto& p : prims) {
        if (over(w) || w.f[idx].hp <= 0) break;
        if (p.kind == PR_END) break;
        if (g_replayHook) g_replayHook(w, idx, ts, &p);
        applyPrim(w, idx, ts, p, false);
    }
    if (g_replayHook && !over(w) && w.f[idx].hp > 0) g_replayHook(w, idx, ts, nullptr);
    endOfTurn(w.f[idx]);
}
