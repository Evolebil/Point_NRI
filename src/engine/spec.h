// Способность = набор атомов: Spec, цена specPrice, описания
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Способность = набор атомов
// ---------------------------------------------------------------------------
struct Spec {
    // пакет атомов 1 (5 ноября): вместо одной атаки действия «Атака»; после успешного спасброска цель невосприимчива;
    // не два раунда подряд; действует с начала боя без действия (заранее наложенное, только «на себя»)
    bool inAttack = false, immSave = false, noRepeat = false, preCast = false;
    // пакет атомов 2: захват — схваченный опутан / урон по времени держится, пока держит; прыжок (teleFt без действия, за
    // движение); копии-отражения; сфера неуязвимости (уровень и раунды); изгнание (раунды); полёт на N раундов; ходов
    // подряд (остановка времени); увеличение на N раундов; свет (радиус); урон лечит союзников (они — цели)
    bool grabRestr = false, grabDot = false, jump = false, healAlly = false;
    int images = 0, globeLvl = 0, globeRounds = 0, banishN = 0, flyRounds = 0, extraTurns = 0, growRounds = 0, lightR = 0;
    bool rechAfter = false;   // пакет атомов 3: когда применения кончились — перезарядка возвращает все
    // 0 атака, 1 спасбросок, 2 без броска
    int delivery = 0;
    int saveStat = DEX;
    int range = 5, radius = 0, uses = UNL;
    bool bonus = false, self = false;
    int dmgN = 0, dmgS = 6;
    int cond = 0, condRounds = 1;
    int dotN = 0, dotS = 6, dotRounds = 0, darkR = 0, invisN = 0, slotLvl = 0;
    bool terr = false, conc = false;
    int pushFt = 0, healN = 0, teleFt = 0, buffN = 0, buffRounds = 0, tempN = 0, acB = 0, acRounds = 0;
    bool hide = false;
    // бафф характеристики на себя: +sbAmt к значению характеристики sbStat на sbRounds раундов
    int sbStat = 0, sbAmt = 0, sbRounds = 0;
    // урон при столкновении: Nк6 силового поля, если толчок упёрся в стену или существо (получают оба)
    int collN = 0;
    // тип урона способности
    int dtype = DT_FIRE;
    // перезарядка: 0 нет; иначе в начале СВОЕГО хода бросок d6, возврат при результате >= recharge (6, 5 или 4)
    int recharge = 0;
    // ground — способность бьёт по КЛЕТКЕ (центр области или места стены), а не по существу; wallLen клеток стены (временной) на wallRounds раундов
    bool ground = false;
    int wallLen = 0, wallRounds = 0;
    // v21, №10: урон — список компонентов {число, размер, тип}: первый — dmgN·к dmgS типа dtype, дальше more (другие типы
    // урона, задаются при сборке)
    struct Dmg {
        int n = 0, s = 6, type = DT_FIRE;
    };
    std::vector<Dmg> more;
    // v21, №23: реакция — способность срабатывает сама, реакцией, по условию: 1 меня атакуют, 2 мне нанесли урон,
    // 3 враг вошёл в зону (дальность способности), 4 враг применил способность в зоне; 0 — обычное действие
    int react = 0;
    // v21, №17: область бьёт всех в радиусе, и своих (миньонов, себя); избирательная — только врагов (за надбавку)
    bool selective = false;
    // v21, этап 5б: надбавка к СЛ этой способности (СЛ = 8 + мастерство + главная + dcB) и состояние без повторного спаса
    // (держится все condRounds раундов, если цель не снимет его иначе)
    int dcB = 0;
    bool noRep = false;
    // v21, оружие и Зуро: приём «при попадании» (применяется действием Атака оружием: эффекты ложатся на первое попадание,
    // плата — только если попал; доставка 0 — без спаса, 1 — спасбросок), атака второй рукой (бой двумя оружиями:
    // бонусным действием одна атака своим оружием после атаки оружием в этот ход; twfMod — модификатор к урону),
    // кость из запаса бойца (к урону приёма; poolN/poolS — копия запаса для цены), урон себе при применении, лечение
    // себе, если цель умерла от удара, обезоруживание; грань и прибавка лечения, грань баффа оружия
    bool onHit = false, offhand = false, twfMod = false, pool = false, disarm = false;
    std::string name;   // имя от игрока (конструктор); пусто — имя по эффектам
    // формы (v21): область — сфера (радиус radius, круг на карте), куб (полусторона radius, квадрат клеток), конус и
    // линия от себя длиной radius в сторону цели или клетки (линия — шириной 5 фт); targets — сколько целей на выбор
    // у способности без области (каждая в пределах дальности)
    int shape = 0, targets = 1;
    // союзники (v21): ally — эффекты «на себя» (лечение, хиты, баффы, невидимость) на себя или союзника в пределах range;
    // cmdAtk — приказ: союзник в пределах range бьёт оружием своей реакцией; protect / strike — для реакции «когда атакуют
    // союзника рядом» (react 5): помеха этой атаке / удар оружием по атакующему
    bool ally = false, cmdAtk = false, protect = false, strike = false;
    // прочее (v21): притягивание на pullFt к себе; урон снижает максимум хитов; вампиризм (лечит половиной нанесённого);
    // контрзаклинание (реакция 4: гасит способность врага в зоне); рассеивание (снимает с цели баффы и концентрацию)
    int pullFt = 0;
    bool maxHpDmg = false, drain = false, counter = false, dispel = false;
    int poolN = 0, poolS = 8;
    int selfN = 0, selfS = 4, killN = 0, killS = 6, killB = 0;
    int healS = 6, healB = 0, buffS = 4;
    // v21, призыв как способность: sumN копий шаблона Boss::tmpl[sumTmpl] за одно применение (0 — не призыв); sumCost —
    // цена шаблона (для цены способности)
    int sumTmpl = -1, sumN = 0;
    long long sumCost = 0;
    int formTmpl = -1;   // превращение (v21): облик — шаблон Boss::tmpl[formTmpl] (цена шаблона — в sumCost)
    std::vector<std::array<long long, 3>> sumMore;   // разные приспешники в одном призыве (v21): ещё группы {шаблон, копий, цена шаблона}
    int sumTotal() const {   // копий за одно применение — всех групп
        int n = sumN;
        for (auto& g : sumMore) n += (int)g[1];
        return n;
    }
    long long price = 0;
    // все компоненты урона по порядку (первый — основной)
    std::vector<Dmg> dmgParts() const {
        std::vector<Dmg> v;
        if (dmgN > 0) v.push_back({dmgN, dmgS, dtype});
        for (auto& d : more)
            if (d.n > 0) v.push_back(d);
        return v;
    }
    // средний урон всех компонентов в к6-эквивалентах (N·(грань+1)/7)
    double dmgD6() const {
        double t = 0;
        for (auto& d : dmgParts()) t += d.n * (d.s + 1) / 7.0;
        return t;
    }
};
const char* REACTN[6] = {"", "когда меня атакуют", "когда мне нанесли урон", "когда враг вошёл в зону", "когда враг применил способность в зоне", "когда атакуют союзника рядом"};
// число шагов по 5 фт сверх первых 5
int rangeIdx(int r) {
    return r <= 5 ? 0 : (r - 5) / 5;
}

