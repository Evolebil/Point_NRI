// Оценка ожидаемой ценности действий (для бота)
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Оценка ожидаемой ценности (для решений бота)
// ---------------------------------------------------------------------------
double evWeaponBase(const World& w, const Fighter& a, const Fighter& d) {
    if (a.b.wN <= 0) return 0;   // безоружный атаки оружием не делает
    double dice = a.b.wN * (wDie(a.b) + 1) / 2.0 + (a.buffRounds > 0 ? a.buffN * (a.buffS + 1) / 2.0 : 0) + a.wMod();
    const bool melee = wpAtkRange(a.b, distFt(a, d)) <= 5;
    forRules(a, [&](const Rule& r, int) {   // «когда → что»: урон каждого попадания (и ауры союзников)
        if (r.what == E_DMG && !r.once && ruleOn(w, a, &d, r, true, melee)) dice += r.n * (r.s + 1) / 2.0;
    });
    return a.b.wAtt * pHitEst(w, a, d, a.wAtk(), wpAtkRange(a.b, distFt(a, d))) * std::max(0.5, dice) *
           dmgMult(d.b, a.b.wType, wPierceNM(a.b)) * ((d.b.uncanny && canReact(d)) ? 0.8 : 1.0);
}
double condValue(const World& w, const Fighter& me, const Fighter& t, int kind, int rounds) {
    if (t.cond[kind] > 0 || t.cimm[kind] > 0 || (t.b.immCond & (1u << kind))) return 0;
    static const double m[C_N] = {0, 0.3, 0.6, 0.4, 0.4, 0.5, 1.3, 0.3, 1.0, 1.0, 1.6, 1.2, 0.4, 0.4, 0.3, 0.9};
    // «подсвечен» усиливает МОИ атаки, остальное режет урон цели
    return me.pol->v[PO_CONDS] * m[kind] * std::min(rounds, 3) *
           (kind == C_LIT ? evWeaponBase(w, me, t) : evWeaponBase(w, t, me));
}
// ожидаемая ценность удара оружием: базовая + довесок (раз в ход, при хотя бы одном попадании)
double evWeapon(const World& w, const Fighter& a, const Fighter& d) {
    double e = evWeaponBase(w, a, d);
    double any0 = 1 - std::pow(1 - pHitEst(w, a, d, a.wAtk(), wpAtkRange(a.b, distFt(a, d))), a.b.wAtt);
    // «когда → что»: урон раз в ход (при хотя бы одном попадании)
    const bool melee = wpAtkRange(a.b, distFt(a, d)) <= 5;
    for (size_t k = 0; k < a.b.rules.size(); k++) {
        const Rule& r = a.b.rules[k];
        if (a.b.wN > 0 && r.what == E_DMG && r.once && !((a.onceUsed >> k) & 1u) && ruleOn(w, a, &d, r, true, melee))
            e += any0 * r.n * (r.s + 1) / 2.0 * dmgMult(d.b, a.b.wType, wPierceNM(a.b));
    }
    if (a.b.wN <= 0 || (a.b.rN <= 0 && a.b.rCond <= 0)) return e;
    double p = pHitEst(w, a, d, a.wAtk(), wpAtkRange(a.b, distFt(a, d))), any = 1 - std::pow(1 - p, a.b.wAtt), r = 0;
    if (a.b.rN > 0) r += a.b.rN * (a.b.rS + 1) / 2.0 * dmgMult(d.b, a.b.rType, !isPhys(a.b.rType) || a.b.wMagic);
    if (a.b.rCond > 0) {
        double pf = a.b.rCond == C_GRAPPLE ? 1.0 : pFailEst(d, condSaveStat(a.b.rCond), a.dc());
        r += pf * condValue(w, a, d, a.b.rCond, a.b.rRounds);
    }
    return e + any * r;
}
// упрётся ли толчок в стену или существо раньше, чем пройдёт полную длину
bool pushBlocked(const World& w, const Fighter& src, const Fighter& t, int ft) {
    int sx = (t.x > src.x) - (t.x < src.x), sy = (t.y > src.y) - (t.y < src.y);
    for (int i = 0; i < ft / 5; i++) {
        int nx = t.x + sx * (i + 1), ny = t.y + sy * (i + 1);
        if (!fpAll(w, t, nx, ny, [&](int cx, int cy) { return !w.wall[cy][cx] && !w.pit[cy][cx] && !w.tree[cy][cx] && w.height[cy][cx] <= w.height[t.y][t.x]; }) || blockedAt(w, t, nx, ny)) return true;
    }
    return false;
}
double evSpecOne(const World& w, const Spec& s, const Fighter& me, const Fighter& t) {
    double dmg = 0, dc = me.dc() + s.dcB;
    for (auto& d : s.dmgParts()) dmg += d.n * (d.s + 1) / 2.0 * dmgMult(t.b, d.type, specMagical(me.b, d.type));
    double push = t.b.immPush ? 0.0
                              : me.pol->v[PO_PUSHW] * s.pushFt / 5.0 +
                                    (s.collN > 0 && pushBlocked(w, me, t, s.pushFt) ? s.collN * 3.5 : 0.0),
           cv = s.cond ? condValue(w, me, t, s.cond, s.condRounds) : 0;
    double ter = (s.terr && !t.b.fly && !t.b.ignDiff) ? me.pol->v[PO_TERRW] * evWeaponBase(w, t, me) : 0;
    double dot = s.dotN > 0 ? s.dotN * (s.dotS + 1) / 2.0 * std::min(s.dotRounds, 3) * dmgMult(t.b, s.dtype, true) *
                                  me.pol->v[PO_DOTW]
                            : 0;
    if (s.delivery == 0) {
        double ph = pHitEst(w, me, t, me.sAtk(), s.range), pf = pFailEst(t, s.saveStat, dc);
        return ph * (dmg + pf * cv + push + dot) + ter;
    }
    if (s.delivery == 1) {
        double pf = pFailEst(t, s.saveStat, dc);
        return dmg * (pf + (1 - pf) * 0.5) + pf * cv + push + pf * dot + ter;
    }
    return dmg + cv + push + dot + ter;
}
// Ценность области с центром (cx, cy): враги в радиусе — польза, свои (и я сам) — вред, если область не избирательная (№17)
double areaValue(const World& w, const Spec& s, const Fighter& me, int cx, int cy) {
    double ev = 0;
    for (auto& e : w.f) {
        if (e.hp <= 0 || !inShapeF(s.shape, s.radius, me.x, me.y, cx, cy, e)) continue;
        if (e.team != me.team)
            ev += evSpecOne(w, s, me, e);
        else if (!s.selective)
            ev -= evSpecOne(w, s, me, e);
    }
    return ev;
}
double evWeapon(const World& w, const Fighter& a, const Fighter& d);
double evSpec(const World& w, const Spec& s, const Fighter& me, const Fighter& center) {
    if (s.offhand) return evWeaponBase(w, me, center) / std::max(1, me.b.wAtt);   // одна атака
    if (s.onHit) {   // действие Атака + приём на первое попадание
        Spec r = s;
        r.onHit = false;
        r.delivery = s.delivery == 1 ? 1 : 2;
        if (s.pool) r.more.push_back({1, s.poolS, me.b.wType});
        const double p = pHitEst(w, me, center, me.wAtk(), wpAtkRange(me.b, distFt(me, center))), any = 1 - std::pow(1 - p, me.b.wAtt);
        return evWeapon(w, me, center) + any * evSpecOne(w, r, me, center);
    }
    if (s.radius == 0 && s.targets > 1) {   // несколько целей: главная и ещё лучшие враги в пределах дальности
        std::vector<double> ev;
        for (auto& e : w.f)
            if (&e != &center && e.hp > 0 && e.team != me.team && distFt(me, e) <= s.range && losF(w, me, e)) ev.push_back(evSpecOne(w, s, me, e));
        std::sort(ev.rbegin(), ev.rend());
        double v = evSpecOne(w, s, me, center);
        for (int k = 0; k < s.targets - 1 && k < (int)ev.size(); k++) v += ev[k];
        return v;
    }
    if (s.radius == 0) return evSpecOne(w, s, me, center);
    return areaValue(w, s, me, center.x, center.y);
}
// Барьер (№22) для бота: ценен, если ближник ещё не подошёл, а я бьюсь издали — стена перед ним стоит доли его урона за
// каждый раунд барьера (до трёх)
double wallValue(const World& w, const Spec& s, const Fighter& me, const Fighter& e) {
    if (s.wallLen <= 0 || !meleeWeapon(e.b) || meleeWeapon(me.b) || distFt(me, e) <= meleeReach(e.b)) return 0;
    return me.pol->v[PO_WALLW] * evWeaponBase(w, e, me) * std::min(s.wallRounds, 3);
}
// Ценность способности по клетке (cx, cy): всё, что накроет область (враги в радиусе), и барьер перед врагом e
double evSpecAt(const World& w, const Spec& s, const Fighter& me, int cx, int cy, const Fighter* wallFor = nullptr) {
    double ev = 0;
    if (s.dmgN || !s.more.empty() || s.cond || s.pushFt || s.dotN || s.terr) ev += areaValue(w, s, me, cx, cy);
    if (wallFor) ev += wallValue(w, s, me, *wallFor);
    return ev;
}
// Лучшая клетка для способности по клетке (№9, №22): центр на враге или сдвинутый к себе так, чтобы враг остался на
// краю области (достаёт дальше дальности и из-за угла); барьер — на клетку перед ближником. Клетка в пределах дальности
// и видна мне. Возвращает ценность (0 — некуда).
double bestGroundCell(const World& w, const Spec& s, const Fighter& me, const std::vector<int>& enemies, int& gx, int& gy) {
    double best = 0;
    gx = gy = -1;
    auto tryCell = [&](int cx, int cy, const Fighter* wallFor) {
        if (cx < 0 || cy < 0 || cx >= w.aw || cy >= w.ah || w.wall[cy][cx]) return;
        if (distFC(w, me, cx, cy) > s.range || !losFC(w, me, cx, cy)) return;
        const double ev = evSpecAt(w, s, me, cx, cy, wallFor);
        if (ev > best) {
            best = ev;
            gx = cx;
            gy = cy;
        }
    };
    for (int ei : enemies) {
        const Fighter& e = w.f[ei];
        if (e.hp <= 0) continue;
        const int sx = (me.x > e.x) - (me.x < e.x), sy = (me.y > e.y) - (me.y < e.y);
        if (s.wallLen > 0) tryCell(e.x + sx, e.y + sy, &e);
        if (s.wallLen > 0 && !(s.dmgN || s.cond || s.pushFt || s.dotN || s.terr)) continue;
        tryCell(e.x, e.y, nullptr);
        const int k = std::min(s.radius / 5, std::max(std::abs(me.x - e.x), std::abs(me.y - e.y)) - 1);
        if (k > 0 && s.shape <= SH_CUBE) tryCell(e.x + sx * k, e.y + sy * k, nullptr);
    }
    return best;
}
