// Решения генератора (bern/catN/magN/warp, сеть-Сборщик) и сборка способностей, оружия, пассивов
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Решения генератора боссов. Каждое значимое решение имеет номер: вероятность, выбор из нескольких или «растяжка» доли
// бюджета. Без сети-Сборщика (g_bb == nullptr) решения случайны с исходными вероятностями; сеть видит, что уже куплено,
// и выдаёт поправку.
// ---------------------------------------------------------------------------
enum MagF {   // числа во весь технический диапазон (magN): у каждого свой номер решения на 24 корзины
    MG_HEALN,
    MG_TELE,
    MG_BUFFN,
    MG_BUFFR,
    MG_TEMPN,
    MG_ACB,
    MG_ACR,
    MG_DARKR,
    MG_INVISN,
    MG_SBR,
    MG_SBAMT,
    MG_ALLYR,
    MG_WALLRANGE,
    MG_WALLLEN,
    MG_WALLR,
    MG_RANGE,
    MG_RADIUS,
    MG_MULTI,
    MG_DMGN,
    MG_MIXN,
    MG_CONDR,
    MG_PUSH,
    MG_COLL,
    MG_DOTN,
    MG_DOTR,
    MG_SPEED,
    MG_AURAN,
    MG_AURAR,
    MG_AURARAD,
    MG_RUN,
    MG_RAURA,
    MG_SUMN,
    MG_FORMU,
    MG_RN,
    MG_RR,
    MG_WLONG,
    MG_POOLN,
    MG_SELFN,
    MG_TRIGN,
    MG_TRIGU,
    MG_WATT,
    MG_WRANGE,
    MG_WREACH,
    MG_USES,
    MG_REGEN,
    MG_LEGACT,
    MG_PARRY,
    MG_SURGE,
    MG_LEGRES,
    MG_IMAGES, MG_GLOBEL, MG_GLOBER, MG_BANISH, MG_FLYR, MG_XTURN, MG_GROWR, MG_LIGHTR, MG_ACMIN, MG_JUMP,   // пакеты атомов 2–3
    MG_N
};
enum BPar {
    BP_SUPP,
    BP_REGEN,
    BP_RESIST,
    BP_IMMTYPE,
    BP_VULN,
    BP_RESNM,
    BP_IMMNM,
    BP_WMAGIC,
    BP_CONDDEF,
    BP_LEGACT,
    BP_PARRY,
    BP_UNCANNY,
    BP_IMMPUSH,
    BP_DISENGB,
    BP_AURA,
    BP_DARKV,
    BP_BLINDS,
    BP_IGNTERR,
    BP_TRIGPH,
    BP_TRIGCOND,
    BP_RIPOSTE,
    BP_SURGE,
    BP_LEGRES,
    BP_IMMCOND,
    BP_SAVEPROF,
    BP_SAVEADV,
    BP_FLY,
    BP_SPEED,
    BP_PACK,
    BP_RECKLESS,
    BP_SUMMON,
    BP_REDIRECT,
    BP_ARSENAL,
    BP_RIDER,
    BP_ABSTOP,
    BP_SELF,
    BP_SELFBONUS,
    BP_SELFCONC,
    BP_RANGE5,
    BP_NORAD,
    BP_HASDMG,
    BP_HASCOND,
    BP_PUSH,
    BP_COLL,
    BP_TERR,
    BP_DOT,
    BP_BONUS,
    BP_CONC,
    BP_SLOT,
    BP_RECHARGE,
    BP_WMELEE,
    BP_WFINESSE,
    BP_WAMMO,
    BP_WREACH,
    BP_DMGMIX,
    BP_GROUND,
    BP_WALL,
    BP_REACT,
    BP_SELECT,
    BC_SELFKIND,
    BC_DMGN = BC_SELFKIND + 9,
    BC_DMGS = BC_DMGN + 10,
    BC_USES = BC_DMGS + DICE_N,
    BC_WDIE = BC_USES + 4,
    BC_WATT = BC_WDIE + DICE_N,
    BC_MAIN = BC_WATT + 12,
    BC_STAT = BC_MAIN + 5,
    BC_DELIV = BC_STAT + 6,
    BW_PASS = BC_DELIV + 3,
    BW_WPN,
    BW_STAT,
    BW_AC,
    BW_SUMALLOC,
    // этап 5 (2026-11-02): числа, которые генератор раньше выбирал простым случаем, — тоже решения Сборщика (какое
    // состояние и на сколько раундов, дальность, размеры эффектов, параметры призыва). Без Сборщика каждое такое решение
    // тратит случайные числа ровно как прежнее irand/rand01 — обычные сборки не меняются. Добавлены в конец, чтобы номера
    // прежних решений (и веса линейных Сборщиков) не сдвинулись
    BC_HEALN,
    BC_TELE = BC_HEALN + 6,
    BC_BUFFN = BC_TELE + 6,
    BC_BUFFR = BC_BUFFN + 3,
    BC_TEMPN = BC_BUFFR + 9,
    BC_ACB = BC_TEMPN + 6,
    BC_ACR = BC_ACB + 4,
    BC_DARKR = BC_ACR + 9,
    BC_INVISN = BC_DARKR + 3,
    BC_SBR = BC_INVISN + 5,
    BC_INVISC = BC_SBR + 9,
    BC_WALLRANGE = BC_INVISC + 8,
    BC_WALLLEN = BC_WALLRANGE + 12,
    BC_WALLR = BC_WALLLEN + 7,
    BC_SAVE = BC_WALLR + 4,
    BC_RANGEA = BC_SAVE + 5,
    BC_RANGEB = BC_RANGEA + 12,
    BC_RADIUS = BC_RANGEB + 12,
    BC_MIXN = BC_RADIUS + 6,
    BC_MIXS = BC_MIXN + 3,
    BC_COND = BC_MIXS + DICE_N,
    BC_CONDR = BC_COND + C_N - 1,
    BC_PUSH = BC_CONDR + 5,
    BC_COLL = BC_PUSH + 6,
    BC_DOTN = BC_COLL + 3,
    BC_DOTS = BC_DOTN + 3,
    BC_DOTR = BC_DOTS + DICE_N,
    BC_CONDRC = BC_DOTR + 4,
    BC_DOTRC = BC_CONDRC + 8,
    BC_REACTT = BC_DOTRC + 8,
    BC_SPEED = BC_REACTT + 4,
    BC_AURAN = BC_SPEED + 12,
    BC_AURAS = BC_AURAN + 4,
    BC_AURAC = BC_AURAS + DICE_N,
    BC_AURAR = BC_AURAC + C_N - 1,
    BC_PHN = BC_AURAR + 2,
    BC_CDN = BC_PHN + 3,
    BC_CDK = BC_CDN + 4,
    BC_RN = BC_CDK + 8,
    BC_RS = BC_RN + 3,
    BC_RCOND = BC_RS + DICE_N,
    BC_RR = BC_RCOND + C_N - 1,
    BC_SUMN = BC_RR + 3,
    BC_SUMK = BC_SUMN + 8,
    BC_WRANGEA = BC_SUMK + 8,
    BC_WRANGEB = BC_WRANGEA + 12,
    BP_SUMCONC = BC_WRANGEB + 12,
    BP_SUMBONUS,
    BP_SUMUNL,
    BP_DCB,    // этап 5б: своя надбавка к СЛ способности
    BC_DCB,    // +1..+8
    BP_NOREP = BC_DCB + 8,   // состояние без повторного спаса
    BP_PROF,   // владения: бонус мастерства выше +2
    BC_PROF,   // +3..+6
    BP_SKILL = BC_PROF + 4,   // владение боевым навыком
    BP_SKILLX,                // экспертиза в нём
    BP_NOWPROF,               // без владения своим оружием
    BP_RULE,                    // «когда → что»: ещё одно правило
    BC_RUWHAT,                  // эффект (E_N)
    BC_RUWHEN = BC_RUWHAT + 16, // условие (W_N); эффектов до 16
    BC_RUCOND = BC_RUWHEN + 12, // состояние противника для W_FOECOND (условий до 12)
    BC_RUN = BC_RUCOND + 15,    // кубиков урона или +КД (1–4)
    BC_RUS = BC_RUN + 4,        // грань кубика урона
    BP_RUONCE = BC_RUS + 16,    // урон раз в ход
    BP_WLIGHT,                  // оружие и Зуро: лёгкое ближнее оружие
    BP_WLONG,                   // дальняя дистанция у дальнего оружия
    BC_WLONG,                   // ×2..×4
    BP_TWF = BC_WLONG + 3,      // атака второй рукой
    BP_TWFMOD,                  // стиль: модификатор к урону второй рукой
    BP_POOL,                    // запас костей с приёмами
    BC_POOLN,                   // костей 1–6
    BC_POOLS = BC_POOLN + 6,    // грань
    BC_MANK = BC_POOLS + 16,    // приёмов 1–3
    BC_MANEFF = BC_MANK + 3,    // эффект приёма: урон, состояние, толчок, обезоруживание
    BP_MANSAVE = BC_MANEFF + 4, // приём со спасброском
    BP_SMITE,                   // удар «при попадании» без запаса (кара)
    BP_SELFDMG,                 // урон себе
    BP_KILLHEAL,                // лечение, если цель умерла
    BC_HEALS,                   // грань лечения
    BP_HEALB = BC_HEALS + 16,   // прибавка к лечению
    BC_BUFFS,                   // грань баффа оружия (4 варианта; место — на 8)
    BC_ARMOR = BC_BUFFS + 8,    // доспехи и щит: вид доспеха (4)
    BP_SHIELD = BC_ARMOR + 4,   // щит
    BP_ARMSTEALTH,              // средний доспех с помехой к Скрытности
    BP_NOARMPROF,               // без владения доспехом
    BP_TWOH,                    // двуручное оружие
    BP_VERS,                    // универсальное оружие
    BC_SHAPE,                   // формы: сфера, куб, конус, линия
    BP_MULTI = BC_SHAPE + 4,    // несколько целей
    BC_MULTI,                   // целей 2–4
    BP_ALLY = BC_MULTI + 3,     // союзники: способность «на себя» — и на союзника
    BC_ALLYR,                   // дальность до союзника (12 ступеней)
    BP_CMDATK = BC_ALLYR + 12,  // приказ союзнику атаковать
    BP_PROTECT,                 // реакция за союзника: помеха
    BP_STRIKE,                  // реакция за союзника: удар
    BP_RAURA,                   // правило-аура для своих
    BC_RAURA,                   // радиус ауры (6 ступеней)
    BP_TRIG = BC_RAURA + 6,     // «событие → эффект»: ещё одно
    BC_TRIGON,                  // событие (до 12)
    BC_TRIGDO = BC_TRIGON + 12, // эффект (до 12)
    BC_TRIGN = BC_TRIGDO + 12,  // кубиков или величина (1–4)
    BC_TRIGS = BC_TRIGN + 4,    // грань
    BP_TRIGLIM = BC_TRIGS + 16, // ограничение применений
    BC_TRIGU,                   // применений 1–3
    BP_SIZE = BC_TRIGU + 3,     // размеры: не средний
    BC_SIZE,                    // размер (6)
    BP_WHEAVY = BC_SIZE + 6,    // тяжёлое оружие
    BP_PULL,                    // прочее: притягивание
    BP_MAXHP,                   // урон снижает максимум хитов
    BP_DRAIN,                   // вампиризм
    BP_DISPEL,                  // рассеивание
    BP_COUNTER,                 // контрзаклинание (реакция 4)
    BP_TRUESIGHT,               // истинное зрение
    BC_TRUESIGHT,               // 10/30/60/120
    BP_CLIMB = BC_TRUESIGHT + 4,   // движение: лазание
    BP_SWIM,                       // плавание
    BP_BURROW,                     // копание
    BP_EVASION,                    // Увёртливость
    BP_SUNSENS,                    // солнечная чувствительность
    BP_LAIR,                       // действия логова
    BC_LAIR,                       // уровень 1–3
    BP_SUMMIX = BC_LAIR + 3,       // разные приспешники в одном призыве
    BP_FORM,                       // превращение
    BP_FORMBONUS,                  // превращение бонусным действием
    BC_FORMU,                      // применений 1–3
    BP_SELFMORE = BC_FORMU + 3,    // после теста 4 ноября: ещё один эффект в той же способности «на себя» (как у игрока)
    BP_MAGICRES,                   // пакет атомов 1 (5 ноября): сопротивление магии
    BP_SPELLRES,                   // сопротивление урону от способностей
    BP_CRITIMM,                    // иммунитет к критам
    BP_INATTACK,                   // способность вместо одной атаки
    BP_IMMSAVE,                    // после успешного спасброска цель невосприимчива
    BP_NOREPEAT,                   // не два раунда подряд
    BP_PRECAST,                    // действует с начала боя
    BP_TRIGDMGT,                   // событие «я получил урон выбранного типа»
    BP_MULTIWPN,                   // запасное оружие бьёт в том же действии «Атака» (мультиатака из разных оружий)
    BP_JUMP, BP_IMAGES, BP_GLOBE, BP_BANISH, BP_FLYBUFF, BP_TIMESTOP, BP_GROW, BP_LIGHTS, BP_HEALALLY,   // пакет атомов 2
    BP_GRABRESTR, BP_GRABDOT, BP_WSILVER, BP_LIGHTP, BP_TRIGCHARGE, BP_RUNEW, BC_RUNEW,
    BP_UNARM = BC_RUNEW + 3, BP_UNARMSTAT, BP_RECHAFTER,                                                            // пакет атомов 3
    BC_MAG,                        // числа во весь технический диапазон — по 24 корзины на MagF
    BP_N = BC_MAG + 24 * MG_N
};
#define MAGID(f) (BC_MAG + 24 * (f))
// технические пределы игрока (те же, что MAX_DICE/MAX_COUNT/MAX_FT/MAX_ROUNDS в char_spec.h): генератор Сборщика — в них же
constexpr int MAX_DICE_G = 1000, MAX_COUNT_G = 1000, MAX_FT_G = 500, MAX_ROUNDS_G = 500;
const char* BPARN[BP_N] = {"подавление",
                           "регенерация",
                           "резист к типам",
                           "иммунитет к типам",
                           "уязвимость (возврат очков)",
                           "резист к немагическому",
                           "иммунитет к немагическому",
                           "магическое оружие",
                           "условная защита",
                           "легендарные действия",
                           "парирование",
                           "невероятное уклонение",
                           "иммунитет к отталкиванию",
                           "Отход бонусным",
                           "аура",
                           "тёмное зрение",
                           "слепое зрение",
                           "игнор местности",
                           "ярость при ХП<=50%",
                           "добивание",
                           "контратака",
                           "всплеск действий",
                           "легендарное сопротивление",
                           "иммунитет к состояниям",
                           "владение спасбросками",
                           "преимущество на спасы",
                           "полёт",
                           "скорость",
                           "стая тактики",
                           "безрассудство",
                           "призыв",
                           "Перенаправление",
                           "запасное оружие",
                           "довесок к удару",
                           "ещё способность (стоп)",
                           "способность на себя",
                           "на себя: бонусное",
                           "на себя: концентрация",
                           "дальность 5 фт",
                           "без области",
                           "есть урон",
                           "есть состояние",
                           "толчок",
                           "столкновение",
                           "труднопроходимая местность",
                           "урон по времени",
                           "бонусное действие",
                           "концентрация",
                           "ячейка",
                           "перезарядка",
                           "оружие ближнее",
                           "оружие фехтовальное",
                           "оружие боеприпасное",
                           "оружие с досягаемостью",
                           "доп. тип урона способности",
                           "способность по клетке",
                           "барьер",
                           "способность-реакция",
                           "избирательная область"};
