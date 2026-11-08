// ============================================================================
// Ход игрока: по командам из внешнего источника (g_human.ask). Правила те же, что у бота и сети: общее начало хода (beginTurn),
// бегство в страхе (forcedFlee) и те же примитивы хода (applyPrim, v21 №4). Подключается из boss_arena_v20.cc перед fight().
//
// Протокол: на каждый шаг хода отправляется запрос (JSON) с оставшимся движением, доступными действиями, способностями и целями;
// в ответ приходит одна команда:
//   {"c":"move","x":..,"y":..}                      перейти (стоимость по местности, провоцированные атаки как обычно)
//   {"c":"attack","tgt":i,"reckless":bool}          атака оружием: одна атака; первая тратит действие Атака, остальные его атаки
//                                                   (atkLeft в запросе) — следующими командами, между ними можно идти и менять цель
//   {"c":"hitobj","x":..,"y":..}                    атака оружием по объекту в клетке (стена, дерево, укрытие): одна атака, как "attack"
//   {"c":"door","x":..,"y":..}                      открыть или закрыть дверь рядом (взаимодействие: первое за ход бесплатно, дальше — действием)
//   {"c":"spell","k":i,"tgt":j,"x":..,"y":..}       способность k по цели j (самоспособности без цели; телепорт — клетка x,y)
//   {"c":"dash"} {"c":"dodge"} {"c":"disengage"}    действия (Рывок, Уклонение, Отход; {"c":"disengage","bonus":true} — бонусным, если умеет)
//   {"c":"swap","k":i}                              сменить оружие на запасное i (1-я за ход бесплатно, 2-я бонусным действием, 3-я действием)
//   {"c":"surge"}                                   всплеск действий: ещё одно действие
//   {"c":"dropconc"}                                прекратить свою концентрацию (бесплатно, v21 №24)
// Способности-реакции (v21 №23) срабатывают сами, по тем же правилам, что у бота; командой их не применить.
//   {"c":"end"}                                     конец хода
// ============================================================================
#pragma once
#include <functional>
#include "mini_json.h"

struct HumanCtx {
    std::function<std::string(const std::string&)> ask;   // запрос (JSON) -> команда (JSON)
    std::function<void(int, int, int)> emit;              // записать кадр (раунд, актёр, легендарное); ставит fight()
    int idx = -1;                                         // номер бойца под управлением игрока
    bool minions = true;                                  // игрок сам ходит своими приспешниками (иначе — бот)
    std::vector<int> seats;                               // команды (v21): место игрока у главного бойца i (−1 — бот); пусто — только idx
    bool multi = false;                                   // несколько игроков: туман на кадрах не рисуется (у каждого свой)
};
thread_local HumanCtx g_human;
// чьё место ходит бойцом i (−1 — бот): свой главный боец или его приспешник
inline int humanSeat(const World& w, int i) {
    int k = i;
    if (w.f[i].summoner >= 0 && g_human.minions) k = w.f[i].summoner;
    if (!g_human.seats.empty()) return k < (int)g_human.seats.size() ? g_human.seats[k] : -1;
    return k == g_human.idx ? 0 : -1;
}
static std::string specTitle(const Spec& s);   // replay_export.h
std::string describe(const Spec& s);

