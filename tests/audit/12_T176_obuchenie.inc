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
