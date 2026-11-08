// Проверки движка, часть 9 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== v21, этап 1: способности =====
    auto bare = [&]() {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.diff, 0, sizeof w.diff);
        w.aw = w.ah = W;
        return w;
    };
    // T165: №10 — урон из нескольких типов: каждый проходит защиты отдельно; одна проверка концентрации на сумму; цена —
    // лестница по сумме плюс надбавка за каждый доп. тип
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.dmgN = 4;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        s.more.push_back({4, 6, DT_COLD});
        int fireOnly = 0, N = 300, oneCheck = 0;
        for (int i = 0; i < N; i++) {
            World w = bare();
            Boss a = plain(100, 0, 0, 5), d = plain(100000, 0, 0, 5);
            d.immMask = 1u << DT_COLD;   // холод не проходит: остаётся только огонь (4к6, не больше 24)
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 12, 10));
            w.f[1].concSpec = 0;
            const int h0 = w.f[1].hp;
            doSpec(w, w.f[0], 0, 1);
            const int lost = h0 - w.f[1].hp;
            if (lost >= 4 && lost <= 24) fireOnly++;
            if (w.f[1].concDCs.size() == 1) oneCheck++;
        }
        Spec one = s;
        one.more.clear();
        Spec sum = one;
        sum.dmgN = 8;   // те же 8к6 одним типом
        const double pMix = specPrice(s, C2), pSum = specPrice(sum, C2), pOne = specPrice(one, C2);
        const double expect = std::ceil(C2[AT_DMG] * tierN(TR_DMG, 8.0) * (specMult(sum, C2) + C2[AT_DMGMIX]));
        T("T165", "§v21 №10: урон из нескольких типов — защиты по каждому типу отдельно, одна проверка концентрации; цена = лестница по сумме + надбавка за доп. тип",
          fireOnly == N && oneCheck == N && pMix > pSum && pSum > pOne && std::fabs(pMix - expect) <= 1,
          fmt("только огонь прошёл %d/%d, одна проверка %d/%d; цена 4к6+4к6 %.0f (ожид. %.0f), 8к6 одного типа %.0f, 4к6 %.0f", fireOnly, N, oneCheck, N, pMix, expect, pSum, pOne));
    }
    // T166: №9 — способность по клетке с любой доставкой: атакой — бросок по КД каждого в области; центр — любая клетка;
    // надбавка AT_GROUND в цене
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec s;
        s.delivery = 0;
        s.range = 60;
        s.radius = 10;
        s.dmgN = 2;
        s.dmgS = 6;
        s.ground = true;
        int hitAc = 0, hitHigh = 0, N = 400;
        for (int i = 0; i < N; i++) {
            World w = bare();
            Boss a = plain(100, 0, 0, 5), lo = plain(1000, 0, 0, 5), hi = plain(1000, 0, 0, 5);
            hi.ac = 100;   // промах по нему, кроме естественной 20
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(lo, 1, 20, 10));
            w.f.push_back(mk(hi, 1, 22, 10));
            doSpec(w, w.f[0], 0, -1, 21, 10);   // центр между ними
            hitAc += w.f[1].hp < 1000;
            hitHigh += w.f[2].hp < 1000;
        }
        Spec plainS = s;
        plainS.ground = false;
        const double pg = specPrice(s, C2), pp = specPrice(plainS, C2);
        T("T166", "§v21 №9: по клетке любой доставкой — атакой бросок по КД каждого в области (центр — пустая клетка); надбавка за выбор центра",
          hitAc > N * 0.9 && hitHigh < N * 0.15 && pg > pp, fmt("попадание по КД 0: %d/%d, по КД 100: %d/%d; цена по клетке %.0f, обычная %.0f", hitAc, N, hitHigh, N, pg, pp));
    }
    // T167: №22 — генератор создаёт способности по клетке и барьеры; бот применяет их (область — по лучшей клетке,
    // барьер — перед подходящим ближником, когда сам бьёт издали)
    if (TB()) {
        Coef C2 = CC();
        int gr = 0, wall = 0, n = 0;
        for (int i = 0; i < 3000; i++)
            for (auto& sp : makeBoss(1600, C2, false).abil) {
                n++;
                gr += sp.ground && sp.wallLen == 0;
                wall += sp.wallLen > 0;
            }
        // бот: барьер перед ближником (сам — лучник)
        World w = bare();
        Boss a = plain(1000, 1, 6, 60), e = plain(1000, 2, 8, 5);
        a.wAmmo = true;
        Spec bw;
        bw.ground = true;
        bw.range = 60;
        bw.wallLen = 5;
        bw.wallRounds = 3;
        bw.bonus = true;   // бонусным: оружием бьёт действием, барьер ставит бонусным
        a.abil.push_back(bw);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(e, 1, 18, 10));
        w.f[0].init = 2000;
        w.f[0].uses[0] = 1;
        takeTurn(w, 0);
        const bool wallUp = !w.dyn.empty() && w.f[0].uses[0] == 0;
        // бот: область по клетке накрывает двух врагов из-за дальности центра
        World v = bare();
        Boss c = plain(1000, 0, 0, 5);
        Spec fb;
        fb.delivery = 2;
        fb.range = 30;
        fb.radius = 10;
        fb.dmgN = 3;
        fb.ground = true;
        c.abil.push_back(fb);
        v.f.push_back(mk(c, 0, 10, 10));
        v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 17, 10));   // 35 фт — дальше дальности, но в области от клетки на 30 фт
        v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 18, 11));
        v.f[0].init = 2000;
        v.f[0].uses[0] = 1;
        takeTurn(v, 0);
        const bool both = v.f[1].hp < 1000 && v.f[2].hp < 1000;
        T("T167", "§v21 №22: генератор создаёт способности по клетке и барьеры; бот ставит барьер перед ближником и накрывает областью врагов за пределом дальности",
          gr > 0 && wall > 0 && wallUp && both, fmt("по клетке %d, барьеров %d из %d способностей; барьер поставлен %d; оба врага в области %d", gr, wall, n, wallUp, both));
    }
    // T168: №23 — реакции по четырём условиям; одна реакция за раунд; командой игрока реакцию не применить
    if (TB()) {
        auto mkReact = [&](int trig, bool self) {
            Spec s;
            s.react = trig;
            if (self) {
                s.self = true;
                s.delivery = 2;
                s.acB = 20;
                s.acRounds = 1;
            } else {
                s.delivery = 2;
                s.range = 30;
                s.dmgN = 2;
                s.dmgS = 6;
            }
            return s;
        };
        // 1: меня атакуют — щит +20 КД до броска
        int shield = 0, N = 300;
        for (int i = 0; i < N; i++) {
            World w = bare();
            Boss d = plain(1000, 0, 0, 5);
            d.ac = 10;
            d.abil.push_back(mkReact(1, true));
            w.f.push_back(mk(plain(1000, 1, 6, 5), 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            bool crit;
            const bool hit = attackRoll(w, w.f[0], w.f[1], PROF, 5, crit);
            shield += w.f[1].reactionUsed && w.f[1].acBuffRounds > 0 && (!hit || crit);
        }
        // 2: мне нанесли урон — ответный удар по виновнику (после действия)
        World w2 = bare();
        Boss r2 = plain(1000, 0, 0, 5);
        r2.abil.push_back(mkReact(2, false));
        w2.f.push_back(mk(plain(1000, 0, 0, 5), 0, 10, 10));
        w2.f.push_back(mk(r2, 1, 12, 10));
        g_srcIdx = 0;
        hurt(w2.f[1], 5, DT_FIRE, true);
        resolveReactions(w2);
        g_srcIdx = -1;
        const bool back = w2.f[0].hp < 1000 && w2.f[1].reactionUsed;
        // 3: враг вошёл в зону (30 фт)
        World w3 = bare();
        Boss r3 = plain(1000, 0, 0, 5);
        r3.abil.push_back(mkReact(3, false));
        w3.f.push_back(mk(plain(1000, 0, 0, 5), 0, 20, 10));   // 50 фт: за 60 фт движения входит в зону 30 фт
        w3.f.push_back(mk(r3, 1, 10, 10));
        Fighter goal;
        goal.x = 12;
        goal.y = 10;
        moveToward(w3, w3.f[0], goal, 0, 60, false);
        const bool entered = w3.f[0].hp < 1000 && w3.f[1].reactionUsed;
        // 4: враг применил способность в зоне; второй раз в том же раунде — нет (реакция одна)
        World w4 = bare();
        Boss caster = plain(1000, 0, 0, 5);
        Spec heal;   // способность без восстановления ХП (лечение могло вернуть урон реакции — тест зависел от броска)
        heal.self = true;
        heal.delivery = 2;
        heal.acB = 1;
        heal.acRounds = 1;
        caster.abil.push_back(heal);
        Boss r4 = plain(1000, 0, 0, 5);
        r4.abil.push_back(mkReact(4, false));
        w4.f.push_back(mk(caster, 0, 10, 10));
        w4.f.push_back(mk(r4, 1, 14, 10));
        doSpec(w4, w4.f[0], 0, 0);
        const int afterFirst = w4.f[0].hp;
        doSpec(w4, w4.f[0], 0, 0);
        const bool cast = afterFirst < 1000 && w4.f[1].reactionUsed && w4.f[1].uses[0] == UNL - 1;
        // игрок: командой реакцию не применить
        std::string err;
        Boss pb;
        long long bu = 0;
        int hpw = 0;
        bool ok = true;
        const bool bonusReact = !chr::bossFromJson(parseJson("{\"budget\":800,\"abilities\":[{\"delivery\":2,\"range\":30,\"dmgN\":1,\"react\":1,\"bonus\":true}]}", &ok), pb, bu, hpw, err);
        T("T168", "§v21 №23: реакции — меня атакуют (щит до броска), мне нанесли урон, враг вошёл в зону, враг применил способность; одна за раунд; реакция не бывает бонусным",
          shield > N * 0.9 && back && entered && cast && bonusReact, fmt("щит %d/%d, ответ на урон %d, на вход в зону %d, на способность (один раз) %d, бонусная реакция отклонена %d", shield, N, back, entered, cast, bonusReact));
    }
    // T169: №24 — концентрацию можно снять бесплатно: командой игрока и стойкой сети 9; эффекты снимаются
    if (TB()) {
        auto setup = [&](World& w) {
            Boss a = plain(1000, 0, 0, 5);
            Spec s;
            s.delivery = 2;
            s.range = 30;
            s.cond = C_SLOW;
            s.condRounds = 5;
            s.conc = true;
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
            doSpec(w, w.f[0], 0, 1);
        };
        World w = bare();
        setup(w);
        const bool had = w.f[0].concSpec == 0 && w.f[1].cond[C_SLOW] > 0;
        std::vector<std::string> cmds = {"{\"c\":\"dropconc\"}", "{\"c\":\"end\"}"};
        size_t ci = 0;
        bool sawConc = false;
        g_human.idx = 0;
        g_human.ask = [&](const std::string& req) {
            if (req.find("\"conc\":0") != std::string::npos) sawConc = true;
            return cmds[std::min(ci++, cmds.size() - 1)];
        };
        humanTurn(w, 0);
        g_human = HumanCtx{};
        const bool humanDrop = w.f[0].concSpec < 0 && w.f[1].cond[C_SLOW] == 0;
        World v = bare();
        setup(v);
        TurnState vts;
        applyPrim(v, 0, vts, Prim{PR_DROPCONC}, true);   // сеть снимает концентрацию тем же примитивом
        const bool netDrop = v.f[1].cond[C_SLOW] == 0 || v.f[0].concSpec != 0;
        T("T169", "§v21 №24: концентрацию снимают бесплатно — команда игрока и примитив сети; её эффекты снимаются",
          had && sawConc && humanDrop && netDrop, fmt("была %d, запрос показал концентрацию %d, снял игрок %d, сеть %d", had, sawConc, humanDrop, netDrop));
    }
    // T170 (старая сеть со стойками) удалён вместе с ней 8 ноября