// Множитель цены способности — СУММА процентов, а не произведение: 1 + надбавки − скидки, но не меньше MULT_FLOOR (одну способность нельзя удешевить больше чем на 80%).
// Надбавки: дальность, область, «без броска» (C−1), бонусное действие (C−1). Скидки: ограничение применений, перезарядка, концентрация, ячейка (1−C).
enum Shape { SH_SPHERE, SH_CUBE, SH_CONE, SH_LINE, SH_N };
const char* SHN[SH_N] = {"сфера", "куб", "конус", "линия"};
// клетка (x, y) в области: (ox, oy) — откуда (сам применивший: конус и линия идут от него), (cx, cy) — куда (центр сферы и
// куба, направление конуса и линии); size — радиус, полусторона или длина в футах. Сфера — круг клеток, центры которых
// не дальше size + 2.5 фт (шаблон D&D накрывает клетку хотя бы наполовину); конус — ширина на расстоянии d равна d
inline bool inShape(int shape, int size, int ox, int oy, int cx, int cy, int x, int y);
// область задевает бойца, если накрыла хоть одну клетку его квадрата (определение ниже — после Fighter)
struct Fighter;
bool inShapeF(int shape, int size, int ox, int oy, int cx, int cy, const Fighter& e);
inline bool inShape(int shape, int size, int ox, int oy, int cx, int cy, int x, int y) {
    if (shape == SH_CUBE) return 5 * std::max(std::abs(x - cx), std::abs(y - cy)) <= size;
    if (shape == SH_SPHERE) {
        const double dx = 5.0 * (x - cx), dy = 5.0 * (y - cy), r = size + 2.5;
        return dx * dx + dy * dy <= r * r;
    }
    const double ux = cx - ox, uy = cy - oy, len = std::sqrt(ux * ux + uy * uy);
    if (len <= 0) return false;
    const double vx = 5.0 * (x - ox), vy = 5.0 * (y - oy);
    const double t = (vx * ux + vy * uy) / len, perp = std::fabs(vx * uy - vy * ux) / len;
    if (t <= 0 || t > size + 2.5) return false;
    return shape == SH_LINE ? perp <= 3.0 : perp <= t / 2.0 + 2.5;
}
// число клеток формы (для цены): сфера и куб вокруг клетки, конус и линия — от себя вдоль оси
inline int shapeCells(int shape, int size) {
    const int R = size / 5 + 2;
    int n = 0;
    for (int y = -R; y <= R; y++)
        for (int x = -R; x <= R; x++)
            if (shape == SH_SPHERE || shape == SH_CUBE ? inShape(shape, size, 0, 0, 0, 0, x, y) : inShape(shape, size, 0, 0, std::max(1, size / 5), 0, x, y)) n++;
    return n;
}
// надбавка за область по площади: «радиус» квадрата с тем же числом клеток
inline double areaTier(const Spec& s) {
    if (s.radius <= 0) return 0;
    const double eq = (std::sqrt((double)shapeCells(s.shape, s.radius)) - 1) / 2.0;   // в клетках
    return tierN(TR_AREA, eq);
}
const double MULT_FLOOR = 0.2;
// Контекст цены (после теста 4 ноября): то, что в цене оружия и длящихся эффектов зависит от остального тела — атаки за
// действие, модификаторы оружия, КД, спасбросок ТЕЛ. Заполняет setPriceCtx (ниже, после Boss); без него — тело «по
// умолчанию» (одна атака, модификаторы 0, КД 10 + единицы от бюджета).
struct PriceCtx {
    bool on = false;
    int acV = 0, ac = 10, att = 1, strMod = 0, dexMod = 0, conSave = 0;
};
thread_local PriceCtx g_pc;
const double FIGHT_ROUNDS = 20.0;   // средняя длина боя в раундах: доля боя под длящимся эффектом
// удары за раунд для цены «за удар»: первая атака + доля за каждую следующую (C[доп. атака], без степени бюджета)
inline double hitsF(const Coef& C, int att) { return 1 + C[AT_WPNATT] * std::max(0, att - 1); }
// доля боя под длящимся эффектом: применений × длительность / длина боя, не больше 1 (без лимита — весь бой);
// перезарядка — сколько раз за бой она в среднем вернётся
inline double coverOf(const Spec& s, int rounds) {
    if (rounds <= 0) return 0;
    double casts = s.uses < UNL ? s.uses : FIGHT_ROUNDS;
    if (s.recharge) casts = std::min(casts, 1 + FIGHT_ROUNDS * (7 - s.recharge) / 6.0 * 0.5);
    return std::min(1.0, casts * rounds / FIGHT_ROUNDS);
}
// самая длинная длительность способности — то, что держит концентрация
inline int concDur(const Spec& s) {
    int d = std::max({s.buffRounds, s.acRounds, s.sbRounds, s.invisN, s.cond > 0 ? s.condRounds : 0, s.dotRounds, s.wallRounds});
    if (s.sumN > 0) d = (int)FIGHT_ROUNDS;   // приспешники держатся концентрацией весь бой
    return std::max(1, d);
}
// концентрация: какая доля длительности в среднем доживёт. По владельцу за раунд попадают ~1,5 атаки (бонус атаки врага —
// по бюджету), каждое попадание — спасбросок ТЕЛ СЛ 10. Свой бонус КД в той же способности тоже бережёт концентрацию.
inline double concKeep(const Spec& s) {
    const double B = g_B - STAT_BASE;
    const int atk = std::clamp((int)std::lround(4 + B / 800.0), 4, 14);
    const int ac = (g_pc.on ? g_pc.ac : 10 + std::clamp((int)std::lround(B / 800.0), 0, 10)) + (s.self ? s.acB : 0);
    const double pHit = std::clamp((21.0 - (ac - atk)) / 20.0, 0.05, 0.95), h = 1.5 * pHit;
    const double pFail = std::clamp((9.0 - g_pc.conSave) / 20.0, 0.05, 0.95);
    const double q = std::pow(1 - pFail, h);
    const int D = concDur(s);
    return q >= 1 - 1e-9 ? 1.0 : (1 - std::pow(q, D)) / ((1 - q) * D);
}
// Множитель цены способности. durOnly — для длящихся эффектов (бафф оружия, бонус КД): их скидка за применения уже в доле
// боя (coverOf), поэтому без лимита применений и перезарядки. Пол MULT_FLOOR ограничивает только скидки: надбавки
// (бонусное действие, «на союзника», дальность) учитываются всегда (после теста 4 ноября: при упоре в пол они пропадали).
double specMult(const Spec& s, const Coef& C, bool durOnly = false) {
    double m = 1, down = 0;
    if (!s.self) {
        m += C[AT_ARANGE] * tierN(TR_RANGE, rangeIdx(s.range));
        m += C[AT_AREA] * areaTier(s) * (s.shape == SH_CONE ? C[AT_SHCONE] : (s.shape == SH_LINE ? C[AT_SHLINE] : 1.0));   // по площади формы
        if (s.targets > 1 && s.radius == 0) m += C[AT_MULTI] * (s.targets - 1);   // несколько целей на выбор
        if (s.delivery == 2) m += C[AT_AUTO] - 1;
    }
    // приём из запаса костей ограничен запасом: скидка как за столько применений (не больше трёх)
    const int usesEff = s.pool ? std::clamp(s.poolN, 1, 3) : s.uses;
    if (usesEff < UNL && !durOnly) down += (1 - C[AT_ULIM]) * (1 - (usesEff - 1) * 0.2);
    if (s.onHit) m += C[AT_ONHIT] - 1;   // приём «при попадании»: к атаке оружием
    if (s.ally) m += C[AT_ALLYTGT] - 1 + C[AT_ARANGE] * tierN(TR_RANGE, rangeIdx(s.range));   // и на союзника в пределах дальности
    if (s.recharge && !durOnly) down += (1 - C[AT_RECHARGE]) * (1 - (7 - s.recharge) / 6.0);   // шанс вернуть заряд за ход p = (7 − порог)/6
    if (s.bonus) m += C[AT_BONUS] - 1;
    if (s.react) m += C[AT_REACT] - 1;   // действие сверх действия, как бонусное
    if (s.ground && !s.self) m += C[AT_GROUND];   // центр области — любая клетка (№9)
    if (s.selective && s.radius > 0 && !s.self) m += C[AT_SELECT];   // область щадит своих (№17)
    m += C[AT_DCB] * s.dcB;   // этап 5б: своя СЛ — дороже вся способность (и урон со спасом, и состояние)
    for (auto& d : s.more)
        if (d.n > 0) m += C[AT_DMGMIX];   // каждый дополнительный тип урона (№10)
    if (s.conc) down += std::max(0.0, 1 - C[AT_CONC] * concKeep(s));   // доля длительности, потерянная при срыве
    if (s.slotLvl > 0) down += 1 - C[AT_SLOTSPEC];
    // пакет атомов 1: вместо одной атаки и «с начала боя» — надбавки; невосприимчивость после успеха и «не два раунда подряд» — скидки
    if (s.inAttack) m += C[AT_INATTACK] - 1;
    if (s.preCast && s.self) m += C[AT_PRECAST] - 1;
    if (s.immSave && s.delivery == 1 && !s.self) down += 1 - C[AT_IMMSAVE];
    if (s.noRepeat) down += 1 - C[AT_NOREPEAT];
    if (s.healAlly) m += C[AT_HEALALLY] - 1;   // пакет атомов 2: лечит союзников
    if (s.rechAfter && s.uses < UNL && s.recharge) m += C[AT_RECHAFTER] - 1;   // пакет атомов 3: перезарядка возвращает все применения
    return m - std::min(down, 1 - MULT_FLOOR);
}
// множитель цены приспешников: коэффициент × (бюджет/1600)^-k; k подбирается калибровкой по наклону винрейта призывателей
inline double minMult(const Coef& C) {
    return C[AT_MINION] * std::pow(g_B / 1600.0, -C[AT_MINEXP]);
}
// Цена шаблонов призыва: шаблон × копии × число применений, всегда полная (скидки способности её не трогают); без лимита
// применений — как C[AT_SUMUNL] применений
inline long long summonTmplPrice(const Spec& s, const Coef& C) {
    if (s.formTmpl >= 0) {   // превращение: облик × число применений × множитель
        const double casts = s.uses < UNL ? s.uses : C[AT_SUMUNL];
        return (long long)std::ceil(C[AT_FORM] * s.sumCost * casts);
    }
    if (s.sumN <= 0) return 0;
    const double casts = s.uses < UNL ? s.uses : C[AT_SUMUNL];
    double pts = (double)s.sumCost * s.sumN;
    for (auto& g : s.sumMore) pts += (double)g[2] * g[1];   // остальные группы — так же, полной ценой
    return (long long)std::ceil(minMult(C) * pts * casts);
}
long long specPrice(const Spec& s, const Coef& C) {
    // атака второй рукой: доля цены одной атаки оружием (кубики оружия скопированы в dmgN/dmgS) и стиль
    if (s.offhand) return std::max(1LL, (long long)std::ceil(C[AT_WPNDICE] * s.dmgN * (s.dmgS + 1) / 2.0 * C[AT_TWF] + (s.twfMod ? C[AT_TWFMOD] * scl(AT_TWFMOD) : 0.0)));
    double base = 0;
    if (s.sumN > 0 || s.formTmpl >= 0) base += C[AT_SUMMON] * scl(AT_SUMMON);   // плата за призыв (превращение) — со скидками способности
    // лестница кубиков урона — в к6-эквивалентах (N·(грань+1)/7) по сумме всех компонентов: 1к100 стоит как ~14к6, а
    // 3к6 огня + 2к8 холода — как одна лестница на 3 + 2.57 к6 (модификация стоит разницу цен плюс надбавку AT_DMGMIX)
    if (s.dmgN > 0 || !s.more.empty() || s.pool) base += C[AT_DMG] * tierN(TR_DMG, s.dmgD6() + (s.pool ? (s.poolS + 1) / 7.0 : 0.0));   // кость запаса — тоже урон
    if (s.cond > 0) {
        // прон снимается на 1-м ходу цели, длительность не работает — не продаём её
        // без повторного спаса каждый раунд сверх первого стоит дороже (этап 5б): он держится наверняка
        double durMult = (s.cond == C_PRONE || s.delivery == 2) ? 1.0 : (1 + (s.noRep ? C[AT_NOREP] : 0.25) * (s.condRounds - 1));
        base += C[AT_PRONE + s.cond - 1] * durMult * scl(AT_PRONE + s.cond - 1);
    }
    if (s.wallLen > 0) base += C[AT_WALL] * s.wallLen * s.wallRounds;
    if (s.pushFt > 0) base += C[AT_PUSH] * tierN(TR_PUSH, s.pushFt / 5.0);
    if (s.pullFt > 0) base += C[AT_PULL] * tierN(TR_PUSH, s.pullFt / 5.0);   // притягивание (v21)
    if (s.counter) base += C[AT_COUNTER] * scl(AT_COUNTER);
    if (s.dispel) base += C[AT_DISPEL] * scl(AT_DISPEL);
    {   // снижение максимума хитов и вампиризм — доля цены урона
        const double dmgP = (s.dmgN > 0 || !s.more.empty()) ? C[AT_DMG] * tierN(TR_DMG, s.dmgD6()) : 0.0;
        if (s.maxHpDmg) base += C[AT_MAXHP] * dmgP;
        if (s.drain) base += C[AT_DRAIN] * dmgP;
    }
    if (s.pushFt > 0 && s.collN > 0) base += C[AT_COLLIDE] * s.collN;
    if (s.darkR > 0) base += C[AT_DARK] * s.darkR / 10.0;
    if (s.invisN > 0) base += C[AT_INVIS] * s.invisN;
    if (s.terr) base += C[AT_TERR];
    if (s.dotN > 0 && !(s.grabDot && s.cond == C_GRAPPLE)) base += C[AT_DOT] * s.dotN * (s.dotS + 1) / 2.0 / 3.5 * s.dotRounds;
    // захват (пакет атомов 2): схваченный опутан; урон по времени — пока держит (средняя длина захвата — C[AT_GRABDOT] раундов)
    if (s.cond == C_GRAPPLE && s.grabRestr) base += C[AT_PRONE + C_RESTR - 1] * scl(AT_PRONE + C_RESTR - 1) * C[AT_GRABRESTR];
    if (s.cond == C_GRAPPLE && s.grabDot && s.dotN > 0) base += C[AT_DOT] * s.dotN * (s.dotS + 1) / 2.0 / 3.5 * C[AT_GRABDOT];
    if (s.healN > 0 || s.healB > 0) base += C[AT_HEAL] * (s.healN * (s.healS + 1) / 2.0 + s.healB) / 3.5;   // за 1к6-эквивалент
    if (s.teleFt > 0) base += C[AT_TELE] * tierN(TR_TELE, s.teleFt / 10.0) * (s.jump ? C[AT_JUMP] : 1.0);   // прыжок — за движение
    // пакет атомов 2: отражения, сфера неуязвимости, изгнание, остановка времени, свет
    if (s.images > 0) base += C[AT_IMAGES] * scl(AT_IMAGES) * s.images;
    if (s.globeLvl > 0) base += C[AT_GLOBE] * scl(AT_GLOBE) * s.globeLvl * std::max(1, s.globeRounds) / 5.0;
    if (s.banishN > 0) base += C[AT_PRONE + C_PARAL - 1] * scl(AT_PRONE + C_PARAL - 1) * C[AT_BANISH] * (1 + 0.25 * (s.banishN - 1));
    if (s.extraTurns > 0) base += C[AT_TIMESTOP] * scl(AT_TIMESTOP) * s.extraTurns;
    if (s.lightR > 0) base += C[AT_LIGHT] * s.lightR / 10.0;
    if (s.hide) base += C[AT_HIDE] * scl(AT_HIDE);
    // длящиеся эффекты (после теста 4 ноября): цена того же постоянного свойства × доля боя под эффектом — временное
    // не дешевле постоянного. Бафф оружия — как кубики оружия на каждый удар; бонус КД — разница по лестнице КД.
    double dur = 0;
    if (s.buffN > 0) dur += C[AT_BUFF] * C[AT_WPNDICE] * s.buffN * (s.buffS + 1) / 2.0 * hitsF(C, g_pc.on ? g_pc.att : 1) * coverOf(s, s.buffRounds);
    if (s.sbAmt > 0) base += C[AT_STATBUF] * (s.sbAmt / 2.0) * s.sbRounds / 5.0;
    if (s.tempN > 0) base += C[AT_TEMP] * tierN(TR_TEMP, s.tempN);
    if (s.acB > 0) dur += C[AT_ACB] * (acCost(g_pc.acV + s.acB) - acCost(g_pc.acV)) * coverOf(s, s.acRounds);
    // полёт и увеличение на время (пакет атомов 2): как постоянное × доля боя
    if (s.flyRounds > 0) dur += C[AT_FLY] * scl(AT_FLY) * C[AT_FLYBUFF] * coverOf(s, s.flyRounds);
    if (s.growRounds > 0) dur += C[AT_GROW] * scl(AT_GROW) * FIGHT_ROUNDS * coverOf(s, s.growRounds);
    if (s.disarm) base += C[AT_DISARM];
    if (s.cmdAtk) base += C[AT_CMDATK] * scl(AT_CMDATK);      // приказ союзнику атаковать
    if (s.protect) base += C[AT_PROTECT] * scl(AT_PROTECT);   // реакция за союзника
    if (s.strike) base += C[AT_STRIKE] * scl(AT_STRIKE);
    if (s.killN > 0 || s.killB > 0) base += C[AT_KILLHEAL] * (s.killN * (s.killS + 1) / 2.0 + s.killB) / 3.5;
    if (s.selfN > 0) base -= C[AT_SELFDMG] * s.selfN * (s.selfS + 1) / 2.0 / 3.5;   // урон себе возвращает очки
    const double pm = s.pool ? C[AT_POOL] : 1.0;
    return std::max(1LL, (long long)std::ceil(base * specMult(s, C) * pm + dur * specMult(s, C, true) * pm)) + summonTmplPrice(s, C);
}
std::string describe(const Spec& s) {
    char buf[300];
    std::string r = s.react ? std::string("[реакция: ") + REACTN[s.react] + "] "
                  : (s.jump && s.teleFt ? "[без действия, за движение] " : (s.inAttack ? "[вместо одной атаки] " : (s.bonus ? "[бонус] " : "[действие] ")));
    if (s.uses < UNL) {
        std::snprintf(buf, sizeof buf, "%d раз(а) за бой; ", s.uses);
        r += buf;
    }
    if (s.recharge) {
        std::snprintf(buf, sizeof buf, "перезарядка %d–6; ", s.recharge);
        r += buf;
    }
    if (s.slotLvl) {
        std::snprintf(buf, sizeof buf, "ячейка %d ур.; ", s.slotLvl);
        r += buf;
    }
    if (s.conc) r += "концентрация; ";
    if (s.preCast && s.self) r += "действует с начала боя; ";
    if (s.noRepeat) r += "не два раунда подряд; ";
    if (s.immSave && s.delivery == 1 && !s.self) r += "после успешного спасброска цель к ней невосприимчива; ";
    if (s.rechAfter && s.uses < UNL && s.recharge) r += "кончились применения — перезарядка возвращает все; ";
    if (s.offhand) {
        std::snprintf(buf, sizeof buf, "атака второй рукой (своим оружием %dк%d)%s ", s.dmgN, s.dmgS, s.twfMod ? " с модификатором" : "");
        r += buf;
    } else if (s.onHit) {
        r += std::string("при попадании оружием") + (s.delivery == 1 ? std::string(" (спасбросок ") + SN[s.saveStat] + ")" : "") + (s.pool ? ", кость запаса" : "") + ": ";
    } else if (!s.self) {
        const char* d = s.delivery == 0 ? "атака" : (s.delivery == 1 ? "спасбросок" : "без броска");
        // стат значим только для спасброска (цель всегда спасается им) и для атаки с довеском-состоянием
        // (цель спасается им от довеска) — бросок самой атаки всегда идёт по главному стату босса.
        bool statMatters = (s.delivery == 1) || (s.delivery == 0 && s.cond > 0);
        std::snprintf(buf, sizeof buf, "%s (%s), дальн. %d фт", d, statMatters ? SN[s.saveStat] : "—", s.range);
        r += buf;
        if (s.dcB > 0) {
            std::snprintf(buf, sizeof buf, ", СЛ +%d", s.dcB);
            r += buf;
        }
        if (s.targets > 1 && !s.radius) {
            std::snprintf(buf, sizeof buf, ", до %d целей на выбор", s.targets);
            r += buf;
        }
        if (s.radius) {
            static const char* SHF[SH_N] = {", сфера радиусом %d фт%s", ", куб %d фт от центра%s", ", конус %d фт от себя%s", ", линия %d фт от себя%s"};
            std::snprintf(buf, sizeof buf, SHF[std::clamp(s.shape, 0, SH_N - 1)], s.radius, s.selective ? " (только врагов)" : " (задевает и своих)");
            r += buf;
        }
        r += ": ";
    } else if (s.sumN > 0) {
        std::snprintf(buf, sizeof buf, "призыв: %d копий шаблона %d (цена шаблона %lld)", s.sumN, s.sumTmpl + 1, s.sumCost);
        r += buf;
        for (auto& g : s.sumMore) {
            std::snprintf(buf, sizeof buf, " + %lld копий шаблона %lld (цена %lld)", g[1], g[0] + 1, g[2]);
            r += buf;
        }
    } else if (s.formTmpl >= 0) {
        std::snprintf(buf, sizeof buf, "превращение в облик (шаблон %d, цена шаблона %lld): свой запас хитов, способности в облике недоступны", s.formTmpl + 1, s.sumCost);
        r += buf;
    } else if (s.cmdAtk) {
        std::snprintf(buf, sizeof buf, "приказ союзнику в %d фт: удар оружием его реакцией ", s.range);
        r += buf;
    } else if (s.react == 5) {
        r += std::string(s.protect ? "помеха атаке по союзнику рядом " : "") + (s.strike ? "удар оружием по атакующему союзника " : "");
    } else if (s.ally) {
        std::snprintf(buf, sizeof buf, "на себя или союзника в %d фт: ", s.range);
        r += buf;
    } else
        r += "на себя: ";
    {
        const auto parts = s.dmgParts();
        for (size_t k = 0; k < parts.size(); k++) {
            std::snprintf(buf, sizeof buf, "%s%dк%d (%s) ", k ? "+ " : "урон ", parts[k].n, parts[k].s, DTN[parts[k].type]);
            r += buf;
        }
    }
    if (s.cond) {
        if (s.cond == C_PRONE)
            r += std::string("+ ") + CN[s.cond] + " (до след. хода цели) ";
        else {
            std::snprintf(buf, sizeof buf, "+ %s на %d р.%s ", CN[s.cond], s.condRounds, s.noRep ? " (без повторного спаса)" : "");
            r += buf;
        }
    }
    if (s.invisN) {
        std::snprintf(buf, sizeof buf, "невидимость на %d р. ", s.invisN);
        r += buf;
    }
    if (s.terr) r += "+ труднопроходимая местность в области ";
    // пакет атомов 2
    if (s.jump && s.teleFt) r += "прыжок: через ущелья и воду, не сквозь стены; ";
    if (s.images) r += "копии-отражения: " + std::to_string(s.images) + "; ";
    if (s.globeLvl) r += "сфера неуязвимости 10 фт от способностей до " + std::to_string(s.globeLvl) + " ур. на " + std::to_string(s.globeRounds) + " р.; ";
    if (s.banishN) r += "изгнание на " + std::to_string(s.banishN) + " р.; ";
    if (s.flyRounds) r += "полёт на " + std::to_string(s.flyRounds) + " р.; ";
    if (s.extraTurns) r += "ещё ходов подряд: " + std::to_string(s.extraTurns) + "; ";
    if (s.growRounds) r += "увеличение (размер +1, +1к4 к оружию) на " + std::to_string(s.growRounds) + " р.; ";
    if (s.lightR) r += "свет " + std::to_string(s.lightR) + " фт; ";
    if (s.healAlly) r += "союзников лечит вместо урона; ";
    if (s.cond == C_GRAPPLE && s.grabRestr) r += "схваченный опутан; ";
    if (s.cond == C_GRAPPLE && s.grabDot && s.dotN) r += "пока держит — " + std::to_string(s.dotN) + "к" + std::to_string(s.dotS) + " в ход; ";
    if (s.wallLen) {
        std::snprintf(buf, sizeof buf, "барьер: стена %d клеток на %d р. ", s.wallLen, s.wallRounds);
        r += buf;
    }
    if (s.ground) r += "(по клетке) ";
    if (s.darkR) {
        std::snprintf(buf, sizeof buf, "тьма вокруг себя, радиус %d фт ", s.darkR);
        r += buf;
    }
    if (s.dotN && !(s.grabDot && s.cond == C_GRAPPLE)) {
        std::snprintf(buf, sizeof buf, "+ урон по времени %dк%d (%s) на %d р. ", s.dotN, s.dotS, DTN[s.dtype],
                      s.dotRounds);
        r += buf;
    }
    if (s.pushFt) {
        std::snprintf(buf, sizeof buf, "+ толчок %d фт ", s.pushFt);
        r += buf;
    }
    if (s.pushFt && s.collN) {
        std::snprintf(buf, sizeof buf, "(при столкновении %dк6 силового поля) ", s.collN);
        r += buf;
    }
    if (s.healN) {
        std::snprintf(buf, sizeof buf, s.healB ? "лечение %dк%d+%d " : "лечение %dк%d ", s.healN, s.healS, s.healB);
        r += buf;
    }
    if (s.teleFt) {
        std::snprintf(buf, sizeof buf, "телепорт %d фт ", s.teleFt);
        r += buf;
    }
    if (s.hide) r += "скрытность ";
    if (s.buffN) {
        std::snprintf(buf, sizeof buf, "бафф +%dк%d к оружию на %d р. ", s.buffN, s.buffS, s.buffRounds);
        r += buf;
    }
    if (s.sbAmt) {
        std::snprintf(buf, sizeof buf, "бафф характеристики %s +%d на %d р. ", SN[s.sbStat], s.sbAmt, s.sbRounds);
        r += buf;
    }
    if (s.tempN) {
        std::snprintf(buf, sizeof buf, "временные хиты %dк6 ", s.tempN);
        r += buf;
    }
    if (s.acB) {
        std::snprintf(buf, sizeof buf, "+%d к КД на %d р. ", s.acB, s.acRounds);
        r += buf;
    }
    if (s.pool) {
        std::snprintf(buf, sizeof buf, "+ кость запаса к%d ", s.poolS);
        r += buf;
    }
    if (s.disarm) r += "+ обезоруживание ";
    if (s.pullFt) {
        std::snprintf(buf, sizeof buf, "+ притягивание %d фт ", s.pullFt);
        r += buf;
    }
    if (s.maxHpDmg) r += "+ снижает максимум хитов ";
    if (s.drain) r += "+ вампиризм ";
    if (s.dispel) r += "+ рассеивание ";
    if (s.counter) r += "контрзаклинание: гасит способность врага в зоне ";
    if (s.selfN) {
        std::snprintf(buf, sizeof buf, "урон себе %dк%d ", s.selfN, s.selfS);
        r += buf;
    }
    if (s.killN || s.killB) {
        std::snprintf(buf, sizeof buf, "если цель умерла — лечение себе %dк%d+%d ", s.killN, s.killS, s.killB);
        r += buf;
    }
    std::snprintf(buf, sizeof buf, "-- %lld очк.", s.price);
    r += buf;
    return r;
}