const char* BCATN[9] = {"вид способности на себя", "кубики урона способности", "размер кубика способности",
                        "число применений",        "размер кубика оружия",     "число атак оружия",
                        "главная характеристика",  "вес характеристики",       "доставка способности"};
// Сеть-Сборщик (net_builder.cc) подменяет статичные параметры: на каждом решении генератора она видит, что уже куплено, и выдаёт поправку.
// Генератор по-прежнему сам следит за бюджетом, поэтому все инварианты цены сохраняются при любых решениях сети.
struct BuildBrain {
    virtual bool bern(int id, double p0, double u) = 0;                              // u — равномерное число из общего потока
    virtual int cat(int base, int n, const double* p0, double u) = 0;                // p0[j] — исходные вероятности (сумма 1)
    virtual double warp(int id, double u) = 0;                                       // доля бюджета u в [0,1) → растянутая
    virtual ~BuildBrain() {}
};
thread_local BuildBrain* g_bb = nullptr;
struct BuildState {   // что видит сеть-Сборщик о строящемся боссе
    double budget = 0, spent = 0, passLeft = 0;
    int nAbil = 0, depth = 0;
};
thread_local BuildState g_bstate;
thread_local int g_buildDepth = 0;
// решение «да/нет» с вероятностью p0
inline bool bern(int id, double p0) {
    if (g_bb) return g_bb->bern(id, p0, rand01());
    return rand01() < p0;
}
// равновероятный выбор из n
inline int catN(int base, int n) {
    if (g_bb) {
        double p[24];   // до 24 вариантов (состояния — 15)
        for (int j = 0; j < n; j++) p[j] = 1.0 / n;
        return g_bb->cat(base, n, p, rand01());
    }
    return irand(n);
}
// выбор по накопленным вероятностям cum0 (последняя = 1)
inline int catP(int base, int n, const double* cum0) {
    double r = rand01();
    if (g_bb) {
        double p[16];
        for (int j = 0; j < n; j++) p[j] = cum0[j] - (j ? cum0[j - 1] : 0.0);
        return g_bb->cat(base, n, p, r);
    }
    for (int j = 0; j < n - 1; j++)
        if (r < cum0[j]) return j;
    return n - 1;
}
// Число во весь технический диапазон игрока (после теста 4 ноября: у Сборщика нет игровых потолков — только технические
// пределы, остальное ограничивает цена). Корзины по логарифмической шкале от lo до hi (до 24), внутри корзины —
// равномерно. Без Сборщика корзина k выпадает с весом 0.7^k (чаще малые числа), Сборщик сдвигает веса.
int magN(int f, int lo, int hi, int step = 1) {
    if (hi <= lo) return lo;
    const int id = MAGID(f), V = (hi - lo) / step + 1, K = std::min(24, V);
    int edge[25];   // корзина k — номера значений [edge[k], edge[k+1])
    for (int k = 0; k < K; k++) edge[k] = (int)std::lround(std::pow((double)V, (double)k / K)) - 1;
    edge[K] = V;
    for (int k = 1; k < K; k++) edge[k] = std::min(std::max(edge[k], edge[k - 1] + 1), V - (K - k));
    double p0[24], tot0 = 0;
    for (int k = 0; k < K; k++) tot0 += (p0[k] = std::pow(0.7, k));
    for (int k = 0; k < K; k++) p0[k] /= tot0;
    const double r = rand01();
    int c = K - 1;
    if (g_bb)
        c = g_bb->cat(id, K, p0, r);
    else {
        double tot = 0;
        for (int k = 0; k < K; k++) tot += p0[k];
        double q = r * tot;
        for (int k = 0; k < K; k++) {
            q -= p0[k];
            if (q <= 0) { c = k; break; }
        }
    }
    return lo + step * (edge[c] + irand(edge[c + 1] - edge[c]));
}
// доля бюджета u∈[0,1): без Сборщика — как есть, Сборщик растягивает
inline double warp(int id, double u) {
    if (g_bb) return g_bb->warp(id, u);
    return u;
}
const double DELIV_CUM[3] = {0.40, 0.85, 1.0};
const double USES_CUM[4] = {0.15, 0.25, 0.35, 1.0};
struct BuildScope {
    BuildState saved;
    BuildScope() : saved(g_bstate) {
        g_buildDepth++;
        g_bstate = BuildState();
        g_bstate.depth = g_buildDepth;
    }
    ~BuildScope() {
        g_buildDepth--;
        g_bstate = saved;   // вложенная сборка (шаблон миньона) не должна портить состояние внешней
    }
};

