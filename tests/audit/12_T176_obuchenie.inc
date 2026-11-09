// Проверки движка, часть 12 из 12 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== v21, этап 3: обучение новой сети =====
    // T176: градиенты новой сети (типы, способность, указатели на врага и клетку, ценность, GRU по времени) совпадают с
    // численными
    if (TB()) {
        pn::PNetT<double> net;
        net.init(11);
        uint64_t sd = 99;
        auto rnd = [&]() { return GruNet<double>::rnd01(sd) * 2 - 1; };
        // 4 решения по ступеням: идти (клетка), применить → заклинание → способность 3 (клетка), взаимодействие → смена
        // оружия → запасное 2, применить → атака → оружие (враг)
        const int NT = 4;
        std::vector<pn::Decision> ds(NT);
        for (int t = 0; t < NT; t++) {
            pn::Decision& d = ds[t];
            d.x.resize(pn::D_IN);
            for (auto& v : d.x) v = (float)(rnd() * 0.5);
            for (int k = 0; k < pn::ST_N; k++) d.stMask[k] = true;
            d.stMask[pn::ST_END] = t != 1;   // «конец хода» закрыт (пустой ход) — маска тоже проверяется
            for (int k = 0; k < CAT_N; k++) d.catMask[k] = k != CAT_SUMMON;
            for (int k = 0; k < pn::NAB; k++) {
                d.abMask[k] = k == pn::AB_WEAPON || (k >= pn::AB_SPEC && k < pn::AB_SPEC + 4) || k == pn::AB_DASH;
                d.abCat[k] = k == pn::AB_WEAPON ? CAT_ATTACK : (k >= pn::AB_DASH ? CAT_MANEUVER : (k % 2 ? CAT_SPELL : CAT_ATTACK));
            }
            for (int k = 0; k < pn::IN_N; k++) d.inMask[k] = k != pn::IN_SURGE;
            for (int k = 0; k < pn::NSWAP; k++) d.swapMask[k] = true;
            d.st = t == 0 ? pn::ST_MOVE : (t == 2 ? pn::ST_INTERACT : pn::ST_APPLY);
            if (t == 1) d.cat = CAT_SPELL, d.ab = pn::AB_SPEC + 2;   // слот 3 — заклинание (нечётный)
            if (t == 3) d.cat = CAT_ATTACK, d.ab = pn::AB_WEAPON;
            if (t == 2) d.in = pn::IN_SWAP, d.sw = 1;
            if (t == 2) continue;   // смена оружия — без указателя
            d.ptrKind = t == 3 ? pn::PT_ENEMY : pn::PT_CELL;
            d.nc = t == 3 ? pn::MAXE : 5;
            const int nf = net.nf[d.ptrKind];
            d.feats.resize((size_t)d.nc * nf);
            for (auto& v : d.feats) v = (float)rnd();
            d.pmask.assign(d.nc, 1);
            d.pmask[0] = 0;
            d.c = 2;
        }
        const double R[NT] = {0.3, -0.2, 1.0, 0.5}, Wg[NT] = {1.0, -0.7, 0.4, 0.8}, vc = 0.5;
        auto lossOf = [&](const pn::PNetT<double>& n) {
            std::vector<double> h(n.H, 0.0), lg(pn::NFIX);
            typename GruNet<double>::Step st;
            double L = 0;
            for (int t = 0; t < NT; t++) {
                std::vector<double> x(ds[t].x.begin(), ds[t].x.end());
                double v;
                n.core.forward(x.data(), h.data(), st, lg.data(), v);
                h = st.h;
                L += -Wg[t] * pn::decisionLogp<double>(n, ds[t], h.data(), lg.data(), 0.0, nullptr, nullptr, nullptr) + 0.5 * vc * (v - R[t]) * (v - R[t]);
            }
            return L;
        };
        // аналитически: как в pt::update
        std::vector<double> gc(net.core.p.size(), 0.0), gp(net.ptr.size(), 0.0);
        {
            std::vector<typename GruNet<double>::Step> st(NT);
            std::vector<std::vector<double>> lg(NT, std::vector<double>(pn::NFIX));
            std::vector<double> val(NT), h(net.H, 0.0);
            std::vector<std::vector<double>> xs(NT);
            for (int t = 0; t < NT; t++) {
                xs[t].assign(ds[t].x.begin(), ds[t].x.end());
                net.core.forward(xs[t].data(), h.data(), st[t], lg[t].data(), val[t]);
                h = st[t].h;
            }
            std::vector<double> dhn(net.H, 0.0), dhp(net.H);
            for (int t = NT - 1; t >= 0; t--) {
                std::vector<double> dlg(pn::NFIX, 0.0), dh(dhn);
                pn::decisionLogp<double>(net, ds[t], st[t].h.data(), lg[t].data(), -Wg[t], dlg.data(), &gp, dh.data());
                net.core.backward(st[t], dlg.data(), vc * (val[t] - R[t]), dh.data(), gc, dhp.data());
                dhn = dhp;
            }
        }
        double worst = 0, worstAbs = 0;
        int checked = 0;
        auto check = [&](bool core, size_t i) {
            pn::PNetT<double> a = net, b = net;
            const double eps = 1e-6;
            (core ? a.core.p[i] : a.ptr[i]) += eps;
            (core ? b.core.p[i] : b.ptr[i]) -= eps;
            const double num = (lossOf(a) - lossOf(b)) / (2 * eps), an = core ? gc[i] : gp[i];
            // относительная ошибка — у градиентов от 1e-5; у мелких (~1e-6) разность при eps 1e-6 даёт лишь 3–4 верных
            // знака, там проверяется абсолютная ошибка
            const double mag = std::fabs(num) + std::fabs(an);
            if (mag >= 1e-5)
                worst = std::max(worst, std::fabs(num - an) / mag);
            else
                worstAbs = std::max(worstAbs, std::fabs(num - an));
            checked++;
        };
        for (size_t i = 0; i < net.core.p.size(); i += net.core.p.size() / 60) check(true, i);
        for (size_t i = net.core.oWo; i < net.core.p.size(); i += 97) check(true, i);   // головы и ценность
        for (size_t i = 0; i < net.ptr.size(); i += net.ptr.size() / 60) check(false, i);
        T("T176", "§v21 этап 3 и шаг 4: градиенты сети по ступеням (тип, категория, способность, взаимодействие, оружие, указатели, ценность, GRU по времени) совпадают с численными",
          worst < 1e-4 && worstAbs < 1e-9,
          fmt("проверено параметров %d, наибольшая относительная ошибка %.2e (градиенты от 1e-5, порог 1e-4), абсолютная у мелких %.1e (порог 1e-9)", checked, worst, worstAbs));
    }
    // T210: участок по заданной местности (кампания, задача Б-1): тот же вход — тот же участок; края по высоте сходятся
    // к середине между клеткой и соседом; поселение — только когда задано; карта годится для боя
    if (TB()) {
        wg::RegionDesc d;
        d.terrain = "Лес";
        d.level = 3;
        d.nb[0] = 3; d.nb[1] = 7; d.nb[2] = 1; d.nb[3] = 3;   // север, восток, юг, запад
        d.road = "NE";
        d.river = true;
        d.settle = "деревня";
        d.pop = 300;
        d.seed = "x12y40";
        static World a, b, e;
        regionArena(a, d, 200, 200);
        const std::string ja = mapToJson(a);
        regionArena(b, d, 200, 200);
        const bool same = ja == mapToJson(b);
        auto edgeAvg = [&](int k) {   // средний уровень на краю: север, восток, юг, запад
            double s = 0;
            for (int i = 0; i < 200; i++) s += k == 0 ? a.height[0][i] : k == 1 ? a.height[i][199] : k == 2 ? a.height[199][i] : a.height[i][0];
            return s / 200;
        };
        bool edges = true;
        for (int k = 0; k < 4; k++) edges = edges && std::fabs(edgeAvg(k) - (d.level + d.nb[k]) / 2.0) <= 1.0;
        int townA = 0, townE = 0;
        for (int y = 80; y < 120; y++)
            for (int x = 80; x < 120; x++) townA += a.wall[y][x] || a.door[y][x];
        wg::RegionDesc d2 = d;
        d2.settle.clear();
        d2.pop = 0;
        regionArena(e, d2, 200, 200);
        for (int y = 80; y < 120; y++)
            for (int x = 80; x < 120; x++) townE += e.wall[y][x] || e.door[y][x];
        CustomMap cm;
        std::string err;
        const std::string js = "{\"parts\":1," + ja.substr(1);
        const bool usable = parseCustomMap(parseJson(js), cm, err);
        T("T210", "§кампания Б-1: участок по заданной местности — тот же вход даёт тот же участок, края сходятся к соседям, поселение только по описанию, карта годится для боя",
          same && edges && townA > 0 && townE == 0 && usable,
          fmt("повтор %d; края С %.1f В %.1f Ю %.1f З %.1f (ожидалось %.1f %.1f %.1f %.1f); постройки в середине с поселением %d, без %d; для боя %d %s", same, edgeAvg(0), edgeAvg(1), edgeAvg(2), edgeAvg(3),
              (d.level + d.nb[0]) / 2.0, (d.level + d.nb[1]) / 2.0, (d.level + d.nb[2]) / 2.0, (d.level + d.nb[3]) / 2.0, townA, townE, usable, err.c_str()));
    }
    // T211: состояние героя (кампания, задача Б-3): "state" на входе — бой начинается с этими ХП, ячейками, применениями и
    // истощением; на выходе — состояние каждого главного бойца (потрачено от полного запаса); 0 ХП и истощение 6 — не в бой
    if (TB()) {
        Coef C2 = CC();
        const std::string base = R"({"name":"Тест","budget":800,"stats":[14,12,12,10,10,10],"acV":2,"weapon":{"n":1,"s":8,"range":5,"att":1,"type":1},
            "abilities":[{"_kind":"attack","range":60,"dmgN":2,"dmgS":6,"dtype":3,"uses":3},{"_kind":"attack","range":60,"dmgN":3,"dmgS":8,"dtype":3,"slotLvl":1}],
            "passives":{"speed":30,"slots":[0,3,0,0,0,0,0,0,0,0]})";
        Boss fresh, tired;
        long long bud = 0;
        std::string err;
        const bool okF = chr::buildCharacter(parseJson(base + "}"), C2, fresh, bud, err);
        const bool okT = chr::buildCharacter(parseJson(base + R"(,"state":{"hp":20,"slots":[0,2],"uses":[2,0],"exh":1}})"), C2, tired, bud, err);
        Boss zero, dead;
        const bool noZero = !chr::buildCharacter(parseJson(base + R"(,"сост":{"hp":0}})"), C2, zero, bud, err);
        const bool noDead = !chr::buildCharacter(parseJson(base + R"(,"state":{"exh":6}})"), C2, dead, bud, err);
        rng.seed(4711);
        Boss opp = makeBoss(400, C2, true, false);
        std::vector<std::string> log;
        rng.seed(99);
        fight(tired, opp, &log);
        const bool startHp = !log.empty() && log[0].find("\"hp\":20,") != std::string::npos;
        const EndState e = g_endState.empty() ? EndState{} : g_endState[0];
        const bool outOk = g_endState.size() == 2 && e.usesSpent.size() == 2 && e.usesSpent[0] >= 2 && e.slotsUsed[1] >= 2 && e.exh >= 1 && e.hp <= 20;
        rng.seed(99);
        fight(fresh, opp);
        const bool freshOk = g_endState.size() == 2 && g_endState[0].maxHp == fresh.hp;
        T("T211", "§кампания Б-3: состояние героя — бой начинается с заданными ХП, ячейками, применениями, истощением; после боя отдаётся состояние каждого; 0 ХП и истощение 6 — не в бой",
          okF && okT && noZero && noDead && startHp && outOk && freshOk,
          fmt("сборка %d/%d, отказ при 0 ХП %d и истощении 6 %d; начало с 20 ХП %d; итог: ХП %d, применения %d, ячейки 1-го %d, истощение %d", okF, okT, noZero, noDead, startHp, e.hp,
              e.usesSpent.empty() ? -1 : e.usesSpent[0], e.slotsUsed[1], e.exh));
    }

