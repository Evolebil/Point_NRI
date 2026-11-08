// Проверки движка, часть 10 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== v21, этап 2: бой =====
    auto open = [&]() {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.dark, 0, sizeof w.dark);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.height, 0, sizeof w.height);
        w.aw = w.ah = 100;
        return w;
    };
    // T171: №13 — кто атаковал врага или остался там, где враг достанет его за ход, погоню не начинает
    if (TB()) {
        auto chaseAfter = [&](bool struck, int to) {
            World w = open();
            w.f.push_back(mk(plain(100, 1, 6, 5), 0, 50, 50));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 52, 50));
            w.f[0].x = to;
            w.f[0].struckFoe = struck;
            chaseAfterTurn(w, 0, 10);   // было 10 фт до врага
            return w.chaseOn;
        };
        const bool flee = chaseAfter(false, 40), hitRun = chaseAfter(true, 40), near = chaseAfter(false, 46);
        T("T171", "§v21 №13: отход на 10+ фт из-под угрозы — погоня; если в этот ход атаковал или остался в досягаемости врага за ход — нет",
          flee && !hitRun && !near, fmt("ушёл на 60 фт: %d; ударил и ушёл: %d; ушёл на 30 фт (враг дойдёт и ударит): %d", flee, hitRun, near));
    }
    // T172: №17 — область бьёт всех в радиусе (и своих, и себя), избирательная — только врагов; бот не бьёт областью,
    // если вред своим больше пользы
    if (TB()) {
        auto run = [&](bool selective) {
            World w = open();
            Boss a = plain(1000, 0, 0, 5);
            Spec s;
            s.delivery = 2;
            s.range = 30;
            s.radius = 15;
            s.dmgN = 4;
            s.selective = selective;
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 13, 10));
            w.f.push_back(mk(plain(1000, 0, 0, 5), 0, 14, 10));   // свой рядом с врагом
            doSpec(w, w.f[0], 0, 1);
            return std::array<int, 3>{w.f[0].hp, w.f[1].hp, w.f[2].hp};
        };
        const auto all = run(false), sel = run(true);
        World v = open();
        Boss c = plain(1000, 0, 0, 5);
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.radius = 10;
        s.dmgN = 6;
        c.abil.push_back(s);
        v.f.push_back(mk(c, 0, 10, 10));
        v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
        v.f.push_back(mk(plain(30, 0, 0, 5), 0, 15, 10));     // свой миньон рядом с врагом
        v.f.push_back(mk(plain(30, 0, 0, 5), 0, 13, 10));
        v.f[0].init = 2000;
        v.f[0].uses[0] = 1;
        takeTurn(v, 0);
        const bool spared = v.f[2].hp == 30 && v.f[3].hp == 30;
        T("T172", "§v21 №17: область бьёт и своих, и себя; избирательная — только врагов; бот не бьёт областью, если заденет больше своих",
          all[1] < 1000 && all[2] < 1000 && sel[1] < 1000 && sel[2] == 1000 && spared,
          fmt("обычная: враг %d, свой %d; избирательная: враг %d, свой %d; бот пощадил своих %d", all[1], all[2], sel[1], sel[2], spared));
    }
    // T173: №12 — туман войны: бот не бьёт врага, которого его сторона не видит и не знает; атака выдаёт позицию; игрок
    // получает в запросе только известных врагов
    if (TB()) {
        World w = open();
        for (int y = 0; y < 30; y++) w.wall[y][15] = true;   // стена между ними
        Boss a = plain(1000, 1, 6, 60);
        a.wAmmo = true;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 20, 10));
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        g_curRound = 5;
        const bool unknown = !isKnown(w, 0, 1);
        bool crit;
        attackRoll(w, w.f[1], w.f[0], PROF, 5, crit);   // враг атаковал (сквозь стену — неважно): выдал себя
        const bool revealed = isKnown(w, 0, 1);
        w.f[1].x = 21;   // ушёл с клетки — снова неизвестен
        const bool lost = !isKnown(w, 0, 1);
        std::string req;
        g_human.idx = 0;
        g_human.ask = [&](const std::string& r) { req = r; return std::string("{\"c\":\"end\"}"); };
        humanTurn(w, 0);
        g_human = HumanCtx{};
        const bool noEnemy = req.find("\"enemies\":[]") != std::string::npos;
        g_curRound = 0;
        T("T173", "§v21 №12: туман войны — враг за стеной неизвестен; атака выдаёт позицию; ушёл — снова неизвестен; игроку не показан",
          unknown && revealed && lost && noEnemy, fmt("неизвестен %d, выдал себя %d, ушёл %d, запрос игрока без врагов %d", unknown, revealed, lost, noEnemy));
    }
    // T177: каждое решение в ходу что-то тратит: шаг в свою клетку отклоняется; смена оружия — 1-я бесплатно, 2-я бонусным
    // действием, 3-я действием, 4-й нет; простой приказ — раз за ход
    if (TB()) {
        World w = open();
        Boss a = plain(1000, 1, 6, 5);
        a.alt.push_back(Wp{});
        a.alt[0].n = 1;
        a.alt[0].s = 8;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 20, 10));
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        const bool stay = !applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 10, 10}, true);
        const bool s1 = applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true) && !ts.bonusUsed && !ts.actionUsed;
        const bool s2 = applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true) && ts.bonusUsed && !ts.actionUsed;
        const bool s3 = applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true) && ts.actionUsed;
        const bool s4 = !applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true);
        const bool o1 = applyPrim(w, 0, ts, Prim{PR_ORDER, 1}, true), o2 = !applyPrim(w, 0, ts, Prim{PR_ORDER, 1}, true);
        T("T177", "§v21 ход: шаг в свою клетку не делается; смена оружия 1-я бесплатно, 2-я бонусным, 3-я действием, 4-й нет; простой приказ раз за ход",
          stay && s1 && s2 && s3 && s4 && o1 && o2,
          fmt("шаг на месте отклонён %d; смены %d/%d/%d, 4-я отклонена %d; приказ %d, второй отклонён %d", stay, s1, s2, s3, s4, o1, o2));
    }
    // T178: упёрся в невидимого врага — тот становится известен команде, попытка стоит 5 фт, боец остаётся на месте; в
    // коридоре известного врага не обойти — шаг отклоняется; в открытом поле известного врага обходит кратчайшим путём
    if (TB()) {
        World w = open();
        for (int x = 0; x < W; x++) w.wall[9][x] = w.wall[11][x] = true;   // коридор по y = 10 во всю ширину
        w.f.push_back(mk(plain(1000, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 11, 10));
        w.f[1].invisRounds = 5;
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        g_curRound = 3;
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        const bool hidden = !isKnown(w, 0, 1);
        const bool ok1 = applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 13, 10}, true);
        const bool bump = ok1 && w.f[0].x == 10 && w.f[0].y == 10 && ts.speed == 25 && isKnown(w, 0, 1);
        const bool blocked = !applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 13, 10}, true) && ts.speed == 25;
        for (int x = 0; x < W; x++) w.wall[9][x] = w.wall[11][x] = false;   // коридор открыт: обход (11,9) или (11,11), 3 шага
        const bool ok3 = applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 13, 10}, true);
        const bool around = ok3 && w.f[0].x == 13 && w.f[0].y == 10 && ts.speed == 10;
        g_curRound = 0;
        T("T178", "§v21 путь: упёрся в невидимого врага — он раскрыт, 5 фт потрачено, боец на месте; известного в коридоре не обойти; в поле — обход",
          hidden && bump && blocked && around,
          fmt("невидим %d; упор %d; в коридоре отклонено %d; обход %d, клетка (%d,%d), скорость %d", hidden, bump, blocked, around, w.f[0].x, w.f[0].y, ts.speed));
    }
    // T179: способность — единая структура (переходник): у вооружённого босса с одной способностью-бонусом, Отходом
    // бонусным и призывом — атака, способность, Рывок, Уклонение, Отход (действием и бонусным), призыв; категории
    // выводятся из эффекта; цену проверяет и списывает один путь (canPay, payEcon, paySpec)
    if (TB()) {
        World w = open();
        Boss a = plain(1000, 1, 6, 5);
        Spec sp;
        sp.bonus = true;
        sp.uses = 1;
        sp.slotLvl = 1;
        sp.dmgN = 1;
        a.abil.push_back(sp);
        a.slots[1] = 1;
        a.disengB = true;
        addSummon(a, plain(50, 1, 4, 5), 1);
        w.f.push_back(mk(a, 0, 10, 10));
        const auto ab = abilitiesOf(w.f[0].b);
        int cat[CAT_N] = {};
        for (auto& x : ab) cat[abCategory(w.f[0].b, x)]++;
        const bool list = ab.size() == 9 && cat[CAT_ATTACK] == 1 && cat[CAT_SPELL] == 1 && cat[CAT_SUMMON] == 1 && cat[CAT_MANEUVER] == 6;   // v21: + Помощь и Подготовка
        TurnState ts;
        const AbCost c = specAbility(w.f[0].b, 0).cost;
        const bool before = canPay(w.f[0], ts, c) && c.econ == EC_BONUS;
        paySpec(w, w.f[0], 0, true);
        payEcon(ts, w.f[0], c.econ);
        const char* why = nullptr;
        const bool after = !canPay(w.f[0], ts, c, &why) && ts.bonusUsed && !ts.actionUsed && w.f[0].slotLeft[1] == 0;
        const bool weapon = canPay(w.f[0], ts, builtinAbility(EF_WEAPON).cost);
        T("T179", "§v21 способность: переходник даёт атаку, способности, Рывок, Уклонение, Отход, призыв; категории из эффекта; цену списывает один путь",
          list && before && after && weapon,
          fmt("способностей %zu (атака %d, заклинание %d, призыв %d, манёвр %d); до платы %d, после %d («%s»), атака ещё доступна %d", ab.size(), cat[CAT_ATTACK],
              cat[CAT_SPELL], cat[CAT_SUMMON], cat[CAT_MANEUVER], before, after, why ? why : "", weapon));
    }
    // T180: призыв — способность: тратит действие (второй раз в ход не пройдёт) и применение; две призывающие способности
    // с разными шаблонами работают независимо (свои шаблоны, свои счётчики)
    if (TB()) {
        World w = open();
        w.f.reserve(64);
        Boss a = plain(1000, 1, 6, 5), t1 = plain(40, 1, 4, 5), t2 = plain(90, 1, 8, 5);
        const int s1 = addSummon(a, t1, 2, 2), s2 = addSummon(a, t2, 1, 1);
        a.abil[s2].bonus = true;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 30, 10));
        w.f[0].uses[s1] = 2;
        w.f[0].uses[s2] = 1;
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        const bool c1 = applyPrim(w, 0, ts, Prim{PR_SPELL, s1, 0}, true) && ts.actionUsed && w.f.size() == 4 && w.f[0].uses[s1] == 1;
        const bool again = !applyPrim(w, 0, ts, Prim{PR_SPELL, s1, 0}, true) && w.f.size() == 4;   // действие уже потрачено
        const bool c2 = applyPrim(w, 0, ts, Prim{PR_SPELL, s2, 0}, true) && ts.bonusUsed && w.f.size() == 5;
        const bool tmplOk = w.f[2].maxHp == 40 && w.f[3].maxHp == 40 && w.f[4].maxHp == 90 && w.f[2].sumSpec == s1 && w.f[4].sumSpec == s2;
        TurnState ts2;
        const bool used = !applyPrim(w, 0, ts2, Prim{PR_SPELL, s2, 0}, true);   // одно применение — израсходовано
        T("T180", "§v21 призыв — способность: тратит действие и применение; две призывающие способности со своими шаблонами независимы",
          c1 && again && c2 && tmplOk && used,
          fmt("1-я: %d, повтор в тот же ход отклонён %d, 2-я (бонусным) %d, шаблоны ХП %d/%d/%d, применение 2-й израсходовано %d", c1, again, c2,
              w.f[2].maxHp, w.f[3].maxHp, w.f.size() > 4 ? w.f[4].maxHp : -1, used));
    }
    // T181: призыв на концентрации — концентрация снята (сорвалась или снята бесплатно), приспешники этой способности
    // исчезают; приспешники другой способности (без концентрации) остаются до гибели
    if (TB()) {
        World w = open();
        w.f.reserve(64);
        Boss a = plain(1000, 1, 6, 5);
        const int sc = addSummon(a, plain(40, 1, 4, 5), 2, UNL, true), sn = addSummon(a, plain(40, 1, 4, 5), 1);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 30, 10));
        doSpec(w, w.f[0], sc, 0);
        doSpec(w, w.f[0], sn, 0);
        const bool held = w.f[0].concSpec == sc && w.f.size() == 5;
        TurnState ts;
        applyPrim(w, 0, ts, Prim{PR_DROPCONC}, true);
        const bool gone = w.f[2].hp <= 0 && w.f[3].hp <= 0 && w.f[2].vanished && w.f[4].hp > 0 && !w.f[4].vanished && w.f[0].concSpec < 0;
        T("T181", "§v21 призыв на концентрации: концентрация снята — его приспешники исчезают; призванные без концентрации остаются",
          held && gone, fmt("концентрация держит призыв %d; после снятия: исчезли %d/%d, другой остался %d", held, w.f[2].vanished, w.f[3].vanished, w.f[4].hp > 0));
    }
    // T182: предохранители и растяжки призыва: растяжка срабатывает раньше потолка (только запись), потолок блокирует
    // (limity.json: живых от способности 20, растяжка 6)
    if (TB()) {
        World w = open();
        w.f.reserve(256);
        Boss a = plain(1000, 1, 6, 5);
        const int si = addSummon(a, plain(40, 1, 4, 5), 4);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 60, 60));
        const Limits& L = limits();
        const long long w0 = g_warnCount.load();
        int firstWarnAt = -1;
        for (int k = 0; k < 10; k++) {
            g_curRound = k;
            summonFrom(w, 0, si);
            int alive = 0;
            for (auto& g : w.f) alive += g.summoner == 0 && g.hp > 0;
            if (firstWarnAt < 0 && g_warnCount.load() > w0) firstWarnAt = alive;
        }
        g_curRound = 0;
        int alive = 0;
        for (auto& g : w.f) alive += g.summoner == 0 && g.hp > 0;
        TurnState ts;
        const bool blocked = !applyPrim(w, 0, ts, Prim{PR_SPELL, si, 0}, true);
        T("T182", "§v21 предохранители призыва: растяжка (WARN) раньше потолка, потолок живых от способности блокирует призыв",
          firstWarnAt > L.warnAliveAbil && firstWarnAt < L.sumAliveAbil && alive == L.sumAliveAbil && blocked,
          fmt("первая растяжка при %d живых (порог %d), живых в итоге %d (потолок %d), призыв сверх потолка отклонён %d", firstWarnAt, L.warnAliveAbil, alive,
              L.sumAliveAbil, blocked));
    }
    // T174: №4 — ход из примитивов: бой, где ходы бота записаны примитивами и повторены исполнителем, совпадает до бита
    if (TB()) {
        Coef C2 = CC();
        int same = 0, N = 80;
        long long prims = 0;
        for (int i = 0; i < N; i++) {
            auto run = [&](PrimLog& L, double* hp) {
                rng.seed(7700 + i);
                Boss A, Bo;
                makePair(200LL << (i % 6), C2, A, Bo);
                A.stats[INT] = Bo.stats[INT] = 10;
                g_primLog = &L;
                g_rounds = 0;
                const int r = fight(A, Bo);
                g_primLog = nullptr;
                hp[0] = g_lastHp[0];
                hp[1] = g_lastHp[1];
                return r * 100000 + (int)g_rounds;
            };
            PrimLog L;
            L.who = i & 1;
            double h1[2], h2[2];
            const int r1 = run(L, h1);
            for (auto& t : L.turns) prims += (long long)t.size();
            L.replay = true;
            const int r2 = run(L, h2);
            same += r1 == r2 && h1[0] == h2[0] && h1[1] == h2[1];
        }
        T("T174", "§v21 №4: ход бота записывается примитивами (идти, атака, способность, Рывок, …); повтор примитивов исполнителем даёт тот же бой до бита",
          same == N, fmt("совпало %d из %d боёв, примитивов %.1f на бой", same, N, (double)prims / N));
    }
    // T175: №5 — новая сеть: наблюдение (окно карты, кандидаты-клетки, известные враги), маски не пускают недопустимого
    // (случайные веса, 60 боёв: отказов исполнителя 0)
    if (TB()) {
        Coef C2 = CC();
        pn::PNet net;
        net.init(3);
        long long steps = 0, illegal = 0;
        for (int i = 0; i < 60; i++) {
            rng.seed(8800 + i);
            Boss A, Bo;
            makePair(200LL << (i % 6), C2, A, Bo);
            pn::Ctx ctx;
            ctx.net = &net;
            ctx.greedy = false;
            ctx.rng.seed(i + 1);
            ctx.side[0].active = true;
            pn::g_pctx = &ctx;
            g_rlTeam[0] = true;
            fight(A, Bo);
            g_rlTeam[0] = false;
            pn::g_pctx = nullptr;
            steps += ctx.steps;
            illegal += ctx.illegal;
        }
        T("T175", "§v21 №5: новая сеть видит окно карты 15×15 и клетки-кандидаты; со случайными весами 60 боёв без единого недопустимого примитива",
          pn::D_OLD == pn::GB + pn::MAXE * pn::FE + MAXA * pn::FA + pn::WIN * pn::WIN * pn::NL && pn::D_IN24 == pn::D_OLD + pn::MAXE * pn::FEX + MAXA * pn::FAX &&
              pn::D_IN25 == pn::D_IN24 + pn::GBX + pn::MAXE * pn::FEX2 + pn::WIN * pn::WIN * pn::NLX + MAXA * pn::FAX2 &&
              pn::D_IN == pn::D_IN25 + pn::GB6 + pn::MAXE * pn::FE6 + MAXA * pn::FA6 && steps > 0 && illegal == 0,   // PN26 (пакеты атомов 1–3)
          fmt("вход %d, решений %lld, отказов %lld", pn::D_IN, steps, illegal));
    }
    // T183: шаг 4 — пустой ход сети запрещён: в начале хода, когда можно идти или применить, «конец хода» закрыт; после
    // шага открыт; тратить нечего — легален только «конец хода», его делает движок, и в обучающую выборку он не идёт;
    // метка «конец пустого хода» (как у бота) непредставима; ход сети всегда кончается
    if (TB()) {
        World w = open();
        w.f.reserve(64);
        w.f.push_back(mk(plain(1000, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 30, 10));
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        ts.x0 = 10;
        ts.y0 = 10;
        pn::Obs o;
        pn::buildObs(w, 0, ts, o);
        const bool closed = !o.stMask[pn::ST_END] && o.stMask[pn::ST_MOVE];
        // шаг туда и обратно — не трата: «конец хода» остаётся закрыт
        applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 11, 10}, true);
        applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 10, 10}, true);
        pn::buildObs(w, 0, ts, o);
        const bool backClosed = !o.stMask[pn::ST_END] && w.f[0].x == 10;
        // Уклонение ход непустым не делает
        applyPrim(w, 0, ts, Prim{PR_DODGE}, true);
        pn::buildObs(w, 0, ts, o);
        const bool dodgeClosed = !o.stMask[pn::ST_END] && ts.actionUsed;
        pn::Decision dl;
        const bool unrep = !pn::labelOf(w, 0, o, nullptr, dl, [](std::vector<pn::Cand>&, int, int) { return 0; });
        applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 12, 10}, true);
        pn::buildObs(w, 0, ts, o);
        const bool open1 = o.stMask[pn::ST_END];
        TurnState t0;   // ни скорости, ни действий: тратить нечего
        t0.actionUsed = t0.bonusUsed = true;
        pn::buildObs(w, 0, t0, o);
        const bool forced = pn::onlyEnd(o);
        // 20 боёв случайной сети с записью решений: ни одного записанного «только конец хода», каждый записанный конец хода
        // был разрешён, бои кончаются
        Coef C2 = CC();
        pn::PNet net;
        net.init(5);
        long long recEnd = 0, badEnd = 0, onlyRec = 0, turns = 0, forcedEnds = 0, caps = 0;
        int done = 0;
        for (int i = 0; i < 20; i++) {
            rng.seed(9100 + i);
            Boss A, Bo;
            makePair(400, C2, A, Bo);
            pn::Ctx ctx;
            ctx.net = &net;
            ctx.greedy = false;
            ctx.rng.seed(i + 7);
            ctx.side[0].active = true;
            ctx.side[0].record = true;
            pn::g_pctx = &ctx;
            g_rlTeam[0] = true;
            fight(A, Bo);
            done++;
            g_rlTeam[0] = false;
            pn::g_pctx = nullptr;
            for (auto& d : ctx.side[0].traj) {
                if (d.st == pn::ST_END) recEnd++, badEnd += !d.stMask[pn::ST_END];
                onlyRec += !d.stMask[pn::ST_MOVE] && !d.stMask[pn::ST_APPLY] && !d.stMask[pn::ST_INTERACT];
            }
            turns += ctx.turns;
            forcedEnds += ctx.forcedEnds;
            caps += ctx.capHits;
        }
        T("T183", "§v21 шаг 4: пустой ход сети запрещён (шаг туда и обратно и манёвры — не трата); тратить нечего — ход завершает движок, шаг не в выборке; ход всегда кончается",
          closed && backClosed && dodgeClosed && unrep && open1 && forced && badEnd == 0 && onlyRec == 0 && done == 20 && forcedEnds > 0,
          fmt("туда-обратно не трата %d, Уклонение не трата %d; в начале хода «конец» закрыт %d, метка пустого конца непредставима %d, после шага открыт %d, без ресурсов только конец %d; 20 боёв: ходов %lld, "
              "завершено движком %lld, записано концов %lld (из них запрещённых %lld), записано «только конец» %lld, упоров в предел %lld",
              backClosed, dodgeClosed, closed, unrep, open1, forced, turns, forcedEnds, recEnd, badEnd, onlyRec, caps));
    }
    // T184: этап 5б — веса PN22 переносятся в PN23 без изменения ходов: на любом входе старые логиты, ценность и оценки
    // указателя на врага совпадают с новыми (новые признаки состояний — с нулевыми весами)
    if (TB()) {
        bool okLoad = false, same = true;
        double maxd = 0;
        pn::PNet nw;
        std::ifstream f0("net_fighter_p.bin", std::ios::binary);
        int hd[6] = {};
        if (f0) f0.read((char*)hd, sizeof hd);
        const bool is22 = hd[0] == pn::P_MAGIC22 || hd[0] == pn::P_MAGIC23 || hd[0] == pn::P_MAGIC24;   // старые веса — перенос
        if (is22 && pn::loadP("net_fighter_p.bin", nw, false)) {
            okLoad = true;
            pn::PNetT<float> old;
            old.D = hd[1];
            old.nf[1] = hd[0] == pn::P_MAGIC22 ? pn::FE : pn::FEP24;
            old.NA = hd[4];   // в старых весах слотов способностей и взаимодействий меньше
            old.init(1);
            f0.read((char*)old.core.p.data(), old.core.p.size() * sizeof(float));
            f0.read((char*)old.ptr.data(), old.ptr.size() * sizeof(float));
            uint64_t sd = 5;
            for (int t = 0; t < 20; t++) {
                std::vector<float> x(pn::D_IN), h(pn::HID), lo(old.NA), ln(pn::NFIX);
                for (auto& v : x) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                for (auto& v : h) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                GruNet<float>::Step so, sn;
                float vo = 0, vn = 0;
                old.core.forward(x.data(), h.data(), so, lo.data(), vo);
                nw.core.forward(x.data(), h.data(), sn, ln.data(), vn);
                const int oldNAB = old.NA - (pn::ST_N + CAT_N + pn::IN_N24 + pn::NSWAP);
                std::vector<char> used(pn::NFIX, 0);
                for (int k = 0; k < old.NA; k++) {
                    const int nr = pn::mapOldRow(k, oldNAB);
                    used[nr] = 1;
                    maxd = std::max(maxd, (double)std::fabs(lo[k] - ln[nr]));
                }
                for (int k = 0; k < pn::NFIX; k++)
                    if (!used[k]) maxd = std::max(maxd, ln[k] < -10 ? 0.0 : 1.0);   // новые слоты и действия закрыты
                maxd = std::max(maxd, (double)std::fabs(vo - vn));
                float pro[pn::KP], prn[pn::KP], fe[pn::FEP];
                for (auto& v : fe) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                old.pre(1, so.h.data(), pro);
                nw.pre(1, sn.h.data(), prn);
                maxd = std::max(maxd, (double)std::fabs(old.score(1, pro, fe) - nw.score(1, prn, fe)));
                float fc[pn::FC];
                for (auto& v : fc) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                old.pre(2, so.h.data(), pro);
                nw.pre(2, sn.h.data(), prn);
                maxd = std::max(maxd, (double)std::fabs(old.score(2, pro, fc) - nw.score(2, prn, fc)));
            }
            same = maxd < 1e-5;
        }
        T("T184", "§v21: старые веса сети-бойца (PN22–PN24) переносятся в PN25 (новые признаки, «выше/ниже») без изменения решений",
          !is22 || (okLoad && same), is22 ? fmt("перенесено %d, наибольшее расхождение логитов/ценности/указателей %.2g", okLoad, maxd) : std::string("веса уже PN25 — перенос не нужен"));
    }
    // T185: этап 5б — своя СЛ способности и состояние без повторного спаса: надбавка к СЛ режет успешные спасы, без
    // повторного спаса состояние держится все раунды; обе покупки дороже обычной способности
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = STR;
        Boss d = plain(200, 0, 0, 5);
        Spec sp;
        sp.delivery = 1;
        sp.saveStat = CON;
        sp.cond = C_POISON;
        sp.condRounds = 5;
        sp.range = 30;
        Spec hi = sp;
        hi.dcB = 8;
        Spec nr = sp;
        nr.noRep = true;
        const int N = 400;
        int fail0 = 0, fail8 = 0, failNR = 0, held0 = 0, heldNR = 0;
        for (int i = 0; i < N; i++) {
            for (int v = 0; v < 3; v++) {
                const Spec& s2 = v == 0 ? sp : (v == 1 ? hi : nr);
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                hitOne(w, s2, w.f[0], w.f[1]);
                const bool got = w.f[1].cond[C_POISON] > 0;
                (v == 0 ? fail0 : (v == 1 ? fail8 : failNR)) += got;
                if (!got || v == 1) continue;
                int turns = 0;
                for (int r = 0; r < 10 && w.f[1].cond[C_POISON] > 0; r++) {
                    endOfTurn(w.f[1]);
                    turns++;
                }
                (v == 0 ? held0 : heldNR) += turns;
            }
        }
        Coef C;
        for (int k = 0; k < AT_N; k++) C[k] = AD[k].init;
        const long long p0 = specPrice(sp, C), p8 = specPrice(hi, C), pn2 = specPrice(nr, C);
        AMask m = 0;
        markSpec(hi, m);
        markSpec(nr, m);
        const bool marked = ((m >> AT_DCB) & ONE) && ((m >> AT_NOREP) & ONE);
        const double avg0 = held0 / (double)std::max(1, fail0), avgNR = heldNR / (double)std::max(1, failNR);
        T("T185", "§v21 этап 5б: надбавка к СЛ способности и состояние без повторного спаса работают и стоят дороже",
          fail8 > fail0 + N / 5 && failNR > 0 && std::fabs(avgNR - 5.0) < 1e-9 && avg0 < 4.0 && p8 > p0 && pn2 > p0 && marked,
          fmt("провалов спаса: СЛ +0 %d, СЛ +8 %d из %d; ходов под ядом за наложение (5 р.): с повторным спасом %.2f, без него %.2f; цена %lld / %lld (СЛ +8) / %lld (без повт.); атомы отмечены %d",
              fail0, fail8, N, avg0, avgNR, p0, p8, pn2, marked));
    }
    // T186: владения (решение владельца 2026-11-02, как в D&D) — общий бонус мастерства идёт к атакам оружием (если
    // владеешь им), атакам и СЛ способностей, владеемым спасброскам и навыкам; экспертиза удваивает его в навыке; цены
    // владений растут с мастерством, «без владения оружием» возвращает очки; генератор и разбор цены сходятся
    if (TB()) {
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = DEX;
        a.stats[DEX] = 18;
        a.wFinesse = true;
        a.prof = 4;
        a.saveProf = 1u << CON;
        a.skillProf = (1u << SK_STEALTH) | (1u << SK_ATHL);
        a.skillExp = 1u << SK_STEALTH;
        Fighter f = mk(a, 0, 10, 10);
        const bool atk = f.wAtk() == 4 + 4 && f.sAtk() == 4 + 4 && f.dc() == 8 + 4 + 4;
        const bool sv = saveBonus(f, CON) == modOf(a.stats[CON]) + 4 && saveBonus(f, STR) == modOf(a.stats[STR]);
        const bool sk = f.skill(SK_STEALTH) == 4 + 8 && f.skill(SK_ATHL) == modOf(a.stats[STR]) + 4 && f.skill(SK_ACRO) == 4;
        Boss nb = a;
        nb.wNoProf = true;
        Fighter g = mk(nb, 0, 10, 10);
        const bool noW = g.wAtk() == 4 && g.sAtk() == 8;
        Coef C;
        for (int k = 0; k < AT_N; k++) C[k] = AD[k].init;
        auto sumItems = [&](const Boss& b, double B) {
            long long t = 0;
            for (auto& it : bossItems(b, C, B)) t += it.cost;
            return t;
        };
        Boss p2 = a, p6 = a;
        p2.prof = 2;
        p6.prof = 6;
        const bool scale = sumItems(p6, 1600) > sumItems(p2, 1600) && sumItems(nb, 1600) < sumItems(a, 1600);
        int bad = 0, withProf = 0, withSkill = 0, noProf = 0;
        for (int i = 0; i < 300; i++) {
            rng.seed(9100 + i);
            const long long B = 200LL << (i % 6);
            Boss b = makeBoss(B, C, false);
            withProf += b.prof > 2;
            withSkill += b.skillProf != 0;
            noProf += b.wNoProf;
            if (b.prof < 2 || b.prof > 6 || (b.skillExp & ~b.skillProf)) bad++;
            if (sumItems(b, (double)B) + STAT_BASE != b.cost) bad++;   // разбор цены сходится с генератором
        }
        T("T186", "§v21 владения: мастерство к атакам, СЛ, спасам и навыкам; экспертиза ×2; без владения оружием — без мастерства к атакам и с возвратом; цена растёт с мастерством",
          atk && sv && sk && noW && scale && bad == 0 && withProf > 0 && withSkill > 0 && noProf > 0,
          fmt("атака/способность/СЛ %d, спасы %d, навыки %d, без владения оружием %d, цена растёт %d; 300 боссов генератора: с мастерством > 2 — %d, с навыками %d, без владения оружием %d, расхождений %d",
              atk, sv, sk, noW, scale, withProf, withSkill, noProf, bad));
    }
    // T187: «когда → что» (решение владельца 2026-11-02) — условие и эффект работают по отдельности: союзник рядом с
    // противником → преимущество; безрассудство — преимущество ближним атакам и атаки по мне с преимуществом (возврат
    // очков); +КД и помеха по мне против атакующего; резист и спасброски только с условием о себе (иначе отказ)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5), d = plain(100, 1, 6, 5), al = plain(100, 1, 6, 5);
        a.rules.push_back(Rule{W_ALLYADJ, 0, E_ADVATK});
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        bool adv0, dis0, adv1, dis1;
        advState(w, w.f[0], w.f[1], 5, adv0, dis0);
        w.f.push_back(mk(al, 0, 12, 10));   // союзник рядом с противником
        advState(w, w.f[0], w.f[1], 5, adv1, dis1);
        const bool pack = !adv0 && adv1;
        Boss rk = plain(100, 1, 6, 5);
        rk.rules.push_back(Rule{W_RECKLESS, 0, E_ADVATK});
        rk.rules.push_back(Rule{W_RECKLESS, 0, E_ADVVSME});
        rk.rules.push_back(Rule{W_ALWAYS, 0, E_AC, 3});
        rk.rules.push_back(Rule{W_HPLOW, 0, E_DISVSME});
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        v.f.push_back(mk(rk, 0, 10, 10));
        v.f.push_back(mk(d, 1, 11, 10));
        bool r0, q0, r1, q1, r2, q2, r3, q3;
        advState(v, v.f[0], v.f[1], 5, r0, q0);   // не рискнул — без преимущества
        v.f[0].wasReckless = true;
        advState(v, v.f[0], v.f[1], 5, r1, q1);    // ближняя — с преимуществом
        advState(v, v.f[0], v.f[1], 30, r2, q2);   // дальняя — без
        advState(v, v.f[1], v.f[0], 5, r3, q3);    // по безрассудному — с преимуществом
        const bool reck = !r0 && r1 && !r2 && r3 && !q3;
        v.f[0].hp = 10;   // ХП ≤ 50% → по мне с помехой
        bool r4, q4;
        advState(v, v.f[1], v.f[0], 5, r4, q4);
        const bool ac = ruleAC(v, v.f[0], v.f[1]) == 3 && q4;
        Coef C;
        for (int k = 0; k < AT_N; k++) C[k] = AD[k].init;
        const bool refund = rulePrice(Rule{W_RECKLESS, 0, E_ADVVSME}, rk, C) < 0 && rulePrice(Rule{W_ALLYADJ, 0, E_ADVATK}, a, C) < rulePrice(Rule{W_ALWAYS, 0, E_ADVATK}, a, C);
        // резист по условию о себе: полурон при ХП > 50%, полный при ≤ 50%
        Boss rs = plain(1000, 0, 0, 5);
        rs.rules.push_back(Rule{W_HPHIGH, 0, E_RES});
        Fighter f1 = mk(rs, 0, 1, 1), f2 = mk(rs, 0, 1, 1);
        f2.hp = 400;
        const int t1 = hurt(f1, 100, DT_FIRE, true), t2 = hurt(f2, 100, DT_FIRE, true);
        const bool res = t1 == 50 && t2 == 100;
        Boss sv = plain(100, 0, 0, 5);
        sv.rules.push_back(Rule{W_ALWAYS, 0, E_ADVSAVE});
        const bool save = advSave(mk(sv, 0, 1, 1)) && !advSave(mk(d, 0, 1, 1));
        const bool allowed = !ruleAllowed(W_ALLYADJ, E_RES) && !ruleAllowed(W_FOECOND, E_ADVSAVE) && ruleAllowed(W_FOECOND, E_DMG) && ruleAllowed(W_HPLOW, E_RESNM);
        T("T187", "§v21 «когда → что»: условие и эффект по отдельности (стая, безрассудство, +КД, помеха по мне, резист и спасброски по условию о себе); цена = эффект × доля времени",
          pack && reck && ac && refund && res && save && allowed,
          fmt("стая %d, безрассудство %d, КД и помеха %d, возврат и доля времени %d, резист по условию %d (урон %d/%d), спасброски %d, запрет сочетаний %d",
              pack, reck, ac, refund, res, t1, t2, save, allowed));
    }
    // T188: оружие и Зуро — атака второй рукой (после атаки оружием, без модификатора без стиля), приём «при попадании»
    // (кость запаса тратится только при попадании; обезоруживание: цель тратит взаимодействие), урон себе и лечение, если
    // цель умерла; дальняя дистанция с помехой; лечение с гранью и прибавкой; Зуро собирается из описания целиком
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss z = plain(100, 1, 6, 5);
        z.mainStat = DEX;
        z.stats[DEX] = 18;
        z.wFinesse = z.wLight = true;
        z.poolN = 4;
        z.poolS = 8;
        Spec off;
        off.offhand = off.bonus = true;
        off.range = 5;
        off.dmgN = 1;
        off.dmgS = 6;
        Spec man;
        man.onHit = man.pool = man.disarm = true;
        man.range = 5;
        man.poolN = 4;
        man.poolS = 8;
        Spec smite;
        smite.onHit = true;
        smite.range = 5;
        smite.dmgN = 2;
        smite.dmgS = 10;
        smite.selfN = 2;
        smite.selfS = 4;
        smite.killN = 1;
        smite.killS = 6;
        smite.killB = 100;
        z.abil = {off, man, smite};
        Boss d = plain(1000, 1, 6, 5);
        d.ac = 1;
        auto fresh = [&]() {
            w.f.clear();
            w.f.push_back(mk(z, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            w.f[0].poolLeft = 4;
            w.f[0].uses.assign(3, UNL);
        };
        fresh();
        const bool offLocked = !resReady(w.f[0], specAbility(w.f[0].b, 0).cost);
        doWeapon(w, w.f[0], 1);
        const bool offOpen = resReady(w.f[0], specAbility(w.f[0].b, 0).cost);
        // вторая рука: без стиля урон 1к6 без +4 (в среднем ~3.6 с критами), со стилем +4
        auto offMean = [&](bool style) {
            double tot = 0;
            for (int i = 0; i < 400; i++) {
                fresh();
                w.f[0].b.abil[0].twfMod = style;
                w.f[0].wAtkTurn = true;
                const int h0 = w.f[1].hp;
                doSpec(w, w.f[0], 0, 1);
                tot += h0 - w.f[1].hp;
            }
            return tot / 400;
        };
        const double offNo = offMean(false), offSt = offMean(true);
        const int maxOff = offNo < 4.5 && offSt - offNo > 3.0 ? 1 : 0;
        // приём: попал — кость потрачена и цель обезоружена; промах — кость цела
        fresh();
        doSpec(w, w.f[0], 1, 1);
        const bool spent = w.f[0].poolLeft == 3 && w.f[1].disarmed;
        TurnState ts;
        beginTurn(w, 1, ts);
        const bool pick = !w.f[1].disarmed && ts.swaps == 1;
        fresh();
        w.f[1].b.ac = 1000;   // не попасть (кроме крита)
        int keep = 0;
        for (int i = 0; i < 50; i++) {
            w.f[0].poolLeft = 4;
            doSpec(w, w.f[0], 1, 1);
            keep += w.f[0].poolLeft == 4;
        }
        // урон себе и лечение, если цель умерла
        fresh();
        w.f[1].hp = 1;
        w.f[0].hp = 50;
        doSpec(w, w.f[0], 2, 1);
        const bool kill = w.f[1].hp <= 0 && w.f[0].hp > 50;
        bool selfHurt = false;   // урон себе — при попадании; натуральная 1 промахивается и по КД 1, поэтому до 5 попыток
        for (int t = 0; t < 5 && !selfHurt; t++) {
            fresh();
            const int me0 = w.f[0].hp;
            doSpec(w, w.f[0], 2, 1);
            selfHurt = w.f[0].hp < me0;
        }
        // дальняя дистанция: дальше обычной — помеха
        Boss bow = plain(100, 1, 8, 30);
        bow.wAmmo = true;
        bow.wLong = 120;
        w.f.clear();
        w.f.push_back(mk(bow, 0, 10, 10));
        w.f.push_back(mk(d, 1, 30, 10));   // 100 фт
        bool a1, d1, a2, d2;
        advState(w, w.f[0], w.f[1], 30, a1, d1);
        w.f[1].x = 14;   // 20 фт
        advState(w, w.f[0], w.f[1], 30, a2, d2);
        const bool longR = atkDist(bow) == 120 && d1 && !d2;
        // лечение 1к10 + 5
        Boss hb = plain(100, 0, 0, 5);
        Spec h;
        h.self = h.bonus = true;
        h.healN = 1;
        h.healS = 10;
        h.healB = 5;
        hb.abil = {h};
        int lo = 1000, hi = 0;
        for (int i = 0; i < 300; i++) {
            w.f.clear();
            w.f.push_back(mk(hb, 0, 10, 10));
            w.f[0].uses.assign(1, UNL);
            w.f[0].hp = 1;
            doSpec(w, w.f[0], 0, 0);
            lo = std::min(lo, w.f[0].hp - 1);
            hi = std::max(hi, w.f[0].hp - 1);
        }
        const bool heal = lo == 6 && hi == 15;
        T("T188", "§v21 оружие и Зуро: вторая рука (после атаки оружием, без модификатора без стиля), приём при попадании (кость — только при попадании, обезоруживание), урон себе и лечение при убийстве, дальняя дистанция с помехой, лечение 1к10+5",
          offLocked && offOpen && maxOff && spent && pick && keep >= 45 && kill && selfHurt && longR && heal,
          fmt("вторая рука: закрыта до атаки %d, открыта после %d, урон без стиля %.2f, со стилем %.2f; приём: кость и обезоруживание %d, подобрал оружие взаимодействием %d, промахов без траты кости %d/50; убийство лечит %d, урон себе %d; дальняя дистанция %d; лечение %d…%d",
              offLocked, offOpen, offNo, offSt, spent, pick, keep, kill, selfHurt, longR, lo, hi));
    }
    // T189: доспехи и щит (v21, как в D&D, плюс КД покупается напрямую) — КД = 10 + единицы + ЛОВ по доспеху + щит;
    // средний срезает ЛОВ до +2, тяжёлый не пропускает её (за срезанное — возврат очков); щит +2 и мешает второй руке и
    // двуручному; без владения — помеха к атакам и спасам СИЛ/ЛОВ; универсальное оружие двумя руками бьёт большей костью
    if (TB()) {
        Coef C2 = CC();
        Boss b = plain(100, 1, 8, 5);
        b.stats[DEX] = 18;   // +4
        b.acV = 3;
        int acs[4];
        for (int k = 0; k < 4; k++) {
            b.armor = k;
            acs[k] = calcAC(b);
        }
        const bool formula = acs[0] == 17 && acs[1] == 17 && acs[2] == 15 && acs[3] == 13;
        b.armor = AR_NONE;
        b.shield = true;
        const bool shield = calcAC(b) == 19;
        b.shield = false;
        Boss lo = b;
        lo.stats[DEX] = 6;   // −2: тяжёлый спасает от минуса
        lo.armor = AR_HEAVY;
        const bool heavyLow = calcAC(lo) == 13 && armorPrice(lo, C2) > 0;
        Boss med = b;
        med.armor = AR_MEDIUM;
        const bool refund = armorPrice(med, C2) < 0;
        Boss np = b;
        np.armor = AR_LIGHT;
        np.noArmProf = true;
        Fighter f = mk(np, 0, 1, 1);
        bool adv, dis;
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(f);
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 2, 1));
        advState(w, w.f[0], w.f[1], 5, adv, dis);
        const bool noProf = dis && armorPrice(np, C2) < armorPrice(b, C2);
        Boss vv = plain(100, 1, 8, 5);
        vv.wVers = 10;
        const bool vers = wDie(vv) == 10 && (vv.shield = true, wDie(vv) == 8);
        int bad = 0;
        for (int i = 0; i < 600; i++) {
            rng.seed(77000 + i);
            Boss g = makeBoss(200LL << (i % 6), C2, true);
            if (g.ac != calcAC(g) || (g.shield && (hasOffhand(g) || (g.wN > 0 && g.wTwoH))) || (g.armor == AR_HEAVY && g.stats[STR] < 13)) bad++;
        }
        T("T189", "§v21 доспехи и щит: КД = 10 + единицы + ЛОВ по доспеху + щит; средний и тяжёлый срезают ЛОВ (возврат), щит +2 и занимает руку, без владения — помеха, универсальное двумя руками",
          formula && shield && heavyLow && refund && noProf && vers && bad == 0,
          fmt("КД без/лёгкий/средний/тяжёлый при ЛОВ 18: %d/%d/%d/%d; щит %d; тяжёлый при ЛОВ 6 %d; возврат за средний %d; без владения %d; универсальное %d; генератор нарушений %d",
              acs[0], acs[1], acs[2], acs[3], shield, heavyLow, refund, noProf, vers, bad));
    }
    // T190: формы (v21) — сфера круг, куб квадрат, конус от себя (ширина = расстоянию), линия от себя шириной 5 фт; цена
    // области по числу клеток; несколько целей на выбор бьют лучших врагов в пределах дальности
    if (TB()) {
        const int sph = shapeCells(SH_SPHERE, 20), cub = shapeCells(SH_CUBE, 20), con = shapeCells(SH_CONE, 30), lin = shapeCells(SH_LINE, 30);
        const bool cells = cub == 81 && sph < cub && sph > 40 && lin == 6 && con > lin && con < 40;
        // конус на восток от (10,10): (13,10) и (13,11) внутри, (10,13) — сбоку — нет, (8,10) — сзади — нет
        const bool cone = inShape(SH_CONE, 30, 10, 10, 16, 10, 13, 10) && inShape(SH_CONE, 30, 10, 10, 16, 10, 13, 11) &&
                          !inShape(SH_CONE, 30, 10, 10, 16, 10, 10, 13) && !inShape(SH_CONE, 30, 10, 10, 16, 10, 8, 10);
        const bool line = inShape(SH_LINE, 30, 10, 10, 16, 10, 15, 10) && !inShape(SH_LINE, 30, 10, 10, 16, 10, 13, 11) && inShape(SH_LINE, 30, 10, 10, 13, 13, 12, 12);
        Coef C2 = CC();
        Spec a;
        a.delivery = 1;
        a.range = 60;
        a.dmgN = 4;
        a.radius = 20;
        Spec cu = a;
        cu.shape = SH_CUBE;
        Spec one = a;
        one.radius = 0;
        Spec three = one;
        three.targets = 3;
        // куб (81 клетка) дороже сферы (69) по надбавке за площадь; в целых очках на малом бюджете разница может округлиться
        const bool price = areaTier(cu) > areaTier(a) && specPrice(cu, C2) >= specPrice(a, C2) && specPrice(three, C2) > specPrice(one, C2);
        // несколько целей: три врага рядом — бьёт троих
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss caster = plain(100, 0, 0, 5);
        three.dmgS = 6;
        caster.abil = {three};
        w.f.push_back(mk(caster, 0, 10, 10));
        for (int k = 0; k < 4; k++) w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14 + k, 10));
        w.f[0].uses.assign(1, UNL);
        doSpec(w, w.f[0], 0, 1);
        int hit = 0;
        for (int k = 1; k <= 4; k++) hit += w.f[k].hp < 1000;
        T("T190", "§v21 формы: сфера — круг, куб — квадрат, конус и линия — от себя; цена области по клеткам; несколько целей на выбор",
          cells && cone && line && price && hit == 3,
          fmt("клеток: сфера 20 фт %d, куб %d, конус 30 фт %d, линия 30 фт %d; конус %d, линия %d; цены %d; задето целей %d из 4", sph, cub, con, lin, cone, line, price, hit));
    }
    // T191: союзники (v21) — лечение на раненого союзника, правило-аура на своих рядом, Помощь (преимущество на одну атаку
    // союзника), Подготовка (удар реакцией, когда враг входит в досягаемость), приказ союзнику атаковать, реакция за
    // союзника (помеха атаке по нему)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss me = plain(100, 1, 6, 5), al = plain(100, 1, 6, 5), en = plain(1000, 1, 6, 5);
        Spec hl;
        hl.self = hl.ally = true;
        hl.range = 30;
        hl.healN = 0;
        hl.healB = 40;
        Spec cmd;
        cmd.self = cmd.ally = cmd.cmdAtk = true;
        cmd.range = 30;
        Spec grd;
        grd.self = true;
        grd.react = 5;
        grd.protect = true;
        grd.range = 5;
        me.abil = {hl, cmd, grd};
        Rule aura;
        aura.what = E_SAVEB;
        aura.n = 3;
        aura.allyR = 10;
        me.rules = {aura};
        w.f.push_back(mk(me, 0, 10, 10));
        w.f.push_back(mk(en, 1, 12, 11));
        w.f.push_back(mk(al, 0, 11, 11));   // союзник рядом с врагом
        w.f[2].hp = 30;
        g_curW = &w;
        // лечение — раненому союзнику
        doSpec(w, w.f[0], 0, -1);
        const bool heal = w.f[2].hp == 70 && w.f[0].hp == 100;
        // аура: +3 к спасброскам у союзника в 10 фт, у врага — нет
        const bool auraOk = saveBonus(w.f[2], DEX) == modOf(al.stats[DEX]) + 3 && saveBonus(w.f[1], DEX) == modOf(en.stats[DEX]);
        // Помощь: враг вплотную ко мне — первая атака союзника по нему с преимуществом, вторая — нет
        bool a1, d1, a2, d2;
        w.f[1].x = 11;
        w.f[1].y = 10;
        TurnState ts;
        ts.speed = 30;
        const bool helpOk = applyPrim(w, 0, ts, Prim{PR_HELP, 1}, true, nullptr);
        advState(w, w.f[2], w.f[1], 5, a1, d1);
        bool crit;
        attackRoll(w, w.f[2], w.f[1], 0, 5, crit);
        advState(w, w.f[2], w.f[1], 5, a2, d2);
        const bool help = helpOk && a1 && !a2;
        // приказ: союзник бьёт реакцией
        w.f[2].reactionUsed = false;
        const int h0 = w.f[1].hp;
        int tries = 0;
        while (w.f[1].hp == h0 && tries++ < 50) {
            w.f[2].reactionUsed = false;
            doSpec(w, w.f[0], 1, -1);
        }
        const bool command = w.f[1].hp < h0 && w.f[2].reactionUsed;
        // реакция за союзника: враг бьёт союзника рядом со мной — помеха, моя реакция потрачена
        w.f[0].reactionUsed = false;
        attackRoll(w, w.f[1], w.f[2], 0, 5, crit);
        const bool guard = w.f[0].reactionUsed;
        // Подготовка: враг входит в досягаемость — удар реакцией
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        v.aw = v.ah = W;
        Boss rd = plain(100, 3, 6, 5);
        v.f.push_back(mk(rd, 0, 10, 10));
        v.f.push_back(mk(plain(1000, 1, 6, 5), 1, 20, 10));
        v.f[1].b.ac = 1;
        v.f[0].readied = true;
        g_curW = &v;
        const int vh = v.f[1].hp;
        moveToCell(v, v.f[1], 11, 10, 60);
        const bool ready = v.f[0].reactionUsed && !v.f[0].readied && v.f[1].hp < vh;
        g_curW = nullptr;
        T("T191", "§v21 союзники: лечение союзнику, аура на своих, Помощь на одну атаку, приказ атаковать, реакция за союзника, Подготовка",
          heal && auraOk && help && command && guard && ready,
          fmt("лечение %d, аура %d, Помощь %d (преимущество %d, потом %d), приказ %d, защита союзника %d, Подготовка %d", heal, auraOk, help, a1, a2, command, guard, ready));
    }