Wp buildWeapon(const Coef& C, double wBudget, bool forceMelee) {
    Wp p;
    int di = catN(BC_WDIE, DICE_N);
    p.s = DICE[di];
    p.finesse = bern(BP_WFINESSE, 0.5);
    p.range = (forceMelee || bern(BP_WMELEE, 0.45)) ? 5 : magN(MG_WRANGE, 10, MAX_FT_G, 5);
    // боеприпасное — всегда ЛОВ, фехтовальным бывает лишь метательное
    if (p.range > 5) {
        p.ammo = bern(BP_WAMMO, 0.7);
        if (p.ammo) p.finesse = false;
    }
    if (p.range <= 5 && bern(BP_WREACH, 0.16)) {
        p.reach = magN(MG_WREACH, 10, MAX_FT_G, 5);
    }
    {
        double r = rand01();
        p.type = r < 0.385 ? DT_PIERCING : (r < 0.731 ? DT_SLASHING : DT_BLUDGEONING);
    }
    int k = magN(MG_WATT, 1, MAX_COUNT_G), n = 0;
    // замер: число атак задано
    if (g_probe.tier == TR_ATT && !g_probe.attUsed) {
        k = (int)g_probe.n + 1;
        g_probe.attUsed = true;
    }
    // не хватает даже на один кубик выбранного размера — берём кубик меньше (иначе к20/к100 на малом бюджете оставляли
    // босса безоружным, а безоружный в v21 оружием не бьёт)
    for (const int k0 = k; di >= 0; di--) {
        p.s = DICE[di];
        k = k0;
        while (k >= 1) {
            double per1 = C[AT_WPNDICE] * (p.s + 1) / 2.0 * hitsF(C, k) * wpMult(C, p.range, p.reach, k);
            n = (int)std::floor((wBudget - (p.finesse ? 3 : 0)) / per1);
            if (n >= 1) break;
            k = k > 1 ? std::max(1, k / 2) : 0;
        }
        if (k >= 1 && n >= 1) break;
    }
    if (k >= 1 && n >= 1) {
        p.n = n;
        p.att = k;
        p.cost = wpPrice(p, C);
    } else
        p = Wp{};
    return p;
}
// фактическая дальность атаки оружием (досягаемость или дальность)
inline int atkDist(const Boss& b) {
    return b.wRange > 5 ? std::max(b.wRange, b.wLong) : b.reach;   // дальняя дистанция — с помехой (advState)
}
// дальность способности: приём «при попадании» и атака второй рукой бьют своим оружием (вторая рука — только вблизи)
inline int specReach(const Boss& b, const Spec& s) {
    if (s.offhand) return b.reach;
    if (s.onHit) return atkDist(b);
    return s.range;
}
inline bool hasOffhand(const Boss& b) {
    for (auto& s : b.abil)
        if (s.offhand) return true;
    return false;
}
// доспехи (v21, как в D&D): без доспеха и лёгкий — вся ЛОВ к КД, средний — не больше +2, тяжёлый — ЛОВ не идёт
enum Armor { AR_NONE, AR_LIGHT, AR_MEDIUM, AR_HEAVY, AR_N };
const char* ARN[AR_N] = {"без доспеха", "лёгкий доспех", "средний доспех", "тяжёлый доспех"};
inline int dexAC(const Boss& b) {
    const int m = modOf(b.stats[DEX]);
    return b.armor == AR_HEAVY ? 0 : (b.armor == AR_MEDIUM ? std::min(m, 2) : m);
}
// КД = 10 + купленные единицы + ЛОВ по доспеху + щит
inline int unarmoredBonus(const Boss& b) {   // защита без доспеха (пакет атомов 3): модификатор выбранной характеристики, если плюс
    return b.armor == AR_NONE && b.unarmoredStat >= 0 && b.unarmoredStat < STAT_N ? std::max(0, modOf(b.stats[b.unarmoredStat])) : 0;
}
inline int calcAC(const Boss& b) {   // без доспеха (пакет атомов 3) — и модификатор выбранной характеристики (если плюс)
    const int ua = b.armor == AR_NONE && b.unarmoredStat >= 0 && b.unarmoredStat < STAT_N ? std::max(0, modOf(b.stats[b.unarmoredStat])) : 0;
    return 10 + b.acV + dexAC(b) + (b.shield ? 2 : 0) + ua;
}
// контекст цены по готовому телу (см. PriceCtx); PcGuard ставит его на время расчёта и возвращает прежний
inline void setPriceCtx(const Boss& b) {
    g_pc.on = true;
    g_pc.acV = b.acV;
    g_pc.ac = calcAC(b);
    g_pc.att = b.wN > 0 ? std::max(1, b.wAtt) : 1;
    g_pc.strMod = modOf(b.stats[STR]);
    g_pc.dexMod = modOf(b.stats[DEX]);
    g_pc.conSave = modOf(b.stats[CON]) + (((b.saveProf >> CON) & 1u) ? b.prof : 0);
}
struct PcGuard {
    PriceCtx prev;
    explicit PcGuard(const Boss& b) : prev(g_pc) { setPriceCtx(b); }
    ~PcGuard() { g_pc = prev; }
};
// грань оружия в бою: универсальное двумя руками (без щита и второй руки) — бóльшая
inline int wDie(const Boss& b) { return (b.wVers > b.wS && !b.shield && !hasOffhand(b)) ? b.wVers : b.wS; }
// цена доспеха и щита: срезанная доспехом ЛОВ к КД возвращает очки (тяжёлый при отрицательной ЛОВ — добавляет КД и стоит),
// щит, помеха к Скрытности (возврат), без владения (возврат); двуручное (возврат) и универсальное оружие
long long armorPrice(const Boss& b, const Coef& C) {
    double p = -C[AT_ARMDEX] * (modOf(b.stats[DEX]) - dexAC(b));
    if (b.shield) p += C[AT_SHIELD];
    if (b.armStealth && b.armor) p -= C[AT_STEALTHDIS];
    if (b.noArmProf && (b.armor || b.shield)) p -= C[AT_NOARMPROF] * scl(AT_NOARMPROF) * b.prof / 2.0;
    // защита без доспеха (пакет атомов 3): такой же КД по лестнице КД, с долей C[AT_UNARMORED]
    if (const int ua = unarmoredBonus(b)) p += C[AT_UNARMORED] * (acCost(b.acV + ua) - acCost(b.acV));
    return p >= 0 ? (long long)std::ceil(p) : -(long long)std::ceil(-p);
}
const char* SZN[6] = {"крошечный", "маленький", "средний", "большой", "огромный", "громадный"};
// цена размера (v21): крупнее среднего — за ступень (захватить и толкнуть могут лишь на ступень меньше); мельче с тяжёлым
// оружием — помеха к его атакам, очки возвращаются
long long sizePrice(const Boss& b, const Coef& C) {
    double p = 0;
    if (b.size > 2) p = C[AT_SIZEUP] * scl(AT_SIZEUP) * (b.size - 2);
    if (b.size < 2 && b.wN > 0 && b.wHeavy) p = -C[AT_SIZEDN] * scl(AT_SIZEDN) * (2 - b.size);
    return p >= 0 ? (long long)std::ceil(p) : -(long long)std::ceil(-p);
}
long long weaponHandsPrice(const Boss& b, const Coef& C) {
    if (b.wN <= 0) return 0;
    double p = 0;
    if (b.wTwoH) p -= C[AT_TWOHAND];
    if (b.wVers > b.wS) p += C[AT_WPNDICE] * b.wN * (b.wVers - b.wS) / 2.0 * hitsF(C, b.wAtt) * wpMult(C, b.wRange, b.reach, b.wAtt) * C[AT_VERS];
    return p >= 0 ? (long long)std::ceil(p) : -(long long)std::ceil(-p);
}
inline void markArmor(const Boss& b, AMask& m) {
    if (b.size > 2) m |= ONE << AT_SIZEUP;
    if (b.size < 2 && b.wN > 0 && b.wHeavy) m |= ONE << AT_SIZEDN;
    if (b.armor == AR_MEDIUM || b.armor == AR_HEAVY) m |= ONE << AT_ARMDEX;
    if (b.shield) m |= ONE << AT_SHIELD;
    if (b.armStealth && b.armor) m |= ONE << AT_STEALTHDIS;
    if (b.noArmProf && (b.armor || b.shield)) m |= ONE << AT_NOARMPROF;
    if (b.wN > 0 && b.wTwoH) m |= ONE << AT_TWOHAND;
    if (b.wN > 0 && b.wVers > b.wS) m |= ONE << AT_VERS;
}
// v21: оружие бьёт вблизи, если оно ближнее или метательное (кинжал, дротик); боеприпасное (лук, арбалет) — только
// издали. Безоружный атаки оружием не делает и провоцированных атак не совершает.
inline bool meleeWeapon(const Boss& b) {
    return b.wN > 0 && (b.wRange <= 5 || !b.wAmmo);
}
// досягаемость ближнего удара: у ближнего оружия своя, у метательного и голых рук 5 фт
inline int meleeReach(const Boss& b) {
    return b.wN > 0 && b.wRange <= 5 ? b.reach : 5;
}
// дальность конкретной атаки оружием: метательным по цели вплотную бьют как ближним (без помехи за врага рядом)
inline int wpAtkRange(const Boss& b, int dist) {
    return (b.wRange > 5 && !b.wAmmo && dist <= 5) ? 5 : b.wRange;
}
// Парирование и контратака: оружием ближнего боя или голыми руками; с луком в руках нельзя
inline bool handsForMelee(const Boss& b) {
    return b.wN <= 0 || meleeWeapon(b);
}