// Туман войны на карте (для показа): клетки, которые видит хоть один боец стороны, — те же правила, что canSee + линия
// обзора, только к клетке: слепое зрение в радиусе, тьма — лишь тёмным или истинным зрением на нужную дальность.
// Битовая маска по строкам арены в base64.
static std::string visMask(const World& w, int team) {
    std::vector<unsigned char> bits((w.aw * w.ah + 7) / 8, 0);
    for (auto& v : w.f) {
        if (v.hp <= 0 || v.team != team || v.fled) continue;
        for (int y = 0; y < w.ah; y++)
            for (int x = 0; x < w.aw; x++) {
                const int k = y * w.aw + x;
                if (bits[k >> 3] >> (k & 7) & 1) continue;
                const int d = distFC(w, v, x, y);
                bool ok = v.b.blindsight >= d;
                if (!ok && !v.cond[C_BLIND] && losFC(w, v, x, y))
                    ok = !w.dark[y][x] || v.b.dv >= d || v.b.truesight >= d;
                if (ok) bits[k >> 3] |= (unsigned char)(1 << (k & 7));
            }
    }
    static const char* B = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string s;
    for (size_t i = 0; i < bits.size(); i += 3) {
        const unsigned n = (unsigned)bits[i] << 16 | (i + 1 < bits.size() ? (unsigned)bits[i + 1] << 8 : 0) | (i + 2 < bits.size() ? bits[i + 2] : 0);
        s += B[n >> 18 & 63];
        s += B[n >> 12 & 63];
        s += i + 1 < bits.size() ? B[n >> 6 & 63] : '=';
        s += i + 2 < bits.size() ? B[n & 63] : '=';
    }
    return s;
}

static std::string humanRequest(World& w, int idx, const TurnState& ts, const std::string& msg) {
    const Fighter& me = w.f[idx];
    std::string o = "{\"me\":" + std::to_string(idx) + ",\"round\":" + std::to_string(g_curRound) + ",\"speedLeft\":" + std::to_string(ts.speed) +
                    ",\"speed0\":" + std::to_string(ts.speed0) + ",\"actionFree\":" + (ts.actionUsed ? "false" : "true") +
                    ",\"bonusFree\":" + (ts.bonusUsed ? "false" : "true") + ",\"surgeLeft\":" + std::to_string(me.surgeLeft) + ",\"conc\":" + std::to_string(me.concSpec) + ",\"msg\":" + jq(msg) +
                    ",\"afraid\":" + (me.cond[C_FRIGHT] ? "true" : "false") + ",\"disengB\":" + (me.b.disengB ? "true" : "false") +
                    ",\"alt\":" + std::to_string(me.b.alt.size()) + ",\"swaps\":" + std::to_string(ts.swaps) + ",\"weapon\":{\"armed\":" + (me.b.wN > 0 ? "true" : "false") +
                    ",\"range\":" + std::to_string(atkDist(me.b)) + ",\"attacks\":" + std::to_string(me.b.wAtt) + ",\"atkLeft\":" + std::to_string(ts.atkLeft) +
                    ",\"reckless\":" + (hasWhen(me.b, W_RECKLESS) && me.b.wRange <= 5 ? "true" : "false") + "},\"abils\":[";
    for (size_t i = 0; i < me.b.abil.size(); i++) {
        const Spec& s = me.b.abil[i];
        std::string why;
        const char* no = nullptr;
        if (s.react) why = std::string("реакция: срабатывает сама, ") + REACTN[s.react];
        else if (!canPay(me, ts, specAbility(me.b, (int)i).cost, &no)) why = no;
        o += std::string(i ? "," : "") + "{\"k\":" + std::to_string(i) + ",\"title\":" + jq(specTitle(s)) + ",\"desc\":" + jq(describe(s)) + ",\"avail\":" + (why.empty() ? "true" : "false") + ",\"why\":" + jq(why) +
             ",\"bonus\":" + (s.bonus ? "true" : "false") + ",\"react\":" + std::to_string(s.react) + ",\"self\":" + (s.self ? "true" : "false") + ",\"ally\":" + (s.ally ? "true" : "false") + ",\"ground\":" + (s.ground ? "true" : "false") +
             ",\"wallLen\":" + std::to_string(s.wallLen) + ",\"range\":" + std::to_string(specReach(me.b, s)) + ",\"radius\":" + std::to_string(s.radius) +
             ",\"tele\":" + std::to_string(s.teleFt) + ",\"uses\":" + std::to_string(me.uses[i] >= UNL ? -1 : me.uses[i]) + "}";
    }
    o += "],\"alts\":[";   // запасное оружие: что можно взять вместо нынешнего
    for (size_t k = 0; k < me.b.alt.size(); k++) {
        const Wp& q = me.b.alt[k];
        o += std::string(k ? "," : "") + "{\"k\":" + std::to_string(k) + ",\"multi\":" + (q.multi ? "true" : "false") +
             ",\"left\":" + std::to_string(k < 8 ? ts.altLeft[k] : 0) + ",\"att\":" + std::to_string(q.att) + ",\"range\":" + std::to_string(q.range > 5 ? q.range : q.reach) + ",\"t\":" + jq(std::to_string(q.n) + "к" + std::to_string(q.s) + " " + DTN[q.type] +
             (q.range > 5 ? ", " + std::to_string(q.range) + " фт" : (q.reach > 5 ? ", досягаемость " + std::to_string(q.reach) + " фт" : ", ближнее")) +
             (q.att > 1 ? ", атак: " + std::to_string(q.att) : "")) + "}";
    }
    o += "],\"doors\":[";   // двери рядом (5 фт): x, y, 1 закрыта / 2 открыта
    bool fd = true;
    for (int y = std::max(0, me.y - 1); y <= std::min(w.ah - 1, me.y + fpN(me)); y++)
        for (int x = std::max(0, me.x - 1); x <= std::min(w.aw - 1, me.x + fpN(me)); x++)
            if (w.door[y][x] && !fpOverlap(me.x, me.y, fpN(me), x, y, 1)) {
                o += std::string(fd ? "" : ",") + "[" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(w.door[y][x]) + "]";
                fd = false;
            }
    o += "]";
    if (me.b.fly)   // летун: высота (уровни по 10 фт), потолок над ним, может ли сейчас держаться в воздухе
        o += ",\"fly\":{\"alt\":" + std::to_string(me.alt) + ",\"max\":" + std::to_string(maxAltAt(w, me, me.x, me.y, me.up)) + ",\"can\":" + (canStayAloft(me) ? "1" : "0") + "}";
    o += ",\"vis\":\"" + visMask(w, me.team) + "\",\"visW\":" + std::to_string(w.aw) + ",\"visH\":" + std::to_string(w.ah);   // туман войны на карте
    o += ",\"enemies\":[";
    bool first = true;
    for (size_t e = 0; e < w.f.size(); e++) {
        const Fighter& E = w.f[e];
        if (E.hp <= 0 || E.team == me.team || E.fled || !isKnown(w, me.team, (int)e)) continue;   // туман войны: только известные
        o += std::string(first ? "" : ",") + "{\"i\":" + std::to_string(e) + ",\"dist\":" + std::to_string(distFt(me, E)) +
             ",\"seen\":" + (teamSees(w, me.team, E) ? "true" : "false") +
             ",\"los\":" + (losF(w, me, E) ? "true" : "false") +
             ",\"charm\":" + (me.cond[C_CHARM] > 0 && me.charmSrc == (int)e ? "true" : "false") + "}";
        first = false;
    }
    return o + "]}";
}

