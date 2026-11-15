// Проверки движка, часть 14 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== правки после быстрого теста 3d (2026-11-15): дым и туман зон, туман войны ночью, прямой путь игрока, зрение шагом 5 фт =====
    {
        static World w;
        auto clean = [&]() {
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            std::memset(w.tree, 0, sizeof w.tree);
            std::memset(w.diff, 0, sizeof w.diff);
            std::memset(w.water, 0, sizeof w.water);
            std::memset(w.height, 0, sizeof w.height);
            std::memset(w.door, 0, sizeof w.door);
            std::memset(w.deck, 0, sizeof w.deck);
            std::memset(w.pit, 0, sizeof w.pit);
            std::memset(w.ladder, 0, sizeof w.ladder);
            std::memset(w.haz, 0, sizeof w.haz);
            std::memset(w.dark, 0, sizeof w.dark);
            w.aw = w.ah = 30;
            w.day = true;
            w.fogAny = false;
            w.dyn.clear();
            w.f.clear();
            initObjects(w);
        };
        // T213: ядовитый дым и некротический туман — сильно заслонённая местность: стоящего в облаке не видно, сквозь облако
        // не видно, из облака не видно; лава (не облако) обзор не закрывает; летун выше облака (20 фт) виден; слепое зрение видит.
        // Туман войны на карте: клетка в дыму и за ним скрыта; ночью без тёмного зрения — лишь 30 фт, освещённое видно
        if (TB()) {
            clean();
            w.f.push_back(mk(plain(100, 1, 6, 5), 0, 2, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 10, 10));
            const Fighter &A = w.f[0], &B = w.f[1];
            const bool open = canSee(w, A, B);
            w.haz[10][6] = 3;   // ядовитый дым между ними
            w.fogAny = true;
            const bool through = !canSee(w, A, B) && !canSee(w, B, A);
            w.haz[10][6] = 1;   // лава — не облако
            const bool lava = canSee(w, A, B);
            w.haz[10][6] = 0;
            w.haz[10][10] = 6;   // B в некротическом тумане
            const bool inside = !canSee(w, A, B) && !canSee(w, B, A);
            w.f[1].alt = HAZ_UP;   // летит над облаком
            const bool above = canSee(w, A, B);
            w.f[1].alt = 0;
            w.f[0].b.blindsight = 60;
            const bool blind = canSee(w, A, B);
            w.f[0].b.blindsight = 0;
            // маска тумана войны (для показа): клетка в облаке и за ним — скрыты, перед ним — видна
            w.haz[10][10] = 0;
            w.haz[10][6] = 3;
            const std::string m = visMask(w, 0);
            auto seen = [&](const std::string& s, int x, int y) {
                static const std::string B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                const int k = y * w.aw + x, byte = k >> 3, grp = byte / 3, off = byte % 3;
                unsigned n = 0;
                for (int i = 0; i < 4; i++) n = n << 6 | (unsigned)std::max(0, (int)B64.find(s[4 * grp + i]));
                return (n >> (16 - 8 * off) & 255) >> (k & 7) & 1;
            };
            const bool mask = seen(m, 4, 10) && !seen(m, 6, 10) && !seen(m, 8, 10) && seen(m, 2, 20);
            // ночь: без тёмного зрения неосвещённое дальше 30 фт скрыто; тёмное зрение 60 фт видит; клетка в свете — видна
            w.haz[10][6] = 0;
            w.fogAny = false;
            w.day = false;
            const std::string n0 = visMask(w, 0);
            const bool night = seen(n0, 8, 10) && !seen(n0, 9, 10) && !seen(n0, 20, 10);
            w.f[0].b.dv = 60;
            const bool dv = seen(visMask(w, 0), 14, 10) && !seen(visMask(w, 0), 20, 10);
            w.f[0].b.dv = 0;
            w.f[1].b.lightR = 10;
            const bool lit = seen(visMask(w, 0), 10, 10) && seen(visMask(w, 0), 12, 10) && !seen(visMask(w, 0), 14, 10);
            T("T213", "дым и туман зон заслоняют (в облаке и сквозь него), лава — нет, над облаком и слепым зрением — видно; маска тумана: облако и ночь 30 фт, свет",
              open && through && lava && inside && above && blind && mask && night && dv && lit,
              fmt("открыто %d, сквозь дым %d, лава %d, в тумане %d, над облаком %d, слепое %d, маска %d, ночь %d, тёмное 60 %d, свет %d", open, through, lava, inside, above, blind, mask, night, dv, lit));
        }
        // T214: путь игрока (g_straight) — по прямой: из равных по цене путей ближайший к прямой, длина та же, что у обычного
        // поиска; при нехватке скорости игрок встаёт на прямой; дерево путей (показ под мышью) — те же клетки
        if (TB()) {
            clean();
            w.f.push_back(mk(plain(100, 1, 6, 5), 0, 2, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 28, 28));
            g_curW = &w;
            std::vector<PStep> p0, p1;
            pathTo(w, w.f[0], 14, 10, p0);
            g_straight = true;
            pathTo(w, w.f[0], 14, 10, p1);
            bool line = p1.size() == 12;
            for (auto& c : p1) line = line && c.second == 10;
            const bool same = p0.size() == p1.size();
            // наискосок (10 вправо, 4 вниз): каждая клетка — не дальше клетки от прямой
            std::vector<PStep> p2;
            pathTo(w, w.f[0], 12, 14, p2);
            bool near = p2.size() == 10;
            for (auto& c : p2) near = near && std::abs((c.first - 2) * 4 - (c.second - 10) * 10) <= 10;
            // дерево путей: те же клетки и футы
            static PathTree PT;
            pathTree(w, w.f[0], PT);
            const int k = 10 * w.aw + 14;
            int hops = 0;
            for (int c = k; PT.par[c] >= 0; c = PT.par[c]) hops++;
            const bool tree = PT.ft[k] == 60 && hops == 12 && PT.ft[10 * w.aw + 2] == 0;
            // ход на 30 фт к далёкой клетке: встал на прямой
            const int moved = moveToCell(w, w.f[0], 20, 10, 30);
            const bool stop = moved == 30 && w.f[0].x == 8 && w.f[0].y == 10;
            g_straight = false;
            g_curW = nullptr;
            T("T214", "путь игрока по прямой: та же длина, клетки на прямой, при нехватке скорости — на прямой; дерево путей совпадает",
              line && same && near && tree && stop,
              fmt("по прямой %d (%zu клеток), длина как у обычного %d (%zu), наискосок %d, дерево %d (фт %d, шагов %d), остановка %d (%d,%d за %d фт)",
                  line, p1.size(), same, p0.size(), near, tree, PT.ft[k], hops, stop, w.f[0].x, w.f[0].y, moved));
        }
        // T215: зрение любой дальности шагом 5 фт; цена — по прямой между прежними ступенями, на ступенях — прежняя
        if (TB()) {
            const bool steps = senseMul(30, SENSE_DV) == 1 && senseMul(60, SENSE_DV) == 2 && senseMul(120, SENSE_DV) == 3 && senseMul(10, SENSE_BS) == 1 &&
                               senseMul(60, SENSE_BS) == 3 && senseMul(120, SENSE_TS) == 4 && senseMul(0, SENSE_TS) == 0;
            const bool lin = std::abs(senseMul(45, SENSE_DV) - 1.5) < 1e-9 && std::abs(senseMul(15, SENSE_DV) - 0.5) < 1e-9 && std::abs(senseMul(20, SENSE_BS) - 1.5) < 1e-9;
            const bool ok = senseOK(35, SENSE_DV) && !senseOK(37, SENSE_DV) && !senseOK(125, SENSE_DV) && senseOK(60, SENSE_BS) && !senseOK(65, SENSE_BS) && !senseOK(-5, SENSE_TS);
            // через сборку из JSON: 45 фт принимается, 47 — нет
            bool okj = true;
            Boss b1, b2;
            std::string e1, e2;
            long long bu1 = 0, bu2 = 0;
            const bool a = chr::buildCharacter(parseJson("{\"v\":21,\"budget\":500,\"stats\":[10,10,10,10,10,10],\"passives\":{\"dv\":45}}", &okj), C, b1, bu1, e1);
            const bool r = !chr::buildCharacter(parseJson("{\"v\":21,\"budget\":500,\"stats\":[10,10,10,10,10,10],\"passives\":{\"dv\":47}}", &okj), C, b2, bu2, e2);
            T("T215", "зрение шагом 5 фт: цена на прежних ступенях та же, между ними — по прямой; сборка принимает 45 фт и отклоняет 47 фт",
              steps && lin && ok && a && b1.dv == 45 && r,
              fmt("ступени %d, по прямой %d, проверка %d, 45 фт %d (%s, dv %d), 47 фт отклонено %d (%s)", steps, lin, ok, a, e1.c_str(), b1.dv, r, e2.c_str()));
        }
    }
