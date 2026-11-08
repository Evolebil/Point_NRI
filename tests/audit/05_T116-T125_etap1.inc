// Проверки движка, часть 5 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== Этап 1, партия 1: иммунитет к отталкиванию, урон при столкновении, Отход бонусным действием =====
    // T115 цена
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 2;
        s.dmgS = 6;
        s.pushFt = 30;
        long long p0 = specPrice(s, C2);
        Spec s1 = s, s2 = s, s3 = s;
        s1.collN = 1;
        s2.collN = 2;
        s3.collN = 3;
        Spec n = s;
        n.pushFt = 0;
        long long q0 = specPrice(n, C2);
        n.collN = 3;
        long long q3 = specPrice(n, C2);
        Boss b = plain(100, 1, 6, 5);
        long long b0 = priceOf(b, C2, 800);
        Boss i = b;
        i.immPush = true;
        Boss d = b;
        d.disengB = true;
        bool ok = p0 < specPrice(s1, C2) && specPrice(s1, C2) < specPrice(s2, C2) &&
                  specPrice(s2, C2) < specPrice(s3, C2) && q0 == q3 &&
                  priceOf(i, C2, 800) - b0 == (long long)std::ceil(C2[AT_IMMPUSH]) &&
                  priceOf(d, C2, 800) - b0 == (long long)std::ceil(C2[AT_DISENGB]);
        T("T115",
          "§этап 1 цена: урон при столкновении растёт с числом кубиков и не продаётся без толчка; иммунитет к "
          "отталкиванию и Отход бонусным действием стоят свои коэффициенты",
          ok,
          fmt("толчок 30 фт: без столкновения %lld, 1к6 %lld, 2к6 %lld, 3к6 %lld; без толчка %lld/%lld; иммунитет "
              "+%lld, Отход +%lld",
              p0, (long long)specPrice(s1, C2), (long long)specPrice(s2, C2), (long long)specPrice(s3, C2), q0, q3,
              priceOf(i, C2, 800) - b0, priceOf(d, C2, 800) - b0));
    }
    // T116 иммунитет к отталкиванию
    if (TB()) {
        auto run = [&](bool imm) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss tb = plain(1000, 0, 0, 5);
            tb.immPush = imm;
            w.f.push_back(mk(plain(10, 0, 0, 5), 0, 50, 50));
            w.f.push_back(mk(tb, 1, 51, 50));
            pushAway(w, w.f[0], w.f[1], 30, 0);
            return w.f[1].x;
        };
        int xi = run(true), xn = run(false);
        T("T116", "§этап 1 иммунитет к отталкиванию: толчок на 30 фт не сдвигает; без иммунитета сдвигает на 6 клеток",
          xi == 51 && xn == 57, fmt("с иммунитетом x=%d, без x=%d (старт 51)", xi, xn));
    }
    // T117 урон при столкновении
    if (TB()) {
        auto run = [&](int collN, int wallX, bool blocker, bool imm, int& moved, int& bdmg) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss tb = plain(1000, 0, 0, 5);
            tb.immPush = imm;
            w.f.push_back(mk(plain(10, 0, 0, 5), 0, 50, 50));
            w.f.push_back(mk(tb, 1, 51, 50));
            if (blocker) w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 54, 50));
            if (wallX > 0) w.wall[50][wallX] = true;
            pushAway(w, w.f[0], w.f[1], 30, collN);
            moved = w.f[1].x - 51;
            bdmg = blocker ? 1000 - w.f[2].hp : 0;
            return 1000 - w.f[1].hp;
        };
        bool ok = true;
        int mv, bd;
        std::string d;
        int dmgMin = 1 << 30, dmgMax = 0;
        for (int i = 0; i < 400; i++) {
            int dm = run(2, 55, false, false, mv, bd);
            dmgMin = std::min(dmgMin, dm);
            dmgMax = std::max(dmgMax, dm);
            if (mv != 3) ok = false;
        }
        if (dmgMin < 2 || dmgMax > 12 || dmgMax <= dmgMin + 3) ok = false;
        d += fmt("в стену 2к6: урон %d..%d, сдвиг %d; ", dmgMin, dmgMax, mv);
        int a = run(0, 55, false, false, mv, bd);
        if (a != 0) ok = false;
        int b2 = run(2, 0, false, false, mv, bd);
        if (b2 != 0 || mv != 6) ok = false;
        int c = run(2, 55, false, true, mv, bd);
        if (c != 0 || mv != 0) ok = false;
        d += fmt("без кубиков %d; свободный путь %d (сдвиг %d); иммунный %d; ", a, b2, 6, c);
        int dt = run(2, 0, true, false, mv, bd);
        if (dt < 2 || dt > 12 || bd < 2 || bd > 12 || mv != 2) ok = false;
        d += fmt("в существо: толкаемый %d, преграда %d, сдвиг %d", dt, bd, mv);
        T("T117",
          "§этап 1 урон при столкновении: стена/существо на пути -> Nк6 силового поля толкаемому (и "
          "преграде-существу); без столкновения и без кубиков урона нет",
          ok, d);
    }
    // T118 Отход бонусным действием
    if (TB()) {
        auto trial = [&](bool atom, int& react, int& far, int& att) {
            react = far = att = 0;
            int N = 200;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss ar = plain(1000, 1, 6, 60);
                ar.disengB = atom;
                Boss me = plain(1000, 1, 6, 5);
                w.f.push_back(mk(ar, 0, 50, 50));
                w.f.push_back(mk(me, 1, 51, 50));
                long long a0 = g_attacksMade;
                takeTurn(w, 0);
                react += w.f[1].reactionUsed;
                far += distFt(w.f[0], w.f[1]) >= 10;
                att += (g_attacksMade - a0) >= 1;
            }
            return 0;
        };
        int r1, f1, a1, r0, f0, a0;
        trial(true, r1, f1, a1);
        trial(false, r0, f0, a0);
        T("T118",
          "§этап 1 Отход бонусным действием: стрелок, до которого дотянулся ближник, отходит без провоцированной атаки "
          "и всё равно атакует",
          r1 == 0 && f1 == 200 && a1 == 200,
          fmt("с атомом: реакций врага %d/200, отошёл %d/200, атаковал %d/200; без атома: реакций %d, отошёл %d, "
              "атаковал %d",
              r1, f1, a1, r0, f0, a0));
    }
    // T119 ИИ учитывает иммунитет и столкновение
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss tb = plain(1000, 0, 0, 5);
        Boss ti = tb;
        ti.immPush = true;
        w.f.push_back(mk(plain(10, 0, 0, 5), 0, 50, 50));
        w.f.push_back(mk(tb, 1, 51, 50));
        w.f.push_back(mk(ti, 1, 51, 52));
        w.wall[50][55] = true;
        w.wall[52][80] = true;
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.pushFt = 30;
        Spec sc = s;
        sc.collN = 3;
        double e0 = evSpecOne(w, s, w.f[0], w.f[1]), e1 = evSpecOne(w, sc, w.f[0], w.f[1]),
               ei = evSpecOne(w, sc, w.f[0], w.f[2]);
        T("T119",
          "§этап 1 ИИ: толчок против иммунной цели ничего не стоит; столкновение со стеной добавляет ожидаемый урон, "
          "свободный путь — нет",
          e0 > 0 && e1 > e0 + 9 && ei == 0 && evSpecOne(w, sc, w.f[0], w.f[2]) == 0,
          fmt("толчок %.2f, с 3к6 столкновения у стены %.2f, иммунная цель %.2f", e0, e1, ei));
    }
    // T120 частота в генераторе
    if (TB()) {
        Coef C2 = CC();
        int N = 4000, ip = 0, db = 0, co = 0;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, false);
            ip += b.immPush;
            db += b.disengB;
            bool c = false;
            for (auto& sp : b.abil)
                if (sp.collN && sp.pushFt) c = true;
            co += c;
        }
        T("T120", "§этап 1 генератор выбирает новые атомы: каждый встречается и не чаще трети боссов",
          ip > N * 0.01 && db > N * 0.01 && co > N * 0.003 && ip < N / 3 && db < N / 3 && co < N / 3,
          fmt("иммунитет к отталкиванию %.1f%%, Отход бонусным %.1f%%, столкновение %.1f%% боссов", 100.0 * ip / N,
              100.0 * db / N, 100.0 * co / N));
    }

    // ===== Этап 1, партия 2: Перенаправление, условные защиты, бафф характеристик =====
    // T121 цена
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.range = 0;
        s.sbStat = STR;
        s.sbAmt = 2;
        s.sbRounds = 5;
        long long p1 = specPrice(s, C2);
        Spec s2 = s;
        s2.sbAmt = 4;
        Spec s3 = s;
        s3.sbRounds = 10;
        long long p2 = specPrice(s2, C2), p3 = specPrice(s3, C2);
        Boss b = plain(100, 1, 6, 5);
        long long b0 = priceOf(b, C2, 800);
        Boss r = b;
        r.redirect = true;
        // условная защита — правило «когда → что» (условие о себе → резист)
        Boss c1 = b;
        c1.rules.push_back(Rule{W_HPHIGH, 0, E_RES});
        Boss c2 = b;
        c2.rules.push_back(Rule{W_HPHIGH, 0, E_RESNM});
        Boss c3 = b;
        c3.rules.push_back(Rule{W_NOCOND, 0, E_RES});
        Boss c4 = b;
        c4.rules.push_back(Rule{W_HPLOW, 0, E_RES});
        long long dc1 = priceOf(c1, C2, 800) - b0, dc2 = priceOf(c2, C2, 800) - b0, dc3 = priceOf(c3, C2, 800) - b0,
                  dc4 = priceOf(c4, C2, 800) - b0, dr = priceOf(r, C2, 800) - b0;
        bool ok = std::llabs(p1 - (long long)std::ceil(C2[AT_STATBUF])) <= 1 && std::llabs(p2 - 2 * p1) <= 1 &&
                  std::llabs(p3 - 2 * p1) <= 1 && dr == (long long)std::ceil(C2[AT_REDIRECT]) &&
                  dc1 == (long long)std::ceil(C2[AT_E_RES] * 5 * C2[AT_W_HPHIGH]) && dc2 < dc1 && dc3 > dc1 && dc1 > dc4;
        T("T121",
          "§этап 1 цена: бафф характеристики ∝ величине и длительности; Перенаправление — свой коэффициент; условная "
          "защита ∝ доле времени условия и доле урона",
          ok,
          fmt("бафф +2/5р %lld, +4/5р %lld, +2/10р %lld; Перенаправление +%lld; защита: ХП>50%% всё +%lld, ХП>50%% "
              "физ. +%lld, нет состояний +%lld, ХП<=50%% +%lld",
              p1, p2, p3, dr, dc1, dc2, dc3, dc4));
    }
    // T122 бафф характеристики: действует, не складывается, кончается, снимается срывом концентрации
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss b = plain(1000, 1, 6, 5);
        b.stats = {12, 10, 10, 10, 10, 10};
        Spec sp;
        sp.self = true;
        sp.delivery = 2;
        sp.range = 0;
        sp.sbStat = STR;
        sp.sbAmt = 4;
        sp.sbRounds = 2;
        b.abil.push_back(sp);
        b.abil.push_back(sp);
        b.abil[1].conc = true;
        w.f.push_back(mk(b, 0, 50, 50));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 52, 50));
        Fighter& f = w.f[0];
        int m0 = f.wMod();
        doSpec(w, f, 0, 0);
        int s1 = f.b.stats[STR], m1 = f.wMod();
        doSpec(w, f, 0, 0);
        int s2 = f.b.stats[STR];
        endOfTurn(f);
        int s3 = f.b.stats[STR];
        endOfTurn(f);
        int s4 = f.b.stats[STR];
        f.uses[1] = UNL;
        doSpec(w, f, 1, 0);
        f.concSpec = 1;
        int s5 = f.b.stats[STR];
        breakConc(w, 0);
        int s6 = f.b.stats[STR];
        T("T122",
          "§этап 1 бафф характеристики: +к значению сразу, повторный не складывается, кончается через заданные раунды, "
          "срыв концентрации снимает",
          s1 == 16 && m1 == m0 + 2 && s2 == 16 && s3 == 16 && s4 == 12 && s5 == 16 && s6 == 12,
          fmt("СИЛ 12 -> %d (мод. оружия %d -> %d), повтор %d, через 1 р. %d, через 2 р. %d; под концентрацией %d, "
              "после срыва %d",
              s1, m0, m1, s2, s3, s4, s5, s6));
    }
    // T123 условная защита
    if (TB()) {
        auto taken = [&](int tr, int sc, int hp, int type, bool mg, bool cond, unsigned res) {
            Boss b = plain(1000, 0, 0, 5);
            if (tr) b.rules.push_back(Rule{tr == 1 ? W_HPHIGH : (tr == 2 ? W_HPLOW : W_NOCOND), 0, sc == 2 ? E_RESNM : E_RES});   // условная защита — правило
            b.resMask = res;
            Fighter f = mk(b, 0, 1, 1);
            f.hp = hp;
            if (cond) f.cond[C_POISON] = 2;
            hurt(f, 100, type, mg);
            return hp - f.hp;
        };
        int a1 = taken(1, 1, 900, DT_FIRE, true, false, 0), a2 = taken(1, 1, 400, DT_FIRE, true, false, 0),
            b1 = taken(2, 1, 400, DT_FIRE, true, false, 0), b2 = taken(2, 1, 900, DT_FIRE, true, false, 0),
            c1 = taken(3, 1, 900, DT_FIRE, true, false, 0), c2 = taken(3, 1, 900, DT_FIRE, true, true, 0),
            d1 = taken(1, 2, 900, DT_SLASHING, false, false, 0), d2 = taken(1, 2, 900, DT_FIRE, true, false, 0),
            d3 = taken(1, 2, 900, DT_SLASHING, true, false, 0),
            e1 = taken(1, 1, 900, DT_FIRE, true, false, 1u << DT_FIRE), e0 = taken(0, 0, 900, DT_FIRE, true, false, 0);
        T("T123",
          "§этап 1 условная защита: резист (×0.5) только пока верно условие (ХП>50% / ХП<=50% / нет состояний); "
          "область «немагический физический» не режет магию; с обычным резистом не складывается",
          a1 == 50 && a2 == 100 && b1 == 50 && b2 == 100 && c1 == 50 && c2 == 100 && d1 == 50 && d2 == 100 &&
              d3 == 100 && e1 == 50 && e0 == 100,
          fmt("ХП>50%%: %d/%d; ХП<=50%%: %d/%d; нет состояний: %d/%d; физ. немагич.: %d, огонь %d, магич. меч %d; "
              "поверх резиста огня %d; без защиты %d",
              a1, a2, b1, b2, c1, c2, d1, d2, d3, e1, e0));
    }
    // T124 Перенаправление
    if (TB()) {
        auto trial = [&](bool atom, bool ally, int& bossHit, int& allyHit, int& swapped, int& react) {
            bossHit = allyHit = swapped = react = 0;
            int N = 300;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss at = plain(10, 1, 6, 60);
                at.wAtt = 2;
                Boss bo = plain(1000, 0, 0, 5);
                bo.redirect = atom;
                Boss mn = plain(1000, 0, 0, 5);
                w.f.push_back(mk(at, 0, 45, 50));
                w.f.push_back(mk(bo, 1, 50, 50));
                if (ally) w.f.push_back(mk(mn, 1, 51, 50));
                doWeapon(w, w.f[0], 1);
                bossHit += w.f[1].hp < 1000;
                if (ally) allyHit += w.f[2].hp < 1000;
                swapped += (ally && w.f[1].x == 51 && w.f[2].x == 50);
                react += w.f[1].reactionUsed;
            }
            return 0;
        };
        int bh, ah, sw, re, bh0, ah0, sw0, re0, bh2, ah2, sw2, re2;
        trial(true, true, bh, ah, sw, re);
        trial(false, true, bh0, ah0, sw0, re0);
        trial(true, false, bh2, ah2, sw2, re2);
        T("T124",
          "§этап 1 Перенаправление: первая атака по боссу достаётся соседнему союзнику (они меняются местами), реакция "
          "тратится, вторая бьёт босса; без союзника или без атома — нет",
          sw == 300 && re == 300 && ah > 270 && bh > 270 && sw0 == 0 && ah0 == 0 && bh0 > 270 && re2 == 0 && bh2 > 270,
          fmt("с атомом: обмен %d/300, урон союзнику %d, боссу %d (от второй атаки); без атома: обмен %d, союзнику %d, "
              "боссу %d; без союзника: реакций %d, боссу %d",
              sw, ah, bh, sw0, ah0, bh0, re2, bh2));
    }
    // T125 частота в генераторе
    if (TB()) {
        Coef C2 = CC();
        int N = 6000, cd = 0, rd = 0, sbn = 0, rdNoMin = 0, mins = 0;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, true);
            cd += hasResRule(b);
            rd += b.redirect;
            if (b.redirect && !hasSummon(b)) rdNoMin++;
            mins += hasSummon(b);
            bool x = false;
            for (auto& sp : b.abil)
                if (sp.sbAmt) x = true;
            sbn += x;
        }
        T("T125", "§этап 1 генератор выбирает новые атомы; Перенаправление — только вместе с миньонами",
          cd > N * 0.02 && rd > 0 && sbn > N * 0.01 && rdNoMin == 0 && cd < N / 3 && sbn < N / 3,
          fmt("условная защита %.1f%%, Перенаправление %d (из %d боссов с миньонами, без миньонов: %d), бафф "
              "характеристики %.1f%% боссов",
              100.0 * cd / N, rd, mins, rdNoMin, 100.0 * sbn / N));
    }
