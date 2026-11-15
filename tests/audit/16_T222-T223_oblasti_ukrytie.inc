// Проверки движка, часть 16 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== режим кубов, этап 2: области в объёме (не сквозь стену и потолок) и укрытие одним лучом =====
    // Та же сцена и те же ответы — в tests/test_render3d.mjs (предпросмотр области в клиенте)
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
            w.aw = w.ah = 20;
            w.theme = 0;
            w.dyn.clear();
            w.f.clear();
            for (int y = 0; y < 20; y++)
                for (int x = 0; x < 20; x++) w.height[y][x] = 4;
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
        // T222: области в объёме. Сцена: стена камня 30 фт по x = 6, применивший — средний в (2, 6). Сфера 10 фт с центром у
        // стены (5, 6) задевает рядом и сбоку, но не за стеной (хотя по расстоянию — в форме); летун на 10 фт над центром —
        // задет, на 15 фт — нет; под каменным потолком центр не достаёт стоящего на крыше; листва область не держит; конус
        // и линия от применившего упираются в стену; прежний бой (без кубов) — как inShapeF
        if (TB()) {
            clean();
            for (int y = 0; y < 20; y++)
                for (int z = G; z < G + 6; z++) vw.set(6, y, z, VX_STONE);
            const Fighter me = sized(2, 0, 2, 6, G);
            auto hits = [&](int shape, int size, int cx, int cy, int cz, const Fighter& e) { return areaHits(w, shape, size, me, cx, cy, cz, e); };
            const bool sphere = hits(SH_SPHERE, 10, 5, 6, G, sized(2, 1, 4, 6, G)) && hits(SH_SPHERE, 10, 5, 6, G, sized(2, 1, 5, 8, G)) &&
                                !hits(SH_SPHERE, 10, 5, 6, G, sized(2, 1, 7, 6, G)) && !hits(SH_SPHERE, 10, 5, 6, G, sized(2, 1, 5, 9, G));
            const bool fly = hits(SH_SPHERE, 10, 5, 6, G, sized(2, 1, 5, 6, G + 2)) && !hits(SH_SPHERE, 10, 5, 6, G, sized(2, 1, 5, 6, G + 3));
            // большой за стеной краем тела (8..9) — нет; большой, задевший форму краем квадрата (3..4, 7..8), — да
            const bool big = !hits(SH_SPHERE, 10, 5, 6, G, sized(3, 1, 7, 6, G)) && hits(SH_SPHERE, 10, 5, 6, G, sized(3, 1, 3, 7, G));
            // потолок: плита камня на 2 куба над землёй над (10..14, 10..14); центр под ней — стоящего на плите не задеть,
            // стоящего под ней рядом — задеть
            for (int y = 10; y <= 14; y++)
                for (int x = 10; x <= 14; x++) vw.set(x, y, G + 1, VX_STONE);
            const bool roof = !hits(SH_SPHERE, 10, 12, 12, G, sized(2, 1, 12, 12, G + 2)) && hits(SH_SPHERE, 10, 12, 12, G, sized(2, 1, 13, 12, G));
            // листва (куб листвы между центром и целью) — не преграда
            vw.set(16, 3, G, VX_LEAF);
            const bool leaf = hits(SH_SPHERE, 10, 15, 3, G, sized(2, 1, 17, 3, G));
            // конус 15 фт и линия 30 фт от применившего вдоль y = 6 к стене: перед стеной — да, за стеной — нет
            const bool cone = hits(SH_CONE, 15, 5, 6, G, sized(2, 1, 4, 6, G)) && !hits(SH_CONE, 15, 5, 6, G, sized(2, 1, 4, 10, G));
            const bool line = hits(SH_LINE, 30, 5, 6, G, sized(2, 1, 5, 6, G)) && !hits(SH_LINE, 30, 5, 6, G, sized(2, 1, 8, 6, G));
            // прежний бой: та же стена в слоях не мешает (области — по клеткам, как всегда)
            w.vx = nullptr;
            g_curW = nullptr;
            const Fighter far = sized(2, 1, 7, 6, 0);
            const bool old = areaHits(w, SH_SPHERE, 10, me, 5, 6, 0, far) == inShapeF(SH_SPHERE, 10, me.x, me.y, 5, 6, far) &&
                             areaHits(w, SH_SPHERE, 10, me, 5, 6, 0, far);
            T("T222", "§3d области в объёме: сфера не сквозь стену и потолок, летун выше радиуса не задет, листва не преграда, конус и линия упираются в стену",
              sphere && fly && big && roof && leaf && cone && line && old,
              fmt("сфера %d, летун %d, большие %d, потолок %d, листва %d, конус %d, линия %d, прежний бой %d", sphere, fly, big, roof, leaf, cone, line, old));
        }
        // T223: укрытие одним лучом (от глаз нападающего к середине тела цели): чистое поле 0; ящик-укрытие или листва на луче —
        // +2; цель за парапетом в 1 куб, нападающий на башне (глаза видят, середина тела закрыта) — +5 и цель видна; камень и
        // ящик на одном луче — +5; прежний бой — coverBonus по слоям
        if (TB()) {
            clean();
            w.f.push_back(sized(2, 0, 2, 6, G));
            w.f.push_back(sized(2, 1, 8, 6, G));
            const bool open = coverF(w, w.f[0], w.f[1]) == 0;
            vw.set(5, 6, G, VX_COVER);
            const bool crate = coverF(w, w.f[0], w.f[1]) == 2 && losF(w, w.f[0], w.f[1]);
            vw.set(5, 6, G, VX_LEAF);
            const bool leaf = coverF(w, w.f[0], w.f[1]) == 2;
            vw.set(5, 6, G, VX_AIR);
            // башня 3 куба в (0, 12), нападающий на ней (глаза — 3,8 куба над землёй), парапет (3, 12), цель (4, 12)
            for (int z = G; z < G + 3; z++) vw.set(0, 12, z, VX_STONE);
            vw.set(3, 12, G, VX_STONE);
            w.f[0] = sized(2, 0, 0, 12, G + 3);
            w.f[1] = sized(2, 1, 4, 12, G);
            const bool wall = coverF(w, w.f[0], w.f[1]) == 5 && losF(w, w.f[0], w.f[1]);
            // ящик и камень на одном луче — берётся большее
            vw.set(2, 12, G + 2, VX_COVER);
            const bool both = coverF(w, w.f[0], w.f[1]) == 5;
            // атака: КД цели с укрытием выше на 5 (через тот же расчёт, что у бросков)
            w.vx = nullptr;
            g_curW = nullptr;
            w.f[0] = sized(2, 0, 2, 6, 0);
            w.f[1] = sized(2, 1, 8, 6, 0);
            w.climb[6][5] = true;
            const bool old = coverF(w, w.f[0], w.f[1]) == coverBonus(w, 2, 6, 8, 6) && coverF(w, w.f[0], w.f[1]) == 2;
            w.climb[6][5] = false;
            T("T223", "§3d укрытие одним лучом: поле 0, ящик и листва +2, парапет под башней +5 (цель видна), большее из двух, прежний бой по слоям",
              open && crate && leaf && wall && both && old,
              fmt("поле %d, ящик %d, листва %d, парапет %d, большее %d, прежний бой %d", open, crate, leaf, wall, both, old));
        }
    }
