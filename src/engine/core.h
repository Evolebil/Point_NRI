// Основа: случайные числа, политика бота (Pol), типы урона, пределы, бюджет и единица цены
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <memory>
#include "dnd_root.h"
#include "gru.h"

// Свой генератор (xorshift64*, зерно через splitmix64): результат по зерну одинаков на любом компиляторе и в любой
// стандартной библиотеке.
struct Rng {
    uint64_t s = 0x9E3779B97F4A7C15ULL;
    Rng(uint64_t sd = 42) { seed(sd); }
    void seed(uint64_t sd) {
        uint64_t z = sd + 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        s = z ^ (z >> 31);
        if (!s) s = 0x9E3779B97F4A7C15ULL;
    }
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 0x2545F4914F6CDD1DULL;
    }
    uint32_t u32() { return (uint32_t)(next() >> 32); }
    // 0..n-1
    uint32_t below(uint32_t n) { return (uint32_t)(((uint64_t)u32() * n) >> 32); }
    // [0,1)
    double unit() { return (double)(next() >> 11) * (1.0 / 9007199254740992.0); }
};
thread_local Rng rng(42);
int rollD20() {
    return 1 + (int)rng.below(20);
}
int rollDie(int s) {
    return s <= 0 ? 0 : 1 + (int)rng.below((uint32_t)s);
}
int rollDice(int n, int s) {
    int t = 0;
    for (int i = 0; i < n; i++) t += rollDie(s);
    return t;
}
double rand01() {
    return rng.unit();
}
int irand(int n) {
    return (int)rng.below((uint32_t)n);
}
// N(0,1), Бокс–Мюллер
double randn() {
    double u = 1.0 - rng.unit(), v = rng.unit();
    return std::sqrt(-2.0 * std::log(u)) * std::cos(6.283185307179586 * v);
}

// клеток по стороне (по 5 фт)
const int W = 200;   // предел арены: карты редактора — до 200×200, арены из мира — до 100×100
// маска атомов: 256 бит (64, потом 128 не хватило). Ведёт себя как целое: сдвиги, &, |, сравнение, проверка в условии
struct AMask {
    uint64_t w[4] = {0, 0, 0, 0};
    AMask(unsigned long long v = 0) { w[0] = v; }
    AMask operator<<(int n) const {   // сдвиг словами и битами
        AMask r(0);
        const int q = n >> 6, b = n & 63;
        for (int k = 3; k >= q; k--) {
            uint64_t v = w[k - q] << b;
            if (b && k - q - 1 >= 0) v |= w[k - q - 1] >> (64 - b);
            r.w[k] = v;
        }
        return r;
    }
    AMask operator>>(int n) const {
        AMask r(0);
        const int q = n >> 6, b = n & 63;
        for (int k = 0; k + q < 4; k++) {
            uint64_t v = w[k + q] >> b;
            if (b && k + q + 1 < 4) v |= w[k + q + 1] << (64 - b);
            r.w[k] = v;
        }
        return r;
    }
    AMask operator&(const AMask& o) const { AMask r(0); for (int k = 0; k < 4; k++) r.w[k] = w[k] & o.w[k]; return r; }
    AMask operator|(const AMask& o) const { AMask r(0); for (int k = 0; k < 4; k++) r.w[k] = w[k] | o.w[k]; return r; }
    AMask& operator|=(const AMask& o) { for (int k = 0; k < 4; k++) w[k] |= o.w[k]; return *this; }
    bool operator==(const AMask& o) const { return w[0] == o.w[0] && w[1] == o.w[1] && w[2] == o.w[2] && w[3] == o.w[3]; }
    bool operator!=(const AMask& o) const { return !(*this == o); }
    explicit operator bool() const { return w[0] | w[1] | w[2] | w[3]; }
};
static const AMask ONE = 1;
static_assert(true, "AMask — 256 атомов");
const int PROF = 2;   // бонус мастерства по умолчанию (v21: покупается до +6, Boss::prof)
// Размеры кубиков (v21, №11): единственный список для оружия, способностей, урона по времени, ауры и довеска.
// Цены считаются от среднего (грань+1)/2, поэтому новый размер не требует своих коэффициентов.
const int DICE[] = {2, 4, 6, 8, 10, 12, 20, 100};
const int DICE_N = (int)(sizeof DICE / sizeof DICE[0]);
inline bool isDie(int s) {
    for (int d : DICE)
        if (d == s) return true;
    return false;
}
// ниже собрать босса нельзя (минимум 1 ХП = 2 очка и т.д.)
// Минимальный бюджет босса: каждая из шести характеристик стоит не меньше 1, ХП не меньше 1 (резерв RESERVE = 8 очков), плюс до 45% бюджета на способности.
const long long MIN_BUDGET = 20;
// Бюджет — любое целое от MIN_BUDGET до MAX_BUDGET (v21, №1: без групп). Все цены и пороги считаются от g_B, поэтому годятся
// для любого бюджета; 200…6400 — только точки калибровки. MAX_BUDGET — технический предел (ХП в int, длина боя).
const long long MAX_BUDGET = 100000;
const long long RESERVE = 8;
// 'без ограничения' (раньше было 99 — кончалось на длинных боях)
const int UNL = 1000000;
// бюджет босса, которого сейчас собираем (для масштаба цен)
thread_local double g_B = 800;

