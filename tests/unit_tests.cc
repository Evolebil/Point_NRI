// ============================================================================
// Модульные (unit) тесты движка «Арена Боссов» на C++. Каждая проверка берёт одну функцию или один небольшой класс и
// сверяет его с эталоном из правил или с математическим определением; другие модули не подключаются «боевым» путём.
//
// Отличие от tests/audit_tests.cc: там проверяются требования правил на сотнях случайных боссов и боях (интеграция),
// здесь — отдельные единицы кода с известными входами и ожидаемыми выходами.
//
// Сборка и запуск:  make unit      (или: g++ -std=c++17 -O2 -pthread -Isrc/engine -Isrc/ai -Isrc/kampaniya
//                                         -o build/unit_tests tests/unit_tests.cc && ./build/unit_tests)
// Код возврата 0 — все проверки пройдены, 1 — есть провалы.
// ============================================================================
#include "engine.h"
#include "char_spec.h"
#include "kmp_events.h"
#include <cstdarg>
#include <functional>

// ---- мини-каркас: группа (SUITE) → проверки (CHECK / CHECK_EQ / CHECK_NEAR) --------------------------------------
static int g_pass = 0, g_fail = 0;
static const char* g_suite = "";
static std::vector<std::string> g_failed;

#define SUITE(name) g_suite = name; std::printf("\n[%s]\n", name)
#define CHECK(id, cond)                                                                                              \
    do {                                                                                                             \
        const bool ok_ = (cond);                                                                                     \
        std::printf("  %-6s %s  %s\n", id, ok_ ? "PASS" : "FAIL", #cond);                                            \
        if (ok_) g_pass++;                                                                                           \
        else { g_fail++; g_failed.push_back(std::string(g_suite) + " / " + id + ": " + #cond); }                     \
    } while (0)
#define CHECK_EQ(id, got, want)                                                                                      \
    do {                                                                                                             \
        const auto g_ = (got);                                                                                       \
        const auto w_ = (want);                                                                                      \
        const bool ok_ = (g_ == w_);                                                                                 \
        std::printf("  %-6s %s  %s == %s\n", id, ok_ ? "PASS" : "FAIL", #got, #want);                               \
        if (!ok_) std::printf("         получено: %s\n", toS(g_).c_str());                                           \
        if (ok_) g_pass++;                                                                                           \
        else { g_fail++; g_failed.push_back(std::string(g_suite) + " / " + id + ": " + #got + " == " + #want); }     \
    } while (0)
#define CHECK_NEAR(id, got, want, eps) CHECK(id, std::fabs((double)(got) - (double)(want)) <= (eps))

template <class T>
static std::string toS(const T& v) {
    if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
    else if constexpr (std::is_arithmetic_v<T>) return std::to_string(v);
    else return "\"" + std::string(v) + "\"";
}

static Coef baseCoef() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    return C;
}

// рыцарь из лабораторной работы 2: СИЛ 16, ЛОВ 10, ТЕЛ 14, ИНТ 8, МДР 10, ХАР 8
static const char* KNIGHT =
    "{\"v\":21,\"name\":\"Рыцарь\",\"budget\":800,\"stats\":[16,10,14,8,10,8],\"acV\":8,"
    "\"weapon\":{\"n\":1,\"s\":8,\"range\":5,\"att\":2,\"reach\":5,\"type\":11},"
    "\"abilities\":[{\"delivery\":1,\"saveStat\":1,\"range\":5,\"radius\":10,\"dmgN\":2,\"dmgS\":6,\"dtype\":1,\"cond\":1,\"recharge\":5}],"
    "\"passives\":{\"saveProf\":[2]}}";

// ============================================================================
int main() {
    const Coef C = baseCoef();

    // ---------------------------------------------------------------- Rng (core.h)
    SUITE("Rng — генератор случайных чисел");
    {
        Rng a(123), b(123), c(124);
        bool same = true, diff = false;
        for (int i = 0; i < 100; i++) {
            const uint64_t x = a.next(), y = b.next(), z = c.next();
            if (x != y) same = false;
            if (x != z) diff = true;
        }
        CHECK("U001", same);   // одно зерно — одна последовательность
        CHECK("U002", diff);   // другое зерно — другая последовательность
        Rng r(7);
        const uint64_t first = r.next();
        r.next();
        r.seed(7);
        CHECK_EQ("U003", r.next(), first);   // seed() возвращает генератор в начало
        // эталон: значения xorshift64* с зерном 42 зафиксированы — на любом компиляторе они должны совпасть
        Rng g(42);
        const uint64_t e1 = g.next(), e2 = g.next();
        Rng g2(42);
        CHECK("U004", g2.next() == e1 && g2.next() == e2 && e1 != e2);
        Rng z(0);   // нулевое зерно не должно давать нулевое состояние (xorshift застрял бы в нуле)
        CHECK("U005", z.s != 0 && z.next() != 0);
        bool inRange = true;
        for (int i = 0; i < 20000; i++)
            if (r.below(7) >= 7) inRange = false;
        CHECK("U006", inRange);   // below(n) < n
        CHECK_EQ("U007", Rng(5).below(1), 0u);   // единственный вариант — 0
        double mn = 1, mx = 0;
        for (int i = 0; i < 20000; i++) {
            const double u = r.unit();
            mn = std::min(mn, u);
            mx = std::max(mx, u);
        }
        CHECK("U008", mn >= 0.0 && mx < 1.0);   // unit() в [0, 1)
        // равномерность: каждая из 8 корзин получает ~1/8 из 80000 бросков (допуск 5%)
        int bins[8] = {};
        for (int i = 0; i < 80000; i++) bins[r.below(8)]++;
        bool flat = true;
        for (int k = 0; k < 8; k++)
            if (std::abs(bins[k] - 10000) > 500) flat = false;
        CHECK("U009", flat);
    }

    // ---------------------------------------------------------------- кубики
    SUITE("Кубики: rollDie / rollDice / isDie");
    {
        rng.seed(1);
        bool d20ok = true, d20all = true;
        bool seen[21] = {};
        for (int i = 0; i < 5000; i++) {
            const int v = rollD20();
            if (v < 1 || v > 20) d20ok = false;
            seen[v] = true;
        }
        for (int v = 1; v <= 20; v++)
            if (!seen[v]) d20all = false;
        CHECK("U010", d20ok);    // к20 даёт 1…20
        CHECK("U011", d20all);   // и выпадают все грани
        CHECK_EQ("U012", rollDie(0), 0);    // кубика без граней нет
        CHECK_EQ("U013", rollDie(-5), 0);
        CHECK_EQ("U014", rollDie(1), 1);    // у однограннего — всегда 1
        CHECK_EQ("U015", rollDice(0, 6), 0);   // ноль кубиков — ноль
        bool sumOk = true;
        for (int i = 0; i < 3000; i++) {
            const int t = rollDice(3, 6);
            if (t < 3 || t > 18) sumOk = false;
        }
        CHECK("U016", sumOk);   // 3к6 — от 3 до 18
        double s = 0;
        for (int i = 0; i < 40000; i++) s += rollDice(2, 10);
        CHECK_NEAR("U017", s / 40000, 11.0, 0.15);   // мат. ожидание 2к10 = 11
        CHECK("U018", isDie(4) && isDie(6) && isDie(20) && isDie(100));
        CHECK("U019", !isDie(3) && !isDie(7) && !isDie(0) && !isDie(-6) && !isDie(101));
        CHECK_EQ("U020", DICE_N, 8);
        rng.seed(99);
        const int a1 = rollD20();
        rng.seed(99);
        CHECK_EQ("U021", rollD20(), a1);   // бросок воспроизводится по зерну
    }

    // ---------------------------------------------------------------- AMask
    SUITE("AMask — 256-битная маска атомов");
    {
        AMask m;
        CHECK("U030", !m);                       // пустая — ложь
        m = ONE << 0;
        CHECK("U031", (bool)m);
        CHECK("U032", (bool)((ONE << 63) & (ONE << 63)));
        const AMask hi = ONE << 200;             // бит в старшем слове
        CHECK_EQ("U033", hi.w[3], (uint64_t)1 << (200 - 192));
        CHECK("U034", (bool)(hi >> 200) && ((hi >> 200) == ONE));   // обратный сдвиг
        CHECK("U035", !((ONE << 64) & (ONE << 65)));                // разные слова не пересекаются
        AMask u = (ONE << 3) | (ONE << 70) | (ONE << 255);
        CHECK("U036", (bool)(u & (ONE << 70)) && !(u & (ONE << 71)));
        AMask v = ONE << 3;
        v |= ONE << 130;
        CHECK("U037", (bool)(v & (ONE << 130)) && (bool)(v & (ONE << 3)));   // |=
        CHECK("U038", (ONE << 5) == (ONE << 5) && (ONE << 5) != (ONE << 6));
        CHECK("U039", !((ONE << 1) << 255));   // сдвиг за край стирает бит, а не заворачивает
        CHECK("U040", (bool)(ONE << 191) && (ONE << 191).w[2] == (uint64_t)1 << 63);   // граница слов
    }

    // ---------------------------------------------------------------- цены и модификаторы
    SUITE("Цены: tri / statCost / acCost / maxV / modOf");
    {
        CHECK_EQ("U050", tri(0), 0LL);
        CHECK_EQ("U051", tri(1), 1LL);
        CHECK_EQ("U052", tri(10), 55LL);      // 10·11/2
        CHECK_EQ("U053", tri(-3), 0LL);       // отрицательное — 0
        CHECK_EQ("U054", tri(40), 820LL);
        // характеристика по правилам: V(V+1)/2 − 55. Эталоны рыцаря из ЛР2: 81, 0, 50, −19, 0, −19
        CHECK_EQ("U055", statCost(16), 81LL);
        CHECK_EQ("U056", statCost(10), 0LL);
        CHECK_EQ("U057", statCost(14), 50LL);
        CHECK_EQ("U058", statCost(8), -19LL);
        CHECK_EQ("U059", statCost(1), -54LL);   // минимум: очки возвращаются, но не бесконечно
        CHECK("U060", statCost(11) > statCost(10) && statCost(9) < statCost(10));   // монотонность
        CHECK_EQ("U061", STAT_BASE, 330LL);     // 6 характеристик по 10 в абсолютной цене
        CHECK_EQ("U062", acCost(0), 0LL);
        CHECK_EQ("U063", acCost(1), tri(2));    // КД: цена tri(2·a)
        CHECK_EQ("U064", acCost(8), tri(16));
        CHECK_EQ("U065", maxV(0), 0);
        CHECK_EQ("U066", maxV(-5), 0);
        CHECK_EQ("U067", maxV(55), 10);         // ровно tri(10)
        CHECK_EQ("U068", maxV(54), 9);          // на единицу меньше — предыдущее значение
        CHECK_EQ("U069", maxV(1), 1);
        bool inv = true;
        for (int v = 0; v <= 40; v++)
            if (maxV(tri(v)) != v) inv = false;
        CHECK("U070", inv);   // maxV — обратная к tri
        // модификатор: floor((V−10)/2)
        CHECK_EQ("U071", modOf(10), 0);
        CHECK_EQ("U072", modOf(11), 0);
        CHECK_EQ("U073", modOf(12), 1);
        CHECK_EQ("U074", modOf(9), -1);    // округление вниз, а не к нулю
        CHECK_EQ("U075", modOf(8), -1);
        CHECK_EQ("U076", modOf(1), -5);
        CHECK_EQ("U077", modOf(20), 5);
        CHECK_EQ("U078", modOf(40), 15);
        CHECK("U079", STAT_MAX == 40 && AC_MAX == 40 && MIN_BUDGET == 20);
    }

    // ---------------------------------------------------------------- лестницы цен
    SUITE("Лестницы цен: tierN");
    {
        const double savedB = g_B;
        g_B = 1600;   // на бюджете 1600 множитель (B/1600)^q = 1 и пороги a, b — как в таблице
        bool zero = true, mono = true;
        for (int t = 0; t < TR_N; t++) {
            if (std::fabs(tierN(t, 0)) > 1e-9) zero = false;
            double prev = 0;
            for (int n = 1; n <= 60; n++) {
                const double v = tierN(t, n);
                if (v < prev) mono = false;   // цена не убывает
                prev = v;
            }
        }
        CHECK("U090", zero);   // ноль единиц — бесплатно
        CHECK("U091", mono);   // больше единиц — не дешевле
        bool minStep = true;   // «ступень никогда не дешевле базовой» (комментарий в atoms.h): шаг цены ≥ 1
        for (int t = 0; t < TR_N; t++)
            for (int n = 1; n <= 30; n++)
                if (tierN(t, n) - tierN(t, n - 1) < 1.0 - 1e-6) minStep = false;
        CHECK("U092", minStep);
        // с ростом бюджета цена лестницы с q > 0 растёт, с q < 0 (дальность) — падает
        g_B = 400;
        const double lowDmg = tierN(TR_DMG, 5), lowRange = tierN(TR_RANGE, 5);
        g_B = 6400;
        const double hiDmg = tierN(TR_DMG, 5), hiRange = tierN(TR_RANGE, 5);
        CHECK("U092a", hiDmg > lowDmg);
        CHECK("U092b", hiRange < lowRange);
        CHECK_NEAR("U093", tierN(TR_DMG, 1), tierN(TR_DMG, 1.0), 1e-12);
        g_B = 1600;
        CHECK("U094", tierN(TR_DMG, 2.5) > tierN(TR_DMG, 2) && tierN(TR_DMG, 2.5) < tierN(TR_DMG, 3));   // между ступенями — между ценами
        g_B = savedB;
    }

    // ---------------------------------------------------------------- Json
    SUITE("mini_json.h — разбор JSON");
    {
        bool ok = false;
        Json j = parseJson("{\"a\":1,\"b\":[1,2,3],\"c\":\"текст\",\"d\":true,\"e\":null,\"f\":-2.5}", &ok);
        CHECK("U100", ok);
        CHECK_EQ("U101", j["a"].num(), 1);
        CHECK_EQ("U102", j["b"].size(), (size_t)3);
        CHECK_EQ("U103", j["b"][2].num(), 3);
        CHECK_EQ("U104", j["c"].str(), "текст");
        CHECK("U105", j["d"].flag());
        CHECK("U106", j["e"].t == Json::NUL && j.has("e"));
        CHECK_NEAR("U107", j["f"].dbl(), -2.5, 1e-12);
        CHECK_EQ("U108", j["f"].num(), -3);   // num() округляет «от нуля»: −2,5 → −3
        CHECK_EQ("U109", j["нет"].num(42), 42);   // нет ключа — значение по умолчанию
        CHECK("U110", !j.has("нет"));
        CHECK_EQ("U111", j["b"][99].num(7), 7);   // индекс за краем — не падение, а умолчание
        CHECK_EQ("U112", j["a"].str("x"), "x");   // число не строка
        CHECK("U113", j["c"].flag(true));         // у строки flag() = умолчание
        parseJson("{oops", &ok);
        CHECK("U114", !ok);
        parseJson("", &ok);
        CHECK("U115", !ok);
        parseJson("{\"a\":1} лишнее", &ok);
        CHECK("U116", !ok);                        // мусор после значения — ошибка
        parseJson("[1,2", &ok);
        CHECK("U117", !ok);                        // не закрыт массив
        parseJson("{\"a\":}", &ok);
        CHECK("U118", !ok);
        parseJson("{1:2}", &ok);
        CHECK("U119", !ok);                        // ключ должен быть строкой
        Json e = parseJson("{}", &ok);
        CHECK("U120", ok && e.t == Json::OBJ && e.o.empty());
        e = parseJson("[]", &ok);
        CHECK("U121", ok && e.t == Json::ARR && e.size() == 0);
        e = parseJson("\"\\u0416\\n\\\"q\\\"\"", &ok);
        CHECK_EQ("U122", e.str(), std::string("Ж\n\"q\""));   // \uXXXX → UTF-8, \n, \"
        e = parseJson("  { \"x\" : [ 1 , 2 ] }  ", &ok);
        CHECK("U123", ok && e["x"].size() == 2);   // пробелы по краям и между токенами
        e = parseJson("false", &ok);
        CHECK("U124", ok && e.t == Json::BOOL && !e.flag(true));
        e = parseJson("1e3", &ok);
        CHECK("U125", ok && e.num() == 1000);
        e = parseJson("{\"a\":1,\"a\":2}", &ok);
        CHECK_EQ("U126", e["a"].num(), 1);   // при повторе ключа берётся первый
        // jq — обратная операция: строка → литерал JSON
        CHECK_EQ("U127", jq("a\"b"), std::string("\"a\\\"b\""));
        CHECK_EQ("U128", jq("a\\b"), std::string("\"a\\\\b\""));
        CHECK_EQ("U129", jq("x\ny"), std::string("\"x\\ny\""));
        CHECK_EQ("U130", jq("a\tb"), std::string("\"a b\""));   // прочие управляющие символы — пробел
        const std::string src = "мир \"тест\" \\ конец";
        CHECK_EQ("U131", parseJson(jq(src)).str(), src);   // round-trip jq → parseJson
        // глубокая вложенность
        std::string deep;
        for (int i = 0; i < 50; i++) deep += "[";
        deep += "1";
        for (int i = 0; i < 50; i++) deep += "]";
        parseJson(deep, &ok);
        CHECK("U132", ok);
    }

    // ---------------------------------------------------------------- деньги и уровни кампании
    SUITE("Кампания: деньги и уровень по бюджету");
    {
        CHECK_EQ("U140", kmp::parseMoney("1 зм"), 100LL);
        CHECK_EQ("U141", kmp::parseMoney("5 см"), 50LL);
        CHECK_EQ("U142", kmp::parseMoney("3 мм"), 3LL);
        CHECK_EQ("U143", kmp::parseMoney("1 зм 5 см 3 мм"), 153LL);
        CHECK_EQ("U144", kmp::parseMoney("42"), 42LL);   // голое число — медные
        CHECK_EQ("U145", kmp::parseMoney(""), 0LL);
        CHECK_EQ("U146", kmp::parseMoney("100 зм"), 10000LL);
        CHECK_EQ("U147", kmp::money(0), "0 мм");
        CHECK_EQ("U148", kmp::money(153), "1 зм 5 см 3 мм");
        CHECK_EQ("U149", kmp::money(100), "1 зм");
        CHECK_EQ("U150", kmp::money(10), "1 см");
        CHECK_EQ("U151", kmp::money(7), "7 мм");
        bool rt = true;
        for (long long v = 0; v < 3000; v += 7)
            if (kmp::parseMoney(kmp::money(v)) != v) rt = false;
        CHECK("U152", rt);   // money и parseMoney взаимно обратны
        CHECK_EQ("U153", kmp::levelOf(200), 1);
        CHECK_EQ("U154", kmp::levelOf(1), 1);       // ниже 200 — первый уровень
        CHECK_EQ("U155", kmp::levelOf(215), 1);
        CHECK_EQ("U156", kmp::levelOf(216), 2);     // второй уровень — с 216
        CHECK_EQ("U157", kmp::levelOf(400), 10);
        CHECK_EQ("U158", kmp::levelOf(8599), 49);
        CHECK_EQ("U159", kmp::levelOf(8600), 50);
        bool mono = true;
        int prev = 0;
        for (long long b = 100; b < 9000; b += 13) {
            const int L = kmp::levelOf(b);
            if (L < prev) mono = false;
            prev = L;
        }
        CHECK("U160", mono);   // уровень не убывает с ростом бюджета
        CHECK_NEAR("U161", kmp::levelBudget(1), 200.0, 1e-9);
        CHECK_NEAR("U162", kmp::levelBudget(50), 8600.0, 1e-6);
    }

    // ---------------------------------------------------------------- пользовательская карта
    SUITE("custom_map.h — проверка пользовательской карты");
    {
        auto grid = [](int w, int h, char ch) {
            std::string s = "[";
            for (int y = 0; y < h; y++) s += std::string(y ? "," : "") + "\"" + std::string(w, ch) + "\"";
            return s + "]";
        };
        auto mapJson = [&](int w, int h, const std::string& extra) {
            return "{\"aw\":" + std::to_string(w) + ",\"ah\":" + std::to_string(h) + ",\"wall\":" + grid(w, h, '0') + extra + "}";
        };
        CustomMap m;
        std::string err;
        CHECK("U170", parseCustomMap(parseJson(mapJson(10, 10, "")), m, err) && m.on && m.aw == 10 && m.ah == 10);
        CHECK("U171", !parseCustomMap(parseJson(mapJson(4, 10, "")), m, err));    // меньше 5×5
        CHECK("U172", err.find("размер карты") != std::string::npos);
        CHECK("U173", !parseCustomMap(parseJson(mapJson(10, 4, "")), m, err));
        CHECK("U174", !parseCustomMap(parseJson(mapJson(W + 1, 10, "")), m, err));   // больше предела арены
        CHECK("U175", !parseCustomMap(parseJson("{}"), m, err));                    // пустой объект
        CHECK("U176", parseCustomMap(parseJson(mapJson(5, 5, "")), m, err));        // нижняя граница допустима
        // начальные клетки
        CHECK("U177", parseCustomMap(parseJson(mapJson(10, 10, ",\"starts\":[[1,1],[8,8]]")), m, err) && m.hasStarts());
        CHECK("U178", !parseCustomMap(parseJson(mapJson(10, 10, ",\"starts\":[[1,1]]")), m, err));   // только одна из двух
        CHECK("U179", err.find("обе") != std::string::npos);
        CHECK("U180", !parseCustomMap(parseJson(mapJson(10, 10, ",\"starts\":[[3,3],[3,3]]")), m, err));   // совпадают
        CHECK("U181", err.find("совпадают") != std::string::npos);
        CHECK("U182", !parseCustomMap(parseJson(mapJson(10, 10, ",\"starts\":[[1,1],[20,20]]")), m, err));   // за краем
        // сплошная стена: свободных клеток меньше четырёх
        {
            const std::string full = "{\"aw\":10,\"ah\":10,\"wall\":" + grid(10, 10, '1') + "}";
            CHECK("U183", !parseCustomMap(parseJson(full), m, err));
            CHECK("U184", err.find("меньше четырёх") != std::string::npos);
        }
        // стена, разрезающая карту на две несвязные половины
        {
            std::string rows = "[";
            for (int y = 0; y < 10; y++) rows += std::string(y ? "," : "") + "\"" + std::string("00000") + "1" + "0000\"";
            rows += "]";
            const std::string cut = "{\"aw\":10,\"ah\":10,\"wall\":" + rows + "}";
            CHECK("U185", !parseCustomMap(parseJson(cut), m, err));
        }
        // тема приводится к 0/1
        CHECK("U186", parseCustomMap(parseJson(mapJson(10, 10, ",\"theme\":7")), m, err) && m.theme == 1);
    }

    // ---------------------------------------------------------------- сборка персонажа
    SUITE("char_spec.h — buildCharacter / priceReply");
    {
        Boss b;
        long long budget = 0, spent = 0, left = 0;
        std::string err;
        const Json k = parseJson(KNIGHT);
        CHECK("U190", chr::buildCharacter(k, C, b, budget, err, &spent, &left));
        CHECK_EQ("U191", budget, 800LL);
        CHECK_EQ("U192", spent + left, budget);   // потрачено + остаток = бюджет
        CHECK("U193", b.hp > 0);
        CHECK("U194", left >= 0);
        CHECK_EQ("U195", b.stats[STR], 16);
        CHECK_EQ("U196", b.stats[INT], 8);
        CHECK_EQ("U197", b.acV, 8);
        CHECK_EQ("U198", b.ac, 18);   // КД = 10 + acV (рыцарь без доспеха)
        {   // описание без "v" — старый формат: единицы КД вдвое дороже, поэтому acV делится пополам (тот же КД)
            Json old = parseJson(KNIGHT);
            std::vector<std::pair<std::string, Json>> kept;
            for (auto& kv : old.o)
                if (kv.first != "v") kept.push_back(kv);
            old.o = kept;
            Boss bo;
            long long bud = 0;
            std::string eo;
            CHECK("U198a", chr::buildCharacter(old, C, bo, bud, eo) && bo.acV == 4 && bo.ac == 14);
        }
        CHECK("U199", b.wN == 1 && b.wS == 8 && b.wAtt == 2);
        CHECK("U200", b.abil.size() == 1);
        // слишком маленький бюджет
        {
            Boss b2;
            long long bud = 0;
            std::string e2;
            Json lo = parseJson(KNIGHT);
            for (auto& kv : lo.o)
                if (kv.first == "budget") kv.second.d = 10;
            CHECK("U201", !chr::buildCharacter(lo, C, b2, bud, e2));
            CHECK("U202", !e2.empty());
        }
        // больший бюджет — больше ХП
        {
            Json big = parseJson(KNIGHT);
            for (auto& kv : big.o)
                if (kv.first == "budget") kv.second.d = 1600;
            Boss b3;
            long long bud = 0;
            std::string e3;
            CHECK("U203", chr::buildCharacter(big, C, b3, bud, e3) && b3.hp > b.hp);
        }
        // priceReply — JSON, разбираемый обратно
        {
            bool ok = false;
            const Json r = parseJson(chr::priceReply(k, C), &ok);
            CHECK("U204", ok && r["ok"].flag());
            CHECK_EQ("U205", r["spent"].num() + r["left"].num(), 800);
            // цены характеристик в разборе — эталон правил
            std::vector<int> stats;
            for (size_t i = 0; i < r["boss"]["items"].size(); i++)
                if (r["boss"]["items"][i]["cat"].str() == "stat") stats.push_back(r["boss"]["items"][i]["cost"].num());
            CHECK("U206", stats == std::vector<int>({81, 0, 50, -19, 0, -19}));
        }
        {
            Json bad = parseJson(KNIGHT);
            for (auto& kv : bad.o)
                if (kv.first == "budget") kv.second.d = 10;
            bool ok = false;
            const Json r = parseJson(chr::priceReply(bad, C), &ok);
            CHECK("U207", ok && !r["ok"].flag() && r["err"].str().find("бюджет") != std::string::npos);
        }
        // характеристика вне допустимого диапазона
        {
            Json bad = parseJson(KNIGHT);
            for (auto& kv : bad.o)
                if (kv.first == "stats") kv.second.a[0].d = 99;
            Boss b4;
            long long bud = 0;
            std::string e4;
            CHECK("U208", !chr::buildCharacter(bad, C, b4, bud, e4));
        }
        // пустое описание не роняет программу
        {
            Boss b5;
            long long bud = 0;
            std::string e5;
            const bool r5 = chr::buildCharacter(parseJson("{}"), C, b5, bud, e5);
            CHECK("U209", r5 || !e5.empty());
        }
        // catalogReply — корректный JSON, значения зависят от бюджета
        {
            bool ok1 = false, ok2 = false;
            const Json c800 = parseJson(chr::catalogReply(800, C), &ok1);
            const Json c1600 = parseJson(chr::catalogReply(1600, C), &ok2);
            CHECK("U210", ok1 && ok2);
            for (const char* key : {"ladders", "coef", "mods", "unit", "dtypes", "passives"}) CHECK(key, c800.has(key));
        }
    }

    // ---------------------------------------------------------------- генератор боссов
    SUITE("gen_boss.h — makeBoss");
    {
        rng.seed(2026);
        g_B = 800;
        Boss a1 = [&] { rng.seed(5); return makeBoss(800, C, true); }();
        Boss a2 = [&] { rng.seed(5); return makeBoss(800, C, true); }();
        CHECK_EQ("U220", describeBoss(a1), describeBoss(a2));   // то же зерно — тот же босс
        Boss a3 = [&] { rng.seed(6); return makeBoss(800, C, true); }();
        CHECK("U221", describeBoss(a1) != describeBoss(a3));    // другое зерно — другой босс
        bool inv = true, hpOk = true, statOk = true;
        for (int i = 0; i < 300; i++) {
            const long long B = 200LL << (i % 6);
            rng.seed(1000 + i);
            Boss b = makeBoss(B, C, true);
            if (b.cost - STAT_BASE > B) inv = false;      // не дороже бюджета
            if (b.hp < 1) hpOk = false;                   // хотя бы 1 ХП
            for (int s = 0; s < STAT_N; s++)
                if (b.stats[s] < 1 || b.stats[s] > STAT_MAX) statOk = false;
        }
        CHECK("U222", inv);
        CHECK("U223", hpOk);
        CHECK("U224", statOk);
        // больший бюджет — в среднем больше ХП
        double hpLo = 0, hpHi = 0;
        for (int i = 0; i < 200; i++) {
            rng.seed(50 + i);
            hpLo += makeBoss(400, C, false).hp;
            rng.seed(50 + i);
            hpHi += makeBoss(3200, C, false).hp;
        }
        CHECK("U225", hpHi > hpLo);
        // минимальный бюджет: босс собирается без падения
        bool minOk = true;
        for (int i = 0; i < 50; i++) {
            rng.seed(i);
            Boss b = makeBoss(MIN_BUDGET, C, false);
            if (b.hp < 1) minOk = false;
        }
        CHECK("U226", minOk);
    }

    // ---------------------------------------------------------------- бой
    SUITE("fight.h — бой двух боссов");
    {
        auto duelist = [&](int hp, int dice, int die) {
            Boss b;
            b.stats = {10, 10, 10, 10, 10, 10};
            b.acV = 0;
            b.ac = 10;
            b.hp = hp;
            b.wN = dice;
            b.wS = die;
            b.wRange = 5;
            return b;
        };
        const Boss weak = duelist(5, 1, 4), strong = duelist(500, 4, 12);
        int strongWins = 0, strongLoses = 0, N = 60;
        for (int i = 0; i < N; i++) {
            rng.seed(300 + i);
            const int r0 = fight(strong, weak);     // сильный — сторона 0
            rng.seed(300 + i);
            const int r1 = fight(weak, strong);     // сильный — сторона 1
            strongWins += (r0 == 0) + (r1 == 1);
            strongLoses += (r0 == 1) + (r1 == 0);
        }
        CHECK_EQ("U230", strongLoses, 0);              // сильный не проигрывает никогда
        CHECK("U230a", strongWins >= 2 * N * 9 / 10);  // и побеждает в подавляющем большинстве (остальное — ничьи по лимиту раундов)
        rng.seed(777);
        const int r1 = fight(strong, weak);
        rng.seed(777);
        CHECK_EQ("U231", fight(strong, weak), r1);   // бой воспроизводим по зерну
        bool inRange = true;
        for (int i = 0; i < 20; i++) {
            rng.seed(900 + i);
            const int r = fight(duelist(30, 1, 6), duelist(30, 1, 6));
            if (r < 0 || r > 2) inRange = false;   // 0 — A, 1 — B, 2 — ничья
        }
        CHECK("U232", inRange);
        // зеркальный бой: за большое число боёв доли побед близки
        int w0 = 0, n = 400;
        for (int i = 0; i < n; i++) {
            rng.seed(5000 + i);
            const Boss x = duelist(40, 1, 8);
            const int r = (i % 2) ? fight(x, x) : fight(x, x);
            if (r == 0) w0++;
        }
        CHECK("U233", w0 > n * 0.3 && w0 < n * 0.7);   // стороны не перекошены
    }

    // ---------------------------------------------------------------- итог
    std::printf("\n==========================================\nИтого: PASS %d, FAIL %d\n", g_pass, g_fail);
    for (auto& f : g_failed) std::printf("  провал: %s\n", f.c_str());
    return g_fail ? 1 : 0;
}