// вид оружия (основное лежит в полях Boss, запасные — в alt)
struct Wp {
    int n = 0, s = 0, range = 5, att = 1, reach = 5, type = DT_SLASHING;
    bool finesse = false, ammo = false;
    bool multi = false;   // пакет атомов 1: бьёт в том же действии «Атака», что и основное (укус + когти), а не вместо него
    long long cost = 0;
};
// Правило «когда → что» (v21, решение владельца 2026-11-02): условие (When) и эффект (What) — отдельные атомы, цена правила =
// цена эффекта × доля времени, когда выполняется условие. Заменило склейки: тактика стаи (союзник рядом → преимущество),
// ярость (ХП ≤ 50% → +к6), добивание (у противника состояние → +к6 раз в ход), безрассудство (атакую безрассудно →
// преимущество; атаки по мне с преимуществом), условная защита (условие → резист), преимущество на спасброски (всегда →).
// Противник — тот, с кем сейчас дело: цель моей атаки или тот, кто атакует меня. Резист и спасброски проверяются без
// противника, поэтому для них годятся только условия о себе
enum When { W_ALWAYS, W_ALLYADJ, W_HPLOW, W_HPHIGH, W_NOCOND, W_FOECOND, W_FOEHPLOW, W_RECKLESS, W_TWOWEAP, W_HEAVY, W_N };
enum What { E_ADVATK, E_DMG, E_AC, E_DISVSME, E_ADVVSME, E_RES, E_RESNM, E_ADVSAVE, E_SAVEB, E_ATKDIE, E_SAVEDIE, E_DMGRED, E_PIERCE, E_NOCOVER, E_POWER, E_NOOA,
           E_SKILLADV, E_DISATK, E_ACMINUS, E_N };   // 8 ноября: преимущество на навык; недостатки — помеха на мои атаки, −КД (возврат)