// ---- Политика бота: все пороги и веса решений в одном векторе (учатся самообучением) ----
enum PolIdx {
    PO_FOCUSFT,
    PO_TGTMAIN,
    PO_TGTKILL,
    PO_DISENG,
    PO_TELE,
    PO_HEAL,
    PO_SUMMON,
    PO_BUFFD,
    PO_RECK,
    PO_CONDS,
    PO_LRFRAC,
    PO_UNCANNY,
    PO_PUSHW,
    PO_DOTW,
    PO_TERRW,
    PO_PARRYW,
    PO_KITE,
    PO_DODGE,
    PO_WALLW,
    PO_N
};
// base — исходные «ручные» значения (до обучения)
struct PolDef {
    const char* name;
    double base, lo, hi;
};
const PolDef PD[PO_N] = {// бонус дальности боссам при выборе фокуса (фт)
                         {"focusFt", 15, 0, 60},
                         // надбавка к ценности цели-боссу
                         {"tgtMain", 0.5, 0, 2},
                         // вес «добить цель» (доля ХП, которую снимет удар)
                         {"tgtKill", 1.0, 0, 3},
                         // ХП, ниже которого дальнобойный отходит от ближника
                         {"disengHp", 0.25, 0, 0.6},
                         // ХП, ниже которого телепорт/скрытность на побег
                         {"teleHp", 0.3, 0, 0.7},
                         // ХП, ниже которого лечение
                         {"healHp", 0.5, 0, 0.95},
                         // во сколько раз волна приспешников должна быть ценнее лучшего действия
                         {"summonFac", 2.0, 0.3, 8},
                         // дальность до врага для баффов: множитель на 2×скорость
                         {"buffDist", 1.0, 0.2, 4},
                         // ХП, выше которого «безрассудная атака»
                         {"reckHp", 0.4, 0, 1},
                         // общий вес ценности состояний
                         {"condScale", 1.0, 0.1, 4},
                         // доля текущих ХП, от которой тратится легендарное сопротивление
                         {"lrFrac", 0.25, 0.03, 1},
                         // доля макс. ХП, от которой тратится Невероятное уклонение
                         {"uncannyFrac", 0.1, 0, 0.6},
                         // веса толчка, урона по времени, местности
                         {"pushW", 0.15, 0, 1.5},
                         {"dotW", 1.0, 0, 4},
                         {"terrW", 0.2, 0, 1.5},
                         // ожидание Парирования цели
                         {"parryW", 0.7, 0, 1.2},
                         // кайтинг: 0 — выкл; иначе множитель скорости ближника, внутри которого дальнобойный отступает
                         {"kite", 0.0, 0, 2.5},
                         // ХП, ниже которого вместо атаки Уклонение (0 — выкл)
                         {"dodgeHp", 0.0, 0, 0.6},
                         // v21: ценность барьера перед ближником (доля его ожидаемого урона за раунд барьера)
                         {"wallW", 0.5, 0, 2}};
// обученные значения (по умолчанию): v19 — переобучены на настоящих миньонах, среднее двух зёрен (7 и 11); прежние
// обученные: {15, 0.5191, 1, 0.25, 0.291, 0.5, 2, 2.1017, 0.4, 0.3413, 0.0767, 0.1, 0.1091, 1.067, 0.2, 0.7, 0, 0}
const double POL_LEARNED[PO_N] = {38.3013, 0.7591, 0.602,  0.1452, 0.3287, 0.2933, 4.5909, 1.6772, 0.3996,
                                  0.9575,  0.2673, 0.0751, 0.0078, 0.1661, 0.0069, 0.762,  0.6052, 0,
                                  0.5};   // wallW (v21) не обучен
