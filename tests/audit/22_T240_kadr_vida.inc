// Проверки движка, часть 22 (фрагмент тела main в tests/audit_tests.cc: общие лямбды и счётчики — там)
    // ===== этап 5 «Красота»: в кадре режима кубов — поля только для вида (тип существа, полёт, свет) =====
    // T240: fighterJson в бою по кубам пишет "ct" (тип существа 0–13), "fl" у летуна, "lt" (радиус света, фт) у светящегося
    // (свой свет или свет способности); в бою по слоям этих полей нет — кадры и сверка боёв прежние
    if (TB()) {
        static World w;
        static VoxWorld vw;
        flatVox(w, vw, [&](World&) {});
        Boss ab = plain(50, 1, 6, 5), fb = ab;
        ab.ctype = 13;
        fb.ctype = 1;
        fb.fly = true;
        fb.lightR = 20;
        w.f.push_back(mk(ab, 0, 2, 10));
        w.f.push_back(mk(fb, 1, 5, 10));
        w.f.push_back(mk(plain(50, 1, 6, 5), 0, 8, 10));
        for (auto& f : w.f) f.zCol = f.y * W + f.x, f.z = VZ0 + 4;
        w.f[2].lightRounds = 3, w.f[2].lightR = 30;
        const std::string a = fighterJson(w.f[0], 0), b = fighterJson(w.f[1], 1), c = fighterJson(w.f[2], 0);
        const bool vox = a.find("\"ct\":13") != std::string::npos && a.find("\"fl\"") == std::string::npos && a.find("\"lt\"") == std::string::npos &&
                         b.find("\"ct\":1,\"fl\":1,\"lt\":20}") != std::string::npos && c.find("\"ct\":9") != std::string::npos && c.find("\"lt\":30") != std::string::npos;
        w.vx = nullptr;
        const std::string la = fighterJson(w.f[0], 0), lb = fighterJson(w.f[1], 1);
        const bool layers = la.find("\"ct\"") == std::string::npos && lb.find("\"ct\"") == std::string::npos && lb.find("\"lt\"") == std::string::npos;
        g_curW = nullptr;
        T("T240", "§5 кадр по кубам: тип существа, полёт и свет бойца — для вида; по слоям — без них",
          vox && layers, fmt("кубы %d (%s | %s | %s), слои %d", vox, a.c_str(), b.c_str(), c.c_str(), layers));
    }