const char* WN[W_N] = {"всегда", "союзник рядом с противником", "мои ХП ≤ 50%", "мои ХП > 50%", "на мне нет состояний", "у противника состояние",
                       "противник ранен (ХП ≤ 50%)", "атакую безрассудно", "держу два оружия", "бью тяжёлым оружием"};
const char* EN[E_N] = {"преимущество на мои атаки", "+ кубики к урону оружия", "+ КД", "атаки по мне с помехой", "атаки по мне с преимуществом",
                       "резист ко всему урону", "резист к немагическому физическому", "преимущество на спасброски", "+ к спасброскам",
                       "+ кость к броскам атаки", "+ кость к спасброскам", "меньше урона от немагического физического", "мой урон игнорирует сопротивление",
                       "мои атаки не замечают укрытия", "размен: −к попаданию, +урон", "после моей атаки цель не провоцирует",
                       "преимущество на проверки навыка", "помеха на мои атаки (недостаток)", "− к КД (недостаток)"};
inline bool whenSelf(int w) { return w == W_ALWAYS || w == W_HPLOW || w == W_HPHIGH || w == W_NOCOND || w == W_RECKLESS || w == W_TWOWEAP || w == W_HEAVY; }
inline bool ruleAllowed(int w, int e) { return (e == E_RES || e == E_RESNM || e == E_ADVSAVE || e == E_SAVEB || e == E_SAVEDIE || e == E_DMGRED || e == E_PIERCE || e == E_NOOA || e == E_SKILLADV || e == E_DISATK || e == E_ACMINUS) ? whenSelf(w) : true; }
struct Rule {
    int when = W_ALWAYS, warg = 0;   // warg — состояние для W_FOECOND
    int what = E_ADVATK, n = 0, s = 6;   // n — кубиков (урон) или +КД
    bool once = false;   // урон «раз в ход»
    int allyR = 0;       // союзники (v21): правило действует и на своих в пределах allyR фт (аура; 0 — только на себя)
    long long price = 0;   // цена при покупке в генераторе (для пересчёта по готовому телу)
};

