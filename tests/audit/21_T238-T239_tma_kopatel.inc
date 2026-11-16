// Проверки движка, часть 21 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== этап 4, решения владельца 2026-11-16: тьма без неба над головой; копатель держится под поверхностью =====
    // общее: ровный мир кубов 20×20, земля высотой 4 (куб ног — VZ0 + 4)
    auto flatVox = [&](World& w, VoxWorld& vw, std::function<void(World&)> shape) {
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.strong, 0, sizeof w.strong);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.water, 0, sizeof w.water);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.dark, 0, sizeof w.dark);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        std::memset(w.ladder, 0, sizeof w.ladder);
        w.aw = w.ah = 20;
        w.theme = 0;
        w.day = true;
        w.dyn.clear();
        w.f.clear();
        w.vxo.clear();
        for (int y = 0; y < 20; y++)
            for (int x = 0; x < 20; x++) w.height[y][x] = 4;
        shape(w);
        initObjects(w);
        voxFromWorld(vw, w);
        w.vx = &vw;
        g_curW = &w;
    };
    // T238: под крышей (над головой твёрдый непрозрачный куб) и в пещере — тьма: без тёмного зрения цель не видна, с тёмным
    // зрением нужной дальности и истинным — видна; под кроной дерева (листва) — небо есть; солнце под крышей не светит;
    // бой по слоям (w.vx пуст) — как прежде
    if (TB()) {
        static World w;
        static VoxWorld vw;
        const int G = VZ0 + 4;
        flatVox(w, vw, [&](World& ww) {
            ww.vxo[10 * W + 10] = {G + 2, "5"};   // крыша (камень) над клеткой (10, 10) на 10 фт
            ww.vxo[10 * W + 14] = {G - 3, "00"};   // пещера под (14, 10): ходы в кубах G−3, G−2, над ними 2 куба земли
            ww.tree[10][6] = 4;   // дерево 20 фт: ствол и крона над (6, 10)
        });
        Boss pb = plain(100, 1, 6, 5), dvb = pb, tsb = pb;
        dvb.dv = 60;
        tsb.truesight = 60;
        w.f.push_back(mk(pb, 0, 2, 10));
        w.f.push_back(mk(pb, 1, 10, 10));   // под крышей
        w.f.push_back(mk(pb, 1, 14, 10));   // в пещере
        w.f.push_back(mk(pb, 1, 6, 11));    // у дерева, под небом
        w.f.push_back(mk(dvb, 0, 2, 12));
        w.f.push_back(mk(tsb, 0, 2, 13));
        for (auto& f : w.f) f.zCol = f.y * W + f.x, f.z = G;
        w.f[2].z = G - 3;
        const Fighter &V = w.f[0], &R = w.f[1], &C = w.f[2], &T6 = w.f[3], &D = w.f[4], &S = w.f[5];
        const bool roofDark = vxNoSky(w, R) && !canSee(w, V, R) && canSee(w, D, R) && canSee(w, S, R);
        const bool caveDark = vxNoSky(w, C) && !canSee(w, V, C) && canSee(w, D, C);
        Fighter crown = T6;
        crown.x = 6, crown.y = 10;   // под кроной (листва небо не закрывает)
        const bool open = !vxNoSky(w, T6) && canSee(w, V, T6) && !vxNoSky(w, crown) && !vxNoSky(w, V);
        const bool sun = inSun(w, V) && !inSun(w, R);
        w.vx = nullptr;
        const bool layers = !vxNoSky(w, R) && canSee(w, V, R);
        g_curW = nullptr;
        T("T238", "§4 тьма без неба: под крышей и в пещере без тёмного зрения не видно, с тёмным и истинным — видно; листва небо не закрывает; солнце под крышей не светит; по слоям — как прежде",
          roofDark && caveDark && open && sun && layers, fmt("крыша %d, пещера %d, под небом и кроной %d, солнце %d, бой по слоям %d", roofDark, caveDark, open, sun, layers));
    }
    // T239: копатель под каменной стеной (её не роют) идёт ходом сразу под поверхностью — не глубже 2 кубов под землёй (раньше
    // при равной цене нырял до дна мира); путь той же длины в футах, что и прежде (+1 к цене — меньше фута)
    if (TB()) {
        static World w;
        static VoxWorld vw;
        const int G = VZ0 + 4;
        flatVox(w, vw, [&](World& ww) {
            for (int y = 0; y < 20; y++) ww.wall[y][10] = true;   // стена поперёк всей арены
        });
        Boss bb = plain(100, 1, 6, 5);
        bb.burrowS = true;
        w.f.push_back(mk(bb, 0, 3, 10));
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 19, 19));
        for (auto& f : w.f) f.zCol = f.y * W + f.x, f.z = G;
        Fighter& B = w.f[0];
        std::vector<PStep> bp;
        vxPathTo(w, B, 17, 10, bp);
        int minZ = 99;
        for (auto& c : bp) minZ = std::min(minZ, c.l);
        const bool reach = !bp.empty() && bp.back().first == 17 && bp.back().second == 10;
        const bool shallow = minZ >= G - 3;   // ноги не ниже второго куба под землёй (поверхность — куб G − 1)
        const bool straight = (int)bp.size() <= 16;
        g_curW = nullptr;
        w.vx = nullptr;
        T("T239", "§4 копатель: под стеной — ходом сразу под поверхностью, не на дне мира; путь не длиннее прежнего",
          reach && shallow && straight, fmt("дошёл %d, шагов %zu, ниже всего куб %d (поверхность %d)", reach, bp.size(), minZ, G - 1));
    }
