// Действия: атака оружием, способности, реакции, стены, превращение
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Действия
// ---------------------------------------------------------------------------
// команда в строю: жив и не сбежал хоть один её главный боец
inline bool teamIn(const World& w, int t) {
    for (int i = 0; i < w.nLead && i < (int)w.f.size(); i++)
        if (w.f[i].team == t && w.f[i].hp > 0 && !w.f[i].fled) return true;
    return false;
}
bool over(const World& w) {
    if (w.nLead == 2 && w.nTeams == 2)
        return w.f[0].hp <= 0 || w.f[1].hp <= 0 || w.f[0].fled || w.f[1].fled;   // fled — сбежал (вышел за арену или оторвался от погони): проиграл
    int in = 0;   // несколько команд (v21): бой идёт, пока в строю хотя бы две
    for (int t = 0; t < w.nTeams && in < 2; t++) in += teamIn(w, t);
    return in < 2;
}
int nearestEnemyInRange(const World& w, const Fighter& me, int range) {
    int best = -1, bd = 1 << 30;
    for (size_t i = 0; i < w.f.size(); i++) {
        const Fighter& e = w.f[i];
        if (e.hp <= 0 || e.team == me.team || !isKnown(w, me.team, (int)i)) continue;   // туман войны: только известные
        // очарованный не может атаковать очаровавшего
        if (me.cond[C_CHARM] > 0 && me.charmSrc == (int)i) continue;
        int d = distFt(me, e);
        if (d <= range && losF(w, me, e) && e.hp < bd) {
            bd = e.hp;
            best = (int)i;
        }
    }
    return best;
}
// Что сделал боец в этот ход — используется только для визуализатора (реплей).
struct ActInfo {
    std::string type = "none";
    int target = -1;
    int abil = -1;   // номер применённой способности (для просмотрщика)
    int gx = -1, gy = -1;   // клетка, по которой применена способность «по клетке»
};
static thread_local ActInfo g_act;
// события погони и побега для кадров просмотрщика (JSON-объекты); пишутся всегда, но читаются только при записи реплея
static thread_local std::vector<std::string> g_evPending;
void evPush(const std::string& json) {
    if (g_evPending.size() < 64) g_evPending.push_back(json);
}
// g_lastHp — доля ХП боссов в конце боя
thread_local bool g_rlTeam[2] = {false, false};
thread_local double g_lastHp[2] = {1, 1};