// Событие → эффект (v21, пакет «запуски»): срабатывает при событии, а не пока выполнено условие. Событие и эффект — свои
// атомы; uses — сколько раз за бой (UNL — без лимита)
enum TrigOn { T_CRIT, T_KILL, T_DOWN, T_DASH, T_DODGE, T_TURN, T_INIT, T_SAVEFAIL, T_MISS, T_MYSAVEFAIL, T_HITBY, T_HITMELEE, T_DMGTYPE, T_CHARGE, T_N };
enum TrigDo { X_BONUSATK, X_HEAL, X_TEMP, X_DMG, X_COND, X_STAY1, X_ADVNEXT, X_INITB, X_SPEED, X_REROLL, X_SELFCOND, X_N };
// атомы событий и эффектов: прежние идут подряд от AT_T_CRIT / AT_X_BONUSATK, добавленные 5 ноября — в конце списка атомов
inline int atTrigOn(int on) { return on == T_HITMELEE ? AT_T_HITMELEE : (on == T_DMGTYPE ? AT_T_DMGTYPE : (on == T_CHARGE ? AT_T_CHARGE : AT_T_CRIT + on)); }
inline int atTrigDo(int x) { return x == X_SELFCOND ? AT_X_SELFCOND : AT_X_BONUSATK + x; }
const char* TON[T_N] = {"я крит по врагу", "я опустил хиты врага до 0", "мои хиты упали до 0", "после Рывка", "после Уклонения", "в начале моего хода",
                        "в начале боя", "враг провалил спасбросок от моей способности", "я промахнулся атакой", "я провалил спасбросок", "по мне попали атакой",
                        "по мне попали атакой вблизи", "я получил урон выбранного типа", "попал вблизи после разбега"};
