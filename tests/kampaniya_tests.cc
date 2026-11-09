// Проверки кампании (исследование и социалка, ТЗ docs/TZ_issledovanie_socialka.md, номера T210+).
// Сборка и запуск: make test_kampaniya. Боевых проверок (tests/audit_tests.cc) не трогает.
#include "engine.h"
#include "char_spec.h"
#include "kmp_events.h"

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
        T("T216", r1["res"]["km"].num() == 35 && r2["res"]["km"].num() == 18 && r3["res"]["km"].num() == 22 && a.day == 4,
          "км в день: путник × местность × темп");
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

    std::printf("\nкампания: %d прошло, %d не прошло\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