// Характеристика спасброска от состояния — по смыслу эффекта (телесное — ТЕЛ, воля — МДР, разум — ИНТ, захват/сбивание
// — СИЛ)
int condSaveStat(int k) {
    switch (k) {
        case C_PRONE:
        case C_RESTR:
        case C_GRAPPLE:
            return STR;
        case C_POISON:
        case C_BLIND:
        case C_STUN:
        case C_PARAL:
            return CON;
        case C_DISTR:
            return INT;
        case C_LIT:
            return DEX;
        // испуган, замедлен, недееспособен, очарован
        default:
            return WIS;
    }
}
// Скидка на подавляемую пассивку (tgt: 1 регенерация, 2 аура, 4 защиты): чем чаще встречается подавляющий урон, тем
// больше скидка
double suppMul(const Boss& b, const Coef& C, int tgt) {
    if (!b.suppMask || !(b.suppTgt & tgt)) return 1.0;
    double q = 0;
    for (int t = 0; t < DT_N; t++)
        if ((b.suppMask >> t) & 1) q += g_share[t];
    return 1.0 - C[AT_SUPP] * std::min(1.0, 5.0 * q);
}
// цена правила «когда → что»: цена эффекта × доля времени условия; «атаки по мне с преимуществом» возвращает очки
long long rulePrice(const Rule& r, const Boss& b, const Coef& C) {
    auto P = [&](int a, double m = 1.0) { return C[a] * m * scl(a); };
    double e = 0;
    switch (r.what) {
        case E_ADVATK: e = P(AT_E_ADVATK); break;
        case E_DMG: e = C[AT_E_DMG] * r.n * (r.s + 1) / 2.0 / 3.5 * (r.once ? C[AT_E_ONCE] : hitsF(C, b.wAtt)); break;   // на каждый удар
        case E_AC: e = C[AT_E_AC] * r.n; break;
        case E_DISVSME: e = P(AT_E_DISVSME); break;
        case E_ADVVSME: e = -P(AT_E_ADVVSME); break;
        case E_RES: e = P(AT_E_RES, 5.0) * suppMul(b, C, 4); break;
        case E_RESNM: e = P(AT_E_RES, 5.0 * g_nmShare) * suppMul(b, C, 4); break;
        case E_ADVSAVE: e = P(AT_E_ADVSAVE); break;
        case E_SAVEB: e = P(AT_E_SAVEB) * r.n; break;
        case E_ATKDIE: e = P(AT_E_ATKDIE) * r.n * (r.s + 1) / 5.0; break;   // за к4-эквивалент
        case E_SAVEDIE: e = P(AT_E_SAVEDIE) * r.n * (r.s + 1) / 5.0; break;
        case E_DMGRED: e = P(AT_E_DMGRED) * r.n; break;
        case E_PIERCE: e = P(AT_E_PIERCE, 5.0 * g_share[std::clamp(r.n, 0, DT_N - 1)]); break;
        case E_NOCOVER: e = P(AT_E_NOCOVER); break;
        case E_POWER: e = P(AT_E_POWER) * r.n * hitsF(C, b.wAtt); break;   // размен на каждый удар
        case E_NOOA: e = P(AT_E_NOOA); break;
        case E_SKILLADV: e = P(AT_E_SKILLADV) * (skCombat(std::clamp(r.n, 0, SK_N - 1)) ? 1.0 : 0.1); break;   // небоевой навык — почти даром
        case E_DISATK: e = -P(AT_E_DISATK); break;          // недостаток: очки возвращаются
        case E_ACMINUS: e = -P(AT_E_ACMINUS) * r.n; break;
    }
    e *= C[AT_W_ALWAYS + r.when];
    if (r.allyR > 0) e *= 1 + C[AT_RULEAURA] * tierN(TR_AURARAD, r.allyR / 5.0);   // аура: и на союзников рядом
    return e >= 0 ? (long long)std::ceil(e) : -(long long)std::ceil(-e);
}
// атомы правила: условие и эффект (и «раз в ход»)
inline void markRule(const Rule& r, AMask& m) {
    static const int EA[E_N] = {AT_E_ADVATK, AT_E_DMG, AT_E_AC, AT_E_DISVSME, AT_E_ADVVSME, AT_E_RES, AT_E_RES, AT_E_ADVSAVE, AT_E_SAVEB, AT_E_ATKDIE, AT_E_SAVEDIE, AT_E_DMGRED, AT_E_PIERCE, AT_E_NOCOVER, AT_E_POWER, AT_E_NOOA,
                                AT_E_SKILLADV, AT_E_DISATK, AT_E_ACMINUS};
    if (r.allyR > 0) m |= ONE << AT_RULEAURA;
    m |= ONE << (AT_W_ALWAYS + r.when);
    m |= ONE << EA[r.what];
    if (r.what == E_DMG && r.once) m |= ONE << AT_E_ONCE;
}
std::string describeRule(const Rule& r) {
    std::string w = r.when == W_FOECOND ? std::string("у противника «") + CN[r.warg] + "»" : WN[r.when];
    std::string e = EN[r.what];
    if (r.what == E_DMG) e = "+" + std::to_string(r.n) + "к" + std::to_string(r.s) + " к урону оружия" + (r.once ? ", раз в ход" : "");
    if (r.what == E_AC) e = "+" + std::to_string(r.n) + " КД";
    if (r.what == E_SAVEB) e = "+" + std::to_string(r.n) + " к спасброскам";
    if (r.what == E_ATKDIE) e = "+" + std::to_string(r.n) + "к" + std::to_string(r.s) + " к броскам атаки";
    if (r.what == E_SAVEDIE) e = "+" + std::to_string(r.n) + "к" + std::to_string(r.s) + " к спасброскам";
    if (r.what == E_DMGRED) e = "немагический физический урон по мне −" + std::to_string(r.n);
    if (r.what == E_PIERCE) e = std::string("мой урон «") + DTN[std::clamp(r.n, 0, DT_N - 1)] + "» игнорирует сопротивление";
    if (r.what == E_POWER) e = "размен: −" + std::to_string(r.n) + " к попаданию, +" + std::to_string(2 * r.n) + " к урону оружия";
    if (r.what == E_SKILLADV) e = std::string("преимущество на проверки «") + SKN[std::clamp(r.n, 0, SK_N - 1)] + "»";
    if (r.what == E_ACMINUS) e = "−" + std::to_string(r.n) + " к КД (недостаток)";
    if (r.allyR > 0) e += " (и союзникам в " + std::to_string(r.allyR) + " фт)";
    return w + " → " + e;
}
// цена «событие → эффект»: цена эффекта × доля пользы события; применений за бой меньше — дешевле (как у способностей)
long long trigPrice(const Trig& t, const Boss& b, const Coef& C) {
    auto P = [&](int a, double m = 1.0) { return C[a] * m * scl(a); };
    const double dice = (t.n * (t.s + 1) / 2.0 + t.b) / 3.5;
    double e = 0;
    switch (t.what) {
        case X_BONUSATK: e = C[AT_X_BONUSATK] * C[AT_WPNDICE] * b.wN * (b.wS + 1) / 2.0; break;
        case X_HEAL: e = C[AT_X_HEAL] * dice; break;
        case X_TEMP: e = C[AT_X_TEMP] * dice; break;
        case X_DMG: e = C[AT_X_DMG] * dice; break;
        case X_COND: e = t.cond > 0 ? C[AT_PRONE + t.cond - 1] * scl(AT_PRONE + t.cond - 1) * C[AT_X_COND] * (1 + 0.25 * (std::max(1, t.n) - 1)) : 0; break;
        case X_STAY1: e = P(AT_X_STAY1); break;
        case X_ADVNEXT: e = P(AT_X_ADVNEXT); break;
        case X_INITB: e = C[AT_X_INITB] * t.n; break;
        case X_SPEED: e = C[AT_X_SPEED] * t.n; break;
        case X_REROLL: e = P(AT_X_REROLL); break;
    }
    if (t.what == X_SELFCOND)   // состояние на себя — помеха владельцу: очки возвращаются
        e = t.cond > 0 ? -C[AT_PRONE + t.cond - 1] * scl(AT_PRONE + t.cond - 1) * C[AT_X_SELFCOND] * (1 + 0.25 * (std::max(1, t.n) - 1)) : 0;
    e *= C[atTrigOn(t.on)] * (t.on == T_DMGTYPE ? 5.0 * g_share[std::clamp(t.arg, 0, DT_N - 1)] : 1.0);
    if (t.uses < UNL) e *= std::max(0.2, 1 - (1 - C[AT_ULIM]) * (1 - (t.uses - 1) * 0.2));
    return e < 0 ? -(long long)std::ceil(-e) : std::max(1LL, (long long)std::ceil(e));   // состояние на себя — возврат
}
inline void markTrig(const Trig& t, AMask& m) {
    m |= ONE << atTrigOn(t.on);
    m |= ONE << atTrigDo(t.what);
}
std::string describeTrig(const Trig& t) {
    std::string e = TDN[t.what];
    const std::string dice = std::to_string(t.n) + "к" + std::to_string(t.s) + (t.b ? "+" + std::to_string(t.b) : "");
    if (t.what == X_HEAL || t.what == X_TEMP || t.what == X_DMG) e += " " + dice;
    if (t.what == X_COND && t.cond > 0) e = std::string("цель ") + CN[t.cond] + " на " + std::to_string(std::max(1, t.n)) + " р.";
    if (t.what == X_INITB) e = "+" + std::to_string(t.n) + " к инициативе";
    if (t.what == X_SPEED) e = "+" + std::to_string(5 * t.n) + " фт скорости в этот ход";
    if (t.what == X_SELFCOND && t.cond > 0) e = std::string("сам ") + CN[t.cond] + " на " + std::to_string(std::max(1, t.n)) + " р.";
    if (t.on == T_HITMELEE && (t.what == X_DMG || t.what == X_COND)) e = "атакующему: " + e;
    if (t.on == T_DMGTYPE) return std::string("я получил урон «") + DTN[std::clamp(t.arg, 0, DT_N - 1)] + "» → " + e + (t.uses < UNL ? " (" + std::to_string(t.uses) + " раз за бой)" : "");
    return std::string(TON[t.on]) + " → " + e + (t.uses < UNL ? " (" + std::to_string(t.uses) + " раз за бой)" : "");
}
inline bool hasResRule(const Boss& b) {
    for (auto& r : b.rules)
        if (r.what == E_RES || r.what == E_RESNM) return true;
    return false;
}
inline bool hasWhen(const Boss& b, int w) {
    for (auto& r : b.rules)
        if (r.when == w) return true;
    return false;
}
// Аура: урон/состояние врагам в радиусе в начале их хода (всегда включена); цена ∝ радиусу
long long auraPrice(const Boss& b, const Coef& C) {
    double t = 0;
    if (b.auraN > 0) t += C[AT_AURA] * tierN(TR_AURADICE, b.auraN * (b.auraS + 1) / 7.0) * 3.5;   // лестница в к6-эквивалентах
    if (b.auraCond > 0) {
        double dm = b.auraCond == C_PRONE ? 1.0 : 1 + 0.25 * (b.auraRounds - 1);
        t += C[AT_PRONE + b.auraCond - 1] * dm * scl(AT_PRONE + b.auraCond - 1) * C[AT_AURACOND];
    }
    t *= 1 + C[AT_AURARAD] * tierN(TR_AURARAD, b.auraRad / 5.0 - 1);
    t *= suppMul(b, C, 2);
    return (long long)std::ceil(t);
}
void markAura(const Boss& b, AMask& m) {
    if (b.auraN > 0) m |= ONE << AT_AURA;
    m |= ONE << AT_AURARAD;
    if (b.auraCond > 0) {
        m |= ONE << AT_AURACOND;
        m |= ONE << (AT_PRONE + b.auraCond - 1);
    }
}
// Цена довеска: доп. урон (за очко среднего) + состояние (цена состояния × множитель довеска); одно округление
// довесок «схватить» (пакет атомов 2): схваченный опутан; урон каждый ход, пока держит
double riderGrabPrice(const Boss& b, const Coef& C) {
    if (b.rCond != C_GRAPPLE) return 0;
    double t = 0;
    if (b.rGrabRestr) t += C[AT_PRONE + C_RESTR - 1] * scl(AT_PRONE + C_RESTR - 1) * C[AT_GRABRESTR];
    if (b.rGrabDotN > 0) t += C[AT_DOT] * b.rGrabDotN * (b.rGrabDotS + 1) / 2.0 / 3.5 * C[AT_GRABDOT];
    return t;
}
long long riderPrice(const Boss& b, const Coef& C) {
    double t = riderGrabPrice(b, C);
    if (b.rN > 0) t += C[AT_RDMG] * b.rN * (b.rS + 1) / 2.0;
    if (b.rCond > 0) {
        double dm = b.rCond == C_PRONE ? 1.0 : 1 + 0.25 * (b.rRounds - 1);
        t += C[AT_PRONE + b.rCond - 1] * dm * scl(AT_PRONE + b.rCond - 1) * C[AT_RCOND];
    }
    return (long long)std::ceil(t);
}
void markRider(const Boss& b, AMask& m) {
    if (b.rN > 0) m |= ONE << AT_RDMG;
    if (b.rCond == C_GRAPPLE && b.rGrabRestr) m |= ONE << AT_GRABRESTR;
    if (b.rCond == C_GRAPPLE && b.rGrabDotN > 0) m |= ONE << AT_GRABDOT;
    if (b.rCond > 0) {
        m |= ONE << AT_RCOND;
        m |= ONE << (AT_PRONE + b.rCond - 1);
    }
}