const char* TDN[X_N] = {"атака оружием бонусным действием", "лечение себе", "временные хиты себе", "урон цели", "состояние цели", "остаться с 1 хитом",
                        "преимущество на следующую атаку", "+ к инициативе", "+ скорость в этот ход", "переброс к20",
                        "состояние на себя"};
// какие эффекты подходят к событию: урон и состояние — только где есть цель события (крит, провал спаса); «остаться с 1» —
// только «хиты упали до 0»; инициатива — только в начале боя
inline bool trigAllowed(int on, int x) {
    const bool rollEv = on == T_MISS || on == T_MYSAVEFAIL || on == T_HITBY;   // переброс — только к броскам
    if (on == T_HITMELEE) return x == X_DMG || x == X_COND;   // огненный щит: урон или состояние атакующему вблизи
    if (on == T_CHARGE) return x == X_COND || x == X_DMG || x == X_BONUSATK;   // наскок: сбить с ног, урон, атака бонусом
    if (x == X_SELFCOND) return on == T_DMGTYPE || on == T_TURN;   // страх огня: помеха себе после урона этим типом
    if (on == T_DMGTYPE) return x == X_TEMP || x == X_HEAL || x == X_ADVNEXT || x == X_SPEED;
    if (x == X_REROLL || rollEv) return x == X_REROLL && rollEv;
    if (x == X_STAY1) return on == T_DOWN;
    if (on == T_DOWN) return x == X_STAY1 || x == X_TEMP;
    if (x == X_INITB) return on == T_INIT;
    if (x == X_DMG || x == X_COND) return on == T_CRIT || on == T_SAVEFAIL;
    if (x == X_BONUSATK) return on == T_CRIT || on == T_KILL;
    if (x == X_SPEED) return on == T_TURN;   // добавка скорости — к ходу, который ещё не начал двигаться
    if (on == T_DASH) return false;          // «после Рывка» — позже: бот решает о Рывке задним числом, повтор боя разошёлся бы
    return true;
}
struct Trig {
    int on = T_KILL, what = X_HEAL, n = 1, s = 6, b = 0, cond = 0, uses = UNL;
    int arg = 0;   // тип урона для T_DMGTYPE («я получил урон этого типа»)
};

