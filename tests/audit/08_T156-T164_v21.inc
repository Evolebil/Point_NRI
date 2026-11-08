// Проверки движка, часть 8 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== v21, этап 0 =====
    // T156: границы калибровки lo <= init <= hi у всех атомов (№14); проверка при запуске (adBoundsOk) тоже проходит
    if (TB()) {
        int bad = 0, floorEq = 0;
        for (int a = 0; a < AT_N; a++) {
            if (!(AD[a].lo <= AD[a].init && AD[a].init <= AD[a].hi)) bad++;
            // №15: пол на уровне цены только у фиксированных. Исключения — атомы, которые калибровка этапа 4 (на сети,
            // 2026-11-02) сама довела до естественной нижней границы: надбавки «без броска» и «реакция» до 1 (без
            // надбавки), скидка концентрации до 0.1, владение спасброском до 2
            const bool hitFloor = a == AT_AUTO || a == AT_REACT || a == AT_CONC || a == AT_SAVEPROF;
            if (AD[a].mode != 2 && AD[a].lo >= 0.99 * AD[a].init && !hitFloor) floorEq++;
        }
        T("T156", "§v21 №14–15: у каждого атома lo <= init <= hi; lo = init только у фиксированных атомов", bad == 0 && floorEq == 0 && adBoundsOk(false),
          fmt("нарушений %d, нефиксированных с lo = init %d", bad, floorEq));
    }
    // T157: безоружный (№2) не атакует оружием, но парирует и контратакует кулаком; без способностей ход — Уклонение
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss u = plain(1000, 0, 0, 5);
        Boss e = plain(1000, 1, 6, 5);
        w.f.push_back(mk(u, 0, 10, 10));
        w.f.push_back(mk(e, 1, 11, 10));
        const int h0 = w.f[1].hp;
        doWeapon(w, w.f[0], 1);
        const bool noHit = w.f[1].hp == h0 && evWeapon(w, w.f[0], w.f[1]) == 0;
        w.f[0].init = 2000;
        takeTurn(w, 0);
        const bool dodged = w.f[0].dodging && w.f[1].hp == h0;
        int par = 0, rip = 0, N = 400;
        for (int i = 0; i < N; i++) {
            World v;
            std::memset(v.wall, 0, sizeof v.wall);
            std::memset(v.climb, 0, sizeof v.climb);
            Boss d = plain(100000, 0, 0, 5);
            d.ac = 10;
            d.parry = 10;
            Boss a = plain(100000, 1, 1, 5);
            v.f.push_back(mk(a, 0, 10, 10));
            v.f.push_back(mk(d, 1, 11, 10));
            bool crit;
            attackRoll(v, v.f[0], v.f[1], PROF, 5, crit);
            par += v.f[1].reactionUsed;
            Boss r = plain(100000, 0, 0, 5);
            r.riposte = true;
            r.ac = -1000;
            v.f[1] = mk(r, 1, 11, 10);
            v.f[1].b.stats[STR] = 14;   // кулак: 1 + мод. СИЛ (+2) = 3
            const int ha = v.f[0].hp;
            doWeapon(v, v.f[0], 1);
            if (v.f[1].reactionUsed && ha - v.f[0].hp >= 1 && ha - v.f[0].hp <= 6) rip++;
        }
        T("T157", "§v21 №2: безоружный не атакует оружием (Уклонение, если нечем действовать), но парирует и контратакует кулаком",
          noHit && dodged && par > N * 0.3 && rip > N * 0.85,
          fmt("удара нет %d, ход — Уклонение %d; парирований %d/%d, контратак кулаком %d/%d", noHit, dodged, par, N, rip, N));
    }
    // T158: концентрация — у каждого попадания своя СЛ max(10, урон/2); урон во временные хиты тоже требует проверки
    if (TB()) {
        World w;
        Boss c = plain(1000, 0, 0, 5);
        w.f.push_back(mk(c, 0, 10, 10));
        Fighter& f = w.f[0];
        f.concSpec = 0;
        hurt(f, 40, DT_FIRE, true);
        hurt(f, 2, DT_FIRE, true);
        const bool perHit = f.concDCs.size() == 2 && f.concDCs[0] == 20 && f.concDCs[1] == 10;
        f.concDCs.clear();
        f.temp = 50;
        const int hp0 = f.hp;
        hurt(f, 30, DT_FIRE, true);
        const bool tempCheck = f.hp == hp0 && f.temp == 20 && f.concDCs.size() == 1 && f.concDCs[0] == 15;
        T("T158", "§v21: концентрация — своя СЛ на каждое попадание; урон, поглощённый временными хитами, тоже даёт проверку",
          perHit && tempCheck, fmt("СЛ по попаданиям верны %d, проверка при уроне во временные хиты %d", perHit, tempCheck));
    }
    // T159: метательное оружие вплотную бьёт как ближнее (без помехи за врага рядом), боеприпасное — с помехой
    if (TB()) {
        auto dis = [&](bool ammo) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 30);
            a.wAmmo = ammo;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 11, 10));
            bool adv, d;
            advState(w, w.f[0], w.f[1], wpAtkRange(w.f[0].b, distFt(w.f[0], w.f[1])), adv, d);
            return d;
        };
        const bool thrownOk = !dis(false), bowDis = dis(true);
        T("T159", "§v21: метательное оружие вплотную — ближняя атака без помехи; лук вплотную — с помехой", thrownOk && bowDis,
          fmt("метательное без помехи %d, лук с помехой %d", thrownOk, bowDis));
    }
    // T160: №3 — 1 единица = +1 КД, цена за тот же КД прежняя (tri(2a)); КД генератора не выше 40; старое описание даёт тот же КД
    if (TB()) {
        Coef C2 = CC();
        bool costOk = true;
        for (int a = 0; a <= 30; a++) costOk = costOk && acCost(a) == tri(2 * a);
        int bad = 0;
        for (long long B : {200LL, 1600LL, 100000LL})
            for (int i = 0; i < 300; i++) {
                Boss b = makeBoss(B, C2, false);
                if (b.ac != calcAC(b) || b.acV > AC_MAX - 10) bad++;
            }
        auto acOf = [&](const char* t, long long& cost) {
            bool ok = true;
            Boss b;
            long long bu = 0;
            std::string e;
            chr::buildCharacter(parseJson(t, &ok), C2, b, bu, e);
            cost = 0;
            for (auto& it : bossItems(b, C2, (double)bu))
                if (it.cat == "ac") cost = it.cost;
            return b.ac;
        };
        long long cOld = 0, cNew = 0;
        const int acOld = acOf("{\"budget\":800,\"acV\":8}", cOld), acNew = acOf("{\"v\":21,\"budget\":800,\"acV\":4}", cNew);
        T("T160", "§v21 №3: 1 единица = +1 КД, цена КД 14 прежняя (36), КД генератора <= 40, старое описание (acV 8) — тот же КД",
          costOk && bad == 0 && acOld == 14 && acNew == 14 && cOld == 36 && cNew == 36,
          fmt("цена tri(2a) %d, нарушений у генератора %d, КД старого/нового описания %d/%d, цена %lld/%lld", costOk, bad, acOld, acNew, cOld, cNew));
    }
    // T161: №7 — скорость покупается вся, с нуля, шагами по 5 фт; у боссов генератора она есть всегда; боец ходит на купленную
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        bool grows = speedUnits(0) == 0;
        for (int ft = 5; ft <= 120; ft += 5) grows = grows && speedUnits(ft) > speedUnits(ft - 5);
        int none = 0, std30 = 0, N = 0, other5 = 0;
        for (long long B : {20LL, 800LL, 6400LL})
            for (int i = 0; i < 400; i++, N++) {
                Boss b = makeBoss(B, C2, false);
                if (b.speed <= 0) none++;
                if (b.speed == 30) std30++;
                if (b.speed % 5) other5++;
            }
        Fighter f;
        f.b.speed = 15;
        T("T161", "§v21 №7: скорость с нуля шагами по 5 фт (цена растёт с каждой ступенью), у каждого босса есть, обычно 30 фт; боец ходит на купленную",
          grows && none == 0 && other5 == 0 && std30 > N * 0.7 && std30 < N * 0.95 && speedOf(f) == 15,
          fmt("цена растёт %d, без скорости %d, ровно 30 фт %d из %d, не кратно 5 %d, скорость бойца %d", grows, none, std30, N, other5, speedOf(f)));
    }
    // T162: №8 — характеристика до 40; произвольные пределы конструктора сняты, технические остаются
    if (TB()) {
        Coef C2 = CC();
        int over = 0, at40 = 0;
        for (int i = 0; i < 300; i++) {
            Boss b = makeBoss(100000, C2, false);
            for (int st : b.stats) {
                over += st > STAT_MAX;
                at40 += st == STAT_MAX;
            }
        }
        auto okChar = [&](const char* t) {
            bool ok = true;
            Boss b;
            long long bu = 0;
            std::string e;
            return chr::buildCharacter(parseJson(t, &ok), C2, b, bu, e);
        };
        const bool stat40 = okChar("{\"budget\":5000,\"stats\":[40,10,10,10,10,10]}");
        auto valid = [&](const char* t) {   // только проверка правил, без цены (широкие значения дороже любого бюджета)
            bool ok = true;
            Boss b;
            long long bu = 0;
            int hpw = 0;
            std::string e;
            return chr::bossFromJson(parseJson(t, &ok), b, bu, hpw, e);
        };
        const bool wide = valid("{\"budget\":100000,\"weapon\":{\"n\":100,\"s\":6,\"att\":50,\"reach\":30},\"passives\":{\"legAct\":8,\"parry\":12,\"speed\":250}}");
        const bool techCap = !valid("{\"budget\":800,\"weapon\":{\"n\":1,\"s\":6,\"range\":505}}");
        T("T162", "§v21 №8: характеристика до 40 (генератор и конструктор); снятые пределы (100 кубиков, 50 атак, досягаемость 30, ЛД 8, скорость 250) принимаются; дальность > 500 фт — нет",
          over == 0 && at40 > 0 && stat40 && wide && techCap, fmt("выше 40: %d, ровно 40: %d; 40 принято %d, широкие пределы %d, 505 фт отклонено %d", over, at40, stat40, wide, techCap));
    }
    // T163: №11 — генератор берёт все кубики списка; цена урона считается по среднему (1к20 = 3к6 = 7к2 по 10.5)
    if (TB()) {
        Coef C2 = CC();
        std::set<int> seenW, seenA;
        for (int i = 0; i < 4000; i++) {
            Boss b = makeBoss(1600, C2, false);
            if (b.wN > 0) seenW.insert(b.wS);
            for (auto& s : b.abil)
                if (s.dmgN > 0) seenA.insert(s.dmgS);
        }
        g_B = 1600;
        auto price = [&](int n, int sd) {
            Spec s;
            s.delivery = 1;
            s.range = 30;
            s.dmgN = n;
            s.dmgS = sd;
            return specPrice(s, C2);
        };
        const long long p20 = price(1, 20), p6 = price(3, 6), p2 = price(7, 2), p100 = price(2, 100), p28 = price(28, 6), p29 = price(29, 6);
        T("T163", "§v21 №11: все кубики к2…к100 встречаются у оружия и способностей; цена по среднему: 1к20 = 3к6 = 7к2 (по 10.5); 2к100 (101) между 28к6 и 29к6",
          (int)seenW.size() == DICE_N && (int)seenA.size() == DICE_N && p20 == p6 && p2 == p6 && p28 <= p100 && p100 <= p29,
          fmt("размеров у оружия %zu, у способностей %zu из %d; цены 1к20 %lld, 3к6 %lld, 7к2 %lld; 28к6 %lld <= 2к100 %lld <= 29к6 %lld", seenW.size(), seenA.size(), DICE_N, p20, p6, p2, p28, p100, p29));
    }
    // T164: №1 — любой бюджет от MIN_BUDGET до MAX_BUDGET: босс не дороже бюджета и разбор цены совпадает с пересчётом
    if (TB()) {
        Coef C2 = CC();
        int over = 0, mism = 0, N = 0;
        for (long long B : {20LL, 37LL, 555LL, 1234LL, 4321LL, 25000LL, 99999LL})
            for (int i = 0; i < 150; i++, N++) {
                Boss b = makeBoss(B, C2, true);
                if (b.cost - STAT_BASE > B) over++;
                if (priceOf(b, C2, (double)(B + STAT_BASE)) != b.cost) mism++;
            }
        const Boss big = makeBoss(MAX_BUDGET * 3, C2, false);
        T("T164", "§v21 №1: бюджет любым числом (20…100000): босс не дороже бюджета, цена пересчитывается; выше предела — как на пределе",
          over == 0 && mism == 0 && big.cost - STAT_BASE <= MAX_BUDGET, fmt("дороже бюджета %d, расхождений цены %d из %d", over, mism, N));
    }
