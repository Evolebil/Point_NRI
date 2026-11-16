// Проверки движка, часть 17 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== режим кубов, этап 2: толчки с уступа, область рушит кубы-объекты, туман войны по кубам, деревья и nudge =====
    {
        static World w;
        static VoxWorld vw;
        const int G = VZ0 + 4;   // куб ног на ровной земле 20 фт
        auto clean = [&]() {
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
            std::memset(w.dark, 0, sizeof w.dark);
            w.aw = w.ah = 20;
            w.theme = 0;
            w.day = true;
            w.fogAny = false;
            w.dyn.clear();
            w.f.clear();
            for (int y = 0; y < 20; y++)
                for (int x = 0; x < 20; x++) w.height[y][x] = 4;
        };
        auto build = [&]() {
            initObjects(w);
            voxFromWorld(vw, w);
            w.vx = &vw;
            g_curW = &w;
        };
        auto sized = [&](int size, int team, int x, int y, int z) {
            Boss b = plain(100, 1, 6, 5);
            b.size = size;
            Fighter f = mk(b, team, x, y);
            f.z = z;
            f.zCol = y * W + x;
            return f;
        };
        // T224: толчок в кубах. По ровному — на всю длину; в стену — стоп и урон столкновения; со скалы 10 фт (2 куба) —
        // толчок кончается над пропастью, цель падает: 1к6, сбита с ног; во врага — стоп; крупнее на две ступени — не толкнуть
        if (TB()) {
            clean();
            for (int y = 0; y < 20; y++)
                for (int x = 0; x < 6; x++) w.height[y][x] = 6;   // плато 30 фт по x < 6, обрыв 10 фт к x = 6
            build();
            w.f.push_back(sized(2, 0, 2, 3, G + 2));
            w.f.push_back(sized(2, 1, 4, 3, G + 2));
            pushAway(w, w.f[0], w.f[1], 10);
            const int hp0 = w.f[1].hp;
            const bool off = w.f[1].x == 6 && w.f[1].z == G + 2;   // шагнул за край — на той же высоте, дальше не летит
            vxFixAll(w);
            const bool fell = w.f[1].z == G && w.f[1].hp < hp0 && w.f[1].hp >= hp0 - 6 && w.f[1].cond[C_PRONE] > 0;
            // по ровному внизу — 15 фт; в стену камня — стоп и урон 2к6
            w.f[0] = sized(2, 0, 8, 10, G);
            w.f[1] = sized(2, 1, 9, 10, G);
            pushAway(w, w.f[0], w.f[1], 15);
            const bool flat = w.f[1].x == 12 && w.f[1].z == G;
            for (int z = G; z < G + 6; z++) vw.set(14, 10, z, VX_STONE);
            const int hp1 = w.f[1].hp;
            pushAway(w, w.f[0], w.f[1], 15, 2);
            const bool wall = w.f[1].x == 13 && w.f[1].hp < hp1;
            // враг за спиной — стоп
            w.f.push_back(sized(2, 0, 10, 12, G));
            w.f[1].x = 9; w.f[1].y = 12; w.f[1].zCol = 12 * W + 9;
            w.f[0].x = 8; w.f[0].y = 12;
            pushAway(w, w.f[0], w.f[1], 15);
            const bool body = w.f[1].x == 9;
            // огромного средний не толкнёт
            Fighter Hg = sized(4, 1, 15, 2, G);
            w.f.push_back(Hg);
            Fighter& H = w.f.back();
            w.f[0].x = 14; w.f[0].y = 2;
            pushAway(w, w.f[0], H, 10);
            const bool big = H.x == 15;
            g_curW = nullptr;
            w.vx = nullptr;
            T("T224", "§3d толчок по кубам: по ровному, в стену (урон), с обрыва — падение 1к6 и сбит, во врага — стоп, огромного не толкнуть",
              off && fell && flat && wall && body && big,
              fmt("с края %d, упал %d, по ровному %d, в стену %d, во врага %d, огромный %d", off, fell, flat, wall, body, big));
        }
        // T225: область рушит кубы-объекты. Сфера урона у стены разрушает видимые от центра кубы стены (первый слой), второй
        // слой за ним цел; землю и траву не трогает; листву рушит; без урона (только состояние) — ничего; прежний бой — по слоям
        if (TB()) {
            clean();
            build();
            for (int y = 0; y < 20; y++)
                for (int z = G; z < G + 6; z++) vw.set(8, y, z, VX_STONE), vw.set(9, y, z, VX_STONE);
            vw.set(5, 12, G, VX_LEAF);
            const Fighter me = sized(2, 0, 2, 10, G);
            Spec s;
            s.shape = SH_SPHERE;
            s.radius = 10;
            s.dmgN = 40;
            s.dmgS = 6;
            s.dtype = DT_FIRE;
            areaHitsObjects(w, s, me, 7, 10, G);
            const bool front = vw.at(8, 10, G) == VX_AIR && vw.at(8, 10, G + 1) == VX_AIR;
            const bool back = vw.at(9, 10, G) == VX_STONE;
            const bool ground = vw.at(7, 10, G - 1) == VX_GRASS && vw.at(6, 10, G - 1) == VX_GRASS;
            areaHitsObjects(w, s, me, 5, 11, G);
            const bool leaf = vw.at(5, 12, G) == VX_AIR;
            vw.set(8, 3, G, VX_STONE);
            Spec c = s;
            c.dmgN = 0;
            c.cond = C_PRONE;
            areaHitsObjects(w, c, me, 7, 3, G);
            const bool noDmg = vw.at(8, 3, G) == VX_STONE;
            g_curW = nullptr;
            w.vx = nullptr;
            T("T225", "§3d область рушит кубы-объекты: первый слой стены — да, второй — нет, земля цела, листва горит, без урона — ничего",
              front && back && ground && leaf && noDmg, fmt("первый слой %d, второй цел %d, земля цела %d, листва %d, без урона %d", front, back, ground, leaf, noDmg));
        }
        // T226: туман войны по кубам (teamSees): враг за стеной не виден; враг в тоннеле под землёй не виден сверху; враг на
        // стене виден; крошечный за парапетом с башни не виден, средний — виден
        if (TB()) {
            clean();
            build();
            for (int y = 0; y < 20; y++)
                for (int z = G; z < G + 6; z++) vw.set(10, y, z, VX_STONE);
            w.f.push_back(sized(2, 0, 5, 5, G));
            w.f.push_back(sized(2, 1, 15, 5, G));
            const bool wall = !teamSees(w, 0, w.f[1]);
            w.f[1].x = 10; w.f[1].z = G + 6;   // на верху стены
            const bool onTop = teamSees(w, 0, w.f[1]);
            vw.set(6, 8, G - 2, VX_AIR);       // полость под землёй
            w.f[1].x = 6; w.f[1].y = 8; w.f[1].z = G - 2;
            const bool tunnel = !teamSees(w, 0, w.f[1]);
            // башня 4 куба в (0, 15), парапет (3, 15), цель (4, 15)
            for (int z = G; z < G + 4; z++) vw.set(0, 15, z, VX_STONE);
            vw.set(3, 15, G, VX_STONE);
            w.f[0] = sized(2, 0, 0, 15, G + 4);
            w.f[1] = sized(0, 1, 4, 15, G);
            const bool tiny = !teamSees(w, 0, w.f[1]);
            w.f[1] = sized(2, 1, 4, 15, G);
            const bool med = teamSees(w, 0, w.f[1]);
            g_curW = nullptr;
            w.vx = nullptr;
            T("T226", "§3d туман войны по кубам: за стеной и в тоннеле не видно, на стене видно, крошечный за парапетом не виден",
              wall && onTop && tunnel && tiny && med, fmt("стена %d, на стене %d, тоннель %d, крошечный %d, средний %d", wall, onTop, tunnel, tiny, med));
        }
        // T227: деревья в кубах (как в 2D — на дерево залезают): у ствола можно держаться и лезть вверх (5 фт + 5 за подъём),
        // на кроне стоять; боец, окружённый деревьями, находит путь наружу; nudge ставит на свободное место поближе по высоте
        // (несколько призванных в одну клетку — разные места, не друг в друге)
        if (TB()) {
            clean();
            w.tree[5][6] = 4;   // дерево 20 фт в (6, 5)
            for (int d = -1; d <= 1; d++) w.tree[10 + d][9] = w.tree[10 + d][11] = 8;   // боец в (10, 10) — стволы со всех сторон
            w.tree[9][10] = w.tree[11][10] = 8;
            build();
            Fighter P = sized(2, 0, 5, 5, G);
            const bool cling = vxStand(w, P, 5, 5, G + 2) && vxStep(w, P, 5, 5, G, 5, 5, G + 1) && vxStepCost(w, P, 5, 5, G, 5, 5, G + 1) == 10 &&
                               !vxStand(w, P, 4, 5, G + 2);
            const int top = vxTopZ(w, 6, 5);
            const bool crown = top > G && vxStand(w, P, 6, 5, top);
            Fighter Q = sized(2, 0, 10, 10, G);
            std::vector<PStep> path;
            pathTo(w, Q, 2, 10, path);
            const bool out = !path.empty() && path.back().first == 2 && path.back().second == 10;
            // три средних в одной клетке: nudge разводит, никто ни в ком
            w.f.push_back(sized(2, 0, 15, 15, G));
            for (int k = 0; k < 3; k++) {
                Fighter g = sized(2, 0, 15, 15, G);
                nudge(w, g);
                w.f.push_back(g);
            }
            bool apart = true, low = true;
            for (size_t i = 0; i < w.f.size(); i++) {
                low = low && w.f[i].z == G;
                for (size_t k = 0; k < i; k++) apart = apart && !touchesAt(w, w.f[i], w.f[i].x, w.f[i].y, w.f[i].z, w.f[k]);
            }
            g_curW = nullptr;
            w.vx = nullptr;
            T("T227", "§3d деревья: у ствола держаться и лезть (10 фт за куб), на кроне стоять, из кольца деревьев путь есть, nudge — свободно и на своей высоте",
              cling && crown && out && apart && low, fmt("у ствола %d, крона %d, путь наружу %d (шагов %d), разведены %d, на земле %d", cling, crown, out, (int)path.size(), apart, low));
        }
    }