struct Boss {
    std::array<int, STAT_N> stats{};
    // acV — купленные единицы КД сверх 10 (цена acCost), ac — итоговый КД = 10 + acV
    int ac = 0, acV = 0, hp = 1;
    // reach — досягаемость ближнего оружия (5/10/15/20 фт)
    int wN = 0, wS = 0, wRange = 5, wAtt = 1, reach = 5;
    bool wFinesse = false;
    int regenN = 0;
    bool fly = false;
    unsigned immCond = 0, saveProf = 0;
    std::vector<Rule> rules;   // «когда → что»
    std::vector<Trig> trigs;   // «событие → эффект» (v21)
    // оружие и Зуро: лёгкое (условие боя двумя оружиями), дальняя дистанция (0 — нет; дальше обычной — с помехой), запас
    // костей для приёмов (кости превосходства, ки): число и грань
    bool wLight = false;
    int wLong = 0, poolN = 0, poolS = 8;
    // доспехи и щит (v21): вид доспеха (Armor), щит, помеха к Скрытности, без владения; двуручное оружие, универсальное
    // (грань двумя руками, 0 — нет)
    int armor = 0;
    bool shield = false, armStealth = false, noArmProf = false, wTwoH = false;
    // размеры (v21): 0 крошечный … 2 средний … 5 громадный; тяжёлое оружие (маленьким — помеха)
    int size = 2;
    bool wHeavy = false;
    int truesight = 0;   // истинное зрение (v21), фт: видит невидимых и сквозь магическую тьму
    int wVers = 0;
    // v21, владения: общий бонус мастерства (+2…+6, покупается сверх +2), без владения своим оружием (мастерство не идёт к
    // атакам оружием; очки возвращаются), владение навыками и экспертиза (биты Skill), число инструментов
    int prof = 2;
    bool wNoProf = false;
    unsigned skillProf = 0, skillExp = 0;
    int toolN = 0;
    int speed = 30;   // скорость, фт (v21, №7: покупается вся, с нуля, шагами по 5 фт)
    std::vector<Spec> abil;
    // шаблоны приспешников призывающих способностей (Spec::sumTmpl — индекс здесь)
    std::vector<Boss> tmpl;
    // wType — тип урона оружия (колющий/рубящий/дробящий)
    int wType = DT_SLASHING, mainStat = STR;
    // защиты по типам урона (биты DT)
    unsigned resMask = 0, immMask = 0, vulnMask = 0;
    // Перенаправление
    bool redirect = false;
    // иммунитет к отталкиванию; Отход бонусным действием
    bool immPush = false, disengB = false;
    // parry — бонус к КД Парирования (0 нет)  // защита «от немагического» и магическое оружие
    bool resNM = false, immNM = false, wMagic = false;
    int legRes = 0, legAct = 0, parry = 0;
    bool uncanny = false;
    // пакет атомов 1 (5 ноября): преимущество на спасброски от способностей; вдвое меньше урона от способностей;
    // критические попадания по нему — обычные
    bool magicRes = false, spellRes = false, critImm = false;
    // пакет атомов 2: оружие посеребрённое; свет вокруг себя (фт); довесок «захват»: схваченный опутан, урон пока держит
    bool wSilver = false, rGrabRestr = false;
    int lightR = 0, rGrabDotN = 0, rGrabDotS = 6;
    int unarmoredStat = -1;   // пакет атомов 3: без доспеха к КД + модификатор этой характеристики (−1 — нет)
    AMask atoms = 0;
    int rN = 0, rS = 0, rType = DT_FIRE, rCond = 0, rRounds = 1;
    long long rcost = 0;
    int dv = 0, blindsight = 0;
    bool ignDiff = false;
    bool climbS = false, swimS = false, burrowS = false;   // скорости лазания, плавания, копания (v21)
    bool evasion = false, sunSens = false;                 // Увёртливость; солнечная чувствительность (v21)
    int ctype = 9;                                         // тип существа (PHB/MM, v21): по умолчанию гуманоид
    int lair = 0;                                          // действия логова (v21): уровень 0–3
    int auraN = 0, auraS = 6, auraType = DT_FIRE, auraRad = 0, auraCond = 0, auraRounds = 1;
    // всплеск действий, контратака, ячейки по уровням 1..9   // довесок к удару оружием (раз в ход): доп. урон и/или состояние
    int surge = 0;
    bool riposte = false;
    int slots[10] = {};
    unsigned suppMask = 0;
    int suppTgt = 0;
    // естественное оружие (укус, когти): у существ с ИНТ<=3 всё оружие естественное
    bool wNatural = false;
    // боеприпасное (только ЛОВ); запасное оружие (арсенал) и его цена
    bool wAmmo = false;
    std::vector<Wp> alt;
    long long acost = 0;
    long long cost = 0, wcost = 0;
    bool at(int a) const { return (atoms.w[a >> 6] >> (a & 63)) & 1ULL; }
    int mod(int s) const { return modOf(stats[s]); }
};
// есть ли у босса способность-реакция (№23)
inline bool hasReact(const Boss& b) {
    for (auto& s : b.abil)
        if (s.react) return true;
    return false;
}
// Смена оружия: меняет основное оружие босса (поля Boss) местами с запасным k
void swapWeapon(Boss& b, size_t k) {
    Wp& a = b.alt[k];
    std::swap(a.n, b.wN);
    std::swap(a.s, b.wS);
    std::swap(a.range, b.wRange);
    std::swap(a.att, b.wAtt);
    std::swap(a.reach, b.reach);
    std::swap(a.type, b.wType);
    std::swap(a.finesse, b.wFinesse);
    std::swap(a.ammo, b.wAmmo);
    std::swap(a.cost, b.wcost);
}
// Множитель цены оружия — сумма надбавок: 1 + дальность + досягаемость + дополнительные атаки
// наценка за дальность и досягаемость (число атак — отдельно, в wpHits)
double wpMult(const Coef& C, int range, int reach, int att) {
    (void)att;
    return 1 + C[AT_WPNRANGE] * tierN(TR_RANGE, rangeIdx(range)) + C[AT_REACH] * (range <= 5 ? (reach - 5) / 5.0 : 0.0);
}
// урон оружия за действие в «очках среднего урона» (после теста 4 ноября): кубики первой атаки + каждая следующая —
// полный удар (кубики и модификатор оружия), с долей C[доп. атака]. Модификатор первой атаки оплачен в характеристике.
// Раньше доп. атаки стоили по кубикам × (бюджет/1600)^1.79: на больших бюджетах вторая атака была дороже первой в десятки раз.
double wpHits(const Coef& C, int n, int s, int range, int att, bool finesse, bool ammo) {
    const int mod = !g_pc.on ? 0 : (range > 5 && ammo ? g_pc.dexMod : (finesse ? std::max(g_pc.strMod, g_pc.dexMod) : g_pc.strMod));
    const double avg = n * (s + 1) / 2.0;
    return avg + C[AT_WPNATT] * std::max(0, att - 1) * (avg + std::max(0, mod));
}
long long wpCostOf(const Coef& C, int n, int s, int range, int reach, int att, bool finesse, bool ammo) {
    return (long long)std::ceil(C[AT_WPNDICE] * wpHits(C, n, s, range, att, finesse, ammo) * wpMult(C, range, reach, att)) + (finesse ? 3 : 0);
}
long long wpPrice(const Wp& p, const Coef& C) { return wpCostOf(C, p.n, p.s, p.range, p.reach, p.att, p.finesse, p.ammo); }
// дальняя дистанция оружия (v21): доля надбавки за дальность от обычной до дальней (дальше обычной — с помехой)
long long weaponLongPrice(const Coef& C, int wN, int wS, int range, int lng, int reach, int att) {
    if (lng <= range || range <= 5) return 0;
    return (long long)std::ceil(C[AT_WPNDICE] * wpHits(C, wN, wS, range, att, false, false) * (wpMult(C, lng, reach, att) - wpMult(C, range, reach, att)) * C[AT_WLONG]);
}
void markWp(const Wp& p, AMask& m) {
    if (p.att >= 2) m |= ONE << AT_WPNATT;
    if (rangeIdx(p.range) >= 1) m |= ONE << AT_WPNRANGE;
    if (p.range <= 5 && p.reach > 5) m |= ONE << AT_REACH;
}
// Сборка вида оружия на бюджет: кубики без предела, число атак, дальность любая кратная 5 фт, досягаемость, вид
// (фехтовальное/боеприпасное/метательное)