// есть ли у босса призыв (шаблоны в tmpl бывают и обликом превращения)
inline bool hasSummonSpec(const Boss& b) {
    for (auto& s : b.abil)
        if (s.sumN > 0) return true;
    return false;
}
void markSpec(const Spec& s, AMask& m) {
    auto set = [&](int a) { m |= ONE << a; };
    if (s.dmgN) set(AT_DMG);
    if (s.cond) set(AT_PRONE + s.cond - 1);
    if (s.pushFt) set(AT_PUSH);
    if (s.pushFt && s.collN) set(AT_COLLIDE);
    if (s.dotN) set(AT_DOT);
    if (s.darkR) set(AT_DARK);
    if (s.invisN) set(AT_INVIS);
    if (s.terr) set(AT_TERR);
    if (s.healN) set(AT_HEAL);
    if (s.teleFt) set(AT_TELE);
    if (s.hide) set(AT_HIDE);
    if (s.buffN) set(AT_BUFF);
    if (s.sbAmt) set(AT_STATBUF);
    if (s.tempN) set(AT_TEMP);
    if (s.acB) set(AT_ACB);
    if (!s.self) {
        if (s.radius) set(AT_AREA);
        if (s.radius && s.shape == SH_CONE) set(AT_SHCONE);
        if (s.radius && s.shape == SH_LINE) set(AT_SHLINE);
        if (s.targets > 1 && !s.radius) set(AT_MULTI);
        if (rangeIdx(s.range) >= 1) set(AT_ARANGE);
        if (s.delivery == 2) set(AT_AUTO);
    }
    if (s.uses < UNL) set(AT_ULIM);
    if (s.recharge) set(AT_RECHARGE);
    if (s.bonus) set(AT_BONUS);
    if (s.conc) set(AT_CONC);
    if (s.slotLvl > 0) set(AT_SLOTSPEC);
    if (s.wallLen > 0) set(AT_WALL);
    if (s.ground && !s.self) set(AT_GROUND);
    if (s.selective && s.radius > 0 && !s.self) set(AT_SELECT);
    if (s.react) set(AT_REACT);
    if (s.onHit) set(AT_ONHIT);
    if (s.offhand) set(AT_TWF);
    if (s.offhand && s.twfMod) set(AT_TWFMOD);
    if (s.pool) set(AT_POOL);
    if (s.selfN) set(AT_SELFDMG);
    if (s.killN || s.killB) set(AT_KILLHEAL);
    if (s.disarm) set(AT_DISARM);
    if (s.pullFt) set(AT_PULL);
    if (s.maxHpDmg) set(AT_MAXHP);
    if (s.drain) set(AT_DRAIN);
    if (s.counter) set(AT_COUNTER);
    if (s.dispel) set(AT_DISPEL);
    if (s.formTmpl >= 0) set(AT_FORM);
    if (s.ally) set(AT_ALLYTGT);
    if (s.inAttack) set(AT_INATTACK);
    if (s.cond == C_GRAPPLE && s.grabRestr) set(AT_GRABRESTR);
    if (s.jump && s.teleFt > 0) set(AT_JUMP);
    if (s.images > 0) set(AT_IMAGES);
    if (s.globeLvl > 0) set(AT_GLOBE);
    if (s.banishN > 0) set(AT_BANISH);
    if (s.flyRounds > 0) set(AT_FLYBUFF);
    if (s.extraTurns > 0) set(AT_TIMESTOP);
    if (s.growRounds > 0) set(AT_GROW);
    if (s.lightR > 0) set(AT_LIGHT);
    if (s.healAlly) set(AT_HEALALLY);
    if (s.rechAfter && s.uses < UNL && s.recharge) set(AT_RECHAFTER);
    if (s.cond == C_GRAPPLE && s.grabDot && s.dotN > 0) set(AT_GRABDOT);
    if (s.preCast && s.self) set(AT_PRECAST);
    if (s.immSave && s.delivery == 1 && !s.self) set(AT_IMMSAVE);
    if (s.noRepeat) set(AT_NOREPEAT);
    if (s.cmdAtk) set(AT_CMDATK);
    if (s.protect) set(AT_PROTECT);
    if (s.strike) set(AT_STRIKE);
    if (s.dcB > 0) set(AT_DCB);
    if (s.noRep) set(AT_NOREP);
    if (s.sumN > 0) {
        set(AT_SUMMON);
        set(AT_MINION);
        if (s.uses >= UNL) set(AT_SUMUNL);
    }
    for (auto& d : s.more)
        if (d.n > 0) set(AT_DMGMIX), set(AT_DMG);
}

