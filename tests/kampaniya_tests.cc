// Проверки кампании (исследование и социалка, ТЗ docs/TZ_issledovanie_socialka.md, номера T210+).
// Сборка и запуск: make test_kampaniya. Боевых проверок (tests/audit_tests.cc) не трогает.
#include "engine.h"
#include "char_spec.h"
#include "kmp_events.h"
#include <chrono>
#include <fstream>
#include <sstream>

static int g_pass = 0, g_fail = 0;
static void T(const char* id, bool ok, const std::string& what) {
    std::printf("%s %s: %s\n", ok ? "PASS" : "FAIL", id, what.c_str());
    (ok ? g_pass : g_fail)++;
}

static Coef C;
// герой: СИЛ 10, ЛОВ 14, ТЕЛ 12, ИНТ 10, МДР 14, ХАР 16, мастерство +3; Выживание (13) и Убеждение (17) с владением,
// Убеждение с экспертизой, Внимательность (12) с владением; воровские инструменты
static const char* HERO =
    "{\"name\":\"Лис\",\"budget\":800,\"prof\":3,\"stats\":[10,14,12,10,14,16],\"passives\":{\"skills\":[12,13,17],\"expert\":[17],"
    "\"saveProf\":[2],\"tools\":[\"воровские\"]}}";

static kmp::State fresh(uint64_t seed = 42) {
    kmp::State s;
    s.seed = seed;
    std::string err;
    Json ev = parseJson(std::string("{\"n\":1,\"type\":\"hero\",\"name\":\"Лис\",\"char\":") + HERO + ",\"purse\":\"10 зм\"}");
    if (!kmp::apply(s, C, ev, err)) std::printf("ошибка героя: %s\n", err.c_str());
    ev = parseJson(std::string("{\"n\":2,\"type\":\"hero\",\"name\":\"Брат\",\"char\":") + HERO + "}");
    kmp::apply(s, C, ev, err);
    return s;
}
// черновик → принять (как мастер)
static Json run(kmp::State& s, const kmp::Content& ct, const std::string& act, int attempt = 0) {
    std::string err;
    const std::string ev = kmp::propose(s, ct, parseJson(act), "мастер", attempt, err);
    if (ev.empty()) {
        std::printf("ошибка черновика: %s\n", err.c_str());
        return Json();
    }
    Json j = parseJson(ev);
    if (!kmp::apply(s, C, j, err)) std::printf("ошибка применения: %s\n", err.c_str());
    return j;
}

