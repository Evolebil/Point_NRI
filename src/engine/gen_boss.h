// Генератор босса (makeBoss) и пара боссов (makePair)
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Генератор босса
// ---------------------------------------------------------------------------
// доля времени, в которую действует условие (ХП>50%, ХП<=50%, нет состояний)
const double CD_W[3] = {0.55, 0.45, 0.70};
inline double condDefPrice(const Coef& C, int trig, int scope) {
    return C[AT_CONDDEF] * scl(AT_CONDDEF) * 5.0 * (scope == 1 ? 1.0 : g_nmShare) * CD_W[trig - 1];
}
Boss makeBoss(long long budget, const Coef& C, bool allowSummon, bool force = false);
Boss makeBossOnce(long long budget, const Coef& C, bool allowSummon, bool force) {
    // минимальный бюджет босса
    if (budget < MIN_BUDGET) budget = MIN_BUDGET;
    if (budget > MAX_BUDGET) budget = MAX_BUDGET;
    budget += STAT_BASE;   // видимый бюджет считает характеристики от 10; внутри учёт абсолютный
    BuildScope bscope;
    Boss b;
    long long spent = 0;
    double prevB = g_B;
    g_B = (double)budget;
    double passLeft = budget * (0.05 + 0.40 * warp(BW_PASS, rand01()));
    g_bstate.budget = (double)budget;
    auto buy = [&](double cost) {
        long long c = (long long)std::ceil(cost);
        if (c > passLeft) return false;
        passLeft -= c;
        spent += c;
        g_bstate.spent = (double)spent;
        g_bstate.passLeft = passLeft;
        return true;
    };
    auto mark = [&](int a) { b.atoms |= ONE << a; };

    auto P = [&](int a, double m = 1.0) { return C[a] * m * scl(a); };
    // скорость (v21, №7): покупается вся, с нуля, шагами по 5 фт, первой из пассивов (без неё босс не ходит). Обычно 30 фт,
    // иногда любая от 5 до 60; не хватает очков — на 5 фт меньше. Атом отмечен только у быстрых (> 30 фт): у всех он
    // есть, и винрейт «владельцев» иначе ничего бы не мерил.
    {
        int sp = bern(BP_SPEED, 0.20) ? magN(MG_SPEED, 5, MAX_FT_G, 5) : 30;
        while (sp > 0 && !buy(P(AT_SPEED, speedUnits(sp)))) sp -= 5;
        b.speed = sp;
        if (sp > 30) mark(AT_SPEED);
    }
    // подавление: какие типы урона на ход отключают регенерацию / ауру / защиты (генератор выбирает сам); за это —
    // скидка на подавляемое
    if (bern(BP_SUPP, 0.25)) {
        int cnt = 1 + (rand01() < 0.4) + (rand01() < 0.15);
        for (int k = 0; k < cnt; k++) b.suppMask |= 1u << pickByWeight(VUL_W);
        b.suppTgt = (rand01() < 0.7 ? 1 : 0) | (rand01() < 0.5 ? 2 : 0) | (rand01() < 0.4 ? 4 : 0);
        if (!b.suppTgt) b.suppTgt = 1;
    }
    if (bern(BP_REGEN, (b.suppMask && (b.suppTgt & 1)) ? 0.60 : 0.20)) {
        int n = magN(MG_REGEN, 1, MAX_DICE_G);
        if (buy(P(AT_REGEN, n) * suppMul(b, C, 1))) {
            b.regenN = n;
            mark(AT_REGEN);
        }
    }
    // защиты по типам урона: цена типа = коэф * 5 * доля урона этого типа; уязвимость ВОЗВРАЩАЕТ очки
    auto typeCost = [&](int a, int t) { return P(a, 5.0 * g_share[t]) * (a == AT_VULN ? 1.0 : suppMul(b, C, 4)); };
    if (bern(BP_RESIST, 0.30)) {
        int n = 1 + (rand01() < 0.40) + (rand01() < 0.15);
        for (int k = 0; k < n; k++) {
            int t = pickByWeight(RES_W);
            if ((b.resMask >> t) & 1) continue;
            if (buy(typeCost(AT_RESIST, t))) {
                b.resMask |= 1u << t;
                mark(AT_RESIST);
            }
        }
    }
    if (bern(BP_IMMTYPE, 0.22)) {
        int n = 1 + (rand01() < 0.30);
        for (int k = 0; k < n; k++) {
            int t = pickByWeight(IMM_W);
            if (((b.resMask | b.immMask) >> t) & 1) continue;
            if (buy(typeCost(AT_IMMTYPE, t))) {
                b.immMask |= 1u << t;
                mark(AT_IMMTYPE);
            }
        }
    }
    if (bern(BP_VULN, 0.15)) {
        int n = 1 + (rand01() < 0.30);
        for (int k = 0; k < n; k++) {
            int t = pickByWeight(VUL_W);
            if (((b.resMask | b.immMask | b.vulnMask) >> t) & 1) continue;
            long long ref = (long long)std::ceil(typeCost(AT_VULN, t));
            b.vulnMask |= 1u << t;
            spent -= ref;
            passLeft += ref;
            mark(AT_VULN);
        }
    }
    if (bern(BP_RESNM, 0.08) && buy(P(AT_RESNM, 5.0 * g_nmShare) * suppMul(b, C, 4))) {
        b.resNM = true;
        mark(AT_RESNM);
    }
    if (bern(BP_IMMNM, 0.08) && buy(P(AT_IMMNM, 5.0 * g_nmShare) * suppMul(b, C, 4))) {
        b.immNM = true;
        mark(AT_IMMNM);
    }
    if (bern(BP_WMAGIC, 0.10) && buy(P(AT_WMAGIC))) {
        b.wMagic = true;
        mark(AT_WMAGIC);
    } else if (bern(BP_WSILVER, 0.06) && buy(P(AT_WMAGIC) * C[AT_WSILVER])) {   // пакет атомов 2: посеребрённое
        b.wSilver = true;
        mark(AT_WSILVER);
    }
    // легендарные действия: 1–5 очков (чаще всего 3)
    if (bern(BP_LEGACT, 0.10)) {
        int pts = magN(MG_LEGACT, 1, MAX_COUNT_G);
        if (buy(P(AT_LEGACT, pts))) {
            b.legAct = pts;
            mark(AT_LEGACT);
        }
    }
    // Парирование: +2..+5 к КД против одной ближней атаки за раунд (в бестиарии 3.4% карточек)
    if (bern(BP_PARRY, 0.08)) {
        int pb = magN(MG_PARRY, 1, MAX_COUNT_G);
        if (buy(P(AT_PARRY, pb))) {
            b.parry = pb;
            mark(AT_PARRY);
        }
    }
    if (bern(BP_LIGHTP, 0.04)) {   // пакет атомов 2: свет вокруг себя
        const int r = magN(MG_LIGHTR, 10, MAX_FT_G, 10);
        if (buy(C[AT_LIGHT] * r / 10.0)) { b.lightR = r; mark(AT_LIGHT); }
    }
    if (bern(BP_MAGICRES, 0.10) && buy(P(AT_MAGICRES))) {   // пакет атомов 1 (сопротивление магии — у 15% карточек бестиария)
        b.magicRes = true;
        mark(AT_MAGICRES);
    }
    if (bern(BP_SPELLRES, 0.03) && buy(P(AT_SPELLRES))) {
        b.spellRes = true;
        mark(AT_SPELLRES);
    }
    if (bern(BP_CRITIMM, 0.03) && buy(P(AT_CRITIMM))) {
        b.critImm = true;
        mark(AT_CRITIMM);
    }
    if (bern(BP_UNCANNY, 0.04) && buy(P(AT_UNCANNY))) {
        b.uncanny = true;
        mark(AT_UNCANNY);
    }
    if (bern(BP_IMMPUSH, 0.08) && buy(P(AT_IMMPUSH))) {
        b.immPush = true;
        mark(AT_IMMPUSH);
    }
    if (bern(BP_DISENGB, 0.08) && buy(P(AT_DISENGB))) {
        b.disengB = true;
        mark(AT_DISENGB);
    }
    // аура: урон и/или состояние врагам в радиусе в начале их хода
    if (bern(BP_AURA, (b.suppMask && (b.suppTgt & 2)) ? 0.30 : 0.10)) {
        double r = rand01(), q = rand01();
        b.auraRad = magN(MG_AURARAD, 10, MAX_FT_G, 5);   (void)q;
        bool dm = r < 0.75, cd = r > 0.55;
        if (dm) {
            b.auraN = magN(MG_AURAN, 1, MAX_DICE_G);
            b.auraS = DICE[catN(BC_AURAS, DICE_N)];
            b.auraType = pickAbilityType();
        }
        if (cd) {
            b.auraCond = 1 + catN(BC_AURAC, C_N - 1);
            b.auraRounds = b.auraCond == C_PRONE ? 1 : magN(MG_AURAR, 1, MAX_ROUNDS_G);
        }
        long long ac = auraPrice(b, C);
        if (ac > 0 && buy((double)ac))
            markAura(b, b.atoms);
        else {
            b.auraN = 0;
            b.auraRad = 0;
            b.auraCond = 0;
            b.auraRounds = 1;
        }
    }
    if (bern(BP_DARKV, 0.35)) {
        double r = rand01();
        int idx = r < 0.30 ? 1 : (r < 0.80 ? 2 : 3);
        if (buy(P(AT_DARKV, idx))) {
            b.dv = idx == 3 ? 120 : 30 * idx;
            mark(AT_DARKV);
        }
    }
    if (bern(BP_BLINDS, 0.06)) {
        double r = rand01();
        int idx = r < 0.40 ? 1 : (r < 0.80 ? 2 : 3);
        if (buy(P(AT_BLINDS, idx))) {
            b.blindsight = idx == 1 ? 10 : (idx == 2 ? 30 : 60);
            mark(AT_BLINDS);
        }
    }
    if (bern(BP_TRUESIGHT, 0.04)) {   // истинное зрение (v21)
        const int k = catN(BC_TRUESIGHT, 4);
        if (buy(P(AT_TRUESIGHT, k + 1))) {
            static const int TS[4] = {10, 30, 60, 120};
            b.truesight = TS[k];
            mark(AT_TRUESIGHT);
        }
    }
    if (bern(BP_IGNTERR, 0.06) && buy(P(AT_IGNTERR))) {
        b.ignDiff = true;
        mark(AT_IGNTERR);
    }
    if (bern(BP_CLIMB, 0.05) && buy(P(AT_CLIMB))) {   // скорости движения (v21)
        b.climbS = true;
        mark(AT_CLIMB);
    }
    if (bern(BP_SWIM, 0.05) && buy(P(AT_SWIM))) {
        b.swimS = true;
        mark(AT_SWIM);
    }
    if (bern(BP_BURROW, 0.03) && buy(P(AT_BURROW))) {
        b.burrowS = true;
        mark(AT_BURROW);
    }
    if (bern(BP_EVASION, 0.04) && buy(P(AT_EVASION))) {
        b.evasion = true;
        mark(AT_EVASION);
    }
    if (bern(BP_LAIR, 0.04)) {   // действия логова
        const int lv = 1 + catN(BC_LAIR, 3);
        if (buy(P(AT_LAIR, lv))) {
            b.lair = lv;
            mark(AT_LAIR);
        }
    }
    if (bern(BP_SUNSENS, 0.03)) {   // недостаток: очки возвращаются
        b.sunSens = true;
        buy(-(double)(long long)std::ceil(P(AT_SUNSENS)));
        mark(AT_SUNSENS);
    }
    if (bern(BP_RIPOSTE, 0.05) && buy(P(AT_RIPOSTE))) {
        b.riposte = true;
        mark(AT_RIPOSTE);
    }
    if (bern(BP_SURGE, 0.06)) {
        int n = magN(MG_SURGE, 1, MAX_COUNT_G);
        if (buy(P(AT_SURGE, n))) {
            b.surge = n;
            mark(AT_SURGE);
        }
    }
    // легендарное сопротивление: N раз за бой (в книгах чаще всего «3/день»)
    if (bern(BP_LEGRES, 0.12)) {
        int n = magN(MG_LEGRES, 1, MAX_COUNT_G);
        if (buy(P(AT_LEGRES, n))) {
            b.legRes = n;
            mark(AT_LEGRES);
        }
    }
    for (int k = 1; k < C_N; k++)
        if (bern(BP_IMMCOND, 0.08 * 8.0 / (C_N - 1)) && buy(P(AT_IMMCOND))) {
            b.immCond |= 1u << k;
            mark(AT_IMMCOND);
        }
    // владения (v21): бонус мастерства +3…+6 (за каждую единицу сверх +2); владения дальше стоят × мастерство/2
    if (bern(BP_PROF, 0.25)) {
        int pr = 3 + catN(BC_PROF, 4);
        while (pr > 2 && !buy(P(AT_PROF, pr - 2))) pr--;
        b.prof = pr;
        if (pr > 2) mark(AT_PROF);
    }
    for (int k = 0; k < SK_N; k++)
        if (skCombat(k) && bern(BP_SKILL, 0.10) && buy(P(AT_SKILLC, b.prof / 2.0))) {
            b.skillProf |= 1u << k;
            mark(AT_SKILLC);
            if (bern(BP_SKILLX, 0.20) && buy(P(AT_SKILLC, b.prof / 2.0))) b.skillExp |= 1u << k;
        }
    for (int st = 0; st < STAT_N; st++)
        if (bern(BP_SAVEPROF, 0.06) && buy(P(AT_SAVEPROF, b.prof / 2.0))) {
            b.saveProf |= 1u << st;
            mark(AT_SAVEPROF);
        }
    if (bern(BP_FLY, 0.12) && buy(P(AT_FLY))) {
        b.fly = true;
        mark(AT_FLY);
    }
    // «когда → что» (v21): до 4 правил; эффект, затем подходящее к нему условие (резист и спасброски — только условия о
    // себе); «атаки по мне с преимуществом» возвращает очки
    for (int k = 0; k < 4 && bern(BP_RULE, k == 0 ? 0.45 : 0.30); k++) {
        Rule r;
        r.what = bern(BP_RUNEW, 0.12) ? E_SKILLADV + catN(BC_RUNEW, 3) : catN(BC_RUWHAT, std::min((int)E_N, 16));
        if (r.what == E_SKILLADV) r.n = irand(SK_N);
        if (r.what == E_ACMINUS) r.n = magN(MG_ACMIN, 1, 20);
        do r.when = catN(BC_RUWHEN, W_N);
        while (!ruleAllowed(r.when, r.what));
        if (r.when == W_FOECOND) r.warg = 1 + catN(BC_RUCOND, C_N - 1);
        if (r.what == E_DMG) {
            r.n = magN(MG_RUN, 1, MAX_DICE_G);
            r.s = DICE[catN(BC_RUS, DICE_N)];
            r.once = bern(BP_RUONCE, 0.4);
        } else if (r.what == E_AC || r.what == E_SAVEB)
            r.n = magN(MG_RUN, 1, 20);   // технический предел правила: +20
        else if (r.what == E_DMGRED)
            r.n = magN(MG_RUN, 1, 1000);
        else if (r.what == E_ATKDIE || r.what == E_SAVEDIE) {   // Благословение: к4
            r.n = 1;
            r.s = DICE[catN(BC_RUS, 4)];
        } else if (r.what == E_PIERCE)
            r.n = catN(BC_RUCOND, DT_N);   // какой тип урона
        else if (r.what == E_POWER)
            r.n = magN(MG_RUN, 1, 20);
        if (!r.once && r.what != E_ADVVSME && bern(BP_RAURA, 0.15)) r.allyR = magN(MG_RAURA, 5, MAX_FT_G, 5);   // аура и на своих
        const long long pr = rulePrice(r, b, C);
        if (pr < 0) {
            spent += pr;
            passLeft -= pr;
        } else if (!buy((double)pr))
            continue;
        r.price = pr;
        b.rules.push_back(r);
        markRule(r, b.atoms);
    }


    // потолок 40 — просто защита от бесконечного цикла, реально упрётся в бюджет раньше
    for (int i = 0; i < 40 && passLeft >= 3; i++) {
        // шанс остановиться раньше, чтобы не у всех был максимум способностей
        if (i > 0 && bern(BP_ABSTOP, 0.15)) break;
        Spec s = randSpec(i);
        for (int t = 0; t < 8; t++) {
            s.price = specPrice(s, C);
            if (s.price <= passLeft) break;
            if (!s.more.empty())
                s.more.pop_back();
            else if (s.dmgN > 1)
                s.dmgN = (s.dmgN + 1) / 2;
            else if (s.cond) {
                s.cond = 0;
                s.condRounds = 1;
                if (s.dmgN == 0) s.dmgN = 1;
            } else if (s.dotN) {
                s.dotN = 0;
                s.dotRounds = 0;
                if (s.dmgN == 0) s.dmgN = 1;
            } else if (s.collN)
                s.collN = 0;
            else if (s.pushFt)
                s.pushFt = 0;
            else if (s.wallLen > 1)
                s.wallLen = (s.wallLen + 1) / 2;
            else if (s.wallRounds > 1)
                s.wallRounds--;
            else
                break;
        }
        s.price = specPrice(s, C);
        long long extra = 0;
        int newL = 0, newN = 0;
        // способность за ячейку: уровень ячейки по силе способности; пул ячеек этого уровня покупается один раз
        if (s.uses >= UNL && !s.recharge && bern(BP_SLOT, 0.25)) {
            int lvl = std::clamp(1 + (int)(s.price / (0.04 * budget)), 1, 9);
            Spec t = s;
            t.slotLvl = lvl;
            long long p = specPrice(t, C), need = 0, n = 0;
            if (b.slots[lvl] == 0) {
                n = lvl <= 3 ? 1 + irand(3) : (lvl <= 6 ? 1 + irand(2) : 1);
                need = (long long)std::ceil(C[AT_SLOT] * lvl * n);
                newL = lvl;
                newN = (int)n;
            }
            if (p + need <= passLeft) {
                s = t;
                s.price = p;
                extra = need;
            } else
                newL = 0;
        }
        const long long fee = abilFee(C, b.abil.size() + 1) - abilFee(C, b.abil.size());   // наценка за очередную способность
        if (s.price + extra + fee <= passLeft && buy((double)(s.price + extra + fee))) {
            b.abil.push_back(s);
            g_bstate.nAbil = (int)b.abil.size();
            markSpec(s, b.atoms);
            if (fee > 0) mark(AT_NABIL);
            if (newL && extra > 0) {
                b.slots[newL] = newN;
                mark(AT_SLOT);
            }
        }
    }

    // пробный компонент (замер лестниц)
    if (g_probe.tier == TR_NABIL) {   // замер лестницы числа способностей: n одноразовых способностей с состоянием (как у сильного Сборщика)
        static const int CK[8] = {C_PARAL, C_STUN, C_FRIGHT, C_RESTR, C_SLOW, C_INCAP, C_CHARM, C_BLIND};
        for (int i = 0; i < (int)g_probe.n; i++) {
            Spec ps;
            ps.delivery = 1;
            ps.range = 60;
            ps.cond = CK[i % 8];
            ps.condRounds = 3;
            ps.uses = 1;
            ps.bonus = (i % 2) == 1;
            ps.saveStat = meaningSave(ps);
            ps.price = specPrice(ps, C);
            const long long cost = ps.price + abilFee(C, b.abil.size() + 1) - abilFee(C, b.abil.size());
            if (spent + cost > (long long)(0.85 * budget)) {
                g_probe.fail = true;
                break;
            }
            spent += cost;
            b.abil.push_back(ps);
            markSpec(ps, b.atoms);
            if (b.abil.size() > (size_t)ABIL_FREE) mark(AT_NABIL);
        }
    } else if (g_probe.tier >= 0 && g_probe.tier != TR_ATT) {
        const int n = (int)g_probe.n;
        Spec ps;
        ps.delivery = 1;
        ps.saveStat = DEX;
        ps.range = 30;
        ps.dmgN = 4;
        ps.dmgS = 6;
        ps.dtype = DT_FIRE;
        bool isSpec = true;
        switch (g_probe.tier) {
            case TR_RANGE:
                ps.range = 5 + 5 * n;
                break;
            case TR_AREA:
                ps.radius = 5 * n;
                ps.range = 60;
                break;
            case TR_PUSH:
                ps.dmgN = 2;
                ps.saveStat = STR;
                ps.pushFt = 5 * n;
                break;
            case TR_TELE:
                ps = Spec();
                ps.self = true;
                ps.delivery = 2;
                ps.range = 0;
                ps.teleFt = 10 * n;
                break;
            case TR_DMG:
                ps.dmgN = n;
                break;
            case TR_TEMP:
                ps = Spec();
                ps.self = true;
                ps.delivery = 2;
                ps.range = 0;
                ps.tempN = n;
                break;
            case TR_ACB:
                ps = Spec();
                ps.self = true;
                ps.delivery = 2;
                ps.range = 0;
                ps.acB = n;
                ps.acRounds = 5;
                break;
            default:
                isSpec = false;
        }
        if (isSpec) {
            ps.price = specPrice(ps, C);
            if (spent + ps.price > (long long)(0.85 * budget))
                g_probe.fail = true;
            else {
                spent += ps.price;
                b.abil.push_back(ps);
                markSpec(ps, b.atoms);
            }
        } else if (g_probe.tier == TR_SPEED) {
            if (b.speed > 0) spent -= (long long)std::ceil(P(AT_SPEED, speedUnits(b.speed)));
            long long pr = (long long)std::ceil(P(AT_SPEED, tierN(TR_SPEED, n)));
            if (spent + pr > (long long)(0.85 * budget))
                g_probe.fail = true;
            else {
                spent += pr;
                b.speed = 5 * n;
                mark(AT_SPEED);
            }
            // радиус ауры / кубики урона ауры
        } else {
            if (b.auraRad > 0) spent -= auraPrice(b, C);
            b.auraRad = g_probe.tier == TR_AURARAD ? 5 * (n + 1) : 10;
            b.auraN = g_probe.tier == TR_AURADICE ? n : 1;
            b.auraS = 6;
            b.auraType = DT_FIRE;
            b.auraCond = 0;
            b.auraRounds = 1;
            long long pr = auraPrice(b, C);
            if (spent + pr > (long long)(0.85 * budget)) {
                g_probe.fail = true;
                b.auraRad = 0;
                b.auraN = 0;
            } else {
                spent += pr;
                markAura(b, b.atoms);
            }
        }
    }
    // призыв — способность (v21): босс выделяет очки на приспешников (alloc), собирает ОДИН шаблон тем же генератором, что
    // и боссов, и покупает способность «призвать k копий» с ценой способности (действие или бонусное, применения,
    // концентрация). Шаблон стоит полную цену за каждую копию и каждое применение (без лимита — как C[AT_SUMUNL]
    // применений). Число копий всего (n) и доля за одно применение (k = доля·n) — те же параметры политики сборки
    if (allowSummon && b.abil.size() < 12 && (force || bern(BP_SUMMON, 0.5))) {
        double alloc = budget * (0.08 + 0.37 * warp(BW_SUMALLOC, rand01()));
        int n = magN(MG_SUMN, 1, 20);
        const double minB = 40;
        auto tb = [&](int nn) { return alloc / (minMult(C) * nn); };
        while (n > 1 && tb(n) < minB) n--;
        double mb = tb(n);
        if (mb >= minB) {
            Boss mn = makeBoss((long long)mb, C, false);
            const int k = 1 + catN(BC_SUMK, n);
            Spec ss;
            ss.self = true;
            ss.delivery = 2;
            ss.range = 0;
            ss.sumN = k;
            ss.sumTmpl = (int)b.tmpl.size();
            ss.sumCost = mn.cost;
            ss.uses = std::max(1, (n + k - 1) / k);
            Boss mn2;   // разные приспешники (v21): в четверти призывов — вторая группа другого шаблона (вдвое дешевле)
            const bool mix = bern(BP_SUMMIX, 0.25) && mb / 2 >= minB;
            if (mix) {
                mn2 = makeBoss((long long)(mb / 2), C, false);
                ss.sumMore.push_back({(long long)b.tmpl.size() + 1, 1, mn2.cost});
            }
            if (bern(BP_SUMCONC, 0.25)) ss.conc = true;     // призыв на концентрации: дешевле, но приспешники исчезают с ней
            if (bern(BP_SUMBONUS, 0.15)) ss.bonus = true;    // бонусным действием
            if (bern(BP_SUMUNL, 0.15)) ss.uses = UNL;      // без лимита применений
            ss.price = specPrice(ss, C);
            const long long fee = abilFee(C, b.abil.size() + 1) - abilFee(C, b.abil.size());
            // не влезает в бюджет — призыва нет; под приспешников нельзя отдать резерв на характеристики и ХП
            if (spent + ss.price + fee <= std::min((long long)(0.95 * budget), budget - RESERVE)) {
                spent += ss.price + fee;
                b.tmpl.push_back(mn);
                if (mix) b.tmpl.push_back(mn2);
                b.abil.push_back(ss);
                g_bstate.nAbil = (int)b.abil.size();
                markSpec(ss, b.atoms);
                if (fee > 0) mark(AT_NABIL);
                long long rf = (long long)std::ceil(P(AT_REDIRECT));
                if (bern(BP_REDIRECT, 0.35) && spent + rf <= std::min((long long)(0.95 * budget), budget - RESERVE)) {
                    spent += rf;
                    b.redirect = true;
                    mark(AT_REDIRECT);
                }
            }
        }
    }
    // превращение (v21): облик — шаблон, собранный тем же генератором на долю бюджета; способность «превратиться»
    // (действием или бонусным, 1–3 раза за бой) стоит облик × применения × C[AT_FORM]
    if (allowSummon && b.abil.size() < 12 && bern(BP_FORM, 0.03)) {
        const double fb = budget * (0.2 + 0.3 * rand01());
        if (fb >= 40) {
            Boss mn = makeBoss((long long)fb, C, false);
            Spec fs;
            fs.self = true;
            fs.delivery = 2;
            fs.range = 0;
            fs.formTmpl = (int)b.tmpl.size();
            fs.sumCost = mn.cost;
            fs.uses = magN(MG_FORMU, 1, 10);
            if (bern(BP_FORMBONUS, 0.3)) fs.bonus = true;
            fs.price = specPrice(fs, C);
            const long long fee = abilFee(C, b.abil.size() + 1) - abilFee(C, b.abil.size());
            if (spent + fs.price + fee <= std::min((long long)(0.95 * budget), budget - RESERVE)) {
                spent += fs.price + fee;
                b.tmpl.push_back(mn);
                b.abil.push_back(fs);
                g_bstate.nAbil = (int)b.abil.size();
                markSpec(fs, b.atoms);
                if (fee > 0) mark(AT_NABIL);
            }
        }
    }

    long long remain = std::max(0LL, budget - spent - RESERVE);   // резерв под минимальные характеристики и ХП

    // оружие: основное + запасное (арсенал); кубики без предела, число атак свободное
    {
        // Парирование и контратака требуют ближнего оружия
        Wp p = buildWeapon(C, (double)(long long)(remain * 0.5 * warp(BW_WPN, rand01())), b.parry > 0 || b.riposte);
        // на такое число атак не хватило бюджета оружия
        if (g_probe.tier == TR_ATT && p.att != (int)g_probe.n + 1) g_probe.fail = true;
        if (p.n >= 1) {
            b.wN = p.n;
            b.wS = p.s;
            b.wRange = p.range;
            b.wAtt = p.att;
            b.reach = p.reach;
            b.wFinesse = p.finesse;
            b.wAmmo = p.ammo;
            b.wType = p.type;
            b.wcost = p.cost;
            markWp(p, b.atoms);
        } else {
            b.wN = 0;
            b.wS = 0;
            b.wAtt = 1;
            b.wRange = 5;
            b.reach = 5;
            b.wFinesse = false;
            b.wAmmo = false;
            b.wcost = 0;
        }
    }
    // без владения своим оружием (v21): мастерство не идёт к атакам оружием, очки возвращаются
    if (b.wN > 0 && bern(BP_NOWPROF, 0.05)) {
        const long long ref = (long long)std::ceil(P(AT_NOWPROF, b.prof / 2.0));
        b.wNoProf = true;
        spent -= ref;
        remain += ref;
        mark(AT_NOWPROF);
    }
    // арсенал: 1–2 запасных вида оружия, бот выбирает по ситуации
    if (b.wN > 0 && bern(BP_ARSENAL, 0.20)) {
        int na = 1 + (rand01() < 0.3);
        for (int a = 0; a < na; a++) {
            long long left = remain - b.wcost - b.acost;
            Wp q = buildWeapon(C, (double)(long long)(left * 0.3 * rand01()), false);
            if (q.n >= 1 && b.acost + q.cost <= (long long)(0.4 * (remain - b.wcost))) {
                q.multi = bern(BP_MULTIWPN, 0.5);   // укус + когти: в бестиарии — самая частая форма мультиатаки
                b.alt.push_back(q);
                b.acost += q.cost;
                markWp(q, b.atoms);
            }
        }
    }
    // довесок к удару: только урон / только состояние / оба
    if (b.wN > 0 && bern(BP_RIDER, 0.25)) {
        double r = rand01();
        bool dm = r < 0.7, cd = r > 0.5;
        if (dm) {
            b.rN = magN(MG_RN, 1, MAX_DICE_G);
            b.rS = DICE[catN(BC_RS, DICE_N)];
            b.rType = pickAbilityType();
        }
        if (cd) {
            b.rCond = 1 + catN(BC_RCOND, C_N - 1);
            b.rRounds = b.rCond == C_PRONE ? 1 : magN(MG_RR, 1, MAX_ROUNDS_G);
            if (b.rCond == C_GRAPPLE && bern(BP_GRABRESTR, 0.4)) b.rGrabRestr = true;   // пакет атомов 2
            if (b.rCond == C_GRAPPLE && bern(BP_GRABDOT, 0.2)) { b.rGrabDotN = magN(MG_DOTN, 1, MAX_DICE_G); b.rGrabDotS = DICE[catN(BC_DOTS, DICE_N)]; }
        }
        b.rcost = riderPrice(b, C);
        if (b.rcost <= 0 || b.rcost > (long long)(0.4 * (remain - b.wcost))) {
            b.rN = 0;
            b.rS = 0;
            b.rCond = 0;
            b.rRounds = 1;
            b.rcost = 0;
        } else
            markRider(b, b.atoms);
    }

    // оружие и Зуро (v21): лёгкое ближнее оружие и атака второй рукой; дальняя дистанция; запас костей с приёмами
    // «при попадании»; удар «при попадании» без запаса (кара) с уроном себе и лечением, если цель умерла
    if (b.wN > 0) {
        long long avail = remain - b.wcost - b.acost - b.rcost;
        auto addSpec = [&](Spec sp) {
            if (b.abil.size() >= 12) return false;
            sp.price = specPrice(sp, C);
            const long long fee = abilFee(C, b.abil.size() + 1) - abilFee(C, b.abil.size());
            if (sp.price + fee > avail / 3) return false;
            avail -= sp.price + fee;
            remain -= sp.price + fee;
            spent += sp.price + fee;
            b.abil.push_back(sp);
            g_bstate.nAbil = (int)b.abil.size();
            markSpec(sp, b.atoms);
            if (fee > 0) mark(AT_NABIL);
            return true;
        };
        // доспехи и щит (v21): двуручное (обе руки) или универсальное (кость больше двумя руками) ближнее оружие
        if (b.wRange <= 5 && bern(BP_TWOH, 0.20)) b.wTwoH = true;
        if (b.wRange <= 5 && !b.wTwoH && bern(BP_VERS, 0.25)) {
            int k = 0;
            while (k < DICE_N - 1 && DICE[k] <= b.wS) k++;
            if (DICE[k] > b.wS) {
                b.wVers = DICE[k];
                const long long vp = weaponHandsPrice(b, C);
                if (vp > avail / 4) b.wVers = 0;
            }
        }
        if (b.wTwoH) {
            const long long r2 = -weaponHandsPrice(b, C);   // возврат
            spent -= r2;
            remain += r2;
            avail += r2;
        } else if (b.wVers) {
            const long long vp = weaponHandsPrice(b, C);
            b.wcost += vp;
            avail -= vp;
        }
        if (b.wRange <= 5 && bern(BP_WHEAVY, b.wTwoH ? 0.6 : 0.1)) b.wHeavy = true;   // тяжёлое (v21)
        if (b.wRange <= 5 && !b.wTwoH && !b.wHeavy && bern(BP_WLIGHT, 0.30)) b.wLight = true;
        if (b.wRange > 5 && bern(BP_WLONG, 0.50)) {
            const int lng = magN(MG_WLONG, std::min(b.wRange + 5, MAX_FT_G), MAX_FT_G, 5);
            const long long lp = weaponLongPrice(C, b.wN, b.wS, b.wRange, lng, b.reach, b.wAtt);
            if (lp <= avail / 4) {
                b.wLong = lng;
                b.wcost += lp;
                avail -= lp;
                mark(AT_WLONG);
            }
        }
        if (b.wLight && bern(BP_TWF, 0.35)) {
            Spec o;
            o.offhand = o.bonus = true;
            o.range = 5;
            o.delivery = 0;
            o.dmgN = b.wN;
            o.dmgS = b.wS;
            o.dtype = b.wType;
            o.twfMod = bern(BP_TWFMOD, 0.5);
            addSpec(o);
        }
        if (bern(BP_POOL, 0.15)) {
            b.poolN = magN(MG_POOLN, 1, MAX_COUNT_G);
            b.poolS = DICE[catN(BC_POOLS, DICE_N)];
            int made = 0;
            for (int k = 0, n = 1 + catN(BC_MANK, 3); k < n; k++) {
                Spec m;
                m.onHit = m.pool = true;
                m.poolN = b.poolN;
                m.poolS = b.poolS;
                m.range = 5;
                m.delivery = bern(BP_MANSAVE, 0.5) ? 1 : 0;
                const int e = catN(BC_MANEFF, 4);
                if (e == 1) {
                    m.cond = 1 + catN(BC_COND, C_N - 1);
                    m.condRounds = 1;
                } else if (e == 2)
                    m.pushFt = magN(MG_PUSH, 5, MAX_FT_G, 5);
                else if (e == 3)
                    m.disarm = true;
                m.saveStat = m.disarm ? STR : meaningSave(m);
                if (m.delivery == 1 && !m.cond && !m.pushFt && !m.disarm) m.delivery = 0;   // спасаться не от чего
                made += addSpec(m);
            }
            if (!made) b.poolN = 0;
        }
        if (bern(BP_SMITE, 0.10)) {
            Spec m;
            m.onHit = true;
            m.range = 5;
            m.delivery = 0;
            m.dmgN = magN(MG_DMGN, 1, MAX_DICE_G);
            m.dmgS = DICE[catN(BC_DMGS, DICE_N)];
            m.dtype = pickAbilityType();
            const int u = catP(BC_USES, 4, USES_CUM);
            m.uses = u == 3 ? UNL : magN(MG_USES, 1, MAX_COUNT_G);
            if (bern(BP_SELFDMG, 0.3)) {
                m.selfN = magN(MG_SELFN, 1, MAX_DICE_G);
                m.selfS = DICE[catN(BC_DOTS, DICE_N)];
            }
            if (bern(BP_KILLHEAL, 0.3)) {
                m.killN = magN(MG_HEALN, 1, MAX_DICE_G);
                m.killS = DICE[catN(BC_HEALS, DICE_N)];
            }
            addSpec(m);
        }
        // союзники (v21): реакция за союзника рядом — помеха атаке по нему или удар оружием по атакующему
        if (bern(BP_PROTECT, 0.06) || bern(BP_STRIKE, 0.06)) {
            Spec p;
            p.self = true;
            p.react = 5;
            p.range = 5;
            p.uses = UNL;
            if (bern(BP_STRIKE, 0.5)) p.strike = true;
            else p.protect = true;
            addSpec(p);
        }
    }
    // приказ союзнику атаковать (v21): имеет смысл с приспешниками
    if (hasSummonSpec(b) && bern(BP_CMDATK, 0.25)) {
        Spec c;
        c.self = c.ally = c.cmdAtk = true;
        c.bonus = true;
        c.range = 30;
        c.uses = UNL;
        c.price = specPrice(c, C);
        const long long fee = abilFee(C, b.abil.size() + 1) - abilFee(C, b.abil.size());
        if (b.abil.size() < 12 && c.price + fee <= (remain - b.wcost - b.acost - b.rcost) / 4) {
            remain -= c.price + fee;
            spent += c.price + fee;
            b.abil.push_back(c);
            g_bstate.nAbil = (int)b.abil.size();
            markSpec(c, b.atoms);
            if (fee > 0) mark(AT_NABIL);
        }
    }
    // «событие → эффект» (v21): до двух; событие, подходящий эффект, применений за бой
    for (int k = 0; k < 2 && bern(BP_TRIG, k == 0 ? 0.30 : 0.20); k++) {
        Trig t;
        t.on = bern(BP_TRIGDMGT, 0.08) ? T_DMGTYPE : (bern(BP_TRIGCHARGE, 0.06) ? T_CHARGE : catN(BC_TRIGON, 12));   // в слоте 12 вариантов
        if (t.on == T_DMGTYPE) t.arg = pickAbilityType();   // тип урона, на который срабатывает
        bool any = false;   // у события может не быть разрешённых эффектов (например, «после Рывка» пока выключено)
        for (int x = 0; x < X_N; x++) any = any || trigAllowed(t.on, x);
        if (!any) continue;
        do t.what = catN(BC_TRIGDO, X_N);
        while (!trigAllowed(t.on, t.what));
        t.n = magN(MG_TRIGN, 1, MAX_DICE_G);
        t.s = DICE[catN(BC_TRIGS, DICE_N)];
        if (t.what == X_COND || t.what == X_SELFCOND) t.cond = 1 + catN(BC_COND, C_N - 1);
        if (bern(BP_TRIGLIM, 0.5)) t.uses = magN(MG_TRIGU, 1, MAX_COUNT_G);
        if (t.what == X_STAY1) t.uses = 1;
        const long long tp = trigPrice(t, b, C);   // после оружия: атака бонусом стоит по цене удара оружием
        if (tp > (remain - b.wcost - b.acost - b.rcost) / 5) continue;
        remain -= tp;
        spent += tp;
        b.trigs.push_back(t);
        markTrig(t, b.atoms);
    }
    long long rest = remain - b.wcost - b.acost - b.rcost;
    const long long restFull = rest + RESERVE;   // то, что делится на характеристики, КД и ХП (резерв возвращается)
    const int mains[5] = {STR, DEX, INT, WIS, CHA};
    b.mainStat = mains[catN(BC_MAIN, 5)];
    for (auto& sp : b.abil)
        if (sp.sbAmt) {
            double q = rand01();
            sp.sbStat = q < 0.45
                            ? b.mainStat
                            : (q < 0.70 ? ((b.wAmmo || b.wFinesse) ? DEX : STR) : (q < 0.85 ? CON : irand(STAT_N)));
        }
    std::array<double, STAT_N> w{};
    for (int s2 = 0; s2 < STAT_N; s2++) w[s2] = warp(BC_STAT + s2, rand01()) + 0.1;
    w[b.mainStat] += 1.0;
    double wsum = 0;
    for (double x : w) wsum += x;
    long long statBudget = (long long)(rest * (0.35 + 0.25 * warp(BW_STAT, rand01())));
    long long acBudget = (long long)(rest * (0.10 + 0.15 * warp(BW_AC, rand01())));
    long long statSpent = 0;
    for (int i = 0; i < STAT_N; i++) {
        // характеристика от 1 до STAT_MAX (0 — недееспособное существо); остаток бюджета уходит в ХП
        b.stats[i] = std::clamp(maxV((long long)(statBudget * w[i] / wsum)), 1, STAT_MAX);
        statSpent += tri(b.stats[i]);
    }
    // КД растёт с той же скоростью, что и бонус атаки (иначе цены плывут с бюджетом)
    b.acV = std::min(maxV(acBudget) / 2, AC_MAX - 10);   // столько же КД, сколько давала прежняя покупка на эти очки
    // доспехи и щит (v21): вид доспеха (тяжёлый — при СИЛ от 13), щит — если обе руки не заняты
    b.armor = catN(BC_ARMOR, AR_N);
    if (b.armor == AR_NONE && bern(BP_UNARM, 0.20)) {   // пакет атомов 3: защита без доспеха
        b.unarmoredStat = bern(BP_UNARMSTAT, 0.5) ? WIS : CON;
        mark(AT_UNARMORED);
    }
    if (b.armor == AR_HEAVY && b.stats[STR] < 13) b.armor = AR_MEDIUM;
    b.armStealth = b.armor == AR_HEAVY || (b.armor == AR_MEDIUM && bern(BP_ARMSTEALTH, 0.3));
    b.shield = !(b.wN > 0 && b.wTwoH) && !hasOffhand(b) && bern(BP_SHIELD, 0.25);
    b.noArmProf = (b.armor || b.shield) && bern(BP_NOARMPROF, 0.05);
    if (bern(BP_SIZE, 0.30)) b.size = catN(BC_SIZE, 6);   // размер (v21): чаще средний
    if (restFull - statSpent - acCost(0) - armorPrice(b, C) - sizePrice(b, C) < 2) b.size = 2;
    while (b.shield && restFull - statSpent - acCost(b.acV) - armorPrice(b, C) - sizePrice(b, C) < 2 && b.acV == 0) b.shield = false;
    while (b.acV > 0 && restFull - statSpent - acCost(b.acV) - armorPrice(b, C) - sizePrice(b, C) < 2) b.acV--;   // на копеечных бюджетах минимальные характеристики дороже КД
    if (restFull - statSpent - acCost(b.acV) - armorPrice(b, C) - sizePrice(b, C) < 2) b.armor = AR_NONE, b.shield = b.armStealth = b.noArmProf = false;
    b.ac = calcAC(b);
    markArmor(b, b.atoms);
    // Пересчёт цен по готовому телу (после теста 4 ноября): доп. атаки, бафф оружия, бонус КД и концентрация зависят от
    // характеристик и КД, а они покупаются последними. Разница — из хитов; не хватает — снимаем КД, затем характеристики.
    const PriceCtx pcPrev = g_pc;
    long long dPrice = 0;   // насколько подорожало купленное до характеристик
    auto reprice = [&]() {
        setPriceCtx(b);
        if (b.wN > 0) {
            long long w = wpCostOf(C, b.wN, b.wS, b.wRange, b.reach, b.wAtt, b.wFinesse, b.wAmmo);
            if (b.wVers > b.wS && !b.wTwoH) w += weaponHandsPrice(b, C);
            if (b.wLong > b.wRange) w += weaponLongPrice(C, b.wN, b.wS, b.wRange, b.wLong, b.reach, b.wAtt);
            dPrice += w - b.wcost;
            b.wcost = w;
        }
        long long a = 0;
        for (auto& q : b.alt) a += (q.cost = wpPrice(q, C));
        dPrice += a - b.acost;
        b.acost = a;
        for (auto& sp : b.abil) {
            const long long np = specPrice(sp, C);
            dPrice += np - sp.price;
            spent += np - sp.price;
            sp.price = np;
        }
        for (auto& r : b.rules) {
            const long long np = rulePrice(r, b, C);
            dPrice += np - r.price;
            spent += np - r.price;
            r.price = np;
        }
    };
    // хиты не ниже половины прежних: сначала снимаем лишние атаки (их цена выросла сильнее всего), потом КД, потом характеристики
    const long long lo0 = restFull - statSpent - acCost(b.acV) - armorPrice(b, C) - sizePrice(b, C), need = std::max(2LL, lo0 / 2);
    reprice();
    for (int guard = 0; guard < 400 && restFull - dPrice - statSpent - acCost(b.acV) - armorPrice(b, C) - sizePrice(b, C) < need; guard++) {
        if (b.wN > 0 && b.wAtt > 1) b.wAtt = std::max(1, b.wAtt * 3 / 4);
        else if (b.acV > 0) b.acV--;
        else {
            int hi = 0;
            for (int i = 1; i < STAT_N; i++) if (b.stats[i] > b.stats[hi]) hi = i;
            if (b.stats[hi] <= 1) break;
            statSpent -= tri(b.stats[hi]) - tri(b.stats[hi] - 1);
            b.stats[hi]--;
        }
        b.ac = calcAC(b);
        reprice();
    }
    g_pc = pcPrev;
    {   // атак могло стать меньше: метка «доп. атака» — по содержимому
        bool many = b.wN > 0 && b.wAtt >= 2;
        for (auto& q : b.alt) many = many || q.att >= 2;
        if (!many) b.atoms.w[AT_WPNATT >> 6] &= ~(1ULL << (AT_WPNATT & 63));
    }
    // ИНТ<=3: взять в руки оружие нечем — только укус и когти
    if (b.wN > 0 && b.stats[INT] <= 3) b.wNatural = true;
    long long leftover = restFull - dPrice - statSpent - acCost(b.acV) - armorPrice(b, C) - sizePrice(b, C);
    b.hp = std::max(1, (int)(leftover / 2));
    b.cost = spent + b.wcost + b.acost + b.rcost + statSpent + acCost(b.acV) + armorPrice(b, C) + sizePrice(b, C) + 2LL * b.hp;
    // подавление осталось только если есть что подавлять
    if (b.suppMask) {
        bool has = ((b.suppTgt & 1) && b.regenN) || ((b.suppTgt & 2) && b.auraRad > 0) ||
                   ((b.suppTgt & 4) && (b.resMask | b.immMask || b.resNM || b.immNM || hasResRule(b)));
        if (has)
            mark(AT_SUPP);
        else {
            b.suppMask = 0;
            b.suppTgt = 0;
        }
    }
    g_B = prevB;
    return b;
}
Boss makeBoss(long long budget, const Coef& C, bool allowSummon, bool force) {
    Boss b;
    for (int t = 0; t < 40; t++) {
        b = makeBossOnce(budget, C, allowSummon, force);
        if (!force || !allowSummon || hasSummonSpec(b)) break;
    }
    return b;
}
// Пара соперников: один (случайно выбранный) босс ОБЯЗАТЕЛЬНО с миньонами, второй — по желанию (50%)
void makePair(long long B, const Coef& C, Boss& A, Boss& Bo) {
    int fs = irand(2);
    Boss f = makeBoss(B, C, true, true), o = makeBoss(B, C, true, false);
    if (fs == 0) {
        A = std::move(f);
        Bo = std::move(o);
    } else {
        A = std::move(o);
        Bo = std::move(f);
    }
}
