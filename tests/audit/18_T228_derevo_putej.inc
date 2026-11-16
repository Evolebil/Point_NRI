// Проверки движка, часть 18 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== режим кубов, этап 3: дерево путей под мышью (vxPathTreeJson) — то же, что пройдёт ход (vxPathTo) =====
    // T228: на карте с уступами, стеной и деревом для каждой клетки в пределах дерева путь из дерева (первый узел клетки)
    // совпадает с путём хода без высоты по шагам; с высотой (крыша стены) — с путём хода к этому кубу; цены не убывают
    // вдоль пути; дальше предела (maxFt) узлов нет
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
            for (int x = 0; x < 16; x++) w.height[y][x] = 4 + (x > 9 ? 2 : 0) + (y > 11 ? 1 : 0);
        for (int y = 2; y < 9; y++) w.wall[y][6] = true;
        w.tree[4][3] = 6;
        w.diff[8][4] = w.diff[8][5] = true;
        initObjects(w);
        voxFromWorld(vw, w);
        w.vx = &vw;
        g_curW = &w;
        Boss pb = plain(100, 1, 6, 5);
        pb.climbS = true;   // лазун — есть пути и на верх стены
        w.f.push_back(mk(pb, 0, 2, 6));
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 14, 14));
        for (auto& f : w.f) f.zCol = -1, f.z = 0;
        vxFixAll(w);
        Fighter& P = w.f[0];
        const std::string js = vxPathTreeJson(w, P, 40);
        bool okp = true;
        const Json t = parseJson("{" + js.substr(1) + "}", &okp)["vt"];
        const int n = (int)t.a.size() / 5;
        auto node = [&](int i, int k) { return t.a[(size_t)(5 * i + k)].num(0); };
        int checked = 0, same = 0, sameZ = 0, checkedZ = 0;
        bool mono = okp && n > 10, far = true;
        for (int i = 0; i < n; i++) {
            far = far && node(i, 4) <= 40;
            if (node(i, 3) >= 0) mono = mono && node(node(i, 3), 4) <= node(i, 4);
        }
        std::vector<bool> seen(16 * 16, false);
        for (int i = 1; i < n; i++) {
            const int x = node(i, 0), y = node(i, 1), z = node(i, 2);
            std::vector<std::array<int, 3>> tp;
            for (int c = i; c > 0; c = node(c, 3)) tp.push_back({node(c, 0), node(c, 1), node(c, 2)});
            std::reverse(tp.begin(), tp.end());
            auto eq = [&](const std::vector<PStep>& path) {
                if (path.size() != tp.size()) return false;
                for (size_t k = 0; k < tp.size(); k++)
                    if (path[k].first != tp[k][0] || path[k].second != tp[k][1] || path[k].l != tp[k][2]) return false;
                return true;
            };
            std::vector<PStep> path;
            if (!seen[(size_t)(y * 16 + x)]) {   // первый узел клетки — ход без высоты
                seen[(size_t)(y * 16 + x)] = true;
                checked++;
                g_vxTz = -1;
                pathTo(w, P, x, y, path);
                same += eq(path);
            } else {   // другой узел той же клетки — ход с высотой щелчка
                checkedZ++;
                g_vxTz = z;
                pathTo(w, P, x, y, path);
                g_vxTz = -1;
                sameZ += eq(path);
            }
        }
        g_curW = nullptr;
        w.vx = nullptr;
        T("T228", "§3d дерево путей под мышью = путь хода (без высоты и с высотой щелчка), цены не убывают, предел дальности",
          mono && far && checked > 20 && same == checked && checkedZ > 0 && sameZ == checkedZ,
          fmt("узлов %d, клеток %d (совпало %d), с высотой %d (совпало %d), цены %d, предел %d", n, checked, same, checkedZ, sameZ, mono, far));
    }