int pickByWeight(const double* w) {
    double tot = 0;
    for (int i = 0; i < DT_N; i++) tot += w[i];
    double r = rand01() * tot;
    for (int i = 0; i < DT_N; i++) {
        r -= w[i];
        if (r <= 0 && w[i] > 0) return i;
    }
    for (int i = DT_N - 1; i >= 0; i--)
        if (w[i] > 0) return i;
    return 0;
}
// 22% физические (колющий/рубящий/дробящий), остальное по весам
int pickAbilityType() {
    if (rand01() < 0.22) {
        double r = rand01();
        return r < 0.385 ? DT_PIERCING : (r < 0.731 ? DT_SLASHING : DT_BLUDGEONING);
    }
    return pickByWeight(AB_W);
}
// Характеристика спасброска — по смыслу эффекта (воля — МДР, телесное — ТЕЛ, увёртывание — ЛОВ, сила личности — ХАР)
int meaningSave(const Spec& s) {
    double r = rand01();
    if (s.cond) {
        switch (s.cond) {
            case C_PRONE:
            case C_RESTR:
            case C_GRAPPLE:
                return r < 0.7 ? STR : DEX;
            case C_POISON:
                return CON;
            case C_BLIND:
                return r < 0.6 ? CON : DEX;
            case C_STUN:
            case C_PARAL:
                return r < 0.7 ? CON : WIS;
            case C_DISTR:
                return r < 0.5 ? INT : WIS;
            case C_LIT:
                return DEX;
            case C_FRIGHT:
            case C_FLEE:
                return WIS;
            case C_CHARM:
                return r < 0.7 ? WIS : CHA;
            case C_INCAP:
                return r < 0.5 ? WIS : CHA;
            case C_WEAK:
                return r < 0.5 ? CHA : WIS;
            case C_SLOW:
                return r < 0.6 ? WIS : DEX;
            default:
                return WIS;
        }
    }
    if (s.dmgN == 0 && s.dotN == 0) return s.pushFt > 0 ? STR : WIS;
    //                            СИЛ   ЛОВ   ТЕЛ   ИНТ   МДР   ХАР
    static const double T[DT_N][6] = {// кислота
                                      {0, .7, .3, 0, 0, 0},
                                      // дробящий
                                      {.5, .3, .2, 0, 0, 0},
                                      // холод
                                      {0, .4, .6, 0, 0, 0},
                                      // огонь
                                      {0, .9, .1, 0, 0, 0},
                                      // силовое поле
                                      {0, .8, .2, 0, 0, 0},
                                      // электричество
                                      {0, 1, 0, 0, 0, 0},
                                      // некротика
                                      {0, 0, .7, 0, .3, 0},
                                      // колющий
                                      {.3, .7, 0, 0, 0, 0},
                                      // яд
                                      {0, 0, 1, 0, 0, 0},
                                      // психический
                                      {0, 0, 0, .5, .4, .1},
                                      // излучение
                                      {0, .3, .5, 0, .2, 0},
                                      // рубящий
                                      {.4, .6, 0, 0, 0, 0},
                                      // звук
                                      {.2, 0, .8, 0, 0, 0}};
    for (int k = 0; k < 6; k++) {
        r -= T[s.dtype][k];
        if (r <= 0 && T[s.dtype][k] > 0) return k;
    }
    for (int k = 5; k >= 0; k--)
        if (T[s.dtype][k] > 0) return k;
    return WIS;
}
Spec randSpec(int sub) {
    Spec s;
    (void)sub;
    // способность на себя
    if (bern(BP_SELF, 0.22)) {
        s.self = true;
        s.delivery = 2;
        s.range = 0;
        // девять видов способности на себя, поровну; после теста 4 ноября — как у игрока, несколько эффектов в одной
        // способности (каждый следующий — с шансом BP_SELFMORE)
        auto addKind = [&](int k) {
        if (k == 0) {
            s.healN = magN(MG_HEALN, 1, MAX_DICE_G);
            s.healS = DICE[catN(BC_HEALS, DICE_N)];   // грань лечения (v21)
            if (bern(BP_HEALB, 0.30)) s.healB = 1 + catN(BC_HEALS + 8, 8);   // и прибавка (второе дыхание: 1к10 + 5)
        }
        else if (k == 1)
            s.teleFt = magN(MG_TELE, 10, MAX_FT_G, 10);
        else if (k == 2)
            s.hide = true;
        else if (k == 3) {
            s.buffN = magN(MG_BUFFN, 1, MAX_DICE_G);
            s.buffS = DICE[catN(BC_BUFFS, 4)];   // грань баффа оружия: к2…к8 (v21)
            s.buffRounds = magN(MG_BUFFR, 1, MAX_ROUNDS_G);
        } else if (k == 4)
            s.tempN = magN(MG_TEMPN, 1, MAX_DICE_G);
        else if (k == 5) {
            s.acB = magN(MG_ACB, 1, MAX_COUNT_G);
            s.acRounds = magN(MG_ACR, 1, MAX_ROUNDS_G);
        } else if (k == 6)
            s.darkR = magN(MG_DARKR, 10, MAX_FT_G, 10);
        else if (k == 7)
            s.invisN = magN(MG_INVISN, 1, MAX_ROUNDS_G);
        else {
            double q = rand01();
            s.sbAmt = magN(MG_SBAMT, 1, MAX_COUNT_G);   (void)q;
            s.sbRounds = magN(MG_SBR, 1, MAX_ROUNDS_G);
        }
        };
        int kinds = 0;
        for (int n = 0; n < 4; n++) {
            const int k = catN(BC_SELFKIND, 9);
            if (kinds >> k & 1) continue;
            kinds |= 1 << k;
            addKind(k);
            if (!bern(BP_SELFMORE, 0.25)) break;
        }
        // пакет атомов 2: прыжок вместо телепорта, отражения, сфера, полёт, ходы подряд, увеличение, свет
        if (s.teleFt > 0 && bern(BP_JUMP, 0.30)) { s.jump = true; s.teleFt = magN(MG_JUMP, 10, MAX_FT_G, 10); }
        if (bern(BP_IMAGES, 0.04)) s.images = magN(MG_IMAGES, 1, MAX_COUNT_G);
        if (bern(BP_GLOBE, 0.03)) { s.globeLvl = magN(MG_GLOBEL, 1, 9); s.globeRounds = magN(MG_GLOBER, 1, MAX_ROUNDS_G); s.radius = 10; }
        if (bern(BP_FLYBUFF, 0.04)) s.flyRounds = magN(MG_FLYR, 1, MAX_ROUNDS_G);
        if (bern(BP_TIMESTOP, 0.01)) s.extraTurns = magN(MG_XTURN, 1, 10);
        if (bern(BP_GROW, 0.03)) s.growRounds = magN(MG_GROWR, 1, MAX_ROUNDS_G);
        if (bern(BP_LIGHTS, 0.03)) { s.lightR = magN(MG_LIGHTR, 10, MAX_FT_G, 10); if (!s.buffRounds) s.buffRounds = 10; }
        // союзники (v21): лечение, хиты и баффы — и на союзника в пределах дальности
        if ((s.healN || s.tempN || s.buffN || s.acB || s.sbAmt || s.invisN) && bern(BP_ALLY, 0.30)) {
            s.ally = true;
            s.range = magN(MG_ALLYR, 5, MAX_FT_G, 5);
        }
        s.bonus = bern(BP_SELFBONUS, 0.4);
        // длящийся бафф — под концентрацией
        if ((s.buffN || s.acB || s.invisN || s.sbAmt) && bern(BP_SELFCONC, 0.30)) {
            s.conc = true;
            if (s.invisN) s.invisN = magN(MG_INVISN, 3, MAX_ROUNDS_G);
        }
    } else if (bern(BP_WALL, 0.05)) {
        // барьер (№22): временная стена поперёк пути на клетке в пределах дальности, без урона
        s.ground = true;
        s.delivery = 0;
        s.range = magN(MG_WALLRANGE, 10, MAX_FT_G, 5);
        s.wallLen = magN(MG_WALLLEN, 2, W);
        s.wallRounds = magN(MG_WALLR, 1, MAX_ROUNDS_G);
    } else {
        s.delivery = catP(BC_DELIV, 3, DELIV_CUM);
        // ХАР не симулируется
        const int sv[5] = {STR, DEX, CON, INT, WIS};
        s.saveStat = sv[catN(BC_SAVE, 5)];
        // любая дальность кратная 5 фт
        s.range = bern(BP_RANGE5, 0.30) ? 5 : magN(MG_RANGE, 10, MAX_FT_G, 5);
        // радиус 5..30 фт с шагом 5
        s.radius = bern(BP_NORAD, 0.70) ? 0 : magN(MG_RADIUS, 5, MAX_FT_G, 5);
        // область можно навести на любую клетку (№9, №22); избирательная бьёт только врагов (№17)
        if (s.radius > 0) {   // форма (v21): конус и линия — от себя, их длина больше радиуса сферы
            s.shape = catN(BC_SHAPE, SH_N);
            if (s.shape == SH_CONE || s.shape == SH_LINE) {
                s.radius *= 2;
                s.range = s.radius;
            }
        } else if (!s.self && bern(BP_MULTI, 0.12))
            s.targets = magN(MG_MULTI, 2, 10);
        if (s.radius > 0 && s.shape <= SH_CUBE && bern(BP_GROUND, 0.25)) s.ground = true;
        if (s.radius > 0 && bern(BP_SELECT, 0.30)) s.selective = true;
        if (bern(BP_HASDMG, 0.70)) {
            s.dmgN = magN(MG_DMGN, 1, MAX_DICE_G);
            s.dmgS = DICE[catN(BC_DMGS, DICE_N)];
        }
        if (s.dmgN) s.dtype = pickAbilityType();
        // модификация при сборке (№10): ещё кубики другого типа урона
        if (s.dmgN && bern(BP_DMGMIX, 0.10)) {
            Spec::Dmg d;
            d.n = magN(MG_MIXN, 1, MAX_DICE_G);
            d.s = DICE[catN(BC_MIXS, DICE_N)];
            d.type = pickAbilityType();
            if (d.type != s.dtype) s.more.push_back(d);
        }
        if (bern(BP_HASCOND, 0.50) || s.dmgN == 0) {
            s.cond = 1 + catN(BC_COND, C_N - 1);
            s.condRounds = (s.cond == C_PRONE) ? 1 : magN(MG_CONDR, 1, MAX_ROUNDS_G);
            // яд — спасбросок ТЕЛ
            if (s.cond == C_POISON) s.saveStat = CON;
            // без броска состояние держится ровно 1 раунд (длительность не продаётся)
            if (s.delivery == 2) s.condRounds = 1;
        }
        if (bern(BP_PUSH, 0.12)) {
            s.pushFt = magN(MG_PUSH, 5, MAX_FT_G, 5);
            if (bern(BP_COLL, 0.35)) s.collN = magN(MG_COLL, 1, MAX_DICE_G);
        } else if (bern(BP_PULL, 0.06))   // притягивание (v21)
            s.pullFt = magN(MG_PUSH, 5, MAX_FT_G, 5);
        if (s.dmgN && bern(BP_MAXHP, 0.05)) s.maxHpDmg = true;   // прочее (v21)
        if (s.dmgN && bern(BP_DRAIN, 0.06)) s.drain = true;
        if (bern(BP_DISPEL, 0.04)) s.dispel = true;
        if (bern(BP_BANISH, 0.03)) { s.banishN = magN(MG_BANISH, 1, MAX_ROUNDS_G); s.saveStat = CHA; }   // пакет атомов 2
        if (s.dmgN && bern(BP_HEALALLY, 0.03)) s.healAlly = true;
        if (s.cond == C_GRAPPLE && bern(BP_GRABRESTR, 0.4)) s.grabRestr = true;
        if (s.cond == C_GRAPPLE && bern(BP_GRABDOT, 0.2)) {
            s.grabDot = true;
            s.dotN = magN(MG_DOTN, 1, MAX_DICE_G);
            s.dotS = DICE[catN(BC_DOTS, DICE_N)];
        }
        // область делает местность труднопроходимой
        if (s.radius >= 10 && bern(BP_TERR, 0.10)) s.terr = true;
        // урон по времени
        if (bern(BP_DOT, 0.10)) {
            s.dotN = magN(MG_DOTN, 1, MAX_DICE_G);
            s.dotS = DICE[catN(BC_DOTS, DICE_N)];
            s.dotRounds = magN(MG_DOTR, 1, MAX_ROUNDS_G);
            if (!s.dmgN) s.dtype = pickAbilityType();
        }
        s.bonus = bern(BP_BONUS, 0.10);
        // состояние на 3–10 раундов — под концентрацией
        if (s.cond && s.cond != C_PRONE && s.delivery != 2 && bern(BP_CONC, 0.30)) {
            s.conc = true;
            s.condRounds = magN(MG_CONDR, 3, MAX_ROUNDS_G);
        } else if (s.dotN > 0 && bern(BP_CONC, 0.30)) {
            s.conc = true;
            s.dotRounds = magN(MG_DOTR, 3, MAX_ROUNDS_G);
        }
        // спасбросок — по смыслу эффекта
        s.saveStat = meaningSave(s);
        // этап 5б: своя СЛ способности (там, где есть спасбросок) и состояние без повторного спаса
        if ((s.delivery == 1 || (s.delivery == 0 && s.cond && s.cond != C_GRAPPLE)) && bern(BP_DCB, 0.20)) s.dcB = 1 + catN(BC_DCB, 8);
        if (s.cond && s.cond != C_PRONE && s.cond != C_GRAPPLE && s.delivery != 2 && s.condRounds >= 2 && bern(BP_NOREP, 0.15)) s.noRep = true;
    }
    {
        int u = catP(BC_USES, 4, USES_CUM);   // лимит или без лимита; лимит — во весь технический диапазон
        s.uses = u == 3 ? UNL : magN(MG_USES, 1, MAX_COUNT_G);
    }
    // перезарядка вместо лимита
    if (s.uses >= UNL && !s.self && bern(BP_RECHARGE, 0.20)) {
        double r2 = rand01();
        s.recharge = r2 < 0.40 ? 6 : (r2 < 0.80 ? 5 : 4);
    } else if (s.uses < UNL && bern(BP_RECHAFTER, 0.06)) {   // пакет атомов 3: кончились применения — перезарядка вернёт все
        s.recharge = 5 + irand(2);
        s.rechAfter = true;
    }
    // пакет атомов 1 (5 ноября): вместо одной атаки, невосприимчивость после успеха, не два раунда подряд, с начала боя
    if (!s.self && !s.bonus && bern(BP_INATTACK, 0.08)) s.inAttack = true;
    if (s.delivery == 1 && !s.self && bern(BP_IMMSAVE, 0.08)) s.immSave = true;
    if (bern(BP_NOREPEAT, 0.05)) s.noRepeat = true;
    if (s.self && (s.buffN || s.acB || s.tempN || s.invisN || s.sbAmt) && bern(BP_PRECAST, 0.12)) s.preCast = true;
    // реакция (№23): срабатывает сама по условию, вместо действия
    if (bern(BP_REACT, 0.08)) {
        s.inAttack = s.preCast = false;   // реакция — не часть хода
        s.react = 1 + catN(BC_REACTT, 4);
        s.bonus = false;
        if (s.react == 4 && !s.self && bern(BP_COUNTER, 0.3)) {   // контрзаклинание (v21): гасит способность врага в зоне
            const int rr = s.range, us = s.uses;
            s = Spec{};
            s.self = s.counter = true;
            s.react = 4;
            s.range = std::max(30, rr);
            s.uses = us;
        }
    }
    return s;
}
