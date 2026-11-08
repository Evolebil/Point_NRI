// Проверки движка, часть 2 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    if (TB()) {
        Fighter t = mk(plain(100, 0, 0, 5), 0, 1, 1);
        t.b.stats = {10, 10, 10, 10, 10, 10};
        double p0 = pFailEst(t, CON, 12);
        t.lrLeft = 3;
        double p1 = pFailEst(t, CON, 12);
        t.lrLeft = 0;
        double p2 = pFailEst(t, CON, 12);
        T("T39", "§LR ИИ учитывает заряды цели: шанс провала спасброска ×0.25 при наличии зарядов",
          std::fabs(p1 - p0 * 0.25) < 1e-9 && p2 == p0, fmt("без зарядов %.2f, с зарядами %.2f", p0, p1));
    }
    if (TB()) {
        Coef C2 = CC();
        rng.seed(55);
        long long u0 = g_lrUsed;
        int fights = 0, had = 0;
        for (int i = 0; i < 3000; i++) {
            Boss A = makeBoss(800, C2, true);
            A.legRes = 3;
            Boss B = makeBoss(800, C2, true);
            fight(A, B);
            fights++;
        }
        double per = (double)(g_lrUsed - u0) / fights;
        T("T40", "(инфо) среднее число потраченных зарядов за бой при LR 3 (от всех спасбросков обеих сторон)",
          per >= 0, fmt("%.2f зарядов за бой на 3000 боях (максимум 3 у A плюс случайные у B)", per));
    }

    // T41..T45 перезарядка
    if (TB()) {
        auto rate = [&](int thr) {
            Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
            Spec sp;
            sp.recharge = thr;
            sp.dmgN = 1;
            sp.dmgS = 6;
            sp.delivery = 2;
            f.b.abil.push_back(sp);
            long long r0 = g_rechRolls, o0 = g_rechOk;
            for (int i = 0; i < 120000; i++) {
                f.uses.assign(1, 0);
                rechargeStart(f);
            }
            return (double)(g_rechOk - o0) / (g_rechRolls - r0);
        };
        double r6 = rate(6), r5 = rate(5), r4 = rate(4);
        T("T41", "§перезарядка: шанс вернуть заряд за ход 6→1/6, 5–6→1/3, 4–6→1/2",
          std::fabs(r6 - 1 / 6.0) < 0.01 && std::fabs(r5 - 1 / 3.0) < 0.01 && std::fabs(r4 - 0.5) < 0.01,
          fmt("6: %.3f, 5–6: %.3f, 4–6: %.3f", r6, r5, r4));
    }
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        Spec a;
        a.recharge = 5;
        a.delivery = 2;
        a.dmgN = 1;
        Spec b2;
        b2.recharge = 0;
        b2.uses = 0;
        b2.delivery = 2;
        b2.dmgN = 1;
        f.b.abil = {a, b2};
        f.uses = {1, 0};
        long long r0 = g_rechRolls;
        for (int i = 0; i < 1000; i++) rechargeStart(f);
        T("T42", "§перезарядка: заряженная способность и способность без перезарядки не бросаются, не меняются",
          f.uses[0] == 1 && f.uses[1] == 0 && g_rechRolls == r0,
          fmt("бросков %lld, заряд %d, обычная %d", g_rechRolls - r0, f.uses[0], f.uses[1]));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 6;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        long long pu = specPrice(s, C2);
        Spec r6 = s, r5 = s, r4 = s, once = s;
        r6.recharge = 6;
        r5.recharge = 5;
        r4.recharge = 4;
        once.uses = 1;
        long long p6 = specPrice(r6, C2), p5 = specPrice(r5, C2), p4 = specPrice(r4, C2), p1 = specPrice(once, C2);
        T("T43", "§перезарядка: цена растёт с шансом: «раз за бой» < 6 < 5–6 < 4–6 < без ограничения",
          p1 <= p6 && p6 < p5 && p5 < p4 && p4 < pu,
          fmt("раз за бой %lld; 6: %lld; 5–6: %lld; 4–6: %lld; без ограничения %lld", p1, p6, p5, p4, pu));
    }
    if (TB()) {
        int bad = 0, rc = 0, cnt = 0, N = 40000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            cnt++;
            if (s.recharge) {
                rc++;
                // перезарядка после траты применений (пакет атомов 3) бывает и при лимите применений
                if ((s.self && !s.rechAfter) || (s.uses < UNL && !s.rechAfter) || (s.recharge < 4 || s.recharge > 6)) bad++;
            }
        }
        T("T44", "§перезарядка: только у способностей не «на себя», без лимита применений (или «после траты применений»), порог 4..6", bad == 0,
          fmt("нарушений %d; перезарядка у %.1f%% способностей", bad, 100.0 * rc / cnt));
    }
    if (TB()) {
        auto usesPerFight = [&](int thr) {
            Coef C2 = CC();
            rng.seed(77 + thr);
            long long u0 = g_rechUsed;
            int N = 2500;
            double len = 0;
            for (int i = 0; i < N; i++) {
                Boss A = makeBoss(800, C2, false);
                A.abil.clear();
                Spec sp;
                sp.delivery = 2;
                sp.range = 60;
                sp.dmgN = 3;
                sp.dmgS = 6;
                sp.recharge = thr;
                sp.dtype = DT_FIRE;
                A.abil.push_back(sp);
                Boss B = makeBoss(800, C2, false);
                fight(A, B);
            }
            return (double)(g_rechUsed - u0) / N;
        };
        double u6 = usesPerFight(6), u5 = usesPerFight(5), u4 = usesPerFight(4);
        T("T45", "(инфо) среднее число применений способности с перезарядкой за бой (оба бойца)", u6 < u5 && u5 < u4,
          fmt("6: %.2f; 5–6: %.2f; 4–6: %.2f", u6, u5, u4));
    }

    // T46..T49 легендарные действия
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.wAtt = 2;
        a.legAct = 3;
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        long long a0 = g_attacksMade;
        bool r1 = legendaryTurn(w, 0);
        long long n1 = g_attacksMade - a0;
        int l1 = w.f[0].legLeft;
        a0 = g_attacksMade;
        bool r2 = legendaryTurn(w, 0);
        long long n2 = g_attacksMade - a0;
        int l2 = w.f[0].legLeft;
        a0 = g_attacksMade;
        bool r3 = legendaryTurn(w, 0);
        T("T46",
          "§LA 3 очка, 2 атаки: сначала полное действие Атака (2 очка, 2 атаки), потом одна атака (1 очко), потом "
          "ничего",
          r1 && n1 == 2 && l1 == 1 && r2 && n2 == 1 && l2 == 0 && !r3 && g_attacksMade - a0 == 0,
          fmt("1-й: атак %lld, осталось %d; 2-й: атак %lld, осталось %d; 3-й: %d", n1, l1, n2, l2, r3));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.wAtt = 2;
        a.legAct = 1;
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        long long a0 = g_attacksMade;
        legendaryTurn(w, 0);
        long long n1 = g_attacksMade - a0;
        w.f[0].legLeft = 3;
        w.f[0].cond[C_STUN] = 1;
        bool st = legendaryTurn(w, 0);
        w.f[0].cond[C_STUN] = 0;
        w.f[0].legLeft = 0;
        bool no = legendaryTurn(w, 0);
        T("T47", "§LA 1 очко — одна атака; оглушённый и без очков не действует", n1 == 1 && !st && !no,
          fmt("атак при 1 очке: %lld; оглушён: %d; без очков: %d", n1, st, no));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.legAct = 3;
        Boss d = plain(100, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 12, 10));
        w.f[0].legLeft = 0;
        takeTurn(w, 0);
        T("T48", "§LA очки восстанавливаются в начале СВОЕГО хода", w.f[0].legLeft == 3,
          fmt("очков после своего хода: %d", w.f[0].legLeft));
    }
    if (TB()) {
        Coef C2 = CC();
        rng.seed(91);
        long long u0 = g_legUsed;
        long long rounds0 = g_rounds;
        int N = 2000;
        for (int i = 0; i < N; i++) {
            Boss A = makeBoss(800, C2, false);
            A.legAct = 3;
            Boss B = makeBoss(800, C2, false);
            B.legAct = 0;
            fight(A, B);
        }
        double rounds = (double)(g_rounds - rounds0);
        double per = (g_legUsed - u0) / rounds;
        T("T49",
          "(инфо) легендарных действий на раунд боя при 3 очках (теоретический максимум 1: один противник = один ход в "
          "раунд)",
          per <= 1.0001, fmt("%.2f на раунд; раундов %.0f на %d боях", per, rounds, N));
    }

    // T50..T56 новые состояния (недееспособен, парализован, очарован, схвачен)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 2, 8, 5);
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[0].cond[C_INCAP] = 3;
        int hp0 = w.f[1].hp;
        int x0 = w.f[0].x;
        takeTurn(w, 0);
        int dmgIncap = hp0 - w.f[1].hp;
        w.f[0].cond[C_INCAP] = 0;
        for (int i = 0; i < 30; i++) takeTurn(w, 0);
        bool dealsNormally = hp0 - w.f[1].hp > 0;
        World w2 = w;
        w2.f[0].x = 10;
        w2.f[0].y = 10;
        w2.f[1].x = 40;
        w2.f[1].y = 10;
        w2.f[0].cond[C_INCAP] = 3;
        w2.f[0].legLeft = 3;
        w2.f[0].b.legAct = 3;
        bool la = legendaryTurn(w2, 0);
        takeTurn(w2, 0);
        bool moved = w2.f[0].x > 10;
        T("T50", "§недееспособен: нет действий и реакций, но двигаться можно; легендарные действия недоступны",
          dmgIncap == 0 && dealsNormally && moved && !la,
          fmt("урон под состоянием: %d; без состояния бьёт: %d; двигался: %d; ЛД: %d", dmgIncap, dealsNormally, moved,
              la));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[1].cond[C_PARAL] = 2;
        int xx = w.f[1].x;
        bool adv, dis;
        advState(w, w.f[0], w.f[1], 5, adv, dis);
        int crits = 0, hits = 0;
        for (int i = 0; i < 4000; i++) {
            bool c;
            if (attackRoll(w, w.f[0], w.f[1], PROF, 5, c)) {
                hits++;
                if (c) crits++;
            }
        }
        w.f[1].x = 20;
        int far_crits = 0, far_hits = 0;
        for (int i = 0; i < 4000; i++) {
            bool c;
            if (attackRoll(w, w.f[0], w.f[1], PROF, 30, c)) {
                far_hits++;
                if (c) far_crits++;
            }
        }
        w.f[1].x = 11;
        Fighter t = mk(plain(100, 0, 0, 5), 0, 1, 1);
        t.b.stats = {30, 30, 10, 10, 10, 10};
        t.cond[C_PARAL] = 1;
        bool failStr = !saveOK(t, STR, 1), failDex = !saveOK(t, DEX, 1), okCon = saveOK(t, CON, 1);
        t.cond[C_PARAL] = 0;
        t.cond[C_STUN] = 1;
        bool stunFail = !saveOK(t, STR, 1);
        int hp0 = w.f[0].hp;
        w.f[1].cond[C_PARAL] = 2;
        takeTurn(w, 1);
        bool skipped = (w.f[0].hp == hp0) && (w.f[1].x == xx);
        bool pf = pFailEst(t, DEX, 5) == 1.0;
        T("T51",
          "§парализован: преимущество по цели, ближний удар в упор — крит, автопровал СИЛ/ЛОВ (и у оглушённого), ход "
          "пропускается",
          adv && hits > 0 && hits == crits && far_crits < far_hits * 0.2 && failStr && failDex && okCon && stunFail &&
              skipped && pf,
          fmt("преим. %d; в упор: %d из %d критов; издали: %d из %d; автопровал СИЛ %d ЛОВ %d (ТЕЛ проходит %d, "
              "оглушён %d); пропуск хода %d",
              adv, crits, hits, far_crits, far_hits, failStr, failDex, okCon, stunFail, skipped));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 2, 8, 5);
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        Boss m = plain(100000, 0, 0, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[0].cond[C_CHARM] = 100;
        w.f[0].cstat[C_CHARM] = -1;
        w.f[0].charmSrc = 1;
        int hp0 = w.f[1].hp;
        takeTurn(w, 0);
        int dmg1 = hp0 - w.f[1].hp;
        int near1 = nearestEnemyInRange(w, w.f[0], 5);
        w.f.push_back(mk(m, 1, 10, 11));
        int mhp = w.f[2].hp;
        int near2 = nearestEnemyInRange(w, w.f[0], 5);
        for (int i = 0; i < 10; i++) takeTurn(w, 0);
        bool hitsOther = mhp - w.f[2].hp > 0;
        bool noCharmer = hp0 - w.f[1].hp == 0;
        T("T52", "§очарован: не может атаковать очаровавшего (но атакует других)",
          dmg1 == 0 && near1 == -1 && near2 == 2 && hitsOther && noCharmer,
          fmt("урон очаровавшему %d, цель в досягаемости: %d (только он) → %d (с приспешником); бьёт приспешника: %d; "
              "очаровавшему всего: %d",
              dmg1, near1, near2, hitsOther, hp0 - w.f[1].hp));
    }
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        int base = speedOf(f);
        f.cond[C_GRAPPLE] = 2;
        int g = speedOf(f);
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 30);
        a.stats = {10, 30, 10, 10, 10, 10};
        Boss d = plain(1000, 1, 6, 5);
        int esc = 0, stay = 0, N = 300;
        for (int i = 0; i < N; i++) {
            World w2 = w;
            w2.f.clear();
            w2.f.push_back(mk(a, 0, 10, 10));
            w2.f.push_back(mk(d, 1, 60, 10));
            w2.f[0].cond[C_GRAPPLE] = 3;
            w2.f[0].cstat[C_GRAPPLE] = -1;
            w2.f[0].grapSrc = 1;
            w2.f[0].cdc[C_GRAPPLE] = 1;
            takeTurn(w2, 0);
            if (!w2.f[0].cond[C_GRAPPLE]) esc++;
        }
        for (int i = 0; i < N; i++) {
            World w2 = w;
            w2.f.clear();
            w2.f.push_back(mk(a, 0, 10, 10));
            w2.f.push_back(mk(d, 1, 60, 10));
            w2.f[0].cond[C_GRAPPLE] = 3;
            w2.f[0].cstat[C_GRAPPLE] = -1;
            w2.f[0].grapSrc = 1;
            w2.f[0].cdc[C_GRAPPLE] = 100;
            takeTurn(w2, 0);
            if (w2.f[0].cond[C_GRAPPLE]) stay++;
        }
        int attackedN = 0;
        for (int i = 0; i < 100; i++) {
            World w3 = w;
            w3.f.clear();
            w3.f.push_back(mk(a, 0, 10, 10));
            w3.f.push_back(mk(d, 1, 13, 10));
            w3.f[0].cond[C_GRAPPLE] = 3;
            w3.f[0].cstat[C_GRAPPLE] = -1;
            w3.f[0].grapSrc = 1;
            w3.f[0].cdc[C_GRAPPLE] = 1;
            int hp0 = w3.f[1].hp;
            takeTurn(w3, 0);
            attackedN += (w3.f[0].cond[C_GRAPPLE] > 0 && w3.f[1].hp < hp0);
        }
        bool attacksInstead = attackedN >= 80;   // попадание ~95% (нат. 1 — промах)
        T("T53",
          "§схвачен: скорость 0; если нечем атаковать — действие на побег по СЛ; если есть цель в досягаемости — "
          "атакует",
          base == 30 && g == 0 && esc >= N * 0.9 && stay == N && attacksInstead,
          fmt("скорость %d→%d; побег при СЛ 1: %d из %d; при СЛ 100: остался %d из %d; при цели в досягаемости "
              "атакует: %d из 100",
              base, g, esc, N, stay, N, attackedN));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 0, 0, 5);
        d.ac = 0;
        d.stats = {30, 30, 30, 30, 30, 30};
        Spec sp;
        sp.delivery = 0;
        sp.cond = C_GRAPPLE;
        sp.condRounds = 3;
        sp.saveStat = STR;
        sp.range = 5;
        sp.dmgN = 0;
        a.abil.push_back(sp);
        a.mainStat = STR;
        int ok = 0, N = 600;
        for (int i = 0; i < N; i++) {
            World w2 = w;
            w2.f.clear();
            w2.f.push_back(mk(a, 0, 10, 10));
            w2.f.push_back(mk(d, 1, 11, 10));
            hitOne(w2, sp, w2.f[0], w2.f[1]);
            if (w2.f[1].cond[C_GRAPPLE] > 0 && w2.f[1].grapSrc == 0 && w2.f[1].cstat[C_GRAPPLE] == -1) ok++;
        }
        T("T54",
          "§схватить: попадание без спасброска (даже у цели со СИЛ 30), источник запоминается, повторного спаса нет",
          ok >= N * 0.9, fmt("схвачено %d из %d попаданий-кандидатов", ok, N));
    }
    if (TB()) {
        Spec ch;
        ch.delivery = 2;
        ch.cond = C_CHARM;
        ch.condRounds = 1;
        ch.range = 30;
        Spec pa = ch;
        pa.cond = C_PARAL;
        Spec st = ch;
        st.cond = C_STUN;
        Spec inc = ch;
        inc.cond = C_INCAP;
        Spec gr = ch;
        gr.cond = C_GRAPPLE;
        Coef C2 = CC();
        g_B = 800;
        T("T55",
          "(инфо) цена состояния «без броска, 1 раунд, 30 фт» на бюджете 800: недееспособен / парализован / очарован / "
          "схвачен / оглушён",
          true,
          fmt("%lld / %lld / %lld / %lld / %lld", specPrice(inc, C2), specPrice(pa, C2), specPrice(ch, C2),
              specPrice(gr, C2), specPrice(st, C2)));
    }
    if (TB()) {
        Coef C2 = CC();
        int cnt[C_N] = {};
        int N = 30000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            if (s.cond) cnt[s.cond]++;
        }
        int tot = 0;
        for (int k = 1; k < C_N; k++) tot += cnt[k];
        std::string d;
        bool ok = true;
        for (int k = 1; k < C_N; k++) {
            double sh = cnt[k] / (double)tot;
            d += fmt("%s %.0f%%; ", CN[k], 100 * sh);
            if (sh < 0.05 || sh > 0.12) ok = false;
        }
        T("T56", "генератор выдаёт все 15 состояний примерно поровну (5–12% каждое)", ok, d);
    }

    // T57 очарование заканчивается
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100000, 0, 0, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        bool ok = true;
        std::string det;
        for (int rounds = 1; rounds <= 3; rounds++) {
            w.f[0].cond[C_CHARM] = rounds;
            w.f[0].cstat[C_CHARM] = -1;
            w.f[0].charmSrc = 1;
            int n = 0;
            while (w.f[0].cond[C_CHARM] > 0 && n < 10) {
                takeTurn(w, 0);
                n++;
            }
            det += fmt("%d р.: кончилось за %d ходов; ", rounds, n);
            if (n != rounds) ok = false;
        }
        T("T57",
          "§очарование (единственный противник — очаровавший) заканчивается ровно через заданное число своих ходов", ok,
          det);
    }

    // T58..T62 досягаемость больше 5 футов
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss r10 = plain(100, 1, 6, 5);
        r10.reach = 10;
        Boss r5 = plain(100, 1, 6, 5);
        Boss rg = plain(100, 1, 6, 30);
        rg.reach = 15;
        Boss tgt = plain(100, 0, 0, 5);
        w.f.push_back(mk(r10, 0, 10, 10));
        w.f.push_back(mk(tgt, 1, 12, 10));
        int a = nearestEnemyInRange(w, w.f[0], atkDist(w.f[0].b));
        w.f[0].b = r5;
        int b2 = nearestEnemyInRange(w, w.f[0], atkDist(w.f[0].b));
        w.f[0].b = rg;
        int dist = atkDist(w.f[0].b);
        T("T58",
          "§досягаемость: оружие с досягаемостью 10 фт достаёт цель в 10 фт, обычное 5 фт — нет; у дальнобойного она "
          "не используется",
          a == 1 && b2 == -1 && dist == 30,
          fmt("досягаемость 10: цель %d; досягаемость 5: цель %d; дальнобойное с reach=15: дальность %d", a, b2, dist));
    }
    if (TB()) {
        auto oa = [&](int reachG, int startDist) {
            int trig = 0, N = 200;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss g = plain(100, 1, 6, 5);
                g.reach = reachG;
                Boss f = plain(100000, 0, 0, 5);
                Boss far = plain(100, 1, 6, 5);
                w.f.push_back(mk(f, 0, 10 + startDist / 5, 10));
                w.f.push_back(mk(g, 1, 10, 10));
                w.f.push_back(mk(far, 1, 60, 10));
                moveToward(w, w.f[0], w.f[2], 0, 30, false);
                if (w.f[1].reactionUsed) trig++;
            }
            return trig;
        };
        int t10 = oa(10, 10), t5 = oa(5, 10), t5adj = oa(5, 5);
        T("T59",
          "§досягаемость: провоцированную атаку даёт выход из ЕЁ досягаемости (10 фт: с расстояния 10 — да; 5 фт: с "
          "расстояния 10 — нет, с 5 — да)",
          t10 == 200 && t5 == 0 && t5adj == 200,
          fmt("reach 10, цель в 10 фт: %d/200; reach 5, цель в 10 фт: %d/200; reach 5, цель в 5 фт: %d/200", t10, t5,
              t5adj));
    }
    if (TB()) {
        auto approach = [&](int reachA) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.reach = reachA;
            Boss d = plain(100000, 0, 0, 5);
            d.ac = 0;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 18, 10));
            int hp0 = w.f[1].hp;
            takeTurn(w, 0);
            return std::make_pair(distFt(w.f[0], w.f[1]), hp0 - w.f[1].hp);
        };
        int hit10 = 0, dist10ok = 0, dist5ok = 0;
        for (int i = 0; i < 60; i++) {
            auto p10 = approach(10), p5 = approach(5);
            dist10ok += p10.first == 10;
            hit10 += p10.second > 0;
            dist5ok += p5.first == 5;
        }
        T("T60", "§досягаемость: ИИ подходит ровно на свою досягаемость и атакует (10 фт против 5 фт)",
          dist10ok == 60 && hit10 >= 50 && dist5ok == 60,
          fmt("reach 10: подошёл на 10 фт %d/60, попал %d/60; reach 5: подошёл на 5 фт %d/60", dist10ok, hit10, dist5ok));
    }
    if (TB()) {
        Coef C2 = CC();
        long long pr[4];
        int rr[4] = {5, 10, 15, 20};
        bool ok = true;
        Boss b = plain(100, 2, 6, 5);
        b.wcost = 0;
        for (int i = 0; i < 4; i++) {
            Boss c = b;
            c.reach = rr[i];
            g_B = 800;
            pr[i] = priceOf(c, C2, 800);
        }
        for (int i = 1; i < 4; i++)
            if (!(pr[i] > pr[i - 1])) ok = false;
        T("T61",
          "§досягаемость: цена растёт с досягаемостью (5 < 10 < 15 < 20 фт); у дальнобойного оружия не добавляется", ok,
          fmt("цены: %lld / %lld / %lld / %lld", pr[0], pr[1], pr[2], pr[3]));
    }
    if (TB()) {
        int melee = 0, r10 = 0, r15 = 0, r20 = 0, rMore = 0, N = 8000;   // rMore — больше 20 фт (после теста 4 ноября: до 500 фт)
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, CC(), false);
            if (b.wN > 0 && b.wRange <= 5) {
                melee++;
                if (b.reach == 10)
                    r10++;
                else if (b.reach == 15)
                    r15++;
                else if (b.reach == 20)
                    r20++;
                else if (b.reach > 20)
                    rMore++;
            }
        }
        double sh = (double)(r10 + r15 + r20 + rMore) / melee;
        T("T62", "(инфо) доля ближних боссов с досягаемостью >5 фт (в бестиарии ≈16% карточек)", sh > 0.12 && sh < 0.20,
          fmt("%.1f%% (10 фт: %d, 15 фт: %d, 20 фт: %d, больше: %d из %d ближних)", 100 * sh, r10, r15, r20, rMore, melee));
    }

    // T63..T68 реакции: Парирование и Невероятное уклонение
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100000, 1, 6, 5);
        d.ac = 15;
        d.parry = 3;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        int h1 = 0, h2 = 0, used = 0, N = 80000;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            h1 += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            used += w.f[1].reactionUsed;
            w.f[1].reactionUsed = true;
            bool c2;
            h2 += attackRoll(w, w.f[0], w.f[1], PROF, 5, c2);
        }
        double p1 = (double)h1 / N, pu = (double)used / N, p2 = (double)h2 / N;
        T("T63",
          "§Парирование +3 против КД 15: первая ближняя атака попадает 25% (было бы 40%), реакция тратится в 15% "
          "случаев; вторая в раунде — 40%",
          std::fabs(p1 - 0.25) < 0.012 && std::fabs(pu - 0.15) < 0.012 && std::fabs(p2 - 0.40) < 0.012,
          fmt("первая %.3f, реакция потрачена %.3f, вторая %.3f", p1, pu, p2));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 30);
        Boss d = plain(100000, 1, 6, 5);
        d.ac = 15;
        d.parry = 3;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 14, 10));
        int N = 60000, hr = 0, ur = 0;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            hr += attackRoll(w, w.f[0], w.f[1], PROF, 30, c);
            ur += w.f[1].reactionUsed;
        }
        int hh = 0, uh = 0;
        w.f[0].hidden = true;
        w.f[0].b.wRange = 5;
        w.f[1].x = 11;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            hh += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            uh += w.f[1].reactionUsed;
        }
        w.f[0].hidden = false;
        int hi = 0, ui = 0;
        w.f[1].cond[C_INCAP] = 3;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            hi += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            ui += w.f[1].reactionUsed;
        }
        T("T64",
          "§Парирование не работает: против дальнобойной атаки, против невидимого (скрытого) атакующего, в "
          "недееспособном состоянии",
          std::fabs((double)hr / N - 0.40) < 0.012 && ur == 0 && uh == 0 && ui == 0 &&
              std::fabs((double)hi / N - 0.40) < 0.012,
          fmt("дальняя атака: %.3f (реакций %d); скрытый атакующий: %.3f (реакций %d, с преимуществом ≈0.64); "
              "недееспособен: %.3f (реакций %d)",
              (double)hr / N, ur, (double)hh / N, uh, (double)hi / N, ui));
    }
    if (TB()) {
        Fighter d = mk(plain(100, 0, 0, 5), 0, 1, 1);
        d.b.uncanny = true;
        int a1 = applyUncanny(d, 20);
        bool u1 = d.reactionUsed;
        int a2 = applyUncanny(d, 20);
        Fighter e = mk(plain(100, 0, 0, 5), 0, 1, 1);
        e.b.uncanny = true;
        int small = applyUncanny(e, 5);
        bool us = e.reactionUsed;
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.uncanny = true;
        f.cond[C_INCAP] = 1;
        int inc = applyUncanny(f, 20);
        T("T65",
          "§Невероятное уклонение: вдвое меньше урона один раз за раунд; мелкие удары (<10% ХП) не тратят реакцию; "
          "недееспособный не может",
          a1 == 10 && u1 && a2 == 20 && small == 5 && !us && inc == 20,
          fmt("20→%d (реакция %d); второй удар 20→%d; мелкий 5→%d (реакция %d); недееспособен 20→%d", a1, u1, a2, small,
              us, inc));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = STR;
        Boss d = plain(200, 0, 0, 5);
        d.ac = 0;
        d.uncanny = true;
        Spec sv;
        sv.delivery = 1;
        sv.saveStat = DEX;
        sv.dmgN = 20;
        sv.dmgS = 6;
        sv.range = 30;
        Spec at;
        at.delivery = 0;
        at.dmgN = 20;
        at.dmgS = 6;
        at.range = 5;
        int savedUsed = 0, atkUsed = 0, N = 300;
        for (int i = 0; i < N; i++) {
            w.f.clear();
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            hitOne(w, sv, w.f[0], w.f[1]);
            savedUsed += w.f[1].reactionUsed;
            w.f[1].reactionUsed = false;
            w.f[1].hp = w.f[1].maxHp;
            int hp0 = w.f[1].hp;
            hitOne(w, at, w.f[0], w.f[1]);
            atkUsed += w.f[1].reactionUsed;
        }
        T("T66",
          "§Невероятное уклонение действует на атаки (с броском), но не на урон от спасброска (дыхание, области)",
          savedUsed == 0 && atkUsed >= N * 0.9,
          fmt("урон от спасброска: реакция потрачена %d из %d; атака с броском: %d из %d", savedUsed, N, atkUsed, N));
    }
    if (TB()) {
        Coef C2 = CC();
        Boss b = plain(100, 1, 6, 5);
        long long p0 = priceOf(b, C2, 800);
        Boss c = b;
        c.parry = 2;
        long long p2 = priceOf(c, C2, 800);
        c.parry = 5;
        long long p5 = priceOf(c, C2, 800);
        Boss u = b;
        u.uncanny = true;
        long long pu = priceOf(u, C2, 800);
        T("T67", "§цены: Парирование растёт с бонусом (+2 < +5), Уклонение стоит отдельно",
          p2 > p0 && p5 > p2 && pu > p0, fmt("без: %lld; +2: %lld; +5: %lld; уклонение: %lld", p0, p2, p5, pu));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 1, 6, 5);
        d.ac = 15;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        double base = pHitEst(w, w.f[0], w.f[1], PROF, 5);
        w.f[1].b.parry = 4;
        double with = pHitEst(w, w.f[0], w.f[1], PROF, 5);
        w.f[1].reactionUsed = true;
        double spent = pHitEst(w, w.f[0], w.f[1], PROF, 5);
        double far = pHitEst(w, w.f[0], w.f[1], PROF, 30);
        T("T68",
          "ИИ-оценка попадания учитывает Парирование цели (ниже), пока реакция не потрачена и против ближней атаки",
          with < base && spent == base && far != with,
          fmt("без парирования %.3f; с парированием %.3f; реакция потрачена %.3f", base, with, spent));
    }

    // T69..T74 довесок к удару оружием
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 1, 5);
        a.wAtt = 3;
        a.rN = 50;
        a.rS = 1;
        a.rType = DT_FIRE;
        Boss d = plain(1000000, 0, 0, 5);
        d.ac = -1000;
        int bad = 0, got = 0, N = 2000;
        for (int i = 0; i < N; i++) {
            w.f.clear();
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            int hp0 = w.f[1].hp;
            w.f[0].riderUsed = false;
            doWeapon(w, w.f[0], 1);
            int D = hp0 - w.f[1].hp;
            if (D >= 100) bad++;
            if (D >= 51) got++;
        }
        T("T69",
          "§довесок: срабатывает один раз за ход (3 атаки): урон ≥100 бывает лишь при крите на первом попадании (~5%), "
          "при двойном было бы ~99%",
          bad < N * 0.10 && got > N * 0.9,
          fmt("урон ≥100: %d из %d (%.1f%%; крит ожидается ~5%%); с довеском %d", bad, N, 100.0 * bad / N, got));
    }
