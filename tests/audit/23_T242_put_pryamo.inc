// Проверки движка, часть 23 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== режим кубов: путь по ровному месту — прямо, а не «уголком» =====
    // T242: шаг наискосок стоит те же 5 фт, что прямо, поэтому равных по цене путей много; из них берётся ближайший к прямой
    // «старт → цель» (как в прежнем дереве путей). На ровном поле: к цели в той же строке — все шаги в этой строке; строго по
    // диагонали — все шаги на диагонали; к цели «конём» — лесенка не дальше полутора клеток от прямой; путь под мышью тот же
    if (TB()) {
        static World w;
        static VoxWorld vw;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.strong, 0, sizeof w.strong);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.water, 0, sizeof w.water);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        std::memset(w.ladder, 0, sizeof w.ladder);
        w.aw = w.ah = 16;
        w.theme = 0;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) w.height[y][x] = 4;
        initObjects(w);
        voxFromWorld(vw, w);
        w.vx = &vw;
        g_curW = &w;
        w.f.push_back(mk(plain(100, 1, 6, 5), 0, 12, 8));
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 1, 15));
        for (auto& f : w.f) f.zCol = -1, f.z = 0;
        vxFixAll(w);
        Fighter& P = w.f[0];
        g_vxTz = -1;
        // наибольшее отклонение шагов пути от прямой «старт → цель» (в клетках)
        auto dev = [&](const std::vector<PStep>& path, int tx, int ty) {
            const double ax = tx - P.x, ay = ty - P.y, len = std::sqrt(ax * ax + ay * ay);
            double m = 0;
            for (auto& s : path) m = std::max(m, std::abs(ax * (s.second - P.y) - ay * (s.first - P.x)) / len);
            return m;
        };
        std::vector<PStep> pr, pd, pk, pl;
        pathTo(w, P, 6, 8, pr);    // влево по строке
        pathTo(w, P, 6, 2, pd);    // строго по диагонали
        pathTo(w, P, 3, 5, pk);    // 9 влево, 3 вверх
        pathTo(w, P, 12, 1, pl);   // вверх по столбцу
        bool row = pr.size() == 6, col = pl.size() == 7;
        for (auto& s : pr) row = row && s.second == 8;
        for (auto& s : pl) col = col && s.first == 12;
        const double dd = dev(pd, 6, 2), dk = dev(pk, 3, 5);
        // путь под мышью (дерево путей) к той же клетке — те же шаги
        const Json t = parseJson("{" + vxPathTreeJson(w, P, 60).substr(1) + "}")["vt"];
        const int n = (int)t.a.size() / 5;
        auto node = [&](int i, int k) { return t.a[(size_t)(5 * i + k)].num(0); };
        bool tree = false;
        for (int i = 1; i < n && !tree; i++)
            if (node(i, 0) == 6 && node(i, 1) == 8) {
                tree = true;
                for (int c = i; c > 0; c = node(c, 3)) tree = tree && node(c, 1) == 8;
            }
        g_curW = nullptr;
        w.vx = nullptr;
        T("T242", "§3d путь по ровному месту прямо: из равных по цене — ближайший к прямой «старт → цель»",
          row && col && pd.size() == 6 && dd < 0.01 && pk.size() == 9 && dk < 1.5 && tree,
          fmt("строка %d (шагов %d), столбец %d, диагональ %.2f, конём %.2f (шагов %d), под мышью %d", row, (int)pr.size(), col, dd,
              dk, (int)pk.size(), tree));
    }
