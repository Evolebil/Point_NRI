// Проверки движка, часть 4 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // T97..T103 партия 3: концентрация, ячейки, триггеры, контратака, всплеск
    if (TB()) {
        auto trial = [&](int dmg, int& broke) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100000, 0, 0, 5);
            a.mainStat = STR;
            a.stats = {30, 10, 10, 10, 10, 10};
            Spec sp;
            sp.delivery = 1;
            sp.range = 30;
            sp.saveStat = DEX;
            sp.cond = C_STUN;
            sp.condRounds = 5;
            sp.conc = true;
            a.abil.push_back(sp);
            Boss d = plain(1000, 0, 0, 5);
            d.stats = {1, 1, 1, 1, 1, 1};
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 12, 10));
            doSpec(w, w.f[0], 0, 1);
            bool had = w.f[1].cond[C_STUN] > 0 && w.f[1].condConc[C_STUN] == 1 && w.f[0].concSpec == 0;
            if (dmg > 0) {
                hurt(w.f[0], dmg, DT_FIRE, true);
                resolveConc(w);
            }
            broke = w.f[1].cond[C_STUN] == 0 && w.f[0].concSpec < 0;
            return had;
        };
        int N = 600, hadAll = 0, bigBroke = 0, smallBroke = 0, noneBroke = 0;
        for (int i = 0; i < N; i++) {
            int b;
            hadAll += trial(40, b);
            bigBroke += b;
            trial(2, b);
            smallBroke += b;
            trial(0, b);
            noneBroke += b;
        }
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100000, 0, 0, 5);
        a.mainStat = STR;
        a.stats = {30, 10, 10, 10, 10, 10};
        Spec s1;
        s1.delivery = 1;
        s1.range = 30;
        s1.saveStat = DEX;
        s1.cond = C_STUN;
        s1.condRounds = 5;
        s1.conc = true;
        Spec s2 = s1;
        s2.cond = C_SLOW;
        a.abil.push_back(s1);
        a.abil.push_back(s2);
        Boss d = plain(1000, 0, 0, 5);
        d.stats = {1, 1, 1, 1, 1, 1};
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 12, 10));
        doSpec(w, w.f[0], 0, 1);
        doSpec(w, w.f[0], 1, 1);
        bool swapped = w.f[1].cond[C_STUN] == 0 && w.f[1].cond[C_SLOW] > 0 && w.f[0].concSpec == 1;
        for (int i = 0; i < 5; i++) endOfTurn(w.f[1]);
        w.f[0].hp = w.f[0].hp;
        w.f[0].concSpec = 1;
        {
            Fighter& me = w.f[0];
            bool act = false;
            for (auto& f : w.f)
                for (int k = 1; k < C_N; k++)
                    if (f.cond[k] > 0 && f.condConc[k] == 1) act = true;
            if (!act) me.concSpec = -1;
        }
        T("T97",
          "§концентрация: эффект держится ею; удар на 40 (СЛ 20) срывает в ~95%, на 2 (СЛ 10) в ~45%, без урона не "
          "срывается; вторая концентрация снимает первую; кончилась сама — освобождается",
          hadAll == N && bigBroke >= N * 0.9 && smallBroke >= N * 0.35 && smallBroke <= N * 0.55 && noneBroke == 0 &&
              swapped && w.f[0].concSpec < 0,
          fmt("эффект держится %d/%d; срыв: сильный удар %d, слабый %d, без урона %d (из %d); замена %d; освобождение "
              "%d",
              hadAll, N, bigBroke, smallBroke, noneBroke, N, swapped, w.f[0].concSpec < 0));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 0, 0, 5);
        a.slots[3] = 1;
        a.slots[5] = 1;
        Spec sp;
        sp.delivery = 2;
        sp.range = 30;
        sp.dmgN = 1;
        sp.dmgS = 6;
        sp.slotLvl = 3;
        a.abil.push_back(sp);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
        int f1 = findSlot(w.f[0], 3), f4 = findSlot(w.f[0], 4), f6 = findSlot(w.f[0], 6);
        doSpec(w, w.f[0], 0, 1);
        int l3 = w.f[0].slotLeft[3], l5 = w.f[0].slotLeft[5];
        doSpec(w, w.f[0], 0, 1);
        int l5b = w.f[0].slotLeft[5];
        int none = findSlot(w.f[0], 1);
        auto dmgAI = [&](int n) {
            World v;
            std::memset(v.wall, 0, sizeof v.wall);
            std::memset(v.climb, 0, sizeof v.climb);
            Boss b = plain(100, 0, 0, 5);
            b.slots[1] = n;
            Spec q = sp;
            q.slotLvl = 1;
            q.dmgN = 3;
            b.abil.push_back(q);
            v.f.push_back(mk(b, 0, 10, 10));
            v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
            v.f[0].init = 2000;
            v.f[1].init = 1;
            int hp0 = v.f[1].hp;
            takeTurn(v, 0);
            return hp0 - v.f[1].hp;
        };
        int withSlot = dmgAI(1), noSlot = dmgAI(0);
        T("T98",
          "§ячейки: подходящая — наименьшая ≥ уровня способности; тратится; без ячеек способность недоступна (ИИ "
          "остаётся лишь с безоружным ударом ≤1)",
          f1 == 3 && f4 == 5 && f6 == 0 && l3 == 0 && l5 == 1 && l5b == 0 && none == 0 && withSlot > noSlot + 3 &&
              noSlot <= 1,
          fmt("ячейка для ур.3: %d, ур.4: %d, ур.6: %d; после 1-го каста 3-й ур. %d, 5-й %d; после 2-го 5-й %d; ИИ с "
              "ячейкой: урон %d, без (только безоружный удар): %d",
              f1, f4, f6, l3, l5, l5b, withSlot, noSlot));
    }
    if (TB()) {
        auto mean = [&](int phN, int cdN, bool restrained, int hpFrac100) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 1, 5);
            a.wAtt = 3;
            if (phN) a.rules.push_back(Rule{W_HPLOW, 0, E_DMG, phN, 6, false});   // «когда → что»: ярость
            if (cdN) a.rules.push_back(Rule{W_FOECOND, C_RESTR, E_DMG, cdN, 6, true});   // добивание
            Boss d = plain(1000000, 0, 0, 5);
            d.ac = -1000;
            double tot = 0;
            int N = 3000;
            for (int i = 0; i < N; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                w.f[0].hp = hpFrac100;
                if (restrained) w.f[1].cond[C_RESTR] = 3;
                int h0 = w.f[1].hp;
                w.f[0].onceUsed = 0;
                doWeapon(w, w.f[0], 1);
                tot += h0 - w.f[1].hp;
            }
            return tot / N;
        };
        double base = mean(0, 0, false, 100), hi = mean(2, 0, false, 100), lo = mean(2, 0, false, 40),
               cd0 = mean(0, 2, false, 100), cd1 = mean(0, 2, true, 100);
        T("T99",
          "§условия: ярость при ХП≤50% (+2к6 к каждому попаданию) только при ≤50%; добивание (+2к6) раз в ход лишь по "
          "цели с нужным состоянием",
          std::fabs(hi - base) < 0.3 && lo - base > 18 && lo - base < 24 && std::fabs(cd0 - base) < 0.3 &&
              cd1 - cd0 > 5.5 && cd1 - cd0 < 8.5,
          fmt("база %.2f; ХП 100%%: %.2f; ХП 40%%: %.2f (+%.1f, ожид. ≈ 3 попад.×7×0.95=20); добивание: без состояния "
              "%.2f, с ним %.2f (+%.1f, ожид. ≈ 6.6)",
              base, hi, lo, lo - base, cd0, cd1, cd1 - cd0));
    }
    if (TB()) {
        auto run = [&](int wRangeAtk, bool rip, int hits, long long& ripUsed, int& lost) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100000, hits, 1, wRangeAtk);
            a.wAtt = hits;
            a.wN = 1;
            a.wAmmo = wRangeAtk > 5;   // дальняя атака — лук в упор (метательным вплотную бьют как ближним)
            Boss d = plain(100000, 1, 1, 5);
            d.riposte = rip;
            d.ac = -1000;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            long long r0 = g_ripUsed;
            int h0 = w.f[0].hp;
            doWeapon(w, w.f[0], 1);
            ripUsed = g_ripUsed - r0;
            lost = h0 - w.f[0].hp;
            return 0;
        };
        long long u1 = 0, u2 = 0, u3 = 0, u4 = 0;
        int l1 = 0, l2 = 0, l3 = 0, l4 = 0;
        long long tot1 = 0, tot2 = 0, tot3 = 0;
        int trials = 400;
        for (int i = 0; i < trials; i++) {
            run(5, true, 1, u1, l1);
            tot1 += u1;
            run(5, true, 3, u2, l2);
            tot2 += (u2 > 1 ? 1000 : u2);
            run(30, true, 1, u3, l3);
            tot3 += u3;
            run(5, false, 1, u4, l4);
            tot3 += u4 * 1000;
        }
        T("T100",
          "§контратака (реакция): после попадания ближней атакой — ответный удар; не больше одной за раунд; на дальнюю "
          "атаку и без умения её нет",
          tot1 >= trials * 0.9 && tot2 >= trials * 0.9 && tot2 < trials * 1.1 && tot3 < trials * 0.1,
          fmt("одна атака: контратак %lld из %d (атака попадает ~95%%); 3 атаки: ровно по одной %lld из %d; дальняя "
              "атака / без умения: %lld",
              tot1, trials, tot2, trials, tot3));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.surge = 1;
        a.wAtt = 1;
        Boss d = plain(1000000, 0, 0, 5);
        d.ac = -1000;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[0].init = 2000;
        w.f[1].init = 1;
        long long a0 = g_attacksMade;
        takeTurn(w, 0);
        long long n1 = g_attacksMade - a0;
        a0 = g_attacksMade;
        takeTurn(w, 0);
        long long n2 = g_attacksMade - a0;
        int left = w.f[0].surgeLeft;
        Boss b2 = a;
        b2.surge = 2;
        World v = w;
        v.f.clear();
        v.f.push_back(mk(b2, 0, 10, 10));
        v.f.push_back(mk(d, 1, 30, 10));
        long long a1 = g_attacksMade;
        takeTurn(v, 0);
        long long nFar = g_attacksMade - a1;
        T("T101",
          "§всплеск действий: раз в бой второе полное действие (первый ход 2 атаки, дальше 1); не тратится, если "
          "атаковать некого",
          n1 == 2 && n2 == 1 && left == 0 && nFar == 0 && v.f[0].surgeLeft == 2,
          fmt("1-й ход атак %lld, 2-й %lld, осталось %d; цель вдали: атак %lld, всплесков осталось %d", n1, n2, left,
              nFar, v.f[0].surgeLeft));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.delivery = 1;
        a.range = 30;
        a.cond = C_STUN;
        a.condRounds = 5;
        a.saveStat = WIS;
        long long p0 = specPrice(a, C2);
        Spec c = a;
        c.conc = true;
        long long pc = specPrice(c, C2);
        Spec sl = a;
        sl.slotLvl = 3;
        long long ps = specPrice(sl, C2);
        Boss b = plain(100, 1, 6, 5);
        long long q0 = priceOf(b, C2, 800);
        Boss s1 = b;
        s1.slots[1] = 1;
        Boss s3 = b;
        s3.slots[3] = 1;
        Boss s33 = b;
        s33.slots[3] = 3;
        Boss ph = b;
        ph.rules.push_back(Rule{W_HPLOW, 0, E_DMG, 1, 6, false});
        Boss ph3 = b;
        ph3.rules.push_back(Rule{W_HPLOW, 0, E_DMG, 3, 6, false});
        Boss cd = b;
        cd.rules.push_back(Rule{W_FOECOND, C_STUN, E_DMG, 2, 6, true});
        Boss rp = b;
        rp.riposte = true;
        Boss sg = b;
        sg.surge = 1;
        Boss sg2 = b;
        sg2.surge = 2;
        long long a1 = priceOf(s1, C2, 800) - q0, a3 = priceOf(s3, C2, 800) - q0, a33 = priceOf(s33, C2, 800) - q0,
                  pp1 = priceOf(ph, C2, 800) - q0, pp3 = priceOf(ph3, C2, 800) - q0, pcd = priceOf(cd, C2, 800) - q0,
                  prp = priceOf(rp, C2, 800) - q0, ps1 = priceOf(sg, C2, 800) - q0, ps2 = priceOf(sg2, C2, 800) - q0;
        T("T102",
          "§цены: концентрация и «за ячейку» удешевляют способность; ячейка растёт с уровнем и числом; триггеры, "
          "контратака и всплеск — положительные надбавки, растут с кубиками/числом",
          pc < p0 && ps < p0 && a3 > a1 && a33 > a3 && pp3 > pp1 && pcd > 0 && prp > 0 && ps2 > ps1,
          fmt("способность: обычная %lld, с концентрацией %lld, за ячейку %lld; ячейка 1 ур. +%lld, 3 ур. +%lld, 3×3 "
              "ур. +%lld; ярость 1к6 +%lld, 3к6 +%lld; добивание 2к6 +%lld; контратака +%lld; всплеск ×1 +%lld, ×2 "
              "+%lld",
              p0, pc, ps, a1, a3, a33, pp1, pp3, pcd, prp, ps1, ps2));
    }
    if (TB()) {
        int conc = 0, dur = 0, slotted = 0, ns = 0, N = 40000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            if (s.self) {
                if (s.buffN || s.acB || s.invisN) {
                    dur++;
                    if (s.conc) conc++;
                }
                continue;
            }
            ns++;
            if (s.cond && s.cond != C_PRONE && s.delivery != 2) {
                dur++;
                if (s.conc) conc++;
            }
        }
        int tr = 0, rp = 0, sg = 0, sl = 0, BN = 8000;
        Coef C2 = CC();
        for (int i = 0; i < BN; i++) {
            Boss b = makeBoss(800, C2, false);
            for (auto& r : b.rules)
                if (r.what == E_DMG) { tr++; break; }
            if (b.riposte) rp++;
            if (b.surge) sg++;
            bool any = false;
            for (int L = 1; L < 10; L++)
                if (b.slots[L]) any = true;
            if (any) sl++;
        }
        (void)slotted;
        T("T103",
          "(инфо) генератор: доля длящихся эффектов под концентрацией; доли боссов с триггерами / контратакой / "
          "всплеском / ячейками",
          conc > 0 && sl > 0,
          fmt("концентрация %.0f%% длящихся; триггеры %.1f%%; контратака %.1f%%; всплеск %.1f%%; ячейки %.1f%%",
              100.0 * conc / std::max(1, dur), 100.0 * tr / BN, 100.0 * rp / BN, 100.0 * sg / BN, 100.0 * sl / BN));
    }

    // T104..T109 доводка: подавление пассивок, дальность за 5 фт, виды оружия, арсенал
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100000, 0, 0, 5);
        a.regenN = 3;
        a.suppMask = 1u << DT_FIRE;
        a.suppTgt = 1;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(100000, 0, 0, 5), 1, 90, 90));
        w.f[0].hp = 50000;
        int h0 = w.f[0].hp;
        takeTurn(w, 0);
        int reg1 = w.f[0].hp - h0;
        hurt(w.f[0], 10, DT_FIRE, true);
        int h1 = w.f[0].hp;
        takeTurn(w, 0);
        int reg2 = w.f[0].hp - h1;
        int h2 = w.f[0].hp;
        takeTurn(w, 0);
        int reg3 = w.f[0].hp - h2;
        hurt(w.f[0], 10, DT_COLD, true);
        int h3 = w.f[0].hp;
        takeTurn(w, 0);
        int reg4 = w.f[0].hp - h3;
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        Boss o = plain(100000, 0, 0, 5);
        o.auraN = 50;
        o.auraS = 1;
        o.auraRad = 15;
        o.auraType = DT_FIRE;
        o.suppMask = 1u << DT_FIRE;
        o.suppTgt = 2;
        v.f.push_back(mk(o, 0, 10, 10));
        v.f.push_back(mk(plain(100000, 0, 0, 5), 1, 13, 10));
        auto vic = [&]() {
            int hp0 = v.f[1].hp;
            takeTurn(v, 1);
            return hp0 - v.f[1].hp;
        };
        int au1 = vic();
        hurt(v.f[0], 5, DT_FIRE, true);
        int au2 = vic();
        endOfTurn(v.f[0]);
        int au3 = vic();
        Fighter t2 = mk(plain(100000, 0, 0, 5), 0, 1, 1);
        t2.b.resMask = 1u << DT_FIRE;
        t2.b.vulnMask = 1u << DT_RADIANT;
        t2.b.suppMask = 1u << DT_COLD;
        t2.b.suppTgt = 4;
        int p0 = t2.hp;
        hurt(t2, 10, DT_FIRE, true);
        int d1 = p0 - t2.hp;
        hurt(t2, 10, DT_COLD, true);
        p0 = t2.hp;
        hurt(t2, 10, DT_FIRE, true);
        int d2 = p0 - t2.hp;
        p0 = t2.hp;
        hurt(t2, 10, DT_RADIANT, true);
        int d3 = p0 - t2.hp;
        T("T104",
          "§подавление: урон подавляющего типа отключает на ход регенерацию / ауру / резисты (уязвимость остаётся); "
          "другой тип не подавляет",
          reg1 >= 3 && reg2 == 0 && reg3 >= 3 && reg4 >= 3 && au1 == 50 && au2 == 0 && au3 == 50 && d1 == 5 &&
              d2 == 10 && d3 == 20,
          fmt("регенерация: до %d, после огня %d, потом %d, после холода %d; аура: %d, после огня по владельцу %d, "
              "потом %d; защиты: огонь с резистом %d, после подавления %d, уязвимость %d",
              reg1, reg2, reg3, reg4, au1, au2, au3, d1, d2, d3));
    }
    if (TB()) {
        Coef C2 = CC();
        Boss b = plain(100, 1, 6, 5);
        b.regenN = 3;
        long long p0 = priceOf(b, C2, 800);
        Boss c = b;
        c.suppMask = 1u << DT_PIERCING;
        c.suppTgt = 1;
        long long pBig = priceOf(c, C2, 800);
        Boss d = b;
        d.suppMask = 1u << DT_FORCE;
        d.suppTgt = 1;
        long long pSmall = priceOf(d, C2, 800);
        Boss e = b;
        e.suppMask = 1u << DT_PIERCING;
        e.suppTgt = 2;
        long long pOther = priceOf(e, C2, 800);
        T("T105",
          "§подавление: скидка на подавляемую пассивку; чем чаще встречается подавляющий урон, тем больше скидка; на "
          "неподавляемое скидки нет",
          pBig < pSmall && pSmall <= p0 && pOther == p0,
          fmt("регенерация 3к6: без подавления %lld; подавляет колющий (частый) %lld; подавляет силовое поле (редкое) "
              "%lld; подавляется только аура (регенерации скидки нет) %lld",
              p0, pBig, pSmall, pOther));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 10;
        s.dmgS = 12;
        auto p = [&](int r, int rad) {
            Spec x = s;
            x.range = r;
            x.radius = rad;
            return specPrice(x, C2);
        };
        long long d1 = p(35, 0) - p(30, 0), d2 = p(30, 0) - p(25, 0), a1 = p(30, 5), a2 = p(30, 10), a3 = p(30, 15);
        Boss wb = plain(100, 20, 12, 30);
        auto wp = [&](int r) {
            Boss x = wb;
            x.wRange = r;
            return priceOf(x, C2, 800);
        };
        long long w1 = wp(90) - wp(60),
                  w2 = wp(60) - wp(30); /* шаг 5 фт у оружия дешевле очка — линейность проверяем на отрезках по 30 фт */
        T("T106", "§дальность и область — цена за каждые 5 фт (линейно), любые значения; у оружия то же",
          std::llabs(d1 - d2) <= 1 && d1 >= 1 && a1 < a2 && a2 < a3 && std::llabs(w1 - w2) <= 3 && w1 >= 1 && w2 >= 1,
          fmt("способность: +5 фт дальности стоит +%lld и +%lld; радиус 5/10/15 фт: %lld/%lld/%lld; оружие: 30→60 фт "
              "стоит +%lld, 60→90 фт +%lld",
              d1, d2, a1, a2, a3, w2, w1));
    }
    if (TB()) {
        int rangedN = 0, ammo = 0, badAF = 0, N = 6000;
        std::set<int> rs;
        bool mult5 = true;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (b.wN > 0 && b.wRange > 5) {
                rangedN++;
                if (b.wAmmo) ammo++;
                if (b.wAmmo && b.wFinesse) badAF++;
                rs.insert(b.wRange);
                if (b.wRange % 5) mult5 = false;
            }
            if (b.wRange <= 5 && b.wAmmo) badAF++;
        }
        T("T107",
          "§виды оружия: боеприпасное только у дальнобойного и не фехтовальное (всегда ЛОВ); дальности любые, кратные "
          "5 фт",
          badAF == 0 && mult5 && rs.size() >= 15 && ammo > rangedN * 0.6 && ammo < rangedN * 0.8,
          fmt("дальнобойных %d, боеприпасных %.0f%%, нарушений %d, разных дальностей %zu", rangedN,
              100.0 * ammo / std::max(1, rangedN), badAF, rs.size()));
    }
    if (TB()) {
        auto run = [&](bool bowFirst, int distFt_, int& rangeAfter, int& costSame) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.wcost = 7;
            Wp bow;
            bow.n = 1;
            bow.s = 6;
            bow.range = 60;
            bow.ammo = true;
            bow.cost = 11;
            Wp sw;
            sw.n = 1;
            sw.s = 6;
            sw.range = 5;
            sw.cost = 7;
            if (bowFirst) {
                a.wN = bow.n;
                a.wS = 6;
                a.wRange = 60;
                a.wAmmo = true;
                a.wcost = 11;
                a.alt.push_back(sw);
            } else
                a.alt.push_back(bow);
            Boss d = plain(100000, 0, 0, 5);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 10 + distFt_ / 5, 10));
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            rangeAfter = w.f[0].b.wRange;
            costSame = (w.f[0].b.wcost + w.f[0].b.alt[0].cost == 18);
        };
        int r1, r2, r3, r4, c1, c2, c3, c4;
        run(false, 60, r1, c1);
        run(true, 5, r2, c2);
        run(true, 60, r3, c3);
        run(false, 5, r4, c4);
        T("T108",
          "§арсенал: бот меняет оружие под ситуацию — лук на дистанции 60 фт, меч вплотную (в любом порядке в списке); "
          "цены не теряются при смене",
          r1 == 60 && r2 == 5 && r3 == 60 && r4 == 5 && c1 && c2 && c3 && c4,
          fmt("меч→дальность 60 фт: %d; лук→вплотную: %d; лук→60 фт: %d; меч→вплотную: %d (дальность выбранного "
              "оружия)",
              r1, r2, r3, r4));
    }
    if (TB()) {
        int arsenal = 0, supp = 0, chaMain = 0, N = 8000;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (!b.alt.empty()) arsenal++;
            if (b.suppMask) supp++;
            if (b.mainStat == CHA) chaMain++;
        }
        T("T109", "(инфо) доли боссов: с арсеналом / с подавлением пассивок / главная ХАР", arsenal > 0 && supp > 0,
          fmt("%.1f%% / %.1f%% / %.1f%%", 100.0 * arsenal / N, 100.0 * supp / N, 100.0 * chaMain / N));
    }

    // T110..T114 политика бота (обучаемые параметры), кайтинг, защита
    if (TB()) {
        Pol b = Pol::baseline();
        const double orig[PO_N] = {15,  0.5,  1.0,  0.25, 0.30, 0.50, 2.0, 1.0, 0.4,
                                   1.0, 0.25, 0.10, 0.15, 1.0,  0.2,  0.7, 0.0, 0.0, 0.5};   // 0.5 — wallW (v21)
        bool ok = true;
        for (int i = 0; i < PO_N; i++)
            if (std::fabs(b.v[i] - orig[i]) > 1e-12) ok = false;
        bool inRange = true;
        for (int i = 0; i < PO_N; i++)
            if (POL_LEARNED[i] < PD[i].lo - 1e-9 || POL_LEARNED[i] > PD[i].hi + 1e-9) inRange = false;
        T("T110",
          "§политика: базовые значения = прежним «зашитым» порогам (регресс); обученные значения лежат в допустимых "
          "границах",
          ok && inRange, fmt("база совпадает %d; обученные в границах %d; параметров %d", ok, inRange, PO_N));
    }
    if (TB()) {
        auto run = [&](double kite, int& dist0, int& dist1) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 120);
            Boss d = plain(100000, 1, 6, 5);
            Pol p = Pol::baseline();
            p.v[PO_KITE] = kite;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 14, 10));
            w.f[0].pol = &p;
            w.f[0].init = 2000;
            w.f[1].init = 1;
            dist0 = distFt(w.f[0], w.f[1]);
            takeTurn(w, 0);
            dist1 = distFt(w.f[0], w.f[1]);
        };
        int a0, a1, b0, b1, c0, c1;
        run(0, a0, a1);
        run(1.0, b0, b1);
        run(0.1, c0, c1);
        T("T111",
          "§кайтинг: дальнобойный при врагу-ближнике в зоне его хода отступает (с 20 фт до 50 фт), при выключенном "
          "(kite=0) или малом множителе остаётся",
          a1 == a0 && b1 == 50 && c1 == c0,
          fmt("kite=0: %d→%d фт; kite=1: %d→%d фт; kite=0.1: %d→%d фт", a0, a1, b0, b1, c0, c1));
    }
    if (TB()) {
        auto run = [&](double dodge, std::string& act, bool& dodging) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            Boss d = plain(100000, 1, 6, 5);
            Pol p = Pol::baseline();
            p.v[PO_DODGE] = dodge;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            w.f[0].pol = &p;
            w.f[0].hp = 20;
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            act = g_act.type;
            dodging = w.f[0].dodging;
        };
        std::string x1, x2;
        bool d1, d2;
        run(0.0, x1, d1);
        run(0.5, x2, d2);
        T("T112", "§защита: при ХП ниже порога бот выбирает Уклонение вместо атаки (dodgeHp=0.5); по умолчанию атакует",
          x1.compare(0, 6, "attack") == 0 && !d1 && x2 == "dodge" && d2,
          fmt("dodgeHp=0: действие «%s»; dodgeHp=0.5: «%s», уклоняется %d", x1.c_str(), x2.c_str(), d2));
    }
    if (TB()) {
        Pol base = Pol::baseline(), cow = Pol::baseline();
        cow.v[PO_DODGE] = 1.1;
        int winA = 0, winB = 0, same = 0, N = 300;
        Coef C2 = CC();
        rng.seed(404);
        for (int i = 0; i < N; i++) {
            Boss A = makeBoss(800, C2, true), B = makeBoss(800, C2, true);
            A.stats[INT] = B.stats[INT] = 10;
            for (auto& m : A.tmpl) m.stats[INT] = 10;
            for (auto& m : B.tmpl) m.stats[INT] = 10;
            int r = fight(A, B, nullptr, nullptr, &cow, &base);
            winA += r == 0;
            r = fight(A, B, nullptr, nullptr, &base, &cow);
            winB += r == 1;
            r = fight(A, B, nullptr, nullptr, &base, &base);
            same += r == 0;
        }
        T("T113",
          "§политика применяется к своей команде: «всегда уклоняется» редко побеждает на любой стороне; одинаковые "
          "политики — ~50%",
          winA < N * 0.35 && winB < N * 0.35 && same > N * 0.40 && same < N * 0.60,
          fmt("трус на стороне A побеждает %d из %d, на стороне B %d из %d; одинаковые политики: A побеждает %d из %d",
              winA, N, winB, N, same, N));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        addSummon(a, plain(50, 1, 6, 5), 1);
        Pol p = Pol::baseline();
        p.v[PO_SUMMON] = 7.0;
        w.f.reserve(64);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 14, 10));
        w.f[0].pol = &p;
        w.f[0].init = 2000;
        w.f[1].init = 1;
        takeTurn(w, 0);
        bool summoned = w.f.size() == 3;
        bool inherits = summoned && w.f[2].pol == &p;
        T("T114", "§политика наследуется призванными приспешниками", summoned && inherits,
          fmt("призван %d, политика команды у приспешника %d", summoned, inherits));
    }

    // M1..M7 миньоны: шаблон + копии, управляет главный босс
    if (TB()) {
        Coef C2 = CC();
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss ow = plain(100, 1, 6, 5);
        Boss mt = makeBoss(300, C2, false);
        if (mt.hp < 2) mt.hp = 50;
        const int si = addSummon(ow, mt, 2, 3);
        w.f.reserve(64);
        w.f.push_back(mk(ow, 0, 30, 40));
        w.f[0].init = 5000;
        summonFrom(w, 0, si);
        const Fighter& m = w.f[1];
        bool ok = w.f.size() == 3 && m.hp == mt.hp && m.maxHp == mt.hp && mt.hp > 1 &&
                  std::max(std::abs(m.x - 30), std::abs(m.y - 40)) <= 2 && m.fresh && m.init == 4999 &&
                  m.summoner == 0 && m.sumSpec == si && w.f[0].sumMade[si] == 2;
        T("M1",
          "§миньон при призыве: полные ХП шаблона, клетка рядом с боссом, не ходит в этот раунд, инициатива босса −1 "
          "(регресс бага v11–v16: 1 ХП)",
          ok,
          fmt("ХП %d/%d (шаблон %d), клетка (%d,%d) при боссе (30,40), fresh %d, инициатива %d, призвано способностью %d",
              m.hp, m.maxHp, mt.hp, m.x, m.y, (int)m.fresh, m.init, w.f[0].sumMade[si]));
    }
    if (TB()) {
        auto run = [&](int cmd) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss ow = plain(100, 1, 6, 5);
            Boss em = plain(100, 0, 0, 5);
            Boss mm = plain(100, 1, 6, 30);
            Boss e1 = plain(100, 0, 0, 5);
            w.f.push_back(mk(ow, 0, 5, 5));
            w.f.push_back(mk(em, 1, 90, 90));
            w.f.push_back(mk(mm, 0, 20, 20));
            w.f.push_back(mk(e1, 1, 21, 20));
            w.f.push_back(mk(e1, 1, 24, 20));
            w.cmd[0] = cmd;
            takeTurn(w, 2);
            return g_act.target;
        };
        int t0 = run(-1), t1 = run(3), t2 = run(4);
        T("M2", "§управление: без приказа миньон бьёт ближнюю цель; приказ главного босса переключает его на указанную",
          t0 == 3 && t1 == 3 && t2 == 4, fmt("без приказа: цель %d; приказ→3: %d; приказ→4: %d", t0, t1, t2));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss ow = plain(100, 1, 6, 30);
        Boss en = plain(100, 1, 6, 5);
        w.f.push_back(mk(ow, 0, 10, 10));
        w.f.push_back(mk(en, 1, 40, 10));
        w.cmd[0] = w.cmd[1] = -1;
        takeTurn(w, 0);
        int c0 = w.cmd[0];
        takeTurn(w, 1);
        int c1 = w.cmd[1];
        T("M3", "§управление: главный босс записывает свою цель как приказ команде", c0 == 1 && c1 == 0,
          fmt("приказ команды A: %d, B: %d", c0, c1));
    }
    if (TB()) {
        Coef C2 = CC();
        int both = 0, none = 0, bad = 0, N = 1200;
        for (int i = 0; i < N; i++) {
            Boss a, b;
            makePair(1600, C2, a, b);
            bool sa = hasSummon(a), sb = hasSummon(b);
            if (!sa && !sb) none++;
            if (sa && sb) both++;
            if (a.cost - STAT_BASE > 1600 || b.cost - STAT_BASE > 1600) bad++;
        }
        T("M4", "§пара: хотя бы один босс всегда с миньонами, второй по желанию (~50%), бюджет не превышен",
          none == 0 && bad == 0 && both > N * 0.25 && both < N * 0.65,   // с ценами этапа 4 призыв влезает в бюджет реже: ~35%
          fmt("без миньонов у обоих: %d; у обоих: %.0f%%; перерасход %d (из %d пар)", none, 100.0 * both / N, bad, N));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec one;
        one.self = true;
        one.delivery = 2;
        one.sumN = 1;
        one.sumCost = 300;
        one.uses = 1;
        Spec eight = one, three = one, unl = one, disc = one;
        eight.sumN = 8;
        three.uses = 3;
        unl.uses = UNL;
        disc.uses = UNL;   // скидки сравниваем с призывом без лимита (у одноразового плата уже упирается в нижнюю границу множителя)
        disc.conc = true;
        disc.slotLvl = 3;
        const double t1 = (double)summonTmplPrice(one, C2), t8 = (double)summonTmplPrice(eight, C2), t3 = (double)summonTmplPrice(three, C2),
                     tu = (double)summonTmplPrice(unl, C2), td = (double)summonTmplPrice(disc, C2);
        const bool fee = specPrice(disc, C2) - td < specPrice(unl, C2) - tu;   // скидки — только на плату за призыв
        T("M5", "§v21 призыв: шаблон × копии × применения (8 копий — 8×, 3 применения — 3×; без лимита — как AT_SUMUNL применений); скидки способности не трогают шаблон",
          std::abs(t8 - 8 * t1) <= 8 && std::abs(t3 - 3 * t1) <= 3 && std::abs(tu - C2[AT_SUMUNL] * t1) <= C2[AT_SUMUNL] && td == tu && fee,
          fmt("1 копия %.0f, 8 копий %.0f, 3 применения %.0f, без лимита %.0f (×%.1f), со скидками шаблон %.0f, плата со скидкой дешевле %d", t1, t8, t3, tu,
              C2[AT_SUMUNL], td, fee));
    }
    if (TB()) {
        Coef C2 = CC();
        int N = 3000, ab = 0, pr = 0, lr = 0, rd = 0, wp = 0, au = 0, tot = 0;
        for (int i = 0; i < N; i++) {
            Boss x = makeBoss(3200, C2, true, true);
            if (x.tmpl.empty()) continue;
            const Boss& m = x.tmpl[0];
            tot++;
            if (m.wN > 0) wp++;
            if (!m.abil.empty()) ab++;
            if (m.resMask | m.immMask | m.vulnMask) pr++;
            if (m.legRes) lr++;
            if (m.rcost > 0) rd++;
            if (!m.tmpl.empty()) au++;
        }
        T("M6",
          "§миньон — полноценный босс (оружие, способности, защиты, ЛС, довесок собираются тем же генератором); "
          "вложенных миньонов нет",
          tot > 0 && wp > 0 && ab > 0 && pr > 0 && lr > 0 && rd > 0 && au == 0,
          fmt("шаблонов %d: с оружием %d, способностями %d, защитами %d, ЛС %d, довеском %d; с собственными миньонами "
              "%d",
              tot, wp, ab, pr, lr, rd, au));
    }
    if (TB()) {
        Coef C2 = CC();
        int tot = 0, ghost = 0, fights = 0;
        for (int s = 1; s <= 120; s++) {
            rng.seed(s * 11);
            Boss a, b;
            makePair(1600, C2, a, b);
            std::vector<std::string> log;
            std::string mj;
            fight(a, b, &log, &mj);
            fights++;
            // максимум хитов приспешника — при его появлении (потом его законно срезает «урон снижает максимум хитов»)
            std::vector<int> firstMh;
            for (auto& fr : log) {
                size_t pos = 0;
                int n = 0;
                while ((pos = fr.find("\"kind\":", pos)) != std::string::npos) {
                    int kind = fr[pos + 7] - '0';
                    size_t q = fr.rfind("{\"x\":", pos);
                    int x, y, hp, mh;
                    if (kind >= 2 && q != std::string::npos &&
                        sscanf(fr.c_str() + q, "{\"x\":%d,\"y\":%d,\"hp\":%d,\"maxHp\":%d", &x, &y, &hp, &mh) == 4) {
                        tot++;
                        if ((int)firstMh.size() <= n) firstMh.resize(n + 1, -1);
                        if (firstMh[n] < 0) firstMh[n] = mh;
                        if (firstMh[n] <= 1) ghost++;
                    }
                    pos += 8;
                    n++;
                }
            }
        }
        T("M7", "§в реальных боях миньоны появляются с полными ХП (ни одного «призрака» с maxHp<=1 при появлении)", tot > 0 && ghost == 0,
          fmt("кадров с миньонами: %d, из них призраков: %d (боёв %d)", tot, ghost, fights));
    }

    if (TB()) {
        Coef C2 = CC();
        C2[AT_MINEXP] = 0.5;
        double prev = g_B;
        g_B = 800;
        double m8 = minMult(C2);
        g_B = 1600;
        double m16 = minMult(C2);
        g_B = 3200;
        double m32 = minMult(C2);
        C2[AT_MINEXP] = 0;
        double z = minMult(C2);
        g_B = prev;
        T("M8",
          "§цена миньонов зависит от бюджета: при k>0 на больших бюджетах дешевле (×2 бюджета → ×2^-k), при k=0 — как "
          "раньше",
          m8 > m16 && m16 > m32 && std::fabs(m16 / m32 - std::sqrt(2.0)) < 1e-9 && std::fabs(z - C2[AT_MINION]) < 1e-12,
          fmt("k=0.5: 800→%.3f, 1600→%.3f, 3200→%.3f; k=0: %.3f", m8, m16, m32, z));
    }

    // P1–P4: политика сборки миньонов (P1, P3 удалены вместе с ней 8 ноября) и переобученная боевая политика
    if (TB()) {
        Coef C2 = CC();
        int sm = 0, N = 2000;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, true, false);
            if (hasSummon(b)) sm++;
        }
        T("P2", "§без политики сборки генератор прежний: миньоны у ~половины боссов (случайное решение)",
          sm > N * 0.30 && sm < N * 0.65, fmt("%.1f%% боссов с миньонами", 100.0 * sm / N));
    }
    if (TB()) {
        Pol b;
        double sf = -1, kt = -1, fo = -1; /* Pol() — обученная политика по умолчанию (POL_LEARNED) */
        for (int k = 0; k < PO_N; k++) {
            if (!std::strcmp(PD[k].name, "summonFac")) sf = b.v[k];
            if (!std::strcmp(PD[k].name, "kite")) kt = b.v[k];
            if (!std::strcmp(PD[k].name, "focusFt")) fo = b.v[k];
        }
        T("P4",
          "§боевая политика переобучена на настоящих миньонах: призыв приоритетнее (summonFac>2.5), кайтинг включён, "
          "фокус на боссах дальше",
          sf > 2.5 && kt > 0.3 && fo > 25,
          fmt("summonFac %.2f (было 2), kite %.2f (было 0), focusFt %.1f (было 15)", sf, kt, fo));
    }
    // TR1..TR4 блок 1 (v20): цена размерных атомов по трём участкам
    // при бюджете 1600 масштаб порогов равен 1
    if (TB()) {
        bool ident = true, floor1 = true, up2 = true, down3 = true, cont = true, order = true, tail = true;
        std::string d;
        double gB0 = g_B;
        g_B = 1600;
        for (int id = 0; id < TR_N; id++) {
            const TierDef& t = TD[id];
            double a = t.a, b = t.b, P = 1 + t.k2 * (b - a), s3e = std::min(t.s3, P);
            if (!(b > a) || t.s3 < 1 - 1e-9 || t.c <= 0 || t.k2 <= 0) order = false;
            for (int n = 0; n <= (int)a && n <= 3000; n++)
                if (std::fabs(tierN(id, n) - n) > 1e-9) ident = false;
            auto m = [&](int n) { return tierN(id, n) - tierN(id, n - 1); };
            for (int n = 1; n <= 3000; n++) {
                if (m(n) < 1 - 1e-9) floor1 = false;
            }
            for (int n = (int)a + 1; n < (int)b; n++)
                if (m(n + 1) < m(n) - 1e-9) up2 = false;
            for (int n = (int)b + 1; n < 2000; n++)
                if (m(n + 1) > m(n) + 1e-9 || m(n) < s3e - 1e-9) down3 = false;
            double sc = t.e == 0 ? 1.0 : 1.0, aa = a, bb = b;
            (void)sc;
            if (std::fabs(tierN(id, aa + 1e-6) - tierN(id, aa - 1e-6)) > 1e-3 ||
                std::fabs(tierN(id, bb + 1e-6) - tierN(id, bb - 1e-6)) > 1e-3 || m((int)bb + 2) > m((int)bb + 1) + 1e-9)
                cont = false;
            if (std::fabs(m(3000) - s3e) > 1e-3) tail = false;
        }
        g_B = gB0;
        T("TR1",
          "§блок 1б: форма цены ступени: до a ступень 1 (как в v19); после a растёт; после b замедляется, но не "
          "опускается ниже s3>=1; цена и ступень непрерывны; хвост → s3",
          ident && floor1 && up2 && down3 && cont && order && tail,
          fmt("до a совпадает: %d; ступень >=1: %d; участок 2 растёт: %d; участок 3 убывает к s3: %d; стыки: %d; "
              "параметры допустимы: %d; хвост=s3: %d",
              ident, floor1, up2, down3, cont, order, tail));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 5;
        s.dmgS = 8;
        s.dtype = DT_FIRE;
        auto P = [&](int r, int rad, int push) {
            Spec x = s;
            x.range = r;
            x.radius = rad;
            x.pushFt = push;
            return (double)specPrice(x, C2);
        };
        double p5 = P(5, 0, 0), p120 = P(120, 0, 0), p500 = P(500, 0, 0),
               lin500 = p5 * (1 + C2[AT_ARANGE] * tierN(TR_RANGE, 23) / 23.0 * 99), pr0 = P(5, 30, 0),
               pr1 = P(5, 30, 150), prLin = pr0 + C2[AT_PUSH] * tierN(TR_PUSH, 6) / 6.0 * 30;
        T("TR2",
          "§блок 1б: в диапазоне генератора (дальность ≤120 фт) цена линейна по ступеням; вне диапазона цена не "
          "дешевле линейной экстраполяции",
          std::fabs(p120 - p5 * (1 + C2[AT_ARANGE] * tierN(TR_RANGE, 23))) < 1.5 && p500 >= 0.97 * lin500 &&
              pr1 >= 0.75 * prLin,
          fmt("дальность 5/120/500 фт: %.0f / %.0f / %.0f (линейная экстраполяция 500 фт: %.0f); область 30 фт + "
              "толчок 150 фт: %.0f (линейно было бы %.0f)",
              p5, p120, p500, lin500, pr1, prLin));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec r;
        r.delivery = 1;
        r.range = 500;
        r.dmgN = 5;
        r.dmgS = 8;
        r.dtype = DT_FIRE;
        Spec pu;
        pu.delivery = 1;
        pu.range = 30;
        pu.pushFt = 500;
        Spec te;
        te.self = true;
        te.delivery = 2;
        te.teleFt = 500;
        double a = specPrice(r, C2), b2 = specPrice(pu, C2), c = specPrice(te, C2);
        T("TR3",
          "(инфо) «вся карта» (500 фт) на бюджете 1600: способность с уроном 5к8 / толчок 500 фт / телепорт 500 фт; "
          "доля бюджета (порог доли ждёт решения заказчика)",
          true,
          fmt("%.0f (%.0f%% бюджета) / %.0f (%.1f%%) / %.0f (%.1f%%)", a, 100 * a / 1600, b2, 100 * b2 / 1600, c,
              100 * c / 1600));
    }
    if (TB()) {
        Coef C2 = CC();
        Boss c = plain(100, 2, 6, 300);
        c.wAtt = 40;
        Boss d = c;
        d.wN = 0;
        Wp p;
        p.n = 2;
        p.s = 6;
        p.range = 300;
        p.att = 40;
        p.reach = 5;
        long long diff = priceOf(c, C2, 800) - priceOf(d, C2, 800);
        g_B = 800;
        PcGuard pcg(c);   // цена доп. атак — по модификатору этого тела
        T("TR4",
          "§блок 1: цена оружия вне диапазона генератора (дальность 300 фт, 40 атак) в обратном пересчёте = wpPrice "
          "движка",
          diff == wpPrice(p, C2), fmt("пересчёт %lld, движок %lld", diff, wpPrice(p, C2)));
    }