static void humanTurn(World& w, int idx) {
    TurnState ts;
    if (!beginTurn(w, idx, ts)) return;   // боец пал, оглушён и т.п.
    Fighter& me = w.f[idx];
    std::string msg;
    // Страх с бегством: действие уже решено правилами (Рывок прочь), как у бота и сети
    if (me.cond[C_FLEE] > 0 && !me.cond[C_INCAP] && !ts.actionUsed) {
        const bool fled = forcedFlee(w, idx, ts);
        if (g_human.emit) g_human.emit(g_curRound, idx, 0);
        if (fled || over(w) || me.hp <= 0) return;
        msg = "Вы в ужасе: действие потрачено на бегство.";
    }
    // Ход — те же примитивы, что у бота и сети (applyPrim с проверкой правил); команда игрока переводится в примитив
    for (int guard = 0; guard < 60; guard++) {
        if (over(w) || me.hp <= 0) return;
        const std::string reply = g_human.ask(humanRequest(w, idx, ts, msg));
        msg.clear();
        bool ok = true;
        const Json cmd = parseJson(reply, &ok);
        const std::string c = ok ? cmd["c"].str() : "end";
        if (c == "end" || c.empty()) break;
        Prim p;
        if (c == "move") {
            p.kind = PR_MOVE;
            p.x = cmd["x"].num(-1);
            p.y = cmd["y"].num(-1);
            bool friendHere = false;
            for (auto& g : w.f)
                if (g.hp > 0 && fpOverlap(g.x, g.y, fpN(g), p.x, p.y, fpN(me)) && &g != &me && g.team == me.team) friendHere = true;
            if (friendHere) { msg = "Клетка занята союзником."; continue; }
        } else if (c == "attack") {
            p.kind = PR_ATTACK;
            p.x = std::max(1, (int)cmd["n"].num(1));   // сколько атак действия по этой цели сразу (по умолчанию одна)
            p.a = cmd["tgt"].num(-1);
            p.b = cmd["reckless"].flag() ? 1 : 0;
            p.y = cmd["wk"].num(-1) + 1;   // запасное оружие мультиатаки (−1 — основное)
        } else if (c == "spell") {
            p.kind = PR_SPELL;
            p.a = cmd["k"].num(-1);
            p.b = cmd["tgt"].num(-1);
            p.x = cmd["x"].num(-1);
            p.y = cmd["y"].num(-1);
        } else if (c == "dash") p.kind = PR_DASH;
        else if (c == "dodge") p.kind = PR_DODGE;
        else if (c == "help") { p.kind = PR_HELP; p.a = cmd["tgt"].num(-1); }   // Помощь против врага вплотную (v21)
        else if (c == "ready") { p.kind = PR_READY; p.a = cmd["k"].num(-1); p.b = cmd["trig"].num(1); }   // Подготовка (v21): что (−1 оружие, k способность) и условие (1/2)
        else if (c == "disengage") { p.kind = PR_DISENGAGE; p.a = cmd["bonus"].flag() ? 1 : 0; }
        else if (c == "surge") p.kind = PR_SURGE;
        else if (c == "dropconc") p.kind = PR_DROPCONC;
        else if (c == "swap") { p.kind = PR_SWAP; p.a = cmd["k"].num(-1); }
        else if (c == "alt") { p.kind = PR_ALT; p.a = me.alt + (cmd["dir"].num(0) > 0 ? 1 : -1); }   // выше / ниже на 10 фт
        else if (c == "door") {   // дверь рядом: открыть или закрыть
            const int x = cmd["x"].num(-1), y = cmd["y"].num(-1);
            if (x < 0 || y < 0 || x >= w.aw || y >= w.ah || !w.door[y][x] || std::max(fpGap(x, 1, me.x, fpN(me)), fpGap(y, 1, me.y, fpN(me))) != 1) { msg = "Рядом нет такой двери."; continue; }
            if (occupied(w, x, y, &me)) { msg = "В проёме кто-то стоит."; continue; }
            if (ts.swaps == 0) ts.swaps++;                       // взаимодействие с предметом — первое за ход бесплатно
            else if (!ts.actionUsed) ts.actionUsed = true;       // дальше — действие «Использовать предмет»
            else { msg = "Взаимодействие уже потрачено, а действия нет."; continue; }
            g_act = ActInfo{};
            g_act.type = w.door[y][x] == 1 ? "door-open" : "door-close";
            g_act.gx = x;
            g_act.gy = y;
            setDoor(w, x, y, w.door[y][x] == 1 ? 2 : 1);
            msg = w.door[y][x] == 2 ? "Дверь открыта." : "Дверь закрыта.";
            if (g_human.emit) g_human.emit(g_curRound, idx, 0);
            continue;
        }
        else if (c == "hitobj") {   // удар по объекту: бросок атаки против КД объекта, урон — хитам объекта
            const int x = cmd["x"].num(-1), y = cmd["y"].num(-1);
            const Ability ab = builtinAbility(EF_WEAPON);
            const bool cont = ts.atkLeft > 0;
            const char* why = nullptr;
            if (me.b.wN <= 0 || me.disarmed) { msg = "Нечем бить: нет оружия."; continue; }
            if (!cont && !canPay(me, ts, ab.cost, &why)) { msg = why ? why : "Нет действия."; continue; }
            if (!isObject(w, x, y) || w.ohp[y][x] <= 0) { msg = "В этой клетке нечего ломать (или объект неразрушим)."; continue; }
            // до объекта — до ближайшей его точки по высоте: дерево и стена стоят от земли клетки до верха (а не только верхушка)
            const int gz = w.height[y][x], top = gz + (w.tree[y][x] ? TREE_UP : (w.wall[y][x] ? WALL_UP : 0)), mz = zF(w, me);
            if (distFP(me.x, me.y, mz, fpN(me), x, y, std::clamp(mz, gz, top), 1) > atkDist(me.b)) { msg = "Объект дальше дальности оружия."; continue; }
            const int r = rollD20(), ac = objAC(w, x, y);
            const bool crit = r == 20, hit = crit || (r != 1 && r + me.wAtk() >= ac);
            g_act = ActInfo{};
            g_act.type = "attack-object";
            g_act.gx = x;
            g_act.gy = y;
            if (cont) ts.atkLeft--;
            else {
                payEcon(ts, me, ab.cost.econ);
                ts.atkLeft = std::max(0, me.b.wAtt - 1);
            }
            ts.spent = ts.lastWasAttack = true;
            me.hidden = false;
            if (!hit) msg = "Промах по объекту (КД " + std::to_string(ac) + ").";
            else {
                const int dmg = std::max(1, rollDice(me.b.wN * (crit ? 2 : 1), wDie(me.b)) + me.wMod());
                const bool broke = hitObject(w, x, y, dmg, me.b.wType);
                msg = broke ? "Объект разрушен: остались обломки." : "Попали: −" + std::to_string(dmg) + ", у объекта осталось " + std::to_string(w.ohp[y][x]) + " хитов.";
            }
            if (g_human.emit) g_human.emit(g_curRound, idx, 0);
            continue;
        }
        else { msg = "Неизвестная команда."; continue; }
        const int ox = me.x, oy = me.y;
        if (!applyPrim(w, idx, ts, p, true, &msg)) continue;
        switch (p.kind) {
            case PR_MOVE:
                if (me.x == ox && me.y == oy) msg = "Не удалось пройти: путь закрыт или слишком далеко.";
                else if ((me.x != p.x || me.y != p.y) && !occupied(w, p.x, p.y, &me)) msg = "Дошли не до конца: не хватило движения или путь закрыт.";
                break;
            case PR_DASH: msg = "Рывок: движения стало вдвое больше."; break;
            case PR_ALT: msg = me.alt > 0 ? "Высота " + std::to_string(10 * me.alt) + " фт." : (me.b.fly && canStayAloft(me) ? "Вы на земле." : "Вы упали."); break;
            case PR_DODGE: msg = "Уклонение: атаки по вам идут с помехой до вашего следующего хода."; break;
            case PR_DISENGAGE: msg = "Отход: движение не провоцирует атак."; break;
            case PR_HELP: msg = "Помощь: первая атака союзника по этому врагу — с преимуществом."; break;
            case PR_READY: msg = "Подготовка: когда враг войдёт в досягаемость, ударите реакцией."; break;
            case PR_SURGE: msg = "Всплеск действий: у вас ещё одно действие."; continue;   // кадр не нужен
            case PR_DROPCONC: msg = "Концентрация прекращена: её эффекты сняты."; break;
            case PR_SWAP: msg = ts.swaps == 1 ? "Оружие сменено." : (ts.swaps == 2 ? "Оружие сменено бонусным действием." : "Оружие сменено действием."); break;
            default: break;
        }
        if (g_human.emit) g_human.emit(g_curRound, idx, 0);
    }
    endOfTurn(me);
    g_act = ActInfo{};
}
