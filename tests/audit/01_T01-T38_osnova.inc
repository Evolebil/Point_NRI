// Проверки движка, часть 1 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // T01 модификатор
    if (TB()) {
        bool ok = true;
        for (int v = 1; v <= 30; v++) {
            int e = (v >= 10) ? (v - 10) / 2 : -((11 - v) / 2);
            if (modOf(v) != e) ok = false;
        }
        T("T01", "§2 мод = floor((V-10)/2), V=1..30", ok, "");
    }
    // T02 tri/maxV
    if (TB()) {
        bool ok = true;
        for (long long b = 0; b < 20000; b++) {
            int v = maxV(b);
            if (tri(v) > b || tri(v + 1) <= b) ok = false;
        }
        T("T02", "§2/§3 цена V(V+1)/2 и обратная maxV согласованы", ok, "");
    }
    // T03 КД
    if (TB()) {
        int bad = 0, N = 4000;
        for (int i = 0; i < N; i++) {
            long long B = 200 << irand(6);
            Boss b = makeBoss(B, C, true);
            if (b.ac != calcAC(b) || b.acV < 0 || b.acV > AC_MAX - 10) bad++;   // v21: КД = 10 + единицы + ЛОВ по доспеху + щит
        }
        T("T03", "§3 КД = 10 + floor(V/2)", bad == 0, fmt("нарушений %d из %d", bad, N));
    }
    // T04 бюджетный инвариант
    if (TB()) {
        int over = 0, overS = 0, nS = 0, N = 6000;
        long long maxOver = 0;
        double eff = 0;
        int hp1 = 0, hp1S = 0;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << irand(6);
            Boss b = makeBoss(B, C, true);
            bool s = hasSummon(b);
            if (s) nS++;
            eff += (double)(b.cost - STAT_BASE) / B;
            if (b.hp <= 1) {
                hp1++;
                if (s) hp1S++;
            }
            if (b.cost - STAT_BASE > B) {
                over++;
                if (s) overS++;
                maxOver = std::max(maxOver, b.cost - STAT_BASE - B);
            }
        }
        T("T04", "§1 стоимость босса никогда не превышает бюджет", over == 0,
          fmt("превышений %d/%d (из них с призывом %d/%d), макс. перерасход %lld, ср. использование бюджета %.1f%%, "
              "боссов с 1 ХП: %d (с призывом %d)",
              over, N, overS, nS, maxOver, 100 * eff / N, hp1, hp1S));
    }
    // T05 round-trip
    if (TB()) {
        int bad = 0, N = 6000;
        long long maxd = 0;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << irand(6);
            Boss b = makeBoss(B, C, true);
            long long p = priceOf(b, C, (double)(B + STAT_BASE));
            if (p != b.cost) {
                bad++;
                maxd = std::max(maxd, std::llabs(p - b.cost));
            }
        }
        T("T05", "обратное направление: цена(собранный босс) = записанной стоимости", bad == 0,
          fmt("расхождений %d/%d, макс. %lld очк.", bad, N, maxd));
    }
    // T06 метки атомов
    if (TB()) {
        std::map<int, int> miss;
        int bad = 0, N = 6000;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << irand(6);
            Boss b = makeBoss(B, C, true);
            AMask f = featureAtoms(b), m = b.atoms;
            if (f != m) {
                bad++;
                for (int a = 0; a < AT_N; a++)
                    if (((f >> a) & 1) != ((m >> a) & 1)) miss[a]++;
            }
        }
        std::string d = fmt("боссов с рассогласованием %d/%d", bad, N);
        for (auto& k : miss) d += fmt("; %s x%d", AD[k.first].name, k.second);
        T("T06", "§11 набор атомов, по которому калибруется цена, = реальному содержимому босса", bad == 0, d);
    }
    // T07 инварианты способностей
    if (TB()) {
        int bad1 = 0, bad2 = 0, bad3 = 0, bad4 = 0, zero = 0, N = 30000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(i % 5);
            if (s.delivery == 2 && s.cond && s.condRounds != 1) bad1++;
            if (s.cond == C_POISON && s.saveStat != CON) bad2++;
            {
                bool cd = s.cond > 0;
                if (cd && (s.cond == C_FRIGHT || s.cond == C_FLEE) && s.saveStat != WIS) bad3++;
                if (!cd && s.dmgN > 0 && (s.dtype == DT_FIRE || s.dtype == DT_LIGHTNING) && s.saveStat != DEX &&
                    s.saveStat != CON)
                    bad3++;
                if (!cd && s.dmgN > 0 && s.dtype == DT_PSYCHIC && s.saveStat != INT && s.saveStat != WIS &&
                    s.saveStat != CHA)
                    bad3++;
                if (!cd && s.dmgN > 0 && s.dtype == DT_POISON && s.saveStat != CON) bad3++;
            }
            if (s.dmgN > 0 && !isDie(s.dmgS)) bad4++;   // v21, №11: размер кубика — только из списка DICE
            g_B = 800;
            if (specPrice(s, C) <= 0) zero++;
        }
        T("T07a", "§6 без броска -> состояние ровно 1 раунд", bad1 == 0, fmt("нарушений %d", bad1));
        T("T07b", "§7 яд -> спасбросок ТЕЛ", bad2 == 0, fmt("нарушений %d", bad2));
        T("T07c",
          "§2 спасбросок по смыслу эффекта: страх→МДР; огонь/электричество→ЛОВ/ТЕЛ; психика→ИНТ/МДР/ХАР; яд→ТЕЛ",
          bad3 == 0, fmt("нарушений %d", bad3));
        T("T07d", "§6 кубик способности — из списка DICE (к2…к100, v21)", bad4 == 0, fmt("вне списка %d", bad4));
        T("T07e", "§6 цена способности >= 1 (нет бесплатных способностей)", zero == 0,
          fmt("способностей с ценой 0: %d из %d (пустая нагрузка)", zero, N));
    }
    // T08 масштаб ⚖
    if (TB()) {
        Spec s;
        s.cond = C_STUN;
        s.condRounds = 1;
        s.delivery = 1;
        s.range = 5;
        bool ok = true;
        std::string d;
        double base = 0;
        for (double B : {400., 800., 1600.}) {
            g_B = B;
            double p = specPrice(s, C);
            if (B == 800) base = p;
            d += fmt("B=%.0f:%.0f ", B, p);
        }
        g_B = 400;
        double p4 = specPrice(s, C);
        g_B = 1600;
        double p16 = specPrice(s, C);
        ok = (std::fabs(p4 - base / 2) <= 1.01) && (std::fabs(p16 - base * 2) <= 1.01);
        T("T08", "§1 цены ⚖ пропорциональны (бюджет/800)", ok, d);
    }
    // T09 d20
    if (TB()) {
        auto mean = [&](bool a, bool d) {
            double t = 0;
            int N = 200000;
            for (int i = 0; i < N; i++) t += d20(a, d);
            return t / N;
        };
        double n = mean(0, 0), ad = mean(1, 0), di = mean(0, 1), bo = mean(1, 1);
        T("T09", "§8 преимущество/помеха; вместе гасятся",
          std::fabs(n - 10.5) < 0.1 && std::fabs(ad - 13.82) < 0.1 && std::fabs(di - 7.18) < 0.1 &&
              std::fabs(bo - 10.5) < 0.1,
          fmt("средние: обычный %.2f, преим. %.2f, помеха %.2f, оба %.2f", n, ad, di, bo));
    }
    // T10 крит/промах
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss hi = plain(10, 1, 6, 5);
        hi.ac = 1000;
        Boss lo = plain(10, 1, 6, 5);
        lo.ac = -1000;
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(hi, 1, 11, 10));
        int hits = 0, crits = 0, N = 100000;
        for (int i = 0; i < N; i++) {
            bool c;
            if (attackRoll(w, w.f[0], w.f[1], PROF, 5, c)) {
                hits++;
                if (c) crits++;
            }
        }
        w.f[1].b = lo;
        int miss = 0;
        for (int i = 0; i < N; i++) {
            bool c;
            if (!attackRoll(w, w.f[0], w.f[1], PROF, 5, c)) miss++;
        }
        T("T10", "§8 нат.20 всегда попадает (крит), нат.1 всегда промах",
          std::fabs(hits / (double)N - 0.05) < 0.005 && hits == crits && std::fabs(miss / (double)N - 0.05) < 0.005,
          fmt("против КД 1000: попаданий %.1f%% (все криты: %s); против КД -1000: промахов %.1f%%", 100.0 * hits / N,
              hits == crits ? "да" : "нет", 100.0 * miss / N));
    }
    // T11 сбит с ног (v0.6): помеха на свои атаки действует только на реакции до его хода
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 11, 10));
        w.f[0].cond[C_PRONE] = 1;
        bool a, d;
        advState(w, w.f[0], w.f[1], 5, a, d);
        T("T11", "§7 (v0.6) сбитый: своя атака реакцией — с помехой; в свой ход он встаёт", d,
          fmt("advState даёт помеху сбитому атакующему: %s", d ? "да" : "нет"));
    }
    // T12 иммунитет после окончания
    if (TB()) {
        Fighter t = mk(plain(10, 0, 0, 5), 0, 1, 1);
        bool a1 = applyCond(t, C_STUN, 1, -1, 10, false);
        endOfTurn(t);
        bool a2 = applyCond(t, C_STUN, 1, -1, 10, false);
        endOfTurn(t);
        bool a3 = applyCond(t, C_STUN, 1, -1, 10, false);
        T("T12", "§7 после окончания — иммунитет до конца следующего хода", a1 && !a2 && a3,
          fmt("наложение: %d, сразу после конца: %d, через ход: %d", a1, a2, a3));
    }
    // T13 движение
    if (TB()) {
        int bad = 0, N = 3000;
        World w;
        for (int i = 0; i < N; i++) {
            genMap(w);
            w.f.clear();
            w.f.push_back(mk(plain(10, 1, 6, 5), 0, irand(100), irand(100)));
            w.f.push_back(mk(plain(10, 1, 6, 5), 1, irand(100), irand(100)));
            if (w.wall[w.f[0].y][w.f[0].x] || w.wall[w.f[1].y][w.f[1].x]) continue;
            int sp = 30;
            int x0 = w.f[0].x, y0 = w.f[0].y;
            int used = moveToward(w, w.f[0], w.f[1], 0, sp, false);
            int cells = std::max(std::abs(w.f[0].x - x0), std::abs(w.f[0].y - y0));
            if (used > sp || 5 * cells > used + 0) bad++;
        }
        T("T13", "§8 движение: потрачено <= скорости; смещение <= потраченного", bad == 0,
          fmt("нарушений %d из %d", bad, N));
    }
    // T14 провоцированные атаки
    if (TB()) {
        auto trial = [&](int mode) {
            int dmgTotal = 0, react = 0, N = 400;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss big = plain(1000, 0, 0, 5);
                // 3 — лук (боеприпасное), 5 — метательное (v21: бьёт и вблизи), 6 — безоружный (v21: не провоцирует)
                Boss g = plain(10, mode == 6 ? 0 : 1, 1, (mode == 3 || mode == 5) ? 30 : 5);
                g.wFinesse = false;
                g.wAmmo = mode == 3;
                w.f.push_back(mk(big, 0, 51, 50));
                w.f.push_back(mk(g, 1, 50, 50));
                w.f.push_back(mk(plain(10, 1, 6, 5), 1, 80, 50));
                w.f[0].disengaged = (mode == 2);
                int hp0 = w.f[0].hp;
                moveToward(w, w.f[0], w.f[2], 0, 30, false);
                if (mode == 4) {
                    hp0 = w.f[0].hp;
                    w.f[0].x = 51;
                    w.f[0].y = 50;
                    w.f[0].disengaged = false;
                    moveToward(w, w.f[0], w.f[2], 0, 30, false);
                }
                dmgTotal += hp0 - w.f[0].hp;
                react += w.f[1].reactionUsed;
            }
            return std::make_pair(dmgTotal, react);
        };
        auto r1 = trial(1), r2 = trial(2), r3 = trial(3), r4 = trial(4), r5 = trial(5), r6 = trial(6);
        T("T14a", "§8 уход из досягаемости ближнего врага -> 1 провоцированная атака",
          r1.second == 400 && r1.first > 300,
          fmt("реакций использовано %d/400, урона получено %d (ожидается ~0.95 на попытку)", r1.second, r1.first));
        T("T14b", "§8 Отход -> без провоцированной атаки", r2.first == 0 && r2.second == 0,
          fmt("урона %d, реакций %d", r2.first, r2.second));
        T("T14c", "§8 враг с луком (боеприпасное оружие) не провоцирует", r3.first == 0 && r3.second == 0,
          fmt("урона %d, реакций %d", r3.first, r3.second));
        T("T14e", "§8 v21: метательное оружие (кинжал, дротик) провоцирует, как ближнее", r5.second == 400 && r5.first > 300,
          fmt("реакций %d/400, урона %d", r5.second, r5.first));
        T("T14f", "§8 v21: безоружный не провоцирует (атаки кулаком нет)", r6.first == 0 && r6.second == 0,
          fmt("урона %d, реакций %d", r6.first, r6.second));
        T("T14d", "§8 реакция — 1 раз за раунд", r4.first == 0, fmt("урона от второй атаки %d", r4.first));
    }
    // T15/T16 боевые журналы: призыв, телепорт, старт
    if (TB()) {
        int capViol = 0, N = 300, teleToward = 0, teleAway = 0, bossCnt = 0;
        double minDist = 1e9;
        int onWall = 0;
        std::regex rf(
            "\"x\":(\\d+),\"y\":(\\d+),\"hp\":(-?\\d+),\"maxHp\":(\\d+),\"ac\":\\d+,\"team\":(\\d),\"kind\":(\\d)");
        std::regex ra("\"actor\":(-?\\d+),\"act\":\"([a-z-]+)\"");
        for (int s = 1; s <= N; s++) {
            rng.seed(s * 13);
            Boss A = makeBoss(1600, C, false);
            addSummon(A, makeBoss(150, C, false), 2);   // призыв без лимита применений: число ограничивает только потолок
            Boss B = makeBoss(1600, C, false);
            std::vector<std::string> log;
            std::string mj;
            fight(A, B, &log, &mj);
            for (size_t i = 0; i < log.size(); i++) {
                int alive[2] = {0, 0};
                for (auto it = std::sregex_iterator(log[i].begin(), log[i].end(), rf); it != std::sregex_iterator();
                     ++it) {
                    int hp = std::stoi((*it)[3]), team = std::stoi((*it)[5]), kind = std::stoi((*it)[6]);
                    if (kind >= 2 && hp > 0) alive[team]++;
                }
                if (alive[0] > limits().sumAliveAbil) capViol++;
                if (i == 0) {
                    std::vector<std::pair<int, int>> pos;
                    for (auto it = std::sregex_iterator(log[i].begin(), log[i].end(), rf); it != std::sregex_iterator();
                         ++it)
                        pos.push_back({std::stoi((*it)[1]), std::stoi((*it)[2])});
                    if (pos.size() >= 2) {
                        double d = 5.0 * std::max(std::abs(pos[0].first - pos[1].first),
                                                  std::abs(pos[0].second - pos[1].second));
                        minDist = std::min(minDist, d);
                    }
                }
            }
        }
        T("T15", "§10 (v21) живых приспешников от способности не больше технического потолка (limity.json)", capViol == 0,
          fmt("кадров с превышением %d (боёв %d)", capViol, N));
        T("T16", "(нет в спецификации) начальная дистанция между боссами", true,
          fmt("минимум в %d боях: %.0f фт — в правилах не задана, в коде >=150 фт при удаче поиска", N, minDist));
    }
    // T17 телепорт сближение/побег
    if (TB()) {
        int toward = 0, away = 0, N = 300;
        std::regex rf("\"x\":(\\d+),\"y\":(\\d+),\"hp\":(-?\\d+),\"maxHp\":(\\d+)");
        std::regex ra("\"actor\":(-?\\d+),\"act\":\"([a-z-]+)\"");
        for (int seed = 1; seed <= N; seed++) {
            rng.seed(seed);
            Boss A = makeBoss(800, C, false);
            Spec t;
            t.self = true;
            t.delivery = 2;
            t.teleFt = 60;
            t.bonus = true;
            t.price = 1;
            A.abil.clear();
            A.abil.push_back(t);
            A.wN = 2;
            A.wS = 6;
            A.wRange = 5;
            A.wAtt = 1;
            A.stats[INT] = 10;   // правило бота (с поставленной сетью ИНТ 12+ ходил бы сетью)
            Boss B = makeBoss(800, C, false);
            std::vector<std::string> log;
            std::string mj;
            fight(A, B, &log, &mj);
            for (size_t i = 1; i < log.size(); i++) {
                std::smatch m;
                if (!std::regex_search(log[i], m, ra) || m[2] != "teleport" || std::stoi(m[1]) != 0) continue;
                std::vector<std::array<int, 4>> v, u;
                for (auto it = std::sregex_iterator(log[i].begin(), log[i].end(), rf); it != std::sregex_iterator();
                     ++it)
                    v.push_back({std::stoi((*it)[1]), std::stoi((*it)[2]), std::stoi((*it)[3]), std::stoi((*it)[4])});
                for (auto it = std::sregex_iterator(log[i - 1].begin(), log[i - 1].end(), rf);
                     it != std::sregex_iterator(); ++it)
                    u.push_back({std::stoi((*it)[1]), std::stoi((*it)[2]), std::stoi((*it)[3]), std::stoi((*it)[4])});
                if (v.size() < 2 || u.size() < 2) continue;
                auto d = [&](std::vector<std::array<int, 4>>& q) {
                    return std::max(std::abs(q[0][0] - q[1][0]), std::abs(q[0][1] - q[1][1]));
                };
                bool low = u[0][2] < 0.5 * u[0][3];
                if (low)
                    continue; /* v10: урон по времени/яд снимается в начале хода, уже после кадра — при ХП кадра <50%
                                 побег мог быть законным */
                (d(v) < d(u)) ? toward++ : away++;
            }
        }
        // v21: туман войны — прыжок «к врагу» идёт к месту, где его видели в последний раз (враг мог уйти, стать невидимым),
        // и к цели-приспешнику, а расстояние меряется до босса; поэтому допуск шире, чем в v20 (там было away <= toward / 10)
        T("T17", "§8 телепорт при ХП>=30% (кадр с ХП>=50%, тик урона по времени не успеет уронить ниже) ведёт К врагу (или к месту, где его видели)",
          away <= toward / 5, fmt("к врагу %d, от врага %d", toward, away));
    }
    // T18 карта связная
    if (TB()) {
        World w;
        int bad = 0, N = 1500;
        int small30 = 0;
        for (int i = 0; i < N; i++) {
            genMap(w);
            // v21 (мир из шума): участок берётся как есть, бойцы ставятся в самую большую связную часть (пешком по правилам движка);
            // проверка: в ней не меньше 2 мест для расстановки, и пересчёт связности даёт ту же часть
            const int starts = startCellsN(w);
            bool mainc0[W][W];
            std::memcpy(mainc0, w.mainc, sizeof mainc0);
            const int mainN = markMain(w);
            if (starts < 2 || std::memcmp(mainc0, w.mainc, sizeof mainc0) != 0) bad++;
            small30 += mainN < 0.3 * w.aw * w.ah;
        }
        T("T18", "§9 карта: бойцы ставятся в самую большую связную часть арены (не меньше 2 мест), обычно она — почти вся арена", bad == 0 && small30 * 20 <= N,
          fmt("плохих карт %d из %d; основная часть меньше 30%% арены — %d", bad, N, small30));
    }
    // T19 воспроизводимость (обязательная): свой xorshift даёт одинаковые числа на любом компиляторе; потоки не мешают
    // друг другу
    if (TB()) {
        rng.seed(42);
        unsigned long long g0 = rng.next(), g1 = rng.next(), g2 = rng.next();
        rng.seed(42);
        unsigned long long h = 1469598103934665603ULL;
        for (int i = 0; i < 1000; i++) {
            h = (h ^ (unsigned)rollD20()) * 1099511628211ULL;
            h = (h ^ (unsigned)rollDie(7)) * 1099511628211ULL;
            h = (h ^ (unsigned)irand(1000)) * 1099511628211ULL;
        }
        rng.seed(7);
        double su = 0;
        for (int i = 0; i < 1000; i++) su += rand01();
        bool gold = g0 == 3580622183945639842ULL && g1 == 10378725325292465923ULL && g2 == 8967075514996744559ULL &&
                    h == 13206841453922315987ULL && std::fabs(su - 506.502927455531) < 1e-9;
        auto run = [&]() {
            rng.seed(777);
            g_rounds = 0;
            double t = 0;
            for (int i = 0; i < 300; i++) {
                Boss a = makeBoss(800, C, true), b = makeBoss(800, C, true);
                t += fight(a, b) * (i + 1) + g_rounds;
            }
            return t;
        };
        double r1 = run(), r2 = run();
        double rt[4] = {0, 0, 0, 0};
        {
            std::vector<std::thread> th;
            for (int k = 0; k < 4; k++)
                th.emplace_back([&, k] {
                    rng.seed(1);
                    (void)run();
                    rt[k] = run();
                });
            for (auto& x : th) x.join();
        }
        bool thr = rt[0] == r1 && rt[1] == r1 && rt[2] == r1 && rt[3] == r1;
        T("T19",
          "(обязательная) зерно -> тот же результат: в процессе, между компиляторами (xorshift, контрольные значения) "
          "и между потоками",
          r1 == r2 && gold && thr,
          fmt("в процессе: %s; контрольные значения xorshift: %s; 4 потока дают тот же результат: %s",
              r1 == r2 ? "да" : "НЕТ", gold ? "совпали" : "НЕ СОВПАЛИ", thr ? "да" : "НЕТ"));
    }
    // T20 симметрия мест
    if (TB()) {
        double wa = 0;
        int N = 40000;
        std::vector<double> sc(N);
        parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно, сумма — по порядку
            rng.seed(31337ULL * 1000003ULL + (uint64_t)i);
            Boss a, b;
            makePair(1600, C, a, b);
            const int r = fight(a, b);
            sc[i] = r == 0 ? 1 : (r == 2 ? .5 : 0);
        });
        for (double x : sc) wa += x;
        double p = 100 * wa / N;
        T("T20", "§11 симметрия: у случайной пары одинаковых распределений первое место выигрывает 50%",
          std::fabs(p - 50) < 0.75, fmt("A выигрывает %.2f%% (допуск ±0.75, 2σ≈0.5)", p));
    }
    // T21 граничные бюджеты: минимум MIN_BUDGET (20)
    if (TB()) {
        std::string d;
        bool ok = true;
        for (long long B : {0LL, 1LL, 19LL}) {
            int over = 0, N = 300;
            for (int i = 0; i < N; i++) {
                Boss b = makeBoss(B, C, true);
                if (b.cost - STAT_BASE > MIN_BUDGET) over++;
            }
            d += fmt("B=%lld: выше минимума %d/%d; ", B, over, N);
            if (over) ok = false;
        }
        for (long long B : {20LL, 50LL, 100LL}) {
            int over = 0, N = 300;
            for (int i = 0; i < N; i++) {
                Boss b = makeBoss(B, C, true);
                if (b.cost - STAT_BASE > B) over++;
            }
            d += fmt("B=%lld: перерасход %d/%d; ", B, over, N);
            if (over) ok = false;
        }
        T("T21", "§1 минимальный бюджет 20 (шесть характеристик по 1, ХП 1, способности); ниже поднимается до 20", ok, d);
    }
    // T22 лимит раундов
    if (TB()) {
        g_rounds = 0;
        g_capped = 0;
        int N = 6000;
        std::vector<char> cap(N, 0);
        parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно
            rng.seed(9ULL * 1000003ULL + (uint64_t)i);
            const long long c0 = g_capped;
            long long B = 200LL << irand(6);
            Boss a = makeBoss(B, C, true), b = makeBoss(B, C, true);
            fight(a, b);
            cap[i] = g_capped > c0;
        });
        g_capped = 0;
        for (char c : cap) g_capped += c;
        T("T22", "§8 лимита раундов нет: доля боёв, дошедших до технического предохранителя (ничья), мала",
          100.0 * g_capped / N < 2.0,
          fmt("%.1f%% боёв доходят до предохранителя в %d раундов (бой тогда считается ничьёй)", 100.0 * g_capped / N,
              g_maxRounds));
    }

    // T23..T29 дополнительные требования
    if (TB()) {
        int cha = 0, mainCha = 0, badm = 0, N = 4000;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(200LL << irand(6), C, true);
            if (b.stats[CHA] > 0) cha++;
            if (b.mainStat == CHA) mainCha++;
            if (b.mainStat == CON) badm++;
        }
        T("T23", "§2 ХАР покупается как остальные; главная характеристика: СИЛ/ЛОВ/ИНТ/МДР/ХАР, но не ТЕЛ",
          cha > N * 0.6 && mainCha > N * 0.1 && badm == 0,
          fmt("боссов с ХАР>0: %d, главная ХАР: %d, главная ТЕЛ: %d из %d", cha, mainCha, badm, N));
    }
    if (TB()) {
        bool ok = true;
        for (int m : {STR, DEX, INT, WIS}) {
            Boss b = plain(10, 1, 6, 5);
            b.stats = {10, 10, 10, 10, 10, 10};
            b.stats[m] = 18;
            b.mainStat = m;
            Fighter f = mk(b, 0, 1, 1);
            if (f.dc() != 8 + PROF + 4 || f.mainMod() != 4) ok = false;
        }
        T("T24", "§2 СЛ = 8 + 2 + мод. главной характеристики", ok, "");
    }
    if (TB()) {
        Boss b = plain(10, 1, 6, 5);
        b.stats = {16, 10, 10, 10, 10, 10};
        Fighter f = mk(b, 0, 1, 1);
        int melee = f.wMod();
        b.stats = {8, 18, 10, 10, 10, 10};
        b.wFinesse = false;
        Fighter g = mk(b, 0, 1, 1);
        int noFin = g.wMod();
        b.wFinesse = true;
        Fighter h = mk(b, 0, 1, 1);
        int fin = h.wMod();
        b.wFinesse = false;
        b.wRange = 30;
        b.wAmmo = true;
        Fighter r = mk(b, 0, 1, 1);
        int rng_ = r.wMod();
        b.wAmmo = false;
        Fighter th = mk(b, 0, 1, 1);
        int thr = th.wMod();
        b.wFinesse = true;
        Fighter thf = mk(b, 0, 1, 1);
        int thrf = thf.wMod();
        T("T25",
          "§5 мод. оружия: СИЛ; фехтовальное — большее из СИЛ/ЛОВ; боеприпасное (дальнее) — только ЛОВ; метательное — "
          "СИЛ (или фехтовальное)",
          melee == 3 && noFin == -1 && fin == 4 && rng_ == 4 && thr == -1 && thrf == 4,
          fmt("СИЛ16: %d; СИЛ8/ЛОВ18 без фехтовального: %d, фехтовальное: %d, боеприпасное: %d, метательное: %d, "
              "метательное фехтовальное: %d",
              melee, noFin, fin, rng_, thr, thrf));
    }
    if (TB()) {
        Spec a;
        a.self = true;
        a.healN = 2;
        a.delivery = 2;
        Spec b2 = a;
        b2.range = 120;
        b2.radius = 30;
        g_B = 800;
        bool ok = specPrice(a, C) == specPrice(b2, C);
        T("T26", "§6 дальность/область не удорожают способность «на себя»", ok,
          fmt("цена %lld и %lld", specPrice(a, C), specPrice(b2, C)));
    }
    if (TB()) {
        Fighter f = mk(plain(10, 0, 0, 5), 0, 1, 1);
        int base = speedOf(f);
        f.cond[C_RESTR] = 1;
        int r = speedOf(f);
        f.cond[C_RESTR] = 0;
        f.cond[C_SLOW] = 1;
        int sl = speedOf(f);
        T("T27", "§7 опутан — скорость 0; замедлен — вдвое", base == 30 && r == 0 && sl == 15,
          fmt("обычная %d, опутан %d, замедлен %d", base, r, sl));
    }
    if (TB()) {
        World w;
        double worstClimb = 0, worstDiff = 0;
        int bad = 0, N = 3000, mnS = 999, mxS = 0, small = 0, big = 0, streams = 0, pits = 0, hills = 0;
        for (int i = 0; i < N; i++) {
            genMap(w);
            mnS = std::min({mnS, w.aw, w.ah});
            mxS = std::max({mxS, w.aw, w.ah});
            small += (w.aw <= 10 && w.ah <= 10);
            big += (w.aw >= 60 || w.ah >= 60);
            bool st = false, pi = false, hi = false;
            int nClimb = 0, nDiff = 0;
            for (int y = 0; y < W; y++)
                for (int x = 0; x < W; x++) {
                    if (x < w.aw && y < w.ah) { nClimb += w.climb[y][x]; nDiff += w.diff[y][x]; }
                    const bool outside = x >= w.aw || y >= w.ah;
                    if (outside && !w.wall[y][x]) bad++;       // за пределами арены — стена
                    if (!outside && w.water[y][x]) st = true;
                    if (!outside && w.pit[y][x]) pi = true;
                    if (!outside && w.height[y][x]) hi = true;
                }
            streams += st; pits += pi; hills += hi;
            if (std::min(w.aw, w.ah) >= 20) {   // на маленьких аренах один предмет занимает заметную долю
                worstClimb = std::max(worstClimb, (double)nClimb / (w.aw * w.ah));
                worstDiff = std::max(worstDiff, (double)nDiff / (w.aw * w.ah));
            }
            if (w.aw < 2 || w.ah < 2 || w.aw > 100 || w.ah > 100) bad++;
        }
        T("T28", "§11 арена — участок мира, стороны от 2 до 100, за краем стена; встречаются ручьи, ущелья и возвышенности", bad == 0 && worstClimb < 0.15 && mnS == 2 && mxS == 100 && small > 0 && big > 0 && streams > 0 && pits > 0 && hills > 0,
          fmt("сторона %d–%d; малых %d, больших %d; ручьёв %d, ущелий %d, возвышенностей %d из %d; на аренах от 20 клеток наибольшая доля лазалок %.2f, труднопроходимой %.2f", mnS, mxS, small, big, streams, pits, hills, N, worstClimb, worstDiff));
    }
    if (TB()) {
        Fighter f = mk(plain(10, 0, 0, 5), 0, 1, 1);
        f.b.stats = {10, 10, 10, 10, 10, 10};
        int a = saveBonus(f, WIS);
        f.b.saveProf = 1u << WIS;
        int b2 = saveBonus(f, WIS);
        int c2 = saveBonus(f, STR);
        T("T29", "§13.1 владение спасброском: +2 только к своей характеристике", a == 0 && b2 == 2 && c2 == 0,
          fmt("без владения %d, с владением МДР %d, СИЛ %d", a, b2, c2));
    }

    // T31..T36 типы урона (v7)
    if (TB()) {
        Boss t = plain(100, 0, 0, 5);
        bool ok = true;
        std::string d;
        auto chk = [&](const char* n, double got, double exp) {
            if (std::fabs(got - exp) > 1e-9) {
                ok = false;
                d += fmt("%s:%.1f≠%.1f ", n, got, exp);
            }
        };
        t.resMask = 1u << DT_FIRE;
        chk("резист", dmgMult(t, DT_FIRE, true), 0.5);
        t.resMask = 0;
        t.immMask = 1u << DT_POISON;
        chk("иммун", dmgMult(t, DT_POISON, true), 0.0);
        t.immMask = 0;
        t.vulnMask = 1u << DT_COLD;
        chk("уязв", dmgMult(t, DT_COLD, true), 2.0);
        t.resMask = 1u << DT_COLD;
        chk("резист+уязв", dmgMult(t, DT_COLD, true), 1.0);
        t.resMask = 0;
        t.vulnMask = 0;
        t.resNM = true;
        chk("немагич. колющий", dmgMult(t, DT_PIERCING, false), 0.5);
        chk("магич. колющий", dmgMult(t, DT_PIERCING, true), 1.0);
        chk("огонь не физический", dmgMult(t, DT_FIRE, false), 1.0);
        t.resNM = false;
        t.immNM = true;
        chk("иммун немагич.", dmgMult(t, DT_SLASHING, false), 0.0);
        chk("иммун, магич.", dmgMult(t, DT_SLASHING, true), 1.0);
        T("T31", "§6.* множители: резист ×0.5, иммунитет 0, уязвимость ×2, вместе ×1, «от немагического»", ok, d);
    }
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.resMask = 1u << DT_FIRE;
        f.b.vulnMask = 1u << DT_RADIANT;
        f.b.immMask = 1u << DT_POISON;
        hurt(f, 10, DT_FIRE, true);
        int a = f.hp;
        hurt(f, 10, DT_RADIANT, true);
        int b2 = f.hp;
        hurt(f, 10, DT_POISON, true);
        int c2 = f.hp;
        hurt(f, 10, DT_ACID, true);
        int d2 = f.hp;
        T("T32",
          "hurt применяет защиты: огонь с резистом 10→5, излучение с уязвимостью 10→20, яд с иммунитетом 0, кислота 10",
          a == 95 && b2 == 75 && c2 == 75 && d2 == 65, fmt("ХП после ударов: %d, %d, %d, %d", a, b2, c2, d2));
    }
    if (TB()) {
        Coef C2 = CC();
        int bad = 0, N = 800;
        for (int i = 0; i < N; i++) {
            g_B = 800;
            Boss b = makeBoss(800, C2, false);
            long long p0 = priceOf(b, C2, 800);
            Boss c = b;
            int t = -1;
            for (int d = 0; d < DT_N; d++)
                if (!(((b.resMask | b.immMask | b.vulnMask) >> d) & 1)) {
                    t = d;
                    break;
                }
            if (t < 0) continue;
            c.vulnMask |= 1u << t;
            long long p1 = priceOf(c, C2, 800);
            if (!(p1 < p0)) bad++;
        }
        T("T33", "уязвимость ВОЗВРАЩАЕТ очки: цена босса с дополнительной уязвимостью ниже", bad == 0,
          fmt("нарушений %d из %d", bad, N));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(50, 2, 6, 5);
        a.wType = DT_PIERCING;
        Boss d = plain(50, 0, 0, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        double base = evWeapon(w, w.f[0], w.f[1]);
        w.f[1].b.resMask = 1u << DT_PIERCING;
        double res = evWeapon(w, w.f[0], w.f[1]);
        w.f[1].b.resMask = 0;
        w.f[1].b.immMask = 1u << DT_PIERCING;
        double imm = evWeapon(w, w.f[0], w.f[1]);
        w.f[1].b.immMask = 0;
        w.f[1].b.vulnMask = 1u << DT_PIERCING;
        double vul = evWeapon(w, w.f[0], w.f[1]);
        T("T34",
          "ИИ учитывает защиты цели при выборе действия (ожидаемый урон: резист ×0.5, иммунитет 0, уязвимость ×2)",
          std::fabs(res - base * 0.5) < 1e-9 && imm == 0 && std::fabs(vul - base * 2) < 1e-9,
          fmt("база %.2f, резист %.2f, иммунитет %.2f, уязвимость %.2f", base, res, imm, vul));
    }
    if (TB()) {
        int cnt[DT_N] = {};
        int N = 0;
        for (int i = 0; i < 6000; i++) {
            Boss b = makeBoss(800, C, false);
            if (b.wN > 0) {
                cnt[b.wType]++;
                N++;
            }
        } /* у безоружного тип оружия не определён */
        double p = cnt[DT_PIERCING] / (double)N, sl = cnt[DT_SLASHING] / (double)N,
               bl = cnt[DT_BLUDGEONING] / (double)N;
        int phys = 0, all = 0;
        for (int i = 0; i < 20000; i++) {
            Spec s = randSpec(0);
            if (s.dmgN) {
                all++;
                if (isPhys(s.dtype)) phys++;
            }
        }
        T("T35", "распределение типов: оружие колющий≈38%, рубящий≈35%, дробящий≈27%; способности физические ≈22%",
          std::fabs(p - 0.385) < 0.03 && std::fabs(sl - 0.346) < 0.03 && std::fabs(bl - 0.269) < 0.03 &&
              std::fabs(phys / (double)all - 0.22) < 0.02,
          fmt("колющий %.1f%%, рубящий %.1f%%, дробящий %.1f%%, способности физические %.1f%%", 100 * p, 100 * sl,
              100 * bl, 100.0 * phys / all));
    }
    if (TB()) {
        int a = 0, b2 = 0, c2 = 0, N = 6000;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C, false);
            if (b.resMask) a++;
            if (b.immMask) b2++;
            if (b.vulnMask) c2++;
        }
        T("T36", "(инфо) доли боссов с защитами: резист / иммунитет / уязвимость", true,
          fmt("%.0f%% / %.0f%% / %.0f%%", 100.0 * a / N, 100.0 * b2 / N, 100.0 * c2 / N));
    }

    // T37..T40 легендарное сопротивление
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.stats = {1, 1, 1, 1, 1, 1};
        f.lrLeft = 2;
        long long u0 = g_lrUsed;
        bool a = saveLR(f, STR, 100, false);
        int l1 = f.lrLeft;
        bool b1 = saveLR(f, STR, 100, true);
        bool c1 = saveLR(f, STR, 100, true);
        bool d1 = saveLR(f, STR, 100, true);
        T("T37", "§LR проваленный спасбросок -> успех, пока есть заряды (и только если эффект значим)",
          !a && l1 == 2 && b1 && c1 && !d1 && f.lrLeft == 0 && g_lrUsed - u0 == 2,
          fmt("незначимый: %d (зарядов %d); значимые подряд: %d %d %d; осталось %d; потрачено %lld", a, l1, b1, c1, d1,
              f.lrLeft, g_lrUsed - u0));
    }
    if (TB()) {
        Fighter t = mk(plain(100, 0, 0, 5), 0, 1, 1);
        Spec st;
        st.cond = C_STUN;
        st.condRounds = 1;
        Spec lt;
        lt.cond = C_SLOW;
        lt.condRounds = 1;
        Spec big;
        big.dmgN = 10;
        big.dmgS = 6;
        big.dtype = DT_FIRE;
        Spec sm;
        sm.dmgN = 1;
        sm.dmgS = 4;
        sm.dtype = DT_FIRE;
        Spec lg;
        lg.cond = C_RESTR;
        lg.condRounds = 3;
        T("T38", "§LR бот тратит заряд на оглушение/отвлечение, долгий контроль и удар ≥25% ХП, но не на мелочь",
          lrWorth(t, st) && !lrWorth(t, lt) && lrWorth(t, big) && !lrWorth(t, sm) && lrWorth(t, lg),
          fmt("оглушение %d, замедление 1р %d, 10к6 %d, 1к4 %d, опутан 3р %d", lrWorth(t, st), lrWorth(t, lt),
              lrWorth(t, big), lrWorth(t, sm), lrWorth(t, lg)));
    }
