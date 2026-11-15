// Проверки движка, часть 15 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== режим кубов: размеры существ (решение владельца 2026-11-16, вопрос №7 ТЗ): рост 0,25 · 0,5 · 1 · 2 · 3 · 4 куба,
    // тело n×n×n, глаза — 0,8 роста; крошечные — до 4 в клетке; протискивание на размер меньше (крупные) =====
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
        // T219: рост и глаза. Квадрат и рост тела — 1/1/1/2/3/4 куба; глаза 0,2/0,4/0,8/1,6/2,4/3,2 над низом куба ног.
        // Средний стоит в тоннеле высотой 1 куб, большой там полным телом — нет; под потолком на 2 куба — да. Копатель-средний
        // роет ход в 1 куб. Обзор с башни: за кубом-парапетом крошечного не видно, среднего — видно; большой глазами выше
        // парапета видит большого за ним, средний среднего — нет
        if (TB()) {
            clean();
            const Fighter t = sized(0, 0, 0, 0, G), s = sized(1, 0, 0, 0, G), m = sized(2, 0, 0, 0, G), l = sized(3, 0, 0, 0, G),
                          h = sized(4, 0, 0, 0, G), g = sized(5, 0, 0, 0, G);
            const bool eyes = vxEyeH(t, 1) == .2 && vxEyeH(s, 1) == .4 && vxEyeH(m, 1) == .8 && vxEyeH(l, 2) == .8 * 2 &&
                              vxEyeH(h, 3) == .8 * 3 && vxEyeH(g, 4) == .8 * 4;
            const bool body = fpFull(t) == 1 && fpFull(s) == 1 && fpFull(m) == 1 && fpFull(l) == 2 && fpFull(h) == 3 && fpFull(g) == 4;
            // тоннель: потолок на 1 куб над землёй в клетках (3..4, 3..4); потолок на 2 куба — в (7..8, 3..4)
            for (int y = 3; y <= 4; y++)
                for (int x = 3; x <= 4; x++) vw.set(x, y, G + 1, VX_STONE), vw.set(x + 4, y, G + 2, VX_STONE);
            Fighter M = sized(2, 0, 3, 3, G), L = sized(3, 0, 3, 3, G), L2 = sized(3, 0, 7, 3, G);
            const bool tunnel = vxStand(w, M, 3, 3, G) && vxFitN(w, M, 3, 3, G) == 1 && vxFitN(w, L, 3, 3, G) == 1 &&
                                vxFitN(w, L2, 7, 3, G) == 2 && !vxBodyIn(w, L, 3, 3, G, 2);
            // копатель-средний под землёй: ход ровно в 1 куб
            Boss bb = plain(100, 1, 6, 5);
            bb.burrowS = true;
            Fighter B = mk(bb, 0, 12, 12);
            B.z = 3;
            const size_t ed0 = vw.edits.size();
            vxDig(w, B);
            const bool dig = vw.at(12, 12, 3) == VX_AIR && vw.at(12, 12, 4) != VX_AIR && vw.edits.size() == ed0 + 1;
            // обзор: A на башне (4 куба камня в (0, 10)), парапет — куб камня в (3, 10), цель — в (4, 10)
            for (int z = G; z < G + 4; z++) vw.set(0, 10, z, VX_STONE);
            vw.set(3, 10, G, VX_STONE);
            w.f.push_back(sized(2, 0, 0, 10, G + 4));
            w.f.push_back(sized(0, 1, 4, 10, G));
            const bool tinyHid = !losF(w, w.f[0], w.f[1]);
            w.f[1] = sized(2, 1, 4, 10, G);
            const bool medSeen = losF(w, w.f[0], w.f[1]);
            // большие на земле: парапет в 1 куб между ними не закрывает (глаза на 1,6), средних — закрывает
            vw.set(10, 15, G, VX_STONE);
            vw.set(10, 16, G, VX_STONE);
            w.f[0] = sized(3, 0, 7, 15, G);
            w.f[1] = sized(3, 1, 12, 15, G);
            const bool bigSee = losF(w, w.f[0], w.f[1]);
            w.f[0] = sized(2, 0, 7, 15, G);
            w.f[1] = sized(2, 1, 12, 15, G);
            const bool medBlind = !losF(w, w.f[0], w.f[1]);
            g_curW = nullptr;
            w.vx = nullptr;
            T("T219", "§3d размеры: рост 0,25…4 куба (тело n×n×n), глаза 0,8 роста, тоннель 1 куб для среднего, ход копателя 1 куб, обзор по росту",
              eyes && body && tunnel && dig && tinyHid && medSeen && bigSee && medBlind,
              fmt("глаза %d, тело %d, тоннель %d, ход %d, крошечный за парапетом %d, средний виден %d, большие поверх %d, средние нет %d",
                  eyes, body, tunnel, dig, tinyHid, medSeen, bigSee, medBlind));
        }
        // T220: крошечные — до 4 в одной клетке (D&D: 2,5×2,5 фт). Четвёртому место есть, пятому нет; клетку с крошечным
        // другие размеры не занимают и крошечный — клетку с маленьким; крошечный выше (летит в другом кубе) и павший не считаются;
        // пятый, поставленный в полную клетку, отходит в соседнюю (nudge)
        if (TB()) {
            clean();
            for (int k = 0; k < 3; k++) w.f.push_back(sized(0, k % 2, 5, 5, G));
            Fighter T4 = sized(0, 0, 5, 5, G);
            const bool fourth = !blockedAt(w, T4, 5, 5);
            w.f.push_back(T4);
            Fighter T5 = sized(0, 1, 5, 5, G);
            const bool fifth = blockedAt(w, T5, 5, 5);
            const bool others = blockedAt(w, sized(1, 0, 5, 5, G), 5, 5) && blockedAt(w, sized(3, 0, 4, 4, G), 4, 4);
            w.f.push_back(sized(1, 1, 8, 8, G));
            const bool intoSmall = blockedAt(w, T5, 8, 8);
            // один из четырёх павший — пятому место есть; летящий выше (куб G + 1) — не в этом кубе
            w.f[0].hp = 0;
            const bool dead = !blockedAt(w, T5, 5, 5);
            w.f[0].hp = 100;
            w.f[1].z = G + 1;
            const bool above = !blockedAt(w, T5, 5, 5);
            w.f[1].z = G;
            // пятый поставлен в полную клетку — отходит в соседнюю свободную
            w.f.push_back(T5);
            nudge(w, w.f.back());
            const bool moved = (w.f.back().x != 5 || w.f.back().y != 5) && std::max(std::abs(w.f.back().x - 5), std::abs(w.f.back().y - 5)) == 1;
            // все четверо в одной клетке друг друга не выталкивают
            bool stay = true;
            for (int k = 0; k < 4; k++) stay = stay && !blockedAt(w, w.f[k], 5, 5);
            g_curW = nullptr;
            w.vx = nullptr;
            T("T220", "§3d крошечные: до 4 в клетке, пятому нет места, другие размеры клетку с крошечным не занимают",
              fourth && fifth && others && intoSmall && dead && above && moved && stay,
              fmt("четвёртый %d, пятый нет %d, другие нет %d, к маленькому нет %d, павший не в счёт %d, выше не в счёт %d, отошёл %d, четверо стоят %d",
                  fourth, fifth, others, intoSmall, dead, above, moved, stay));
        }
        // T221: протискивание в кубах (PHB): крупный встаёт туда, куда помещается тело на размер меньше (большой — в тоннель
        // 1×1×1, огромный — в 2×2×2, громадный — в 3×3×3, но не на два размера меньше); шаг туда вдвое дороже; протиснувшийся —
        // squeezed (помеха атакам и спасброскам ЛОВ, атаки по нему с преимуществом) и занимает меньший квадрат; путь сквозь
        // узкий тоннель находится; средний в тоннеле 1 куб не протискивается
        if (TB()) {
            clean();
            // тоннель шириной и высотой 1 куб вдоль y = 5 от x = 4 до x = 12 — сквозь стену камня 20 фт через всю арену
            for (int x = 4; x <= 12; x++)
                for (int y = 0; y < 20; y++)
                    for (int z = G; z < G + 4; z++)
                        if (!(y == 5 && z == G)) vw.set(x, y, z, VX_STONE);
            Fighter L = sized(3, 0, 2, 5, G), H = sized(4, 0, 4, 5, G), Gg = sized(5, 0, 4, 5, G), M = sized(2, 0, 6, 5, G);
            const bool large = vxStand(w, L, 6, 5, G) && vxFitN(w, L, 6, 5, G) == 1;
            const bool huge = !vxStand(w, H, 6, 5, G) && !vxStand(w, Gg, 6, 5, G);
            const bool cost = vxStepCost(w, L, 5, 5, G, 6, 5, G) == 10 && vxStepCost(w, L, 1, 0, G, 2, 0, G) == 5 && vxStepCost(w, M, 5, 5, G, 6, 5, G) == 5;
            // протиснувшийся в тоннеле: squeezed, квадрат 1, соседняя клетка (7, 5) его не задевает
            L.x = 6; L.y = 5; L.z = G;
            w.f.push_back(L);
            w.f.push_back(sized(2, 1, 7, 5, G));
            const bool sq = squeezed(w.f[0]) && fpN(w.f[0]) == 1 && !touchesAt(w, w.f[1], 7, 5, G, w.f[0]) && !squeezed(w.f[1]);
            // путь большого сквозь тоннель на другую сторону (14, 5): найден, в стене — только по тоннелю на уровне земли
            w.f.clear();
            Fighter L0 = sized(3, 0, 1, 5, G);
            std::vector<PStep> path;
            pathTo(w, L0, 14, 5, path);
            bool through = !path.empty() && path.back().first == 14 && path.back().second == 5;
            for (auto& c : path) through = through && (c.first < 4 || c.first > 12 || (c.second == 5 && c.l == G));
            // залы (другая арена): потолок на 3 куба — громадный протискивается (3×3×3), на 2 куба — огромный (2×2×2), громадный — нет
            clean();
            for (int y = 10; y < 20; y++)
                for (int x = 0; x < 20; x++) vw.set(x, y, x < 8 ? G + 2 : G + 3, VX_STONE);
            const bool fits = vxFitN(w, H, 1, 13, G) == 2 && vxFitN(w, Gg, 10, 13, G) == 3 && vxFitN(w, Gg, 1, 13, G) == 0 && vxFitN(w, Gg, 10, 2, G) == 4;
            g_curW = nullptr;
            w.vx = nullptr;
            T("T221", "§3d протискивание: крупный — в место на размер меньше (не на два), шаг вдвое дороже, помеха и меньший квадрат, путь сквозь тоннель",
              large && huge && fits && cost && sq && through,
              fmt("большой в тоннель %d, огромный и громадный нет %d, залы %d, цена шага %d, протиснулся %d, путь сквозь тоннель %d (шагов %d)",
                  large, huge, fits, cost, sq, through, (int)path.size()));
        }
    }
