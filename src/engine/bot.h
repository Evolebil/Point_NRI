// Правило-бот: ход бойца (takeTurn)
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// подготовленное действие (8 ноября): способность (на себя — себе) или одна атака оружием
void readyFire(World& w, int gi, int fi) {
    Fighter& g = w.f[gi];
    if (g.readyAbil >= 0 && g.readyAbil < (int)g.b.abil.size()) {
        if (resReady(g, specAbility(g.b, g.readyAbil).cost)) doSpec(w, g, g.readyAbil, g.b.abil[g.readyAbil].self ? gi : fi);
    } else
        doWeapon(w, g, fi, 1);
}
void takeTurn(World& w, int idx) {
    TurnState ts;
    if (!beginTurn(w, idx, ts)) return;
    Fighter& me = w.f[idx];
    if (forcedFlee(w, idx, ts)) return;
    int speed = ts.speed;
    const int speed0 = ts.speed0;
    bool &actionUsed = ts.actionUsed, &bonusUsed = ts.bonusUsed;   // бюджет хода — общий TurnState (платит payEcon)

    const Brain br = brainOf(w, me);
    std::vector<int> enemies;
    for (size_t i = 0; i < w.f.size(); i++)
        if (w.f[i].hp > 0 && w.f[i].team != me.team && !(me.cond[C_CHARM] > 0 && me.charmSrc == (int)i) &&
            (br.sense >= SENSE_INF || distFt(me, w.f[i]) <= br.sense + 60 * g_curRound) && isKnown(w, me.team, (int)i))
            enemies.push_back((int)i);
    // очарованному некого атаковать — но состояние всё равно идёт и заканчивается
    if (enemies.empty()) {
        // туман войны: никого не видно — идёт к месту, где последний раз видел врага (самые свежие сведения, ближе)
        int bk = -1;
        for (size_t i = 0; i < w.f.size() && i < w.known[me.team].size(); i++) {
            const auto& k = w.known[me.team][i];
            if (w.f[i].hp <= 0 || w.f[i].team == me.team || k[2] < 0 || (k[0] == me.x && k[1] == me.y)) continue;
            if (bk < 0 || k[2] > w.known[me.team][bk][2] ||
                (k[2] == w.known[me.team][bk][2] && std::max(std::abs(k[0] - me.x), std::abs(k[1] - me.y)) <
                                                         std::max(std::abs(w.known[me.team][bk][0] - me.x), std::abs(w.known[me.team][bk][1] - me.y))))
                bk = (int)i;
        }
        if (bk >= 0 && ts.speed > 0 && !me.cond[C_FRIGHT]) {
            Fighter spot;
            spot.x = w.known[me.team][bk][0];
            spot.y = w.known[me.team][bk][1];
            moveToward(w, me, spot, 0, ts.speed, false, true);
            g_act.type = "search";
        } else
            g_act.type = "idle";
        endOfTurn(me);
        return;
    }
    int focus = enemies[0], fd = 1 << 30;
    for (int e : enemies) {
        int d = distFt(me, w.f[e]) - (br.focus && e < w.nLead ? me.pol->v[PO_FOCUSFT] : 0);
        if (d < fd) {
            fd = d;
            focus = e;
        }
    }
    // послушание миньона зависит от ХАР главного босса
    const bool obeys = !(idx >= w.nLead && br.obey < 1.0) || me.ai.unit() < br.obey;
    // приказ главного босса
    if (obeys && idx >= w.nLead && w.cmd[me.team] >= 0 &&
        std::find(enemies.begin(), enemies.end(), w.cmd[me.team]) != enemies.end())
        focus = w.cmd[me.team];
    if (idx < w.nLead) {
        recordPrim(idx, {PR_ORDER, focus});
        if (w.cmd[me.team] != focus) evPush("{\"k\":\"order\",\"who\":" + std::to_string(idx) + ",\"tgt\":" + std::to_string(focus) + "}");
        w.cmd[me.team] = focus;
    }
    Fighter& F = w.f[focus];
    double hpFrac = (double)me.hp / me.maxHp;
    // высота полёта (v21): стрелок (оружие от 30 фт) держится на 20 фт — ближнику с земли не достать, ближник — на уровне цели
    if (me.b.fly && canStayAloft(me) && speed >= 10 && !me.cond[C_FRIGHT]) {
        const int ground = zF(w, me) - me.alt, cap = maxAltAt(w, me, me.x, me.y, me.up);
        int want = atkDist(me.b) >= 30 ? 2 : zF(w, F) - ground;
        want = std::clamp(want, 0, cap);
        if (want != me.alt) {
            const int step = std::min(std::abs(want - me.alt), speed / 10), a = me.alt + (want > me.alt ? step : -step);
            recordPrim(idx, {PR_ALT, a});
            ts.speed -= 10 * step;
            speed -= 10 * step;
            setAlt(w, me, a);
            if (me.hp <= 0 || over(w)) return;
        }
    }
    // превращение (v21): облик — запас хитов и тело; бот превращается сразу, как только видит врага и может заплатить
    if (!me.inForm)
        for (size_t k = 0; k < me.b.abil.size(); k++) {
            if (me.b.abil[k].formTmpl < 0 || !canPay(me, ts, specAbility(me.b, (int)k).cost)) continue;
            const Prim fp{PR_SPELL, (int)k, idx};
            recordPrim(idx, fp);
            const int s0 = ts.speed;
            applyPrim(w, idx, ts, fp, false, nullptr);
            speed += ts.speed - s0;
            break;
        }

    // арсенал: оружие под ситуацию (дальность до цели, помехи)
    if (!me.b.alt.empty() && !me.cond[C_INCAP]) {
        auto score = [&](Fighter& m) {
            double ev = evWeapon(w, m, F);
            int d = distFt(m, F), r = atkDist(m.b);
            return ev * (d <= r + speed ? 1.0 : (d <= r + 2 * speed ? 0.7 : 0.4));
        };
        double bestS = me.b.wN > 0 ? score(me) : -1;
        int bestK = -1;
        for (size_t k = 0; k < me.b.alt.size(); k++) {
            swapWeapon(me.b, k);
            double sc = score(me);
            swapWeapon(me.b, k);
            if (sc > bestS) {
                bestS = sc;
                bestK = (int)k;
            }
        }
        if (bestK >= 0) {
            recordPrim(idx, {PR_SWAP, bestK});
            swapWeapon(me.b, (size_t)bestK);
        }
    }
    // отступить от цели не более чем на maxFt
    auto retreatBy = [&](int maxFt) {
        maxFt = std::min(maxFt, speed);
        if (maxFt < 5 || me.cond[C_RESTR] || me.cond[C_GRAPPLE]) return;
        int dx = me.x - F.x, dy = me.y - F.y, sx = dx == 0 ? ((int)me.ai.below(2) * 2 - 1) : (dx > 0 ? 1 : -1),
            sy = dy == 0 ? ((int)me.ai.below(2) * 2 - 1) : (dy > 0 ? 1 : -1), st = maxFt / 5 + 1;
        Fighter away;
        away.x = std::clamp(me.x + sx * st, 0, w.aw - 1);
        away.y = std::clamp(me.y + sy * st, 0, w.ah - 1);
        speed -= moveToward(w, me, away, 0, maxFt, false);
    };
    // Отход: при низком ХП и вражеском ближнике рядом лучше выйти без штрафа за провокацию, чем героически стоять
    if (br.disengage && hpFrac < me.pol->v[PO_DISENG] && meleeWeapon(F.b) && distFt(me, F) <= meleeReach(F.b) &&
        me.b.wRange > 5 && !actionUsed) {
        recordPrim(idx, {PR_DISENGAGE, 0});
        me.disengaged = true;
        payEcon(ts, me, EC_ACTION);
        g_act.type = "disengage";
        int dx = (me.x - F.x), dy = (me.y - F.y);
        int tx = std::clamp(me.x + (dx == 0 ? ((int)me.ai.below(2) * 2 - 1) : (dx > 0 ? 1 : -1)) * (speed / 5), 0, w.aw - 1);
        int ty = std::clamp(me.y + (dy == 0 ? ((int)me.ai.below(2) * 2 - 1) : (dy > 0 ? 1 : -1)) * (speed / 5), 0, w.ah - 1);
        Fighter away;
        away.x = tx;
        away.y = ty;
        // Отход: действие, реакцию не провоцирует; тратит общий бюджет движения
        speed -= moveToward(w, me, away, 0, speed, false);
    }

    // Отход бонусным действием: стрелок, до которого дотянулся ближник, отходит бонусным действием и сохраняет действие
    // для атаки
    if (br.disengage && me.b.disengB && !bonusUsed && !actionUsed && !me.disengaged && me.b.wRange > 5 &&
        meleeWeapon(F.b) && distFt(me, F) <= meleeReach(F.b)) {
        recordPrim(idx, {PR_DISENGAGE, 1});
        me.disengaged = true;
        payEcon(ts, me, EC_BONUS);
        retreatBy(speed / 2);
    }

    // самоспособности: лечение / побег / бафф
    // toward=false: прыжок ОТ врага (побег); toward=true: прыжок К врагу (сближение)
    auto trySelf = [&](auto pred, bool toward = false) {
        for (size_t i = 0; i < me.b.abil.size(); i++) {
            const Spec& s = me.b.abil[i];
            if (!s.self || s.react || !specResReady(me, (int)i) || !pred(s)) continue;
            if (!(s.jump && s.teleFt > 0) && (s.bonus ? bonusUsed : actionUsed)) continue;   // прыжок — без действия, за движение
            if (s.teleFt > 0) {
                int dx = (F.x > me.x) - (F.x < me.x), dy = (F.y > me.y) - (F.y < me.y);
                if (!toward) {
                    dx = -dx;
                    dy = -dy;
                    if (!dx && !dy) dx = 1;
                } else if (!dx && !dy)
                    continue;
                bool done = false;
                for (int st = s.teleFt / 5; st >= 1 && !done; st--) {
                    int nx = me.x + dx * st, ny = me.y + dy * st;
                    if (!fpAll(w, me, nx, ny, [&](int cx, int cy) { return !w.wall[cy][cx] && !w.pit[cy][cx]; }) || blockedAt(w, me, nx, ny)) continue;
                    // не на голову врагу
                    if (toward && 5 * std::max(std::abs(nx - F.x), std::abs(ny - F.y)) < 5) continue;
                    if (s.jump) {   // прыжок (пакет атомов 2): хватает движения и не сквозь стену — как проверяет applyPrim
                        const int d = distFC(w, me, nx, ny);
                        if (d > speed || !losFC(w, me, nx, ny)) continue;
                        speed -= d;
                    }
                    me.x = nx;
                    me.y = ny;
                    done = true;
                }
                // прыгнуть некуда — применение не тратим
                if (!done) continue;
                me.up = !nodeOK(w, me, me.x, me.y, 0) && w.deck[me.y][me.x] > 0;
                hazardHit(w, me);
                recordPrim(idx, {PR_SPELL, (int)i, -1, me.x, me.y});
                paySpec(w, me, (int)i, false);
                g_act.type = "teleport";
                g_act.target = -1;
            } else {
                recordPrim(idx, {PR_SPELL, (int)i, focus});
                doSpec(w, me, (int)i, focus);
            }
            if (s.jump && s.teleFt > 0)
                ;   // прыжок оплачен движением
            else if (s.bonus)
                payEcon(ts, me, EC_BONUS);
            else
                payEcon(ts, me, EC_ACTION);
            return true;
        }
        return false;
    };
    if (br.selfTactics && hpFrac < me.pol->v[PO_TELE] && distFt(me, F) <= 10) {
        if (!trySelf([](const Spec& s) { return s.teleFt > 0; })) trySelf([](const Spec& s) { return s.hide; });
    }
    if (br.selfSimple && hpFrac < me.pol->v[PO_HEAL])
        trySelf([](const Spec& s) { return s.healN > 0; });
    // союзники (v21): лечение раненому союзнику (меньше половины хитов) и приказ атаковать — тому, кто достаёт врага
    if (br.selfSimple)
        for (size_t i = 0; i < me.b.abil.size(); i++) {
            const Spec& s = me.b.abil[i];
            if (!s.self || !s.ally || s.react || !specResReady(me, (int)i) || (s.bonus ? bonusUsed : actionUsed)) continue;
            const int t = pickAlly(w, me, s);
            if (t < 0 || t == idx) continue;
            const Fighter& g = w.f[t];
            const bool want = s.cmdAtk || ((s.healN || s.healB) && g.hp * 2 < g.maxHp) || (s.tempN && g.temp <= 0 && g.hp * 2 < g.maxHp);
            if (!want) continue;
            recordPrim(idx, {PR_SPELL, (int)i, t});
            doSpec(w, me, (int)i, t);
            payEcon(ts, me, s.bonus ? EC_BONUS : EC_ACTION);
            if (over(w) || me.hp <= 0) break;
        }
    if (br.selfTactics && me.buffRounds <= 0 &&
        distFt(me, F) <= 2 * speed * me.pol->v[PO_BUFFD] + 10)
        trySelf([](const Spec& s) { return s.buffN > 0; });
    if (br.selfTactics && me.sbRounds <= 0 &&
        distFt(me, F) <= 2 * speed * me.pol->v[PO_BUFFD] + 10)
        trySelf([](const Spec& s) { return s.sbAmt > 0; });
    if (br.selfSimple && me.temp <= 0 &&
        distFt(me, F) <= 2 * speed * me.pol->v[PO_BUFFD] + 10)
        trySelf([](const Spec& s) { return s.tempN > 0; });
    if (br.selfTactics && me.acBuffRounds <= 0 &&
        distFt(me, F) <= 2 * speed * me.pol->v[PO_BUFFD] + 10)
        trySelf([](const Spec& s) { return s.acB > 0; });
    // тьма вокруг себя — только если сам вижу в ней (тёмное/слепое зрение), а враг нет
    if (br.selfTactics && (me.b.dv >= 10 || me.b.blindsight >= 10) && F.b.dv < 30 && F.b.blindsight < 10 &&
        !w.dark[me.y][me.x] && distFt(me, F) <= 3 * speed + 60)
        trySelf([](const Spec& s) { return s.darkR > 0; });
    if (br.selfTactics && me.invisRounds <= 0 && F.b.blindsight < 30 &&
        distFt(me, F) <= 2 * speed * me.pol->v[PO_BUFFD] + 10)
        trySelf([](const Spec& s) { return s.invisN > 0; });

    // выбор дальности: берём самый ценный вариант без учёта позиции
    double bestRaw = evWeapon(w, me, F);
    int desired = me.b.wN > 0 ? atkDist(me.b) : 5;
    for (size_t i = 0; i < me.b.abil.size(); i++) {
        const Spec& s = me.b.abil[i];
        if (s.self || s.react || !specResReady(me, (int)i)) continue;
        double ev = evSpec(w, s, me, F);
        if (ev > bestRaw) {
            bestRaw = ev;
            desired = specReach(me.b, s);
        }
    }
    // призыв: босс решает, применить ли призывающую способность. Призываем, только если волна даёт не меньше пользы,
    // чем собственное лучшее действие
    for (size_t k = 0; k < me.b.abil.size(); k++) {
        const Spec& s = me.b.abil[k];
        if (s.sumN <= 0 || s.react || !canPay(me, ts, specAbility(me.b, (int)k).cost)) continue;
        const int m = summonRoom(w, idx, (int)k);
        if (m <= 0) continue;
        Fighter tmp;
        tmp.b = me.b.tmpl[s.sumTmpl];
        tmp.team = me.team;
        tmp.x = me.x;
        tmp.y = me.y;
        tmp.hp = tmp.maxHp = tmp.b.hp;
        double ev = evWeapon(w, tmp, F);
        for (auto& sp : tmp.b.abil)
            if (!sp.self) ev = std::max(ev, evSpec(w, sp, tmp, F));
        const double waveEV = ev * m;
        if (!br.evalSummon || waveEV * me.pol->v[PO_SUMMON] >= bestRaw) {
            recordPrim(idx, {PR_SPELL, (int)k, idx});
            const int econ = specAbility(me.b, (int)k).cost.econ;
            doSpec(w, me, (int)k, idx);
            payEcon(ts, w.f[idx], econ);
        }
        break;   // одна волна за ход
    }
    // перемещение (испуганный не приближается)
    bool inPos = distFt(me, F) <= desired && losF(w, me, F);
    // кайтинг: дальнобойный не даёт ближнику подойти
    if (br.kite && me.pol->v[PO_KITE] > 0 && me.b.wRange > 5 && F.b.wRange <= 5 && speed > 0 && !me.cond[C_RESTR] &&
        !me.cond[C_GRAPPLE] && !me.cond[C_FLEE]) {
        int d = distFt(me, F), threat = F.b.reach + (int)(speedOf(F) * me.pol->v[PO_KITE]),
            room = std::min(speed, desired - d);
        if (d > F.b.reach && d <= threat && room >= 5) {
            int dx = me.x - F.x, dy = me.y - F.y, sx = dx == 0 ? ((int)me.ai.below(2) * 2 - 1) : (dx > 0 ? 1 : -1),
                sy = dy == 0 ? ((int)me.ai.below(2) * 2 - 1) : (dy > 0 ? 1 : -1), st = room / 5 + 1;
            Fighter away;
            away.x = std::clamp(me.x + sx * st, 0, w.aw - 1);
            away.y = std::clamp(me.y + sy * st, 0, w.ah - 1);
            speed -= moveToward(w, me, away, 0, room, false);
            inPos = distFt(me, F) <= desired && losF(w, me, F);
        }
    }
    // испуганный не приближается
    if (!inPos && !me.cond[C_FRIGHT] && !me.cond[C_FLEE]) {
        int need = distFt(me, F) - desired;
        // сначала прыжок К врагу
        if (need > speed) {
            if (br.selfTactics) trySelf([](const Spec& s) { return s.teleFt > 0; }, true);
            need = distFt(me, F) - desired;
        }
        // потом бег
        if (need > speed && !actionUsed && speed > 0) {
            const int walk = speed;
            speed += speed0;
            const size_t r0 = g_rec ? g_rec->size() : 0;
            const int moved = moveToward(w, me, F, desired, speed, true);
            speed -= moved;
            // пал от провоцированной атаки на шаге за пределом обычной скорости — шаг был в счёт Рывка (иначе повтор хода
            // с обычной скоростью этого шага не сделает)
            if (moved > walk || me.hp <= 0) {
                if (g_rec && idx == g_recIdx) g_rec->insert(g_rec->begin() + r0, Prim{PR_DASH});   // Рывок — до движения
                payEcon(ts, me, EC_ACTION);
                me.dashedNow = true;   // Рывок (в погоне считается и может утомить)
            } else
                speed = std::max(0, walk - moved);   // дальше обычной скорости не пошёл: действие не потрачено
        } else
            speed -= moveToward(w, me, F, desired, speed, true);
    }

    // основное действие и бонусное: лучший осуществимый вариант
    // 0 — действие, 1 — бонусное, 2 — всплеск действий (второе действие)
    for (int pass = 0; pass < 3; pass++) {
        if (over(w) || me.hp <= 0) break;   // бой решён (или босс пал от реакции): дальше не действует, как и повтор хода
        bool bonusPass = pass == 1;
        if (pass == 2) {
            if (me.surgeLeft <= 0 || !actionUsed || over(w) || me.hp <= 0 || me.cond[C_DISTR] || me.cond[C_INCAP] ||
                me.cond[C_FLEE] ||
                !(g_act.type.compare(0, 6, "attack") == 0 || g_act.type.compare(0, 7, "ability") == 0))
                continue;
            actionUsed = false;
        }
        if (bonusPass ? bonusUsed : actionUsed) continue;
        // cands — все осуществимые варианты (нужны только для шума)
        double best = 0;
        int bAct = -2, bT = -1, bGx = -1, bGy = -1;
        std::vector<std::array<int, 4>> cands;   // {действие, цель, клетка x, клетка y}
        if (!bonusPass && me.b.wN > 0) {   // безоружный атаки оружием не делает
            for (int e : enemies) {
                Fighter& E = w.f[e];
                if (E.hp <= 0 || distFt(me, E) > atkDist(me.b) || !losF(w, me, E)) continue;
                double ev = evWeapon(w, me, E),
                       sc = br.focus ? ev * (1 + (e < w.nLead ? me.pol->v[PO_TGTMAIN] : 0) +
                                             me.pol->v[PO_TGTKILL] * std::min(1.0, ev / std::max(1, E.hp)))
                                     : ev;
                if (br.noise > 0) cands.push_back({-1, e, -1, -1});
                // приказанная цель ценнее
                if (obeys && idx >= w.nLead && e == w.cmd[me.team]) sc *= 1.6;
                if (sc > best) {
                    best = sc;
                    bAct = -1;
                    bT = e;
                }
            }
        }
        for (size_t i = 0; i < me.b.abil.size(); i++) {
            const Spec& s = me.b.abil[i];
            if (s.self || s.react || s.bonus != bonusPass || !specResReady(me, (int)i)) continue;
            if (s.ground) {   // по клетке (№9, №22): лучшая клетка в пределах дальности и видимости
                int gx, gy;
                double sc = bestGroundCell(w, s, me, enemies, gx, gy);
                if (sc <= 0) continue;
                if (br.noise > 0) cands.push_back({(int)i, -1, gx, gy});
                if (sc > best) {
                    best = sc;
                    bAct = (int)i;
                    bT = -1;
                    bGx = gx;
                    bGy = gy;
                }
                continue;
            }
            for (int e : enemies) {
                Fighter& E = w.f[e];
                if (E.hp <= 0 || distFt(me, E) > specReach(me.b, s) || !losF(w, me, E)) continue;
                double ev = evSpec(w, s, me, E),
                       sc = br.focus ? ev * (1 + (e < w.nLead ? me.pol->v[PO_TGTMAIN] : 0) +
                                             me.pol->v[PO_TGTKILL] * std::min(1.0, ev / std::max(1, E.hp)))
                                     : ev;
                if (br.noise > 0) cands.push_back({(int)i, e, -1, -1});
                // приказанная цель ценнее
                if (obeys && idx >= w.nLead && e == w.cmd[me.team]) sc *= 1.6;
                if (sc > best) {
                    best = sc;
                    bAct = (int)i;
                    bT = e;
                    bGx = bGy = -1;
                }
            }
        }
        // ошибка: случайный вариант вместо лучшего
        if (br.noise > 0 && !cands.empty() && me.ai.unit() < br.noise) {
            auto c = cands[me.ai.below((uint32_t)cands.size())];
            bAct = c[0];
            bT = c[1];
            bGx = c[2];
            bGy = c[3];
        }
        // при низких ХП — защита вместо атаки
        if (br.dodge && pass == 0 && bAct != -2 && hpFrac < me.pol->v[PO_DODGE] && !me.cond[C_DISTR]) {
            recordPrim(idx, {PR_DODGE});
            me.dodging = true;
            payEcon(ts, me, EC_ACTION);
            fireTrig(w, me, T_DODGE, nullptr);   // «событие → эффект» (v21)
            g_act.type = "dodge";
            continue;
        }
        if (pass == 2 && bAct != -2) recordPrim(idx, {PR_SURGE});
        if (bAct == -1) {
            if (hasWhen(me.b, W_RECKLESS) && me.b.wRange <= 5 && hpFrac > me.pol->v[PO_RECK]) me.wasReckless = true;
            recordPrim(idx, {PR_ATTACK, bT, me.wasReckless ? 1 : 0});
            doWeapon(w, me, bT);
            if (!bonusPass) resolvePending(w, idx, ts);   // «событие → эффект»: атака бонусом после крита или убийства
        } else if (bAct >= 0) {
            recordPrim(idx, {PR_SPELL, bAct, bT, bGx, bGy});
            doSpec(w, me, bAct, bT, bGx, bGy);
            if (!bonusPass) resolvePending(w, idx, ts);
        }
        // нечем атаковать (v21): враг рядом и союзник при нём — Помощь; враг подходит к моей досягаемости — Подготовка;
        // иначе Уклонение
        else if (pass == 0 && bAct == -2) {
            int helpT = -1;
            for (int e : enemies) {
                const Fighter& E = w.f[e];
                if (E.hp <= 0 || distFt(me, E) > 5) continue;
                for (auto& g : w.f)
                    if (&g != &me && g.hp > 0 && g.team == me.team && g.b.wN > 0 && distFt(g, E) <= atkDist(g.b)) helpT = e;
            }
            const int near = nearestEnemyInRange(w, me, 1 << 20);
            const bool readyOk = me.b.wN > 0 && !me.disarmed && near >= 0 && distFt(me, w.f[near]) > atkDist(me.b) &&
                                 distFt(me, w.f[near]) <= atkDist(me.b) + speedOf(w.f[near]) + 5;
            if (!me.cond[C_DISTR] && (helpT >= 0 || readyOk)) {
                const Prim pp = helpT >= 0 ? Prim{PR_HELP, helpT} : Prim{PR_READY};
                recordPrim(idx, pp);
                if (helpT >= 0)
                    w.f[helpT].helpBy = idx;
                else
                    me.readied = true;
                payEcon(ts, me, EC_ACTION);
                g_act.type = helpT >= 0 ? "help" : "ready";
                continue;
            }
            recordPrim(idx, {PR_DODGE});
            me.dodging = true;
            payEcon(ts, me, EC_ACTION);
            fireTrig(w, me, T_DODGE, nullptr);   // «событие → эффект» (v21)
            g_act.type = "dodge";
            continue;
        }
        if (bAct != -2) {
            if (bonusPass)
                payEcon(ts, me, EC_BONUS);
            else {
                payEcon(ts, me, EC_ACTION);
                if (pass == 2) {
                    me.surgeLeft--;
                    g_surge++;
                }
            }
        } else if (pass == 2)
            actionUsed = true;   // всплеск не понадобился: второе действие не открывается (это не плата)
    }
    endOfTurn(me);
}
