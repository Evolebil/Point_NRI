// Проверки движка, часть 11 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // T192: «событие → эффект» (v21) — хиты до 0 → остаться с 1 (раз за бой), убийство → атака бонусом (если бонусное
    // свободно), крит → урон цели, после Уклонения → лечение, в начале хода → временные хиты, +инициатива, преимущество на
    // следующую атаку — на одну атаку
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss me = plain(100, 1, 6, 5);
        me.trigs = {Trig{T_DOWN, X_STAY1, 1, 6, 0, 0, 1}, Trig{T_KILL, X_BONUSATK}, Trig{T_DODGE, X_HEAL, 0, 6, 10}, Trig{T_TURN, X_TEMP, 0, 6, 7}, Trig{T_CRIT, X_ADVNEXT}};
        w.f.push_back(mk(me, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 11, 11));   // главный враг (жив — бой идёт)
        w.f.push_back(mk(plain(1, 1, 6, 5), 1, 11, 10));      // приспешник, которого убьём
        hurt(w.f[0], 500, DT_FIRE, true);
        const bool stay = w.f[0].hp == 1;
        hurt(w.f[0], 500, DT_FIRE, true);
        const bool once = w.f[0].hp <= 0;
        w.f[0].hp = 100;
        // убийство → атака бонусом по следующему врагу
        TurnState ts;
        ts.speed = 30;
        w.f[1].b.ac = -100;
        const int h1 = w.f[1].hp;
        w.f[2].b.ac = -100;
        applyPrim(w, 0, ts, Prim{PR_ATTACK, 2, 0}, true, nullptr);
        const bool bonus = w.f[2].hp <= 0 && ts.bonusUsed && w.f[1].hp < h1;
        // Уклонение → лечение 10
        w.f[0].hp = 50;
        TurnState t2;
        applyPrim(w, 0, t2, Prim{PR_DODGE}, true, nullptr);
        const bool dodge = w.f[0].hp == 60;
        // начало хода → 7 временных хитов
        TurnState t3;
        w.f[0].temp = 0;
        beginTurn(w, 0, t3);
        const bool turn = w.f[0].temp == 7;
        // преимущество на следующую атаку — одна атака
        w.f[0].advNext = true;
        bool a1, d1, a2, d2, crit;
        advState(w, w.f[0], w.f[1], 5, a1, d1);
        attackRoll(w, w.f[0], w.f[1], 0, 5, crit);
        advState(w, w.f[0], w.f[1], 5, a2, d2);
        const bool adv = a1 && !a2;
        Coef C2 = CC();
        const bool price = trigPrice(Trig{T_DOWN, X_STAY1, 1, 6, 0, 0, 1}, me, C2) > 0 && !trigAllowed(T_KILL, X_DMG) && !trigAllowed(T_TURN, X_STAY1) && trigAllowed(T_INIT, X_INITB);
        T("T192", "§v21 «событие → эффект»: 0 хитов → 1 (раз за бой), убийство → атака бонусом, Уклонение → лечение, начало хода → временные хиты, преимущество на одну атаку",
          stay && once && bonus && dodge && turn && adv && price,
          fmt("остаться с 1 %d (второй раз нет %d), атака бонусом %d, лечение после Уклонения %d, временные хиты в начале хода %d, преимущество на одну атаку %d, цены и сочетания %d",
              stay, once, bonus, dodge, turn, adv, price));
    }
    // T193: броски и ресурсы (v21) — Благословение (+к4 к атаке) поднимает попадания; переброс промаха (Везунчик) — тоже,
    // и тратит применения; «Второй шанс» режет попадания по мне; снижение урона −3 у немагического физического; игнор
    // сопротивления; размен −5/+10 включается, когда выгоден
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        auto hitRate = [&](const Boss& a, const Boss& d, int N) {
            int h = 0;
            for (int i = 0; i < N; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                bool crit;
                h += attackRoll(w, w.f[0], w.f[1], 0, 5, crit);
            }
            return (double)h / N;
        };
        Boss a = plain(100, 1, 6, 5), d = plain(100, 1, 6, 5);
        d.ac = 15;
        const double base = hitRate(a, d, 4000);
        Boss bl = a;
        bl.rules = {Rule{W_ALWAYS, 0, E_ATKDIE, 1, 4}};
        const double bless = hitRate(bl, d, 4000);
        Boss lk = a;
        lk.trigs = {Trig{T_MISS, X_REROLL, 1, 6, 0, 0, UNL}};
        const double lucky = hitRate(lk, d, 4000);
        Boss sc = d;
        sc.trigs = {Trig{T_HITBY, X_REROLL, 1, 6, 0, 0, UNL}};
        const double second = hitRate(a, sc, 4000);
        const bool rolls = bless > base + 0.07 && lucky > base + 0.15 && second < base - 0.10;
        // снижение урона и игнор сопротивления
        Boss ham = plain(1000, 0, 0, 5);
        ham.rules = {Rule{W_ALWAYS, 0, E_DMGRED, 3}};
        Fighter f = mk(ham, 0, 1, 1);
        const int r1 = hurt(f, 10, DT_SLASHING, false), r2 = hurt(f, 10, DT_SLASHING, true);
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        Boss src = plain(100, 0, 0, 5);
        src.rules = {Rule{W_ALWAYS, 0, E_PIERCE, DT_FIRE}};
        Boss fr = plain(1000, 0, 0, 5);
        fr.resMask = 1u << DT_FIRE;
        v.f.push_back(mk(src, 0, 1, 1));
        v.f.push_back(mk(fr, 1, 2, 1));
        g_curW = &v;
        g_srcIdx = 0;
        const int p1 = hurt(v.f[1], 20, DT_FIRE, true);
        g_srcIdx = -1;
        const int p2 = hurt(v.f[1], 20, DT_FIRE, true);
        g_curW = nullptr;
        const bool dmg = r1 == 7 && r2 == 10 && p1 == 20 && p2 == 10;
        Coef C2 = CC();
        const bool price = rulePrice(Rule{W_ALWAYS, 0, E_ATKDIE, 1, 4}, a, C2) > 0 && trigPrice(Trig{T_MISS, X_REROLL, 1, 6, 0, 0, 3}, a, C2) > 0 && !trigAllowed(T_MISS, X_HEAL);
        T("T193", "§v21 броски и ресурсы: Благословение, Везунчик и Второй шанс меняют попадания; снижение урона; игнор сопротивления",
          rolls && dmg && price,
          fmt("попадания: обычно %.2f, с Благословением %.2f, с перебросом %.2f, против Второго шанса %.2f; урон −3: %d и магический %d; сквозь резист %d, без источника %d; цены %d",
              base, bless, lucky, second, r1, r2, p1, p2, price));
    }
    // T194: размеры (v21) — схватить и толкнуть можно не больше чем на ступень крупнее; маленьким тяжёлое оружие —
    // с помехой; крупный стоит очков, маленький с тяжёлым оружием — возвращает
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss med = plain(100, 1, 6, 5), big = plain(100, 1, 6, 5), huge = plain(100, 1, 6, 5);
        big.size = 3;
        huge.size = 4;
        w.f.push_back(mk(med, 0, 10, 10));
        w.f.push_back(mk(big, 1, 11, 10));
        w.f.push_back(mk(huge, 1, 10, 11));
        g_curW = &w;
        const bool gBig = applyCond(w.f[1], C_GRAPPLE, 1, -1, 10, false, 0), gHuge = applyCond(w.f[2], C_GRAPPLE, 1, -1, 10, false, 0);
        const int hx = w.f[2].x, hy = w.f[2].y;
        pushAway(w, w.f[0], w.f[2], 10);
        const bool noPush = w.f[2].x == hx && w.f[2].y == hy;
        g_curW = nullptr;
        // маленький с тяжёлым оружием бьёт реже
        auto rate = [&](int size) {
            int h = 0;
            for (int i = 0; i < 3000; i++) {
                World v;
                std::memset(v.wall, 0, sizeof v.wall);
                std::memset(v.climb, 0, sizeof v.climb);
                Boss a = plain(100, 1, 6, 5);
                a.size = size;
                a.wHeavy = true;
                Boss d = plain(100000, 0, 0, 5);
                d.ac = 15;
                v.f.push_back(mk(a, 0, 10, 10));
                v.f.push_back(mk(d, 1, 11, 10));
                const int h0 = v.f[1].hp;
                doWeapon(v, v.f[0], 1, 1);
                h += v.f[1].hp < h0;
            }
            return h / 3000.0;
        };
        const double small = rate(1), mid = rate(2);
        Coef C2 = CC();
        Boss sm = plain(100, 1, 6, 5);
        sm.size = 1;
        sm.wHeavy = true;
        const bool price = sizePrice(big, C2) > 0 && sizePrice(sm, C2) < 0 && sizePrice(med, C2) == 0;
        T("T194", "§v21 размеры: схватить и толкнуть — не больше чем на ступень крупнее; маленьким тяжёлое оружие — с помехой; цена размера",
          gBig && !gHuge && noPush && small < mid - 0.1 && price,
          fmt("схватил большого %d, огромного %d; огромный не сдвинут %d; попадания тяжёлым: маленький %.2f, средний %.2f; цены %d", gBig, gHuge, noPush, small, mid, price));
    }
    // T195: прочее (v21) — притягивание к себе, снижение максимума хитов, вампиризм, рассеивание снимает баффы и
    // концентрацию, контрзаклинание гасит способность в зоне, истинное зрение видит невидимого, Подвижный не провоцирует
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.aw = w.ah = W;
        Boss me = plain(100, 1, 6, 5), en = plain(1000, 1, 6, 5);
        Spec sp;
        sp.delivery = 2;
        sp.range = 60;
        sp.dmgN = 10;
        sp.dmgS = 6;
        sp.pullFt = 30;
        sp.maxHpDmg = sp.drain = sp.dispel = true;
        me.abil = {sp};
        w.f.push_back(mk(me, 0, 10, 10));
        w.f.push_back(mk(en, 1, 20, 10));
        w.f[0].hp = 50;
        w.f[1].buffRounds = 3;
        w.f[1].invisRounds = 3;
        hitOne(w, sp, w.f[0], w.f[1]);
        const bool misc = w.f[1].x < 20 && w.f[1].maxHp < 1000 && w.f[0].hp > 50 && w.f[1].buffRounds == 0 && w.f[1].invisRounds == 0;
        // контрзаклинание
        Boss cs = plain(100, 0, 0, 5);
        Spec c;
        c.self = c.counter = true;
        c.react = 4;
        c.range = 60;
        cs.abil = {c};
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        Boss caster = plain(100, 0, 0, 5);
        Spec fire;
        fire.delivery = 2;
        fire.range = 60;
        fire.dmgN = 10;
        caster.abil = {fire};
        v.f.push_back(mk(caster, 0, 10, 10));
        v.f.push_back(mk(cs, 1, 15, 10));
        const int h0 = v.f[1].hp;
        doSpec(v, v.f[0], 0, 1);
        const bool counter = v.f[1].hp == h0 && v.f[1].reactionUsed;
        // истинное зрение
        Boss ts = plain(100, 0, 0, 5);
        ts.truesight = 30;
        Fighter a = mk(ts, 0, 10, 10), b2 = mk(plain(100, 0, 0, 5), 1, 13, 10), c2 = mk(plain(100, 0, 0, 5), 0, 10, 10);
        b2.invisRounds = 3;
        const bool sight = canSee(v, a, b2) && !canSee(v, c2, b2);
        Coef C2 = CC();
        Spec plain1;
        plain1.delivery = 2;
        plain1.range = 60;
        plain1.dmgN = 10;
        const bool price = specPrice(sp, C2) > specPrice(plain1, C2) && specPrice(c, C2) > 0;
        T("T195", "§v21 прочее: притягивание, снижение максимума хитов, вампиризм, рассеивание, контрзаклинание, истинное зрение",
          misc && counter && sight && price,
          fmt("притягивание/максимум/вампиризм/рассеивание %d (x=%d, максимум %d, мои хиты %d), контрзаклинание %d, истинное зрение %d, цены %d",
              misc, w.f[1].x, w.f[1].maxHp, w.f[0].hp, counter, sight, price));
    }
    // T196: местность (v21, шаг 1) — дерево: залезть стоит подъём на 20 фт (15 фт движения), с вершины до соседа на земле
    // 20 фт (ближним не достать), крона — полукрытие; дерево закрывает обзор, но с вершины другого дерева видно поверх;
    // объекты с хитами: огненный шар валит дерево (остаются обломки — труднопроходимо), яд объектам не вредит, стена — 54 хита
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.tree[10][11] = true;   // дерево рядом с (10,10)
        w.tree[10][14] = true;   // дерево между (12,10) и (16,10)
        w.tree[10][18] = true;
        w.wall[20][5] = true;
        initObjects(w);
        Fighter g = mk(plain(100, 0, 0, 5), 0, 10, 10);
        const int climbCost = stepCost(w, g, 10, 10, 11, 10), down = stepCost(w, g, 11, 10, 10, 10);
        const int dUp = cellDist(&w, 11, 10, 10, 10), dFlat = cellDist(&w, 10, 10, 12, 10);
        const bool climb = climbCost == 15 && down == 5 && dUp == 20 && dFlat == 10 && coverBonus(w, 5, 10, 11, 10) == 2;
        const bool sight = !los(w, 12, 10, 16, 10) && los(w, 11, 10, 18, 10) && los(w, 12, 10, 13, 10);
        const bool hp = w.ohp[10][11] == OBJ_TREE_HP && w.ohp[20][5] == OBJ_WALL_HP && objAC(w, 11, 10) == 15 && objAC(w, 5, 20) == 17;
        const bool poison = !hitObject(w, 11, 10, 100, DT_POISON) && w.tree[10][11];
        Spec fb;
        fb.delivery = 1;
        fb.range = 60;
        fb.radius = 10;
        fb.dmgN = 20;
        fb.dmgS = 10;
        fb.dtype = DT_FIRE;
        areaHitsObjects(w, fb, g, 14, 10);
        bool rubble = false;
        for (auto& d : w.dyn) rubble = rubble || (d[0] == 14 && d[1] == 10 && d[2] == 4);
        const bool burn = !w.tree[10][14] && w.diff[10][14] && rubble && w.tree[10][11];
        const bool wall = !hitObject(w, 5, 20, 53, DT_BLUDGEONING) && hitObject(w, 5, 20, 1, DT_BLUDGEONING) && !w.wall[20][5];
        T("T196", "§v21 местность: дерево (залезть, высота, укрытие, обзор), объекты с хитами (огонь валит дерево, яд мимо, стена 54 хита)",
          climb && sight && hp && poison && burn && wall,
          fmt("дерево: залезть %d фт, спуститься %d, с вершины до соседа %d фт, по земле %d, укрытие %d; обзор %d; хиты %d; яд %d; огонь %d; стена %d",
              climbCost, down, dUp, dFlat, coverBonus(w, 5, 10, 11, 10), sight, hp, poison, burn, wall));
    }
    // T197: опасные зоны (v21, шаг 2) — лава бьёт при входе не чаще раза за ход (второй шаг по лаве в тот же ход — без
    // урона), в начале нового хода снова; летун над лавой не горит, но ядовитый дым его травит; путь при равной длине
    // обходит зону (длина пути та же — кандидаты сети верны)
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.hazN = 4;
        w.hazDC = 30;   // спасбросок не пройти: урон полный
        for (int x = 11; x <= 12; x++) w.haz[10][x] = 1;   // лава — в коридоре между стенами, обойти нельзя
        for (int x = 9; x <= 14; x++) w.wall[9][x] = w.wall[11][x] = true;
        w.haz[15][10] = 3;                                  // ядовитый дым
        w.f.push_back(mk(plain(100, 0, 0, 5), 0, 10, 10));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 25, 25));
        Fighter& g = w.f[0];
        g.hp = g.maxHp = 1000;
        g_turnSerial++;
        moveToCell(w, g, 13, 10, 15);   // через лаву по прямой: 2 клетки лавы, урон один раз
        const int hit1 = 1000 - g.hp;
        g_turnSerial++;
        g.x = 11;
        const int h0 = g.hp;
        hazardHit(w, g);                 // новый ход в лаве — урон снова
        const int hit2 = h0 - g.hp;
        Boss fl = plain(100, 0, 0, 5);
        fl.fly = true;
        w.f.push_back(mk(fl, 0, 11, 10));
        Fighter& b = w.f.back();
        b.hp = b.maxHp = 1000;
        g_turnSerial++;
        hazardHit(w, b);
        const int flyLava = 1000 - b.hp;
        b.x = 10;
        b.y = 15;
        g_turnSerial++;
        hazardHit(w, b);
        const int flySmoke = 1000 - b.hp;
        // путь (10,13) → (14,13) по ровному: прямо 4 клетки; ставим лаву на (12,13) — путь той же длины обходит её
        w.haz[13][12] = 1;
        Fighter p = mk(plain(100, 0, 0, 5), 0, 10, 13);
        std::vector<PStep> path;
        pathTo(w, p, 14, 13, path);
        bool avoid = path.size() == 4;
        for (auto& c : path) avoid = avoid && w.haz[c.second][c.first] == 0;
        const bool once = hit1 >= 4 && hit1 <= 40, again = hit2 >= 4 && hit2 <= 40;
        T("T197", "§v21 опасные зоны: урон при входе раз за ход и в начале хода, летун над лавой цел (дым травит), путь обходит зону при равной длине",
          once && again && flyLava == 0 && flySmoke > 0 && avoid,
          fmt("через 2 клетки лавы −%d (4к10, раз за ход), новый ход в лаве −%d, летун над лавой −%d, в дыму −%d, путь %zu шагов в обход %d",
              hit1, hit2, flyLava, flySmoke, path.size(), avoid));
    }
    // T198: высоты и второй ярус (v21, шаг 3) — обрыв (перепад 2 уровня) пешком не пройти, по лестнице можно, летун —
    // только под открытым небом; под настилом на 20 фт средний проходит, огромный — нет; путь на настил по лестнице;
    // стоящий на настиле и стоящий под ним — 20 фт и не видят друг друга сквозь настил; настил рушится — стоявший на нём
    // падает (урон, сбит с ног); толкнули с настила — падает; карта игрока с обрывом без лестницы не проходит проверку
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.haz, 0, sizeof w.haz);
        std::memset(w.ladder, 0, sizeof w.ladder);
        std::memset(w.deck, 0, sizeof w.deck);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 30; y++)
            for (int x = 20; x < 30; x++) w.height[y][x] = 2;   // скала 20 фт с x = 20
        Fighter walker = mk(plain(100, 0, 0, 5), 0, 19, 5);
        Boss fb = plain(100, 0, 0, 5);
        fb.fly = true;
        Fighter flyer = mk(fb, 0, 19, 5);
        const bool cliff = !stepOK(w, walker, 19, 5, 0, 20, 5, 0) && stepOK(w, flyer, 19, 5, 0, 20, 5, 0);
        w.ladder[6][20] = true;
        const bool ladder = stepOK(w, walker, 19, 6, 0, 20, 6, 0);
        // настил на 20 фт над (5..9, 15): под ним средний проходит, огромный — нет; летун под настилом обрыв не перелетит
        for (int x = 5; x <= 9; x++) w.deck[15][x] = 2;
        w.ladder[15][4] = true;
        Boss huge = plain(100, 0, 0, 5);
        huge.size = 4;
        Fighter hg = mk(huge, 1, 4, 14);
        const bool clearance = nodeOK(w, walker, 6, 15, 0) && !nodeOK(w, hg, 6, 15, 0);
        w.deck[5][19] = 3;   // навес над клеткой у скалы: летун под ним на скалу не взлетит
        const bool roofFly = !stepOK(w, flyer, 19, 5, 0, 20, 5, 0);
        w.deck[5][19] = 0;
        Fighter p = mk(plain(100, 0, 0, 5), 0, 3, 15);
        std::vector<PStep> path;
        for (int x = 5; x <= 9; x++) w.pit[15][x] = true;   // настил — мост над ущельем: путь только по нему, через лестницу
        pathTo(w, p, 7, 15, path);
        const bool onDeckPath = !path.empty() && path.back().l == 1;
        for (int x = 5; x <= 9; x++) w.pit[15][x] = false;
        w.f.push_back(mk(plain(100, 0, 0, 5), 0, 7, 15));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 7, 16));
        w.f[0].up = true;
        g_curW = &w;
        const int dUpDown = distFt(w.f[0], w.f[1]);
        w.f[1].x = 7;
        w.f[1].y = 15;   // прямо под настилом
        const bool slab = !losF(w, w.f[0], w.f[1]);
        initObjects(w);
        w.f[0].hp = w.f[0].maxHp = 1000;
        w.f[1].hp = w.f[1].maxHp = 1000;
        const bool col = hitDeck(w, 7, 15, 1000, DT_FIRE) && !w.f[0].up && w.f[0].hp < 1000 && w.f[0].cond[C_PRONE] > 0 && w.f[1].hp < 1000 && w.deck[15][7] == 0;
        // толкнули с настила (8,15) на север, где настила нет — падает
        w.f[0].x = 8;
        w.f[0].y = 15;
        w.f[0].up = true;
        w.f[0].cond[C_PRONE] = 0;
        w.f[1].x = 8;
        w.f[1].y = 17;   // толкает с юга
        const int hp0 = w.f[0].hp;
        pushAway(w, w.f[1], w.f[0], 10);
        const bool pushOff = !w.f[0].up && w.f[0].y == 14 && w.f[0].hp < hp0;
        g_curW = nullptr;
        // карта игрока: обрыв без лестницы — разделена, с лестницей — годится
        auto mapJ = [](bool lad) {
            std::string h = "[", l = "[";
            for (int y = 0; y < 8; y++) {
                h += std::string(y ? "," : "") + "\"00002222\"";
                l += std::string(y ? "," : "") + (lad && y == 3 ? "\"00001000\"" : "\"00000000\"");
            }
            return "{\"aw\":8,\"ah\":8,\"height\":" + h + "],\"ladder\":" + l + "]}";
        };
        CustomMap cm;
        std::string err1, err2;
        bool okJ = true;
        const bool bad = !parseCustomMap(parseJson(mapJ(false), &okJ), cm, err1), good = parseCustomMap(parseJson(mapJ(true), &okJ), cm, err2);
        T("T198", "§v21 высоты и ярусы: обрыв (пешком нет, лестница, летун под небом), рост под настилом, путь на настил, обзор сквозь настил, обрушение, толчок с настила, проверка карты",
          cliff && ladder && clearance && roofFly && onDeckPath && dUpDown == 20 && slab && col && pushOff && bad && good,
          fmt("обрыв %d, лестница %d, рост %d, летун под навесом %d, путь на настил %d, на настиле↔внизу %d фт, обзор закрыт %d, обрушение %d, толчок с настила %d, карта: без лестницы «%s», с лестницей %d",
              cliff, ladder, clearance, roofFly, onDeckPath, dUpDown, slab, col, pushOff, err1.substr(0, 40).c_str(), good));
    }
    // T199: мир из шума — участок определяется зерном мира и координатами: тот же мир, построенный заново, и любое окно кэша
    // дают те же клетки; другое зерно — другой мир
    if (TB()) {
        auto same = [](const wg::Cell& a, const wg::Cell& b) {
            return a.wall == b.wall && a.climb == b.climb && a.diff == b.diff && a.dark == b.dark && a.pit == b.pit && a.water == b.water &&
                   a.tree == b.tree && a.ladder == b.ladder && a.strong == b.strong && a.town == b.town && a.road == b.road &&
                   a.height == b.height && a.haz == b.haz && a.deck == b.deck && a.door == b.door && a.biome == b.biome;
        };
        static wg::Plan P1, P2, P3;
        wg::buildPlan(P1, 777, 1000);
        wg::buildPlan(P2, 777, 1000);
        wg::buildPlan(P3, 778, 1000);
        int diffWin = 0, diffPlan = 0, diffSeed = 0, n = 0;
        for (int k = 0; k < 6; k++) {   // участки у поселений (сложные клетки) и случайные
            const bool nearTown = k < 3 && k < (int)P1.towns.size();
            const int cx = nearTown ? P1.towns[k].x : 100 + 150 * k, cy = nearTown ? P1.towns[k].y : 700 - 90 * k;
            std::vector<wg::Cell> a;
            wg::fillLevelCache(P1, cx - 30, cy - 30, 60, 60);
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++) a.push_back(wg::worldCell(P1, x, y));
            wg::fillLevelCache(P1, cx - 7, cy - 45, 90, 70);   // другое окно
            size_t i = 0;
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++, i++) diffWin += !same(a[i], wg::worldCell(P1, x, y));
            wg::fillLevelCache(P2, cx - 25, cy - 25, 50, 50);
            i = 0;
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++, i++) diffPlan += !same(a[i], wg::worldCell(P2, x, y));
            wg::fillLevelCache(P3, cx - 30, cy - 30, 60, 60);
            i = 0;
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++, i++) diffSeed += !same(a[i], wg::worldCell(P3, x, y));
            n += 1600;
        }
        const bool plans = P1.towns.size() == P2.towns.size() && P1.rivers.size() == P2.rivers.size() && P1.roads.size() == P2.roads.size();
        T("T199", "§11 мир из шума: участок задан зерном и координатами (окно и повторная постройка мира не меняют клеток); другое зерно — другой мир",
          diffWin == 0 && diffPlan == 0 && plans && diffSeed > n / 10,
          fmt("клеток %d: отличий при другом окне %d, при новой постройке %d (план одинаков: %d); при другом зерне %d", n, diffWin, diffPlan, plans, diffSeed));
    }
    // T200: биомы по температуре и влажности, объекты по биому: леса (лес, тайга, джунгли) гуще деревьями, чем сухие и холодные
    // открытые (степь, пустыня, тундра, ледник); в мире есть поселения со стенами и дверями, реки с водой
    if (TB()) {
        int cnt[wg::B_N] = {}, trees[wg::B_N] = {};
        int towns = 0, townWall = 0, townDoor = 0, water = 0, worlds = 0;
        for (uint64_t sd : {11ULL, 2026ULL, 31337ULL, 900001ULL}) {
            static wg::Plan P;
            wg::buildPlan(P, sd, 1000);
            worlds++;
            towns += (int)P.towns.size();
            for (int y = 3; y < 1000; y += 9)
                for (int x = 5; x < 1000; x += 9) {
                    const wg::Cell c = wg::worldCell(P, x, y);
                    water += c.water;
                    if (c.town) {
                        townWall += c.wall;
                        townDoor += c.door > 0;
                        continue;
                    }
                    cnt[c.biome]++;
                    trees[c.biome] += c.tree;
                }
            for (auto& t : P.towns)   // дома поселения: стены и двери у центра
                for (int y = t.y - t.r; y <= t.y + t.r; y++)
                    for (int x = t.x - t.r; x <= t.x + t.r; x++) {
                        if (x < 0 || y < 0 || x >= 1000 || y >= 1000) continue;
                        const wg::Cell c = wg::worldCell(P, x, y);
                        if (!c.town) continue;
                        townWall += c.wall;
                        townDoor += c.door > 0;
                    }
        }
        int kinds = 0;
        for (int b = 0; b < wg::B_N; b++) kinds += cnt[b] > 50;
        auto share = [&](std::initializer_list<int> bs) {
            double t = 0, n = 0;
            for (int b : bs) t += trees[b], n += cnt[b];
            return n > 0 ? t / n : 0.0;
        };
        const double woods = share({wg::B_FOREST, wg::B_TAIGA, wg::B_JUNGLE}), open = share({wg::B_STEPPE, wg::B_DESERT, wg::B_TUNDRA, wg::B_GLACIER});
        std::string det;
        for (int b = 0; b < wg::B_N; b++) det += fmt("%s %d, ", wg::biomeName(b), cnt[b]);
        T("T200", "§11 биомы (температура × влажность) и объекты по биому; поселения со стенами и дверями; реки",
          kinds >= 7 && woods > 3 * open && woods > 0.05 && towns >= worlds && townWall > 0 && townDoor > 0 && water > 0,
          fmt("биомов %d из 9 (%sклеток в 4 мирах); деревьев: леса %.3f, открытые %.3f; поселений %d, их стен %d, дверей %d; воды %d",
              kinds, det.c_str(), woods, open, towns, townWall, townDoor, water));
    }
    // T201: двери и крепкие стены — закрытая дверь закрывает обзор; пройти через неё — +5 фт (открывает); открытая не мешает;
    // у двери 18 хитов, выбитая — пустой проём; стена крепости — 120 хитов
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.strong, 0, sizeof w.strong);
        std::memset(w.deck, 0, sizeof w.deck);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 5; y < 15; y++) w.wall[y][10] = true;   // стена с дверью в (10,10)
        w.wall[10][10] = false;
        w.door[10][10] = 1;
        w.wall[20][5] = w.strong[20][5] = true;
        initObjects(w);
        Fighter g = mk(plain(100, 0, 0, 5), 0, 9, 10);
        const bool shutSight = !los(w, 8, 10, 12, 10);
        const int costShut = stepCost(w, g, 9, 10, 10, 10);
        setDoor(w, 10, 10, 2);
        const bool openSight = los(w, 8, 10, 12, 10);
        const int costOpen = stepCost(w, g, 9, 10, 10, 10);
        setDoor(w, 10, 10, 1);
        const bool hp = w.ohp[10][10] == OBJ_DOOR_HP && w.ohp[20][5] == OBJ_STRONG_HP;
        const bool broke = !hitObject(w, 10, 10, 17, DT_BLUDGEONING) && hitObject(w, 10, 10, 1, DT_BLUDGEONING) && w.door[10][10] == 0 && !w.wall[10][10] &&
                           los(w, 8, 10, 12, 10);
        const bool strong = !hitObject(w, 5, 20, 119, DT_BLUDGEONING) && hitObject(w, 5, 20, 1, DT_BLUDGEONING);
        T("T201", "§11 двери: закрытая закрывает обзор, пройти — +5 фт (открывает), открытая не мешает; 18 хитов, выбитая — проём; стена крепости 120 хитов",
          shutSight && costShut == 10 && openSight && costOpen == 5 && hp && broke && strong,
          fmt("обзор закрыта/открыта %d/%d; шаг через закрытую %d фт, открытую %d; хиты %d; выбита %d; крепкая стена %d",
              !shutSight, openSight, costShut, costOpen, hp, broke, strong));
    }
    // T202: размеры арен из мира — стороны 2–100, чаще средние (10–60 — около 80%, 2–5 — около 3%, 80–100 — около 5%)
    if (TB()) {
        const int N = 200000;
        int mn = 999, mx = 0, tiny = 0, mid = 0, huge = 0;
        for (int i = 0; i < N; i++) {
            const int a = pickArenaSide();
            mn = std::min(mn, a);
            mx = std::max(mx, a);
            tiny += a <= 5;
            mid += a >= 10 && a <= 60;
            huge += a >= 80;
        }
        const double pt = 100.0 * tiny / N, pm = 100.0 * mid / N, ph = 100.0 * huge / N;
        T("T202", "§11 размер арены из мира: стороны 2–100, 2–5 около 3%, 10–60 около 80%, 80–100 около 5%",
          mn == 2 && mx == 100 && std::fabs(pt - 3) < 0.5 && std::fabs(pm - 80) < 0.7 && std::fabs(ph - 5) < 0.5,
          fmt("сторона %d–%d; 2–5 %.1f%%, 10–60 %.1f%%, 80–100 %.1f%%", mn, mx, pt, pm, ph));
    }
    // T203: клетки по размеру — большой 2×2, огромный 3×3, громадный 4×4 от левой верхней клетки; расстояние между ближайшими
    // клетками; в проход шириной в клетку крупный не пролезет; область задевает по любой клетке квадрата; чужой квадрат занят
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 30; y++) w.wall[y][20] = y != 15;   // стена с проходом в одну клетку (20,15)
        initObjects(w);
        Boss big = plain(100, 0, 0, 5), huge = plain(100, 0, 0, 5);
        big.size = 3;
        huge.size = 5;
        w.f.push_back(mk(big, 0, 10, 10));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 12, 10));
        w.f.push_back(mk(huge, 1, 2, 20));
        g_curW = &w;
        const Fighter &B = w.f[0], &M = w.f[1], &H = w.f[2];
        const int dBM = distFt(B, M), dHB = distFt(H, B);
        const bool occ = occupied(w, 11, 11, &M) && !occupied(w, 12, 12, &M) && occupied(w, 5, 23, nullptr) && !occupied(w, 6, 24, nullptr);
        std::vector<PStep> pb, pm, ph;
        Boss huge3 = plain(100, 0, 0, 5);
        huge3.size = 4;
        Fighter bigW = mk(big, 0, 15, 15), medW = mk(plain(100, 0, 0, 5), 0, 15, 15), hugeW = mk(huge3, 0, 15, 14);
        pathTo(w, bigW, 25, 15, pb);
        pathTo(w, medW, 25, 15, pm);
        pathTo(w, hugeW, 25, 14, ph);
        // большой протискивается в проход шириной в клетку (как средний: вдвое дороже), огромному (3×3 → 2×2) не пролезть
        const int sqCost = stepCost(w, bigW, 19, 15, 20, 15);
        // огромный не пролезает: идёт как можно ближе (до стены), но за стену не попадает
        const bool hugeStops = !ph.empty() && ph.back().first < 20;
        const bool gap = !pb.empty() && pb.back().first == 25 && !pm.empty() && hugeStops && sqCost == 10 && stepCost(w, medW, 19, 15, 20, 15) == 5;
        // шар радиусом 5 фт вокруг (14,14) задевает угол квадрата большого (11,11), но не (10,10)-клетку отдельно стоящего среднего
        const bool area = inShapeF(SH_SPHERE, 5, 0, 0, 12, 12, B) && !inShapeF(SH_SPHERE, 5, 0, 0, 14, 14, B);
        const bool fit = !nodeOKn(w, bigW, 19, 15, 0, 2) && nodeOK(w, bigW, 19, 15, 0) && fpAt(w, bigW, 20, 15, 0) == 1 && nodeOKn(w, bigW, 21, 15, 0, 2) && !nodeOK(w, hugeW, 19, 14, 0);
        g_curW = nullptr;
        T("T203", "§11 клетки по размеру: большой 2×2, огромный 3×3, громадный 4×4; расстояние между ближайшими клетками; протискивание на размер меньше; область по любой клетке",
          dBM == 5 && dHB == 45 && occ && gap && area && fit,
          fmt("большой–средний вплотную %d фт, громадный–большой %d фт; занятые клетки %d; через проход в клетку: средний %s, большой %s (протискиваясь, шаг %d фт), огромный %s; область %d; помещается %d",
              dBM, dHB, occ, pm.empty() ? "нет" : "да", pb.empty() ? "нет" : "да", sqCost, hugeStops ? "нет (встал у стены)" : "да", area, fit));
    }
    // T204: высота полёта — летун на высоте (10 фт за уровень) дальше от стоящего внизу (ближним с земли не достать), подъём
    // стоит 10 фт движения за уровень, потолок (настил) ограничивает; сбитый с ног летун падает (1к6 за 10 фт); область от земли
    // достаёт летуна не выше своего размера; не летун взлететь не может
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.deck[20][20] = 3;   // навес на 30 фт
        initObjects(w);
        Boss fb = plain(200, 1, 6, 5), gb = plain(200, 1, 6, 5);
        fb.fly = true;
        w.f.push_back(mk(fb, 0, 10, 10));
        w.f.push_back(mk(gb, 1, 11, 10));
        g_curW = &w;
        Fighter &Fl = w.f[0], &Gr = w.f[1];
        TurnState ts;
        ts.speed = 30;
        std::string why;
        const int d0 = distFt(Fl, Gr);
        const bool up = applyPrim(w, 0, ts, Prim{PR_ALT, 2}, true, &why);
        const int d2 = distFt(Fl, Gr), left = ts.speed;
        const bool noReach = d2 > meleeReach(Gr.b);
        TurnState ts2;
        ts2.speed = 30;
        const bool walkerNo = !applyPrim(w, 1, ts2, Prim{PR_ALT, 1}, true, &why);
        const int capOpen = maxAltAt(w, Fl, 10, 10, 0), capRoof = maxAltAt(w, Fl, 20, 20, 0);
        Fl.alt = 3;
        const bool area = !inShapeF(SH_SPHERE, 20, 0, 0, 10, 10, Fl);
        Fl.alt = 2;
        const bool area2 = inShapeF(SH_SPHERE, 20, 0, 0, 10, 10, Fl);
        const int hp0 = Fl.hp;
        applyCond(Fl, C_PRONE, 1, -1, 0, false);
        const bool fell = Fl.alt == 0 && Fl.hp < hp0 && Fl.hp >= hp0 - 12;
        g_curW = nullptr;
        T("T204", "§7 высота полёта: 10 фт за уровень, ближним с земли не достать, потолок, падение сбитого (1к6 за 10 фт), область от земли",
          up && d0 == 5 && d2 == 20 && left == 10 && noReach && walkerNo && capOpen == ALT_MAX && capRoof == 2 && area && area2 && fell,
          fmt("подъём на 20 фт %d (осталось движения %d из 30); расстояние до стоящего рядом %d → %d фт (ближним не достать %d); не летун не взлетел %d; предел высоты %d, под навесом в 30 фт %d; огненный шар 20 фт: летуна на 30 фт не задел %d, на 20 фт задел %d; сбит с ног — упал %d (−%d)",
              up, left, d0, d2, noReach, walkerNo, capOpen, capRoof, area, area2, fell, hp0 - Fl.hp));
    }
    // T205: бойцы в одной клетке на разной высоте — мешают, только если пересекаются по высоте (рост: средний 10 фт, большой
    // 20): под стоящим на мосту пройти можно; под летуном на 10 фт пройдёт средний, большой — нет
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int x = 5; x < 15; x++) w.deck[10][x] = 2;   // мост на 20 фт
        initObjects(w);
        Boss fb = plain(100, 0, 0, 5), big = plain(100, 0, 0, 5);
        fb.fly = true;
        big.size = 3;
        w.f.push_back(mk(plain(100, 0, 0, 5), 0, 8, 10));   // на мосту
        w.f[0].up = true;
        w.f.push_back(mk(fb, 0, 20, 20));                    // летун на 10 фт
        w.f[1].alt = 1;
        g_curW = &w;
        Fighter walker = mk(plain(100, 0, 0, 5), 1, 8, 9), bigW = mk(big, 1, 19, 19);
        const bool underBridge = !blockedAt(w, walker, 8, 10) && blockedAt(w, w.f[0], 8, 10) == false;
        Fighter onDeck = mk(plain(100, 0, 0, 5), 1, 9, 10);
        onDeck.up = true;
        const bool deckBlock = blockedAt(w, onDeck, 8, 10);   // на мосту — та же клетка, тот же уровень
        const bool underFlier = !blockedAt(w, walker, 20, 20) && blockedAt(w, bigW, 19, 19);
        g_curW = nullptr;
        T("T205", "§11 бойцы в одной клетке на разной высоте: мешают, только если пересекаются по высоте (под мостом, под летуном)",
          underBridge && deckBlock && underFlier,
          fmt("под стоящим на мосту пройти %d, на мосту — занято %d; под летуном на 10 фт: средний проходит, большой упирается %d", underBridge, deckBlock, underFlier));
    }
    // T206: скорости движения (v21) — лазун: лазалка и подъём без надбавки, по обрыву как по лестнице; пловец: вода не
    // труднопроходима; копатель: сквозь ущелье и обрыв; у каждого атома цена, конструктор их принимает
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.water, 0, sizeof w.water);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.ladder, 0, sizeof w.ladder);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 30; y++) w.height[y][15] = w.height[y][16] = 3;   // скала в 30 фт
        w.climb[5][5] = true;
        w.water[6][6] = w.diff[6][6] = true;
        for (int y = 0; y < 30; y++) w.pit[y][25] = true;   // ущелье
        initObjects(w);
        Boss cb = plain(100, 0, 0, 5), sb = cb, bb = cb, pb = cb;
        cb.climbS = true;
        sb.swimS = true;
        bb.burrowS = true;
        Fighter C1 = mk(cb, 0, 14, 10), S1 = mk(sb, 0, 5, 6), B1 = mk(bb, 0, 24, 10), P1 = mk(pb, 0, 14, 10);
        const bool cliff = stepOK(w, C1, 14, 10, 0, 15, 10, 0) && !stepOK(w, P1, 14, 10, 0, 15, 10, 0) && stepCost(w, C1, 14, 10, 15, 10) == 5;
        const bool climbT = stepCost(w, C1, 4, 5, 5, 5) == 5 && stepCost(w, P1, 4, 5, 5, 5) == 10;
        const bool swim = stepCost(w, S1, 5, 6, 6, 6) == 5 && stepCost(w, P1, 5, 6, 6, 6) == 10;
        std::vector<PStep> pbw, ppw;
        pathTo(w, B1, 27, 10, pbw);
        Fighter P2 = mk(pb, 0, 24, 10);
        pathTo(w, P2, 27, 10, ppw);
        const bool burrow = !pbw.empty() && pbw.back().first == 27 && (ppw.empty() || ppw.back().first < 25);
        Coef C2 = CC();
        g_B = 1600;
        const bool price = C2[AT_CLIMB] > 0 && C2[AT_SWIM] > 0 && C2[AT_BURROW] > 0;
        bool ok = true;
        Boss cbx;
        long long bu = 0;
        std::string e;
        const bool ctor = chr::buildCharacter(parseJson("{\"budget\":1600,\"passives\":{\"climbS\":true,\"swimS\":true,\"burrowS\":true}}", &ok), C2, cbx, bu, e) && cbx.climbS && cbx.swimS && cbx.burrowS;
        T("T206", "§4 скорости лазания, плавания, копания: лазун по скале и лазалке без надбавки, пловец по воде, копатель сквозь ущелье; цена, конструктор",
          cliff && climbT && swim && burrow && price && ctor,
          fmt("скала %d, лазалка %d, вода %d, ущелье %d, цена %d, конструктор %d %s", cliff, climbT, swim, burrow, price, ctor, e.c_str()));
    }
    // T207: Увёртливость (ЛОВ от области: успех — 0, провал — половина), солнечная чувствительность (днём под небом — помеха,
    // ночью и под навесом — нет), действия логова (обвал бьёт врагов у центра, потом трясина — не подряд), превращение (свой запас
    // хитов облика, способности недоступны, хиты облика кончились — свой облик и лишний урон на свои хиты)
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.dark, 0, sizeof w.dark);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.deck[5][5] = 3;
        initObjects(w);
        g_curW = &w;
        // Увёртливость: 400 огненных шаров по увёртливому и обычному с ЛОВ 10
        Boss caster = plain(100, 0, 0, 5);
        Spec fb;
        fb.delivery = 1;
        fb.saveStat = DEX;
        fb.range = 60;
        fb.radius = 20;
        fb.dmgN = 8;
        fb.dmgS = 6;
        fb.dtype = DT_FIRE;
        caster.abil = {fb};
        Boss ev = plain(100000, 0, 0, 5), no = ev;
        ev.evasion = true;
        w.f.push_back(mk(caster, 0, 10, 10));
        w.f.push_back(mk(ev, 1, 14, 10));
        w.f.push_back(mk(no, 1, 14, 12));
        long long dEv = 0, dNo = 0, zero = 0;
        for (int i = 0; i < 400; i++) {
            const int h1 = w.f[1].hp, h2 = w.f[2].hp;
            hitOne(w, fb, w.f[0], w.f[1]);
            hitOne(w, fb, w.f[0], w.f[2]);
            dEv += h1 - w.f[1].hp;
            dNo += h2 - w.f[2].hp;
            zero += h1 == w.f[1].hp;
        }
        const bool evasion = dEv * 10 < dNo * 6 && zero > 100;
        // солнце
        Boss ss = plain(100, 1, 6, 5);
        ss.sunSens = true;
        Fighter sa = mk(ss, 0, 10, 10), sb = mk(plain(100, 1, 6, 5), 1, 11, 10), sc = mk(ss, 0, 5, 5);
        bool a1, d1, a2, d2, a3, d3;
        w.day = true;
        advState(w, sa, sb, 5, a1, d1);
        advState(w, sc, sb, 5, a2, d2);   // под навесом
        w.day = false;
        advState(w, sa, sb, 5, a3, d3);
        w.day = true;
        const bool sun = d1 && !d2 && !d3;
        // логово: обвал по двум врагам рядом, затем трясина
        w.f.clear();
        Boss lb = plain(500, 0, 0, 5);
        lb.lair = 2;
        w.f.push_back(mk(lb, 0, 2, 2));
        w.f.push_back(mk(plain(500, 0, 0, 5), 1, 20, 20));
        w.f.push_back(mk(plain(500, 0, 0, 5), 1, 21, 20));
        const int lh1 = w.f[1].hp, lh2 = w.f[2].hp;
        lairAction(w, 0);
        const bool rock = w.f[1].hp < lh1 && w.f[2].hp < lh2 && lh1 - w.f[1].hp <= 40 && w.f[0].lairLast == 1;
        lairAction(w, 0);
        const bool bog = w.f[0].lairLast == 2 && w.diff[20][20] && w.diff[22][22];
        // превращение
        w.f.clear();
        Boss dr = plain(50, 1, 4, 5), bear = plain(30, 2, 8, 5);
        bear.size = 3;
        dr.tmpl = {bear};
        Spec fs;
        fs.self = true;
        fs.delivery = 2;
        fs.formTmpl = 0;
        fs.uses = 1;
        dr.abil = {fs};
        w.f.push_back(mk(dr, 0, 10, 10));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 20, 20));
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        std::string why;
        const bool cast = applyPrim(w, 0, ts, Prim{PR_SPELL, 0, 0}, true, &why);
        Fighter& D = w.f[0];
        const bool inForm = D.inForm && D.hp == 30 && D.b.size == 3 && D.b.wN == 2 && fpN(D) == 2;
        TurnState ts2;
        ts2.speed = 30;
        const bool blocked = !applyPrim(w, 0, ts2, Prim{PR_SPELL, 0, 0}, true, &why);
        hurt(D, 40, DT_BLUDGEONING, false);
        const bool back = !D.inForm && D.hp == 40 && D.b.size == 2 && D.b.wN == 1;
        g_curW = nullptr;
        T("T207", "§9 Увёртливость, солнечная чувствительность, действия логова, превращение",
          evasion && sun && rock && bog && cast && inForm && blocked && back,
          fmt("урон огненным шаром: увёртливый %lld, обычный %lld (без урона %lld из 400); солнце %d; логово: обвал %d, трясина %d; превращение %d, в облике %d, способности закрыты %d, возврат (хиты 40) %d %s",
              dEv, dNo, zero, sun, rock, bog, cast, inForm, blocked, back, why.c_str()));
    }
    // T208: призыв у персонажа (конструктор) — группы разных приспешников, цена = шаблоны × копии × применения; призыв
    // ставит копии всех групп; превращение из конструктора; у генератора есть смешанные призывы
    if (TB()) {
        Coef C2 = CC();
        const char* wolf = "{\"v\":21,\"budget\":150,\"stats\":[12,15,12,3,12,6],\"acV\":3,\"main\":1,\"weapon\":{\"n\":2,\"s\":4,\"range\":5,\"att\":1,\"reach\":5,\"type\":11,\"finesse\":true},\"passives\":{\"speed\":40}}";
        const char* skel = "{\"v\":21,\"budget\":120,\"stats\":[10,14,15,6,8,5],\"acV\":3,\"main\":1,\"weapon\":{\"n\":1,\"s\":6,\"range\":5,\"att\":1,\"reach\":5,\"type\":11,\"finesse\":true},\"passives\":{\"speed\":30}}";
        const char* bear = "{\"v\":21,\"budget\":400,\"stats\":[19,10,16,2,13,7],\"acV\":1,\"main\":0,\"size\":3,\"weapon\":{\"n\":2,\"s\":6,\"range\":5,\"att\":2,\"reach\":5,\"type\":10},\"passives\":{\"speed\":40,\"climbS\":true}}";
        auto mk1 = [&](const std::string& abil, Boss& b, std::string& e) {
            bool ok = true;
            long long bu = 0;
            const std::string js = "{\"v\":21,\"budget\":3000,\"stats\":[10,10,10,10,10,10],\"weapon\":{\"n\":1,\"s\":6,\"range\":5,\"att\":1,\"reach\":5,\"type\":11},\"abilities\":[" + abil + "]}";
            return chr::buildCharacter(parseJson(js, &ok), C2, b, bu, e);
        };
        Boss b1, b2, b3;
        std::string e1, e2, e3;
        const bool one = mk1(std::string("{\"self\":true,\"uses\":1,\"summon\":{\"groups\":[{\"n\":2,\"minion\":") + wolf + "}]}}", b1, e1);
        const bool two = mk1(std::string("{\"self\":true,\"uses\":1,\"summon\":{\"groups\":[{\"n\":2,\"minion\":") + wolf + "},{\"n\":1,\"minion\":" + skel + "}]}}", b2, e2);
        const bool form = mk1(std::string("{\"self\":true,\"uses\":1,\"form\":{\"body\":") + bear + "}}", b3, e3);
        const bool parse = one && two && form && b2.abil[0].sumN == 2 && b2.abil[0].sumMore.size() == 1 && b2.tmpl.size() == 2 && b3.abil[0].formTmpl == 0;
        g_B = 3000 + STAT_BASE;
        const bool price = parse && b2.abil[0].price > b1.abil[0].price && b3.abil[0].price > 0;
        // призыв ставит волков и скелета
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.height, 0, sizeof w.height);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.f.reserve(64);
        initObjects(w);
        int wolves = 0, skels = 0;
        if (parse) {
            w.f.push_back(mk(b2, 0, 10, 10));
            w.f[0].uses = {1};
            g_curW = &w;
            summonFrom(w, 0, 0);
            g_curW = nullptr;
            for (size_t i = 1; i < w.f.size(); i++) (w.f[i].b.wN == 2 ? wolves : skels)++;
        }
        int mixed = 0;
        for (int i = 0; i < 3000; i++) {
            Boss b = makeBoss(1600, C2, true, true);
            for (auto& sp : b.abil) mixed += !sp.sumMore.empty();
        }
        T("T208", "§16 призыв у персонажа: группы разных приспешников из конструктора, цена, призыв ставит все группы; превращение из конструктора; смешанные призывы у генератора",
          parse && price && wolves == 2 && skels == 1 && mixed > 0,
          fmt("разбор %d (%s%s%s), цена %d, волков %d, скелетов %d, смешанных призывов у генератора %d из 3000", parse, e1.c_str(), e2.c_str(), e3.c_str(), price, wolves, skels, mixed));
    }
    // T209: нарастающая стойкость (этап 7) — паралич СЛ 20 на 10 раундов: без стойкости держался бы ~8 раундов из 10, с ней
    // каждый провал +2 к следующему спасброску — в среднем меньше 5; лёгкие состояния (опутан) стойкости не дают
    if (TB()) {
        double sumP = 0, sumR = 0;
        const int N = 4000;
        for (int i = 0; i < N; i++) {
            Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1), g = f;
            applyCond(f, C_PARAL, 10, WIS, 20, true);
            applyCond(g, C_RESTR, 10, STR, 20, true);
            int rp = 0, rr = 0;
            while (f.cond[C_PARAL] > 0 && rp < 20) { endOfTurn(f); rp++; }
            while (g.cond[C_RESTR] > 0 && rr < 20) { endOfTurn(g); rr++; }
            sumP += rp;
            sumR += rr;
        }
        const double mp = sumP / N, mr = sumR / N;
        T("T209", "§6 нарастающая стойкость: каждый проваленный повторный спасбросок от жёсткого контроля — +2 к следующему",
          mp < 5.0 && mr > 7.0, fmt("паралич СЛ 20 держится в среднем %.2f раунда, опутанность СЛ 20 — %.2f (без стойкости)", mp, mr));
    }