// ---- Концентрация и ячейки ----
// наименьшая ячейка >= L
int findSlot(const Fighter& f, int L) {
    for (int k = L; k <= 9; k++)
        if (f.slotLeft[k] > 0) return k;
    return 0;
}
void endStatBuff(Fighter& f) {
    if (f.sbAmt) f.b.stats[f.sbStat] -= f.sbAmt;
    f.sbAmt = 0;
    f.sbRounds = 0;
}
// Перенаправление: цель атаки меняется местами с соседним союзником, и атака достаётся ему (реакция)
Fighter& redirected(World& w, const Fighter& a, Fighter& d) {
    if (!d.b.redirect || !canReact(d) || a.hidden || !canSee(w, d, a)) return d;
    Fighter* best = nullptr;
    for (auto& g : w.f)
        if (&g != &d && g.hp > 0 && g.team == d.team && distFt(g, d) <= 5 && (!best || g.hp > best->hp)) best = &g;
    if (!best) return d;
    std::swap(best->x, d.x);
    std::swap(best->y, d.y);
    spendReaction(d);
    return *best;
}
// концентрация сорвана: снимаются состояния и урон по времени, наложенные ею, и баффы заклинателя
void breakConc(World& w, int idx) {
    Fighter& c = w.f[idx];
    if (c.concSpec < 0) return;
    for (auto& f : w.f) {
        for (int k = 1; k < C_N; k++)
            if (f.cond[k] > 0 && f.condConc[k] == idx + 1) f.cond[k] = 0;
        if (f.dotRounds > 0 && f.dotConc == idx + 1) f.dotRounds = 0;
    }
    const Spec& s = c.b.abil[c.concSpec];
    if (s.sumN > 0)   // призыв на концентрации: его приспешники исчезают
        for (auto& f : w.f)
            if (f.summoner == idx && f.sumSpec == c.concSpec && f.hp > 0) {
                f.hp = 0;
                f.vanished = true;
            }
    if (s.buffN) c.buffRounds = 0;
    if (s.sbAmt) endStatBuff(c);
    if (s.acB) c.acBuffRounds = 0;
    if (s.invisN) c.invisRounds = 0;
    c.concSpec = -1;
    c.concDCs.clear();
    g_concBroken++;
}
// ---- Способность — единая структура (переходник поверх Spec, оружия и встроенных манёвров): цена + эффект.
// Атака оружием — способность за действие; Рывок, Уклонение, Отход — встроенные способности. Категория выводится из
// эффекта и отдельно не хранится. Проверка и списание цены — один путь (canPay, payEcon, paySpec) для бота, сети и игрока.
enum Econ { EC_ACTION, EC_BONUS, EC_REACTION, EC_FREE, EC_ATTACK };   // EC_ATTACK — одна атака действия «Атака» (пакет атомов 1)
enum EffKind { EF_WEAPON, EF_SPEC, EF_DASH, EF_DODGE, EF_DISENGAGE, EF_SUMMON, EF_HELP, EF_READY, EF_N };
enum AbCat { CAT_ATTACK, CAT_SPELL, CAT_SUMMON, CAT_MANEUVER, CAT_N };
const char* CATN[CAT_N] = {"атака", "заклинание", "призыв", "манёвр"};
struct AbCost {
    int econ = EC_ACTION;
    // способность Boss::abil, чьи ресурсы тратятся (применения, ячейка, перезарядка, концентрация); −1 — таких нет
    int spec = -1, slotLvl = 0, uses = UNL, recharge = 0;
    bool conc = false;
};
struct Ability {
    int eff = EF_WEAPON;
    AbCost cost;
};
inline Ability builtinAbility(int eff, int econ = EC_ACTION) {
    Ability a;
    a.eff = eff;
    a.cost.econ = econ;
    return a;
}
inline Ability specAbility(const Boss& b, int k) {
    const Spec& s = b.abil[k];
    Ability a;
    a.eff = s.sumN > 0 ? EF_SUMMON : EF_SPEC;   // эффект определяет категорию
    a.cost.econ = s.react ? EC_REACTION : (s.jump && s.teleFt > 0 ? EC_FREE : (s.inAttack ? EC_ATTACK : (s.bonus ? EC_BONUS : EC_ACTION)));
    a.cost.spec = k;
    a.cost.slotLvl = s.slotLvl;
    a.cost.uses = s.uses;
    a.cost.recharge = s.recharge;
    a.cost.conc = s.conc;
    return a;
}
// Переходник: все способности бойца по его сборке — атака оружием (если вооружён), способности Boss::abil (и реакции),
// Рывок, Уклонение, Отход действием и бонусным действием (если умеет); призыв — способность Boss::abil с эффектом sumN
std::vector<Ability> abilitiesOf(const Boss& b) {
    std::vector<Ability> v;
    if (b.wN > 0) v.push_back(builtinAbility(EF_WEAPON));
    for (size_t k = 0; k < b.abil.size(); k++) v.push_back(specAbility(b, (int)k));
    v.push_back(builtinAbility(EF_DASH));
    v.push_back(builtinAbility(EF_DODGE));
    v.push_back(builtinAbility(EF_DISENGAGE));
    if (b.disengB) v.push_back(builtinAbility(EF_DISENGAGE, EC_BONUS));
    v.push_back(builtinAbility(EF_HELP));    // Помощь и Подготовка — у всех, как Рывок (v21)
    if (b.wN > 0) v.push_back(builtinAbility(EF_READY));
    return v;
}
// категория способности сборки (решение владельца): призыв — «призыв»; по врагу без ячейки и концентрации — «атака»;
// с ячейкой, концентрацией или на себя — «заклинание»
inline int specCategory(const Spec& s) {
    if (s.sumN > 0) return CAT_SUMMON;
    return (s.self || s.slotLvl > 0 || s.conc) ? CAT_SPELL : CAT_ATTACK;
}
inline int abCategory(const Boss& b, const Ability& a) {
    switch (a.eff) {
        case EF_WEAPON: return CAT_ATTACK;
        case EF_SPEC:
        case EF_SUMMON: return specCategory(b.abil[a.cost.spec]);
        default: return CAT_MANEUVER;
    }
}
// Хватает ли ресурсов самой способности (применения, концентрация, ячейка); экономику действия проверяет canPay
bool resReady(const Fighter& me, const AbCost& c, const char** why = nullptr) {
    auto no = [&](const char* m) {
        if (why) *why = m;
        return false;
    };
    if (c.spec < 0) return true;
    if (me.inForm) return no("В облике способности недоступны.");
    if (me.uses[c.spec] <= 0) return no("Способность использована.");
    if (c.conc && me.concSpec >= 0) return no("Вы уже держите концентрацию.");
    if (c.slotLvl > 0 && !findSlot(me, c.slotLvl)) return no("Нет подходящей ячейки заклинания.");
    const Spec& s = me.b.abil[c.spec];
    if (s.pool && me.poolLeft <= 0) return no("Кости запаса кончились.");
    if ((s.onHit || s.offhand) && (me.b.wN <= 0 || me.disarmed)) return no("Нужно оружие в руке.");
    if (s.offhand && !me.wAtkTurn) return no("Атака второй рукой — после атаки оружием в этот ход.");
    if (s.noRepeat && c.spec < (int)me.usedRound.size() && me.usedRound[c.spec] >= 0 && me.usedRound[c.spec] >= g_curRound - 1)
        return no("Не два раунда подряд: эта способность была в прошлом раунде.");
    return true;
}
inline bool specResReady(const Fighter& me, int k) {
    return resReady(me, specAbility(me.b, k).cost);
}
// Плата за способность k (один путь списания): применение, наименьшая подходящая ячейка, концентрация. takeConc = false —
// эффект без длительности (прыжок): концентрацию не занимает
void paySpec(World& w, Fighter& me, int k, bool takeConc) {
    const Spec& s = me.b.abil[k];
    if (k < (int)me.usedRound.size()) me.usedRound[k] = g_curRound;   // «не два раунда подряд»
    {   // используемость (№6): каждое применение способности отмечает её атомы
        const int i = (int)(&me - &w.f[0]);
        if (i >= 0 && i < 2) {
            AMask m = 0;
            markSpec(s, m);
            for (int a = 0; a < AT_N; a++)
                if ((m >> a) & ONE) g_castAtom[i][a]++;
        }
    }
    me.uses[k]--;
    if (s.slotLvl > 0) {
        const int L = findSlot(me, s.slotLvl);
        if (L) me.slotLeft[L]--;
    }
    if (takeConc && s.conc) {
        const int myIdx = (int)(&me - &w.f[0]);
        if (me.concSpec >= 0) breakConc(w, myIdx);
        me.concSpec = k;
        me.concDCs.clear();
        g_concCast++;
    }
}
// проверка концентрации: на каждое полученное попадание спасбросок ТЕЛ против своей СЛ max(10, урон/2)
void resolveConc(World& w) {
    for (size_t i = 0; i < w.f.size(); i++) {
        Fighter& f = w.f[i];
        if (f.concSpec < 0) continue;
        if (f.hp <= 0) {
            breakConc(w, (int)i);
            continue;
        }
        std::vector<int> dcs;
        dcs.swap(f.concDCs);
        for (int dc : dcs)
            if (d20(false, false) + saveBonus(f, CON) < dc) {
                breakConc(w, (int)i);
                break;
            }
    }
}
void applyRider(World& w, Fighter& me, Fighter& t, bool crit) {
    if (me.b.rN > 0)
        hurt(t, rollDice(me.b.rN * (crit ? 2 : 1), me.b.rS), me.b.rType, !isPhys(me.b.rType) || me.b.wMagic);
    if (me.b.rCond > 0 && t.hp > 0) {
        int st = condSaveStat(me.b.rCond), dc = me.dc();
        Spec sp;
        sp.cond = me.b.rCond;
        sp.condRounds = me.b.rRounds;
        // захват — без спасброска (СЛ побега)
        bool ok = me.b.rCond == C_GRAPPLE ? false : saveLR(t, st, dc, lrWorth(t, sp));
        if (!ok && applyCond(t, me.b.rCond, me.b.rRounds, st, dc, true, (int)(&me - &w.f[0])) && me.b.rCond == C_GRAPPLE)
            grabExtras(t, (int)(&me - &w.f[0]), me.b.rGrabRestr, me.b.rGrabDotN, me.b.rGrabDotS, me.b.wType);
    }
}
// nAtt<0 — полное действие Атака
// досягаемость цели t: дальше неё t не ответит атакой оружием (безоружный не отвечает вовсе)
inline int threatDist(const Fighter& t) {
    return t.b.wN > 0 ? atkDist(t.b) : 0;
}
inline void useAttack(const World& w, const Fighter& me, int tIdx, bool weapon) {
    const int i = (int)(&me - &w.f[0]);
    if (i < 0 || i > 1 || tIdx < 0 || tIdx >= (int)w.f.size()) return;
    g_use[i][weapon ? U_WATK : U_SATK]++;
    if (distFt(me, w.f[tIdx]) > threatDist(w.f[tIdx])) g_use[i][weapon ? U_WOUT : U_SOUT]++;
}
void hitOne(World& w, const Spec& s, Fighter& me, Fighter& t0);
// приём «при попадании» (v21): плата (применение, кость запаса) — только при попадании; эффекты способности ложатся на
// попавшую цель (без своего броска атаки; со спасброском, если доставка 1), кость запаса — к урону; урон себе; лечение
// себе, если цель умерла от удара; обезоруживание
void applyManeuver(World& w, Fighter& me, int si, Fighter& t, bool crit) {
    const Spec s = me.b.abil[si];
    paySpec(w, me, si, true);
    g_act.abil = si;
    if (s.pool) me.poolLeft--;
    Spec r = s;
    r.onHit = false;
    r.delivery = s.delivery == 1 ? 1 : 2;   // уже попал: без своего броска
    r.radius = 0;
    if (s.pool) r.more.push_back({1, s.poolS, s.dmgN > 0 ? s.dtype : me.b.wType});
    if (crit) {   // крит удваивает кости приёма, как и кости оружия
        r.dmgN *= 2;
        for (auto& d : r.more) d.n *= 2;
    }
    if (r.dmgN > 0 || !r.more.empty() || r.cond || r.pushFt || r.dotN) hitOne(w, r, me, t);
    if (s.disarm && t.hp > 0 && t.b.wN > 0 && !t.b.wNatural && (s.delivery != 1 || !saveLR(t, STR, me.dc() + s.dcB, false))) t.disarmed = true;
    if (s.selfN > 0) hurt(me, rollDice(s.selfN, s.selfS), s.dtype, true);
    if ((s.killN > 0 || s.killB > 0) && t.hp <= 0 && me.hp > 0) me.hp = std::min(me.maxHp, me.hp + rollDice(s.killN, s.killS) + s.killB);
}
// man — приём «при попадании» (индекс способности, −1 — нет); offhand — атака второй рукой (одна, модификатор к урону
// только со стилем или если он отрицательный)
void doWeaponMain(World& w, Fighter& me, int tIdx, int nAtt, int man, bool offhand, bool twfMod);
// удары оружием мультиатаки (запасное с флагом multi): по той же цели, если достают, иначе по ближайшей в досягаемости
void altMultiAttacks(World& w, Fighter& me, int tIdx) {
    for (size_t k = 0; k < me.b.alt.size(); k++) {
        if (!me.b.alt[k].multi) continue;
        if (me.hp <= 0 || me.disarmed || over(w)) return;
        swapWeapon(me.b, k);
        int t = tIdx;
        if (t < 0 || t >= (int)w.f.size() || w.f[t].hp <= 0 || distFt(me, w.f[t]) > atkDist(me.b)) t = nearestEnemyInRange(w, me, atkDist(me.b));
        if (t >= 0) doWeaponMain(w, me, t, me.b.wAtt, -1, false, false);
        swapWeapon(me.b, k);
    }
}
// Атака оружием; nAtt == −1 — целое действие «Атака»: все удары основным оружием и (пакет атомов 1) оружием мультиатаки
void doWeapon(World& w, Fighter& me, int tIdx, int nAtt, int man, bool offhand, bool twfMod) {
    doWeaponMain(w, me, tIdx, nAtt, man, offhand, twfMod);
    if (nAtt == -1 && man < 0 && !offhand && !me.b.alt.empty()) altMultiAttacks(w, me, tIdx);
}
void doWeaponMain(World& w, Fighter& me, int tIdx, int nAtt, int man, bool offhand, bool twfMod) {
    if (me.b.wN <= 0 || me.disarmed) return;   // безоружный (или обезоруженный) атаки оружием не делает (v21)
    useAttack(w, me, tIdx, true);
    if (!offhand) me.wAtkTurn = true;
    g_act.type = wpAtkRange(me.b, distFt(me, w.f[tIdx])) > 5 ? "attack-ranged" : "attack-melee";
    g_act.target = tIdx;
    int total = nAtt < 0 ? me.b.wAtt : std::min(nAtt, me.b.wAtt);
    for (int i = 0; i < total; i++) {
        if (over(w)) return;
        if (w.f[tIdx].hp <= 0) {
            tIdx = nearestEnemyInRange(w, me, atkDist(me.b));
            if (tIdx < 0) return;
        }
        Fighter& t = redirected(w, me, w.f[tIdx]);
        bool crit;
        const int range = wpAtkRange(me.b, distFt(me, t));   // метательным вплотную бьют как ближним
        // размен (v21, Мастер большого оружия): −N к попаданию, +2N к урону — когда это выгоднее по ожиданию
        int power = 0;
        forRules(me, [&](const Rule& rl, int) {
            if (rl.what == E_POWER && ruleOn(w, me, &t, rl, true, range <= 5)) power = std::max(power, rl.n);
        });
        if (power > 0) {
            const double avg = me.b.wN * (wDie(me.b) + 1) / 2.0 + me.wMod(), p1 = pHitEst(w, me, t, me.wAtk(), range), p2 = pHitEst(w, me, t, me.wAtk() - power, range);
            if (!(p2 * (avg + 2 * power) > p1 * avg)) power = 0;
        }
        g_heavyDis = me.b.wHeavy && me.b.size < 2;   // размеры (v21): маленьким тяжёлое оружие — с помехой
        bool hit = attackRoll(w, me, t, me.wAtk() - power, range, crit);
        g_heavyDis = false;
        g_attacksMade++;
        me.hidden = false;
        if (!hit) continue;
        int dmg = 2 * power + rollDice(me.b.wN * (crit ? 2 : 1), wDie(me.b)) + (offhand && !twfMod ? std::min(0, me.wMod()) : me.wMod());
        if (me.growRounds != 0) dmg += rollDice(crit ? 2 : 1, 4);   // увеличение (пакет атомов 2): +1к4 к урону оружия
        if (me.buffRounds > 0) dmg += rollDice(me.buffN * (crit ? 2 : 1), me.buffS);
        // «когда → что»: кубики к урону оружия (каждое попадание или раз в ход)
        int extra[DT_N] = {};   // кубики правил своего типа урона — отдельным уроном (свои сопротивления цели)
        forRules(me, [&](const Rule& r, int k) {
            if (r.what != E_DMG || (r.once && k >= 0 && ((me.onceUsed >> k) & 1u)) || !ruleOn(w, me, &t, r, true, range <= 5)) return;
            const int add = rollDice(r.n * (crit ? 2 : 1), r.s);
            if (r.dt >= 0 && r.dt != me.b.wType) extra[r.dt] += add; else dmg += add;
            if (r.once && k >= 0) me.onceUsed |= 1u << k;
        });
        const int hpBefore = t.hp;
        hurt(t, applyUncanny(t, std::max(dmg, 1)), me.b.wType, wPierceNM(me.b));
        for (int d = 0; d < DT_N; d++)
            if (extra[d] > 0 && t.hp > 0) hurt(t, extra[d], d, true);
        if (crit) fireTrig(w, me, T_CRIT, &t);                      // «событие → эффект» (v21)
        if (hpBefore > 0 && t.hp <= 0) fireTrig(w, me, T_KILL, &t);
        if (range <= 5) {   // Подвижный (v21): после моей атаки ближнего боя цель не провоцирует от меня до конца хода
            bool noOA = false;
            forRules(me, [&](const Rule& rl, int) {
                if (rl.what == E_NOOA && selfOn(me, rl)) noOA = true;
            });
            if (noOA) t.noOAvs = (int)(&me - &w.f[0]);
        }
        if (man >= 0 && me.hp > 0) {   // приём — на первое попадание этого действия
            applyManeuver(w, me, man, t, crit);
            man = -1;
        }
        if (!me.riderUsed && (me.b.rN > 0 || me.b.rCond > 0)) {
            me.riderUsed = true;
            applyRider(w, me, t, crit);
        }
        if (range <= 5) riposte(w, t, me);
    }
}
// реакция «когда атакуют союзника рядом» (v21): свой рядом с целью тратит реакцию — помеха этой атаке (как стиль
// «Защита») или удар оружием по атакующему (как «Страж»). 1 — помеха, 0 — нет, −1 — атакующий пал
int allyReact(World& w, Fighter& a, Fighter& d) {
    bool protDis = false;
    for (size_t gi = 0; gi < w.f.size() && !protDis; gi++) {
        Fighter& g = w.f[gi];
        if (&g == &d || &g == &a || g.hp <= 0 || g.team != d.team || !canReact(g)) continue;
        for (size_t k = 0; k < g.b.abil.size(); k++) {
            const Spec& s = g.b.abil[k];
            if (s.react != 5 || !specResReady(g, (int)k) || distFt(g, d) > std::max(5, s.range)) continue;
            if (s.strike && (g.b.wN <= 0 || g.disarmed || distFt(g, a) > atkDist(g.b))) continue;
            spendReaction(g);
            paySpec(w, g, (int)k, false);
            evPush("{\"k\":\"react\",\"who\":" + std::to_string(gi) + ",\"abil\":" + std::to_string(k) + ",\"trig\":5}");
            g_reactUsed++;
            if (s.protect) protDis = true;
            if (s.strike) {
                const ActInfo saved = g_act;
                doWeapon(w, g, (int)(&a - &w.f[0]), 1);
                g_act = saved;
                if (a.hp <= 0) return -1;
            }
            break;
        }
    }
    return protDis ? 1 : 0;
}
// «событие → эффект» (v21): событие on у бойца me, tgt — цель события (кого критнул, убил, кто провалил спас). Атака
// бонусным действием и добавка скорости ждут хода (resolvePending: там есть бюджет хода)
void fireTrig(World& w, Fighter& me, int on, Fighter* tgt) {
    if (me.b.trigs.empty() || me.trigLeft.size() != me.b.trigs.size() || me.hp <= 0) return;
    for (size_t k = 0; k < me.b.trigs.size(); k++) {
        const Trig& t = me.b.trigs[k];
        if (t.on != on || me.trigLeft[k] <= 0 || t.what == X_INITB || t.what == X_STAY1) continue;
        if (on == T_DMGTYPE && t.arg != g_trigType) continue;
        if ((t.what == X_DMG || t.what == X_COND) && (!tgt || tgt->hp <= 0)) continue;
        if (t.uses < UNL) me.trigLeft[k]--;
        switch (t.what) {
            case X_BONUSATK:
                me.pendBonus = true;
                me.pendTgt = tgt ? (int)(tgt - &w.f[0]) : -1;
                break;
            case X_HEAL: me.hp = std::min(me.maxHp, me.hp + rollDice(t.n, t.s) + t.b); break;
            case X_TEMP: me.temp = std::max(me.temp, rollDice(t.n, t.s) + t.b); break;
            case X_DMG: hurt(*tgt, rollDice(t.n, t.s) + t.b, t.dt >= 0 ? t.dt : (me.b.wN > 0 ? me.b.wType : DT_FORCE), t.dt >= 0 || me.b.wMagic); break;
            case X_COND: if (t.cond > 0) applyCond(*tgt, t.cond, std::max(1, t.n), condSaveStat(t.cond), me.dc(), true, (int)(&me - &w.f[0])); break;
            case X_ADVNEXT: me.advNext = true; break;
            case X_SPEED: me.pendSpeed += 5 * t.n; break;
            case X_SELFCOND: if (t.cond > 0) applyCond(me, t.cond, std::max(1, t.n), -1, 0, false); break;   // без спасброска
        }
    }
}
// притягивание (v21): цель сдвигается к источнику по шагу, пока не встанет вплотную; стены, ущелья и существа останавливают
// опасная зона под f: урон не чаще раза за ход (номер хода — g_turnSerial)
thread_local int g_turnSerial = 0;
void hazardHit(World& w, Fighter& f) {
    int k = 0;   // зона хоть под одной клеткой квадрата
    for (int dy = 0; dy < fpN(f) && !k; dy++)
        for (int dx = 0; dx < fpN(f) && !k; dx++)
            if (f.x + dx < W && f.y + dy < W) k = w.haz[f.y + dy][f.x + dx];
    // летун над землёй не касается жидкостей и льда, а с 20 фт и выше — никаких зон
    if (!k || f.hp <= 0 || ((f.b.fly || f.up) && HAZ[k].flySafe) || f.alt >= 2 || f.hazTurn == g_turnSerial) return;
    f.hazTurn = g_turnSerial;
    int dmg = rollDice(w.hazN, 10);
    const bool ok = saveOK(f, HAZ[k].save, w.hazDC);
    if (ok) dmg /= 2;
    const int src0 = g_srcIdx, hp0 = f.hp;
    g_srcIdx = -1;
    hurt(f, dmg, HAZ[k].dtype, false);
    g_srcIdx = src0;
    evPush("{\"k\":\"haz\",\"who\":" + std::to_string((int)(&f - &w.f[0])) + ",\"kind\":" + std::to_string(k) + ",\"dmg\":" + std::to_string(hp0 - f.hp) + ",\"save\":" + (ok ? "1" : "0") + "}");
}
void pullToward(World& w, Fighter& src, Fighter& t, int ft) {
    if (t.b.immPush || t.b.size > src.b.size + 1) return;
    for (int i = 0; i < ft / 5; i++) {
        if (fpGap(t.x, fpN(t), src.x, fpN(src)) <= 1 && fpGap(t.y, fpN(t), src.y, fpN(src)) <= 1) break;
        const int nx = t.x + ((src.x > t.x) - (src.x < t.x)), ny = t.y + ((src.y > t.y) - (src.y < t.y));
        if (!fpAll(w, t, nx, ny, [&](int cx, int cy) { return !w.wall[cy][cx] && w.door[cy][cx] != 1 && !w.pit[cy][cx] && !w.tree[cy][cx]; }) || blockedAt(w, t, nx, ny)) break;
        if (!stepOK(w, t, t.x, t.y, t.up, nx, ny, t.up)) break;   // по своему ярусу, без обрывов
        t.x = nx;
        t.y = ny;
    }
    hazardHit(w, t);
}
void pushAway(World& w, Fighter& src, Fighter& t, int ft, int collN = 0) {
    if (t.b.immPush || t.b.size > src.b.size + 1) return;   // размеры (v21): толкнуть — не больше чем на ступень крупнее
    int steps = ft / 5;
    int sx = (t.x > src.x) - (t.x < src.x), sy = (t.y > src.y) - (t.y < src.y);
    const int h0 = zF(w, t);
    for (int i = 0; i < steps; i++) {
        int nx = t.x + sx, ny = t.y + sy;
        bool out = nx < 0 || ny < 0 || nx >= W || ny >= W;
        if (!out && t.up && !w.wall[ny][nx] && !w.deck[ny][nx] && !blockedAt(w, t, nx, ny)) {   // столкнули с настила: падает вниз
            t.x = nx;
            t.y = ny;
            t.up = false;
            break;
        }
        if (!out && t.up) {   // по настилу: дальше только по настилу
            if (w.wall[ny][nx] || blockedAt(w, t, nx, ny) || std::abs(w.deck[ny][nx] - w.deck[t.y][t.x]) > 1) break;
            t.x = nx;
            t.y = ny;
            continue;
        }
        // упёрся (стена, ущелье, уступ вверх, существо): урон при столкновении получает толкаемый и, если это существо, преграда
        out = out || !fpAll(w, t, nx, ny, [](int, int) { return true; });   // квадрат выходит за арену
        if (out || !fpAll(w, t, nx, ny, [&](int cx, int cy) { return !w.wall[cy][cx] && w.door[cy][cx] != 1 && !w.pit[cy][cx] && !w.tree[cy][cx] && w.height[cy][cx] <= w.height[t.y][t.x]; }) ||
            blockedAt(w, t, nx, ny)) {
            if (collN > 0) {
                hurt(t, rollDice(collN, 6), DT_FORCE, true);
                if (!out)
                    if (Fighter* g = blockerAt(w, t, nx, ny)) hurt(*g, rollDice(collN, 6), DT_FORCE, true);
            }
            break;
        }
        t.x = nx;
        t.y = ny;
    }
    // сброс с возвышенности или настила: падение, по 1к6 за уровень (10 фт) и сбит с ног
    const int drop = h0 - zF(w, t);
    if (drop > 0 && t.hp > 0) {
        hurt(t, rollDice(drop, 6), DT_BLUDGEONING, false);
        if (t.hp > 0) applyCond(t, C_PRONE, 1, -1, 0, false);
    }
    hazardHit(w, t);
}
// цель внутри чужой (или своей) сферы неуязвимости, а заклинатель снаружи: способность уровня не выше сферы не проходит
bool globeBlocks(const World& w, const Spec& s, const Fighter& me, const Fighter& t) {
    const int lvl = s.slotLvl > 0 ? s.slotLvl : 1;   // способности без ячейки — как 1-й уровень
    for (auto& g : w.f)
        if (g.hp > 0 && g.globeRounds > 0 && lvl <= g.globeLvl && distFt(g, t) <= g.globeR && distFt(g, me) > g.globeR) return true;
    return false;
}
void hitOne(World& w, const Spec& s, Fighter& me, Fighter& t0) {
    if (t0.banished > 0) return;                    // изгнанного нет на поле
    if (globeBlocks(w, s, me, t0)) return;          // сфера неуязвимости (пакет атомов 2)
    if (s.healAlly && t0.team == me.team) {         // способность лечит союзников вместо урона (пакет атомов 2)
        int h = 0;
        for (auto& part : s.dmgParts()) h += rollDice(part.n, part.s);
        if (&t0 != &me || s.radius > 0) t0.hp = std::min(t0.maxHp, t0.hp + h);
        return;
    }
    if (me.team != t0.team) me.struckFoe = true;
    Fighter& t = (s.delivery == 0 && s.radius == 0) ? redirected(w, me, t0) : t0;
    int dc = me.dc() + s.dcB;
    bool crit = false;
    bool applyC = false, applyD = false, saveFailed = false;
    // урон по компонентам (№10): каждый тип бросается и проходит защиты цели отдельно
    const auto parts = s.dmgParts();
    std::vector<int> dmg(parts.size(), 0);
    auto roll = [&](bool twice) {
        for (size_t k = 0; k < parts.size(); k++) dmg[k] = rollDice(parts[k].n * (twice ? 2 : 1), parts[k].s);
    };
    if (s.delivery == 0) {
        if (!attackRoll(w, me, t, me.sAtk(), s.range, crit)) return;
        roll(crit);
        // Невероятное уклонение: одна реакция на всю атаку, решение по сумме, вдвое меньше каждый компонент
        int tot = 0;
        for (int x : dmg) tot += x;
        if (!parts.empty() && applyUncanny(t, tot) < tot)
            for (int& x : dmg) x /= 2;
        // урон по времени при попадании — без спасброска
        applyD = s.dotN > 0;
        // захват при попадании — без спасброска
        g_saveVsAbil = true;
        applyC = s.cond && (s.cond == C_GRAPPLE || !saveLR(t, s.saveStat, dc, lrWorth(t, s)));
        g_saveVsAbil = false;
    } else if (s.delivery == 1) {
        // невосприимчив после успеха (пакет атомов 1): эта способность этого заклинателя его больше не берёт
        const int immKey = (int)(&me - &w.f[0]) * 64 + g_curAbil;
        if (s.immSave && std::find(t.immAbil.begin(), t.immAbil.end(), immKey) != t.immAbil.end()) return;
        g_saveVsAbil = true;
        bool ok = saveLR(t, s.saveStat, dc, lrWorth(t, s));
        g_saveVsAbil = false;
        if (ok && s.immSave) t.immAbil.push_back(immKey);
        roll(false);
        const bool evade = t.b.evasion && s.saveStat == DEX && s.radius > 0 && !t.cond[C_STUN] && !t.cond[C_PARAL];   // Увёртливость (PHB)
        if (ok || evade)
            for (int& x : dmg) x = ok && evade ? 0 : x / 2;
        applyC = s.cond && !ok;
        applyD = s.dotN > 0 && !ok;
        saveFailed = !ok;
    } else {
        roll(false);
        applyC = s.cond > 0;
        applyD = s.dotN > 0;
    }
    // сопротивление урону от способностей (пакет атомов 1): вдвое меньше; с резистом к типу не складывается
    if (t.b.spellRes)
        for (size_t k = 0; k < parts.size(); k++)
            if (!((t.b.resMask >> parts[k].type) & 1)) dmg[k] /= 2;
    // один источник урона — одна проверка концентрации на сумму полученного (PHB)
    int taken = 0;
    const int hpBefore = t.hp;
    for (size_t k = 0; k < parts.size(); k++) taken += hurt(t, dmg[k], parts[k].type, specMagical(me.b, parts[k].type), false);
    // прочее (v21): урон снижает максимум хитов; вампиризм — половина нанесённого себе; притягивание; рассеивание
    if (s.maxHpDmg && taken > 0) {
        t.maxHp = std::max(1, t.maxHp - taken);
        t.hp = std::min(t.hp, t.maxHp);
    }
    if (s.drain && taken > 0 && me.hp > 0) me.hp = std::min(me.maxHp, me.hp + taken / 2);
    const bool landed = s.delivery == 2 || (s.delivery == 1 ? saveFailed : true);
    if (s.pullFt > 0 && t.hp > 0 && landed) pullToward(w, me, t, s.pullFt);
    if (s.dispel && t.hp > 0 && landed) {
        t.buffRounds = t.acBuffRounds = t.invisRounds = 0;
        if (t.sbRounds > 0) {
            t.sbRounds = 0;
            endStatBuff(t);
        }
        if (t.concSpec >= 0) breakConc(w, (int)(&t - &w.f[0]));
    }
    if (me.team != t.team) {   // «событие → эффект» (v21): враг провалил спасбросок от моей способности; убил
        if (saveFailed) fireTrig(w, me, T_SAVEFAIL, &t);
        if (hpBefore > 0 && t.hp <= 0) fireTrig(w, me, T_KILL, &t);
    }
    if (taken > 0 && t.concSpec >= 0) t.concDCs.push_back(std::max(10, taken / 2));
    if (applyC && t.hp > 0 &&
        applyCond(t, s.cond, s.condRounds, s.saveStat, dc, s.delivery != 2 && !s.noRep, (int)(&me - &w.f[0]),
                  s.conc ? (int)(&me - &w.f[0]) + 1 : 0) && s.cond == C_GRAPPLE)
        grabExtras(t, (int)(&me - &w.f[0]), s.grabRestr, s.grabDot ? s.dotN : 0, s.dotS, s.dtype);
    if (s.grabDot && s.cond == C_GRAPPLE) applyD = false;   // урон по времени — пока держит (grabExtras), а не на N раундов
    // изгнание (пакет атомов 2): цель исчезает с поля на N раундов (атака — при попадании, спасбросок — при провале)
    if (s.banishN > 0 && t.hp > 0 && (s.delivery != 1 || saveFailed)) {
        t.banished = std::max(t.banished, s.banishN);
        t.banishSrc = (int)(&me - &w.f[0]);
    }
    // один урон по времени на цель: заменяется, если новый не слабее
    if (applyD && t.hp > 0) {
        double nw = s.dotN * (s.dotS + 1) / 2.0 * s.dotRounds, old = t.dotN * (t.dotS + 1) / 2.0 * t.dotRounds;
        if (nw >= old) {
            t.dotN = s.dotN;
            t.dotS = s.dotS;
            t.dotType = s.dtype;
            t.dotRounds = s.dotRounds;
            t.dotConc = s.conc ? (int)(&me - &w.f[0]) + 1 : 0;
        }
    }
    if (s.pushFt && t.hp > 0) pushAway(w, me, t, s.pushFt, s.collN);
    if (s.delivery == 0 && s.range <= 5) riposte(w, t, me);
}
// Запись изменения арены (для просмотрщика) и временная стена: линия из n клеток поперёк направления от колдующего к клетке (cx, cy), по центру на ней.
// Клетки с существами, ущелья и уже стены пропускаются. Стена непроходима, закрывает обзор; через rounds раундов исчезает.
void dynAdd(World& w, int x, int y, int kind, int rounds) { w.dyn.push_back({x, y, kind, rounds}); }
// дверь: состояние в мире и в изменениях арены (6 открыта, 7 закрыта — последняя запись по клетке)
void setDoor(World& w, int x, int y, int st) {
    if (!w.door[y][x] || w.door[y][x] == st) return;
    w.door[y][x] = (unsigned char)st;
    for (size_t i = 0; i < w.dyn.size();)
        if (w.dyn[i][0] == x && w.dyn[i][1] == y && (w.dyn[i][2] == 6 || w.dyn[i][2] == 7)) w.dyn.erase(w.dyn.begin() + i);
        else i++;
    dynAdd(w, x, y, st == 2 ? 6 : 7, -1);
    evPush(std::string("{\"k\":\"door\",\"x\":") + std::to_string(x) + ",\"y\":" + std::to_string(y) + ",\"open\":" + (st == 2 ? "1" : "0") + "}");
}
int placeWall(World& w, const Fighter& me, int cx, int cy, int n, int rounds) {
    const int dx = cx - me.x, dy = cy - me.y, ax = std::abs(dx), ay = std::abs(dy), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    int px, py;
    if (ax == 0 && ay == 0) { px = 1; py = 0; }
    else if (ax >= 2 * ay) { px = 0; py = 1; }
    else if (ay >= 2 * ax) { px = 1; py = 0; }
    else { px = -sy; py = sx; }
    int placed = 0;
    for (int i = -(n - 1) / 2; i <= n / 2; i++) {
        const int x = cx + px * i, y = cy + py * i;
        if (x < 0 || y < 0 || x >= w.aw || y >= w.ah || w.wall[y][x] || w.pit[y][x] || occupied(w, x, y, nullptr)) continue;
        w.wall[y][x] = true;
        w.mapVer = ++g_mapSerial;
        dynAdd(w, x, y, 1, rounds + 1);
        placed++;
    }
    return placed;
}
// в конце раунда: временные стены тают
void expireDyn(World& w) {
    for (size_t i = 0; i < w.dyn.size();) {
        auto& d = w.dyn[i];
        if (d[3] > 0 && --d[3] == 0) {
            if (d[2] == 1) w.wall[d[1]][d[0]] = false, w.mapVer = ++g_mapSerial;
            w.dyn.erase(w.dyn.begin() + i);
        } else
            i++;
    }
}
// урон объекту в клетке (x, y): хиты кончились — объект рушится, остаются обломки (труднопроходимо). Яд и психический — мимо
bool hitObject(World& w, int x, int y, int dmg, int dtype) {
    if (!isObject(w, x, y) || w.ohp[y][x] <= 0 || dmg <= 0 || dtype == DT_POISON || dtype == DT_PSYCHIC) return false;
    w.ohp[y][x] = (short)std::max(0, w.ohp[y][x] - dmg);
    if (w.ohp[y][x] > 0) return false;
    if (w.door[y][x]) {   // выбитая дверь: пустой проём
        w.door[y][x] = 0;
        dynAdd(w, x, y, 8, -1);
        evPush("{\"k\":\"break\",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + "}");
        return true;
    }
    w.wall[y][x] = w.tree[y][x] = w.climb[y][x] = false;
    w.mapVer = ++g_mapSerial;
    w.diff[y][x] = true, w.mapVer = ++g_mapSerial;
    dynAdd(w, x, y, 4, -1);
    evPush("{\"k\":\"break\",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + "}");
    return true;
}
// настил (мост, балкон, потолок) рушится: стоявшие на нём падают (1к6 за уровень, сбиты с ног), стоявших под ним бьёт
// обвал (2к10 дробящего, спасбросок ЛОВ СЛ 15 — половина); внизу обломки
bool hitDeck(World& w, int x, int y, int dmg, int dtype) {
    if (x < 0 || y < 0 || x >= w.aw || y >= w.ah || !w.deck[y][x] || w.dhp[y][x] <= 0 || dmg <= 0 || dtype == DT_POISON || dtype == DT_PSYCHIC) return false;
    w.dhp[y][x] = (short)std::max(0, w.dhp[y][x] - dmg);
    if (w.dhp[y][x] > 0) return false;
    const int top = w.deck[y][x];
    w.deck[y][x] = 0;
    w.mapVer = ++g_mapSerial;
    dynAdd(w, x, y, 5, -1);
    evPush("{\"k\":\"collapse\",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + "}");
    for (auto& f : w.f) {
        if (f.hp <= 0 || f.x != x || f.y != y) continue;
        if (f.up) {
            f.up = false;
            const int drop = top - zAt(w, x, y);
            if (drop > 0) hurt(f, rollDice(drop, 6), DT_BLUDGEONING, false);
            if (f.hp > 0) applyCond(f, C_PRONE, 1, -1, 0, false);
        } else {
            int d = rollDice(2, 10);
            if (saveOK(f, DEX, 15)) d /= 2;
            hurt(f, d, DT_BLUDGEONING, false);
        }
    }
    if (!w.wall[y][x] && !w.pit[y][x]) {
        w.diff[y][x] = true, w.mapVer = ++g_mapSerial;
        dynAdd(w, x, y, 4, -1);
    }
    return true;
}
// область способности задевает объекты: урон кубиками способности (объект не спасается), по разу на клетку
void areaHitsObjects(World& w, const Spec& s, const Fighter& me, int cx, int cy) {
    if (s.dmgN <= 0 && s.more.empty()) return;
    const int rr = s.radius / 5 + 1;
    const bool fromMe = s.shape == SH_CONE || s.shape == SH_LINE;
    const int bx = fromMe ? me.x : cx, by = fromMe ? me.y : cy;
    for (int y = std::max(0, by - rr); y <= std::min(w.ah - 1, by + rr); y++)
        for (int x = std::max(0, bx - rr); x <= std::min(w.aw - 1, bx + rr); x++) {
            if (!inShape(s.shape, s.radius, me.x, me.y, cx, cy, x, y)) continue;
            if (w.deck[y][x] && w.dhp[y][x] > 0) hitDeck(w, x, y, rollDice(std::max(1, s.dmgN), s.dmgS), s.dmgN > 0 ? s.dtype : (s.more.empty() ? DT_FORCE : s.more[0].type));
            if (!isObject(w, x, y) || w.ohp[y][x] <= 0) continue;
            if (s.dmgN > 0 && hitObject(w, x, y, rollDice(s.dmgN, s.dmgS), s.dtype)) continue;
            for (auto& m : s.more)
                if (hitObject(w, x, y, rollDice(m.n, m.s), m.type)) break;
        }
}
void summonFrom(World& w, int idx, int si);
// Превращение (PHB, Дикий облик): тело — шаблон облика (размер, СИЛ/ЛОВ/ТЕЛ, КД, оружие, скорость, передвижение, защиты,
// хиты — свой запас); ИНТ, МДР, ХАР, мастерство, способности (в облике недоступны), правила, легендарное, логово — свои.
// Хиты облика кончились — возврат в свой облик, лишний урон переходит на свои хиты
void shapeInto(World& w, Fighter& f, int t) {
    if (f.inForm || t < 0 || t >= (int)f.b.tmpl.size()) return;
    auto orig = std::make_shared<Boss>(f.b);
    Boss nb = f.b.tmpl[t];
    for (int st : {INT, WIS, CHA}) nb.stats[st] = orig->stats[st];
    nb.abil = orig->abil;
    nb.tmpl = orig->tmpl;
    nb.rules = orig->rules;
    nb.trigs = orig->trigs;
    nb.legAct = orig->legAct;
    nb.legRes = orig->legRes;
    nb.lair = orig->lair;
    nb.prof = std::max(nb.prof, orig->prof);
    for (int L = 0; L < 10; L++) nb.slots[L] = orig->slots[L];
    nb.mainStat = orig->mainStat;
    nb.cost = orig->cost;
    nb.alt.clear();
    f.formOrig = orig;
    f.b = nb;
    f.inForm = true;
    f.formHpSave = f.hp;
    f.formMaxSave = f.maxHp;
    f.hp = f.maxHp = std::max(1, nb.hp);
    f.alt = std::min(f.alt, f.b.fly ? ALT_MAX : 0);
    evPush("{\"k\":\"form\",\"who\":" + std::to_string((int)(&f - &w.f[0])) + ",\"on\":1}");
    nudge(w, f);   // облик крупнее — раздвинуться
    fallCheck(f);  // облик не летает — падает
}
// хиты облика кончились (или конец облика): свой облик, лишний урон — на свои хиты
void shapeBack(Fighter& f) {
    if (!f.inForm || !f.formOrig) return;
    const int excess = std::max(0, -f.hp);
    f.b = *f.formOrig;
    f.formOrig.reset();
    f.inForm = false;
    f.maxHp = f.formMaxSave;
    f.hp = f.formHpSave - excess;
    if (g_curW) evPush("{\"k\":\"form\",\"who\":" + std::to_string((int)(&f - &g_curW->f[0])) + ",\"on\":0}");
    fallCheck(f);
}
// союзники (v21): цель способности «на союзника» — я или свой в пределах дальности и видимости. pref — выбранный (игроком,
// ботом), если подходит; иначе: лечение — тому, кому больше не хватает хитов (по доле), временные хиты — у кого их нет и
// хитов меньше, баффы — тому, у кого этого баффа нет (себе — в первую очередь); приказ атаковать — союзнику, который
// достаёт врага оружием и может ответить реакцией. −1 — некому
int pickAlly(const World& w, const Fighter& me, const Spec& s, int pref = -1) {
    const int myIdx = (int)(&me - &w.f[0]);
    auto ok = [&](int i) {
        if (i < 0 || i >= (int)w.f.size()) return false;
        const Fighter& g = w.f[i];
        if (g.hp <= 0 || g.team != me.team || g.fled) return false;
        if (i != myIdx && (distFt(me, g) > s.range || !losF(w, me, g))) return false;
        if (s.cmdAtk) {
            if (i == myIdx || g.b.wN <= 0 || g.disarmed || !canReact(g)) return false;
            return nearestEnemyInRange(w, g, atkDist(g.b)) >= 0;
        }
        return true;
    };
    if (ok(pref)) return pref;
    int best = -1;
    double bv = -1e18;
    for (int i = 0; i < (int)w.f.size(); i++) {
        if (!ok(i)) continue;
        const Fighter& g = w.f[i];
        double v;
        if (s.cmdAtk)
            v = g.b.wN * (g.b.wS + 1) / 2.0 * g.b.wAtt;
        else if (s.healN || s.healB)
            v = 1.0 - (double)g.hp / std::max(1, g.maxHp);
        else if (s.tempN)
            v = (g.temp > 0 ? -1.0 : 0.0) + 1.0 - (double)g.hp / std::max(1, g.maxHp);
        else {
            const bool has = (s.buffN && g.buffRounds > 0) || (s.acB && g.acBuffRounds > 0) || (s.sbAmt && g.sbRounds > 0) || (s.invisN && g.invisRounds > 0);
            v = (has ? -10.0 : 0.0) + (i == myIdx ? 1.0 : 0.0) + g.maxHp / 1e6;
        }
        if (v > bv) {
            bv = v;
            best = i;
        }
    }
    return best;
}
void doSpec(World& w, Fighter& me, int si, int tIdx, int gx = -1, int gy = -1) {
    const Spec& s = me.b.abil[si];
    g_act.abil = si;
    g_curAbil = si;
    // приём «при попадании» — действие Атака оружием с приёмом (плата — при попадании); вторая рука — одна атака оружием
    if (s.onHit) {
        doWeapon(w, me, tIdx, -1, si);
        return;
    }
    if (s.offhand) {
        paySpec(w, me, si, false);
        doWeapon(w, me, tIdx, 1, -1, true, s.twfMod);
        return;
    }
    if (s.recharge) g_rechUsed++;
    paySpec(w, me, si, true);
    if (w.f.size() >= 2) revealTo(w, 1 - me.team, (int)(&me - &w.f[0]));   // способность выдаёт применившего (туман войны)
    {   // метрики использования (№6)
        const int i = (int)(&me - &w.f[0]);
        if (i >= 0 && i < 2) {
            if (s.hide) g_use[i][U_HIDE]++;
            if (s.invisN > 0) g_use[i][U_INVIS]++;
            if (s.darkR > 0) g_use[i][U_DARK]++;
        }
        if (!s.self && tIdx >= 0 && tIdx < (int)w.f.size() && w.f[tIdx].team != me.team) useAttack(w, me, tIdx, false);
    }
    if (s.formTmpl >= 0) {   // превращение
        shapeInto(w, me, s.formTmpl);
        g_act.type = "form";
        return;
    }
    if (s.sumN > 0) {   // призыв — эффект способности
        summonFrom(w, (int)(&me - &w.f[0]), si);
        g_act.type = "summon";
        return;
    }
    // реакция врагов «применил способность в зоне» (№23): до того, как способность сработает; павший не довёл её
    for (size_t i = 0; i < w.f.size(); i++)
        if (w.f[i].hp > 0 && w.f[i].team != me.team && hasReact(w.f[i].b)) tryReact(w, (int)i, 4, (int)(&me - &w.f[0]));
    // подготовка «враг применил способность» (8 ноября): подготовленное оружие или способность — по нему
    for (size_t i = 0; i < w.f.size() && me.hp > 0; i++) {
        Fighter& g = w.f[i];
        if (!g.readied || g.readyTrig != 2 || g.hp <= 0 || g.team == me.team || !canReact(g)) continue;
        const bool ab = g.readyAbil >= 0 && g.readyAbil < (int)g.b.abil.size();
        const int reach = ab ? std::max(5, specReach(g.b, g.b.abil[g.readyAbil])) : atkDist(g.b);
        if (distFt(g, me) > reach || !losF(w, g, me) || (!ab && (g.b.wN <= 0 || g.disarmed))) continue;
        spendReaction(g);
        g.readied = false;
        readyFire(w, (int)i, (int)(&me - &w.f[0]));
    }
    if (me.hp <= 0) return;
    if (g_countered) {   // контрзаклинание (v21): способность погашена, плата уже списана
        g_countered = false;
        g_act.type = "countered";
        return;
    }
    if (s.self && s.cmdAtk) {   // приказ: союзник бьёт оружием своей реакцией по ближайшему врагу в досягаемости
        const int gi = pickAlly(w, me, s, tIdx);
        g_act.type = "command";
        g_act.target = gi;
        if (gi >= 0) {
            Fighter& g = w.f[gi];
            const int e = nearestEnemyInRange(w, g, atkDist(g.b));
            if (e >= 0) {
                spendReaction(g);
                const ActInfo saved = g_act;
                doWeapon(w, g, e, 1);
                g_act = saved;
            }
        }
        return;
    }
    if (s.self) {
        // союзники (v21): эффекты «на себя» у способности «на союзника» — тому, кого выбрали (или себе)
        const int ti = s.ally ? pickAlly(w, me, s, tIdx) : -1;
        Fighter& tg = ti >= 0 ? w.f[ti] : me;
        if (s.healN || s.healB) {
            tg.hp = std::min(tg.maxHp, tg.hp + rollDice(s.healN, s.healS) + s.healB);
            g_act.type = "heal";
        }
        if (s.hide) {
            me.hidden = true;
            g_act.type = "hide";
        }
        if (s.buffN) {
            tg.buffN = s.buffN;
            tg.buffS = s.buffS;
            tg.buffRounds = s.buffRounds;
            g_act.type = "buff";
        }
        if (s.sbAmt) {
            endStatBuff(tg);
            tg.sbStat = s.sbStat;
            tg.sbAmt = s.sbAmt;
            tg.sbRounds = s.sbRounds;
            tg.b.stats[s.sbStat] += s.sbAmt;
            g_act.type = "statbuff";
        }
        // не лечат и не складываются
        if (s.tempN) {
            tg.temp = std::max(tg.temp, rollDice(s.tempN, 6));
            g_act.type = "temp";
        }
        if (s.acB) {
            tg.acBuff = s.acB;
            tg.acBuffRounds = s.acRounds;
            g_act.type = "acbuff";
        }
        if (s.darkR) {
            int rr = s.darkR / 5;
            for (int y = std::max(0, me.y - rr); y <= std::min(W - 1, me.y + rr); y++)
                for (int x = std::max(0, me.x - rr); x <= std::min(W - 1, me.x + rr); x++) {
                if (!w.dark[y][x]) dynAdd(w, x, y, 3, -1);
                w.dark[y][x] = true;
            }
            g_act.type = "darkness";
        }
        if (s.invisN) {
            tg.invisRounds = s.invisN;
            g_act.type = "invisible";
        }
        // пакет атомов 2: отражения, сфера неуязвимости, полёт на время, ходы подряд, увеличение, свет
        if (s.images > 0) tg.images = std::max(tg.images, s.images);
        if (s.globeLvl > 0) {
            tg.globeLvl = s.globeLvl;
            tg.globeR = std::max(10, s.radius);
            tg.globeRounds = std::max(1, s.globeRounds);
        }
        if (s.flyRounds > 0 && (!tg.b.fly || tg.flyBuff > 0)) {
            tg.b.fly = true;
            tg.flyBuff = std::max(tg.flyBuff, s.flyRounds);
        }
        if (s.extraTurns > 0) me.extraTurns += s.extraTurns;
        if (s.growRounds > 0 && tg.growRounds <= 0 && tg.b.size < 5) {
            tg.b.size++;
            if (g_curW && blockedAt(w, tg, tg.x, tg.y)) tg.b.size--;   // не помещается — только урон
            else tg.growRounds = -s.growRounds;   // минус: размер вырос (вернуть по окончании)
            if (tg.growRounds == 0) tg.growRounds = s.growRounds;
        } else if (s.growRounds > 0 && tg.growRounds == 0) tg.growRounds = s.growRounds;
        if (s.lightR > 0) {
            tg.lightR = s.lightR;
            tg.lightRounds = std::max(1, s.buffRounds > 0 ? s.buffRounds : 10);
        }
        g_act.target = ti >= 0 && ti != (int)(&me - &w.f[0]) ? ti : -1;
        return;
    }
    if (s.ground) {   // способность по клетке: область вокруг неё, труднопроходимая местность, барьер
        g_act.type = "ability-ground";
        g_act.target = -1;
        g_act.gx = gx;
        g_act.gy = gy;
        if (s.dmgN || !s.more.empty() || s.cond || s.pushFt || s.dotN)
            for (auto& e : w.f)
                if (e.hp > 0 && (e.team != me.team || !s.selective) && inShapeF(s.shape, s.radius, me.x, me.y, gx, gy, e)) hitOne(w, s, me, e);
        if (s.radius > 0) areaHitsObjects(w, s, me, gx, gy);
        if (s.terr) {
            const int rr = s.radius / 5 + 1;
            const bool fromMe = s.shape == SH_CONE || s.shape == SH_LINE;
            const int bx = fromMe ? me.x : gx, by = fromMe ? me.y : gy;
            for (int y = std::max(0, by - rr); y <= std::min(w.ah - 1, by + rr); y++)
                for (int x = std::max(0, bx - rr); x <= std::min(w.aw - 1, bx + rr); x++)
                    if (inShape(s.shape, s.radius, me.x, me.y, gx, gy, x, y) && !w.wall[y][x] && !w.diff[y][x]) { w.diff[y][x] = true, w.mapVer = ++g_mapSerial; dynAdd(w, x, y, 2, -1); }
        }
        if (s.wallLen > 0) placeWall(w, me, gx, gy, s.wallLen, s.wallRounds);
        me.hidden = false;
        return;
    }
    g_act.type = s.radius > 0 ? "ability-area" : (s.range > 5 ? "ability-ranged" : "ability-melee");
    g_act.target = tIdx;
    Fighter& c = w.f[tIdx];
    if (s.radius == 0) {
        hitOne(w, s, me, c);
        if (s.targets > 1) {   // ещё цели на выбор: лучшие враги в пределах дальности (бот и сеть выбирают главную)
            std::vector<std::pair<double, int>> more;
            for (size_t i = 0; i < w.f.size(); i++) {
                const Fighter& e = w.f[i];
                if ((int)i == tIdx || e.hp <= 0 || e.team == me.team || distFt(me, e) > s.range || !losF(w, me, e) || !isKnown(w, me.team, (int)i)) continue;
                more.push_back({-evSpecOne(w, s, me, e), (int)i});
            }
            std::sort(more.begin(), more.end());
            for (int k = 0; k < s.targets - 1 && k < (int)more.size() && me.hp > 0; k++) hitOne(w, s, me, w.f[more[k].second]);
        }
    } else {
        int cx = c.x, cy = c.y;
        for (auto& e : w.f)
            if (e.hp > 0 && (e.team != me.team || !s.selective) && inShapeF(s.shape, s.radius, me.x, me.y, cx, cy, e))
                hitOne(w, s, me, e);
        areaHitsObjects(w, s, me, cx, cy);
        if (s.terr) {
            const int rr = s.radius / 5 + 1;
            const bool fromMe = s.shape == SH_CONE || s.shape == SH_LINE;
            const int bx = fromMe ? me.x : cx, by = fromMe ? me.y : cy;
            for (int y = std::max(0, by - rr); y <= std::min(W - 1, by + rr); y++)
                for (int x = std::max(0, bx - rr); x <= std::min(W - 1, bx + rr); x++) {
                    if (!inShape(s.shape, s.radius, me.x, me.y, cx, cy, x, y)) continue;
                    if (!w.diff[y][x] && !w.wall[y][x]) dynAdd(w, x, y, 2, -1);
                    w.diff[y][x] = true, w.mapVer = ++g_mapSerial;
                }
        }
    }
    me.hidden = false;
}
// Способность-реакция (№23). Срабатывает сама (у бота, сети и игрока одинаково), если есть реакция, способность доступна
// и полезна: враждебная — по виновнику события в пределах дальности (по клетке — с центром на нём), на себя — на себя
// (лечение — если ХП ниже половины, телепорт — прочь от виновника). Одна реакция за раунд, общий лимит с остальными.
bool tryReact(World& w, int meIdx, int trig, int srcIdx, int prevDist) {
    if (meIdx < 0 || srcIdx < 0 || meIdx >= (int)w.f.size() || srcIdx >= (int)w.f.size() || (w.f.size() >= 2 && over(w))) return false;
    Fighter& me = w.f[meIdx];
    Fighter& src = w.f[srcIdx];
    if (!canReact(me) || src.hp <= 0 || me.team == src.team) return false;
    const int d = distFt(me, src);
    for (size_t k = 0; k < me.b.abil.size(); k++) {
        const Spec& s = me.b.abil[k];
        if (s.react != trig || !specResReady(me, (int)k)) continue;
        const int zone = s.self ? std::max(s.range, 5) : s.range;   // зона реакции — дальность способности
        if (s.counter) {   // контрзаклинание (v21): гасит способность врага в зоне
            if (trig != 4 || d > zone || !losF(w, me, src)) continue;
            spendReaction(me);
            paySpec(w, me, (int)k, false);
            evPush("{\"k\":\"react\",\"who\":" + std::to_string(meIdx) + ",\"abil\":" + std::to_string(k) + ",\"trig\":4,\"counter\":1}");
            g_countered = true;
            g_reactUsed++;
            return true;
        }
        if (trig == 3 && !(prevDist > zone && d <= zone)) continue;   // вошёл в зону только что
        if ((trig == 3 || trig == 4) && d > zone) continue;
        int gx = -1, gy = -1, tx = -1, ty = -1;
        if (s.self) {
            if (s.healN && me.hp * 2 >= me.maxHp) continue;
            if (s.teleFt > 0) {   // прыжок прочь от виновника
                int dx = (me.x > src.x) - (me.x < src.x), dy = (me.y > src.y) - (me.y < src.y);
                if (!dx && !dy) dx = 1;
                for (int st = s.teleFt / 5; st >= 1 && tx < 0; st--) {
                    const int nx = me.x + dx * st, ny = me.y + dy * st;
                    if (!fpAll(w, me, nx, ny, [&](int cx, int cy) { return !w.wall[cy][cx] && !w.pit[cy][cx]; }) || blockedAt(w, me, nx, ny)) continue;
                    tx = nx;
                    ty = ny;
                }
                if (tx < 0) continue;
            }
        } else {
            if (me.cond[C_CHARM] > 0 && me.charmSrc == srcIdx) continue;
            if (d > s.range || !losF(w, me, src)) continue;
            double ev;
            if (s.ground) {
                gx = src.x;
                gy = src.y;
                ev = evSpecAt(w, s, me, gx, gy, &src);
            } else
                ev = evSpec(w, s, me, src);
            if (ev <= 0) continue;
        }
        spendReaction(me);
        const ActInfo saved = g_act;
        const int src0 = g_srcIdx;
        g_srcIdx = meIdx;
        evPush("{\"k\":\"react\",\"who\":" + std::to_string(meIdx) + ",\"abil\":" + std::to_string(k) + ",\"trig\":" + std::to_string(trig) + "}");
        if (tx >= 0) {
            me.x = tx;
            me.y = ty;
            me.up = !nodeOK(w, me, me.x, me.y, 0) && w.deck[me.y][me.x] > 0;
            hazardHit(w, me);
            paySpec(w, me, (int)k, false);
        } else
            doSpec(w, me, (int)k, srcIdx, gx, gy);
        g_srcIdx = src0;
        g_act = saved;
        g_reactUsed++;
        return true;
    }
    return false;
}
// Реакции «мне нанесли урон» (№23): после действия или хода — по тому, кто нанёс урон последним
void resolveReactions(World& w) {
    for (int pass = 0; pass < 4; pass++) {
        std::vector<std::pair<int, int>> pend;
        for (size_t i = 0; i < w.f.size(); i++)
            if (w.f[i].hurtBy >= 0) {
                if (w.f[i].hp > 0 && hasReact(w.f[i].b)) pend.push_back({(int)i, w.f[i].hurtBy});
                w.f[i].hurtBy = -1;
            }
        if (pend.empty()) return;
        for (auto& p : pend) tryReact(w, p.first, 2, p.second);
    }
}
// жёсткий контроль: теряется ход или управление
inline bool hardCtl(int k) { return k == C_STUN || k == C_DISTR || k == C_PARAL || k == C_INCAP || k == C_CHARM || k == C_FLEE; }
void endOfTurn(Fighter& me) {
    me.turnX0 = me.turnY0 = -1;   // вне своего хода разбега нет (атаки реакцией — без наскока)
    // пакет атомов 2: сфера, полёт на время, увеличение, свет — по своим раундам
    if (me.globeRounds > 0) me.globeRounds--;
    if (me.flyBuff > 0 && --me.flyBuff == 0) {
        me.b.fly = false;
        fallCheck(me);
    }
    if (me.growRounds > 0) me.growRounds--;
    else if (me.growRounds < 0 && ++me.growRounds == 0) me.b.size = std::max(0, me.b.size - 1);   // размер вырастал — вернуть
    if (me.lightRounds > 0) me.lightRounds--;
    for (int k = 1; k < C_N; k++) {
        if (me.cond[k] > 0) {
            // нарастающая стойкость (v21, этап 7): каждый проваленный повторный спасбросок от жёсткого контроля — +2 к следующим
            const int dc = me.cdc[k] - (hardCtl(k) ? 2 * me.cgrit[k] : 0);
            if (k != C_PRONE && me.cstat[k] >= 0 && saveLR(me, me.cstat[k], dc, hardCtl(k)))
                me.cond[k] = 0;
            else {
                me.cond[k]--;
                if (k != C_PRONE && me.cstat[k] >= 0 && hardCtl(k)) me.cgrit[k]++;
            }
            if (me.cond[k] == 0) me.cgrit[k] = 0;
            // защита от вечного контроля: до конца СЛЕДУЮЩЕГО своего хода
            if (me.cond[k] == 0 && k != C_PRONE) me.cimm[k] = 1;
        } else if (me.cimm[k] > 0)
            me.cimm[k]--;
    }
    if (me.buffRounds > 0) me.buffRounds--;
    if (me.sbRounds > 0 && --me.sbRounds == 0) endStatBuff(me);
    if (me.acBuffRounds > 0) me.acBuffRounds--;
    if (me.invisRounds > 0) me.invisRounds--;
    if (me.supp > 0) me.supp--;
}
int speedOf(const Fighter& f);
// летун держится в воздухе, пока может двигаться (PHB): не сбит с ног, не опутан и не схвачен (скорость 0), не оглушён и не
// парализован. Иначе падает: 1к6 дробящего за каждые 10 фт высоты, сбит с ног
inline bool canStayAloft(const Fighter& f) {
    return f.b.fly && !f.cond[C_PRONE] && !f.cond[C_RESTR] && !f.cond[C_GRAPPLE] && !f.cond[C_STUN] && !f.cond[C_PARAL] && f.hp > 0;
}
void fallCheck(Fighter& f) {
    if (f.alt <= 0 || canStayAloft(f)) return;
    const int h = f.alt;
    f.alt = 0;
    if (f.hp <= 0) return;
    const int hp0 = f.hp;
    hurt(f, rollDice(h, 6), DT_BLUDGEONING, false);
    f.cond[C_PRONE] = std::max(f.cond[C_PRONE], 1);
    if (g_curW) evPush("{\"k\":\"fall\",\"who\":" + std::to_string((int)(&f - &g_curW->f[0])) + ",\"ft\":" + std::to_string(10 * h) + ",\"dmg\":" + std::to_string(hp0 - f.hp) + "}");
}
int speedOf(const Fighter& f) {
    int sp = f.b.speed;
    if (f.cond[C_RESTR] || f.cond[C_GRAPPLE] || f.cond[C_PARAL] || f.exh >= 5) return 0;
    if (f.cond[C_SLOW]) sp /= 2;
    if (f.exh >= 2) sp /= 2;   // истощение 2: скорость вдвое
    return std::max(0, sp - f.speedPen);   // осложнение погони этого хода
}
// Сколько приспешников призовёт способность si бойца idx сейчас: sumN копий, но не выше технических потолков (живых от
// способности и у босса, призвано за бой способностью и у босса — limity.json)
int summonRoom(const World& w, int idx, int si) {
    const Fighter& me = w.f[idx];
    const Spec& s = me.b.abil[si];
    const Limits& L = limits();
    int aliveAbil = 0, aliveBoss = 0, fightBoss = 0;
    for (auto& g : w.f)
        if (g.summoner == idx) {
            fightBoss++;
            if (g.hp > 0) {
                aliveBoss++;
                aliveAbil += g.sumSpec == si;
            }
        }
    const int made = si < (int)me.sumMade.size() ? me.sumMade[si] : 0;
    return std::max(0, std::min({s.sumTotal(), L.sumAliveAbil - aliveAbil, L.sumFightAbil - made, L.sumAliveBoss - aliveBoss, L.sumFightBoss - fightBoss}));
}
// Призыв способностью si: копии шаблона появляются рядом с призывателем и ходят сразу после него; живут до гибели, а
// призванные способностью с концентрацией исчезают, когда концентрация снята (breakConc)
void summonFrom(World& w, int idx, int si) {
    int m = summonRoom(w, idx, si);
    // бойцы лежат в w.f, и на них держат ссылки: перекладывать вектор нельзя. Бой отводит место под все потолки;
    // нет места (мир собран вручную) — призывается сколько влезает, и это растяжка
    if (w.f.size() + m > w.f.capacity()) {
        tripwire("призыв: нет места в мире", (int)w.f.capacity(), (int)(w.f.size() + m));
        m = (int)(w.f.capacity() - w.f.size());
    }
    const Limits& L = limits();
    for (int i = 0; i < m; i++) {
        const Fighter& me = w.f[idx];   // ссылка заново: push_back ниже может переложить бойцов
        Fighter g;
        const Spec& ss = me.b.abil[si];
        int ti = ss.sumTmpl, k = i - ss.sumN;   // i-я копия: сначала основная группа, потом остальные
        for (size_t q = 0; k >= 0 && q < ss.sumMore.size(); q++) {
            ti = (int)ss.sumMore[q][0];
            k -= (int)ss.sumMore[q][1];
        }
        g.b = me.b.tmpl[ti];
        g.b.tmpl.clear();
        // приспешниками управляет эвристика
        g.team = me.team;
        g.pol = me.pol;
        g.rl = false;
        g.summoner = idx;
        g.sumSpec = si;
        // (в v11–v16 эти присваивания случайно попали в комментарий: миньоны появлялись с 1 ХП)
        g.hp = g.maxHp = g.b.hp;
        g.x = me.x;
        g.y = me.y;
        g.up = me.up;
        g.fresh = true;
        g.ai.seed(rng.next());
        // миньон действует сразу после своего босса
        g.init = me.init - 1;
        g.uses.assign(g.b.abil.size(), 0);
        g.usedRound.assign(g.b.abil.size(), -10);
        g.poolLeft = g.b.poolN;
        g.trigLeft.clear();
        for (auto& t : g.b.trigs) g.trigLeft.push_back(t.uses);
        g.lrLeft = g.b.legRes;
        g.legLeft = g.b.legAct;
        g.surgeLeft = g.b.surge;
        for (int L2 = 1; L2 < 10; L2++) g.slotLeft[L2] = g.b.slots[L2];
        for (size_t k = 0; k < g.b.abil.size(); k++) g.uses[k] = g.b.abil[k].recharge && !(g.b.abil[k].rechAfter && g.b.abil[k].uses < UNL) ? 1 : g.b.abil[k].uses;
        nudge(w, g);
        w.f.push_back(g);
    }
    Fighter& me = w.f[idx];
    if ((int)me.sumMade.size() <= si) me.sumMade.resize(me.b.abil.size(), 0);
    me.sumMade[si] += m;
    // растяжки: живых от способности и у босса, призвано за бой, призывов за ход
    int aliveAbil = 0, aliveBoss = 0, fightBoss = 0;
    for (auto& g : w.f)
        if (g.summoner == idx) {
            fightBoss++;
            if (g.hp > 0) aliveBoss++, aliveAbil += g.sumSpec == si;
        }
    if (aliveAbil > L.warnAliveAbil) tripwire("призыв: живых от способности", L.warnAliveAbil, aliveAbil);
    if (aliveBoss > L.warnAliveBoss) tripwire("призыв: живых у босса", L.warnAliveBoss, aliveBoss);
    if (me.sumMade[si] > L.warnFightAbil) tripwire("призыв: за бой способностью", L.warnFightAbil, me.sumMade[si]);
    if (fightBoss > L.warnFightBoss) tripwire("призыв: за бой у босса", L.warnFightBoss, fightBoss);
    if (me.sumTurn != g_curRound) me.sumTurn = g_curRound, me.sumTurnN = 0;
    if (++me.sumTurnN > L.warnTurnSummons) tripwire("призыв: призывов за ход", L.warnTurnSummons, me.sumTurnN);
}