struct Pol {
    double v[PO_N];
    Pol() {
        for (int i = 0; i < PO_N; i++) v[i] = POL_LEARNED[i];
    }
    static Pol baseline() {
        Pol p;
        for (int i = 0; i < PO_N; i++) p.v[i] = PD[i].base;
        return p;
    }
};
Pol g_pol;
// ---- Типы урона (13 как в PHB) ----
enum DT {
    DT_ACID,
    DT_BLUDGEONING,
    DT_COLD,
    DT_FIRE,
    DT_FORCE,
    DT_LIGHTNING,
    DT_NECROTIC,
    DT_PIERCING,
    DT_POISON,
    DT_PSYCHIC,
    DT_RADIANT,
    DT_SLASHING,
    DT_THUNDER,
    DT_N
};
const char* DTN[DT_N] = {"кислота",       "дробящий",  "холод",   "огонь", "силовое поле",
                         "электричество", "некротика", "колющий", "яд",    "психический",
                         "излучение",     "рубящий",   "звук"};
inline bool isPhys(int t) {
    return t == DT_BLUDGEONING || t == DT_PIERCING || t == DT_SLASHING;
}
// Доля урона каждого типа в выборке (цена защиты по типу ∝ доле). Уточняется при калибровке по реальным боям.
double g_share[DT_N] = {0.022, 0.185, 0.068, 0.061, 0.021, 0.029, 0.031, 0.250, 0.034, 0.018, 0.029, 0.237, 0.017};
// доля НЕмагического физического урона (для защит «от немагического оружия»)
double g_nmShare = 0.595;
thread_local bool g_track = false;
thread_local double g_dmgT[DT_N] = {};
thread_local double g_dmgNM = 0;
// Как часто встречаются типы у атакующих способностей и в защитах (веса, не вероятности; частоты по бестиарию)
//                              кисл  дроб  хол   огонь сила  элек  некр  кол   яд    псих  изл   руб   звук
// не физические
const double AB_W[DT_N] = {0.07, 0.00, 0.10, 0.20, 0.05, 0.10, 0.10, 0.00, 0.10, 0.06, 0.10, 0.00, 0.06};
const double RES_W[DT_N] = {0.04, 0.06, 0.12, 0.12, 0.02, 0.10, 0.10, 0.06, 0.10, 0.04, 0.06, 0.06, 0.06};
const double IMM_W[DT_N] = {0.04, 0.02, 0.08, 0.10, 0.01, 0.05, 0.08, 0.02, 0.34, 0.13, 0.03, 0.02, 0.03};
const double VUL_W[DT_N] = {0.02, 0.12, 0.10, 0.35, 0.01, 0.05, 0.03, 0.04, 0.01, 0.02, 0.15, 0.02, 0.08};

// Лимита раундов в правилах нет: бой идёт до смерти одного из боссов. Технический предохранитель от бесконечного боя (взаимная неуязвимость,
// вечный отход и т.п.): не закончившийся за g_maxRounds раундов бой считается ничьёй (g_capped считает такие бои).
int g_maxRounds = 500;

// ---- Предохранители и растяжки (limity.json). Предохранитель — жёсткий технический потолок, блокирует. Растяжка — порог
// ниже потолка, ничего не блокирует, пишет WARN в журнал (stderr: «WARN {...}») и в события реплея. Игровых лимитов нет:
// число призванных ограничивает цена способности; потолки только страхуют от вырожденных боёв. Файл — плоский JSON
// {"имя": число}; путь — DND_LIMITS (по умолчанию limity.json рядом с программой); нет файла — значения ниже.
struct Limits {
    int sumAliveAbil = 20, sumFightAbil = 50, sumAliveBoss = 40, sumFightBoss = 100;   // потолки призыва
    int warnAliveAbil = 6, warnFightAbil = 15, warnAliveBoss = 12, warnFightBoss = 30, warnTurnSummons = 3;   // растяжки
};
inline const Limits& limits() {
    static const Limits L = [] {
        Limits l;
        const char* e = std::getenv("DND_LIMITS");
        const std::string path = e ? std::string(e) : dndPath("config/limity.json");
        std::ifstream f(path);
        if (!f) {
            dndMissing(path, "потолки и растяжки призыва — по умолчанию");
            return l;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        const std::string t = ss.str();
        auto num = [&](const char* key, int& v) {
            const std::string k = std::string("\"") + key + "\"";
            const size_t p = t.find(k);
            if (p == std::string::npos) return;
            const size_t c = t.find(':', p + k.size());
            if (c != std::string::npos) v = std::atoi(t.c_str() + c + 1);
        };
        num("призвано_живых_от_способности", l.sumAliveAbil);
        num("призвано_за_бой_способностью", l.sumFightAbil);
        num("призвано_живых_у_босса", l.sumAliveBoss);
        num("призвано_за_бой_у_босса", l.sumFightBoss);
        num("растяжка_живых_от_способности", l.warnAliveAbil);
        num("растяжка_за_бой_способностью", l.warnFightAbil);
        num("растяжка_живых_у_босса", l.warnAliveBoss);
        num("растяжка_за_бой_у_босса", l.warnFightBoss);
        num("растяжка_призывов_за_ход", l.warnTurnSummons);
        return l;
    }();
    return L;
}
// номер боя в потоке (для записей растяжек) и счётчик срабатываний; в stderr — первые 50 за запуск (DND_WARN_ALL — все)
thread_local long long g_fightNo = 0;
std::atomic<long long> g_warnCount{0};
void evPush(const std::string& json);
extern thread_local int g_curRound;
void tripwire(const char* name, int threshold, int value) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "{\"k\":\"warn\",\"name\":\"%s\",\"порог\":%d,\"значение\":%d,\"бой\":%lld,\"ход\":%d}", name, threshold, value, g_fightNo,
                  g_curRound);
    const long long n = ++g_warnCount;
    if (n <= 50 || std::getenv("DND_WARN_ALL")) std::fprintf(stderr, "WARN %s\n", buf);
    evPush(buf);
}

