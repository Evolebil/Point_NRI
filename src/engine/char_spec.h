// ============================================================================
// Персонаж игрока: описание в JSON -> Boss, проверка правил, разбор цены, каталог цен для интерфейса.
// Подключается после replay_export.h (нужны bossItems, bossViewJson, mini_json.h).
//
// Описание персонажа (все поля необязательны, кроме budget):
//   {"v":21,"name":"Рыцарь","budget":800,"stats":[СИЛ,ЛОВ,ТЕЛ,ИНТ,МДР,ХАР],"acV":5 (единицы КД сверх 10),"main":0,"hp":0 (0 = всё оставшееся),
//    (описания без "v" или с "v" < 21 — старые: в них acV — прежние КД-очки, +1 КД за 2, и "speedBonus" — надбавка к 30 фт)
//    "weapon":{"n":2,"s":8,"range":5,"att":1,"reach":5,"type":11,"finesse":false,"ammo":false},
//    "abilities":[{"delivery":1,"saveStat":1,"range":30,"radius":15,"dmgN":3,"dmgS":6,"dtype":3,"cond":0,"condRounds":1,"uses":0,"recharge":0,
//                  "bonus":false,"self":false,"conc":false,"slotLvl":0,"pushFt":0,"collN":0,"dotN":0,"dotS":6,"dotRounds":0,"healN":0,"teleFt":0,
//                  "buffN":0,"buffRounds":0,"tempN":0,"acB":0,"acRounds":0,"sbStat":0,"sbAmt":0,"sbRounds":0,"hide":false,"darkR":0,"invisN":0,"terr":false,
//                  "ground":false,"wallLen":0,"wallRounds":0,"more":[{"n":2,"s":8,"type":2}] (доп. урон другого типа),"react":0 (1–4: условие реакции),
//                  "selective":false (область только по врагам; иначе задевает и своих),"dcB":0 (надбавка к СЛ, 0–8),
//                  "noRep":false (состояние без повторного спаса)}],
//    "passives":{"regenN":0,"res":[типы],"imm":[типы],"vuln":[типы],"resNM":false,"immNM":false,"wMagic":false,"legRes":0,"legAct":0,"parry":0,
//                "uncanny":false,"immPush":false,"disengB":false,"redirect":false,"immCond":[состояния],"saveProf":[статы],
//                "rules":[{"when":0,"warg":0,"what":0,"n":0,"s":6,"once":false}] («когда → что», индексы When/What; старые pack, reckless,
//                saveAdv, phN, cdN/cdK, condDefT/condDefS читаются и переводятся в правила),"fly":false,"speed":30,"riposte":false,"surge":0,
//                "slots":[0,..9 штук],"dv":0,"blindsight":0,"ignDiff":false,
//                "aura":{"n":0,"s":6,"type":0,"rad":0,"cond":0,"rounds":1},"rider":{"n":0,"s":6,"type":0,"cond":0,"rounds":1}}}
// ============================================================================
#pragma once
#include "mini_json.h"