// Перезарядка: в начале СВОЕГО хода бросок d6 по каждой потраченной способности; возврат при результате >= порога
void rechargeStart(Fighter& me) {
    for (size_t i = 0; i < me.b.abil.size(); i++) {
        const Spec& s = me.b.abil[i];
        if (s.recharge > 0 && me.uses[i] <= 0) {
            g_rechRolls++;
            if (rollDie(6) >= s.recharge) {
                me.uses[i] = s.rechAfter && s.uses < UNL ? s.uses : 1;   // пакет атомов 3: вернулись все применения
                g_rechOk++;
            }
        }
    }
}
// Ауры врагов: в начале хода бойца урон и/или состояние от каждой ауры, в радиусе которой он стоит. true — боец погиб.
bool applyAuras(World& w, int idx) {
    Fighter& me = w.f[idx];
    for (size_t j = 0; j < w.f.size(); j++) {
        const Fighter& o = w.f[j];
        if (o.hp <= 0 || o.team == me.team || o.b.auraRad <= 0 || distFt(o, me) > o.b.auraRad ||
            (o.supp > 0 && (o.b.suppTgt & 2)))
            continue;
        if (o.b.auraN > 0) {
            const int src0 = g_srcIdx;
            g_srcIdx = (int)j;   // урон ауры наносит её владелец
            hurt(me, rollDice(o.b.auraN, o.b.auraS), o.b.auraType, true);
            g_srcIdx = src0;
            if (me.hp <= 0) return true;
        }
        if (o.b.auraCond > 0) {
            int st = condSaveStat(o.b.auraCond), dc = o.dc();
            Spec sp;
            sp.cond = o.b.auraCond;
            sp.condRounds = o.b.auraRounds;
            bool ok = o.b.auraCond == C_GRAPPLE ? false : saveLR(me, st, dc, lrWorth(me, sp));
            if (!ok) applyCond(me, o.b.auraCond, o.b.auraRounds, st, dc, true, (int)j);
        }
    }
    return false;
}