int main() {
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    kmp::Content ct;

    // T210 деньги
    T("T210", kmp::parseMoney("1 зм 5 см 3 мм") == 153 && kmp::money(153) == "1 зм 5 см 3 мм" && kmp::parseMoney("100 зм") == 10000, "деньги: зм/см/мм ↔ медные");
    // T211 уровень по бюджету: 200 → 1, 216 → 2, 400 → 10, 8600 → 50
    T("T211", kmp::levelOf(200) == 1 && kmp::levelOf(400) == 10 && kmp::levelOf(398) == 9 && kmp::levelOf(8600) == 50 && kmp::levelOf(8599) == 49 &&
                  kmp::levelOf(215) == 1 && kmp::levelOf(216) == 2,
      "уровень по кривой ×1,08 (50-й — 8600)");

    kmp::State s = fresh();
    const kmp::Hero& h = s.heroes[0];
    // T212 модификаторы: Убеждение = ХАР 3 + 3 + 3 (экспертиза); Выживание = МДР 2 + 3; Атлетика = СИЛ 0
    T("T212", kmp::heroSkill(h.b, SK_PERSUADE) == 9 && kmp::heroSkill(h.b, SK_SURVIVAL) == 5 && kmp::heroSkill(h.b, SK_ATHL) == 0,
      "навык: характеристика + владение + экспертиза");
    // T213 пассивная Внимательность 10 + 2 + 3 = 15; с истощением 1 — помеха, −5
    {
        kmp::Hero x = h;
        const int p0 = kmp::passive(x, "Внимательность");
        x.exh = 1;
        T("T213", p0 == 15 && kmp::passive(x, "Внимательность") == 10, "пассивная проверка, истощение — помеха");
    }
    // T214 инструмент: владение — +мастерство, без владения — 0
    {
        int m1 = 0, m2 = 0;
        bool a = false, d = false;
        std::string p, e;
        kmp::checkMod(h, "инструмент:воровские", m1, a, d, p, e);
        kmp::checkMod(h, "инструмент:травника", m2, a, d, p, e);
        T("T214", m1 == 3 && m2 == 0, "проверка инструментом по названию");
    }
    // T215 проверка: итог = к20 + модификатор, успех против СЛ
    {
        Json ev = run(s, ct, "{\"type\":\"check\",\"hero\":\"Лис\",\"what\":\"Убеждение\",\"dc\":15}");
        const Json& c = ev["checks"][0];
        T("T215", c["total"].num() == c["die"].num() + 9 && c["ok"].flag() == (c["total"].num() >= 15) && ev["res"]["ok"].flag() == c["ok"].flag(),
          "проверка навыка против СЛ");
    }
    // T216 путь: пеший по дороге 35 км, по лесу без дороги ~17 (канон)
    {
        kmp::State a = fresh();
        a.food = 100, a.water = 100, a.purse = 100000;
        Json r1 = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\",\"road\":true}");
        Json r2 = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\",\"road\":false}");
        Json r3 = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\",\"road\":false,\"pace\":\"быстрый\"}");
        auto want = [](const Json& r, double km) { return r["res"]["km"].num() == std::llround(r["res"]["lost"].flag() ? km / 2 : km); };   // сбились — половина
        T("T216", r1["res"]["km"].num() == 35 && want(r2, 17.5) && want(r3, 21.875) && a.day == 4, "км в день: путник × местность × темп");
    }
    // T217–T219 встречи: порог местности; на дороге +2; обратный путь −4 и засада на к12 1–3
    {
        kmp::Content c2;
        c2.table["Болото"] = std::vector<std::string>(12, "встреча");
        c2.ambush["Болото"] = "гать — булливаги";
        int thrOff = 0, thrRoad = 0, thrBack = 0, amb = 0, ambBad = 0, hits = 0;
        for (int sd = 1; sd <= 300; sd++) {
            kmp::State a = fresh(sd);
            a.food = a.water = 100;
            Json r = run(a, c2, "{\"type\":\"day\",\"terrain\":\"Болото\"}");
            thrOff = r["res"]["enc"]["thr"].num();
            hits += r["res"]["enc"]["hit"].flag() != (r["res"]["enc"]["d20"].num() >= thrOff);
            thrRoad = run(a, c2, "{\"type\":\"day\",\"terrain\":\"Болото\",\"road\":true}")["res"]["enc"]["thr"].num();
            a.returnTrip = true;
            Json b = run(a, c2, "{\"type\":\"day\",\"terrain\":\"Болото\"}");
            thrBack = b["res"]["enc"]["thr"].num();
            const bool am = b["res"]["enc"]["ambush"].flag();
            amb += am;
            ambBad += am != (b["res"]["enc"]["hit"].flag() && b["res"]["enc"]["d12"].num() <= 3);
            if (am && b["res"]["enc"]["text"].str() != "гать — булливаги") ambBad++;
        }
        T("T217", thrOff == 13 && hits == 0, "встреча: к20 ≥ порога местности (болото 13)");
        T("T218", thrRoad == 15, "на дороге порог +2");
        T("T219", thrBack == 9 && amb > 0 && ambBad == 0, "обратный путь: порог −4, засада на к12 1–3 с текстом таблицы");
    }
    // T220 отдых: долгий — все ХП, половина костей хитов, −1 истощение; короткий — кости хитов к8 + ТЕЛ
    {
        kmp::State a = fresh();
        kmp::Hero& x = a.heroes[0];
        x.hp = 1, x.hd = 0, x.exh = 2;
        run(a, ct, "{\"type\":\"rest\",\"kind\":\"долгий\"}");
        const bool lng = x.hp == x.hpMax && x.hd == std::min(x.hdMax, std::max(1, x.hdMax / 2)) && x.exh == 1;
        x.hp = 1;
        const int hd0 = x.hd;
        Json r = run(a, ct, "{\"type\":\"rest\",\"kind\":\"короткий\",\"spend\":{\"Лис\":1}}");
        T("T220", lng && x.hd == hd0 - 1 && x.hp > 1 && x.hp <= 1 + 8 + 1 && r["res"]["hours"].num() == 1, "отдых: долгий и короткий");
    }
    // T221 голод: без еды 3 + ТЕЛ (ТЕЛ 12 → 4) дня без истощения, на 5-й — уровень
    {
        kmp::State a = fresh();
        a.water = 1000, a.purse = 100000;
        for (int d = 0; d < 4; d++) run(a, ct, "{\"type\":\"day\",\"terrain\":\"Город\",\"road\":true}");
        const int e4 = a.heroes[0].exh;
        run(a, ct, "{\"type\":\"day\",\"terrain\":\"Город\",\"road\":true}");
        T("T221", e4 == 0 && a.heroes[0].exh == 1 && a.heroes[0].daysNoFood == 5, "голод: 3 + ТЕЛ дней, потом истощение");
    }
    // T222 истощение 6 — смерть
    {
        kmp::State a = fresh();
        run(a, ct, "{\"type\":\"gm\",\"hero\":\"Лис\",\"exh\":6}");
        T("T222", a.heroes[0].dead, "истощение 6 — смерть");
    }
    // T223 траты на жизнь: скромный — 1 зм на героя в день
    {
        kmp::State a = fresh();
        a.food = a.water = 100, a.purse = 500;
        run(a, ct, "{\"type\":\"day\",\"terrain\":\"Город\",\"road\":true}");
        T("T223", a.purse == 300, "траты на жизнь каждый день");
    }
    // T224 роли: проводник бросает навигацию; без проводника — лучший по Выживанию с помехой; добытчик — пропитание
    {
        kmp::State a = fresh();
        a.food = a.water = 100;
        Json r = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\",\"roles\":{\"Брат\":\"проводник\",\"Лис\":\"добытчик\"}}");
        bool guide = false, forage = false;
        for (auto& c : r["checks"].a) {
            if (c["who"].str() == "Брат" && c["what"].str() == "Выживание" && c["dc"].num() == 15 && !c["dis"].flag()) guide = true;
            if (c["who"].str() == "Лис" && c["what"].str() == "Выживание" && c["dc"].num() == 10) forage = true;
        }
        Json r2 = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\"}");
        T("T224", guide && forage && r2["checks"][0]["dis"].flag() && r["res"]["nav"]["by"].str() == "Брат", "роли в пути: проводник и добытчик");
    }
    // T225 дозор: лучшая пассивная Внимательность дозорных; быстрый темп −5
    {
        kmp::State a = fresh();
        a.food = a.water = 100;
        Json r = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\",\"road\":true,\"roles\":{\"Лис\":\"дозорный\"}}");
        Json r2 = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Лес\",\"road\":true,\"pace\":\"быстрый\",\"roles\":{\"Лис\":\"дозорный\"}}");
        T("T225", r["res"]["lookout"].num() == 15 && r2["res"]["lookout"].num() == 10, "дозорный: пассивная Внимательность");
    }
    // T226 каскад выплат Акта I: 1000 → 900 → 405 (подтверждение) → 810
    {
        kmp::State a = fresh();
        run(a, ct, "{\"type\":\"quest\",\"id\":\"Ролан\",\"base\":\"1000 зм\",\"step\":0.9,\"confirm\":0.5}");
        const long long p0 = a.purse;
        Json t1 = run(a, ct, "{\"type\":\"turnin\",\"quest\":\"Ролан\",\"fact\":\"аномалия\",\"party\":\"Чёрный Кабан\"}");
        Json t2 = run(a, ct, "{\"type\":\"turnin\",\"quest\":\"Ролан\",\"fact\":\"кристаллы друг над другом\",\"party\":\"Серебряные клинки\"}");
        Json t3 = run(a, ct, "{\"type\":\"turnin\",\"quest\":\"Ролан\",\"fact\":\"аномалия\"}");
        Json t4 = run(a, ct, "{\"type\":\"turnin\",\"quest\":\"Ролан\",\"fact\":\"нижний кристалл притягивает\"}");
        T("T226",
          t1["res"]["pay"].num() == 100000 && t2["res"]["pay"].num() == 90000 && t3["res"]["pay"].num() == 40500 && t3["res"]["confirm"].flag() &&
              t4["res"]["pay"].num() == 81000 && a.purse == p0 + 40500 + 81000,
          "каскад выплат за сведения");
    }
    // T227 повтор журнала: свёртка тех же событий даёт то же состояние до бита; переброс — другие кости
    {
        kmp::State a = fresh(7);
        a.food = a.water = 50;
        std::vector<std::string> jr;
        std::string err;
        for (const char* act : {"{\"type\":\"setup\",\"purse\":\"50 зм\",\"food\":30,\"water\":30}", "{\"type\":\"day\",\"terrain\":\"Горы\",\"roles\":{\"Лис\":\"проводник\"}}",
                                "{\"type\":\"day\",\"terrain\":\"Болото\",\"hours\":10}", "{\"type\":\"rest\",\"kind\":\"короткий\",\"spend\":{\"Брат\":1}}",
                                "{\"type\":\"buy\",\"item\":\"паёк\",\"qty\":5,\"price\":\"5 см\",\"haggle\":{\"hero\":\"Лис\",\"vsMod\":2}}",
                                "{\"type\":\"learn\",\"fact\":\"болото\"}"}) {
            const std::string ev = kmp::propose(a, ct, parseJson(act), "мастер", 0, err);
            jr.push_back(ev);
            kmp::apply(a, C, parseJson(ev), err);
        }
        kmp::State b = fresh(7);
        b.food = b.water = 50;
        for (auto& ev : jr) kmp::apply(b, C, parseJson(ev), err);
        const std::string d0 = kmp::propose(a, ct, parseJson("{\"type\":\"day\",\"terrain\":\"Лес\"}"), "мастер", 0, err);
        const std::string d1 = kmp::propose(a, ct, parseJson("{\"type\":\"day\",\"terrain\":\"Лес\"}"), "мастер", 0, err);
        const std::string d2 = kmp::propose(a, ct, parseJson("{\"type\":\"day\",\"terrain\":\"Лес\"}"), "мастер", 1, err);
        T("T227", kmp::stateJson(a) == kmp::stateJson(b) && d0 == d1 && d0 != d2, "повтор журнала и переброс");
    }
    // T228 торг: ±10% за каждые 5 разницы, предел ±30%, проигрыш на 10+ — торговец обижен; продажа — за половину
    {
        bool okPct = true, offendOk = true;
        for (int sd = 1; sd <= 200; sd++) {
            kmp::State a = fresh(sd);
            a.purse = 1000000;
            Json r = run(a, ct, "{\"type\":\"buy\",\"item\":\"конь\",\"price\":\"75 зм\",\"npc\":\"барышник\",\"haggle\":{\"hero\":\"Лис\",\"vsMod\":3}}");
            const int diff = r["checks"][0]["total"].num() - r["checks"][1]["total"].num();
            const int want = std::clamp((diff >= 0 ? diff / 5 : -((-diff + 4) / 5)) * 10, -30, 30);
            okPct &= r["res"]["pct"].num() == want && r["res"]["total"].num() == std::llround(7500 * (1 - want / 100.0));
            offendOk &= (diff <= -10) == (a.npcs.count("барышник") && a.npcs["барышник"].shift == -1);
        }
        kmp::State a = fresh();
        a.inv["меч"] = 1;
        run(a, ct, "{\"type\":\"sell\",\"item\":\"меч\",\"price\":\"15 зм\"}");
        T("T228", okPct && offendOk && a.purse == 750 && !a.inv.count("меч"), "торг и продажа за половину");
    }
    // T229 покупка без денег не проходит и не портит состояние
    {
        kmp::State a = fresh();
        a.purse = 10;
        std::string err;
        const std::string ev = kmp::propose(a, ct, parseJson("{\"type\":\"buy\",\"item\":\"конь\",\"price\":\"75 зм\"}"), "мастер", 0, err);
        const bool ok = kmp::apply(a, C, parseJson(ev), err);
        T("T229", !ok && a.purse == 10 && a.inv.empty(), "не хватает денег — отказ");
    }
    // T230 отношение NPC: шкала −2…+2 внутри ступени, перелив — следующая ступень
    {
        kmp::State a = fresh();
        run(a, ct, "{\"type\":\"attitude\",\"npc\":\"Ролан\",\"tier\":\"безразличный\"}");
        run(a, ct, "{\"type\":\"attitude\",\"npc\":\"Ролан\",\"delta\":3}");
        const bool up = a.npcs["Ролан"].tier == 1 && a.npcs["Ролан"].shift == -2;
        run(a, ct, "{\"type\":\"attitude\",\"npc\":\"Ролан\",\"delta\":-1}");
        T("T230", up && a.npcs["Ролан"].tier == 0 && a.npcs["Ролан"].shift == 2, "отношение: ступени и шкала");
    }
    // T231 форсированный марш: каждый час сверх 8 — спасбросок ТЕЛ СЛ 10 + час
    {
        kmp::State a = fresh();
        a.food = a.water = 100;
        Json r = run(a, ct, "{\"type\":\"day\",\"terrain\":\"Пашни и сёла\",\"road\":true,\"hours\":11}");
        int saves = 0, dcOk = 0;
        for (auto& c : r["checks"].a)
            if (c["what"].str() == "спасбросок ТЕЛ") saves++, dcOk += c["dc"].num() >= 11 && c["dc"].num() <= 13;
        T("T231", saves == 6 && dcOk == 6 && r["res"]["km"].num() == 48, "форсированный марш");
    }

    // T232 разговор: начало — отношение с карточки, действия узла (деньги, сведения, отношение) применяются
    {
        kmp::State a = fresh();
        run(a, ct, "{\"type\":\"dialog\",\"npc\":\"Ролан\",\"node\":\"приветствие\",\"attitude\":\"безразличный\"}");
        const bool start = a.dlgNpc == "Ролан" && a.dlgNode == "приветствие" && a.npcs["Ролан"].tier == 0;
        run(a, ct, "{\"type\":\"dialog\",\"node\":\"аванс\",\"do\":[{\"type\":\"gm\",\"purse\":10000},{\"type\":\"learn\",\"fact\":\"болото\"},"
                   "{\"type\":\"attitude\",\"npc\":\"Ролан\",\"delta\":1}],\"shop\":true}");
        const bool acts = a.purse == 10000 && a.facts.count("болото") && a.npcs["Ролан"].shift == 1 && a.dlgShop && a.dlgNpc == "Ролан";
        run(a, ct, "{\"type\":\"dialog\",\"node\":\"\"}");
        T("T232", start && acts && a.dlgNpc.empty() && !a.dlgShop, "разговор: узлы и действия узла");
    }
    // T233 проверка из разговора ведёт в узел успеха или провала с его действиями
    {
        int okN = 0, failN = 0, bad = 0;
        for (int sd = 1; sd <= 60; sd++) {
            kmp::State a = fresh(sd);
            run(a, ct, "{\"type\":\"dialog\",\"npc\":\"Ролан\",\"node\":\"приветствие\"}");
            Json e = run(a, ct, "{\"type\":\"check\",\"hero\":\"Лис\",\"what\":\"Убеждение\",\"dc\":20,\"dlg\":{\"ok\":\"аванс-больше\",\"okDo\":[{\"type\":\"gm\",\"purse\":5000}],"
                                "\"fail\":\"отказ\",\"failDo\":[{\"type\":\"attitude\",\"npc\":\"Ролан\",\"delta\":-1}]}}");
            const bool ok = e["res"]["ok"].flag();
            (ok ? okN : failN)++;
            if (ok ? (a.dlgNode != "аванс-больше" || a.purse != 5000) : (a.dlgNode != "отказ" || a.npcs["Ролан"].shift != -1)) bad++;
        }
        T("T233", okN && failN && !bad, "проверка в разговоре: переход по итогу");
    }
    // T234 состязание в разговоре (Обман против Проницательности NPC): победа говорящего → ok
    {
        int bad = 0;
        for (int sd = 1; sd <= 40; sd++) {
            kmp::State a = fresh(sd);
            run(a, ct, "{\"type\":\"dialog\",\"npc\":\"Ролан\",\"node\":\"приветствие\"}");
            Json e = run(a, ct, "{\"type\":\"contest\",\"hero\":\"Лис\",\"what\":\"Обман\",\"vs\":\"Ролан\",\"vsWhat\":\"Проницательность\",\"vsMod\":3,"
                                "\"dlg\":{\"ok\":\"поверил\",\"fail\":\"раскусил\"}}");
            if (a.dlgNode != (e["res"]["win"].str() == "Лис" ? "поверил" : "раскусил")) bad++;
        }
        T("T234", !bad, "состязание в разговоре");
    }

    // T235 путь по миру: полоса по 7,5 км вдоль x — 2 клетки дороги, 4 пашни (река в 4-й), 2 леса, вода
    {
        struct Strip : kmp::World {
            std::string t = "000000117", r = "110000000", v = "000100000";
            kmp::WCell at(double x, double y, double) const override {
                kmp::WCell c;
                const int i = (int)std::floor(x / 7.5);
                if (i < 0 || i >= 9 || y < 0 || y > 7.5) { c.terr = kmp::TERR_WATER; return c; }
                c.terr = t[i] - '0', c.road = r[i] == '1', c.river = v[i] == '1';
                return c;
            }
        };
        auto W = std::make_shared<Strip>();
        W->w = 67.5, W->h = 7.5;
        kmp::Content c2;
        c2.world = W;
        kmp::State a = fresh();
        a.food = a.water = 100, a.hasPos = true, a.x = 0.1, a.y = 3.75;
        run(a, c2, "{\"type\":\"route\",\"path\":[[63.7,3.75]]}");
        Json d1 = run(a, c2, "{\"type\":\"day\"}");
        // 35 км: 14,9 км дороги ×1, дальше пашни ×0,75 (20,1 → 15,1 км) → ~30 км; больше времени — на пашнях, реку перешли
        const bool day1 = std::abs(d1["res"]["km"].num() - 30) <= 1 && d1["res"]["terrain"].str() == "Река и переправа" && !d1["res"]["road"].flag() && a.x > 29 && a.x < 31;
        Json d2 = run(a, c2, "{\"type\":\"day\"}");
        Json d3 = run(a, c2, "{\"type\":\"day\"}");
        T("T235", day1 && d2["res"]["terrain"].str() == "Пашни и сёла" && d3["res"]["blocked"].flag() && a.x < 60.1 && a.route.size() == 1 &&
                      d3["res"]["terrain"].str() == "Лес",
          "путь по миру: местность и дорога дня из пути, вода не пускает");
        // T236 мастер задал местность сам — мир не используется
        Json m = run(a, c2, "{\"type\":\"day\",\"terrain\":\"Горы\"}");
        T("T236", m["res"]["terrain"].str() == "Горы" && !m["res"].has("pos"), "местность от мастера важнее карты");
    }
    // T237 Акт I как мир: Париж → Орлеан (~110 км по прямой, по римской дороге) — 3–5 дней пешком
    {
        std::ifstream f("web/data/region_act1.json");
        std::stringstream ss;
        ss << f.rdbuf();
        Json d = parseJson(ss.str());
        auto A = std::make_shared<kmp::ActWorld>();
        kmp::Region& R = A->R;
        R.w = d["w"].num(0), R.h = d["h"].num(0), R.lon0 = d["lon0"].dbl(0), R.lat0 = d["lat0"].dbl(0), R.dlon = d["dlon"].dbl(1), R.dlat = d["dlat"].dbl(1);
        R.terr = d["terrain"].str(), R.road = d["road"].str(), R.river = d["river"].str();
        A->build({});
        kmp::Content c2;
        c2.world = A;
        double px, py, ox, oy;
        A->toKm(2.35, 48.86, px, py), A->toKm(1.909, 47.902, ox, oy);
        kmp::State a = fresh();
        a.food = a.water = 100, a.purse = 100000, a.hasPos = true, a.x = px, a.y = py;
        run(a, c2, "{\"type\":\"route\",\"path\":[[" + std::to_string(ox) + "," + std::to_string(oy) + "]]}");
        int days = 0;
        while (!a.route.empty() && days < 10) run(a, c2, "{\"type\":\"day\"}"), days++;
        T("T237", R.ok() && A->at(px, py, 0).terr == kmp::TERR_TOWN && days >= 3 && days <= 5 && std::hypot(a.x - ox, a.y - oy) < 0.1 && A->w > 700 && A->w < 850,
          "Акт I как мир (" + std::to_string((int)A->w) + " км): Париж — Орлеан за " + std::to_string(days) + " дня");
    }
    // T238 мир по параметрам: тот же вход — те же клетки и куски; порядок запросов не важен; доля воды ≈ заданной
    {
        auto mk = [](uint64_t seed) { auto G = std::make_shared<kmp::GenWorld>(); G->p.sizeKm = 300, G->p.seed = seed, G->p.sea = 0.4; G->build(); return G; };
        auto g1 = mk(5), g2 = mk(5), g3 = mk(6);
        const std::string t1 = kmp::tileJson(*g1, 1, 1, 1), t2a = kmp::tileJson(*g2, 0, 3, 3), t2 = kmp::tileJson(*g2, 1, 1, 1), t1a = kmp::tileJson(*g1, 0, 3, 3);
        int water = 0, n = 0;
        for (int j = 0; j < 60; j++)
            for (int i = 0; i < 60; i++) water += g1->at((i + 0.5) * 5, (j + 0.5) * 5, 0).terr == kmp::TERR_WATER, n++;
        T("T238", t1 == t2 && t1a == t2a && t1 != kmp::tileJson(*g3, 1, 1, 1) && std::abs(water / (double)n - 0.4) < 0.08 && !g1->towns.empty() && !g1->roads.segs.empty(),
          "мир по параметрам детерминирован; воды " + std::to_string(water * 100 / n) + "% при заданных 40%");
    }
    // T239 масштаб: клетка 1000 фт; мир 5000 км помещается в куски на уровне zMax; поселение стоит на суше, к нему ведёт дорога
    {
        auto G = std::make_shared<kmp::GenWorld>();
        G->p.sizeKm = 5000, G->p.seed = 3;
        G->build();
        const kmp::Town& t = G->towns[0];
        const bool onLand = G->at(t.x, t.y, 0).terr == kmp::TERR_TOWN;
        bool road = false;
        for (int k = 0; k < 64 && !road; k++) road = G->at(t.x + std::cos(k * 0.1) * t.r * 1.5, t.y + std::sin(k * 0.1) * t.r * 1.5, 0.6).road;
        T("T239", std::abs(kmp::CELL_KM - 0.3048) < 1e-9 && (256 << G->zMax()) >= G->cellsW() && G->cellsW() > 16000 && onLand && road,
          "масштаб: клетка 1000 фт, мир 5000 км — " + std::to_string(G->cellsW()) + " клеток, zMax " + std::to_string(G->zMax()));
    }

    // T240–T243 поиск пути (ТЗ v2 §4)
    {
        // мир 30 × 10 км: лес везде, дорога петлёй по краю (y < 1: вверх, вдоль, вниз), озеро посередине в x 12–16, y 3–7
        struct Loop : kmp::World {
            kmp::WCell at(double x, double y, double) const override {
                kmp::WCell c;
                if (x < 0 || y < 0 || x > 30 || y > 10) { c.terr = kmp::TERR_WATER; return c; }
                if (x > 12 && x < 16 && y > 3 && y < 7) { c.terr = kmp::TERR_WATER; return c; }
                c.terr = kmp::TERR_FOREST;
                c.road = y < 1 || ((x < 1.5 || x > 28.5) && y < 6);
                return c;
            }
        };
        Loop L;
        L.w = 30, L.h = 10;
        double mult[kmp::TERR_N];
        for (int t = 0; t < kmp::TERR_N; t++) mult[t] = std::max(0.01, kmp::terrains()[t].mult);
        mult[kmp::TERR_TOWN] = 1;
        kmp::Pts pf, pr, ps;
        std::string err;
        const bool ok = kmp::findRoute(L, 1, 5, {{29, 5}}, kmp::PM_FAST, mult, pf, err) && kmp::findRoute(L, 1, 5, {{29, 5}}, kmp::PM_ROADS, mult, pr, err) &&
                        kmp::findRoute(L, 1, 5, {{29, 5}}, kmp::PM_STRAIGHT, mult, ps, err);
        auto wet = [&](const kmp::Pts& p) {   // путь пересекает воду?
            double x = 1, y = 5;
            for (auto& q : p) {
                for (int k = 1; k <= 50; k++)
                    if (L.at(x + (q.first - x) * k / 50, y + (q.second - y) * k / 50, 0).terr == kmp::TERR_WATER) return true;
                x = q.first, y = q.second;
            }
            return false;
        };
        const kmp::RouteSum sf = kmp::routeSummary(L, 1, 5, pf, 35, mult), ss = kmp::routeSummary(L, 1, 5, ps, 35, mult);
        T("T240", ok && !wet(pf) && !wet(pr) && !wet(ps) && pf.back() == std::make_pair(29.0, 5.0), "путь обходит воду во всех режимах" + (ok ? std::string() : ": " + err));
        T("T241", ok && sf.roadKm > sf.km * 0.6 && ss.roadKm < ss.km * 0.3 && ss.km < sf.km,
          "быстрее всего — по дороге петлёй (" + std::to_string((int)sf.km) + " км, дорога " + std::to_string((int)(100 * sf.roadKm / std::max(1.0, sf.km))) +
              "%), напрямик — короче, по лесу (" + std::to_string((int)ss.km) + " км)");
        std::string e2;
        kmp::Pts bad;
        T("T242", !kmp::findRoute(L, 1, 5, {{14, 5}}, kmp::PM_FAST, mult, bad, e2) && e2 == "цель в воде", "цель в воде — понятная ошибка");
    }
    {
        std::ifstream f("web/data/region_act1.json");
        std::stringstream ss;
        ss << f.rdbuf();
        Json d = parseJson(ss.str());
        auto A = std::make_shared<kmp::ActWorld>();
        kmp::Region& R = A->R;
        R.w = d["w"].num(0), R.h = d["h"].num(0), R.lon0 = d["lon0"].dbl(0), R.lat0 = d["lat0"].dbl(0), R.dlon = d["dlon"].dbl(1), R.dlat = d["dlat"].dbl(1);
        R.terr = d["terrain"].str(), R.road = d["road"].str(), R.river = d["river"].str();
        A->build({});
        double px, py, cx, cy;
        A->toKm(2.35, 48.86, px, py), A->toKm(2.35, 43.21, cx, cy);
        kmp::Content c2;
        c2.world = A;
        kmp::State a = fresh();
        a.hasPos = true, a.x = px, a.y = py, a.food = a.water = 1000, a.purse = 1000000;
        const auto t0 = std::chrono::steady_clock::now();
        Json rf = run(a, c2, "{\"type\":\"route\",\"mode\":\"быстрее всего\",\"way\":[[" + std::to_string(cx) + "," + std::to_string(cy) + "]]}");
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        const std::string fastPath = kmp::jsonOut(rf["res"]["path"]);
        Json rs = run(a, c2, "{\"type\":\"route\",\"mode\":\"напрямик\",\"way\":[[" + std::to_string(cx) + "," + std::to_string(cy) + "]]}");
        const int df = rf["res"]["sum"]["days"].num(), ds = rs["res"]["sum"]["days"].num();
        // день пути — по найденному пути: отряд идёт, пока маршрут не кончится
        run(a, c2, "{\"type\":\"route\",\"mode\":\"быстрее всего\",\"way\":[[" + std::to_string(cx) + "," + std::to_string(cy) + "]]}");
        int days = 0;
        while (!a.route.empty() && days < 60) run(a, c2, "{\"type\":\"day\",\"roles\":{\"Лис\":\"проводник\"}}"), days++;
        T("T243", fastPath != kmp::jsonOut(rs["res"]["path"]) && df >= 12 && df <= 30 && rf["res"]["sum"]["road"].num() > rs["res"]["sum"]["road"].num() && ms < 3000 &&
                      std::hypot(a.x - cx, a.y - cy) < 0.5 && days >= df,
          "Париж — Каркассон: быстрее всего " + std::to_string(rf["res"]["sum"]["km"].num()) + " км, " + std::to_string(df) + " дн., дорога " +
              std::to_string(rf["res"]["sum"]["road"].num()) + "%; напрямик " + std::to_string(rs["res"]["sum"]["km"].num()) + " км, " + std::to_string(ds) +
              " дн.; поиск " + std::to_string((int)ms) + " мс; дошли за " + std::to_string(days) + " дн.");
    }

    std::printf("\nкампания: %d прошло, %d не прошло\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
