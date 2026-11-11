// ============================================================================
// Тесты устойчивости (стресс-тесты) движка «Арена Боссов»: что будет, если подать мусор, крайние и враждебные значения.
// Аналог «деления на ноль в калькуляторе»: программа не должна падать, зависать, молча портить состояние или
// выдавать неразбираемые ответы — на неверный ввод она обязана ответить понятной ошибкой.
//
// Три вида проверок:
//   rejects(...)   — неверный вход ОТКЛОНЯЕТСЯ, причём с текстом ошибки (а не «молча принят»);
//   survives(...)  — вход любой, но программа не падает, а если приняла, то результат согласован
//                    (потрачено + остаток = бюджет, ХП ≥ 1, характеристики 1…40);
//   инварианты     — состояние кампании после любого события остаётся корректным и сериализуется в верный JSON.
// Сбой процесса (падение, деление на ноль, зависание) ловят обработчики сигналов: в журнал пишется id проверки,
// на которой это случилось, — по нему сразу видно, какой вход «положил» программу.
//
// Проверки с пометкой «ДЕФЕКТ?» написаны по чтению кода: если они красные — это найденная ошибка, а не ошибка теста.
//
// Сборка и запуск: make stress  (или: g++ -std=c++17 -O1 -g -pthread -Isrc/engine -Isrc/ai -Isrc/kampaniya
//                                       -o build/stress_tests tests/stress_tests.cc && ./build/stress_tests)
// Код возврата 0 — все проверки пройдены, 1 — есть провалы, 3 — процесс упал или завис (см. журнал).
// ============================================================================
#include "engine.h"
#include "char_spec.h"
#include "kmp_events.h"
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <functional>
#include <unistd.h>

// ---- мини-каркас (как в unit_tests.cc) + «последняя проверка» для отчёта о падении ------------------------------
static int g_pass = 0, g_fail = 0;
static const char* g_suite = "";
static char g_curBuf[96] = "(до первой проверки)";
static void cur(const std::string& id) { std::snprintf(g_curBuf, sizeof g_curBuf, "%s", id.c_str()); }
static std::vector<std::string> g_failed;

