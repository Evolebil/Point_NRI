// Правила: броски, состояния, урон, туман войны
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Правила: броски, состояния, урон
// ---------------------------------------------------------------------------
int d20(bool adv, bool dis) {
    if (adv == dis) return rollD20();
    int a = rollD20(), c = rollD20();
    return adv ? std::max(a, c) : std::min(a, c);
}
bool selfOn(const Fighter& me, const Rule& r);
int saveBonus(const Fighter& t, int stat) {
    int b = 0;   // «+N к спасброскам» — свои правила и ауры союзников (v21)
    forRules(t, [&](const Rule& r, int) {
        if (r.what == E_SAVEB && selfOn(t, r)) b += r.n;
    });
    return t.mod(stat) + (((t.b.saveProf >> stat) & 1u) ? t.b.prof : 0) + b;
}
bool advSave(const Fighter& t);
bool useReroll(const Fighter& f, int on);
// броски для показа игроку (кадры): пишутся, только когда бой записывается (g_evLog), чтобы не тормозить обучение
thread_local bool g_evLog = false;
void evPush(const std::string& json);
inline int fIdx(const Fighter& f) { return g_curW ? (int)(&f - &g_curW->f[0]) : -1; }
inline void evSave(const Fighter& t, int stat, int r, int b, int dc, bool ok, bool autoFail) {
    if (!g_evLog) return;
    evPush("{\"k\":\"save\",\"who\":" + std::to_string(fIdx(t)) + ",\"stat\":" + std::to_string(stat) + ",\"d20\":" + std::to_string(r) +
           ",\"b\":" + std::to_string(b) + ",\"dc\":" + std::to_string(dc) + ",\"ok\":" + (ok ? "1" : "0") + (autoFail ? ",\"auto\":1" : "") + "}");
}
bool saveOK(const Fighter& t, int stat, int dc) {
    // PHB: оглушён/парализован автоматически проваливает СИЛ и ЛОВ
    if ((t.cond[C_STUN] || t.cond[C_PARAL]) && (stat == STR || stat == DEX)) { evSave(t, stat, 0, 0, dc, false, true); return false; }
    // опутан — помеха на спасброски ЛОВ
    const bool adv = advSave(t), dis = (stat == DEX && (t.cond[C_RESTR] > 0 || squeezed(t))) || t.exh >= 3 || ((stat == STR || stat == DEX) && t.b.noArmProf && (t.b.armor || t.b.shield));
    int die = 0;   // «+ кость к спасброскам» (Благословение) — свои правила и ауры (v21)
    forRules(t, [&](const Rule& r, int) {
        if (r.what == E_SAVEDIE && selfOn(t, r)) die += rollDice(r.n, r.s);
    });
    const int sb = saveBonus(t, stat);
    int r = d20(adv, dis);
    bool ok = r + sb + die >= dc;
    if (!ok && useReroll(t, T_MYSAVEFAIL)) { r = d20(adv, dis); ok = r + sb + die >= dc; }   // переброс (Везунчик)
    evSave(t, stat, r, sb + die, dc, ok, false);
    return ok;
}
// счётчик для статистики
thread_local long long g_lrUsed = 0;
// статистика перезарядки
thread_local long long g_rechUsed = 0, g_rechRolls = 0, g_rechOk = 0;
thread_local long long g_attacksMade = 0, g_legUsed = 0;
// статистика атак и легендарных действий
thread_local long long g_concCast = 0, g_concBroken = 0, g_surge = 0, g_ripUsed = 0, g_reactUsed = 0;
// Легендарное сопротивление: проваленный спасбросок становится успешным, пока есть заряды.
// Бот тратит заряд только на значимый эффект (см. lrWorth).
bool saveLR(Fighter& t, int stat, int dc, bool significant) {
    if (saveOK(t, stat, dc)) return true;
    if (significant && t.lrLeft > 0) {
        t.lrLeft--;
        g_lrUsed++;
        return true;
    }
    return false;
}
double dmgMult(const Boss& b, int t, bool magical, bool extraRes = false);
bool lrWorth(const Fighter& t, const Spec& s) {
    // потеря хода — всегда стоит заряда
    if (s.cond == C_STUN || s.cond == C_DISTR || s.cond == C_PARAL || s.cond == C_INCAP || s.cond == C_CHARM ||
        s.cond == C_FLEE)
        return true;
    // долгий контроль
    if (s.cond && s.condRounds >= 3) return true;
    double avg = 0;
    for (auto& d : s.dmgParts()) avg += d.n * (d.s + 1) / 2.0 * dmgMult(t.b, d.type, true);
    // удар, срезающий четверть оставшихся ХП
    return avg >= t.pol->v[PO_LRFRAC] * t.hp;
}
void fallCheck(Fighter& f);   // летун падает (ниже)
// захват (пакет атомов 2): схваченный опутан / получает урон каждый ход, пока его держит src
void grabExtras(Fighter& t, int src, bool restr, int dotN, int dotS, int type) {
    if (t.cond[C_GRAPPLE] <= 0 || t.grapSrc != src) return;
    if (restr) {
        t.cond[C_RESTR] = std::max(t.cond[C_RESTR], 1000);
        t.cstat[C_RESTR] = -1;
        t.restrByGrab = true;
    }
    if (dotN > 0) {
        t.grabDotN = dotN;
        t.grabDotS = dotS;
        t.grabDotType = type;
        t.grabDotSrc = src;
    }
}
// захват кончился — кончается и то, что держалось на нём
void grabSync(Fighter& t) {
    if (t.cond[C_GRAPPLE] > 0) return;
    if (t.restrByGrab) t.cond[C_RESTR] = 0;
    t.restrByGrab = false;
    t.grabDotN = 0;
}
bool applyCond(Fighter& t, int kind, int rounds, int stat, int dc, bool repeatSave, int src = -1, int concSrc = 0) {
    if (t.b.immCond & (1u << kind) || t.cimm[kind] > 0 || t.banished > 0) return false;
    // размеры (v21): схватить можно существо не больше чем на ступень крупнее себя
    if (kind == C_GRAPPLE && g_curW && src >= 0 && src < (int)g_curW->f.size() && t.b.size > g_curW->f[src].b.size + 1) return false;
    t.cond[kind] = std::max(t.cond[kind], rounds);
    // из захвата выбираются действием (СЛ), а не повторным спасом
    t.cstat[kind] = (repeatSave && kind != C_GRAPPLE) ? stat : -1;
    t.cdc[kind] = dc;
    // чьей концентрацией держится (индекс+1), 0 — ничьей
    t.condConc[kind] = concSrc;
    fallCheck(t);   // сбит с ног, опутан, схвачен, оглушён, парализован — летун падает
    if (kind == C_CHARM) t.charmSrc = src;
    if (kind == C_GRAPPLE) t.grapSrc = src;
    return true;
}
inline bool specMagical(const Boss& b, int type) {
    return !isPhys(type) || b.wMagic;
}
// оружие пробивает сопротивление немагическому физическому: магическое или (пакет атомов 2) посеребрённое
inline bool wPierceNM(const Boss& b) { return b.wMagic || b.wSilver; }
inline bool specMagical(const Boss& b, const Spec& s) {
    return specMagical(b, s.dtype);
}
// множитель урона: иммунитет 0; сопротивление ×0.5; уязвимость ×2 (вместе — ×1)
double dmgMult(const Boss& b, int t, bool magical, bool extraRes) {
    if ((b.immMask >> t) & 1) return 0.0;
    if (b.immNM && isPhys(t) && !magical) return 0.0;
    bool res = ((b.resMask >> t) & 1) || (b.resNM && isPhys(t) && !magical) || extraRes;
    bool vul = (b.vulnMask >> t) & 1;
    return (res ? 0.5 : 1.0) * (vul ? 2.0 : 1.0);
}
// «когда → что»: условие о себе (без противника)
bool selfOn(const Fighter& me, const Rule& r) {
    switch (r.when) {
        case W_ALWAYS: return true;
        case W_HPLOW: return me.hp * 2 <= me.maxHp;
        case W_HPHIGH: return me.hp * 2 > me.maxHp;
        case W_NOCOND:
            for (int k = 1; k < C_N; k++)
                if (me.cond[k] > 0) return false;
            return true;
        case W_RECKLESS: return me.wasReckless;
        case W_TWOWEAP: return me.b.wN > 0 && !me.disarmed && hasOffhand(me.b);
        case W_HEAVY: return me.b.wN > 0 && !me.disarmed && me.b.wHeavy;
    }
    return false;
}
// переброс к20 по событию (v21): тратит одно применение
bool useReroll(const Fighter& f, int on) {
    if (f.b.trigs.empty() || f.trigLeft.size() != f.b.trigs.size()) return false;
    for (size_t k = 0; k < f.b.trigs.size(); k++) {
        const Trig& t = f.b.trigs[k];
        if (t.on != on || t.what != X_REROLL || f.trigLeft[k] <= 0) continue;
        if (t.uses < UNL) f.trigLeft[k]--;
        return true;
    }
    return false;
}
// резист по правилу (условие о себе), пока его не подавили
bool condDefActive(const Fighter& d, int type, bool magical) {
    if (d.supp > 0 && (d.b.suppTgt & 4)) return false;
    bool on = false;
    forRules(d, [&](const Rule& r, int) {
        if ((r.what == E_RES || (r.what == E_RESNM && isPhys(type) && !magical)) && selfOn(d, r)) on = true;
    });
    return on;
}
// преимущество на спасброски по правилу
bool advSave(const Fighter& t) {
    bool on = g_saveVsAbil && t.b.magicRes;   // сопротивление магии: преимущество на спасброски от способностей
    forRules(t, [&](const Rule& r, int) {
        if (r.what == E_ADVSAVE && selfOn(t, r)) on = true;
    });
    return on;
}
// Урон по бойцу. concCheck = false — проверку концентрации ставит вызывающий (один урон из нескольких типов — одна
// проверка на сумму, PHB). Возвращает урон, который боец получил (после защит, вместе с ушедшим во временные хиты).
void shapeBack(Fighter& f);   // превращение: возврат (ниже)
int hurt(Fighter& d, int dmg, int type, bool magical, bool concCheck = true) {
    if (dmg <= 0 || d.banished > 0) return 0;   // изгнанного не достать
    if (g_track) {
        g_dmgT[type] += dmg;
        if (isPhys(type) && !magical) g_dmgNM += dmg;
    }
    // броски и ресурсы (v21): урон немагического физического меньше на N (Мастер тяжёлых доспехов); урон типа, которым
    // источник «игнорирует сопротивление», проходит сквозь резист (не сквозь иммунитет)
    if (isPhys(type) && !magical)
        forRules(d, [&](const Rule& r, int) {
            if (r.what == E_DMGRED && selfOn(d, r)) dmg -= r.n;
        });
    if (dmg <= 0) return 0;
    bool pierce = false;
    if (g_curW && g_srcIdx >= 0 && g_srcIdx < (int)g_curW->f.size())
        for (auto& r : g_curW->f[g_srcIdx].b.rules)
            if (r.what == E_PIERCE && r.n == type && selfOn(g_curW->f[g_srcIdx], r)) pierce = true;
    // защиты подавлены — остаётся лишь уязвимость
    const bool imm = ((d.b.immMask >> type) & 1) || (d.b.immNM && isPhys(type) && !magical);
    dmg = (int)std::floor(dmg * ((d.supp > 0 && (d.b.suppTgt & 4)) || (pierce && !imm)
                                     ? (((d.b.vulnMask >> type) & 1) ? 2.0 : 1.0)
                                     : dmgMult(d.b, type, magical, condDefActive(d, type, magical))));
    if (dmg <= 0) return 0;
    if (g_srcIdx >= 0) d.hurtBy = g_srcIdx;
    // урон подавляющего типа: пассивки отключены до конца следующего хода владельца
    if ((d.b.suppMask >> type) & 1) d.supp = d.inTurn ? 2 : 1;
    // концентрация: на каждое попадание своя СЛ спасброска ТЕЛ = max(10, урон/2); урон, ушедший во временные хиты,
    // тоже полученный урон (PHB)
    if (concCheck && d.concSpec >= 0) d.concDCs.push_back(std::max(10, dmg / 2));
    const int taken = dmg;
    // временные хиты поглощают урон первыми
    int ab = std::min(d.temp, dmg);
    d.temp -= ab;
    dmg -= ab;
    const bool wasUp = d.hp > 0;
    d.hp -= dmg;
    if (d.hp <= 0 && d.inForm) shapeBack(d);   // облик пал: свой облик, лишний урон — на свои хиты
    // «событие → эффект» (v21): мои хиты упали до 0 — остаться с 1 хитом (Неумолимая ярость) или получить временные хиты
    if (d.hp <= 0 && !d.b.trigs.empty() && d.trigLeft.size() == d.b.trigs.size())
        for (size_t k = 0; k < d.b.trigs.size(); k++) {
            const Trig& t = d.b.trigs[k];
            if (t.on != T_DOWN || d.trigLeft[k] <= 0) continue;
            if (t.uses < UNL) d.trigLeft[k]--;
            if (t.what == X_STAY1) d.hp = 1;
            if (t.what == X_TEMP && d.hp <= 0) {   // временные хиты не спасают от 0 — но их получает тот, кто остался
                d.hp = 1;
                d.temp = std::max(d.temp, rollDice(t.n, t.s) + t.b);
            }
            if (d.hp > 0) break;
        }
    // пакет атомов 1: «я получил урон выбранного типа» (страх огня) — пока жив
    if (taken > 0 && d.hp > 0 && g_curW && !d.b.trigs.empty()) {
        const int prevT = g_trigType;
        g_trigType = type;
        fireTrig(const_cast<World&>(*g_curW), d, T_DMGTYPE, nullptr);
        g_trigType = prevT;
    }
    // очки команды (v21): убитый враг — на счёт команды того, кто нанёс урон
    if (wasUp && d.hp <= 0 && g_curW && g_srcIdx >= 0 && g_srcIdx < (int)g_curW->f.size() && g_curW->f[g_srcIdx].team != d.team && g_curW->f[g_srcIdx].team < MAXT)
        g_curW->kills[g_curW->f[g_srcIdx].team]++;
    return taken;
}
bool allyAdjacent(const World& w, const Fighter& me, const Fighter& target) {
    for (auto& g : w.f)
        if (&g != &me && g.hp > 0 && g.team == me.team &&
            5 * std::max(std::abs(g.x - target.x), std::abs(g.y - target.y)) <= 5)
            return true;
    return false;
}
// «когда → что»: условие с противником foe (цель моей атаки или тот, кто атакует меня); melee — моя атака ближняя
// (безрассудство даёт преимущество только ближним атакам, а атаки по безрассудному — любые)
bool ruleOn(const World& w, const Fighter& me, const Fighter* foe, const Rule& r, bool offense, bool melee) {
    switch (r.when) {
        case W_ALLYADJ: return foe && allyAdjacent(w, me, *foe);
        case W_FOECOND: return foe && foe->cond[r.warg] > 0;
        case W_FOEHPLOW: return foe && foe->hp * 2 <= foe->maxHp;
        case W_RECKLESS: return me.wasReckless && (!offense || melee);
    }
    return selfOn(me, r);
}
// +КД по правилам против атакующего a
// «когда → что: преимущество на проверки навыка sk» (8 ноября; n — номер навыка)
bool skillAdv(const Fighter& f, int sk) {
    bool on = false;
    forRules(f, [&](const Rule& r, int) {
        if (r.what == E_SKILLADV && r.n == sk && selfOn(f, r)) on = true;
    });
    return on;
}
int ruleAC(const World& w, const Fighter& d, const Fighter& a) {
    int ac = 0;
    forRules(d, [&](const Rule& r, int) {
        if (r.what == E_AC && ruleOn(w, d, &a, r, false, false)) ac += r.n;
        if (r.what == E_ACMINUS && ruleOn(w, d, &a, r, false, false)) ac -= r.n;   // недостаток (пакет атомов 3)
    });
    return ac;
}
bool enemyAdjacent(const World& w, const Fighter& me) {
    for (auto& g : w.f)
        if (g.hp > 0 && g.team != me.team && distFt(me, g) <= 5) return true;
    return false;
}
// Видит ли viewer цель t: слепое зрение видит всё в радиусе, ослеплённый и «скрытый» не видны, цель в тьме видна лишь с
// тёмным зрением достаточной дальности
inline bool litAt(const World& w, const Fighter& t);   // свет (ниже)
bool canSee(const World& w, const Fighter& v, const Fighter& t) {
    if (t.banished > 0) return false;   // изгнан — вне боя
    int d = distFt(v, t);
    if (v.b.blindsight >= d) return true;
    const bool ts = v.b.truesight >= d && !v.cond[C_BLIND];   // истинное зрение (v21): невидимых и сквозь тьму
    if (v.cond[C_BLIND] || t.hidden || (t.invisRounds > 0 && !ts)) return false;
    if (w.dark[t.y][t.x] && v.b.dv < d && !ts) return false;
    // ночь (пакет атомов 2, свет): без тёмного зрения неосвещённое видно не дальше 30 фт
    if (!w.day && d > 30 && v.b.dv < d && !ts && !litAt(w, t)) return false;
    return true;
}
// цель освещена: в радиусе своего или чужого света (пассив «свет вокруг себя» или способность на время)
inline bool litAt(const World& w, const Fighter& t) {
    for (auto& g : w.f) {
        if (g.hp <= 0 || g.banished > 0) continue;
        const int r = std::max(g.b.lightR, g.lightRounds > 0 ? g.lightR : 0);
        if (r > 0 && distFt(g, t) <= r) return true;
    }
    return false;
}
thread_local int g_curRound = 0;   // номер текущего раунда (туман войны, восприятие МДР, наблюдение сети)
// ---- Туман войны (v21, №12): сторона видит врага, если его видит хоть один её боец (canSee и нет стены между ними).
// Бот, сеть и игрок знают только видимых врагов и тех, кого видели в прошлом раунде на той же клетке (атаковать «угаданную
// клетку» можно, с помехой). Атака или способность выдают позицию. Если сторона 5 раундов не знает ни одного врага, она их
// слышит (технический предохранитель от бесконечного боя вслепую).
bool teamSees(const World& w, int team, const Fighter& t) {
    for (auto& g : w.f)
        if (g.hp > 0 && g.team == team && !g.fled && (&g == &t || (canSee(w, g, t) && losF(w, g, t)))) return true;
    return false;
}
inline std::array<int, 3>& knownOf(World& w, int team, int i) {
    auto& K = w.known[team];
    if ((int)K.size() <= i) K.resize(w.f.size() > (size_t)i ? w.f.size() : (size_t)i + 1, {-1, -1, -1});
    return K[i];
}
void revealTo(World& w, int team, int i) {
    if (team < 0 || team > 1 || i < 0 || i >= (int)w.f.size()) return;
    knownOf(w, team, i) = {w.f[i].x, w.f[i].y, g_curRound};
    w.knownRound[team] = g_curRound;
}
void updateKnown(World& w, int team) {
    for (size_t i = 0; i < w.f.size(); i++)
        if (w.f[i].hp > 0 && w.f[i].team != team && teamSees(w, team, w.f[i])) revealTo(w, team, (int)i);
}
// враг i известен стороне: виден или стоит там, где его видели в этом или прошлом раунде
bool isKnown(const World& w, int team, int i) {
    const Fighter& e = w.f[i];
    if (e.team == team || teamSees(w, team, e)) return true;
    if (i >= (int)w.known[team].size()) return false;
    const auto& k = w.known[team][i];
    return k[2] >= 0 && k[0] == e.x && k[1] == e.y && k[2] >= g_curRound - 1;
}
// на солнце: день, клетка не во тьме и не под навесом (стоящий на настиле — под небом)
inline bool inSun(const World& w, const Fighter& f) { return w.day && !w.dark[f.y][f.x] && (f.up || !w.deck[f.y][f.x]); }
void advState(const World& w, const Fighter& a, const Fighter& d, int range, bool& adv, bool& dis) {
    // не видят меня — преимущество; не вижу цель — помеха
    adv = !canSee(w, d, a);
    dis = !canSee(w, a, d);
    if (d.dodging) dis = true;
    // «когда → что»: мои правила на атаку, правила цели на атаки по ней (безрассудство — тоже правило: рискнул в свой ход,
    // по тебе бьют с преимуществом до следующего хода)
    forRules(a, [&](const Rule& r, int) {
        if (r.what == E_ADVATK && ruleOn(w, a, &d, r, true, range <= 5)) adv = true;
        if (r.what == E_DISATK && ruleOn(w, a, &d, r, true, range <= 5)) dis = true;   // недостаток (пакет атомов 3)
    });
    forRules(d, [&](const Rule& r, int) {
        if ((r.what == E_DISVSME || r.what == E_ADVVSME) && ruleOn(w, d, &a, r, false, false)) (r.what == E_DISVSME ? dis : adv) = true;
    });
    if (d.cond[C_RESTR] || d.cond[C_BLIND] || d.cond[C_STUN] || d.cond[C_PARAL] || d.cond[C_LIT]) adv = true;
    if (d.cond[C_PRONE]) {
        if (distFt(a, d) <= 5)
            adv = true;
        else
            dis = true;
    }
    if (a.exh >= 3 || a.cond[C_PRONE] || a.cond[C_RESTR] || a.cond[C_FRIGHT] || a.cond[C_POISON] || a.cond[C_BLIND] ||
        a.cond[C_WEAK] || a.cond[C_FLEE])
        dis = true;
    if (range > 5 && enemyAdjacent(w, a)) dis = true;
    if (range > 5 && distFt(a, d) > range) dis = true;   // дальше обычной дистанции оружия (дальняя дистанция) — с помехой
    if (d.helpBy >= 0 && d.helpBy < (int)w.f.size() && &w.f[d.helpBy] != &a && w.f[d.helpBy].team == a.team) adv = true;   // Помощь союзника (v21)
    if (a.advNext) adv = true;   // «событие → эффект»: преимущество на следующую атаку
    if (a.b.noArmProf && (a.b.armor || a.b.shield)) dis = true;   // доспех или щит без владения
    if (squeezed(a)) dis = true;   // протискивается (PHB)
    if (a.b.sunSens && inSun(w, a)) dis = true;   // солнечная чувствительность
    if (squeezed(d)) adv = true;
}
int coverBonus(const World& w, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy,
        x = x0, y = y0;
    while (!(x == x1 && y == y1)) {
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
        // полукрытие: +2 КД (PHB "Укрытия")
        if ((x != x1 || y != y1) && (x != x0 || y != y0) && w.climb[y][x]) return 2;
    }
    return w.tree[y1][x1] ? 2 : 0;   // в кроне дерева — полукрытие листвы
}
inline bool canReact(const Fighter& d) {
    return d.hp > 0 && !d.reactionUsed && !d.cond[C_STUN] && !d.cond[C_PARAL] && !d.cond[C_INCAP];
}
// Невероятное уклонение: реакцией вдвое меньше урона от атаки, которая попала (бот тратит, если урон >= 10% максимума
// ХП)
int applyUncanny(Fighter& d, int dmg) {
    if (d.b.uncanny && canReact(d) && dmg >= d.pol->v[PO_UNCANNY] * d.maxHp) {
        spendReaction(d);
        return dmg / 2;
    }
    return dmg;
}
int allyReact(World& w, Fighter& a, Fighter& d);   // ниже: реакция за союзника (1 — помеха атаке, −1 — атакующий пал)
inline void evRoll(const Fighter& a, const Fighter& d, int r, int b, int ac, bool adv, bool dis, bool hit, bool crit) {
    if (!g_evLog) return;
    evPush("{\"k\":\"roll\",\"who\":" + std::to_string(fIdx(a)) + ",\"tgt\":" + std::to_string(fIdx(d)) + ",\"d20\":" + std::to_string(r) +
           ",\"b\":" + std::to_string(b) + ",\"ac\":" + std::to_string(ac) + ",\"adv\":" + std::to_string(adv == dis ? 0 : (adv ? 1 : -1)) +
           ",\"hit\":" + (hit ? "1" : "0") + ",\"crit\":" + (crit ? "1" : "0") + "}");
}
bool attackRoll(World& w, Fighter& a, Fighter& d, int mod, int range, bool& crit) {
    if (a.team != d.team) {
        a.struckFoe = true;
        revealTo(w, d.team, (int)(&a - &w.f[0]));   // атака выдаёт атакующего (туман войны)
    }
    // реакция цели «меня атакуют» (№23): до броска; если цель ушла (телепорт) или атакующий пал — атака не состоялась
    crit = false;
    if (hasReact(d.b)) {
        const int dx0 = d.x, dy0 = d.y;
        tryReact(w, (int)(&d - &w.f[0]), 1, (int)(&a - &w.f[0]));
        if (a.hp <= 0 || d.x != dx0 || d.y != dy0) return false;
    }
    // реакция «когда атакуют союзника рядом» (v21): помеха этой атаке или удар оружием по атакующему
    const int pr = allyReact(w, a, d);
    if (pr < 0) return false;   // атакующий пал от удара защитника
    const bool protDis = pr == 1;
    // отражения (пакет атомов 2): атака может уйти в копию (3 копии — 6+, 2 — 8+, 1 — 11+ на к20); копия КД 10 + ЛОВ
    if (d.images > 0) {
        const int need = d.images >= 3 ? 6 : (d.images == 2 ? 8 : 11);
        if (rollD20() >= need) {
            if (rollD20() + mod >= 10 + d.mod(DEX)) d.images--;
            crit = false;
            return false;
        }
    }
    bool adv, dis;
    advState(w, a, d, range, adv, dis);
    if (protDis || g_heavyDis) dis = true;
    if (d.helpBy >= 0 && d.helpBy < (int)w.f.size() && &w.f[d.helpBy] != &a && w.f[d.helpBy].team == a.team) d.helpBy = -1;   // Помощь — на одну атаку
    a.advNext = false;   // и преимущество от события — тоже
    int r = d20(adv, dis);
    crit = (r == 20);
    if (r == 1) { evRoll(a, d, r, mod, -1, adv, dis, false, false); return false; }
    bool noCover = false;
    int die = 0;   // броски и ресурсы (v21): «+ кость к броскам атаки», «атаки не замечают укрытия»
    forRules(a, [&](const Rule& rl, int) {
        if (rl.what == E_ATKDIE && ruleOn(w, a, &d, rl, true, range <= 5)) die += rollDice(rl.n, rl.s);
        if (rl.what == E_NOCOVER && ruleOn(w, a, &d, rl, true, range <= 5)) noCover = true;
    });
    int ac = d.b.ac + (d.acBuffRounds > 0 ? d.acBuff : 0) + (noCover ? 0 : coverBonus(w, a.x, a.y, d.x, d.y)) + ruleAC(w, d, a);
    bool hit = crit || r + mod + die >= ac;   // mod — полный бонус атаки (wAtk или sAtk: с мастерством)
    // переброс (v21): промахнулся — свой переброс (Везунчик); попали по мне — заставить перебросить (Второй шанс)
    if (!hit && useReroll(a, T_MISS)) {
        r = d20(adv, dis);
        crit = r == 20;
        if (r == 1) { evRoll(a, d, r, mod + die, ac, adv, dis, false, false); return false; }
        hit = crit || r + mod + die >= ac;
    }
    if (hit && !crit && useReroll(d, T_HITBY)) {
        r = d20(adv, dis);
        crit = r == 20;
        if (r == 1) { evRoll(a, d, r, mod + die, ac, adv, dis, false, false); return false; }
        hit = crit || r + mod + die >= ac;
    }
    // Парирование: реакцией +к КД против ближней атаки, которая попала бы; нужно видеть атакующего (скрытый не виден);
    // парируют оружием ближнего боя или голыми руками, но не с луком в руках
    if (hit && !crit && range <= 5 && d.b.parry > 0 && canReact(d) && handsForMelee(d.b) && !a.hidden &&
        r + mod + die < ac + d.b.parry) {
        spendReaction(d);
        hit = false;
    }
    // PHB: попадание в упор по парализованному — критическое
    if (hit && d.cond[C_PARAL] && distFt(a, d) <= 5) crit = true;
    if (crit && d.b.critImm) crit = false;   // иммунитет к критам (пакет атомов 1): попадание — обычное
    evRoll(a, d, r, mod + die, ac, adv, dis, hit, crit);
    // «по мне попали атакой вблизи» (огненный щит): урон или состояние атакующему
    if (hit && range <= 5 && !d.b.trigs.empty()) fireTrig(w, d, T_HITMELEE, &a);
    // «попал вблизи после разбега» (наскок, пакет атомов 2): за этот ход сместился хотя бы на 20 фт
    if (hit && range <= 5 && !a.b.trigs.empty() && a.turnX0 >= 0 && 5 * std::max(std::abs(a.x - a.turnX0), std::abs(a.y - a.turnY0)) >= 20)
        fireTrig(w, a, T_CHARGE, &d);
    return hit;
}
void oppAttack(World& w, Fighter& g, Fighter& victim) {
    bool crit;
    if (attackRoll(w, g, victim, g.wAtk(), 5, crit)) {
        int dmg = rollDice(g.b.wN * (crit ? 2 : 1), wDie(g.b));   // провоцирует только вооружённый (moveToward)
        hurt(victim, applyUncanny(victim, std::max(dmg + g.wMod(), 1)), g.b.wType, wPierceNM(g.b));
        riposte(w, victim, g);
    }
}
// Реакция «контратака»: по тому, кто попал по мне ближней атакой, — одна ближняя атака оружием или, без оружия,
// кулаком (1 + мод. СИЛ) (общий каркас: одна реакция за раунд)
void riposte(World& w, Fighter& d, Fighter& a) {
    if (&d == &a || d.team == a.team) return;   // своя область по себе или союзнику — не повод для контратаки
    if (!d.b.riposte || !handsForMelee(d.b) || !canReact(d) || a.hp <= 0 || distFt(d, a) > meleeReach(d.b)) return;
    spendReaction(d);
    g_ripUsed++;
    bool crit;
    if (attackRoll(w, d, a, d.wAtk(), 5, crit)) {
        int dmg = d.b.wN > 0 ? rollDice(d.b.wN * (crit ? 2 : 1), wDie(d.b)) : 1;
        const int src0 = g_srcIdx;
        g_srcIdx = (int)(&d - &w.f[0]);   // урон контратаки наносит d
        hurt(a, std::max(dmg + d.wMod(), 1), d.b.wType, wPierceNM(d.b));
        g_srcIdx = src0;
    }
}
double pHitEst(const World& w, const Fighter& a, const Fighter& d, int mod, int range) {
    bool adv, dis;
    advState(w, a, d, range, adv, dis);
    // ожидаемый эффект Парирования
    double ac = d.b.ac + (d.acBuffRounds > 0 ? d.acBuff : 0) + coverBonus(w, a.x, a.y, d.x, d.y) + ruleAC(w, d, a) +
                ((d.b.parry > 0 && range <= 5 && canReact(d)) ? a.pol->v[PO_PARRYW] * d.b.parry : 0.0);
    double p = std::clamp((21.0 - (ac - mod)) / 20.0, 0.05, 0.95);
    if (adv && !dis) return 1 - (1 - p) * (1 - p);
    if (dis && !adv) return p * p;
    return p;
}
double pFailEst(const Fighter& t, int stat, int dc) {
    if ((t.cond[C_STUN] || t.cond[C_PARAL]) && (stat == STR || stat == DEX)) return 1.0;
    double p = std::clamp((dc - saveBonus(t, stat) - 1) / 20.0, 0.0, 1.0);
    bool dis = stat == DEX && t.cond[C_RESTR] > 0;
    const bool sa = advSave(t);
    double q = (sa && !dis) ? p * p : ((dis && !sa) ? 1 - (1 - p) * (1 - p) : p);
    // ИИ знает, что у цели есть заряды легендарного сопротивления
    return t.lrLeft > 0 ? q * 0.25 : q;
}
