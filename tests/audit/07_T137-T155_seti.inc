// Проверки движка, часть 7 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== Этап 4: политика Сборщика =====
    // T133–T136 (линейная политика Сборщика) удалены вместе с ней 8 ноября

    // ===== Этап 6: сеть-боец =====
    // T137 градиенты GRU: ручной обратный проход через время = численное дифференцирование
    if (TB()) {
        GruNet<double> net;
        net.init(5, 4, 3, 4, 2026);
        for (auto& v : net.p) v += 0.15 * std::sin(1.7 * (&v - &net.p[0]));   // чтобы вентили не были симметричны
        const int T_ = 5;
        std::vector<std::vector<double>> xs(T_, std::vector<double>(5)), cl(T_, std::vector<double>(4));
        std::vector<double> cv(T_);
        uint64_t sd = 77;
        for (int t2 = 0; t2 < T_; t2++) {
            for (auto& x : xs[t2]) x = GruNet<double>::rnd01(sd) * 2 - 1;
            for (auto& c : cl[t2]) c = GruNet<double>::rnd01(sd) * 2 - 1;
            cv[t2] = GruNet<double>::rnd01(sd) * 2 - 1;
        }
        auto lossOf = [&](const GruNet<double>& n) {
            std::vector<double> h(3, 0.0);
            double L = 0;
            for (int t2 = 0; t2 < T_; t2++) {
                GruNet<double>::Step st;
                double lg[4], v;
                n.forward(xs[t2].data(), h.data(), st, lg, v);
                for (int k = 0; k < 4; k++) L += cl[t2][k] * lg[k];
                L += cv[t2] * v * v;   // нелинейная ценность — проверяем и вклад по ценности
                h = st.h;
            }
            return L;
        };
        // аналитический градиент
        std::vector<GruNet<double>::Step> steps(T_);
        std::vector<double> h(3, 0.0), vals(T_);
        for (int t2 = 0; t2 < T_; t2++) {
            double lg[4];
            net.forward(xs[t2].data(), h.data(), steps[t2], lg, vals[t2]);
            h = steps[t2].h;
        }
        std::vector<double> g(net.size(), 0.0), dhn(3, 0.0), dhp(3);
        for (int t2 = T_ - 1; t2 >= 0; t2--) {
            net.backward(steps[t2], cl[t2].data(), 2 * cv[t2] * vals[t2], dhn.data(), g, dhp.data());
            dhn = dhp;
        }
        double worst = 0;
        int checked = 0;
        for (size_t i = 0; i < net.size(); i += 3) {
            GruNet<double> a = net, b = net;
            const double eps = 1e-6;
            a.p[i] += eps;
            b.p[i] -= eps;
            double num = (lossOf(a) - lossOf(b)) / (2 * eps);
            double err = std::fabs(num - g[i]) / std::max(1e-6, std::fabs(num) + std::fabs(g[i]));
            worst = std::max(worst, err);
            checked++;
        }
        T("T137", "§этап 6 сеть-боец: градиенты GRU (ручной обратный проход по времени) совпадают с численными", worst < 1e-5,
          fmt("проверено параметров %d из %zu, наибольшая относительная ошибка %.2e", checked, net.size(), worst));
    }

    // T138–T139: сеть-боец в бою (бойцы с ИНТ >= 12 ходят по сети; 12–15 — в 25% ходов обычным ботом)
    if (TB()) {
        Coef C2 = CC();
        const bool haveNet = anyFighterNet();   // v21: старая сеть отключена, новая (prim_net) обучается на этапе 3
        auto wr = [&](int intv, int N, unsigned sd) {
            std::vector<int> win(N, 0);
            parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно
                rng.seed(sd + i);
                long long B = 200LL << (i % 6);
                Boss A, Bo;
                makePair(B, C2, A, Bo);
                for (Boss* b : {&A, &Bo}) {
                    b->stats[WIS] = 10; b->stats[CHA] = 10; b->stats[INT] = 10;
                    for (auto& m : b->tmpl) { m.stats[WIS] = 10; m.stats[CHA] = 10; m.stats[INT] = 10; }
                }
                for (int side = 0; side < 2; side++) {   // зеркально (v21): ИНТ проверяемого — то за A, то за B
                    Boss a2 = A, b2 = Bo;
                    (side ? b2 : a2).stats[INT] = intv;
                    rng.seed(sd + i + 7777777ULL * (side + 1));
                    const int r = fight(a2, b2);
                    win[i] += (r == side) ? 2 : (r == 2 ? 1 : 0);   // ничья — пол-очка
                }
            });
            int wins = 0;
            for (int x : win) wins += x;
            return 100.0 * wins / (4.0 * N);
        };
        const int N = 6000;
        double w10 = wr(10, N, 51000000), w13 = wr(13, N, 52000000), w16 = wr(16, N, 53000000);
        double sig = 100 * std::sqrt(0.25 / N);
        T("T138", "§этап 6 сеть-боец: при загруженных весах ИНТ 16 заметно сильнее ИНТ 10 (>= +5 п.п.), контроль ИНТ 10 против ИНТ 10 около 50%",
          !haveNet ? std::fabs(w10 - 50) < 3 * sig : (w16 > 55.0 && std::fabs(w10 - 50) < 3 * sig),
          fmt("сеть загружена: %s%s; винрейт стороны: ИНТ 10 (контроль) %.1f%%, ИНТ 16 %.1f%% (σ≈%.1f)", haveNet ? "да" : "нет",
              haveNet ? "" : " (без весов проверяется только контроль: сеть обучается на этапе 3)", w10, w16, sig));
        T("T139", "§этап 6 ступени ИНТ: ИНТ 12–15 (сеть с забывчивостью) не слабее ИНТ 10 и не сильнее ИНТ 16", w13 > 50 - 2 * sig && w13 < w16 + 2 * sig,
          fmt("винрейт стороны: ИНТ 10 %.1f%%, ИНТ 13 %.1f%%, ИНТ 16 %.1f%%", w10, w13, w16));
    }
    // T140–T146: погоня (DMG): провоцированные атаки, рывки и истощение, осложнения, отрыв, побег = поражение
    if (TB()) {
        auto clearW = [](World& w) {
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            std::memset(w.dark, 0, sizeof w.dark);
            w.aw = w.ah = 100;
            w.theme = 0;
        };
        {   // T140: в погоне между главными боссами нет провоцированных атак, вне погони — есть
            int dmgChase = 0, dmgPlain = 0;
            for (int mode = 0; mode < 2; mode++)
                for (int i = 0; i < 300; i++) {
                    World w;
                    clearW(w);
                    w.chaseOn = (mode == 0);
                    Boss big = plain(1000, 0, 0, 5), g = plain(10, 1, 1, 5);
                    g.wFinesse = false;
                    w.f.push_back(mk(big, 0, 51, 50));
                    w.f.push_back(mk(g, 1, 50, 50));
                    w.f.push_back(mk(plain(10, 1, 6, 5), 1, 80, 50));
                    const int hp0 = w.f[0].hp;
                    moveToward(w, w.f[0], w.f[2], 0, 30, false);
                    (mode == 0 ? dmgChase : dmgPlain) += hp0 - w.f[0].hp;
                }
            T("T140", "§7 погоня: провоцированных атак между участниками нет (вне погони есть)", dmgChase == 0 && dmgPlain > 200,
              fmt("урон в погоне %d, вне погони %d", dmgChase, dmgPlain));
        }
        {   // T141: рывки сверх 3 + мод. ТЕЛ -> проверка ТЕЛ СЛ 10 или истощение; в пределах нормы истощения нет
            int exhOver = 0, exhOk = 0, N = 400;
            for (int i = 0; i < N; i++) {
                for (int lim = 0; lim < 2; lim++) {
                    World w;
                    clearW(w);
                    Boss b = plain(100, 1, 6, 5);
                    b.stats[CON] = 1;   // мод -5: проверка СЛ 10 проваливается при 1–14 на кубике (70%)
                    w.f.push_back(mk(b, 0, 40, 50));
                    w.f.push_back(mk(plain(100, 1, 6, 5), 1, 60, 50));
                    w.chaseOn = true;
                    w.chasePrey = 0;
                    w.f[0].dashLimit = lim == 0 ? 0 : 99;
                    w.f[0].dashedNow = true;
                    chaseAfterTurn(w, 0, 5);
                    (lim == 0 ? exhOver : exhOk) += w.f[0].exh;
                }
            }
            T("T141", "§7 погоня: рывок сверх 3 + мод. ТЕЛ — проверка ТЕЛ СЛ 10, провал даёт истощение; в пределах нормы — нет", exhOver > N * 0.6 && exhOk == 0,
              fmt("истощений при превышении %d из %d, в норме %d", exhOver, N, exhOk));
        }
        {   // T142: уровни истощения: 2 — скорость вдвое, 4 — максимум ХП вдвое, 5 — скорость 0
            World w;
            clearW(w);
            Boss b = plain(100, 1, 6, 5);
            b.stats[CON] = 1;
            w.f.push_back(mk(b, 0, 40, 50));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 60, 50));
            Fighter& f = w.f[0];
            f.exh = 2;
            const int s2 = speedOf(f);
            f.exh = 5;
            const int s5 = speedOf(f);
            f.exh = 3;
            f.dashLimit = 0;
            w.chaseOn = true;
            w.chasePrey = 0;
            int tries = 0;
            while (f.exh < 4 && tries++ < 200) {
                f.dashedNow = true;
                chaseAfterTurn(w, 0, 5);
            }
            T("T142", "§7 истощение: 2 — скорость 15; 4 — максимум ХП вдвое; 5 — скорость 0", s2 == 15 && s5 == 0 && f.exh >= 4 && f.maxHp == 50 && f.hp <= 50,
              fmt("скорость при 2: %d, при 5: %d, уровень %d, макс. ХП %d", s2, s5, f.exh, f.maxHp));
        }
        {   // T143: осложнения: слабый (ЛОВ/СИЛ 1) почти всегда страдает: опутан (дикая местность 7) и сбит с ног (город 3)
            int restr = 0, prone = 0, N = 300;
            for (int i = 0; i < N; i++) {
                World w;
                clearW(w);
                Boss b = plain(1000, 1, 6, 5);
                b.stats[DEX] = 1; b.stats[STR] = 1;
                w.f.push_back(mk(b, 0, 40, 50));
                w.f.push_back(mk(plain(100, 1, 6, 5), 1, 60, 50));
                w.theme = 0;
                applyComplication(w, w.f[0], 7);
                restr += w.f[0].cond[C_RESTR] > 0;
                w.theme = 1;
                applyComplication(w, w.f[0], 3);
                prone += w.f[0].cond[C_PRONE] > 0;
            }
            T("T143", "§7 осложнения погони: силок опутывает, споткнувшийся падает", restr > N * 0.85 && prone > N * 0.6, fmt("опутан %d/%d, сбит %d/%d", restr, N, prone, N));
        }
        {   // T144: отрыв: если преследуемый не терял преследователя из виду — проверка проваливается сама; потерял — ЛОВ против МДР
            int seenEsc = 0, hiddenEsc = 0, N = 300;
            for (int i = 0; i < N; i++) {
                for (int seen = 0; seen < 2; seen++) {
                    World w;
                    clearW(w);
                    Boss a = plain(100, 1, 6, 5), b = plain(100, 1, 6, 5);
                    a.stats[DEX] = 30; b.stats[WIS] = 1;
                    w.f.push_back(mk(a, 0, 40, 50));
                    w.f.push_back(mk(b, 1, 70, 50));
                    for (int y = 40; y < 60; y++) w.dark[y][38] = true;   // много тьмы рядом — преимущество
                    w.chaseOn = true;
                    w.chasePrey = 0;
                    w.chaseSeen = seen == 1;
                    chaseEndOfRound(w);
                    (seen ? seenEsc : hiddenEsc) += w.f[0].fled;
                }
            }
            T("T144", "§7 погоня: оторваться можно только потеряв преследователя из виду (проверка ЛОВ против МДР)", seenEsc == 0 && hiddenEsc > N * 0.95,
              fmt("побегов без потери из виду %d, с потерей %d из %d", seenEsc, hiddenEsc, N));
        }
        {   // T145: побег = поражение; fled заканчивает бой
            World w;
            clearW(w);
            w.f.push_back(mk(plain(100, 1, 6, 5), 0, 40, 50));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 45, 50));
            const bool before = over(w);
            w.f[0].fled = true;
            T("T145", "§7 вышедший за арену или оторвавшийся (fled) заканчивает бой", !before && over(w), "");
        }
        {   // T147: погоня начинается только при отходе из-под угрозы: на 200 фт обычное перемещение погоней не считается, в пределах рывка врага — считается
            int farChase = 0, nearChase = 0, N = 100;
            for (int i = 0; i < N; i++)
                for (int near = 0; near < 2; near++) {
                    World w;
                    clearW(w);
                    w.f.push_back(mk(plain(100, 1, 6, 5), 0, 40, 50));
                    w.f.push_back(mk(plain(100, 1, 6, 5), 1, near ? 47 : 80, 50));
                    const int dist0 = distFt(w.f[0], w.f[1]);
                    w.f[0].x -= 6;   // отошёл на 30 фт
                    chaseAfterTurn(w, 0, dist0);
                    (near ? nearChase : farChase) += w.chaseOn;
                }
            T("T147", "§7 погоня: начинается при отходе на 10+ фт из-под угрозы врага (досягаемость + рывок); издалека — нет", farChase == 0 && nearChase == N,
              fmt("погонь издалека %d, вблизи %d из %d", farChase, nearChase, N));
        }
        {   // T146: в настоящих боях убегающие проигрывают: бой в тесной арене 5×5 без выхода из лимита, погони и побеги случаются, зависших нет
            g_style.fixedW = g_style.fixedH = 5;
            long long c0 = g_chases, e0 = g_escapes, cap0 = g_capped;
            Coef C2 = CC();
            const int N = 600;
            int draws = 0;
            for (int i = 0; i < N; i++) {
                rng.seed(77000 + i);
                Boss A, Bo;
                makePair(400LL << (i % 4), C2, A, Bo);
                draws += fight(A, Bo) == 2;
            }
            g_style = MapStyle();
            T("T146", "§7 арена 5×5: бои заканчиваются (ничьих не более 3%), погонь нет или немного",
              draws <= N * 0.03 && g_capped - cap0 <= N * 0.03, fmt("ничьих %d из %d, по лимиту %lld, погонь %lld, побегов %lld", draws, N, g_capped - cap0, g_chases - c0, g_escapes - e0));
        }
    }
    // T148: разбор цены по покупкам для просмотрщика совпадает с независимым пересчётом цены босса
    if (TB()) {
        Coef C2 = CC();
        int bad = 0, N = 600;
        long long worst = 0;
        for (int i = 0; i < N; i++) {
            rng.seed(88000 + i);
            const long long B = 200LL << (i % 6);
            Boss b = makeBoss(B, C2, true, i % 3 == 0);
            long long sum = 0;
            for (auto& it : bossItems(b, C2, (double)B)) sum += it.cost;
            const long long ref = priceOf(b, C2, (double)(B + STAT_BASE));
            if (sum + STAT_BASE != ref) { bad++; worst = std::max(worst, std::llabs(sum + STAT_BASE - ref)); }
        }
        T("T148", "§просмотрщик: сумма покупок босса равна его цене по таблице (независимый пересчёт)", bad == 0, fmt("расхождений %d из %d (наибольшее %lld)", bad, N, worst));
    }
    // T149: цена способности и оружия — СУММА процентов (а не произведение), скидки ограничены снизу MULT_FLOOR
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.radius = 15;
        s.shape = SH_CUBE;   // v21: надбавка за куб — прежняя лестница по радиусу
        s.dmgN = 4;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        const double expectRange = C2[AT_ARANGE] * tierN(TR_RANGE, rangeIdx(s.range)), expectArea = C2[AT_AREA] * tierN(TR_AREA, s.radius / 5.0);
        const double m0 = specMult(s, C2);
        bool sum1 = std::fabs(m0 - (1 + expectRange + expectArea)) < 1e-9;
        Spec lim = s;
        lim.uses = 1;
        lim.recharge = 0;
        Spec both = lim;
        both.conc = true;
        both.bonus = true;
        // после теста 4 ноября: скидка концентрации — доля длительности, потерянная при срыве; пол ограничивает только скидки
        const double dU = (1 - C2[AT_ULIM]) * 1.0, dC = std::max(0.0, 1 - C2[AT_CONC] * concKeep(both)), pB = C2[AT_BONUS] - 1;
        const double want = 1 + expectRange + expectArea - std::min(dU + dC, 1 - MULT_FLOOR) + pB;
        bool sum2 = std::fabs(specMult(both, C2) - want) < 1e-9;
        Spec cheap = s;
        cheap.uses = 1;
        cheap.conc = true;
        cheap.slotLvl = 1;
        cheap.recharge = 0;
        const double floorWant = 1 + expectRange + expectArea - (1 - MULT_FLOOR);   // скидки упёрлись в пол, надбавки остались
        bool floorOk = std::fabs(specMult(cheap, C2) - floorWant) < 1e-9;
        Wp wp;
        wp.n = 2; wp.s = 8; wp.range = 30; wp.att = 3; wp.reach = 5;
        const double wm = 1 + C2[AT_WPNRANGE] * tierN(TR_RANGE, rangeIdx(wp.range));   // число атак — отдельно (wpHits)
        bool wpOk = std::fabs(wpMult(C2, wp.range, wp.reach, wp.att) - wm) < 1e-9;
        T("T149", "§цены: множитель способности и оружия — сумма процентов (1 + надбавки − скидки), скидки в сумме не больше 80%, надбавки всегда", sum1 && sum2 && floorOk && wpOk,
          fmt("сумма-1 %d, сумма-2 %d, пол %d (множитель %.3f), оружие %d", sum1, sum2, floorOk, specMult(cheap, C2), wpOk));
    }
    // T150–T152: персонаж игрока: проверка правил, цена, ход по командам
    if (TB()) {
        Coef C2 = CC();
        auto parse = [&](const char* t) { bool ok = true; Json j = parseJson(t, &ok); return j; };
        const char* good = "{\"name\":\"Т\",\"budget\":800,\"stats\":[16,10,14,8,10,8],\"acV\":8,\"weapon\":{\"n\":1,\"s\":8,\"att\":2,\"type\":11},"
                           "\"abilities\":[{\"delivery\":1,\"saveStat\":1,\"range\":5,\"radius\":10,\"dmgN\":2,\"dmgS\":6,\"dtype\":1,\"cond\":1,\"recharge\":5}],\"passives\":{\"saveProf\":[2]}}";
        Boss b;
        long long bud = 0, spent = 0, left = 0;
        std::string err;
        const bool okGood = chr::buildCharacter(parse(good), C2, b, bud, err, &spent, &left);
        long long sum = 0;
        for (auto& it : bossItems(b, C2, (double)bud)) sum += it.cost;
        T("T150", "§персонаж: описание в JSON собирается в босса, ХП = остаток бюджета (2 очка за ХП), сумма покупок = потрачено, не больше бюджета", okGood && sum == spent && spent <= bud && left >= 0 && left < 2 && b.hp > 1,
          fmt("ok %d, потрачено %lld из %lld, остаток %lld, ХП %d, сумма покупок %lld", okGood, spent, bud, left, b.hp, sum));
        const char* bad[] = {
            "{\"budget\":800,\"stats\":[41,10,10,10,10,10]}",
            "{\"budget\":100,\"stats\":[30,30,30,30,30,30]}",
            "{\"budget\":800,\"abilities\":[{\"self\":false,\"delivery\":0}]}",
            "{\"budget\":800,\"abilities\":[{\"self\":true,\"delivery\":0}]}",
            "{\"budget\":800,\"abilities\":[{\"delivery\":0,\"dmgN\":1,\"recharge\":5,\"uses\":2}]}",
            "{\"budget\":800,\"hp\":100000}",
            "{\"budget\":800,\"passives\":{\"rider\":{\"n\":2}},\"weapon\":{\"n\":0}}",
            "{\"v\":21,\"budget\":5000,\"acV\":31}",
            "{\"budget\":800,\"weapon\":{\"n\":1,\"s\":7}}",
            "{\"budget\":800,\"passives\":{\"speed\":33}}",
            "{\"budget\":100001}",
        };
        int rejected = 0;
        for (const char* t : bad) {
            Boss bb;
            long long bu = 0;
            std::string e2;
            if (!chr::buildCharacter(parse(t), C2, bb, bu, e2)) rejected++;
        }
        T("T151", "§персонаж: нарушения правил отклоняются (характеристика 41, КД 41, кубик к7, скорость не кратна 5, бюджет выше предела, бюджет не покрывает ХП, способность без эффекта, перезарядка вместе с лимитом, ХП дороже остатка, довесок без оружия)",
          rejected == (int)(sizeof bad / sizeof bad[0]), fmt("отклонено %d из %zu", rejected, sizeof bad / sizeof bad[0]));
        // ход игрока: команды идут через g_human.ask; игрок подходит, бьёт и заканчивает ход, бой завершается без зависаний
        int finished = 0, moved = 0, attacked = 0, N = 40, askedTotal = 0;
        for (int i = 0; i < N; i++) {
            rng.seed(99000 + i);
            Boss me, opp;
            long long bu = 0;
            std::string e3;
            chr::buildCharacter(parse(good), C2, me, bu, e3);
            opp = makeBoss(800, C2, true, false);
            g_human.idx = 0;
            int asked = 0;
            g_human.ask = [&](const std::string& req) -> std::string {
                asked++;
                bool ok = true;
                Json q = parseJson(req, &ok);
                if (!ok || asked > 400) return "{\"c\":\"end\"}";
                const Json& en = q["enemies"][0];
                if (en.t != Json::OBJ) return "{\"c\":\"end\"}";
                const int d = en["dist"].num(), tgt = en["i"].num();
                if (q["actionFree"].flag() && en["los"].flag() && d <= q["weapon"]["range"].num()) { attacked++; return "{\"c\":\"attack\",\"tgt\":" + std::to_string(tgt) + "}"; }
                if (q["speedLeft"].num() >= 5 && d > q["weapon"]["range"].num()) { moved++; return "{\"c\":\"move\",\"x\":0,\"y\":0}"; }
                return "{\"c\":\"end\"}";
            };
            std::vector<std::string> frames;
            const int r = fight(me, opp, &frames);
            g_human.idx = -1;
            g_human.ask = nullptr;
            finished += (r >= 0 && r <= 2);
            askedTotal += asked;
        }
        T("T152", "§игра: ход игрока по командам (перейти, атаковать, закончить) — бои заканчиваются, игрок действительно ходит", finished == N && moved > 0 && attacked > 0,
          fmt("боёв закончено %d из %d, команд движения %d, атак %d, запросов хода %d", finished, N, moved, attacked, askedTotal));
    }
    // T153: характеристики от 10: десять бесплатно, выше платим, ниже возвращаем (V·(V+1)/2 − 55); разбор цены и персонаж это учитывают
    if (TB()) {
        Coef C2 = CC();
        const bool tab = statCost(10) == 0 && statCost(16) == 81 && statCost(8) == -19 && statCost(1) == -54 && statCost(30) == 410 && STAT_BASE == 330;
        auto parse = [&](const char* t) { return parseJson(t); };
        Boss base, dump;
        long long bud = 0, sp0 = 0, sp1 = 0, l0 = 0, l1 = 0;
        std::string err;
        const bool ok0 = chr::buildCharacter(parse("{\"budget\":300,\"stats\":[10,10,10,10,10,10]}"), C2, base, bud, err, &sp0, &l0);
        const bool ok1 = chr::buildCharacter(parse("{\"budget\":300,\"stats\":[18,10,10,10,10,3]}"), C2, dump, bud, err, &sp1, &l1);
        long long sumStats = 0;
        for (auto& it : bossItems(dump, C2, 300.0))
            if (it.cat == "stat") sumStats += it.cost;
        // 18: +116, 3: −49 → +67; всё остальное (ХП) тратит остаток
        const bool items = sumStats == 116 - 49;
        T("T153", "§2 характеристики от 10: десять бесплатно, выше платим, ниже возвращаем; бюджет «от десяток»", tab && ok0 && ok1 && items && base.hp > dump.hp,
          fmt("таблица %d, ХП при всех десятках %d, при 18/3 %d, итог по характеристикам %lld (ожидалось 67)", tab, base.hp, dump.hp, sumStats));
    }
    // T154: барьер — временная стена: непроходима, закрывает обзор, пропускает клетки с существами, через раунды исчезает
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.aw = w.ah = 30;
        w.f.push_back(mk(plain(100, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 14, 10));
        const bool seeBefore = los(w, 10, 10, 18, 10);
        const int placed = placeWall(w, w.f[0], 14, 11, 5, 2);   // клетка (14,10) занята врагом: её пропускаем
        const bool blocked = !los(w, 10, 12, 18, 12) && w.wall[12][14] && !w.wall[10][14];
        expireDyn(w);
        const bool stillThere = w.wall[11][14];
        expireDyn(w);
        expireDyn(w);
        const bool gone = !w.wall[11][14] && w.dyn.empty();
        T("T154", "§способности по клетке: барьер ставится поперёк, не на существ, закрывает обзор и через заданное число раундов исчезает", seeBefore && placed == 4 && blocked && stillThere && gone,
          fmt("поставлено клеток %d (ожидалось 4), обзор закрыт %d, держится после 1 раунда %d, исчез %d", placed, blocked, stillThere, gone));
    }
    // T155: карта игрока: проверка размера, связности и начальных клеток; бой идёт по этой карте и начинается на заданных клетках
    if (TB()) {
        auto mk2 = [&](int aw, int ah, const std::string& wallRow, int wallCol, const char* starts) {
            std::string j = "{\"aw\":" + std::to_string(aw) + ",\"ah\":" + std::to_string(ah) + ",\"wall\":[";
            for (int y = 0; y < ah; y++) {
                std::string row(aw, '0');
                if (wallCol >= 0) row[wallCol] = '1';
                j += (y ? ",\"" : "\"") + row + "\"";
            }
            return j + "]" + starts + "}";
        };
        CustomMap m;
        std::string err;
        const bool good = parseCustomMap(parseJson(mk2(12, 9, "", -1, ",\"starts\":[[1,4],[10,4]]")), m, err);
        const bool small = !parseCustomMap(parseJson(mk2(4, 9, "", -1, "")), m, err);
        const bool split = !parseCustomMap(parseJson(mk2(12, 9, "", 6, "")), m, err);
        const bool badStart = !parseCustomMap(parseJson(mk2(12, 9, "", 6, ",\"starts\":[[6,2],[10,4]]")), m, err);
        const bool oneStart = !parseCustomMap(parseJson(mk2(12, 9, "", -1, ",\"starts\":[[1,4]]")), m, err);
        parseCustomMap(parseJson(mk2(12, 9, "", -1, ",\"starts\":[[1,4],[10,4]]")), g_customMap, err);
        Coef C2 = CC();
        rng.seed(4242);
        Boss a = makeBoss(300, C2, true, false), b = makeBoss(300, C2, true, false);
        std::vector<std::string> frames;
        std::string mapJson;
        const int r = fight(a, b, &frames, &mapJson);
        g_customMap = CustomMap();
        bool startsOk = !frames.empty() && frames[0].find("\"x\":1,\"y\":4,") != std::string::npos && frames[0].find("\"x\":10,\"y\":4,") != std::string::npos;
        const bool sizeOk = mapJson.find("\"aw\":12,\"ah\":9") != std::string::npos;
        T("T155", "§карта игрока: размер 5–100, свободные клетки связаны, начальные клетки свободны и заданы парой; бой идёт на этой карте с этих клеток", good && small && split && badStart && oneStart && startsOk && sizeOk && r >= 0,
          fmt("верная %d, мала %d, разделена %d, старт на стене %d, одна стартовая %d, бой на 12×9 %d, старт на местах %d", good, small, split, badStart, oneStart, sizeOk, startsOk));
    }
