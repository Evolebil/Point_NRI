// Проверки движка, часть 13 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== 3D, шаг высоты 5 фт (ветка 3d, решения 16–17 в docs/TZ_3d.md) =====
    // T212: уровни высоты по 5 фт и деревья своей высоты (10–40 фт): расстояние, подъём и обрыв, цена подъёма, дерево —
    // залезть и обзор поверх него, падение 1к6 за 10 фт, полёт шагом 5 фт, старые карты (по 10 фт) и новый формат "hz":5
    if (TB()) {
        static World w;
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
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        // уступы на строке 5: x = 1 — 5 фт, x = 2 — 10 фт, x = 3 — 25 фт (обрыв)
        w.height[5][1] = 1;
        w.height[5][2] = 2;
        w.height[5][3] = 5;
        initObjects(w);
        Fighter P = mk(plain(100, 0, 0, 5), 0, 0, 5);
        // 1. расстояние: 5 фт за уровень высоты (PHB, по наибольшему смещению)
        const bool dist = dist3(0, 0, 0, 1, 0, 1) == 5 && dist3(0, 0, 0, 1, 0, 4) == 20 && cellDist(&w, 0, 5, 2, 5) == 10;
        // 2. уступ до 10 фт — подъём, выше — обрыв
        const bool steps = stepOK(w, P, 0, 5, 0, 1, 5, 0) && stepOK(w, P, 0, 5, 0, 2, 5, 0) && !stepOK(w, P, 2, 5, 0, 3, 5, 0);
        // 3. подъём: +5 фт движения за каждые (начатые) 10 фт
        const bool cost = stepCost(w, P, 0, 5, 1, 5) == 10 && stepCost(w, P, 0, 5, 2, 5) == 10 && climbFt(3) == 10 && climbFt(0) == 0;
        // 4. дерево своей высоты: стоят на вершине, залезть с земли можно на любое (10–40 фт)
        w.tree[10][10] = TREE_MAX;
        w.tree[10][12] = TREE_MIN;
        initObjects(w);
        const bool treeTop = zAt(w, 10, 10) == 8 && zAt(w, 12, 10) == 2 && stepOK(w, P, 9, 10, 0, 10, 10, 0) && stepOK(w, P, 11, 10, 0, 12, 10, 0);
        // 5. обзор поверх дерева: со скал в 30 фт по обе стороны дерево в 40 фт закрывает, в 20 фт — нет
        for (int y = 18; y <= 22; y++) {
            w.height[y][14] = w.height[y][18] = 6;
            w.tree[y][16] = 0;
        }
        w.tree[20][16] = TREE_MAX;
        const bool tallBlocks = !losZ(w, 14, 20, 6, 18, 20, 6);
        w.tree[20][16] = TREE_DEF;
        const bool lowSeen = losZ(w, 14, 20, 6, 18, 20, 6);
        // 6. падение: 1к6 за каждые 10 фт (5 фт — без урона), не больше 20к6
        const bool fall = fallDice(1) == 0 && fallDice(2) == 1 && fallDice(5) == 2 && fallDice(100) == 20;
        // 7. полёт шагом 5 фт: подняться на 5 фт — 5 фт движения
        Boss fb = plain(200, 1, 6, 5);
        fb.fly = true;
        w.f.push_back(mk(fb, 0, 25, 25));
        g_curW = &w;
        TurnState ts;
        ts.speed = 30;
        std::string why;
        const bool fly5 = applyPrim(w, 0, ts, Prim{PR_ALT, 1}, true, &why) && w.f[0].alt == 1 && ts.speed == 25 && maxAltAt(w, w.f[0], 25, 25, 0) == ALT_MAX && ALT_MAX == 12;
        g_curW = nullptr;
        // 8. старая карта (уровни по 10 фт, дерево «1») переводится: ×2, дерево — 20 фт
        bool okJ = true;
        CustomMap cm;
        std::string err;
        const std::string rows5 = "[\"00000\",\"00000\",\"00000\",\"00000\",\"00000\"]";
        auto mapJ = [&](const char* hz, const char* h, const char* t, const char* d) {
            return std::string("{\"aw\":5,\"ah\":5") + hz + ",\"height\":[\"" + h + "\",\"00000\",\"00000\",\"00000\",\"00000\"],\"tree\":[\"" + t +
                   "\",\"00000\",\"00000\",\"00000\",\"00000\"],\"deck\":[\"" + d + "\",\"00000\",\"00000\",\"00000\",\"00000\"],\"wall\":" + rows5 + "}";
        };
        static World mw;
        const bool oldOk = parseCustomMap(parseJson(mapJ("", "12000", "00010", "00000"), &okJ), cm, err);
        if (oldOk) applyCustomMap(mw, cm);
        const bool oldMap = oldOk && mw.height[0][0] == 2 && mw.height[0][1] == 4 && mw.tree[0][3] == TREE_DEF;
        // 9. новый формат "hz":5: уровни буквами (настил на 55 фт — «b»), дерево своей высоты (за пределами 10–40 фт — к пределу), запись обратно
        const bool newOk = parseCustomMap(parseJson(mapJ(",\"hz\":5", "12000", "00809", "000b0"), &okJ), cm, err);
        if (newOk) applyCustomMap(mw, cm);
        const std::string back = newOk ? mapToJson(mw) : "";
        const bool newMap = newOk && mw.height[0][0] == 1 && mw.height[0][1] == 2 && mw.deck[0][3] == 11 && mw.tree[0][2] == 8 && mw.tree[0][4] == TREE_MAX &&
                            back.find("\"hz\":5") != std::string::npos && back.find("\"000b0") != std::string::npos;
        T("T212", "§7 высота по 5 фт: расстояние, уступ 10 фт и обрыв, цена подъёма, дерево 10–40 фт (залезть, обзор), падение, полёт 5 фт, карты старые и \"hz\":5",
          dist && steps && cost && treeTop && tallBlocks && lowSeen && fall && fly5 && oldMap && newMap,
          fmt("расстояние %d, уступы %d, цена подъёма %d (5 фт: %d, 10 фт: %d), дерево %d, обзор: 40 фт закрывает %d, 20 фт нет %d, падение %d, полёт %d (осталось %d), старая карта %d, новая %d %s",
              dist, steps, cost, stepCost(w, P, 0, 5, 1, 5), stepCost(w, P, 0, 5, 2, 5), treeTop, tallBlocks, lowSeen, fall, fly5, ts.speed, oldMap, newMap, err.c_str()));
    }
    // T216: генератор мира ставит деревья разной высоты (10–40 фт, по зерну и месту клетки): в лесу встречаются и низкие, и
    // высокие, все в пределах, тот же вход — те же высоты
    if (TB()) {
        wg::RegionDesc d;
        d.terrain = "Лес";
        d.level = 3;
        d.seed = "x5y9";
        static World a, b;
        regionArena(a, d, 120, 120);
        regionArena(b, d, 120, 120);
        int n = 0, lo = 99, hi = 0, bad = 0, diff = 0;
        for (int y = 0; y < a.ah; y++)
            for (int x = 0; x < a.aw; x++) {
                if (a.tree[y][x] != b.tree[y][x]) diff++;
                if (!a.tree[y][x]) continue;
                n++;
                lo = std::min(lo, (int)a.tree[y][x]);
                hi = std::max(hi, (int)a.tree[y][x]);
                if (a.tree[y][x] < TREE_MIN || a.tree[y][x] > TREE_MAX) bad++;
            }
        T("T216", "§7 генератор: деревья разной высоты 10–40 фт (по зерну и месту), повтор даёт те же высоты",
          n > 50 && bad == 0 && hi - lo >= 3 && diff == 0,
          fmt("деревьев %d, высоты %d–%d фт, вне 10–40 фт %d, отличий при повторе %d", n, 5 * lo, 5 * hi, bad, diff));
    }
    // T217: объёмный мир (этап 1б): перевод карты в кубы, хиты и КД — у материала, повреждённый куб — в разреженном списке,
    // разрушенный — воздух и событие для кадра; неразрушимое, яд, номера кубов через границы чанков
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.strong, 0, sizeof w.strong);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.water, 0, sizeof w.water);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = 40;
        w.ah = 20;
        w.theme = 0;
        w.dyn.clear();
        w.wall[2][2] = true;
        w.wall[2][3] = true;
        w.tree[5][5] = TREE_MAX;
        w.deck[8][8] = 6;
        w.pit[10][10] = true;
        w.water[12][12] = true;
        w.height[14][14] = 3;
        w.dyn.push_back({3, 2, 4, -1});   // стена (3, 2) разрушена — обломки
        static VoxWorld v;
        voxFromWorld(v, w);
        // 1. перевод: земля, стена 30 фт, дерево 40 фт, настил на 30 фт, ущелье, вода, уступ 15 фт, обломки
        const bool conv = v.at(0, 0, 0) == VX_BEDROCK && v.at(0, 0, VZ0 - 1) == VX_GRASS && v.at(0, 0, VZ0) == VX_AIR &&
                          v.at(2, 2, VZ0) == VX_STONE && v.at(2, 2, VZ0 + 5) == VX_STONE && v.at(2, 2, VZ0 + 6) == VX_AIR &&
                          v.at(5, 5, VZ0 + 5) == VX_WOOD && v.at(5, 5, VZ0 + 7) == VX_LEAF && v.at(5, 5, VZ0 + 8) == VX_AIR &&
                          v.at(8, 8, VZ0 + 5) == VX_PLANK && v.at(8, 8, VZ0) == VX_AIR && v.at(10, 10, VZ0 - 1) == VX_AIR &&
                          v.at(10, 10, 0) == VX_BEDROCK && v.at(12, 12, VZ0 - 1) == VX_WATER && v.at(14, 14, VZ0 + 2) == VX_GRASS &&
                          v.at(14, 14, VZ0 + 3) == VX_AIR && v.at(3, 2, VZ0) == VX_AIR && v.at(3, 2, VZ0 - 1) == VX_RUBBLE;
        // 2. хиты и КД — из таблицы материала, повреждённых нет
        const bool table = v.hpAt(2, 2, VZ0) == OBJ_WALL_HP && v.acAt(2, 2, VZ0) == 17 && v.acAt(5, 5, VZ0) == 15 && v.dmg.empty() && v.changes.empty();
        // 3. удар по кубу стены: в списке один этот куб, соседний — целый, материал тот же
        const unsigned ver0 = v.chunkVer[(size_t)(v.idx(2, 2, VZ0) / (VCH * VCH * VCH))];
        const int r1 = damageVox(v, 2, 2, VZ0, 10, DT_BLUDGEONING);
        const bool sparse = r1 == 1 && v.hpAt(2, 2, VZ0) == OBJ_WALL_HP - 10 && v.dmg.size() == 1 && v.hpAt(2, 2, VZ0 + 1) == OBJ_WALL_HP &&
                            v.at(2, 2, VZ0) == VX_STONE && v.changes.empty();
        // 4. добили: воздух, из списка ушёл, событие для кадра, версия чанка выросла
        const int r2 = damageVox(v, 2, 2, VZ0, 100, DT_FIRE);
        const bool kill = r2 == 2 && v.at(2, 2, VZ0) == VX_AIR && v.dmg.empty() && v.changes.size() == 1 && v.changes[0][2] == VZ0 &&
                          v.changes[0][3] == VX_AIR && v.chunkVer[(size_t)(v.idx(2, 2, VZ0) / (VCH * VCH * VCH))] > ver0;
        // 5. неразрушимое (дно мира, вода) и яд с психическим уроном — без эффекта
        const bool immune = damageVox(v, 0, 0, 0, 999, DT_FIRE) == 0 && damageVox(v, 12, 12, VZ0 - 1, 999, DT_FIRE) == 0 &&
                            damageVox(v, 2, 2, VZ0 + 1, 999, DT_POISON) == 0 && damageVox(v, 2, 2, VZ0 + 1, 999, DT_PSYCHIC) == 0 && v.at(2, 2, VZ0 + 1) == VX_STONE;
        // 6. номера кубов различны и через границы чанков (мир 40×20×высота)
        std::vector<char> seen(v.mat.size(), 0);
        bool uniq = true;
        for (int z = 0; z < v.sz && uniq; z++)
            for (int y = 0; y < v.sy && uniq; y++)
                for (int x = 0; x < v.sx; x++) {
                    const int i = v.idx(x, y, z);
                    if (i < 0 || (size_t)i >= seen.size() || seen[(size_t)i]) { uniq = false; break; }
                    seen[(size_t)i] = 1;
                }
        // 7. много ударов по разным кубам: в списке ровно повреждённые, добитые оттуда уходят
        int hurtN = 0;
        for (int x = 0; x < 40; x++) hurtN += damageVox(v, x, 0, 1, 5, DT_SLASHING) == 1;
        for (int x = 0; x < 20; x++) damageVox(v, x, 0, 1, 100, DT_SLASHING);
        const bool many = hurtN == 40 && v.dmg.size() == 20 && v.at(5, 0, 1) == VX_AIR && v.at(25, 0, 1) == VX_GRASS;   // на ровной земле куб z = 1 — верхний, трава
        T("T217", "§1б объёмный мир: перевод карты в кубы, хиты и КД у материала, повреждённые — разреженно, разрушенный — воздух и событие",
          conv && table && sparse && kill && immune && uniq && many,
          fmt("перевод %d, таблица %d, разреженно %d, разрушение %d, неразрушимое и яд %d, номера %d, много ударов %d (в списке %d)",
              conv, table, sparse, kill, immune, uniq, many, (int)v.dmg.size()));
    }
    // T218: режим кубов (этап 1б): шаг и путь по узлам-кубам (уступ 10 фт — да, 15 — обрыв), обзор лучом (стена закрывает,
    // листва — нет), копатель роет ход сквозь землю, разбить блок действием (стена кончилась — нет и в слоях), падение при
    // разрушенном поле (1к6 за 10 фт), летун — вверх по кубу, застрявший в твёрдом встаёт наверх без урона
    if (TB()) {
        static World w;
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
            for (int x = 0; x < 20; x++) w.height[y][x] = 4;   // земля 20 фт: под ней есть где рыть
        for (int y = 0; y < 20; y++) w.wall[y][10] = true;   // стена 30 фт через всю арену по x = 10
        w.height[2][2] = 6;   // уступ 10 фт
        w.height[2][4] = 7;   // уступ 15 фт рядом с ровным (3, 2)
        const int G = VZ0 + 4;   // куб ног на ровной земле
        w.tree[15][5] = TREE_DEF;
        initObjects(w);
        static VoxWorld vw;
        voxFromWorld(vw, w);
        w.vx = &vw;
        g_curW = &w;
        Boss pb = plain(100, 1, 6, 5), bb = pb, fb = pb;
        bb.burrowS = true;
        fb.fly = true;
        w.f.push_back(mk(pb, 0, 1, 2));   // пеший
        w.f.push_back(mk(pb, 1, 18, 18));   // враг за стеной
        for (auto& f : w.f) f.zCol = -1, f.z = 0;
        vxFixAll(w);
        Fighter& P = w.f[0];
        // 1. шаг: на уступ 10 фт — да (+5 фт), с ровного на 15 фт — обрыв; узел ног — верх земли
        const bool step = P.z == G && stepOK(w, P, 1, 2, G, 2, 2, G + 2) && stepCostL(w, P, 1, 2, G, 2, 2, G + 2) == 10 &&
                          !stepOK(w, P, 3, 2, G, 4, 2, G + 3);
        // 2. путь пешего к врагу за стеной во всю длину — не найден: идёт к стене как можно ближе
        std::vector<PStep> path;
        pathTo(w, P, 18, 18, path);
        const bool wallPath = !path.empty() && path.back().first < 10;
        // 3. обзор: стена закрывает, сквозь листву видно, над стеной (с высоты) — видно
        const bool losWall = !losF(w, P, w.f[1]);
        const bool losLeaf = losZ(w, 4, 15, 6, 6, 15, 6) && losZ(w, 8, 5, 10, 12, 5, 10);   // глаза в кроне; над стеной
        // 4. копатель: путь под стеной — роет ход (кубы земли → воздух, события vox в правках)
        Fighter B = mk(bb, 0, 9, 5);
        B.z = 2;   // в земле под поверхностью
        std::vector<PStep> bp;
        pathTo(w, B, 11, 5, bp);
        bool under = !bp.empty() && bp.back().first == 11;
        for (auto& c : bp) under = under && c.l < G;   // весь ход — ниже поверхности, сквозь землю под стеной
        B.x = 10; B.y = 5; B.z = 2;
        const size_t ed0 = vw.edits.size();
        vxDig(w, B);
        const bool dug = vw.at(10, 5, 2) == VX_AIR && vw.at(10, 5, 3) == VX_AIR && vw.edits.size() == ed0 + 2 && vw.at(10, 5, G) == VX_STONE;
        // 5. разбить блок: все кубы стены в клетке (10, 0) — объекта больше нет и в слоях
        for (int z = G; z < G + WALL_UP; z++) damageVox(vw, 10, 0, z, 1000, DT_BLUDGEONING);
        vxAfterBreak(w, 10, 0);
        const bool broke = !w.wall[0][10] && vw.at(10, 0, G) == VX_AIR && w.wall[1][10];
        // 6. пол разрушен под пешим (1, 2): два куба вниз — падение на 10 фт, 1к6, сбит с ног
        const int hp0 = P.hp;
        damageVox(vw, 1, 2, G - 1, 1000, DT_BLUDGEONING);
        damageVox(vw, 1, 2, G - 2, 1000, DT_BLUDGEONING);
        vxFixAll(w);
        const bool fell = P.z == G - 2 && P.hp < hp0 && P.hp >= hp0 - 6 && P.cond[C_PRONE] > 0;
        // 7. летун: вверх по кубу в своей клетке; пешему — нельзя
        Fighter Fl = mk(fb, 0, 15, 10);
        Fl.z = G;
        const bool flyUp = stepOK(w, Fl, 15, 10, G, 15, 10, G + 1) && !stepOK(w, w.f[1], 18, 18, G, 18, 18, G + 1);
        // 8. застрял в стене (перенесён в ту же клетку) — встаёт на верх стены без урона
        w.f[1].x = 10; w.f[1].y = 10; w.f[1].z = G + 1; w.f[1].zCol = 10 * W + 10;
        const int hp1 = w.f[1].hp;
        vxFixAll(w);
        const bool snap = w.f[1].z == G + WALL_UP && w.f[1].hp == hp1;
        g_curW = nullptr;
        w.vx = nullptr;
        T("T218", "§1б режим кубов: шаг и путь по кубам, обзор лучом, копатель роет, блок разрушается действием, падение, летун, застрявший встаёт наверх",
          step && wallPath && losWall && losLeaf && under && dug && broke && fell && flyUp && snap,
          fmt("шаг %d, путь у стены %d, обзор: стена %d, листва и над стеной %d, копатель под стеной %d, ход %d, блок %d, падение %d (−%d), летун %d, наверх %d",
              step, wallPath, losWall, losLeaf, under, dug, broke, fell, hp0 - P.hp, flyUp, snap));
    }
