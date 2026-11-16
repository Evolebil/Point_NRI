// Проверки движка, часть 20 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== этап 4 «Мир и генератор»: слой кубов карты (World::vxo) — пещеры, дома с этажами и крышами =====
    // общее: участок мира по зерну в заданном месте со слоем кубов; мир кубов по нему; пешеход среднего роста
    auto volArena = [&](World& w, unsigned long long seed, int side, int wx, int wy, int theme) {
        const MapStyle keep = g_style;
        g_style = MapStyle{};
        g_style.worldSeed = seed;
        g_style.fixedW = g_style.fixedH = side;
        g_style.wx = wx;
        g_style.wy = wy;
        g_style.theme = theme;
        g_volGen = true;
        genMap(w);
        g_volGen = false;
        g_style = keep;
        initObjects(w);
        w.dyn.clear();
        w.f.clear();
    };
    auto walkerAt = [&](World& w, int x, int y, int z) {
        Fighter f = mk(plain(100, 1, 6, 5), 0, x, y);
        f.z = z;
        f.zCol = y * W + x;
        return f;
    };
    // T230: пещеры — ходы и залы под землёй (кубы воздуха в слое), над ходом не меньше 2 кубов земли (у края арены, где
    // выходов нет, — везде), под водой, ущельем, опасностью и поселением пещер нет; повтор участка — тот же слой; слои карты
    // и общий генератор случайных чисел с ним и без него — те же (бои по слоям не меняются)
    if (TB()) {
        static World a, b, c;
        rng.seed(230);
        volArena(a, 2, 100, 264, 759, -1);
        const uint64_t r1 = rng.next();
        volArena(b, 2, 100, 264, 759, -1);
        rng.seed(230);
        {   // тот же участок без слоя кубов: слои и случайные числа — те же
            const MapStyle keep = g_style;
            g_style = MapStyle{};
            g_style.worldSeed = 2;
            g_style.fixedW = g_style.fixedH = 100;
            g_style.wx = 264;
            g_style.wy = 759;
            genMap(c);
            g_style = keep;
        }
        const uint64_t r2 = rng.next();
        bool layersSame = c.vxo.empty();
        for (int y = 0; y < a.ah; y++)
            for (int x = 0; x < a.aw; x++)
                layersSame = layersSame && a.height[y][x] == c.height[y][x] && a.wall[y][x] == c.wall[y][x] && a.tree[y][x] == c.tree[y][x] &&
                             a.deck[y][x] == c.deck[y][x] && a.door[y][x] == c.door[y][x] && a.water[y][x] == c.water[y][x] && a.mainc[y][x] == c.mainc[y][x];
        static VoxWorld v;
        voxFromWorld(v, a);
        int caveN = 0, edgeThin = 0, wet = 0;
        for (const auto& [k, o] : a.vxo) {
            const int x = k % W, y = k / W, g = VZ0 + a.height[y][x];
            const bool edge = x < 2 || y < 2 || x >= a.aw - 2 || y >= a.ah - 2;
            for (int i = 0; i < (int)o.second.size(); i++) {
                if (o.second[(size_t)i] != '0') continue;
                const int z = o.first + i;
                if (z >= g) continue;
                caveN++;
                if (edge && z > g - 3) edgeThin++;
                if (a.water[y][x] || a.pit[y][x] || a.haz[y][x]) wet++;
            }
        }
        T("T230", "§4 пещеры: ходы и залы под землёй, у края — под 2 кубами земли, не под водой и ущельем; повтор тот же; слои и случайные числа — без изменений",
          caveN > 2000 && edgeThin == 0 && wet == 0 && a.vxo == b.vxo && layersSame && r1 == r2 && v.at(50, 50, 0) == VX_BEDROCK,
          fmt("кубов пещер %d, у края тонко %d, под водой %d, повтор %d, слои без изменений %d", caveN, edgeThin, wet, a.vxo == b.vxo, layersSame));
    }
    // T231: у пещеры (полость от 12 кубов) — выход на поверхность: пешеход среднего роста (не лазун, не копатель) с поверхности
    // доходит до большинства полостей и спускается глубже 10 фт под землю
    if (TB()) {
        static World w;
        static VoxWorld v;
        volArena(w, 3, 100, 264, 759, -1);
        voxFromWorld(v, w);
        w.vx = &v;
        g_curW = &w;
        const int AW = w.aw, AH = w.ah, Z = v.sz;
        auto cave = [&](int x, int y, int z) { return z >= 1 && z < VZ0 + w.height[y][x] - 1 && v.at(x, y, z) == VX_AIR; };
        auto id = [&](int x, int y, int z) { return ((size_t)y * AW + x) * Z + z; };
        std::vector<int> comp((size_t)AW * AH * Z, -1), sizes;
        for (int y = 0; y < AH; y++)
            for (int x = 0; x < AW; x++)
                for (int z = 1; z < Z; z++) {
                    if (!cave(x, y, z) || comp[id(x, y, z)] >= 0) continue;
                    const int cid = (int)sizes.size();
                    int n = 0;
                    std::vector<std::array<int, 3>> st{{x, y, z}};
                    comp[id(x, y, z)] = cid;
                    while (!st.empty()) {
                        const auto q = st.back();
                        st.pop_back();
                        n++;
                        static const int D[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                        for (const auto& e : D) {
                            const int nx = q[0] + e[0], ny = q[1] + e[1], nz = q[2] + e[2];
                            if (nx < 0 || ny < 0 || nz < 0 || nx >= AW || ny >= AH || nz >= Z || !cave(nx, ny, nz) || comp[id(nx, ny, nz)] >= 0) continue;
                            comp[id(nx, ny, nz)] = cid;
                            st.push_back({nx, ny, nz});
                        }
                    }
                    sizes.push_back(n);
                }
        int sx = -1, sy = -1;
        for (int y = 0; y < AH && sx < 0; y++)
            for (int x = 0; x < AW; x++)
                if (w.mainc[y][x] && !w.wall[y][x] && !w.tree[y][x]) { sx = x; sy = y; break; }
        Fighter f = walkerAt(w, sx, sy, vxTopZ(w, sx, sy));
        std::vector<char> hit(sizes.size(), 0);
        int deep = 0;
        vxDijkstra(w, f, -1, -1, [&](int, int, int x, int y, int z) {
            if (cave(x, y, z)) {
                hit[(size_t)comp[id(x, y, z)]] = 1;
                deep = std::max(deep, VZ0 + w.height[y][x] - z);
            }
            return false;
        });
        int big = 0, bigHit = 0;
        for (size_t i = 0; i < sizes.size(); i++)
            if (sizes[i] >= 12) big++, bigHit += hit[i];
        g_curW = nullptr;
        w.vx = nullptr;
        T("T231", "§4 пещеры: выход с поверхности — пешеход доходит до большинства пещер и спускается глубже 10 фт",
          big >= 10 && bigHit * 10 >= big * 8 && deep >= 3, fmt("пещер от 12 кубов %d, дошёл %d, глубже всего %d кубов", big, bigHit, deep));
    }
    // T232: дома из кубов — стены высотой дома (этаж — 2 куба до потолка и куб пола), закрытая дверь — 2 куба двери, двускатная
    // крыша из досок уступами по кубу; в доме выше этажа пешеход поднимается по ступеням на верхний этаж, снаружи по приставной
    // лестнице — на крышу (решение владельца 2026-11-16)
    if (TB()) {
        static World w;
        static VoxWorld v;
        volArena(w, 4, 20, 0, 0, 1);   // план мира — затем участок 60×60 на середине первого города (не деревни)
        int tx = 500, ty = 500;
        for (const auto& t : g_wplan.towns)
            if (t.kind >= 1) { tx = t.x; ty = t.y; break; }
        volArena(w, 4, 60, tx - 30, ty - 30, 1);
        voxFromWorld(v, w);
        w.vx = &v;
        g_curW = &w;
        std::map<std::pair<int, int>, wg::Cell> houses;   // дома целиком на арене и целиком свои (не срезаны площадью или краем поселения)
        std::map<std::pair<int, int>, int> cells;
        for (int y = 0; y < w.ah; y++)
            for (int x = 0; x < w.aw; x++) {
                const wg::Cell c = wg::worldCell(g_wplan, w.wx + x, w.wy + y);
                if (c.hw) houses.emplace(std::make_pair(x - c.hx, y - c.hy), c), cells[{x - c.hx, y - c.hy}]++;
            }
        for (auto it = houses.begin(); it != houses.end();)
            it = cells[it->first] == it->second.hw * it->second.hh ? std::next(it) : houses.erase(it);
        int built = 0, multi = 0, upOk = 0, roofed = 0, roofOk = 0, doors = 0, doorOk = 0, wallOk = 0;
        for (const auto& [o, h] : houses) {
            const int x0 = o.first, y0 = o.second, b = VZ0 + w.height[y0][x0];
            if (!w.vxo.count(y0 * W + x0)) continue;   // дом на неровной земле — по слоям
            built++;
            const int Hs = h.hall ? 3 : 2, F = h.floors, zt = b + F * (Hs + 1) - 1;
            // угол дома: стена до карниза, на карнизе — доски крыши, выше — воздух
            bool wall = v.at(x0, y0, zt) == VX_PLANK && v.at(x0, y0, zt + 1) == VX_AIR;
            for (int z = b; z < zt; z++) wall = wall && (v.at(x0, y0, z) == (h.wood ? VX_PLANK : VX_STONE) || v.at(x0, y0, z) == VX_STRONG);
            wallOk += wall;
            for (int qy = 0; qy < h.hh; qy++)
                for (int qx = 0; qx < h.hw; qx++)
                    if (w.door[y0 + qy][x0 + qx] == 1) doors++, doorOk += v.at(x0 + qx, y0 + qy, b) == VX_DOOR && v.at(x0 + qx, y0 + qy, b + 1) == VX_DOOR;
            auto inHouse = [&](int x, int y) { return x >= x0 && y >= y0 && x < x0 + h.hw && y < y0 + h.hh; };
            // пешеход на полу первого этажа (свободная клетка внутри): дойдёт ли до пола этажа 2 по ступеням
            int mx = -1, my = -1;
            for (int qy = 1; qy < h.hh - 1 && mx < 0; qy++)
                for (int qx = 1; qx < h.hw - 1; qx++)
                    if (v.at(x0 + qx, y0 + qy, b) == VX_AIR && v.at(x0 + qx, y0 + qy, b + 1) == VX_AIR) { mx = x0 + qx; my = y0 + qy; break; }
            if (F >= 2 && mx >= 0) {
                bool up = false;
                vxDijkstra(w, walkerAt(w, mx, my, b), -1, -1, [&](int, int, int x, int y, int z) {
                    if (inHouse(x, y) && x > x0 && y > y0 && x < x0 + h.hw - 1 && y < y0 + h.hh - 1 && z == b + Hs + 1) up = true;
                    return up;
                });
                multi++, upOk += up;
            }
            // пешеход у приставной лестницы снаружи: дойдёт ли до крыши (ноги выше карниза над домом)
            for (int qy = -1; qy <= h.hh; qy++)
                for (int qx = -1; qx <= h.hw; qx++) {
                    const int lx = x0 + qx, ly = y0 + qy;
                    if (lx < 0 || ly < 0 || lx >= w.aw || ly >= w.ah || inHouse(lx, ly) || v.at(lx, ly, b) != VX_LADDER) continue;
                    bool roof = false;
                    vxDijkstra(w, walkerAt(w, lx, ly, b), -1, -1, [&](int, int, int x, int y, int z) {
                        if (inHouse(x, y) && z > zt) roof = true;
                        return roof;
                    });
                    roofed++, roofOk += roof;
                }
        }
        g_curW = nullptr;
        w.vx = nullptr;
        T("T232", "§4 дома: стены высотой дома, крыша из досок, закрытая дверь — 2 куба; по ступеням — на верхний этаж, по лестнице — на крышу",
          built >= 4 && wallOk == built && multi >= 1 && upOk == multi && roofed >= 1 && roofOk == roofed && doorOk == doors,
          fmt("домов из кубов %d (стены как надо %d), в 2+ этажа %d (поднялся %d), с лестницей %d (на крыше %d), закрытых дверей %d (из кубов двери %d)",
              built, wallOk, multi, upOk, roofed, roofOk, doors, doorOk));
    }
    // T233: слой кубов — в формате карты: mapToJson → parseCustomMap → applyCustomMap — тот же слой и тот же мир кубов;
    // негодное (вне карты, дно мира, чужие символы) отбрасывается; карта без слоя — мир кубов по слоям
    if (TB()) {
        static World a, r;
        static VoxWorld va, vr;
        volArena(a, 4, 40, 562, 169, 1);
        bool okp = true;
        Json j = parseJson("{\"parts\":1," + mapToJson(a).substr(1), &okp);
        CustomMap m;
        std::string err;
        const bool parsed = okp && parseCustomMap(j, m, err);
        applyCustomMap(r, m);
        voxFromWorld(va, a);
        voxFromWorld(vr, r);
        const bool same = parsed && r.vxo == a.vxo && va.mat == vr.mat && va.sz == vr.sz;
        Json bad = parseJson("{\"aw\":6,\"ah\":6,\"hz\":5,\"vx\":[[1,1,0,\"00\"],[9,1,1,\"00\"],[2,2,1,\"0z5\"]]}", &okp);
        CustomMap mb;
        parseCustomMap(bad, mb, err);
        const bool badOk = mb.vx.size() == 1 && mb.vx.count(2 * W + 2) && mb.vx[2 * W + 2].second == "0.5";
        T("T233", "§4 слой кубов в формате карты: туда и обратно — тот же мир кубов; негодное отбрасывается",
          same && !a.vxo.empty() && badOk, fmt("разобрано %d (%s), слой тот же %d, столбцов %zu, отбор негодного %d", parsed, err.c_str(), same, a.vxo.size(), badOk));
    }
    // T234: кампания — пещеры соседних участков сходятся: участки (0, 0) и (1, 0) одного мира региона; у общей границы ходы —
    // из одного шума в координатах региона (кубы пещеры там, где шум велит), есть ходы, идущие через границу; без места в
    // регионе — тоже строится (свои координаты)
    if (TB()) {
        static World L, R2, N;
        wg::RegionDesc d;
        d.terrain = "Горы";
        d.level = 7;
        for (int k = 0; k < 4; k++) d.nb[k] = 7;
        d.wseed = "t234";
        d.gx = 0, d.gy = 0;
        d.seed = "t234:0:0";
        g_volGen = true;
        regionArena(L, d, 200, 200);
        d.gx = 1;
        d.seed = "t234:1:0";
        regionArena(R2, d, 200, 200);
        d.gx = d.gy = -1;
        regionArena(N, d, 60, 60);
        g_volGen = false;
        const uint64_t cs = wg::seedOfText("пещеры:t234");
        auto airAt = [&](const World& w, int x, int y, int z) {
            const auto it = w.vxo.find(y * W + x);
            if (it == w.vxo.end()) return false;
            const int k = z - it->second.first;
            return k >= 0 && k < (int)it->second.second.size() && it->second.second[(size_t)k] == '0';
        };
        int match = 0, mism = 0, cross = 0;
        for (int y = 0; y < 200; y++) {
            const int gl = VZ0 + L.height[y][199] - 3, gr = VZ0 + R2.height[y][0] - 3;
            for (int z = 1; z <= std::min(gl, gr); z++) {
                const bool nl = wg::caveAir(cs, 199, y, z), nr = wg::caveAir(cs, 200, y, z);
                const bool al = airAt(L, 199, y, z), ar = airAt(R2, 0, y, z);
                const bool okL = L.water[y][199] || L.pit[y][199] || L.haz[y][199] || al == nl, okR = R2.water[y][0] || R2.pit[y][0] || R2.haz[y][0] || ar == nr;
                (okL && okR) ? match++ : mism++;
                cross += al && ar;
            }
        }
        T("T234", "§4 кампания: пещеры соседних участков — из одного шума в координатах региона, ходы через границу есть",
          mism == 0 && match > 500 && cross > 0 && !N.vxo.empty(), fmt("совпало %d, не совпало %d, ходов через границу %d, участок без места — столбцов %zu", match, mism, cross, N.vxo.size()));
    }
    // T235: бой в кубах на карте с домами: бойцы, поставленные картой в дом, начинают на полу (на земле), а не на крыше;
    // бой идёт до конца
    if (TB()) {
        static World a;
        volArena(a, 4, 40, 562, 169, 1);
        int s[2][2] = {{-1, -1}, {-1, -1}}, n = 0;
        for (int y = 1; y < a.ah - 1 && n < 2; y++)
            for (int x = 1; x < a.aw - 1 && n < 2; x++) {
                const wg::Cell c = wg::worldCell(g_wplan, a.wx + x, a.wy + y);
                if (!c.hw || c.hx <= 0 || c.hy <= 0 || c.hx >= c.hw - 1 || c.hy >= c.hh - 1 || !a.vxo.count(y * W + x) || !startCellOK(a, x, y)) continue;
                const auto& o = a.vxo[y * W + x];
                if (o.second.find('a') == std::string::npos || (n && std::abs(x - s[0][0]) + std::abs(y - s[0][1]) < 12)) continue;   // крыша над клеткой
                s[n][0] = x, s[n][1] = y, n++;
            }
        bool okp = true;
        const std::string js = mapToJson(a);
        Json j = parseJson("{\"parts\":1,\"starts\":[[" + std::to_string(s[0][0]) + "," + std::to_string(s[0][1]) + "],[" + std::to_string(s[1][0]) + "," +
                               std::to_string(s[1][1]) + "]]," + js.substr(1), &okp);
        std::string err;
        const bool parsed = n == 2 && okp && parseCustomMap(j, g_customMap, err);
        std::vector<std::string> log;
        int z0 = -1, z1 = -1, res = -9;
        if (parsed) {
            g_voxMode = true;
            res = fight(plain(40, 1, 6, 5), plain(40, 1, 6, 5), &log);
            g_voxMode = false;
            const Json fr = parseJson(log.empty() ? "{}" : log[0], &okp);
            z0 = fr["f"][0]["z"].num(-1);
            z1 = fr["f"][1]["z"].num(-1);
        }
        g_customMap = CustomMap();
        const int b0 = n == 2 ? VZ0 + a.height[s[0][1]][s[0][0]] : -2, b1 = n == 2 ? VZ0 + a.height[s[1][1]][s[1][0]] : -2;
        T("T235", "§4 бой в кубах: поставленные в дом начинают на полу, а не на крыше; бой до конца",
          parsed && z0 == b0 && z1 == b1 && res >= 0 && log.size() > 2, fmt("клетки (%d,%d) и (%d,%d), разбор %d %s, z %d/%d при полу %d/%d, итог %d, кадров %zu",
              s[0][0], s[0][1], s[1][0], s[1][1], parsed, err.c_str(), z0, z1, b0, b1, res, log.size()));
    }
    // T236: несколько боёв в кубах подряд в одном потоке (как в сервере игры): генератор следующего боя ищет места по слоям,
    // а не по кубам прошлого боя (раньше второй бой зависал в расстановке)
    if (TB()) {
        const MapStyle keep = g_style;
        int done = 0;
        g_voxMode = true;
        for (int k = 0; k < 3; k++) {
            g_style = MapStyle{};
            g_style.worldSeed = 1 + k;
            g_style.fixedW = g_style.fixedH = 20;
            std::vector<std::string> log;
            const int r = fight(plain(40, 1, 6, 5), plain(40, 1, 6, 5), &log);
            done += r >= 0 && log.size() > 2;
        }
        g_voxMode = false;
        g_style = keep;
        T("T236", "§4 бои в кубах подряд в одном потоке: каждый расставляется по своей карте и идёт до конца", done == 3, fmt("боёв до конца %d из 3", done));
    }
    // T237: рельеф шагом 5 фт в кубах: земля столбца = VZ0 + высота клетки (то же берёт 3D-вид из карты); на склонах обрыв
    // от 15 фт там, где ступени по 10 фт отличаются не больше чем на одну, — редкость (меньше 0,1 % пар соседних клеток)
    if (TB()) {
        static World w;
        static VoxWorld v;
        volArena(w, 7, 100, 300, 300, -1);
        voxFromWorld(v, w);
        int cols = 0, colOk = 0;
        long pairs = 0, steep = 0;
        for (int y = 0; y < w.ah; y++)
            for (int x = 0; x < w.aw; x++) {
                if (!w.wall[y][x] && !w.tree[y][x] && !w.pit[y][x] && !w.climb[y][x] && !w.deck[y][x] && !w.door[y][x] && !w.vxo.count(y * W + x)) {
                    const int g = VZ0 + w.height[y][x];
                    cols++;
                    colOk += MAT[v.at(x, y, g - 1)].solid || MAT[v.at(x, y, g - 1)].liquid ? !MAT[v.at(x, y, g)].solid : 0;
                }
                const wg::Cell a = wg::worldCell(g_wplan, w.wx + x, w.wy + y);
                for (int k = 0; k < 2; k++) {
                    const int nx = x + (k == 0), ny = y + (k == 1);
                    if (nx >= w.aw || ny >= w.ah) continue;
                    const wg::Cell c = wg::worldCell(g_wplan, w.wx + nx, w.wy + ny);
                    pairs++;
                    steep += std::abs(a.height - c.height) <= 1 && std::abs(w.height[y][x] - w.height[ny][nx]) >= 3;
                }
            }
        T("T237", "§4 рельеф 5 фт в кубах: земля столбца по высоте клетки, лишних обрывов на склонах почти нет",
          cols > 5000 && colOk == cols && steep * 1000 < pairs, fmt("столбцов %d (земля на месте %d), пар %ld, обрывов на склоне %ld", cols, colOk, pairs, steep));
    }