long long tri(int v) {
    return v <= 0 ? 0 : (long long)v * (v + 1) / 2;
}
// Характеристика стоит V·(V+1)/2 − 55: значение 10 бесплатно, выше десяти платим, ниже десяти очки возвращаются (минимум 1: −54).
// Внутренний учёт движка и калибровки остаётся «по-старому» (абсолют); чтобы бюджет в правилах и в интерфейсе был «от десяток», к нему прибавляется STAT_BASE.
const int STAT_ZERO = 10;
const long long STAT_BASE = 6 * 55;   // шесть характеристик по 10 в абсолютной цене
inline long long statCost(int v) { return (long long)v * (v + 1) / 2 - 55; }
// Технические пределы (v21, №8): характеристика 1…40, купленный КД до 40 (баффы, укрытие и Парирование могут дать больше)
const int STAT_MAX = 40, AC_MAX = 40;
// КД (v21, №3): 1 единица = +1 КД, КД = 10 + a. Цена за тот же КД прежняя: tri(2a) (раньше +1 КД стоил 2 единицы)
inline long long acCost(int a) { return tri(2 * a); }
int maxV(long long b) {
    if (b <= 0) return 0;
    int v = (int)std::floor((-1.0 + std::sqrt(1.0 + 8.0 * (double)b)) / 2.0);
    while (tri(v + 1) <= b) v++;
    while (v > 0 && tri(v) > b) v--;
    return v;
}
int modOf(int raw) {
    return (int)std::floor((raw - 10) / 2.0);
}

enum Stat { STR = 0, DEX, CON, INT, WIS, CHA, STAT_N };
enum Cond {
    C_NONE = 0,
    C_PRONE,
    C_RESTR,
    C_FRIGHT,
    C_POISON,
    C_BLIND,
    C_STUN,
    C_SLOW,
    C_DISTR,
    C_INCAP,
    C_PARAL,
    C_CHARM,
    C_GRAPPLE,
    C_WEAK,
    C_LIT,
    C_FLEE,
    C_N
};
const char* CN[C_N] = {"-",
                       "сбит с ног",
                       "опутан",
                       "испуган",
                       "отравлен",
                       "ослеплён",
                       "оглушён",
                       "замедлен",
                       "отвлечён (иллюзия)",
                       "недееспособен",
                       "парализован",
                       "очарован",
                       "схвачен",
                       "ослаблен (помеха на атаки)",
                       "подсвечен (атаки по нему с преимуществом)",
                       "в бегстве"};
const char* SN[STAT_N] = {"СИЛ", "ЛОВ", "ТЕЛ", "ИНТ", "МДР", "ХАР"};
// Навыки D&D (v21, владения): характеристика навыка; в бою работают четыре — Атлетика и Акробатика (вырваться из захвата),
// Скрытность и Внимательность (оторваться в погоне и заметить беглеца); остальные — для исследования и социалки
enum Skill { SK_ATHL, SK_ACRO, SK_SLEIGHT, SK_STEALTH, SK_ARCANA, SK_HISTORY, SK_INVEST, SK_NATURE, SK_RELIGION, SK_ANIMAL, SK_INSIGHT, SK_MEDICINE, SK_PERCEP, SK_SURVIVAL, SK_DECEPT, SK_INTIM, SK_PERFORM, SK_PERSUADE, SK_N };
const char* SKN[SK_N] = {"Атлетика", "Акробатика", "Ловкость рук", "Скрытность", "Магия", "История", "Анализ", "Природа", "Религия", "Уход за животными",
                         "Проницательность", "Медицина", "Внимательность", "Выживание", "Обман", "Запугивание", "Выступление", "Убеждение"};
const int SKST[SK_N] = {0, 1, 1, 1, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5};
inline bool skCombat(int k) { return k == SK_ATHL || k == SK_ACRO || k == SK_STEALTH || k == SK_PERCEP; }
