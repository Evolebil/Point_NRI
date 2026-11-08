// Проверки движка, часть 3 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 1, 5);
        a.rN = 50;
        a.rS = 1;
        a.rType = DT_FIRE;
        a.wType = DT_SLASHING;
        Boss d = plain(1000000, 0, 0, 5);
        d.ac = -1000;
        auto run = [&](unsigned res, unsigned imm, unsigned vul) {
            std::set<int> v;
            Boss dd = d;
            dd.resMask = res;
            dd.immMask = imm;
            dd.vulnMask = vul;
            for (int i = 0; i < 600; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(dd, 1, 11, 10));
                int hp0 = w.f[1].hp;
                doWeapon(w, w.f[0], 1);
                int D = hp0 - w.f[1].hp;
                if (D > 0) v.insert(D);
            }
            return v;
        };
        auto r1 = run(1u << DT_FIRE, 0, 0), r2 = run(0, 1u << DT_FIRE, 0), r3 = run(0, 0, 1u << DT_FIRE),
             r0 = run(0, 0, 0);
        bool ok = r0 == std::set<int>{51, 102} && r1 == std::set<int>{26, 52} && r2 == std::set<int>{1, 2} &&
                  r3 == std::set<int>{101, 202};
        T("T70",
          "§довесок: свой тип урона и защиты цели (обычный 51, резист 26, иммунитет 1, уязвимость 101; крит удваивает)",
          ok,
          fmt("обычный: %zu знач., резист: %zu, иммунитет: %zu, уязв.: %zu", r0.size(), r1.size(), r2.size(),
              r3.size()));
    }
    if (TB()) {
        auto rate = [&](int cond, int statv, int conStat) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 1, 5);
            a.stats = {30, 10, 10, 10, 10, 10};
            a.mainStat = STR;
            a.rCond = cond;
            a.rRounds = 2;
            Boss d = plain(1000000, 0, 0, 5);
            d.ac = -1000;
            d.stats = {statv, statv, statv, statv, statv, statv};
            (void)conStat;
            int ap = 0, N = 3000;
            for (int i = 0; i < N; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                doWeapon(w, w.f[0], 1);
                if (w.f[1].cond[cond] > 0) ap++;
            }
            return ap / (double)N;
        };
        double weak = rate(C_STUN, 1, 0), strong = rate(C_STUN, 30, 0), gr = rate(C_GRAPPLE, 30, 0),
               wis = rate(C_FRIGHT, 30, 0);
        T("T71",
          "§довесок: состояние — спасбросок по смыслу (оглушён→ТЕЛ, испуган→МДР), у слабого проходит всегда, у "
          "сильного ~45%; захват без спасброска",
          weak > 0.9 && strong > 0.3 && strong < 0.55 && gr > 0.9 && wis > 0.3 && wis < 0.55,
          fmt("слабый %.2f, сильный (ТЕЛ30) %.2f, захват vs СИЛ30 %.2f, испуган vs МДР30 %.2f (попадание 0.95)", weak,
              strong, gr, wis));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(1000, 1, 6, 5);   // цель вооружена: оглушение ценно тем, что она не ударит (безоружный не бьёт)
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        double base = evWeapon(w, w.f[0], w.f[1]);
        w.f[0].b.rN = 2;
        w.f[0].b.rS = 6;
        w.f[0].b.rType = DT_FIRE;
        double dm = evWeapon(w, w.f[0], w.f[1]);
        w.f[0].b.rN = 0;
        w.f[0].b.rCond = C_STUN;
        w.f[0].b.rRounds = 1;
        double cd = evWeapon(w, w.f[0], w.f[1]);
        w.f[0].b.rCond = 0;
        w.f[0].b.rN = 2;
        w.f[1].b.immMask = 1u << DT_FIRE;
        double imm = evWeapon(w, w.f[0], w.f[1]);
        T("T72",
          "ИИ учитывает довесок: урон и состояние повышают ценность удара, иммунитет цели к типу довеска — обнуляет "
          "урон довеска",
          dm > base && cd > base && std::fabs(imm - base) < 1e-9,
          fmt("база %.2f, +2к6 огня %.2f, +оглушение %.2f, иммунитет к огню %.2f", base, dm, cd, imm));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Boss b = plain(100, 1, 6, 5);
        long long p[5];
        b.rN = 1;
        b.rS = 6;
        p[0] = riderPrice(b, C2);
        b.rN = 3;
        p[1] = riderPrice(b, C2);
        b.rN = 0;
        b.rCond = C_PRONE;
        p[2] = riderPrice(b, C2);
        b.rCond = C_STUN;
        b.rRounds = 1;
        p[3] = riderPrice(b, C2);
        b.rRounds = 3;
        p[4] = riderPrice(b, C2);
        Boss c = b;
        g_B = 1600;
        long long big = riderPrice(c, C2);
        g_B = 800;
        T("T73",
          "§довесок: цена растёт с кубиками и силой состояния/длительностью, состояние масштабируется с бюджетом ⚖",
          p[1] > p[0] && p[3] > p[2] && p[4] > p[3] && big >= 2 * p[4] - 1,
          fmt("1к6: %lld, 3к6: %lld, сбит: %lld, оглушение 1р: %lld, 3р: %lld; на бюджете 1600: %lld", p[0], p[1], p[2],
              p[3], p[4], big));
    }
    if (TB()) {
        int rd = 0, both = 0, wpn = 0, N = 8000;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (b.wN > 0) wpn++;
            if (b.rcost > 0) {
                rd++;
                if (b.rN > 0 && b.rCond > 0) both++;
            }
        }
        T("T74", "(инфо) доля боссов с довеском среди вооружённых; доля с обоими эффектами", rd > 0,
          fmt("%.1f%% (оба эффекта %.1f%% от довесков); вооружённых %d из %d", 100.0 * rd / wpn,
              100.0 * both / std::max(1, rd), wpn, N));
    }

    // T75..T79 временные хиты и бонус КД на себя
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.temp = 10;
        hurt(f, 4, DT_FIRE, true);
        int a = f.hp, ta = f.temp;
        hurt(f, 10, DT_FIRE, true);
        int b2 = f.hp, tb = f.temp;
        f.b.immMask = 1u << DT_COLD;
        f.temp = 5;
        hurt(f, 50, DT_COLD, true);
        int tc = f.temp;
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss bb = plain(100, 0, 0, 5);
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.tempN = 1;
        bb.abil.push_back(s);
        w.f.push_back(mk(bb, 0, 10, 10));
        w.f[0].hp = 50;
        int mx = 0;
        for (int i = 0; i < 300; i++) {
            doSpec(w, w.f[0], 0, 0);
            mx = std::max(mx, w.f[0].temp);
        }
        T("T75",
          "§временные хиты: поглощают урон первыми; не лечат; не складываются (берётся большее); иммунитет урона не "
          "тратит их",
          a == 100 && ta == 6 && b2 == 96 && tb == 0 && tc == 5 && w.f[0].hp == 50 && mx <= 6 && mx >= 5,
          fmt("10 вр.хп, удар 4: хп %d, вр.хп %d; затем удар 10: хп %d, вр.хп %d; иммунный удар: вр.хп %d; 300 "
              "применений 1к6: макс %d, хп цели %d",
              a, ta, b2, tb, tc, mx, w.f[0].hp));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 15;
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.acB = 4;
        s.acRounds = 2;
        d.abil.push_back(s);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        auto rate = [&]() {
            int h = 0, N = 60000;
            for (int i = 0; i < N; i++) {
                bool c;
                w.f[1].reactionUsed = false;
                h += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            }
            return h / (double)N;
        };
        double p0 = rate();
        doSpec(w, w.f[1], 0, 0);
        double p1 = rate();
        endOfTurn(w.f[1]);
        double p2 = rate();
        endOfTurn(w.f[1]);
        double p3 = rate();
        T("T76", "§бонус КД: +4 к КД снижает шанс попадания (КД 15→19: 40%→20%), действует ровно 2 своих хода",
          std::fabs(p0 - 0.40) < 0.012 && std::fabs(p1 - 0.20) < 0.012 && std::fabs(p2 - 0.20) < 0.012 &&
              std::fabs(p3 - 0.40) < 0.012,
          fmt("без: %.3f, с бонусом: %.3f, после 1 хода: %.3f, после 2: %.3f", p0, p1, p2, p3));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Spec st;
        st.self = true;
        st.delivery = 2;
        st.tempN = 3;
        Spec sa;
        sa.self = true;
        sa.delivery = 2;
        sa.acB = 3;
        sa.acRounds = 5;
        sa.bonus = true;
        a.abil.push_back(st);
        a.abil.push_back(sa);
        Boss d = plain(100, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 18, 10));
        w.f[0].init = 2000;
        w.f[1].init = 1;
        takeTurn(w, 0);
        T("T77", "ИИ применяет временные хиты и бонус КД перед боем (враг на подходе)",
          w.f[0].temp > 0 && w.f[0].acBuffRounds > 0,
          fmt("вр.хп %d, бонус КД %d на %d р.", w.f[0].temp, w.f[0].acBuff, w.f[0].acBuffRounds));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.self = true;
        a.delivery = 2;
        a.tempN = 1;
        Spec b2 = a;
        b2.tempN = 4;
        Spec c;
        c.self = true;
        c.delivery = 2;
        c.acB = 2;
        c.acRounds = 5;
        c.uses = 1;   // без лимита бонус КД держится весь бой при любой длительности (доля боя = 1)
        Spec c2 = c;
        c2.acB = 6;
        Spec c3 = c;
        c3.acRounds = 25;
        g_B = 1600;
        Spec r = a;
        r.range = 120;
        r.radius = 30;
        long long pa = specPrice(a, C2), pb = specPrice(b2, C2), pc = specPrice(c, C2), pc2 = specPrice(c2, C2),
                  pc3 = specPrice(c3, C2);
        T("T78",
          "§цены: вр.хп растут с числом кубиков; бонус КД — с величиной и длительностью; дальность на «себя» не "
          "удорожает",
          pb > pa && pc2 > pc && pc3 > pc && specPrice(r, C2) == pa,
          fmt("вр.хп 1к6: %lld, 4к6: %lld; КД +1/5р: %lld, +3/5р: %lld, +1/10р: %lld", pa, pb, pc, pc2, pc3));
    }
    if (TB()) {
        int cnt[9] = {}, tot = 0;
        for (int i = 0; i < 40000; i++) {
            Spec s = randSpec(0);
            if (!s.self) continue;
            // после теста 4 ноября в одной способности бывает несколько эффектов: считаем каждый вид, доля — от всех видов
            const bool has[9] = {s.healN > 0, s.teleFt > 0, s.hide, s.buffN > 0, s.tempN > 0, s.acB > 0, s.darkR > 0, s.invisN > 0, s.sbAmt > 0};
            for (int k = 0; k < 9; k++)
                if (has[k]) { cnt[k]++; tot++; }
        }
        bool ok = true;
        std::string d;
        const char* nm[9] = {"лечение", "телепорт",    "скрытность",         "бафф", "вр.хп", "КД",
                             "тьма",    "невидимость", "бафф характеристики"};
        for (int k = 0; k < 9; k++) {
            double sh = cnt[k] / (double)tot;
            d += fmt("%s %.0f%%; ", nm[k], 100 * sh);
            if (sh < 0.08 || sh > 0.25) ok = false;
        }
        T("T79", "генератор «на себя» выдаёт все 9 видов (8–25% каждый)", ok, d);
    }

    // T80..T84 помеха/преимущество как состояния и урон по времени
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 11, 10));
        bool a, d;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool base = !a && !d;
        w.f[0].cond[C_WEAK] = 2;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool weak = !a && d;
        w.f[0].cond[C_WEAK] = 0;
        w.f[1].cond[C_LIT] = 2;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool lit = a && !d;
        w.f[0].cond[C_WEAK] = 2;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool both = a && d;
        T("T80", "§ослаблен: помеха на СВОИ атаки; подсвечен: преимущество у атакующих по нему (вместе гасятся в d20)",
          base && weak && lit && both,
          fmt("обычные %d, ослаблен (помеха) %d, подсвечен (преим.) %d, оба (преим.+помеха) %d", base, weak, lit,
              both));
    }
    if (TB()) {
        auto trial = [&](int tstat, int fire_imm, int delivery, int ostat, int& lost, int& after) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 0, 0, 5);
            a.mainStat = STR;
            a.stats = {ostat, 10, 10, 10, 10, 10};
            Boss d = plain(1000, 0, 0, 5);
            d.stats = {tstat, tstat, tstat, tstat, tstat, tstat};
            if (fire_imm) d.immMask = 1u << DT_FIRE;
            Spec sp;
            sp.delivery = delivery;
            sp.range = 30;
            sp.saveStat = DEX;
            sp.dotN = 2;
            sp.dotS = 1;
            sp.dotRounds = 3;
            sp.dtype = DT_FIRE;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 12, 10));
            hitOne(w, sp, w.f[0], w.f[1]);
            int hp0 = w.f[1].hp;
            int rr = w.f[1].dotRounds;
            for (int i = 0; i < 3; i++) takeTurn(w, 1);
            lost = hp0 - w.f[1].hp;
            takeTurn(w, 1);
            after = hp0 - w.f[1].hp - lost;
            return rr;
        };
        int l1, a1, l2, a2, l3, a3, l4, a4;
        int r1 = trial(10, 0, 2, 10, l1, a1), r2 = trial(10, 1, 2, 10, l2, a2), r3 = trial(1, 0, 1, 30, l3, a3),
            r4 = trial(30, 0, 1, 10, l4, a4);
        T("T81",
          "§урон по времени: 2 урона в начале хода цели ровно 3 хода; иммунитет к типу его гасит; со спасброском "
          "вешается лишь при провале",
          r1 == 3 && l1 == 6 && a1 == 0 && l2 == 0 && l3 == 6 && l4 == 0,
          fmt("без броска: раундов %d, урон за 3 хода %d, после %d; иммунная цель: %d; спас провален: %d; спас пройден "
              "(ЛОВ30): %d",
              r1, l1, a1, l2, l3, l4));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.delivery = 2;
        a.dotN = 1;
        a.dotS = 6;
        a.dotRounds = 1;
        Spec b2 = a;
        b2.dotN = 3;
        Spec c = a;
        c.dotRounds = 4;
        Spec d = a;
        d.dotS = 12;
        T("T82", "§цены: урон по времени растёт с числом кубиков, их размером и длительностью",
          specPrice(b2, C2) > specPrice(a, C2) && specPrice(c, C2) > specPrice(a, C2) &&
              specPrice(d, C2) > specPrice(a, C2),
          fmt("1к6×1р: %lld, 3к6×1р: %lld, 1к6×4р: %lld, 1к12×1р: %lld", specPrice(a, C2), specPrice(b2, C2),
              specPrice(c, C2), specPrice(d, C2)));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = STR;
        Boss d = plain(1000, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.dmgN = 1;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        double e0 = evSpecOne(w, s, w.f[0], w.f[1]);
        s.dotN = 2;
        s.dotS = 6;
        s.dotRounds = 3;
        double e1 = evSpecOne(w, s, w.f[0], w.f[1]);
        w.f[1].b.immMask = 1u << DT_FIRE;
        double e2 = evSpecOne(w, s, w.f[0], w.f[1]);
        Spec sl;
        sl.delivery = 2;
        sl.range = 30;
        sl.cond = C_LIT;
        sl.condRounds = 1;
        double l0 = evSpecOne(w, sl, w.f[0], w.f[1]);
        Spec sw = sl;
        sw.cond = C_WEAK;
        double w0 = evSpecOne(w, sw, w.f[0], w.f[1]);
        T("T83",
          "ИИ учитывает урон по времени (и иммунитет цели к его типу); «подсвечен/ослаблен» имеют ненулевую ценность",
          e1 > e0 && e2 == 0 && l0 > 0 && w0 > 0,
          fmt("огонь 1к6: %.2f, +DoT: %.2f, иммунная цель: %.2f; подсвечен %.2f, ослаблен %.2f", e0, e1, e2, l0, w0));
    }
    if (TB()) {
        int dot = 0, all = 0, N = 40000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            if (s.self) continue;
            all++;
            if (s.dotN) dot++;
        }
        T("T84", "(инфо) доля несамоспособностей с уроном по времени", dot > 0, fmt("%.1f%%", 100.0 * dot / all));
    }

    // T85..T91 партия 2: видимость и чувства, местность, ауры
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 16, 10));
        bool a, d;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool base = !a && !d;
        w.dark[10][16] = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool darkT = d && !a;
        w.f[0].b.dv = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool dv30 = !a && !d;
        w.f[0].b.dv = 20;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool dv20 = d && !a;
        w.f[0].b.dv = 0;
        w.dark[10][16] = false;
        w.dark[10][10] = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool darkA = a && !d;
        w.f[1].b.dv = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool darkAdv = !a && !d;
        w.f[1].b.dv = 0;
        w.dark[10][10] = false;
        w.f[1].hidden = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool hid = d && !a;
        w.f[0].b.blindsight = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool bs = !a && !d;
        w.f[0].b.blindsight = 0;
        w.f[1].hidden = false;
        w.f[0].hidden = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool hidA = a && !d;
        w.f[1].b.blindsight = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool bsA = !a && !d;
        T("T85",
          "§видимость: цель в тьме — помеха без тёмного зрения (достаточной дальности); сам в тьме — преимущество; "
          "слепое зрение видит «скрытого»",
          base && darkT && dv30 && dv20 && darkA && darkAdv && hid && bs && hidA && bsA,
          fmt("обычно %d; цель в тьме: помеха %d, тёмное зр.30 на 30 фт %d, 20 фт %d; я в тьме: преим. %d, у врага "
              "тёмное зр. %d; скрытая цель %d, слепое зр. %d; я скрыт %d, у врага слепое зр. %d",
              base, darkT, dv30, dv20, darkA, darkAdv, hid, bs, hidA, bsA));
    }
    if (TB()) {
        auto cells = [&](int fly, int ign, bool diffRow) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.fly = fly;
            a.ignDiff = ign;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 50, 10));
            if (diffRow)   // полоса во всю высоту: обойти нельзя (путь — кратчайший по цене шага)
                for (int y = 0; y < W; y++)
                    for (int x = 11; x < 50; x++) w.diff[y][x] = true;
            int used = moveToward(w, w.f[0], w.f[1], 0, 30, false);
            return std::make_pair(w.f[0].x - 10, used);
        };
        auto n = cells(0, 0, false), dd = cells(0, 0, true), fl = cells(1, 0, true), ig = cells(0, 1, true);
        T("T86",
          "§труднопроходимая местность: 10 фт за клетку (30 фт → 3 клетки вместо 6); полёт и «игнорирует» не "
          "замедляются",
          n.first == 6 && dd.first == 3 && dd.second <= 30 && fl.first == 6 && ig.first == 6,
          fmt("обычно %d клеток, по местности %d (потрачено %d фт), полёт %d, игнор %d", n.first, dd.first, dd.second,
              fl.first, ig.first));
    }
    if (TB()) {
        auto lost = [&](int vx, unsigned res, unsigned imm, int hpOwner, int auraN, int cond, int vstat,
                        bool& stunned) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss o = plain(100, 0, 0, 5);
            o.auraN = auraN;
            o.auraS = 1;
            o.auraRad = 15;
            o.auraType = DT_FIRE;
            o.auraCond = cond;
            o.auraRounds = 1;
            o.stats = {30, 10, 10, 10, 10, 10};
            o.mainStat = STR;
            Boss v = plain(1000, 0, 0, 5);
            v.resMask = res;
            v.immMask = imm;
            v.stats = {vstat, vstat, vstat, vstat, vstat, vstat};
            w.f.push_back(mk(o, 0, 10, 10));
            w.f[0].hp = hpOwner;
            w.f.push_back(mk(v, 1, vx, 10));
            int hp0 = w.f[1].hp;
            g_act = ActInfo{};
            takeTurn(w, 1);
            stunned = g_act.type == "stunned";
            return hp0 - w.f[1].hp;
        };
        bool s1, s2, s3, s4, s5, s6;
        int in = lost(13, 0, 0, 100, 50, 0, 10, s1), out = lost(14, 0, 0, 100, 50, 0, 10, s2),
            rs = lost(13, 1u << DT_FIRE, 0, 100, 50, 0, 10, s3), im = lost(13, 0, 1u << DT_FIRE, 100, 50, 0, 10, s4),
            dead = lost(13, 0, 0, 0, 50, 0, 10, s5);
        int w1 = 0, w2 = 0;
        for (int i = 0; i < 400; i++) {
            bool q;
            lost(13, 0, 0, 100, 0, C_STUN, 1, q);
            w1 += q;
            lost(13, 0, 0, 100, 0, C_STUN, 30, q);
            w2 += q;
        }
        bool st1 = w1 >= 396, st2 = w2 >= 120 && w2 <= 240;
        (void)s6;
        T("T87",
          "§аура: урон в начале хода врага в радиусе (15 фт — да, 20 — нет); защиты цели действуют; мёртвый владелец "
          "не бьёт; состояние — со спасброском",
          in == 50 && out == 0 && rs == 25 && im == 0 && dead == 0 && st1 && st2,
          fmt("в радиусе %d, снаружи %d, резист %d, иммунитет %d, владелец мёртв %d; оглушение (СЛ 20): слабый (ТЕЛ1) "
              "%d из 400, сильный (ТЕЛ30) %d из 400 (ожидается ~180)",
              in, out, rs, im, dead, w1, w2));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss bb = plain(100, 0, 0, 5);
        Spec dk;
        dk.self = true;
        dk.delivery = 2;
        dk.darkR = 20;
        Spec tr;
        tr.delivery = 2;
        tr.range = 60;
        tr.radius = 10;
        tr.shape = SH_CUBE;   // v21: квадрат клеток — это куб (по умолчанию область — сфера)
        tr.terr = true;
        bb.abil.push_back(dk);
        bb.abil.push_back(tr);
        w.f.push_back(mk(bb, 0, 50, 50));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 60, 50));
        doSpec(w, w.f[0], 0, 1);
        doSpec(w, w.f[0], 1, 1);
        int nd = 0, nf = 0;
        for (int y = 0; y < W; y++)
            for (int x = 0; x < W; x++) {
                nd += w.dark[y][x];
                nf += w.diff[y][x];
            }
        T("T88",
          "§способности: «тьма вокруг себя» радиус 20 фт = 9×9 клеток; область с местностью радиус 10 фт = 5×5 клеток "
          "вокруг цели",
          nd == 81 && nf == 25 && w.dark[50][50] && w.diff[50][60], fmt("клеток тьмы %d, труднопроходимых %d", nd, nf));
    }
    if (TB()) {
        auto run = [&](int dv, int bs) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.dv = dv;
            a.blindsight = bs;
            Spec dk;
            dk.self = true;
            dk.delivery = 2;
            dk.darkR = 20;
            a.abil.push_back(dk);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 18, 10));
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            int n = 0;
            for (int y = 0; y < W; y++)
                for (int x = 0; x < W; x++) n += w.dark[y][x];
            return n;
        };
        int with = run(60, 0), blind = run(0, 30), without = run(0, 0);
        T("T89", "ИИ ставит тьму только когда сам в ней видит (тёмное/слепое зрение), а враг нет",
          with > 0 && blind > 0 && without == 0,
          fmt("тёмное зрение: %d клеток, слепое: %d, без чувств: %d", with, blind, without));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Boss b = plain(100, 1, 6, 5);
        long long p0 = priceOf(b, C2, 800), pd[3], pb[3];
        int dv[3] = {30, 60, 120}, bs[3] = {10, 30, 60};
        for (int i = 0; i < 3; i++) {
            Boss c = b;
            c.dv = dv[i];
            pd[i] = priceOf(c, C2, 800);
            Boss e = b;
            e.blindsight = bs[i];
            pb[i] = priceOf(e, C2, 800);
        }
        Boss a1 = b;
        a1.auraRad = 10;
        a1.auraN = 1;
        a1.auraS = 6;
        Boss a2 = a1;
        a2.auraN = 3;
        Boss a3 = a1;
        a3.auraRad = 30;
        Boss a4 = a1;
        a4.auraCond = C_STUN;
        a4.auraRounds = 1;
        long long q1 = priceOf(a1, C2, 800), q2 = priceOf(a2, C2, 800), q3 = priceOf(a3, C2, 800),
                  q4 = priceOf(a4, C2, 800);
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.radius = 10;
        s.dmgN = 1;
        long long ps0 = specPrice(s, C2);
        s.terr = true;
        long long ps1 = specPrice(s, C2);
        T("T90",
          "§цены: чувства растут по ступеням; аура — с кубиками, радиусом и состоянием; местность в области — надбавка",
          pd[0] > p0 && pd[1] > pd[0] && pd[2] > pd[1] && pb[1] > pb[0] && pb[2] > pb[1] && q2 > q1 && q3 > q1 &&
              q4 > q1 && ps1 > ps0,
          fmt("тёмное зр. 30/60/120: +%lld/+%lld/+%lld; слепое 10/30/60: +%lld/+%lld/+%lld; аура 1к6 10фт %lld, 3к6 "
              "%lld, 30фт %lld, +оглушение %lld; местность +%lld",
              pd[0] - p0, pd[1] - p0, pd[2] - p0, pb[0] - p0, pb[1] - p0, pb[2] - p0, q1, q2, q3, q4, ps1 - ps0));
    }
    if (TB()) {
        int au = 0, dv = 0, bs = 0, ig = 0, N = 8000;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (b.auraRad > 0) au++;
            if (b.dv) dv++;
            if (b.blindsight) bs++;
            if (b.ignDiff) ig++;
        }
        T("T91",
          "(инфо) доли боссов: аура / тёмное зрение / слепое зрение / игнор местности (цель по бестиарию: чувства у "
          "большинства, аура ~7%)",
          au > 0 && dv > 0,
          fmt("%.1f%% / %.1f%% / %.1f%% / %.1f%%", 100.0 * au / N, 100.0 * dv / N, 100.0 * bs / N, 100.0 * ig / N));
    }

    // T92..T96 доводка состояний: опутан, испуган/в бегстве, невидимость
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.stats = {10, 10, 10, 10, 10, 10};
        auto rate = [&](int stat) {
            int ok = 0, N = 60000;
            for (int i = 0; i < N; i++) ok += saveOK(f, stat, 11);
            return ok / (double)N;
        };
        double d0 = rate(DEX), s0 = rate(STR), pf0 = pFailEst(f, DEX, 11);
        f.cond[C_RESTR] = 2;
        double d1 = rate(DEX), s1 = rate(STR), pf1 = pFailEst(f, DEX, 11), pfs = pFailEst(f, STR, 11);
        T("T92", "§опутан: помеха на спасброски ЛОВ (успех 50%→25%), остальные спасброски не меняются; ИИ учитывает",
          std::fabs(d0 - 0.5) < 0.012 && std::fabs(d1 - 0.25) < 0.012 && std::fabs(s0 - 0.5) < 0.012 &&
              std::fabs(s1 - 0.5) < 0.012 && std::fabs(pf0 - 0.5) < 1e-9 && std::fabs(pf1 - 0.75) < 1e-9 &&
              std::fabs(pfs - 0.5) < 1e-9,
          fmt("ЛОВ: %.3f→%.3f (оценка провала %.2f→%.2f), СИЛ: %.3f→%.3f", d0, d1, pf0, pf1, s0, s1));
    }
    if (TB()) {
        auto run = [&](int cond, int dist, int& enemyLost) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            Boss d = plain(100000, 1, 6, 5);
            w.f.push_back(mk(a, 0, 50, 50));
            w.f.push_back(mk(d, 1, 50 + dist / 5, 50));
            if (cond) w.f[0].cond[cond] = 3;
            w.f[0].init = 2000;
            w.f[1].init = 1;
            int hp0 = w.f[1].hp;
            takeTurn(w, 0);
            enemyLost = hp0 - w.f[1].hp;
            return distFt(w.f[0], w.f[1]);
        };
        int e1, e2, e3, e4;
        int flee = run(C_FLEE, 10, e1), fright = run(C_FRIGHT, 100, e2), free_ = run(0, 100, e3),
            frightNear = run(C_FRIGHT, 5, e4);
        T("T93",
          "§испуган: не приближается (стоит), но атакует с помехой; §в бегстве: действие на Рывок прочь (+60 фт), не "
          "атакует",
          flee == 70 && e1 == 0 && fright == 100 && free_ < 100 && e2 == 0 && e4 >= 0,
          fmt("в бегстве с 10 фт: дистанция %d, урон врагу %d; испуган с 100 фт: %d (свободный: %d); испуган вплотную: "
              "урон врагу %d",
              flee, e1, fright, free_, e4));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 1, 6, 5);
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.invisN = 2;
        d.abil.push_back(s);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        doSpec(w, w.f[1], 0, 0);
        int r0 = w.f[1].invisRounds;
        bool x, y;
        advState(w, w.f[0], w.f[1], 5, x, y);
        bool onTarget = y && !x;
        advState(w, w.f[1], w.f[0], 5, x, y);
        bool fromHim = x && !y;
        doWeapon(w, w.f[1], 0);
        int r1 = w.f[1].invisRounds;
        w.f[0].b.blindsight = 30;
        advState(w, w.f[0], w.f[1], 5, x, y);
        bool bs = !x && !y;
        endOfTurn(w.f[1]);
        endOfTurn(w.f[1]);
        int r2 = w.f[1].invisRounds;
        T("T94",
          "§невидимость: по невидимому — помеха, он бьёт с преимуществом; не спадает от атаки; слепое зрение видит; "
          "кончается через N ходов",
          r0 == 2 && onTarget && fromHim && r1 == 2 && bs && r2 == 0,
          fmt("раундов %d; по нему с помехой %d; его атака с преим. %d; после атаки %d; слепое зрение врага %d; после "
              "2 ходов %d",
              r0, onTarget, fromHim, r1, bs, r2));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.self = true;
        a.delivery = 2;
        a.invisN = 1;
        Spec b2 = a;
        b2.invisN = 4;
        Spec r = a;
        r.range = 120;
        r.radius = 30;
        T("T95", "§цены: невидимость растёт с длительностью; дальность на «себя» не удорожает",
          specPrice(b2, C2) > specPrice(a, C2) && specPrice(r, C2) == specPrice(a, C2),
          fmt("1 р.: %lld, 4 р.: %lld", specPrice(a, C2), specPrice(b2, C2)));
    }
    if (TB()) {
        auto run = [&](int bs) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            Spec s;
            s.self = true;
            s.delivery = 2;
            s.invisN = 3;
            a.abil.push_back(s);
            Boss d = plain(100, 1, 6, 5);
            d.blindsight = bs;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 18, 10));
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            return w.f[0].invisRounds;
        };
        int no = run(0), yes = run(30);
        T("T96", "ИИ уходит в невидимость перед боем, кроме случая, когда у врага слепое зрение", no > 0 && yes == 0,
          fmt("против обычного: %d р., против слепого зрения: %d", no, yes));
    }