#define SUITE(name) do { g_suite = name; alarm(120); std::printf("\n[%s]\n", name); std::fflush(stdout); } while (0)
#define CHECK(id, cond)                                                                                              \
    do {                                                                                                             \
        cur(id);                                                                                                     \
        const bool ok_ = (cond);                                                                                     \
        std::printf("  %-7s %s  %s\n", (id), ok_ ? "PASS" : "FAIL", #cond);                                          \
        std::fflush(stdout);                                                                                         \
        if (ok_) g_pass++;                                                                                           \
        else { g_fail++; g_failed.push_back(std::string(g_suite) + " / " + (id) + ": " + #cond); }                   \
    } while (0)

static void onCrash(int sig) {
    const char* m1 = "\n!!! ПРОЦЕСС ПРЕРВАН сигналом ";
    const char* name = sig == SIGSEGV ? "SIGSEGV (выход за память / переполнение стека)" : sig == SIGFPE ? "SIGFPE (деление на ноль)"
                     : sig == SIGABRT ? "SIGABRT (abort / исключение)" : sig == SIGALRM ? "SIGALRM (зависание: нет ответа 120 с)"
                     : sig == SIGBUS ? "SIGBUS" : "другим";
    const char* m2 = " на проверке: ";
    (void)!write(2, m1, strlen(m1));
    (void)!write(2, name, strlen(name));
    (void)!write(2, m2, strlen(m2));
    const char* c = g_curBuf;
    (void)!write(2, c, strlen(c));
    (void)!write(2, "\n", 1);
    _exit(3);
}

static Coef C;

// ---- помощники --------------------------------------------------------------------------------------------------
// рыцарь из ЛР2 — «хороший» вход, от которого отламываются плохие
static const std::string K =
    "{\"v\":21,\"name\":\"Рыцарь\",\"budget\":800,\"stats\":[16,10,14,8,10,8],\"acV\":8,"
    "\"weapon\":{\"n\":1,\"s\":8,\"range\":5,\"att\":2,\"reach\":5,\"type\":11},"
    "\"abilities\":[{\"delivery\":1,\"saveStat\":1,\"range\":5,\"radius\":10,\"dmgN\":2,\"dmgS\":6,\"dtype\":1,\"cond\":1,\"recharge\":5}],"
    "\"passives\":{\"saveProf\":[2]}}";
static const std::string MINION = "{\"budget\":100,\"stats\":[10,10,10,10,10,10],\"weapon\":{\"n\":1,\"s\":6}}";

// заменить первую подстроку; если её нет — сам тест сломан (иначе проверка была бы «вхолостую»)
static std::string rep(std::string s, const std::string& a, const std::string& b) {
    const size_t p = s.find(a);
    if (p == std::string::npos) {
        std::printf("  !!! ТЕСТ СЛОМАН: в описании нет подстроки %s\n", a.c_str());
        g_fail++;
        return s;
    }
    return s.replace(p, a.size(), b);
}
static std::string withAbility(const std::string& ab) { return rep(K, "\"abilities\":[", "\"abilities\":[" + ab + ","); }
static std::string withPassive(const std::string& x) { return rep(K, "\"passives\":{", "\"passives\":{" + x + ","); }
static std::string withTop(const std::string& x) { return rep(K, "\"abilities\"", x + ",\"abilities\""); }

struct BR {
    bool parsed = true, ok = false;
    std::string err;
    long long budget = 0, spent = 0, left = 0;
    Boss b;
};
static BR build(const std::string& js) {
    BR r;
    const Json j = parseJson(js, &r.parsed);
    if (!r.parsed) { r.err = "JSON не разобран"; return r; }
    r.ok = chr::buildCharacter(j, C, r.b, r.budget, r.err, &r.spent, &r.left);
    return r;
}
// согласованность результата: отказ — всегда с объяснением; успех — бюджет сходится, ХП и характеристики в пределах
static bool sane(const BR& r) {
    if (!r.ok) return !r.err.empty();
    if (r.left < 0 || r.spent + r.left != r.budget || r.b.hp < 1 || r.b.hp > 1000000) return false;
    for (int i = 0; i < STAT_N; i++)
        if (r.b.stats[i] < 1 || r.b.stats[i] > STAT_MAX) return false;
    return true;
}
static bool survives(const std::string& js) { return sane(build(js)); }
static bool rejects(const std::string& js) { const BR r = build(js); return r.parsed && !r.ok && !r.err.empty(); }
static bool rejectsWith(const std::string& js, const char* part) {
    const BR r = build(js);
    return r.parsed && !r.ok && r.err.find(part) != std::string::npos;
}
static bool jsonOk(const std::string& s) { bool ok = false; parseJson(s, &ok); return ok; }
static double secondsSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// ---- кампания ----------------------------------------------------------------------------------------------------
static const char* HERO =
    "{\"name\":\"Лис\",\"budget\":800,\"prof\":3,\"stats\":[10,14,12,10,14,16],\"passives\":{\"skills\":[12,13,17],\"expert\":[17],"
    "\"saveProf\":[2],\"tools\":[\"воровские\"]}}";
static kmp::State fresh(uint64_t seed = 42, int heroes = 2) {
    kmp::State s;
    s.seed = seed;
    std::string err;
    for (int i = 0; i < heroes; i++) {
        const Json ev = parseJson(std::string("{\"n\":") + std::to_string(i + 1) + ",\"type\":\"hero\",\"name\":\"" + (i ? "Брат" : "Лис") + "\",\"char\":" + HERO +
                                  (i ? "" : ",\"purse\":\"10 зм\"") + "}");
        kmp::apply(s, C, ev, err);
    }
    return s;
}
// событие как в сервере: черновик → применение; при отказе состояние откатывается (как в kampaniya.cc)
static bool act(kmp::State& s, const kmp::Content& ct, const std::string& a) {
    std::string err;
    const kmp::State bak = s;
    const std::string ev = kmp::propose(s, ct, parseJson(a), "мастер", 0, err);
    if (ev.empty()) return false;
    if (!kmp::apply(s, C, parseJson(ev), err)) { s = bak; return false; }
    return true;
}
static bool heroesOk(const kmp::State& s) {
    for (auto& h : s.heroes)
        if (h.hp < 0 || h.hp > h.hpMax || h.hd < 0 || h.hd > h.hdMax || h.exh < 0 || h.exh > 6) return false;
    return true;
}
static bool stateOk(const kmp::State& s) { return heroesOk(s) && jsonOk(kmp::stateJson(s)); }

int main() {
    static char altStack[1 << 16];   // отдельный стек для обработчика: иначе при переполнении основного стека он не запустится
    stack_t ss{};
    ss.ss_sp = altStack;
    ss.ss_size = sizeof altStack;
    sigaltstack(&ss, nullptr);
    for (int sg : {SIGSEGV, SIGFPE, SIGABRT, SIGALRM, SIGBUS}) {
        struct sigaction sa{};
        sa.sa_handler = onCrash;
        sa.sa_flags = SA_ONSTACK;
        sigaction(sg, &sa, nullptr);
    }
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    g_B = 800;

    // ======================================================================= 1. бюджет
    SUITE("Бюджет: границы и мусор");
    {
        CHECK("S001", build(K).ok);                                                        // контроль: исходный рыцарь собирается
        CHECK("S002", rejectsWith(rep(K, "\"budget\":800", "\"budget\":0"), "бюджет"));
        CHECK("S003", rejectsWith(rep(K, "\"budget\":800", "\"budget\":-1"), "бюджет"));
        CHECK("S004", rejectsWith(rep(K, "\"budget\":800", "\"budget\":19"), "бюджет"));   // MIN_BUDGET − 1
        CHECK("S005", rejectsWith(rep(K, "\"budget\":800", "\"budget\":100001"), "бюджет"));   // MAX_BUDGET + 1
        CHECK("S006", rejects(rep(K, "\"budget\":800", "\"budget\":1e30")));               // приведение double → long long вне диапазона
        CHECK("S007", rejects(rep(K, "\"budget\":800", "\"budget\":-1e30")));
        CHECK("S008", survives(rep(K, "\"budget\":800", "\"budget\":\"800\"")));           // строка вместо числа
        CHECK("S009", survives(rep(K, "\"budget\":800", "\"budget\":null")));
        CHECK("S010", survives(rep(K, "\"budget\":800", "\"budget\":[800]")));
        CHECK("S011", survives(rep(K, "\"budget\":800", "\"budget\":800.9")));
        CHECK("S012", survives("{\"budget\":20}"));                                        // ровно минимум, без покупок
        CHECK("S013", survives("{\"budget\":100000}"));                                    // ровно максимум: ХП не переполняют int
        CHECK("S014", survives(rep(K, "\"budget\":800", "\"budget\":100000")));
    }

    // ======================================================================= 2. характеристики
    SUITE("Характеристики и КД");
    {
        CHECK("S020", rejectsWith(rep(K, "[16,10,14,8,10,8]", "[0,10,14,8,10,8]"), "СИЛ: допустимо"));
        CHECK("S021", rejectsWith(rep(K, "[16,10,14,8,10,8]", "[16,-5,14,8,10,8]"), "ЛОВ: допустимо"));
        CHECK("S022", rejectsWith(rep(K, "[16,10,14,8,10,8]", "[16,10,41,8,10,8]"), "ТЕЛ: допустимо"));
        CHECK("S023", rejectsWith(rep(K, "[16,10,14,8,10,8]", "[16,10,14,999999999,10,8]"), "ИНТ: допустимо"));
        CHECK("S024", rejects(rep(K, "[16,10,14,8,10,8]", "[16,10,14,8,10,1e30]")));
        CHECK("S025", survives(rep(K, "[16,10,14,8,10,8]", "\"abc\"")));                   // не массив → 10 по умолчанию
        CHECK("S026", survives(rep(K, "[16,10,14,8,10,8]", "[]")));
        CHECK("S027", survives(rep(K, "[16,10,14,8,10,8]", "[16]")));                      // короткий массив
        CHECK("S028", survives(rep(K, "[16,10,14,8,10,8]", "[16,10,14,8,10,8,99,99]")));   // лишние элементы игнорируются
        CHECK("S029", survives(rep(K, "[16,10,14,8,10,8]", "[null,\"x\",true,[],{},10]")));
        CHECK("S030", survives(rep(K, "[16,10,14,8,10,8]", "[1,1,1,1,1,1]")));             // все минимальные
        CHECK("S031", survives(rep(K, "[16,10,14,8,10,8]", "[40,40,40,40,40,40]")));       // все максимальные: цена огромна, но без переполнения
        CHECK("S032", rejects(rep(K, "[16,10,14,8,10,8]", "[40,40,40,40,40,40]")));        // …и должна упереться в бюджет 800
        CHECK("S033", rejectsWith(rep(K, "\"acV\":8", "\"acV\":31"), "единицы КД"));       // AC_MAX − 10 = 30
        CHECK("S034", rejectsWith(rep(K, "\"acV\":8", "\"acV\":-1"), "единицы КД"));
        CHECK("S035", survives(rep(K, "\"acV\":8", "\"acV\":30")));
        CHECK("S036", survives(rep(K, "\"acV\":8", "\"acV\":\"много\"")));
        CHECK("S037", rejects(rep(K, "\"acV\":8", "\"acV\":1e30")));
    }

    // ======================================================================= 3. оружие
    SUITE("Оружие: крайние значения");
    {
        auto Wp = [&](const std::string& from, const std::string& to) { return rep(K, from, to); };
        CHECK("S040", rejectsWith(Wp("\"s\":8", "\"s\":0"), "грань кубика оружия"));
        CHECK("S041", rejectsWith(Wp("\"s\":8", "\"s\":7"), "грань кубика оружия"));        // не стандартный кубик
        CHECK("S042", rejectsWith(Wp("\"s\":8", "\"s\":-6"), "грань кубика оружия"));
        CHECK("S043", rejectsWith(Wp("\"n\":1,\"s\":8", "\"n\":1001,\"s\":8"), "кубики оружия"));
        CHECK("S044", survives(Wp("\"n\":1,\"s\":8", "\"n\":-3,\"s\":8")));                // отрицательное число кубиков → безоружный
        CHECK("S045", survives(Wp("\"n\":1,\"s\":8", "\"n\":0,\"s\":8")));
        CHECK("S046", rejectsWith(Wp("\"range\":5,\"att\"", "\"range\":0,\"att\""), "дальность оружия"));
        CHECK("S047", rejectsWith(Wp("\"range\":5,\"att\"", "\"range\":7,\"att\""), "кратны"));
        CHECK("S048", rejectsWith(Wp("\"range\":5,\"att\"", "\"range\":501,\"att\""), "дальность оружия"));
        CHECK("S049", rejectsWith(Wp("\"reach\":5", "\"reach\":0"), "досягаемость"));
        CHECK("S050", rejectsWith(Wp("\"reach\":5", "\"reach\":7"), "кратны"));
        CHECK("S051", rejectsWith(Wp("\"att\":2", "\"att\":1001"), "атак за действие"));
        CHECK("S052", survives(Wp("\"att\":2", "\"att\":0")));                              // max(1, …)
        CHECK("S053", survives(Wp("\"att\":2", "\"att\":-7")));
        CHECK("S054", rejectsWith(Wp("\"type\":11", "\"type\":13"), "тип урона оружия"));    // DT_N = 13
        CHECK("S055", rejectsWith(Wp("\"type\":11", "\"type\":-1"), "тип урона оружия"));
        // самое дорогое оружие, какое допускает проверка диапазонов: цена обязана упереться в бюджет, а не переполниться
        const BR r = build(Wp("\"n\":1,\"s\":8,\"range\":5,\"att\":2", "\"n\":1000,\"s\":100,\"range\":500,\"att\":1000"));
        CHECK("S056", !r.ok && !r.err.empty());
        CHECK("S057", survives(Wp("\"weapon\":{", "\"weapon\":5,\"weapon_\":{")));          // weapon — число, а не объект
        CHECK("S058", survives(Wp("\"weapon\":{", "\"weapon\":\"меч\",\"weapon_\":{")));
        CHECK("S059", survives(Wp("\"weapon\":{", "\"weapon\":[],\"weapon_\":{")));
    }

    // ======================================================================= 4. способности
    SUITE("Способности: крайние значения");
    {
        auto A = [&](const std::string& from, const std::string& to) { return rep(K, from, to); };
        CHECK("S060", rejectsWith(A("\"dmgS\":6", "\"dmgS\":7"), "грань кубика"));
        CHECK("S061", rejectsWith(A("\"dmgN\":2", "\"dmgN\":1001"), "кубики урона"));
        CHECK("S062", rejectsWith(A("\"dmgN\":2", "\"dmgN\":-1"), "кубики урона"));
        CHECK("S063", rejectsWith(A("\"delivery\":1", "\"delivery\":3"), "тип попадания"));
        CHECK("S064", rejectsWith(A("\"saveStat\":1", "\"saveStat\":6"), "характеристика спасброска"));
        CHECK("S065", rejectsWith(A("\"saveStat\":1", "\"saveStat\":-1"), "характеристика спасброска"));
        CHECK("S066", rejectsWith(A("\"range\":5,\"radius\"", "\"range\":0,\"radius\""), "дальность способности"));
        CHECK("S067", rejectsWith(A("\"radius\":10", "\"radius\":501"), "радиус области"));
        CHECK("S068", rejectsWith(A("\"dtype\":1", "\"dtype\":13"), "тип урона"));
        CHECK("S069", rejectsWith(A("\"cond\":1", "\"cond\":16"), "состояние"));
        CHECK("S070", rejectsWith(A("\"recharge\":5", "\"recharge\":3"), "перезарядка"));   // допустимо 4–6
        CHECK("S071", rejectsWith(A("\"recharge\":5", "\"recharge\":7"), "перезарядка"));
        CHECK("S072", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"react\":6"), "условие реакции"));
        CHECK("S073", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"dcB\":9"), "надбавка к СЛ"));
        CHECK("S074", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"condRounds\":0"), "раундов состояния"));
        CHECK("S075", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"condRounds\":501"), "раундов состояния"));
        CHECK("S076", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"slotLvl\":10"), "уровень ячейки"));
        CHECK("S077", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"pushFt\":501"), "толчок"));
        CHECK("S078", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"dotN\":1,\"dotRounds\":0"), "укажите число раундов"));
        CHECK("S079", rejectsWith(A("\"recharge\":5", "\"recharge\":5,\"wallLen\":201,\"wallRounds\":1"), "длина барьера"));   // W = 200
        CHECK("S080", survives(A("\"recharge\":5", "\"recharge\":5,\"uses\":-1")));
        CHECK("S081", survives(A("\"recharge\":5", "\"recharge\":5,\"uses\":2147483647")));
        CHECK("S082", survives(A("\"recharge\":5", "\"recharge\":5,\"targets\":0")));
        CHECK("S083", survives(A("\"recharge\":5", "\"recharge\":5,\"targets\":-4")));
        CHECK("S084", survives(A("\"recharge\":5", "\"recharge\":5,\"shape\":99")));
        CHECK("S085", survives(A("\"recharge\":5", "\"recharge\":5,\"more\":[{\"n\":1,\"s\":6,\"type\":99}]")));
        CHECK("S086", survives(A("\"recharge\":5", "\"recharge\":5,\"more\":5")));
        // не-объекты в списке способностей
        CHECK("S087", survives(rep(K, "\"abilities\":[", "\"abilities\":[null,5,\"x\",[],")));
        CHECK("S088", survives(rep(K, "\"abilities\":[", "\"abilities\":{},\"abilities_\":[")));
        CHECK("S089", survives(rep(K, "\"abilities\":[", "\"abilities\":7,\"abilities_\":[")));
        // способностей слишком много: отказ по лимиту, а не по бюджету и не падение
        {
            std::string many = "{\"budget\":100000,\"abilities\":[";
            for (int i = 0; i < 500; i++) many += std::string(i ? "," : "") + "{\"dmgN\":1,\"dmgS\":6}";
            many += "]}";
            CHECK("S090", rejectsWith(many, "способностей не больше"));
        }
        // контроль и ДЕФЕКТ?: бафф характеристики — sbStat нигде не проверяется (char_spec.h: s.sbStat = j["sbStat"].num(0)),
        // а в бою (actions.h) делается f.b.stats[s.sbStat] += … — выход за границы массива
        const std::string buf0 = "{\"self\":true,\"sbStat\":0,\"sbAmt\":1,\"sbRounds\":3}";
        CHECK("S091", build(withAbility(buf0)).ok);                                        // контроль: корректный бафф принимается
        CHECK("S092", rejects(withAbility("{\"self\":true,\"sbStat\":99,\"sbAmt\":1,\"sbRounds\":3}")));   // ДЕФЕКТ?
        CHECK("S093", rejects(withAbility("{\"self\":true,\"sbStat\":-1,\"sbAmt\":1,\"sbRounds\":3}")));   // ДЕФЕКТ?
        CHECK("S094", rejects(withAbility("{\"self\":true,\"sbStat\":6,\"sbAmt\":1,\"sbRounds\":3}")));    // ДЕФЕКТ? (граница STAT_N)
    }

    // ======================================================================= 5. пассивы, правила, события, состояние героя
    SUITE("Пассивы, правила, события, состояние");
    {
        CHECK("S100", rejectsWith(withTop("\"prof\":1"), "бонус мастерства"));
        CHECK("S101", rejectsWith(withTop("\"prof\":7"), "бонус мастерства"));
        CHECK("S102", survives(withTop("\"prof\":6")));
        CHECK("S103", rejectsWith(withPassive("\"speed\":7"), "шагами по 5"));
        CHECK("S104", rejectsWith(withPassive("\"speed\":-5"), "скорость"));
        CHECK("S105", rejectsWith(withPassive("\"speed\":501"), "скорость"));
        CHECK("S106", survives(withPassive("\"speed\":0")));                               // неподвижный
        CHECK("S107", rejectsWith(withPassive("\"unarmoredStat\":6"), "защиты без доспеха"));
        CHECK("S108", rejectsWith(withPassive("\"unarmoredStat\":-2"), "защиты без доспеха"));
        CHECK("S109", rejectsWith(withPassive("\"lair\":4"), "логова"));
        CHECK("S110", rejectsWith(withPassive("\"lair\":-1"), "логова"));
        CHECK("S111", rejectsWith(withPassive("\"truesight\":7"), "истинное зрение"));
        CHECK("S112", rejectsWith(withPassive("\"dv\":20"), "тёмное зрение"));
        CHECK("S113", rejectsWith(withPassive("\"blindsight\":5"), "слепое зрение"));
        CHECK("S114", rejectsWith(withPassive("\"tools\":41"), "инструменты"));
        CHECK("S115", rejectsWith(withPassive("\"expert\":[3]"), "экспертиза"));
        CHECK("S116", survives(withPassive("\"skills\":[99,-1,1000000,\"x\",null],\"immCond\":[99,-3,0],\"res\":[500],\"imm\":[-9],\"vuln\":[13]")));
        CHECK("S117", rejects(withPassive("\"regenN\":1001")));
        CHECK("S118", rejects(withPassive("\"legRes\":1001")));
        CHECK("S119", rejects(withPassive("\"slots\":[0,1001]")));
        CHECK("S120", survives(withPassive("\"slots\":[0,1,1,1,1,1,1,1,1,1,1,1,1,1]")));    // лишние ячейки
        CHECK("S121", rejectsWith(withPassive("\"aura\":{\"rad\":7,\"n\":1,\"s\":6}"), "радиус ауры"));
        CHECK("S122", rejectsWith(withPassive("\"aura\":{\"rad\":10,\"n\":0,\"cond\":0}"), "аура без урона"));
        CHECK("S123", survives(withPassive("\"rider\":{\"n\":1,\"s\":6}")));           // довесок при оружии допустим
        CHECK("S124", survives(withPassive("\"rider\":{\"n\":1001,\"s\":6}")));
        // правила «когда → что»
        CHECK("S130", rejectsWith(withPassive("\"rules\":[{\"when\":99,\"what\":0}]"), "условие правила"));
        CHECK("S131", rejectsWith(withPassive("\"rules\":[{\"when\":-1,\"what\":0}]"), "условие правила"));
        CHECK("S132", rejectsWith(withPassive("\"rules\":[{\"when\":0,\"what\":99}]"), "эффект правила"));
        CHECK("S133", survives(withPassive("\"rules\":[{\"when\":0,\"what\":1,\"n\":0,\"s\":6}]")));     // «+кубики урона» с 0 кубиков
        CHECK("S134", survives(withPassive("\"rules\":[{\"when\":0,\"what\":1,\"n\":1,\"s\":7}]")));     // кубик не из списка
        {
            std::string many = "\"rules\":[";
            for (int i = 0; i < 13; i++) many += std::string(i ? "," : "") + "{\"when\":0,\"what\":0}";
            many += "]";
            CHECK("S135", rejectsWith(withPassive(many), "не больше 12"));
        }
        CHECK("S136", survives(withPassive("\"rules\":5")));
        CHECK("S137", survives(withPassive("\"rules\":[null,5,[],\"x\"]")));
        // события «событие → эффект»
        CHECK("S140", rejectsWith(withPassive("\"trigs\":[{\"on\":99,\"what\":0}]"), "событие"));
        CHECK("S141", rejectsWith(withPassive("\"trigs\":[{\"on\":0,\"what\":99}]"), "эффект события"));
        CHECK("S142", rejects(withPassive("\"trigs\":[{\"on\":0,\"what\":0,\"s\":7}]")));
        CHECK("S143", rejects(withPassive("\"trigs\":[{\"on\":0,\"what\":0,\"n\":1001}]")));
        CHECK("S144", rejects(withPassive("\"trigs\":[{\"on\":0,\"what\":0,\"b\":1001}]")));
        // состояние героя (кампания)
        CHECK("S150", rejectsWith(withTop("\"hp\":-1"), "здоровье"));
        CHECK("S151", rejectsWith(withTop("\"hp\":10000000"), "дороже остатка"));
        CHECK("S152", survives(withTop("\"hp\":1e30")));
        CHECK("S153", rejectsWith(withTop("\"state\":{\"exh\":6}"), "истощение"));
        CHECK("S154", rejectsWith(withTop("\"state\":{\"hp\":0}"), "0 ХП"));
        CHECK("S155", rejects(withTop("\"state\":{\"hp\":-5}")));                          // отрицательное → 0 → «без сознания»
        CHECK("S156", survives(withTop("\"state\":{\"hp\":5,\"slots\":[0,-3,999999999],\"uses\":[-1,2147483647,5,5,5],\"exh\":-4}")));
        CHECK("S157", survives(withTop("\"state\":5")));
        CHECK("S158", survives(withTop("\"state\":{\"slots\":\"x\",\"uses\":{}}")));
        CHECK("S159", survives(withTop("\"name\":\"" + [] { std::string r; for (int i = 0; i < 50000; i++) r += "яЯ"; return r; }() + "\"")));  // очень длинное имя
        CHECK("S160", survives(withTop("\"name\":\"\\u0000\\ud800\"")));                    // NUL и «одинокий» суррогат в имени
    }

    // ======================================================================= 6. призыв и превращение
    SUITE("Призыв и превращение: вложенность, количество");
    {
        auto S = [&](const std::string& groups) { return withAbility("{\"summon\":{\"groups\":" + groups + "}}"); };
        const std::string g1 = "[{\"n\":2,\"minion\":" + MINION + "}]";
        CHECK("S200", survives(S(g1)));
        CHECK("S201", rejectsWith(S("[]"), "от 1 до 4 групп"));
        CHECK("S202", rejectsWith(S("[{\"n\":1,\"minion\":" + MINION + "},{\"n\":1,\"minion\":" + MINION + "},{\"n\":1,\"minion\":" + MINION +
                                    "},{\"n\":1,\"minion\":" + MINION + "},{\"n\":1,\"minion\":" + MINION + "}]"), "от 1 до 4 групп"));
        CHECK("S203", rejectsWith(S("[{\"n\":0,\"minion\":" + MINION + "}]"), "копий приспешника"));
        CHECK("S204", rejectsWith(S("[{\"n\":21,\"minion\":" + MINION + "}]"), "копий приспешника"));
        CHECK("S205", rejectsWith(S("[{\"n\":-1,\"minion\":" + MINION + "}]"), "копий приспешника"));
        CHECK("S206", survives(S("[{\"n\":1}]")));                                         // приспешника нет совсем (читается как описание по умолчанию)
        CHECK("S207", survives(S("[{\"n\":1,\"minion\":5}]")));
        CHECK("S208", survives(S("[5,null,\"x\"]")));
        CHECK("S209", rejectsWith(S("[{\"n\":1,\"minion\":{\"budget\":5}}]"), "приспешник 1:"));
        CHECK("S210", rejects(S("[{\"n\":20,\"minion\":{\"budget\":100000,\"stats\":[10,10,10,10,10,10]}}]")));   // 20 дорогих копий: не переполнение, а отказ по бюджету
        CHECK("S211", rejectsWith(S("[{\"n\":1,\"minion\":{\"budget\":100,\"abilities\":[{\"summon\":{\"groups\":" + g1 + "}}]}}]"), "сам призывать не может"));
        // превращение
        CHECK("S212", survives(withAbility("{\"form\":{\"body\":" + MINION + "}}")));
        CHECK("S213", rejectsWith(withAbility("{\"form\":{\"body\":{\"budget\":100,\"abilities\":[{\"form\":{\"body\":" + MINION + "}}]}}}"), "в облике нет своих"));
        CHECK("S214", rejects(withAbility("{\"form\":{\"body\":{\"budget\":5}}}")));
        CHECK("S215", survives(withAbility("{\"form\":{\"body\":5}}")));
        CHECK("S216", survives(withAbility("{\"form\":{}}")));
        // очень глубокая цепочка превращений: отказ должен наступить на первом уровне, не уходя в рекурсию
        {
            std::string deep = MINION;
            for (int i = 0; i < 1500; i++) deep = "{\"budget\":100,\"abilities\":[{\"form\":{\"body\":" + deep + "}}]}";
            CHECK("S217", survives(withAbility("{\"form\":{\"body\":" + deep + "}}")));
        }
        // одновременно призыв и превращение
        CHECK("S218", survives(withAbility("{\"summon\":{\"groups\":" + g1 + "},\"form\":{\"body\":" + MINION + "}}")));
    }

    // ======================================================================= 7. корни JSON, ответы priceReply / catalogReply
    SUITE("Ответы сервера: priceReply / catalogReply");
    {
        const char* roots[] = {"[]", "null", "42", "\"x\"", "true", "{}", "[{}]", "{\"budget\":{}}"};
        int k = 0;
        for (const char* r : roots) {
            const std::string id = "S3" + std::to_string(10 + k++);
            cur(id);
            bool ok = false;
            const Json j = parseJson(r, &ok);
            const std::string reply = chr::priceReply(j, C);       // не падает и всегда возвращает разбираемый JSON
            CHECK(id.c_str(), ok && jsonOk(reply));
        }
        // каталог для любых бюджетов — разбираемый JSON (в нём не должно быть inf / nan)
        const long long budgets[] = {0, -1, 1, 19, 20, 100000, 1000000, 1LL << 40};
        k = 0;
        for (long long b : budgets) {
            const std::string id = "S3" + std::to_string(30 + k++);
            cur(id);
            CHECK(id.c_str(), jsonOk(chr::catalogReply(b, C)));    // ДЕФЕКТ? для 0, −1: g_B = 0 → log(0) в tierN → inf в ценах
        }
        // priceReply на «плохом» персонаже тоже разбираем, с ok:false и причиной
        {
            bool ok = false;
            const Json r = parseJson(chr::priceReply(parseJson(rep(K, "[16,10,14,8,10,8]", "[99,10,14,8,10,8]")), C), &ok);
            CHECK("S350", ok && !r["ok"].flag() && !r["err"].str().empty());
        }
    }

    // ======================================================================= 8. пользовательская карта
    SUITE("Пользовательская карта: мусор в слоях и стартах");
    {
        auto grid = [](int w, int h, char ch) {
            std::string s = "[";
            for (int y = 0; y < h; y++) s += std::string(y ? "," : "") + "\"" + std::string(w, ch) + "\"";
            return s + "]";
        };
        auto M = [&](const std::string& body) { CustomMap m; std::string err; const bool ok = parseCustomMap(parseJson(body), m, err); return std::make_pair(ok, err); };
        auto N = [&](const std::string& body) { M(body); return true; };   // «просто не падает»
        CHECK("S400", M("{\"aw\":10,\"ah\":10}").first);                                   // слоёв нет совсем — вся карта свободна
        CHECK("S401", M("{\"aw\":10,\"ah\":10,\"wall\":" + grid(10, 10, '0') + "}").first);
        CHECK("S402", !M("{\"aw\":201,\"ah\":10}").first && !M("{\"aw\":10,\"ah\":201}").first);   // предел арены W = 200
        CHECK("S403", !M("{\"aw\":-10,\"ah\":10}").first);
        CHECK("S404", !M("{\"aw\":100000,\"ah\":100000}").first);
        CHECK("S405", !M("{\"aw\":\"10\",\"ah\":\"10\"}").first);                           // числа строками → 0 → отказ
        CHECK("S406", !M("{\"aw\":10.5,\"ah\":null}").first);
        CHECK("S407", !M("[]").first && !M("null").first && !M("5").first);
        CHECK("S408", N("{\"aw\":10,\"ah\":10,\"wall\":5}"));            // слой — число: не падает
        CHECK("S409", N("{\"aw\":10,\"ah\":10,\"wall\":[5,null,[],{}]}"));
        CHECK("S410", N("{\"aw\":10,\"ah\":10,\"wall\":[\"1\",\"юникод\",\"\"]}"));   // короткие и многобайтовые строки
        CHECK("S411", N("{\"aw\":10,\"ah\":10,\"height\":" + grid(10, 10, '9') + ",\"deck\":" + grid(10, 10, ':') + ",\"door\":" + grid(10, 10, 'x') + ",\"haz\":" + grid(10, 10, '~') + "}"));
        CHECK("S412", M("{\"aw\":10,\"ah\":10,\"wall\":" + grid(500, 500, '0') + "}").first);   // строки длиннее карты обрезаются
        CHECK("S413", N("{\"aw\":10,\"ah\":10,\"starts\":\"A\"}"));
        CHECK("S414", N("{\"aw\":10,\"ah\":10,\"starts\":[[1],[2]]}"));
        CHECK("S415", !M("{\"aw\":10,\"ah\":10,\"starts\":[[-1,-1],[0,0]]}").first);        // −1 — «не задана», но задана вторая
        CHECK("S416", !M("{\"aw\":10,\"ah\":10,\"starts\":[[1e30,1],[2,2]]}").first);
        CHECK("S417", !M("{\"aw\":10,\"ah\":10,\"starts\":[[0,0],[10,10]]}").first);        // за краем (aw = 10 → индексы 0…9)
        CHECK("S418", M("{\"aw\":10,\"ah\":10,\"starts\":[[0,0],[9,9]]}").first);           // граница допустима
        // клетки-ловушки: старт на стене, на дереве, в ущелье, на двери
        {
            std::string wall = grid(10, 10, '0');
            wall[2] = '1';   // клетка (0,0) — стена
            CHECK("S419", !M("{\"aw\":10,\"ah\":10,\"wall\":" + wall + ",\"starts\":[[0,0],[5,5]]}").first);
            CHECK("S420", !M("{\"aw\":10,\"ah\":10,\"tree\":" + grid(10, 10, '1') + ",\"starts\":[[0,0],[5,5]]}").first);
            CHECK("S421", !M("{\"aw\":10,\"ah\":10,\"pit\":" + grid(10, 10, '1') + ",\"starts\":[[0,0],[5,5]]}").first);
            CHECK("S422", !M("{\"aw\":10,\"ah\":10,\"door\":" + grid(10, 10, '1') + ",\"starts\":[[0,0],[5,5]]}").first);
        }
        // карта предельного размера: проверка связности укладывается во время (нет квадратичного взрыва)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const bool ok = M("{\"aw\":200,\"ah\":200,\"wall\":" + grid(200, 200, '0') + "}").first;
            CHECK("S423", ok && secondsSince(t0) < 10.0);
        }
        // «шахматная» карта — тысячи изолированных клеток: отказ без зависания
        {
            std::string rows = "[";
            for (int y = 0; y < 100; y++) {
                std::string r;
                for (int x = 0; x < 100; x++) r += ((x + y) % 2) ? '1' : '0';
                rows += std::string(y ? "," : "") + "\"" + r + "\"";
            }
            rows += "]";
            const auto t0 = std::chrono::steady_clock::now();
            const auto res = M("{\"aw\":100,\"ah\":100,\"wall\":" + rows + "}");
            CHECK("S424", secondsSince(t0) < 10.0 && (res.first || !res.second.empty()));
        }
    }

    // ======================================================================= 9. бой и генератор
    SUITE("Бой и генератор: вырожденные бойцы");
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
        auto valid = [](int r) { return r >= 0 && r <= 2; };
        rng.seed(1);
        const auto t0 = std::chrono::steady_clock::now();
        CHECK("S500", fight(duelist(10, 0, 0), duelist(10, 0, 0)) == 2);                    // оба безоружны: бой кончается ничьёй, а не вечностью
        CHECK("S501", valid(fight(duelist(0, 1, 6), duelist(0, 1, 6))));                    // у обоих 0 ХП
        CHECK("S502", valid(fight(duelist(-5, 1, 6), duelist(30, 1, 6))));                  // отрицательные ХП
        CHECK("S503", valid(fight(duelist(1, 1000, 100), duelist(1, 1000, 100))));          // огромный урон за удар
        CHECK("S504", valid(fight(duelist(2147483647, 1, 4), duelist(2147483647, 1, 4)))); // ХП = INT_MAX: ничья по лимиту, без переполнения
        {
            Boss a = duelist(20, 1, 6), b = duelist(20, 1, 6);
            a.speed = b.speed = 0;                                                          // оба неподвижны, дальности не хватает
            a.wRange = b.wRange = 5;
            CHECK("S505", valid(fight(a, b)));
        }
        {
            Boss a = duelist(20, 1, 6), b = duelist(20, 1, 6);
            a.stats = {1, 1, 1, 1, 1, 1};
            b.stats = {40, 40, 40, 40, 40, 40};
            CHECK("S506", valid(fight(a, b)));
        }
        {
            Boss a = duelist(20, 1, 6), b = duelist(20, 1, 6);
            a.ac = 1000;
            b.ac = -1000;
            CHECK("S507", valid(fight(a, b)));
        }
        {
            Boss a = duelist(20, 1, 6), b = duelist(20, 1, 6);
            a.wRange = 500;                                                                 // дальность во всю карту
            CHECK("S508", valid(fight(a, b)));
        }
        CHECK("S509", secondsSince(t0) < 60.0);                                             // все вырожденные бои вместе — быстро (лимит раундов работает)
        // генератор на краях допустимого диапазона бюджетов
        {
            bool okLow = true, okHigh = true;
            for (int i = 0; i < 30; i++) {
                rng.seed(7000 + i);
                const Boss lo = makeBoss(MIN_BUDGET, C, false);
                if (lo.hp < 1) okLow = false;
                for (int s = 0; s < STAT_N; s++)
                    if (lo.stats[s] < 1 || lo.stats[s] > STAT_MAX) okLow = false;
            }
            const auto tg = std::chrono::steady_clock::now();
            for (int i = 0; i < 3; i++) {
                rng.seed(8000 + i);
                const Boss hi = makeBoss(MAX_BUDGET, C, true);
                if (hi.hp < 1 || hi.cost - STAT_BASE > MAX_BUDGET) okHigh = false;
            }
            CHECK("S510", okLow);
            CHECK("S511", okHigh && secondsSince(tg) < 60.0);
        }
        // описание босса — на любых значениях, без выхода за таблицы названий
        {
            Boss b = duelist(10, 1, 6);
            b.size = 99;
            b.ctype = -4;
            b.armor = 77;
            CHECK("S512", !describeBoss(b).empty());                                        // describeBoss зажимает индексы std::clamp
        }
    }

    // ======================================================================= 10. кампания: деньги и уровни
    SUITE("Кампания: деньги и уровни на мусоре");
    {
        const char* junk[] = {"", " ", "зм", "см", "мм", "abc", "-5 зм", "5 зм 5 зм", "1 зм -3 см", "99999999999999999999 зм", "9223372036854775807 зм",
                              "1e30 зм", "0x10 зм", "5 xx", "5зм", "зм 5", "\t\n", "1 зм 5 см 3 мм мусор", "١٢٣ зм"};
        int k = 0;
        for (const char* t : junk) {
            const std::string id = "S6" + std::to_string(10 + k++);
            cur(id);
            const long long v = kmp::parseMoney(t);                                         // не падает; сумма никогда не отрицательна
            CHECK(id.c_str(), v >= 0);
        }
        CHECK("S640", !kmp::money(0).empty() && !kmp::money(-5).empty() && !kmp::money(LLONG_MAX).empty());
        CHECK("S641", kmp::levelOf(0) == 1 && kmp::levelOf(-1) == 1 && kmp::levelOf(LLONG_MIN) == 1);   // log(≤0) → NaN → int: ДЕФЕКТ? если красное
        CHECK("S642", kmp::levelOf(LLONG_MAX) >= 50 && kmp::levelOf(1000000000000LL) >= 50);
        CHECK("S643", std::isfinite(kmp::levelBudget(0)) && std::isfinite(kmp::levelBudget(-1)) && std::isfinite(kmp::levelBudget(1000)));
        CHECK("S644", kmp::levelBudget(51) >= kmp::levelBudget(50));
    }

    // ======================================================================= 11. кампания: события на враждебном вводе
    SUITE("Кампания: события на враждебном вводе");
    {
        const kmp::Content ct;
        // пустой отряд: везде, где считают «среднее по героям», возможно деление на ноль
        {
            const char* acts[] = {"{\"type\":\"day\",\"terrain\":\"Лес\"}", "{\"type\":\"day\",\"terrain\":\"Лес\",\"road\":true,\"pace\":\"быстрый\"}",
                                  "{\"type\":\"rest\",\"kind\":\"долгий\"}", "{\"type\":\"rest\",\"kind\":\"короткий\"}",
                                  "{\"type\":\"check\",\"hero\":\"Лис\",\"what\":\"Убеждение\",\"dc\":15}", "{\"type\":\"buy\",\"item\":\"конь\",\"price\":\"75 зм\"}",
                                  "{\"type\":\"attitude\",\"npc\":\"Ролан\",\"delta\":3}", "{\"type\":\"dialog\",\"npc\":\"Ролан\",\"node\":\"x\"}",
                                  "{\"type\":\"setup\",\"purse\":\"50 зм\",\"food\":30,\"water\":30}"};
            int k = 0;
            for (const char* a : acts) {
                const std::string id = "S7" + std::to_string(10 + k++);
                cur(id);
                kmp::State s = fresh(5, 0);
                act(s, ct, a);                                                              // результат не важен — важно, что процесс жив
                CHECK(id.c_str(), stateOk(s));
            }
        }
        // мусорные события
        {
            const char* acts[] = {"{}", "[]", "5", "null", "\"day\"", "{\"type\":5}", "{\"type\":null}", "{\"type\":\"zzz\"}", "{\"type\":\"day\",\"terrain\":5}",
                                  "{\"type\":\"day\",\"hours\":-5}", "{\"type\":\"day\",\"hours\":0}", "{\"type\":\"day\",\"hours\":1000000}", "{\"type\":\"day\",\"hours\":\"abc\"}",
                                  "{\"type\":\"day\",\"pace\":\"ползком\"}", "{\"type\":\"day\",\"terrain\":\"\"}", "{\"type\":\"day\",\"roles\":{\"Призрак\":\"проводник\"}}",
                                  "{\"type\":\"day\",\"roles\":5}", "{\"type\":\"check\"}", "{\"type\":\"check\",\"hero\":\"Лис\",\"what\":\"\",\"dc\":-999}",
                                  "{\"type\":\"check\",\"hero\":\"Лис\",\"what\":\"Убеждение\",\"dc\":2147483647}", "{\"type\":\"check\",\"hero\":\"Призрак\",\"what\":\"Убеждение\",\"dc\":10}",
                                  "{\"type\":\"contest\",\"hero\":\"Лис\",\"vs\":\"\",\"vsMod\":1e30}", "{\"type\":\"rest\",\"kind\":\"вечный\"}", "{\"type\":\"rest\",\"kind\":\"короткий\",\"spend\":{\"Лис\":-3}}",
                                  "{\"type\":\"rest\",\"kind\":\"короткий\",\"spend\":{\"Лис\":2147483647}}", "{\"type\":\"rest\",\"kind\":\"короткий\",\"spend\":[]}",
                                  "{\"type\":\"gm\",\"hero\":\"Лис\",\"exh\":-1}", "{\"type\":\"gm\",\"hero\":\"Лис\",\"exh\":1000}", "{\"type\":\"gm\",\"hero\":\"Лис\",\"hp\":-50}",
                                  "{\"type\":\"gm\",\"hero\":\"Лис\",\"hp\":2147483647}", "{\"type\":\"gm\",\"hero\":\"Призрак\",\"exh\":3}", "{\"type\":\"gm\",\"purse\":-9223372036854775807}",
                                  "{\"type\":\"gm\",\"pos\":[1e30,-1e30]}", "{\"type\":\"gm\",\"pos\":[\"a\"]}", "{\"type\":\"route\",\"path\":[[1e30,1e30]]}", "{\"type\":\"route\",\"path\":5}",
                                  "{\"type\":\"route\",\"path\":[]}", "{\"type\":\"fog\",\"op\":\"скрыть\",\"x\":1e30,\"y\":-1,\"r\":-5}", "{\"type\":\"fog\",\"op\":\"скрыть\",\"x\":0,\"y\":0,\"r\":1e30}",
                                  "{\"type\":\"turnin\",\"quest\":\"нет такого\",\"fact\":\"x\"}", "{\"type\":\"quest\",\"id\":\"Q\",\"base\":\"-1000 зм\",\"step\":-5,\"confirm\":1e30}",
                                  "{\"type\":\"quest\",\"id\":\"Q\",\"base\":\"1000 зм\",\"step\":0,\"confirm\":0}", "{\"type\":\"learn\",\"fact\":\"\"}", "{\"type\":\"dialog\",\"node\":\"\"}"};
            int k = 0;
            for (const char* a : acts) {
                const std::string id = "S7" + std::to_string(100 + k++);
                cur(id);
                kmp::State s = fresh(11 + k);
                s.food = s.water = 100;
                act(s, ct, a);
                CHECK(id.c_str(), stateOk(s));                                              // герои в границах, состояние сериализуется в верный JSON
            }
        }
        // экономика: покупка и продажа не должны приносить деньги из ничего (отрицательное количество и цена)
        {
            auto purseAfter = [&](const std::string& a, bool buyer) {
                kmp::State s = fresh();
                s.purse = 10000;
                const long long p0 = s.purse;
                if (!buyer) s.inv["меч"] = 1;
                act(s, ct, a);
                return std::make_pair(p0, s.purse);
            };
            auto r1 = purseAfter("{\"type\":\"buy\",\"item\":\"конь\",\"qty\":-5,\"price\":\"75 зм\"}", true);
            CHECK("S800", r1.second <= r1.first);                                           // покупка с qty < 0 не пополняет кошелёк
            auto r2 = purseAfter("{\"type\":\"buy\",\"item\":\"конь\",\"qty\":1,\"price\":\"-75 зм\"}", true);
            CHECK("S801", r2.second <= r2.first);
            auto r3 = purseAfter("{\"type\":\"buy\",\"item\":\"конь\",\"qty\":2147483647,\"price\":\"75 зм\"}", true);
            CHECK("S802", r3.second >= 0 && r3.second <= r3.first);                         // нет переполнения qty × цена
            auto r4 = purseAfter("{\"type\":\"sell\",\"item\":\"меч\",\"qty\":-3,\"price\":\"15 зм\"}", false);
            CHECK("S803", r4.second <= r4.first);                                           // продажа qty < 0 не даёт денег
            auto r5 = purseAfter("{\"type\":\"sell\",\"item\":\"копьё\",\"qty\":1,\"price\":\"15 зм\"}", false);
            CHECK("S804", r5.second <= r5.first);                                           // продажа предмета, которого нет
            auto r6 = purseAfter("{\"type\":\"sell\",\"item\":\"меч\",\"qty\":5,\"price\":\"15 зм\"}", false);
            CHECK("S805", r6.second <= r6.first + 750);                                     // продаёшь больше, чем есть: максимум за один меч (половина цены)
        }
        // длинные серии дней: ресурсы не уходят в минус, день растёт, ничего не взрывается
        {
            kmp::State s = fresh(3);
            s.food = 5;
            s.water = 5;
            s.purse = 10000;
            bool fine = true;
            for (int i = 0; i < 200 && fine; i++) {
                act(s, ct, "{\"type\":\"day\",\"terrain\":\"Горы\"}");
                fine = stateOk(s) && s.food >= 0 && s.water >= 0;
            }
            CHECK("S810", fine);
        }
        // загрузка из журнала с мусором: ошибка вместо падения, состояние до этого не портится
        {
            kmp::State s = fresh();
            std::string err;
            const char* evs[] = {"{}", "{\"n\":1}", "{\"n\":1,\"type\":5}", "{\"n\":-1,\"type\":\"day\"}", "{\"n\":1,\"type\":\"hero\"}",
                                 "{\"n\":1,\"type\":\"hero\",\"name\":\"X\",\"char\":5}", "{\"n\":1,\"type\":\"hero\",\"name\":\"X\",\"char\":{\"budget\":0}}", "[]", "null"};
            int k = 0;
            for (const char* e : evs) {
                const std::string id = "S82" + std::to_string(k++);
                cur(id);
                const kmp::State bak = s;
                const bool ok = kmp::apply(s, C, parseJson(e), err);
                if (!ok) s = bak;
                CHECK(id.c_str(), stateOk(s));
            }
        }
    }

    // ======================================================================= 12. JSON-разборщик под нагрузкой (в конце: может уронить процесс)
    SUITE("mini_json.h: враждебные входные данные");
    {
        bool ok = true;
        auto bad = [&](const std::string& s) { parseJson(s, &ok); return !ok; };
        CHECK("S900", bad("\"abc") && bad("{\"a\":") && bad("[") && bad("{") && bad("}") && bad("]") && bad("tru") && bad("nul"));
        CHECK("S901", (bad("\"\\u12\""), true));                                            // неполная \u-последовательность: не падает
        CHECK("S902", (parseJson("{\"a\":1,}", &ok), true) && (parseJson("[1,]", &ok), true) && (parseJson("01", &ok), true) && (parseJson("-", &ok), true));
        CHECK("S903", (parseJson("1e999", &ok), true) && (parseJson("-1e999", &ok), true) && (parseJson("1e-999", &ok), true));
        {
            const Json inf = parseJson("{\"a\":1e999,\"b\":-1e999,\"c\":123456789012345678901234567890}", &ok);
            CHECK("S904", (inf["a"].num(), inf["b"].num(), inf["c"].num(), true));          // num() от бесконечности и огромных чисел — без падения
        }
        CHECK("S905", (parseJson(std::string(400, '9'), &ok), true));                        // число на 400 цифр
        CHECK("S906", (parseJson(std::string("\"a\0b\"", 5), &ok), true));                   // NUL внутри строки
        CHECK("S907", (parseJson("\"\xff\xfe\xc0\x80\"", &ok), true));                       // недопустимый UTF-8
        CHECK("S908", (parseJson("\"\\ud800\"", &ok), true) && (parseJson("\"\\udc00\\ud800\"", &ok), true) && (parseJson("\"\\uZZZZ\"", &ok), true));
        {
            const auto t0 = std::chrono::steady_clock::now();
            parseJson("\"" + std::string(1 << 22, 'a') + "\"", &ok);                         // строка 4 МБ
            CHECK("S909", ok && secondsSince(t0) < 5.0);
        }
        {
            std::string big = "[";
            for (int i = 0; i < 300000; i++) big += std::string(i ? "," : "") + std::to_string(i);
            big += "]";
            const auto t0 = std::chrono::steady_clock::now();
            const Json j = parseJson(big, &ok);
            CHECK("S910", ok && j.size() == 300000 && secondsSince(t0) < 5.0);              // 300 тыс. элементов: нет квадратичного роста
        }
        {
            std::string big = "{";
            for (int i = 0; i < 50000; i++) big += std::string(i ? "," : "") + "\"k" + std::to_string(i) + "\":" + std::to_string(i);
            big += "}";
            const auto t0 = std::chrono::steady_clock::now();
            const Json j = parseJson(big, &ok);
            CHECK("S911", ok && secondsSince(t0) < 10.0 && j["k49999"].num(-1) == 49999);   // 50 тыс. ключей: поиск не убивает время
        }
        // глубокая вложенность: либо разобрано, либо отказ — но не переполнение стека
        std::printf("  (дальше глубокая вложенность: при переполнении стека процесс упадёт — id проверки будет в сообщении)\n");
        for (int depth : {1000, 10000, 100000}) {
            const std::string id = "S92" + std::to_string(depth == 1000 ? 0 : depth == 10000 ? 1 : 2);
            cur(id);
            const std::string arr = std::string(depth, '[') + "1" + std::string(depth, ']');
            const std::string obj = [&] { std::string s; for (int i = 0; i < depth; i++) s += "{\"a\":"; s += "1"; s += std::string(depth, '}'); return s; }();
            parseJson(arr, &ok);
            parseJson(obj, &ok);
            CHECK(id.c_str(), true);
        }
    }

    std::printf("\n==========================================\nИтого: PASS %d, FAIL %d\n", g_pass, g_fail);
    for (auto& f : g_failed) std::printf("  провал: %s\n", f.c_str());
    return g_fail ? 1 : 0;
}
