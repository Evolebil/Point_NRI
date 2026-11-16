// Проверки движка, часть 19 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== генератор мира: высота земли арены шагом 5 фт (wg::fineHeight) =====
    // T229: levelOf5 / 2 == levelOf на всём диапазоне шума; на участке мира по зерну высоты арены — внутри своей ступени по
    // 10 фт (2·lv или 2·lv + 1), есть и полшага (нечётные уровни), вода ровная (2·lv), повтор того же участка — те же высоты;
    // участок кампании по местности (regionArena) — ровно 2·lv, как прежде
    if (TB()) {
        bool lvOk = true;
        for (int i = 0; i <= 3000; i++) {
            const double h = i / 2000.0;
            lvOk = lvOk && wg::levelOf5(h) / 2 == wg::levelOf(h);
        }
        const MapStyle keep = g_style;
        g_style = MapStyle{};
        g_style.worldSeed = 777;
        g_style.worldSize = 1000;
        g_style.fixedW = g_style.fixedH = 60;
        g_style.wx = 400;
        g_style.wy = 520;
        static World a, b;
        genMap(a);
        genMap(b);
        int odd = 0, bad = 0, wetOdd = 0, diff = 0, n = 0;
        for (int y = 0; y < a.ah; y++)
            for (int x = 0; x < a.aw; x++) {
                const wg::Cell c = wg::worldCell(g_wplan, a.wx + x * a.wstep, a.wy + y * a.wstep);
                const int h = a.height[y][x];
                n++;
                if (a.height[y][x] != b.height[y][x]) diff++;
                if (h != 2 * c.height && h != 2 * c.height + 1) bad++;
                if (h & 1) odd++;
                if (c.water && (h & 1)) wetOdd++;
            }
        g_style = keep;
        wg::RegionDesc d;
        d.terrain = "Холмы";
        d.level = 4;
        d.seed = "t229";
        static World r;
        regionArena(r, d, 50, 50);
        int rOdd = 0;
        for (int y = 0; y < r.ah; y++)
            for (int x = 0; x < r.aw; x++) rOdd += r.height[y][x] & 1;
        T("T229", "§7 генератор: высота арены шагом 5 фт внутри своей ступени 10 фт, вода ровная, повтор тот же, участок кампании — как прежде",
          lvOk && n > 1000 && bad == 0 && odd > 20 && odd < n && wetOdd == 0 && diff == 0 && rOdd == 0,
          fmt("levelOf5 %d, клеток %d, вне ступени %d, полшага %d, вода с полшагом %d, отличий при повторе %d, участок кампании с полшагом %d",
              lvOk, n, bad, odd, wetOdd, diff, rOdd));
    }