namespace chr {
// Только технические пределы (v21, №8): расстояния — сторона карты, раунды — предохранитель боя (500), счётчики (кубики,
// атаки, заряды, ячейки) — защита от зависания; способностей не больше, чем различает сеть-боец (MAXA).
static const int MAX_DICE = 1000, MAX_COUNT = 1000, MAX_ABIL = MAXA, MAX_FT = 500, MAX_ROUNDS = 500;

static unsigned maskOf(const Json& a, int n) {
    unsigned m = 0;
    for (size_t i = 0; i < a.size(); i++) {
        const int k = a[i].num(-1);
        if (k >= 0 && k < n) m |= 1u << k;
    }
    return m;
}
static std::string diceList() {
    std::string o = "допустимо";
    for (int k = 0; k < DICE_N; k++) o += std::string(k ? ", к" : " к") + std::to_string(DICE[k]);
    return o;
}
static bool inRange(int v, int lo, int hi, const char* what, std::string& err) {
    if (v >= lo && v <= hi) return true;
    err = std::string(what) + ": допустимо " + std::to_string(lo) + "–" + std::to_string(hi) + ", получено " + std::to_string(v);
    return false;
}

static bool specFromJson(const Json& j, Spec& s, std::string& err) {
    s.delivery = j["delivery"].num(0);
    s.saveStat = j["saveStat"].num(DEX);
    s.range = j["range"].num(5);
    s.radius = j["radius"].num(0);
    const int u = j["uses"].num(0);
    s.uses = u <= 0 ? UNL : u;
    s.bonus = j["bonus"].flag();
    s.self = j["self"].flag();
    s.dmgN = j["dmgN"].num(0);
    s.dmgS = j["dmgS"].num(6);
    s.dtype = j["dtype"].num(DT_FIRE);
    s.cond = j["cond"].num(0);
    s.condRounds = j["condRounds"].num(1);
    s.dotN = j["dotN"].num(0);
    s.dotS = j["dotS"].num(6);
    s.dotRounds = j["dotRounds"].num(0);
    s.darkR = j["darkR"].num(0);
    s.invisN = j["invisN"].num(0);
    s.slotLvl = j["slotLvl"].num(0);
    s.terr = j["terr"].flag();
    s.conc = j["conc"].flag();
    s.inAttack = j["inAttack"].flag();   // пакет атомов 1: вместо одной атаки; невосприимчивость после успеха; не два раунда подряд; с начала боя
    s.immSave = j["immSave"].flag();
    s.noRepeat = j["noRepeat"].flag();
    s.preCast = j["preCast"].flag();
    // пакет атомов 2–3
    s.grabRestr = j["grabRestr"].flag(); s.grabDot = j["grabDot"].flag(); s.jump = j["jump"].flag(); s.healAlly = j["healAlly"].flag();
    s.rechAfter = j["rechAfter"].flag();
    s.images = j["images"].num(0); s.globeLvl = j["globeLvl"].num(0); s.globeRounds = j["globeRounds"].num(0); s.banishN = j["banishN"].num(0);
    s.flyRounds = j["flyRounds"].num(0); s.extraTurns = j["extraTurns"].num(0); s.growRounds = j["growRounds"].num(0); s.lightR = j["lightR"].num(0);
    if (!inRange(s.images, 0, MAX_COUNT, "копии-отражения", err) || !inRange(s.globeLvl, 0, 9, "уровень сферы", err) ||
        !inRange(s.globeRounds, 0, MAX_ROUNDS, "раундов сферы", err) || !inRange(s.banishN, 0, MAX_ROUNDS, "раундов изгнания", err) ||
        !inRange(s.flyRounds, 0, MAX_ROUNDS, "раундов полёта", err) || !inRange(s.extraTurns, 0, 10, "ходов подряд", err) ||
        !inRange(s.growRounds, 0, MAX_ROUNDS, "раундов увеличения", err) || !inRange(s.lightR, 0, MAX_FT, "радиус света", err)) return false;
    s.pushFt = j["pushFt"].num(0);
    s.healN = j["healN"].num(0);
    s.teleFt = j["teleFt"].num(0);
    s.buffN = j["buffN"].num(0);
    s.buffRounds = j["buffRounds"].num(0);
    s.tempN = j["tempN"].num(0);
    s.acB = j["acB"].num(0);
    s.acRounds = j["acRounds"].num(0);
    s.hide = j["hide"].flag();
    s.sbStat = j["sbStat"].num(0);
    s.sbAmt = j["sbAmt"].num(0);
    s.sbRounds = j["sbRounds"].num(0);
    s.collN = j["collN"].num(0);
    s.recharge = j["recharge"].num(0);
    s.ground = j["ground"].flag();
    s.wallLen = j["wallLen"].num(0);
    s.wallRounds = j["wallRounds"].num(0);
    // v21: доп. урон другого типа (№10) — [{"n":2,"s":8,"type":2}, ...]; реакция (№23) — 0 нет, 1–4 условие
    for (const Json& m : j["more"].a) {
        Spec::Dmg d;
        d.n = m["n"].num(0);
        d.s = m["s"].num(6);
        d.type = m["type"].num(DT_FIRE);
        if (d.n > 0) s.more.push_back(d);
    }
    s.react = j["react"].num(0);
    s.selective = j["selective"].flag();   // v21, №17: область бьёт только врагов
    s.dcB = j["dcB"].num(0);               // этап 5б: надбавка к СЛ способности
    s.noRep = j["noRep"].flag();           // этап 5б: состояние без повторного спаса
    // оружие и Зуро (v21): «onHit» — приём при попадании оружием, «offhand» — атака второй рукой (с «twfMod» — с модификатором),
    // «pool» — кость из запаса, «disarm», урон себе (selfN/selfS), лечение себе, если цель умерла (killN/killS/killB),
    // грань и прибавка лечения (healS/healB), грань баффа оружия (buffS)
    s.name = j["name"].str("").substr(0, 80);   // имя способности от игрока
    s.shape = j["shape"].num(0);            // формы (v21): 0 сфера, 1 куб, 2 конус, 3 линия; targets — целей на выбор
    s.targets = j["targets"].num(1);
    if (!inRange(s.shape, 0, SH_N - 1, "форма области", err) || !inRange(s.targets, 1, 10, "целей на выбор", err)) return false;
    s.onHit = j["onHit"].flag();
    s.offhand = j["offhand"].flag();
    s.twfMod = j["twfMod"].flag();
    s.pool = j["pool"].flag();
    s.disarm = j["disarm"].flag();
    s.selfN = j["selfN"].num(0);
    s.selfS = j["selfS"].num(4);
    s.killN = j["killN"].num(0);
    s.killS = j["killS"].num(6);
    s.killB = j["killB"].num(0);
    s.healS = j["healS"].num(6);
    s.healB = j["healB"].num(0);
    s.buffS = j["buffS"].num(4);
    if (!inRange(s.selfN, 0, MAX_DICE, "урон себе", err) || !inRange(s.killN, 0, MAX_DICE, "лечение при убийстве", err) || !inRange(s.killB, 0, 1000, "прибавка лечения при убийстве", err) ||
        !inRange(s.healB, 0, 1000, "прибавка лечения", err))
        return false;
    if (!isDie(s.selfS) || !isDie(s.killS) || !isDie(s.healS) || !isDie(s.buffS)) { err = "грань кубика: " + diceList(); return false; }
    if (!inRange(s.delivery, 0, 2, "тип попадания", err) || !inRange(s.saveStat, 0, STAT_N - 1, "характеристика спасброска", err) ||
        !inRange(s.range, 5, MAX_FT, "дальность способности", err) || !inRange(s.radius, 0, MAX_FT, "радиус области", err) || !inRange(s.dmgN, 0, MAX_DICE, "кубики урона", err) ||
        !inRange(s.dtype, 0, DT_N - 1, "тип урона", err) || !inRange(s.dcB, 0, 8, "надбавка к СЛ", err) || !inRange(s.cond, 0, C_N - 1, "состояние", err) ||
        !inRange(s.condRounds, 1, MAX_ROUNDS, "раундов состояния", err) || !inRange(s.slotLvl, 0, 9, "уровень ячейки", err) || !inRange(s.pushFt, 0, MAX_FT, "толчок", err) ||
        !inRange(s.healN, 0, MAX_DICE, "лечение", err) || !inRange(s.teleFt, 0, MAX_FT, "телепорт", err) || !inRange(s.tempN, 0, MAX_DICE, "временные хиты", err) ||
        !inRange(s.recharge, 0, 6, "перезарядка", err) || !inRange(s.dotN, 0, MAX_DICE, "урон по времени", err) || !inRange(s.dotRounds, 0, MAX_ROUNDS, "раундов урона по времени", err) ||
        !inRange(s.buffN, 0, MAX_DICE, "бафф оружия", err) || !inRange(s.buffRounds, 0, MAX_ROUNDS, "раундов баффа оружия", err) ||
        !inRange(s.acB, 0, MAX_COUNT, "бонус КД", err) || !inRange(s.acRounds, 0, MAX_ROUNDS, "раундов бонуса КД", err) ||
        !inRange(s.sbAmt, 0, MAX_COUNT, "бафф характеристики", err) || !inRange(s.sbRounds, 0, MAX_ROUNDS, "раундов баффа характеристики", err) || !inRange(s.darkR, 0, MAX_FT, "тьма", err) ||
        !inRange(s.invisN, 0, MAX_ROUNDS, "невидимость", err) || !inRange(s.collN, 0, MAX_DICE, "столкновение", err) || !inRange(s.wallLen, 0, W, "длина барьера", err) ||
        !inRange(s.wallRounds, 0, MAX_ROUNDS, "раундов барьера", err))
        return false;
    if (!isDie(s.dmgS) || !isDie(s.dotS)) { err = "грань кубика: " + diceList(); return false; }
    if (!inRange(s.react, 0, 5, "условие реакции", err)) return false;
    // союзники (v21): «ally» — на себя или союзника в пределах дальности; «cmdAtk» — приказ союзнику атаковать; реакция 5
    // «когда атакуют союзника рядом» — «protect» (помеха) и/или «strike» (удар оружием по атакующему)
    s.pullFt = j["pullFt"].num(0);          // прочее (v21): притягивание, снижение максимума хитов, вампиризм, рассеивание, контрзаклинание
    s.maxHpDmg = j["maxHpDmg"].flag();
    s.drain = j["drain"].flag();
    s.dispel = j["dispel"].flag();
    s.counter = j["counter"].flag();
    if (!inRange(s.pullFt, 0, MAX_FT, "притягивание", err)) return false;
    if (s.counter) {   // контрзаклинание — реакция «враг применил способность в зоне», без своих эффектов
        const std::string nm = s.name;
        const int rr = s.range, uses = s.uses, rech = s.recharge, slot = s.slotLvl;
        s = Spec{};
        s.name = nm; s.self = s.counter = true; s.react = 4; s.range = std::max(5, rr); s.uses = uses; s.recharge = rech; s.slotLvl = slot;
        return true;
    }
    s.ally = j["ally"].flag();
    s.cmdAtk = j["cmdAtk"].flag();
    s.protect = j["protect"].flag();
    s.strike = j["strike"].flag();
    if (s.react == 5 || s.cmdAtk) {   // эти способности без своих эффектов: только защита, удар или приказ
        if (s.react == 5 && !s.protect && !s.strike) { err = "реакция за союзника: выберите помеху атаке или удар по атакующему"; return false; }
        const std::string nm = s.name;
        const int rr = s.range, uses = s.uses, rech = s.recharge, react = s.react, slot = s.slotLvl;
        const bool prot = s.protect, str = s.strike, cmd = s.cmdAtk, bonus = s.bonus;
        s = Spec{};
        s.name = nm; s.self = true; s.range = std::max(5, rr); s.uses = uses; s.recharge = rech; s.slotLvl = slot;
        s.react = react; s.protect = prot; s.strike = str; s.cmdAtk = cmd; s.ally = cmd; s.bonus = cmd && bonus;
        if (cmd && react) { err = "приказ союзнику — действием или бонусным действием, не реакцией"; return false; }
        return true;
    }
    if (s.react && s.bonus) { err = "реакция не бывает бонусным действием"; return false; }
    if (!s.more.empty() && s.dmgN <= 0) { err = "доп. урон другого типа добавляется к основному урону способности"; return false; }
    for (size_t k = 0; k < s.more.size(); k++) {
        const Spec::Dmg& d = s.more[k];
        if (!inRange(d.n, 1, MAX_DICE, "кубики доп. урона", err) || !inRange(d.type, 0, DT_N - 1, "тип доп. урона", err)) return false;
        if (!isDie(d.s)) { err = "грань кубика доп. урона: " + diceList(); return false; }
        bool dup = d.type == s.dtype;
        for (size_t q = 0; q < k; q++) dup = dup || s.more[q].type == d.type;
        if (dup) { err = std::string("доп. урон: тип «") + DTN[d.type] + "» уже есть в способности, нужен другой"; return false; }
    }
    if (s.wallLen > 0 && s.wallRounds <= 0) { err = "барьер: укажите число раундов"; return false; }
    if (s.wallLen > 0) s.ground = true;   // барьер всегда ставится на клетку
    if (s.ground && s.self) { err = "способность «по клетке» не может быть «на себя»"; return false; }
    if (s.recharge && s.recharge < 4) { err = "перезарядка: порог 4, 5 или 6"; return false; }
    if (s.recharge && s.uses < UNL && !s.rechAfter) { err = "способность не может одновременно иметь перезарядку и ограниченное число применений"; return false; }
    if (s.dotN && s.dotRounds <= 0) { err = "урон по времени: укажите число раундов"; return false; }
    if (s.buffN && s.buffRounds <= 0) { err = "бафф оружия: укажите число раундов"; return false; }
    if (s.sbAmt && s.sbRounds <= 0) { err = "бафф характеристики: укажите число раундов"; return false; }
    if (s.acB && s.acRounds <= 0) { err = "бонус КД: укажите число раундов"; return false; }
    if (s.offhand) {   // вторая рука: кубики и тип берутся у оружия (buildCharacter); всё остальное не нужно
        const std::string nm = s.name;
        s = Spec{};
        s.name = nm;
        s.offhand = s.bonus = true;
        s.twfMod = j["twfMod"].flag();
        s.range = 5;
        return true;
    }
    if (s.onHit) {   // приём бьёт своим оружием вблизи или издали — дальность и область не нужны
        s.self = false;
        s.ground = false;
        s.radius = 0;
        s.range = 5;
        if (s.delivery == 2) s.delivery = 0;
        const bool any = s.dmgN || s.cond || s.pushFt || s.dotN || s.pool || s.disarm || s.selfN || s.killN || s.killB;
        if (!any) { err = "приём при попадании без эффекта"; return false; }
        if (s.dmgN == 0) s.more.clear();
        return true;
    }
    if (j["summon"].t == Json::OBJ || j["form"].t == Json::OBJ) {   // призыв, превращение (v21): шаблоны собираются в buildCharacter
        s.self = true;
        s.delivery = 2;
        s.range = 0;
        s.radius = 0;
        s.dmgN = 0;
        s.cond = 0;
        s.more.clear();
        return true;
    }
    const bool selfEffect = s.healN || s.tempN || s.acB || s.sbAmt || s.buffN || s.darkR || s.invisN || s.hide || s.teleFt ||
                            s.images || s.globeLvl || s.flyRounds || s.extraTurns || s.growRounds || s.lightR;   // + пакет атомов 2
    const bool hostile = s.dmgN || s.cond || s.pushFt || s.dotN || s.pullFt || s.dispel || s.banishN > 0;   // доп. урон (more) бывает только вместе с основным
    if (s.self && !selfEffect) { err = "способность «на себя» без эффекта"; return false; }
    const bool zone = s.ground && (s.wallLen > 0 || (s.terr && s.radius > 0));
    if (!s.self && !hostile && !zone) { err = "способность по цели без урона, состояния, толчка, урона по времени, барьера или местности"; return false; }
    if (s.ground && hostile && s.radius < 5) { err = "способность по клетке с уроном или состоянием требует радиус области от 5 фт"; return false; }
    if (zone && !hostile) { s.delivery = 0; s.dmgN = 0; s.more.clear(); }
    if (!s.self && (s.healN || s.tempN || s.acB || s.sbAmt || s.buffN || s.darkR || s.invisN || s.hide || s.teleFt)) { err = "эффекты на себя нельзя смешивать со способностью по цели (сделайте две способности)"; return false; }
    if (s.ally && (!s.self || s.teleFt || s.hide || s.darkR)) { err = "на союзника — только лечение, временные хиты, баффы и невидимость"; return false; }
    if (s.self) { if (!s.ally) s.range = 5; s.radius = 0; s.delivery = 0; s.dmgN = 0; s.cond = 0; s.pushFt = 0; s.dotN = 0; s.more.clear(); }
    if (s.pushFt == 0) s.collN = 0;
    if (s.radius == 0) s.shape = SH_SPHERE;
    if (s.radius > 0 || s.self) s.targets = 1;   // несколько целей — у способности без области
    if (s.radius > 0 && (s.shape == SH_CONE || s.shape == SH_LINE)) s.range = s.radius;   // конус и линия — от себя на свою длину
    if (s.dmgN == 0 && s.delivery == 0 && !s.self && s.cond == 0 && s.pushFt == 0 && s.dotN == 0 && !zone) { err = "атака без эффекта"; return false; }
    return true;
}

// Описание -> босс (без цены). budget возвращается отдельно; hpWanted 0 = всё оставшееся (считается позже)
static bool bossFromJson(const Json& j, Boss& b, long long& budget, int& hpWanted, std::string& err) {
    budget = (long long)j["budget"].dbl(800);
    if (budget < MIN_BUDGET || budget > MAX_BUDGET) { err = "бюджет: допустимо " + std::to_string(MIN_BUDGET) + "–" + std::to_string(MAX_BUDGET); return false; }
    for (int i = 0; i < STAT_N; i++) {
        b.stats[i] = j["stats"][i].t == Json::NUM ? j["stats"][i].num() : 10;
        if (!inRange(b.stats[i], 1, STAT_MAX, SN[i], err)) return false;
    }
    const bool oldFmt = j["v"].num(0) < 21;
    b.acV = j["acV"].num(0);   // единицы КД сверх 10 (v21: 1 единица = +1 КД)
    if (oldFmt) b.acV /= 2;    // старое описание: тот же КД
    if (!inRange(b.acV, 0, AC_MAX - 10, "единицы КД", err)) return false;
    const Json& w = j["weapon"];
    b.wN = w["n"].num(0);
    b.wS = w["s"].num(6);
    b.wRange = w["range"].num(5);
    b.wAtt = std::max(1, w["att"].num(1));
    b.reach = w["reach"].num(5);
    b.wType = w["type"].num(DT_SLASHING);
    b.wFinesse = w["finesse"].flag();
    b.wSilver = w["silver"].flag();   // пакет атомов 2: посеребрённое
    b.wAmmo = w["ammo"].flag();
    b.wLight = w["light"].flag();          // оружие и Зуро (v21): лёгкое, дальняя дистанция, запас костей
    b.wLong = w["long"].num(0);
    b.poolN = w["poolN"].num(0);
    b.poolS = w["poolS"].num(8);
    b.wHeavy = w["heavy"].flag();          // размеры (v21): тяжёлое оружие; размер существа 0–5
    b.size = j["size"].num(2);
    b.ctype = j["ctype"].num(9);   // тип существа (v21)
    if (!inRange(b.ctype, 0, 13, "тип существа", err)) return false;
    if (!inRange(b.size, 0, 5, "размер", err)) return false;
    b.wTwoH = w["twoHanded"].flag();       // доспехи и щит (v21): двуручное, универсальное (грань двумя руками)
    b.wVers = w["versatile"].num(0);
    if (b.wVers && !isDie(b.wVers)) { err = "грань универсального оружия: " + diceList(); return false; }
    {   // запасное оружие (после теста 4 ноября): до 8 видов, смена в бою — 1-я за ход бесплатно, 2-я бонусным, 3-я действием
        const Json& ws = j["altWeapons"];
        if (ws.size() > 8) { err = "запасного оружия — не больше 8"; return false; }
        for (size_t k = 0; k < ws.size(); k++) {
            const Json& q = ws[k];
            Wp a;
            a.n = q["n"].num(1); a.s = q["s"].num(6); a.range = q["range"].num(5); a.att = std::max(1, q["att"].num(1));
            a.reach = q["reach"].num(5); a.type = q["type"].num(DT_SLASHING); a.finesse = q["finesse"].flag(); a.ammo = q["ammo"].flag();
            a.multi = q["multi"].flag();   // бьёт в том же действии «Атака» (мультиатака из разных оружий)
            if (!inRange(a.n, 1, MAX_DICE, "кубики запасного оружия", err) || !isDie(a.s) || !inRange(a.range, 5, MAX_FT, "дальность запасного оружия", err) ||
                !inRange(a.att, 1, MAX_COUNT, "атак запасного оружия", err) || !inRange(a.reach, 5, MAX_FT, "досягаемость запасного оружия", err) ||
                !inRange(a.type, 0, DT_N - 1, "тип урона запасного оружия", err)) { if (err.empty()) err = "грань запасного оружия: " + diceList(); return false; }
            if (a.range > 5) a.reach = 5;
            b.alt.push_back(a);
        }
    }
    const Json& ar = j["armor"];           // доспех: вид 0–3, щит, помеха к Скрытности, владение (по умолчанию владеет)
    b.armor = ar["kind"].num(0);
    b.shield = ar["shield"].flag();
    b.armStealth = ar["stealthDis"].flag() || b.armor == AR_HEAVY;
    b.noArmProf = ar.has("prof") && !ar["prof"].flag();
    if (!inRange(b.armor, 0, AR_N - 1, "вид доспеха", err)) return false;
    if (!inRange(b.poolN, 0, MAX_COUNT, "кости запаса", err) || !isDie(b.poolS)) { if (err.empty()) err = "грань кости запаса: " + diceList(); return false; }
    if (b.wN > 0) {
        if (!inRange(b.wN, 1, MAX_DICE, "кубики оружия", err) || !inRange(b.wRange, 5, MAX_FT, "дальность оружия", err) ||
            !inRange(b.wAtt, 1, MAX_COUNT, "атак за действие", err) || !inRange(b.reach, 5, MAX_FT, "досягаемость", err) || !inRange(b.wType, 0, DT_N - 1, "тип урона оружия", err))
            return false;
        if (!isDie(b.wS)) { err = "грань кубика оружия: " + diceList(); return false; }
        if (b.wRange % 5 || b.reach % 5) { err = "дальность и досягаемость кратны 5 фт"; return false; }
        if (b.wRange > 5) b.reach = 5;
        if (b.wRange <= 5 || b.wLong <= b.wRange) b.wLong = 0;
        if (b.wLong % 5 || b.wLong > MAX_FT) { err = "дальняя дистанция кратна 5 фт и не больше " + std::to_string(MAX_FT); return false; }
        if (b.wRange > 5) b.wLight = b.wTwoH = false, b.wVers = 0;   // лёгкое, двуручное, универсальное — ближнее
        if (b.wVers <= b.wS) b.wVers = 0;
        if (b.wTwoH && (b.wLight || b.wVers)) { err = "двуручное оружие не бывает лёгким или универсальным"; return false; }
        if (b.wTwoH && b.shield) { err = "щит и двуручное оружие вместе не держат: обе руки заняты"; return false; }
    } else {
        b.wN = 0; b.wS = 0; b.wRange = 5; b.wAtt = 1; b.reach = 5;
    }
    b.mainStat = j.has("main") ? j["main"].num(STR) : (b.wFinesse || b.wAmmo ? DEX : STR);
    if (!inRange(b.mainStat, 0, STAT_N - 1, "главная характеристика", err)) return false;
    for (const Json& a : j["abilities"].a) {
        if ((int)b.abil.size() >= MAX_ABIL) { err = "способностей не больше " + std::to_string(MAX_ABIL); return false; }
        Spec s;
        if (!specFromJson(a, s, err)) { err = "способность " + std::to_string(b.abil.size() + 1) + ": " + err; return false; }
        const std::string at = "способность " + std::to_string(b.abil.size() + 1) + ": ";
        if ((s.onHit || s.offhand) && b.wN <= 0) { err = at + "приёмы и вторая рука — только с оружием"; return false; }
        if (s.offhand) {
            if (!b.wLight) { err = at + "атака второй рукой — только лёгким ближним оружием"; return false; }
            s.dmgN = b.wN;   // вторая рука бьёт тем же оружием
            s.dmgS = b.wS;
            s.dtype = b.wType;
        }
        if (s.pool) {
            if (b.poolN <= 0) { err = at + "приём из запаса костей, а запаса нет (оружие → запас костей)"; return false; }
            s.poolN = b.poolN;
            s.poolS = b.poolS;
        }
        b.abil.push_back(s);
    }
    const Json& p = j["passives"];
    b.regenN = p["regenN"].num(0);
    b.resMask = maskOf(p["res"], DT_N);
    b.immMask = maskOf(p["imm"], DT_N);
    b.vulnMask = maskOf(p["vuln"], DT_N);
    b.resNM = p["resNM"].flag();
    b.immNM = p["immNM"].flag();
    b.wMagic = p["wMagic"].flag();
    b.legRes = p["legRes"].num(0);
    b.legAct = p["legAct"].num(0);
    b.parry = p["parry"].num(0);
    b.uncanny = p["uncanny"].flag();
    b.magicRes = p["magicRes"].flag();   // пакет атомов 1
    b.spellRes = p["spellRes"].flag();
    b.critImm = p["critImm"].flag();
    b.lightR = p["lightR"].num(0);   // пакеты атомов 2–3: свет вокруг себя, защита без доспеха
    b.unarmoredStat = p.has("unarmoredStat") ? p["unarmoredStat"].num(-1) : -1;
    if (!inRange(b.lightR, 0, MAX_FT, "радиус света", err) || !inRange(b.unarmoredStat, -1, STAT_N - 1, "характеристика защиты без доспеха", err)) return false;
    b.immPush = p["immPush"].flag();
    b.disengB = p["disengB"].flag();
    b.redirect = false;   // Перенаправление требует приспешников
    b.immCond = maskOf(p["immCond"], C_N) & ~1u;
    b.saveProf = maskOf(p["saveProf"], STAT_N);
    // владения (v21): бонус мастерства +2…+6, навыки и экспертиза (индексы Skill), число инструментов, владение своим оружием
    b.prof = j["prof"].num(2);
    if (!inRange(b.prof, 2, 6, "бонус мастерства", err)) return false;
    b.wNoProf = j["weapon"].has("prof") && !j["weapon"]["prof"].flag();
    b.skillProf = maskOf(p["skills"], SK_N);
    b.skillExp = maskOf(p["expert"], SK_N);
    if (b.skillExp & ~b.skillProf) { err = "экспертиза — только в навыке, которым владеешь"; return false; }
    b.toolN = p["tools"].t == Json::ARR ? (int)p["tools"].a.size() : p["tools"].num(0);   // список названий или число
    if (!inRange(b.toolN, 0, 40, "инструменты", err)) return false;
    b.fly = p["fly"].flag();
    b.speed = p.has("speed") ? p["speed"].num(30) : 30 + p["speedBonus"].num(0);   // speedBonus — старые описания (надбавка к 30 фт)
    // «когда → что» (v21): правила [{when, warg, what, n, s, once}]; старые описания (тактика стаи, безрассудство, ярость,
    // добивание, условная защита, преимущество на спасы) переводятся в правила
    {
        auto addR = [&](int w, int wa, int e, int n, int sd, bool once) { Rule r; r.when = w; r.warg = wa; r.what = e; r.n = n; r.s = sd; r.once = once; b.rules.push_back(r); };
        const Json& rl = p["rules"];
        for (size_t i = 0; i < rl.size(); i++) {
            const Json& q = rl[i];
            addR(q["when"].num(0), q["warg"].num(0), q["what"].num(0), q["n"].num(0), q["s"].num(6), q["once"].flag());
            b.rules.back().allyR = q["allyR"].num(0);   // аура: и на союзников в пределах allyR фт (v21)
        }
        if (p["pack"].flag()) addR(W_ALLYADJ, 0, E_ADVATK, 0, 6, false);
        if (p["reckless"].flag()) { addR(W_RECKLESS, 0, E_ADVATK, 0, 6, false); addR(W_RECKLESS, 0, E_ADVVSME, 0, 6, false); }
        if (p["saveAdv"].flag()) addR(W_ALWAYS, 0, E_ADVSAVE, 0, 6, false);
        if (p["phN"].num(0) > 0) addR(W_HPLOW, 0, E_DMG, p["phN"].num(0), 6, false);
        if (p["cdN"].num(0) > 0) addR(W_FOECOND, p["cdK"].num(1), E_DMG, p["cdN"].num(0), 6, true);
        if (p["condDefT"].num(0) > 0) addR(p["condDefT"].num(0) == 1 ? W_HPHIGH : (p["condDefT"].num(0) == 2 ? W_HPLOW : W_NOCOND), 0, p["condDefS"].num(1) == 2 ? E_RESNM : E_RES, 0, 6, false);
        if (b.rules.size() > 12) { err = "правил «когда → что» не больше 12"; return false; }
        // «событие → эффект» (v21): [{on, what, n, s, b, cond, uses}] (uses 0 — без ограничения)
        const Json& tl = p["trigs"];
        for (size_t i = 0; i < tl.size(); i++) {
            const Json& q = tl[i];
            Trig t;
            t.on = q["on"].num(0);
            t.what = q["what"].num(0);
            t.n = q["n"].num(1);
            t.s = q["s"].num(6);
            t.b = q["b"].num(0);
            t.cond = q["cond"].num(0);
            t.arg = q["arg"].num(0);   // тип урона для «я получил урон выбранного типа»
            t.uses = q["uses"].num(0) > 0 ? q["uses"].num(0) : UNL;
            if (!inRange(t.on, 0, T_N - 1, "событие", err) || !inRange(t.what, 0, X_N - 1, "эффект события", err) || !inRange(t.n, 0, MAX_DICE, "кубики события", err) ||
                !inRange(t.b, 0, 1000, "прибавка события", err) || !isDie(t.s))
                { if (err.empty()) err = "грань кубика события: " + diceList(); return false; }
            if (!trigAllowed(t.on, t.what)) { err = std::string("событие «") + TON[t.on] + " → " + TDN[t.what] + "» не сочетается"; return false; }
            if ((t.what == X_COND || t.what == X_SELFCOND) && !inRange(t.cond, 1, C_N - 1, "состояние события", err)) return false;
            if (t.on == T_DMGTYPE && !inRange(t.arg, 0, DT_N - 1, "тип урона события", err)) return false;
            if (t.what == X_STAY1) t.uses = std::min(t.uses, MAX_COUNT);
            b.trigs.push_back(t);
        }
        if (b.trigs.size() > 12) { err = "событий не больше 12"; return false; }
        for (auto& r : b.rules) {
            if (!inRange(r.when, 0, W_N - 1, "условие правила", err) || !inRange(r.what, 0, E_N - 1, "эффект правила", err)) return false;
            if (!ruleAllowed(r.when, r.what)) { err = std::string("правило «") + WN[r.when] + " → " + EN[r.what] + "»: резист и спасброски — только с условием о себе"; return false; }
            if (r.when == W_FOECOND && !inRange(r.warg, 1, C_N - 1, "состояние противника в правиле", err)) return false;
            if (r.what == E_DMG && (!inRange(r.n, 1, MAX_DICE, "кубики правила", err) || std::find(DICE, DICE + DICE_N, r.s) == DICE + DICE_N)) { if (err.empty()) err = "грань кубика правила"; return false; }
            if (r.what == E_AC && !inRange(r.n, 1, 20, "+КД правила", err)) return false;
            if (r.what == E_SAVEB && !inRange(r.n, 1, 20, "+ к спасброскам в правиле", err)) return false;
            if ((r.what == E_ATKDIE || r.what == E_SAVEDIE) && (!inRange(r.n, 1, MAX_DICE, "кости к броску в правиле", err) || !isDie(r.s))) { if (err.empty()) err = "грань кости к броску: " + diceList(); return false; }
            if (r.what == E_DMGRED && !inRange(r.n, 1, 1000, "снижение урона в правиле", err)) return false;
            if (r.what == E_PIERCE && !inRange(r.n, 0, DT_N - 1, "тип урона в правиле", err)) return false;
            if (r.what == E_POWER && !inRange(r.n, 1, 20, "размен в правиле", err)) return false;
            if (!inRange(r.allyR, 0, MAX_FT, "радиус ауры правила", err)) return false;
            if (r.allyR % 5) { err = "радиус ауры кратен 5 фт"; return false; }
            if (r.allyR > 0 && r.once) r.once = false;   // у аур нет «раз в ход»
        }
    }
    b.riposte = p["riposte"].flag();
    b.surge = p["surge"].num(0);
    for (int L = 1; L < 10; L++) b.slots[L] = p["slots"][L].num(0);
    b.dv = p["dv"].num(0);
    b.blindsight = p["blindsight"].num(0);
    b.truesight = p["truesight"].num(0);   // истинное зрение (v21)
    if (b.truesight != 0 && b.truesight != 10 && b.truesight != 30 && b.truesight != 60 && b.truesight != 120) { err = "истинное зрение: 0, 10, 30, 60 или 120 фт"; return false; }
    b.ignDiff = p["ignDiff"].flag();
    b.climbS = p["climbS"].flag();   // скорости лазания, плавания, копания (v21)
    b.swimS = p["swimS"].flag();
    b.burrowS = p["burrowS"].flag();
    b.lair = p["lair"].num(0);   // действия логова (v21): уровень 0–3
    if (b.lair < 0 || b.lair > 3) { err = "действия логова: уровень 0–3"; return false; }
    b.evasion = p["evasion"].flag();   // Увёртливость, солнечная чувствительность (v21)
    b.sunSens = p["sunSens"].flag();
    if (!inRange(b.regenN, 0, MAX_DICE, "регенерация", err) || !inRange(b.legRes, 0, MAX_COUNT, "легендарное сопротивление", err) || !inRange(b.legAct, 0, MAX_COUNT, "легендарные действия", err) ||
        !inRange(b.parry, 0, MAX_COUNT, "парирование", err) || !inRange(b.speed, 0, MAX_FT, "скорость", err) ||
        !inRange(b.surge, 0, MAX_COUNT, "всплеск действий", err))
        return false;
    if (b.speed % 5) { err = "скорость покупается шагами по 5 фт (с нуля)"; return false; }
    if (b.dv != 0 && b.dv != 30 && b.dv != 60 && b.dv != 120) { err = "тёмное зрение: 0, 30, 60 или 120 фт"; return false; }
    if (b.blindsight != 0 && b.blindsight != 10 && b.blindsight != 30 && b.blindsight != 60) { err = "слепое зрение: 0, 10, 30 или 60 фт"; return false; }
    for (int L = 1; L < 10; L++)
        if (!inRange(b.slots[L], 0, MAX_COUNT, "ячейки заклинаний", err)) return false;
    const Json& au = p["aura"];
    b.auraN = au["n"].num(0);
    b.auraS = au["s"].num(6);
    b.auraType = au["type"].num(DT_FIRE);
    b.auraRad = au["rad"].num(0);
    b.auraCond = au["cond"].num(0);
    b.auraRounds = au["rounds"].num(1);
    if (b.auraRad > 0) {
        if (!inRange(b.auraRad, 10, MAX_FT, "радиус ауры", err) || b.auraRad % 5) { err = "радиус ауры: 10–" + std::to_string(MAX_FT) + " фт, кратно 5"; return false; }
        if (!inRange(b.auraN, 0, MAX_DICE, "кубики ауры", err) || !inRange(b.auraType, 0, DT_N - 1, "тип урона ауры", err) || !inRange(b.auraCond, 0, C_N - 1, "состояние ауры", err) ||
            !inRange(b.auraRounds, 1, MAX_ROUNDS, "раундов состояния ауры", err)) return false;
        if (!isDie(b.auraS)) { err = "грань кубика ауры: " + diceList(); return false; }
        if (b.auraN == 0 && b.auraCond == 0) { err = "аура без урона и состояния"; return false; }
    } else { b.auraRad = 0; b.auraN = 0; b.auraCond = 0; }
    const Json& rd = p["rider"];
    b.rN = rd["n"].num(0);
    b.rS = rd["s"].num(6);
    b.rType = rd["type"].num(DT_FIRE);
    b.rCond = rd["cond"].num(0);
    b.rGrabRestr = rd["grabRestr"].flag();   // пакет атомов 2: захват — опутан; урон каждый ход, пока держит
    b.rGrabDotN = rd["grabDotN"].num(0);
    b.rGrabDotS = rd["grabDotS"].num(6);
    if (!inRange(b.rGrabDotN, 0, MAX_DICE, "урон захвата", err)) return false;
    b.rRounds = rd["rounds"].num(1);
    if (b.rN > 0 || b.rCond > 0) {
        if (b.wN <= 0) { err = "довесок к удару требует оружия"; return false; }
        if (!inRange(b.rN, 0, MAX_DICE, "кубики довеска", err) || !inRange(b.rType, 0, DT_N - 1, "тип урона довеска", err) || !inRange(b.rCond, 0, C_N - 1, "состояние довеска", err) ||
            !inRange(b.rRounds, 1, MAX_ROUNDS, "раундов состояния довеска", err)) return false;
        if (b.rN > 0 && !isDie(b.rS)) { err = "грань кубика довеска: " + diceList(); return false; }
    }
    hpWanted = j["hp"].num(0);
    if (hpWanted < 0) { err = "здоровье не может быть отрицательным"; return false; }
    return true;
}

// Босс по описанию: проверка бюджета, ХП = всё оставшееся (или меньше, если игрок задал). Возвращает false с текстом ошибки при нарушении правил
static bool buildCharacter(const Json& j, const Coef& C, Boss& b, long long& budget, std::string& err, long long* spentOut = nullptr, long long* leftOut = nullptr) {
    int hpWanted = 0;
    if (!bossFromJson(j, b, budget, hpWanted, err)) return false;
    // доспехи и щит (v21): тяжёлый — от СИЛ 13; щит занимает руку (нет второй руки); КД = 10 + единицы + ЛОВ по доспеху + щит
    if (b.armor == AR_HEAVY && b.stats[STR] < 13) { err = "тяжёлый доспех требует СИЛ 13 или выше"; return false; }
    if (b.shield && hasOffhand(b)) { err = "щит занимает руку: атаки второй рукой не будет"; return false; }
    // призыв (v21): у способности "summon":{"groups":[{"n":копий,"minion":{описание приспешника, свой бюджет}}, …]} — до 4 групп
    // разных приспешников; приспешник собирается теми же правилами, сам призывать не может
    for (size_t i = 0; i < b.abil.size() && i < j["abilities"].size(); i++) {
        const Json& sm = j["abilities"][i]["summon"];
        Spec& s = b.abil[i];
        const std::string at = "способность " + std::to_string(i + 1) + ": ";
        const Json& fm = j["abilities"][i]["form"];   // превращение: "form":{"body":{описание облика}}
        if (fm.t == Json::OBJ) {
            for (const Json& ma : fm["body"]["abilities"].a)
                if (ma["summon"].t == Json::OBJ || ma["form"].t == Json::OBJ) { err = at + "в облике нет своих призывов и превращений"; return false; }
            Boss mb;
            long long mbud = 0;
            std::string e2;
            if (!buildCharacter(fm["body"], C, mb, mbud, e2)) { err = at + "облик: " + e2; return false; }
            s.formTmpl = (int)b.tmpl.size();
            s.sumCost = mb.cost;
            b.tmpl.push_back(mb);
            continue;
        }
        if (sm.t != Json::OBJ) continue;
        const Json& gs = sm["groups"];
        if (gs.size() < 1 || gs.size() > 4) { err = at + "призыв — от 1 до 4 групп приспешников"; return false; }
        for (size_t g = 0; g < gs.size(); g++) {
            const Json& mj = gs[g]["minion"];
            for (const Json& ma : mj["abilities"].a)
                if (ma["summon"].t == Json::OBJ) { err = at + "приспешник сам призывать не может"; return false; }
            Boss mb;
            long long mbud = 0;
            std::string e2;
            if (!buildCharacter(mj, C, mb, mbud, e2)) { err = at + "приспешник " + std::to_string(g + 1) + ": " + e2; return false; }
            const int n = gs[g]["n"].num(1);
            if (!inRange(n, 1, 20, "копий приспешника", err)) { err = at + err; return false; }
            if (g == 0) {
                s.sumTmpl = (int)b.tmpl.size();
                s.sumN = n;
                s.sumCost = mb.cost;
            } else
                s.sumMore.push_back({(long long)b.tmpl.size(), n, mb.cost});
            b.tmpl.push_back(mb);
        }
    }
    b.ac = calcAC(b);
    b.hp = 1;
    long long spent = 0;
    for (auto& it : bossItems(b, C, (double)budget)) spent += it.cost;   // с одним ХП (2 очка)
    const long long nonHp = spent - 2;
    long long left = budget - nonHp;
    if (left < 2) { err = "не хватает очков даже на 1 ХП: потрачено " + std::to_string(nonHp) + " из " + std::to_string(budget); if (spentOut) *spentOut = nonHp; if (leftOut) *leftOut = left; return false; }
    long long hp = left / 2;
    if (hpWanted > 0) {
        if (hpWanted > hp) { err = "ХП " + std::to_string(hpWanted) + " дороже остатка бюджета (хватает на " + std::to_string(hp) + ")"; return false; }
        hp = hpWanted;
    }
    b.hp = (int)std::min<long long>(hp, 1000000);
    if (b.hp < 1) b.hp = 1;
    b.cost = nonHp + 2LL * b.hp;
    if (spentOut) *spentOut = b.cost;
    if (leftOut) *leftOut = budget - b.cost;
    {
        const double prevB = g_B;
        g_B = (double)budget + STAT_BASE;   // цена способности зависит от бюджета (лестницы): та же, что в разборе bossItems
        PcGuard pcg(b);                     // и от тела (атаки, КД, модификаторы)
        for (auto& sp : b.abil) sp.price = specPrice(sp, C);
        b.wcost = b.wN > 0 ? wpCostOf(C, b.wN, b.wS, b.wRange, b.reach, b.wAtt, b.wFinesse, b.wAmmo) : 0;
        b.acost = 0;
        for (auto& q : b.alt) b.acost += (q.cost = wpPrice(q, C));
        if (b.wN > 0) b.wcost += weaponLongPrice(C, b.wN, b.wS, b.wRange, b.wLong, b.reach, b.wAtt);
        g_B = prevB;
    }
    return true;
}

// Ответ на «price»: разбор цены по покупкам, остаток, ошибки правил (даже при перерасходе возвращаем разбор, чтобы игрок видел, где потратил)
// Разбор цены способности для конструктора (после теста 4 ноября: «видно, что сколько стоит»): для каждой части —
// насколько способность дешевле без неё (у скидок — отрицательное: столько они экономят). Пары [название, очки].
static std::string specPartsJson(const Spec& s, const Coef& C) {
    const long long full = specPrice(s, C);
    std::string o = "[";
    bool first = true;
    auto part = [&](const std::string& name, const std::function<void(Spec&)>& drop) {
        Spec t = s;
        drop(t);
        const long long d = full - specPrice(t, C);
        if (d == 0) return;
        o += std::string(first ? "" : ",") + "[" + jq(name) + "," + std::to_string(d) + "]";
        first = false;
    };
    if (s.dmgN > 0) part("урон " + std::to_string(s.dmgN) + "к" + std::to_string(s.dmgS), [](Spec& t) { t.dmgN = 0; });
    if (!s.more.empty()) part("доп. урон другого типа", [](Spec& t) { t.more.clear(); });
    if (s.cond > 0) part(std::string("состояние «") + CN[s.cond] + "» на " + std::to_string(s.condRounds) + " р.", [](Spec& t) { t.cond = 0; });
    if (s.pushFt > 0) part("толчок " + std::to_string(s.pushFt) + " фт", [](Spec& t) { t.pushFt = 0; t.collN = 0; });
    if (s.pullFt > 0) part("притягивание " + std::to_string(s.pullFt) + " фт", [](Spec& t) { t.pullFt = 0; });
    if (s.dotN > 0) part("урон по времени", [](Spec& t) { t.dotN = 0; });
    if (s.healN > 0 || s.healB > 0) part("лечение", [](Spec& t) { t.healN = 0; t.healB = 0; });
    if (s.teleFt > 0) part("телепорт " + std::to_string(s.teleFt) + " фт", [](Spec& t) { t.teleFt = 0; });
    if (s.buffN > 0) part("бафф оружия +" + std::to_string(s.buffN) + "к" + std::to_string(s.buffS) + " на " + std::to_string(s.buffRounds) + " р.", [](Spec& t) { t.buffN = 0; });
    if (s.acB > 0) part("+" + std::to_string(s.acB) + " КД на " + std::to_string(s.acRounds) + " р.", [](Spec& t) { t.acB = 0; });
    if (s.tempN > 0) part("временные хиты " + std::to_string(s.tempN) + "к6", [](Spec& t) { t.tempN = 0; });
    if (s.sbAmt > 0) part("бафф характеристики", [](Spec& t) { t.sbAmt = 0; });
    if (s.invisN > 0) part("невидимость " + std::to_string(s.invisN) + " р.", [](Spec& t) { t.invisN = 0; });
    if (s.darkR > 0) part("тьма", [](Spec& t) { t.darkR = 0; });
    if (s.hide) part("укрытие", [](Spec& t) { t.hide = false; });
    if (s.terr) part("труднопроходимая местность", [](Spec& t) { t.terr = false; });
    if (s.wallLen > 0) part("барьер", [](Spec& t) { t.wallLen = 0; });
    if (!s.self && s.range > 5) part("дальность " + std::to_string(s.range) + " фт", [](Spec& t) { t.range = 5; });
    if (!s.self && s.radius > 0) part("область " + std::to_string(s.radius) + " фт", [](Spec& t) { t.radius = 0; });
    if (s.targets > 1) part("целей: " + std::to_string(s.targets), [](Spec& t) { t.targets = 1; });
    if (!s.self && s.delivery == 2) part("всегда попадает", [](Spec& t) { t.delivery = 0; });
    if (s.bonus) part("бонусным действием", [](Spec& t) { t.bonus = false; });
    if (s.react) part("реакцией", [](Spec& t) { t.react = 0; });
    if (s.ally) part("и на союзника", [](Spec& t) { t.ally = false; });
    if (s.dcB > 0) part("СЛ +" + std::to_string(s.dcB), [](Spec& t) { t.dcB = 0; });
    if (s.uses < UNL) part(std::to_string(s.uses) + " раз за бой", [](Spec& t) { t.uses = UNL; });
    if (s.recharge) part("перезарядка", [](Spec& t) { t.recharge = 0; });
    if (s.conc) part("концентрация", [](Spec& t) { t.conc = false; });
    if (s.slotLvl > 0) part("расходует ячейку", [](Spec& t) { t.slotLvl = 0; });
    return o + "]";
}

static std::string priceReply(const Json& j, const Coef& C) {
    Boss b;
    long long budget = 0, spent = 0, left = 0;
    std::string err;
    const bool ok = buildCharacter(j, C, b, budget, err, &spent, &left);
    if (ok) {
        const std::string name = j["name"].str("Безымянный");
        std::string parts = "[";   // разбор цены каждой способности (то же тело и бюджет, что в цене)
        {
            const double prevB = g_B;
            g_B = (double)budget + STAT_BASE;
            PcGuard pcg(b);
            for (size_t i = 0; i < b.abil.size(); i++) parts += (i ? "," : "") + specPartsJson(b.abil[i], C);
            g_B = prevB;
        }
        return "{\"ok\":true,\"spent\":" + std::to_string(spent) + ",\"left\":" + std::to_string(left) + ",\"hp\":" + std::to_string(b.hp) + ",\"abParts\":" + parts + "],\"boss\":" +
               bossViewJson(b, C, (double)budget, name, false) + "}";
    }
    // перерасход или нарушение правил: пробуем показать разбор без проверки ХП
    Boss b2;
    long long bud2 = 0;
    int hpw = 0;
    std::string err2;
    std::string out = "{\"ok\":false,\"err\":" + jq(err);
    if (bossFromJson(j, b2, bud2, hpw, err2)) {
        b2.ac = calcAC(b2);
        b2.hp = 1;
        long long sp = 0;
        for (auto& it : bossItems(b2, C, (double)bud2)) sp += it.cost;
        out += ",\"spent\":" + std::to_string(sp - 2) + ",\"left\":" + std::to_string(bud2 - (sp - 2)) + ",\"boss\":" + bossViewJson(b2, C, (double)bud2, j["name"].str("Безымянный"), false);
    }
    return out + "}";
}

// Каталог для интерфейса: перечисления, прайс пассивов при данном бюджете, коэффициенты надбавок и скидок (для справочной шпаргалки)
static std::string catalogReply(long long budget, const Coef& C) {
    const double prev = g_B;
    g_B = (double)budget + STAT_BASE;   // budget — видимый, лестницы и множители считаются от внутреннего
    auto P = [&](int a, double m = 1.0) { return C[a] * m * scl(a); };
    auto cp = [&](double x) { return (long long)std::ceil(x); };
    std::string o = "{\"budget\":" + std::to_string(budget) + ",\"minBudget\":" + std::to_string(MIN_BUDGET) + ",\"maxBudget\":" + std::to_string(MAX_BUDGET) +
                    ",\"statMax\":" + std::to_string(STAT_MAX) + ",\"acMax\":" + std::to_string(AC_MAX - 10) + ",\"maxFt\":" + std::to_string(MAX_FT) +
                    ",\"maxRounds\":" + std::to_string(MAX_ROUNDS) + ",\"maxDice\":" + std::to_string(MAX_DICE) + ",\"maxCount\":" + std::to_string(MAX_COUNT) + ",\"dice\":[";
    for (int k = 0; k < DICE_N; k++) o += std::string(k ? "," : "") + std::to_string(DICE[k]);
    o += "],\"stats\":[";
    for (int i = 0; i < STAT_N; i++) o += std::string(i ? "," : "") + jq(SN[i]);
    o += "],\"skills\":[";
    for (int k = 0; k < SK_N; k++) o += std::string(k ? "," : "") + "{\"name\":" + jq(SKN[k]) + ",\"stat\":" + std::to_string(SKST[k]) + ",\"combat\":" + (skCombat(k) ? "true" : "false") + "}";
    o += "],\"dtypes\":[";
    for (int i = 0; i < DT_N; i++) o += std::string(i ? "," : "") + jq(DTN[i]);
    o += "],\"conds\":[";
    for (int i = 0; i < C_N; i++) o += std::string(i ? "," : "") + jq(CN[i]);
    o += "],\"condPrice\":[";
    for (int i = 0; i < C_N; i++) o += std::string(i ? "," : "") + (i ? std::to_string(cp(C[AT_PRONE + i - 1] * scl(AT_PRONE + i - 1))) : "0");
    o += "],\"dshare\":[";
    for (int i = 0; i < DT_N; i++) o += std::string(i ? "," : "") + std::to_string(g_share[i]);
    o += "],\"passives\":[";
    struct Pv { const char* key; const char* label; double price; };
    std::vector<Pv> pv = {
        {"resNM", "Резист к немагическому физическому", P(AT_RESNM, 5.0 * g_nmShare)},
        {"immNM", "Иммунитет к немагическому физическому", P(AT_IMMNM, 5.0 * g_nmShare)},
        {"wMagic", "Магическое оружие", P(AT_WMAGIC)},
        {"uncanny", "Невероятное уклонение", P(AT_UNCANNY)},
        {"magicRes", "Сопротивление магии", P(AT_MAGICRES)},
        {"spellRes", "Сопротивление урону от способностей", P(AT_SPELLRES)},
        {"critImm", "Иммунитет к критам", P(AT_CRITIMM)},
        {"immPush", "Неподвижность (иммунитет к отталкиванию)", P(AT_IMMPUSH)},
        {"disengB", "Отход бонусным действием", P(AT_DISENGB)},
        {"fly", "Полёт", P(AT_FLY)},
        {"riposte", "Контратака", P(AT_RIPOSTE)},
        {"ignDiff", "Лёгкая поступь", P(AT_IGNTERR)},
        {"climbS", "Скорость лазания", P(AT_CLIMB)},
        {"swimS", "Скорость плавания", P(AT_SWIM)},
        {"burrowS", "Скорость копания", P(AT_BURROW)},
        {"evasion", "Увёртливость", P(AT_EVASION)},
        {"sunSens", "Солнечная чувствительность (возврат очков)", -std::ceil(P(AT_SUNSENS))},
    };
    for (size_t i = 0; i < pv.size(); i++) o += std::string(i ? "," : "") + "{\"key\":" + jq(pv[i].key) + ",\"label\":" + jq(pv[i].label) + ",\"price\":" + std::to_string(cp(pv[i].price)) + "}";
    o += "],\"unit\":{\"regen\":" + std::to_string(cp(P(AT_REGEN))) + ",\"resist5\":" + std::to_string(P(AT_RESIST, 5.0)) + ",\"immCond\":" + std::to_string(cp(P(AT_IMMCOND))) +
         ",\"saveProf\":" + std::to_string(cp(P(AT_SAVEPROF))) + ",\"prof1\":" + std::to_string(cp(P(AT_PROF))) + ",\"skillC\":" + std::to_string(cp(P(AT_SKILLC))) +
         ",\"skillNC\":" + std::to_string(cp(P(AT_SKILLNC))) + ",\"noWProf\":" + std::to_string(cp(P(AT_NOWPROF))) + ",\"shield\":" + std::to_string(cp(C[AT_SHIELD])) + ",\"armDex\":" + std::to_string(cp(C[AT_ARMDEX])) + ",\"legRes\":" + std::to_string(cp(P(AT_LEGRES))) + ",\"legAct\":" + std::to_string(cp(P(AT_LEGACT))) + ",\"lair\":" + std::to_string(cp(P(AT_LAIR))) +
         ",\"parry\":" + std::to_string(cp(P(AT_PARRY))) + ",\"surge\":" + std::to_string(cp(P(AT_SURGE))) +
         ",\"speed30\":" + std::to_string(cp(P(AT_SPEED, speedUnits(30)))) + ",\"slot\":" + std::to_string(C[AT_SLOT]) +
         ",\"imm5\":" + std::to_string(P(AT_IMMTYPE, 5.0)) + ",\"vuln5\":" + std::to_string(P(AT_VULN, 5.0)) + ",\"aura\":" + std::to_string(P(AT_AURA)) + ",\"rdmg\":" + std::to_string(P(AT_RDMG)) +
         ",\"collide\":" + std::to_string(P(AT_COLLIDE)) + ",\"invis\":" + std::to_string(P(AT_INVIS)) + ",\"dark\":" + std::to_string(P(AT_DARK)) + ",\"terr\":" + std::to_string(P(AT_TERR)) +
         ",\"statbuf\":" + std::to_string(P(AT_STATBUF)) + ",\"hide\":" + std::to_string(P(AT_HIDE)) + ",\"summon\":" + std::to_string(P(AT_SUMMON)) + ",\"darkv\":" + std::to_string(cp(P(AT_DARKV))) + ",\"blind\":" + std::to_string(cp(P(AT_BLINDS))) + "}";
    // «когда → что»: условия и эффекты, какие сочетания разрешены (резист и спасброски — только условия о себе)
    o += ",\"whens\":[";
    for (int w = 0; w < W_N; w++) o += std::string(w ? "," : "") + "{\"name\":" + jq(WN[w]) + ",\"self\":" + (whenSelf(w) ? "true" : "false") + ",\"share\":" + std::to_string(C[AT_W_ALWAYS + w]) + "}";
    o += "],\"whats\":[";
    for (int e = 0; e < E_N; e++) o += std::string(e ? "," : "") + jq(EN[e]);
    o += "]";
    o += ",\"tons\":[";   // «событие → эффект» (v21): события, эффекты и какие сочетаются
    for (int k = 0; k < T_N; k++) o += std::string(k ? "," : "") + jq(TON[k]);
    o += "],\"tdos\":[";
    for (int k = 0; k < X_N; k++) o += std::string(k ? "," : "") + jq(TDN[k]);
    o += "],\"tok\":[";
    for (int k = 0; k < T_N; k++) {
        o += std::string(k ? "," : "") + "[";
        for (int x = 0; x < X_N; x++) o += std::string(x ? "," : "") + (trigAllowed(k, x) ? "1" : "0");
        o += "]";
    }
    o += "]";
    // шпаргалка: цены ступеней размерных атомов при этом бюджете (накопленные tierN) и процентные модификаторы способности
    o += ",\"mods\":{\"floor\":" + std::to_string(MULT_FLOOR) + ",\"auto\":" + std::to_string(C[AT_AUTO] - 1) + ",\"bonus\":" + std::to_string(C[AT_BONUS] - 1) +
         ",\"conc\":" + std::to_string(1 - C[AT_CONC]) + ",\"slot\":" + std::to_string(1 - C[AT_SLOTSPEC]) + ",\"usesAt1\":" + std::to_string(1 - C[AT_ULIM]) +
         ",\"rechargeAt0\":" + std::to_string(1 - C[AT_RECHARGE]) + "}";
    o += ",\"ladders\":{";
    const struct { const char* key; int id; int maxN; } L[] = {{"range", TR_RANGE, 40}, {"area", TR_AREA, 20}, {"push", TR_PUSH, 20}, {"tele", TR_TELE, 20}, {"speed", TR_SPEED, 24},
                                                              {"auraRad", TR_AURARAD, 20}, {"auraDice", TR_AURADICE, 40}, {"dmgDice", TR_DMG, 60}, {"temp", TR_TEMP, 40}, {"acb", TR_ACB, 10},
                                                              {"att", TR_ATT, 30}, {"nabil", TR_NABIL, 12}};
    for (size_t k = 0; k < sizeof L / sizeof L[0]; k++) {
        o += std::string(k ? "," : "") + jq(L[k].key) + ":[";
        for (int n = 0; n <= L[k].maxN; n++) { char buf[32]; std::snprintf(buf, sizeof buf, "%.4g", tierN(L[k].id, n)); o += std::string(n ? "," : "") + buf; }
        o += "]";
    }
    o += "},\"coef\":{\"wpnDice\":" + std::to_string(C[AT_WPNDICE]) + ",\"wpnRange\":" + std::to_string(C[AT_WPNRANGE]) + ",\"reach\":" + std::to_string(C[AT_REACH]) + ",\"wpnAtt\":" + std::to_string(C[AT_WPNATT]) +
         ",\"dmg\":" + std::to_string(C[AT_DMG]) + ",\"aRange\":" + std::to_string(C[AT_ARANGE]) + ",\"area\":" + std::to_string(C[AT_AREA]) + ",\"push\":" + std::to_string(C[AT_PUSH]) +
         ",\"heal\":" + std::to_string(C[AT_HEAL]) + ",\"tele\":" + std::to_string(C[AT_TELE]) + ",\"temp\":" + std::to_string(C[AT_TEMP]) + ",\"buff\":" + std::to_string(C[AT_BUFF]) +
         ",\"wall\":" + std::to_string(C[AT_WALL]) + ",\"dot\":" + std::to_string(C[AT_DOT]) + ",\"nabil\":" + std::to_string(C[AT_NABIL] * scl(AT_NABIL)) + "}}";
    g_B = prev;
    return o;
}
}   // namespace chr
