// Мир и бойцы: Fighter, World, клетки по размеру, путь, движение
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Мир, бойцы, движение
// ---------------------------------------------------------------------------
struct Fighter {
    Boss b;
    mutable long long fpKey = -1;   // кэш квадрата крупного (fpN): узел и версия карты
    mutable int fpVal = 1;
    int team = 0;
    int hp = 1, maxHp = 1;
    int hazTurn = -1;   // номер хода (g_turnSerial), когда его уже задела опасная зона
    bool up = false;    // стоит на втором ярусе (мост, балкон, навес)
    int alt = 0;        // высота полёта над землёй (настилом), уровней по 10 фт (v21)
    int lairLast = 0;   // прошлое действие логова (1 обвал, 2 трясина): подряд одно и то же нельзя
    // превращение (v21): в облике — тело шаблона со своим запасом хитов; свой облик (Boss) и хиты ждут возврата
    bool inForm = false;
    int formHpSave = 0, formMaxSave = 0;
    std::shared_ptr<Boss> formOrig;
    int x = 0, y = 0, init = 0;
    // cimm: невосприимчивость после окончания состояния
    int cond[C_N] = {};
    int cstat[C_N] = {};
    int cdc[C_N] = {};
    int cimm[C_N] = {};
    int cgrit[C_N] = {};   // нарастающая стойкость (v21, этап 7): проваленных повторных спасбросков от этого жёсткого контроля подряд
    bool hidden = false, fresh = false;
    // приспешник: кто призвал (индекс бойца) и какой способностью; vanished — исчез (концентрация призыва снята)
    int summoner = -1, sumSpec = -1;
    bool vanished = false;
    std::vector<int> sumMade;   // у призывателя: призвано каждой способностью за бой (технический потолок)
    int sumTurn = -1, sumTurnN = 0;   // призывов в раунде sumTurn (растяжка)
    unsigned onceUsed = 0;   // правила «когда → что» с уроном «раз в ход», сработавшие в этот ход (биты по номеру правила)
    int poolLeft = 0;   // кости запаса (приёмы)
    int helpBy = -1;      // Помощь (v21): кто отметил меня — первая атака его союзника по мне с преимуществом
    mutable std::vector<int> trigLeft;   // «событие → эффект» (v21): сколько раз ещё сработает (переброс спаса — и в const-проверках)
    bool advNext = false, pendBonus = false;   // преимущество на следующую атаку; ждёт атака бонусным действием
    int pendSpeed = 0, pendTgt = -1;           // добавка к скорости этого хода; цель атаки бонусом
    int noOAvs = -1;   // Подвижный (v21): этот боец не провоцирует от меня до конца хода того, кто меня ударил
    bool readied = false; // Подготовка (v21): ударю реакцией, когда враг войдёт в досягаемость
    int readyAbil = -1;   // 8 ноября: подготовленное действие — оружие (−1) или способность k
    int readyTrig = 1;    // условие: 1 — враг вошёл в досягаемость (дальность способности), 2 — враг применил способность в ней
    bool wAtkTurn = false, disarmed = false;   // атаковал оружием в этот ход (для второй руки); обезоружен
    bool riderUsed = false, reactionUsed = false, dodging = false, disengaged = false,
         wasReckless = false;
    // источники очарования и захвата   // legLeft — очки легендарных действий   // lrLeft — оставшиеся заряды
    // легендарного сопротивления
    int poisonTick = 0;
    int lrLeft = 0;
    int legLeft = 0;
    int charmSrc = -1, grapSrc = -1;
    // действующий бафф характеристики (значение прибавлено прямо к b.stats)
    int sbStat = 0, sbAmt = 0, sbRounds = 0;
    int buffRounds = 0, buffN = 0, buffS = 4;
    // урон по времени, висящий на бойце
    int dotN = 0, dotS = 6, dotType = DT_FIRE, dotRounds = 0;
    // политика этой команды
    const Pol* pol = &g_pol;
    // ходом управляет обучаемый бот
    bool rl = false;
    // подавление пассивок (ходов осталось), идёт ли свой ход
    int supp = 0;
    bool inTurn = false;
    // концентрация (индекс способности; СЛ проверки за каждое попадание с последней проверки), чьё это DoT/состояние
    // (+1), всплески, ячейки
    // погона (DMG): истощение (0–5), рывки за бой, лимит рывков без риска (3 + мод. ТЕЛ), рывок в этом ходу, осложнение, ждущее бойца, штраф к скорости в этом ходу, побег
    int exh = 0, dashes = 0, dashLimit = 3, comp = 0, speedPen = 0;
    // свои случайные числа для решений бота (шум, послушание, направление отхода): броски кубиков идут из общего rng,
    // поэтому ход, записанный примитивами и повторённый, даёт тот же бой (v21, №4)
    Rng ai;
    bool dashedNow = false, fled = false;
    bool struckFoe = false;   // в этот ход атаковал врага или применил по нему способность (погоня, v21 №13)
    int fledWhy = 0;   // 1 вышел за край арены, 2 оторвался от преследователя, 3 преследователь выбился из сил
    int concSpec = -1, dotConc = 0, surgeLeft = 0;
    std::vector<int> concDCs;
    int condConc[C_N] = {};
    int slotLeft[10] = {};
    // невидимость: ходов осталось (не спадает от атаки, в отличие от «скрытности»)
    int invisRounds = 0;
    // временные хиты; бонус КД и сколько своих ходов он ещё действует
    int temp = 0, acBuff = 0, acBuffRounds = 0;
    // кто последним нанёс урон (для реакции «мне нанесли урон»; −1 — никто), сбрасывается после проверки реакций
    int hurtBy = -1;
    std::vector<int> uses;
    std::vector<int> usedRound;   // раунд последнего применения способности k (для «не два раунда подряд»)
    std::vector<int> immAbil;     // «невосприимчив после успеха»: ключи (заклинатель × 64 + способность)
    // пакет атомов 2: состояние от механик (не входит в C_N — сеть-боец считает состояния по своей таблице)
    bool restrByGrab = false;     // опутан, пока схвачен
    int grabDotN = 0, grabDotS = 6, grabDotType = DT_BLUDGEONING, grabDotSrc = -1;   // урон в начале хода, пока схвачен
    int images = 0;               // копии-отражения
    int globeLvl = 0, globeR = 0, globeRounds = 0;   // своя сфера неуязвимости
    int banished = 0, banishSrc = -1;   // изгнан на N раундов (вне боя)
    int flyBuff = 0;              // полёт на N раундов (b.fly был false)
    int extraTurns = 0;           // остановка времени: ещё ходов подряд
    int growRounds = 0;           // увеличен на N раундов
    int lightRounds = 0, lightR = 0;   // свет от способности
    int turnX0 = -1, turnY0 = -1; // где начал ход (наскок: разбег)
    int mod(int s) const { return modOf(b.stats[s]); }
    int mainMod() const { return mod(b.mainStat); }
    // боеприпасное — только ЛОВ, метательное — СИЛ (или фехтовальное)
    int wMod() const {
        if (b.wRange > 5) return b.wAmmo ? mod(DEX) : (b.wFinesse ? std::max(mod(STR), mod(DEX)) : mod(STR));
        return b.wFinesse ? std::max(mod(STR), mod(DEX)) : mod(STR);
    }
    int dc() const { return 8 + b.prof + mainMod(); }
    // бонус атаки оружием (мастерство — если владеет своим оружием) и способностью (мастерство всегда)
    int wAtk() const { return wMod() + (b.wNoProf ? 0 : b.prof); }
    int sAtk() const { return mainMod() + b.prof; }
    // бонус проверки навыка: модификатор + мастерство при владении + ещё раз при экспертизе
    int skill(int k) const { return mod(SKST[k]) + (((b.skillProf >> k) & 1u) ? b.prof : 0) + (((b.skillExp >> k) & 1u) ? b.prof : 0); }
};
// Реакция — один путь списания для всех реакций (провоцированная атака, Парирование, контратака, перенаправление,
// способность-реакция)
inline void spendReaction(Fighter& f) {
    f.reactionUsed = true;
}
// Метрики использования (ТЗ v21, №6) за бой, по главным боссам 0 и 1: атак оружием и способностями (из них — из-за
// пределов досягаемости цели), пройдено фт (из них — не на сближение: отход, выход из укрытия, возврат), применений
// скрытности, невидимости, тьмы. Обнуляются в начале боя; калибровка по ним не снижает цену неиспользуемых атомов
enum UseKind { U_WATK, U_WOUT, U_SATK, U_SOUT, U_FT, U_FTAWAY, U_HIDE, U_INVIS, U_DARK, U_N };
thread_local long long g_use[2][U_N] = {};
// применений способностей по атомам (их отметки markSpec) за бой, у главных боссов 0 и 1 — используемость атомов способностей
thread_local long long g_castAtom[2][AT_N] = {};

constexpr int MAXT = 64;   // команд в бою — технический предохранитель (v21: ограничения нет, только он)
thread_local unsigned g_mapSerial = 0;   // версии карт (World::mapVer) — общие для потока, чтобы не совпадали у разных боёв
struct World {
    // diff — труднопроходимая местность, dark — тьма
    bool wall[W][W] = {};
    bool climb[W][W] = {};
    bool diff[W][W] = {};
    bool dark[W][W] = {};
    // арена — прямоугольник aw × ah клеток (мин. 5×5), всё за его пределами — стена; theme: 0 дикая местность, 1 город
    int aw = W, ah = W, theme = 0;
    bool pit[W][W] = {};                  // ущелье: пешком не пройти, обзор не закрывает, мост — обычные клетки в нём
    bool water[W][W] = {};                // ручей (труднопроходим: diff); отдельный слой нужен для рисунка и описания
    unsigned char height[W][W] = {};      // возвышенность: 0, 1, 2 (по 10 фт); подъём +5 фт движения за уровень, сброс вниз — падение
    bool tree[W][W] = {};                 // дерево (высокий предмет): обзор закрывает, внизу не пройти — только залезть (вершина на TREE_UP выше)
    unsigned char haz[W][W] = {};         // опасная зона: вид (HAZ), 0 — нет
    bool ladder[W][W] = {};               // лестница: с неё и на неё — любой перепад высоты (лезть: +5 фт за уровень)
    unsigned char deck[W][W] = {};        // второй ярус (мост, балкон, навес, потолок): уровень его верха, 0 — нет
    short dhp[W][W] = {};                 // хиты второго яруса (обрушить)
    unsigned char door[W][W] = {};        // дверь в проёме: 0 нет, 1 закрыта (как стена: не пройти, не видно), 2 открыта
    bool day = true;                      // день: под открытым небом — солнце (солнечная чувствительность, v21)
    // версия карты: растёт, когда меняются стены, ямы, деревья, настилы (кэш квадратов крупных). Новый мир сразу берёт
    // свою версию: с 0 у всех кэш потока принимал за свои клетки прошлого боя на том же адресе (бой зависел от предыдущих)
    unsigned mapVer = ++g_mapSerial;
    bool strong[W][W] = {};               // крепкая стена (крепость, башни): больше хитов
    bool mainc[W][W] = {};                // клетка основной связной части арены (там начинают бой)
    unsigned long long wseed = 0;         // мир, из которого вырезана арена (0 — не из мира), и угол участка
    int wsize = 0, wx = 0, wy = 0;
    int hazN = 2, hazDC = 13;             // урон опасной зоны — hazN к10, спасбросок СЛ hazDC (по силе боссов боя)
    short ohp[W][W] = {};                 // хиты объекта в клетке (стена, дерево, укрытие); 0 — неразрушим (край арены, барьеры способностей)
    // погоня: идёт ли, кто преследуемый (индекс босса), не терял ли он из виду преследователя за раунд
    bool chaseOn = false, chaseSeen = true;
    int chasePrey = -1;
    // cmd — цель, выбранная главным боссом команды: миньоны бьют её
    std::vector<Fighter> f;
    int cmd[MAXT] = {};   // по команде (заполняется в начале боя)
    // динамические изменения арены от способностей: {x, y, вид (1 временная стена, 2 труднопроходимая местность, 3 тьма), раундов до исчезновения (−1 навсегда)}
    std::vector<std::array<int, 4>> dyn;
    // туман войны (v21, №12): что сторона знает о врагах — {x, y, раунд, когда видели} по индексу бойца (−1 — не знает);
    // раунд, когда сторона последний раз видела хоть одного врага
    std::vector<std::array<int, 3>> known[MAXT];
    int knownRound[MAXT] = {};
    // команды (v21): бойцы 0..nLead−1 — главные (из состава боя), остальные — приспешники; nTeams — команд в бою; kills —
    // убитых врагов на счету команды (очки, если бой дошёл до предела раундов)
    int nTeams = 2, nLead = 2;
    mutable int kills[MAXT] = {};
};
// ---- объекты и высота (v21, местность) ----
// Уровень клетки — в единицах по 10 фт: земля (height) плюс вершина дерева, если существо на нём. Расстояние между
// клетками — трёхмерное, как в PHB: 5 фт × наибольшее из смещений по x, y и высоте (уровень = 2 клетки).
// Объекты — стена, дерево, укрытие — имеют КД и хиты (DMG «Объекты»: дерево КД 15, камень КД 17; иммунитет к яду и
// психическому урону). Разрушенный объект оставляет обломки — труднопроходимую местность.
constexpr int TREE_UP = 2, WALL_UP = 3;   // дерево — 20 фт, стена — 30 фт над землёй
constexpr int OBJ_WALL_HP = 54, OBJ_TREE_HP = 27, OBJ_CLIMB_HP = 27, OBJ_STRONG_HP = 120, OBJ_DOOR_HP = 18;
// Опасные зоны (v21, местность): вещество задаёт только тип урона и спасбросок. Как в DMG: урон, когда существо впервые
// за ход входит в зону или начинает в ней ход; спасбросок — половина. Летун не касается жидкостей и льда, но не облаков.
struct HazKind { const char* name; int dtype, save; bool flySafe; };
constexpr HazKind HAZ[] = {{"", 0, 0, true},
                           {"лава", DT_FIRE, DEX, true},          {"кислота", DT_ACID, DEX, true},
                           {"ядовитый дым", DT_POISON, CON, false}, {"ледяное поле", DT_COLD, CON, true},
                           {"грозовое поле", DT_LIGHTNING, DEX, false}, {"некротический туман", DT_NECROTIC, CON, false}};
constexpr int HAZ_N = 7;
inline int zAt(const World& w, int x, int y) { return w.height[y][x] + (w.tree[y][x] ? TREE_UP : 0); }
// Второй ярус (2.5D): в клетке земля (height, дерево) и, если есть, верхний настил deck. Под настилом пройти можно,
// если хватает высоты по росту (рост: до среднего — 1 уровень, большой — 2, огромный — 3, громадный — 4); настил, на
// который не забраться, — просто потолок. Шаг между узлами (клетка, ярус): перепад до 1 уровня — подъём (+5 фт за
// уровень), больше — обрыв: только по лестнице, на дерево и с него, летуну — под открытым небом (не под настилом).
constexpr int DECK_HP = 40;
inline int sizeLv(const Fighter& f) { return f.b.size <= 2 ? 1 : f.b.size - 1; }
inline int nodeZ(const World& w, int x, int y, int l) { return l ? w.deck[y][x] : zAt(w, x, y); }
inline int zF(const World& w, const Fighter& f) { return nodeZ(w, f.x, f.y, f.up ? 1 : 0) + f.alt; }
// Высота полёта (v21): летун висит на alt уровней (по 10 фт) над землёй или настилом под собой — до ALT_MAX, а под настилом
// (потолком) — пока помещается по росту. Подняться или опуститься на уровень — 10 фт движения.
constexpr int ALT_MAX = 6;
inline bool canStayAloft(const Fighter& f);   // держится в воздухе (ниже)
inline int maxAltAt(const World& w, const Fighter& f, int x, int y, int l) {
    if (!f.b.fly) return 0;
    int m = ALT_MAX;
    const int n = sizeLv(f) > 1 ? sizeLv(f) : 1;
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++) {
            const int cx = x + dx, cy = y + dy;
            if (cx >= W || cy >= W || l || !w.deck[cy][cx]) continue;
            m = std::min(m, std::max(0, w.deck[cy][cx] - zAt(w, cx, cy) - sizeLv(f)));
        }
    return m;
}
inline int dist3(int x0, int y0, int z0, int x1, int y1, int z1) { return 5 * std::max({std::abs(x1 - x0), std::abs(y1 - y0), 2 * std::abs(z1 - z0)}); }
// Клетки по размеру (PHB): до среднего — 1 клетка, большой — 2×2, огромный — 3×3, громадный — 4×4. Позиция бойца (x, y) —
// левая верхняя клетка его квадрата. Расстояние — между ближайшими клетками квадратов, обзор — от любой клетки до любой,
// область задевает, если накрыла хоть одну клетку
inline int fpFull(const Fighter& f) { return sizeLv(f); }
int fpN(const Fighter& f);   // занятый квадрат с учётом протискивания (ниже)
inline int fpGap(int a0, int an, int b0, int bn) { return std::max({0, b0 - (a0 + an - 1), a0 - (b0 + bn - 1)}); }
inline int distFP(int ax, int ay, int az, int an, int bx, int by, int bz, int bn) {
    return 5 * std::max({fpGap(ax, an, bx, bn), fpGap(ay, an, by, bn), 2 * std::abs(az - bz)});
}
inline bool fpOverlap(int ax, int ay, int an, int bx, int by, int bn) { return ax < bx + bn && bx < ax + an && ay < by + bn && by < ay + an; }
inline bool nodeOK1(const World& w, const Fighter& f, int x, int y, int l) {
    if (x < 0 || y < 0 || x >= w.aw || y >= w.ah || w.wall[y][x]) return false;
    if (l) return w.deck[y][x] > 0;
    return (!w.pit[y][x] || f.b.burrowS) && (!w.deck[y][x] || w.deck[y][x] - zAt(w, x, y) >= sizeLv(f));   // копатель — и под ущельем
}
// квадрат n×n в узле (клетка, ярус): весь на арене, без стен и ям, без деревьев (крупный на дерево не лезет), перепад
// внутри квадрата не больше уровня
inline bool nodeOKn(const World& w, const Fighter& f, int x, int y, int l, int n) {
    if (n == 1) return nodeOK1(w, f, x, y, l);
    if (!nodeOK1(w, f, x, y, l)) return false;
    const int z0 = l ? w.deck[y][x] : zAt(w, x, y);
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++) {
            const int cx = x + dx, cy = y + dy;
            if (!nodeOK1(w, f, cx, cy, l) || (!l && w.tree[cy][cx]) || std::abs((l ? w.deck[cy][cx] : zAt(w, cx, cy)) - z0) > 1) return false;
        }
    return true;
}
// клетка стоит 10 фт: лазалка (кроме лазуна и копателя) или труднопроходимая местность (кроме лёгкой поступи, копателя и —
// для воды — пловца)
inline bool roughCell(const World& w, const Fighter& f, int x, int y, int l) {
    if (l || f.b.burrowS) return false;
    if (w.climb[y][x] && !f.b.climbS) return true;
    return w.diff[y][x] && !f.b.ignDiff && !(f.b.swimS && w.water[y][x]);
}
// Протискивание (PHB): крупное существо может втиснуться в место на размер меньше — тогда движение вдвое дороже, его
// атаки и спасброски ЛОВ с помехой, атаки по нему с преимуществом.
// Кэш узлов для крупного (поиск пути проверяет узел несколько раз): квадрат (0 — не встать, n или n−1) и «неровно» (лазалка
// или труднопроходимая местность хоть под одной клеткой). Сбрасывается, когда меняются боец, его размер или карта.
static thread_local unsigned char fitV[2 * W * W];
static thread_local unsigned fitSt[2 * W * W], fitCur = 0;
static thread_local const void* fitOwnerW = nullptr;
static thread_local const void* fitOwnerF = nullptr;
static thread_local unsigned fitVer = 0;
static thread_local int fitSize = -1;
inline unsigned char fitInfo(const World& w, const Fighter& f, int x, int y, int l) {
    const int key = f.b.size | (f.b.ignDiff ? 256 : 0) | (f.b.climbS ? 512 : 0) | (f.b.swimS ? 1024 : 0) | (f.b.burrowS ? 2048 : 0);
    if (fitOwnerW != &w || fitOwnerF != &f || fitVer != w.mapVer || fitSize != key) {
        fitOwnerW = &w;
        fitOwnerF = &f;
        fitVer = w.mapVer;
        fitSize = key;
        if (++fitCur == 0) {
            std::memset(fitSt, 0, sizeof fitSt);
            fitCur = 1;
        }
    }
    if (x < 0 || y < 0 || x >= W || y >= W) return 0;
    const int k = (l ? W * W : 0) + y * W + x;
    if (fitSt[k] == fitCur) return fitV[k];
    const int n = fpFull(f);
    int m = nodeOKn(w, f, x, y, l, n) ? n : (nodeOKn(w, f, x, y, l, n - 1) ? n - 1 : 0);
    bool rough = false;
    for (int dy = 0; dy < m && !rough; dy++)
        for (int dx = 0; dx < m && !rough; dx++)
            rough = roughCell(w, f, x + dx, y + dy, l);
    fitSt[k] = fitCur;
    return fitV[k] = (unsigned char)(m | (rough ? 8 : 0));
}
inline bool nodeOK(const World& w, const Fighter& f, int x, int y, int l) {
    return fpFull(f) == 1 ? nodeOK1(w, f, x, y, l) : (fitInfo(w, f, x, y, l) & 7) != 0;
}
// квадрат f, стоящего в узле: полный или (протиснулся) на размер меньше
inline int fpAt(const World& w, const Fighter& f, int x, int y, int l) {
    if (fpFull(f) == 1) return 1;
    const int m = fitInfo(w, f, x, y, l) & 7;
    return m ? m : fpFull(f);
}
inline bool stepOK(const World& w, const Fighter& f, int x, int y, int l, int nx, int ny, int nl) {
    if (!nodeOK(w, f, nx, ny, nl)) return false;
    const int dz = std::abs(nodeZ(w, nx, ny, nl) - nodeZ(w, x, y, l));
    if (dz <= 1 || w.ladder[y][x] || w.ladder[ny][nx]) return true;
    if (!l && !nl && (f.b.climbS || f.b.burrowS)) return true;   // лазун — по скале, копатель — сквозь землю
    if (!l && !nl && (w.tree[y][x] || w.tree[ny][nx]) && dz <= TREE_UP + 1) return true;
    return f.b.fly && !w.deck[y][x] && !w.deck[ny][nx];
}
inline bool isObject(const World& w, int x, int y) { return x >= 0 && y >= 0 && x < w.aw && y < w.ah && (w.wall[y][x] || w.tree[y][x] || w.climb[y][x] || w.door[y][x]); }
inline int objAC(const World& w, int x, int y) { return w.wall[y][x] ? 17 : ((w.tree[y][x] || w.door[y][x]) ? 15 : 17); }
inline int cellDist(const World* w, int x0, int y0, int x1, int y1) {
    const int dz = w ? 2 * std::abs(zAt(*w, x0, y0) - zAt(*w, x1, y1)) : 0;
    return 5 * std::max({std::abs(x1 - x0), std::abs(y1 - y0), dz});
}
void hazardHit(World& w, Fighter& f);   // опасная зона под бойцом (ниже)
void setDoor(World& w, int x, int y, int st);   // открыть (2) или закрыть (1) дверь (ниже)
// хиты объектам: в начале боя (после генерации или карты игрока)
inline void initObjects(World& w) {
    w.mapVer = ++g_mapSerial;   // новая карта
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) {
            w.ohp[y][x] = (short)(w.wall[y][x] ? (w.strong[y][x] ? OBJ_STRONG_HP : OBJ_WALL_HP)
                                  : (w.tree[y][x] ? OBJ_TREE_HP : (w.door[y][x] ? OBJ_DOOR_HP : (w.climb[y][x] ? OBJ_CLIMB_HP : 0))));
            w.dhp[y][x] = (short)(w.deck[y][x] ? DECK_HP : 0);
        }
}
// мир текущего боя (v21, ауры союзников): правила-ауры союзников проверяются и там, где мира под рукой нет (спасброски,
// резисты); nullptr — вне боя (тесты с ручными бойцами — ауры не действуют)
thread_local const World* g_curW = nullptr;
thread_local bool g_heavyDis = false;
thread_local bool g_countered = false;   // способность погашена контрзаклинанием (v21)   // атака тяжёлым оружием маленького существа (помеха)
void doWeapon(World& w, Fighter& me, int tIdx, int nAtt = -1, int man = -1, bool offhand = false, bool twfMod = false);
inline bool canReact(const Fighter& d);
void fireTrig(World& w, Fighter& me, int on, Fighter* tgt);   // «событие → эффект» (v21), ниже
void readyFire(World& w, int gi, int fi);   // подготовленное действие бойца gi по fi (ниже, после doSpec)
thread_local int g_trigType = -1;      // тип полученного урона для события T_DMGTYPE
thread_local bool g_saveVsAbil = false;   // спасбросок идёт от способности (сопротивление магии)
thread_local int g_curAbil = 0;           // номер способности, которая сейчас действует (невосприимчивость после успеха)
int distFt(const Fighter& a, const Fighter& b);
// правила, действующие на x: свои (k — номер правила) и правила-ауры союзников рядом (k = −1: «раз в ход» у аур нет)
template <class F>
inline void forRules(const Fighter& x, F&& f) {
    for (size_t k = 0; k < x.b.rules.size(); k++) f(x.b.rules[k], (int)k);
    if (!g_curW) return;
    for (auto& y : g_curW->f)
        if (&y != &x && y.hp > 0 && y.team == x.team)
            for (auto& r : y.b.rules)
                if (r.allyR > 0 && distFt(x, y) <= r.allyR) f(r, -1);
}
int fpN(const Fighter& f) {
    if (fpFull(f) == 1 || !g_curW) return fpFull(f);
    const long long key = ((((long long)g_curW->mapVer * W + f.y) * W + f.x) * 2 + (f.up ? 1 : 0)) * 8 + f.b.size;   // размер меняет превращение
    if (f.fpKey != key) {
        f.fpKey = key;
        f.fpVal = fpAt(*g_curW, f, f.x, f.y, f.up ? 1 : 0);
    }
    return f.fpVal;
}
inline bool squeezed(const Fighter& f) { return fpN(f) < fpFull(f); }
int distFt(const Fighter& a, const Fighter& b) {
    if (!g_curW) return distFP(a.x, a.y, 0, fpN(a), b.x, b.y, 0, fpN(b));
    return distFP(a.x, a.y, zF(*g_curW, a), fpN(a), b.x, b.y, zF(*g_curW, b), fpN(b));
}
// от бойца до клетки (земля клетки): от ближайшей клетки его квадрата
inline int distFC(const World& w, const Fighter& f, int x, int y) { return distFP(f.x, f.y, zF(w, f), fpN(f), x, y, zAt(w, x, y), 1); }
// прямая видимость: линия от глаз (уровень клетки + 5 фт) до глаз; закрывают стена (на WALL_UP выше земли), дерево (на
// TREE_UP) и земля клетки, если линия проходит ниже их верха. На ровной арене — как раньше: закрывают стены и деревья
// Настил закрывает обзор сквозь себя: линия, пересекающая его уровень над этой клеткой (в том числе над своей и над клеткой
// цели — стоящий под мостом не видит стоящего на нём), закрыта
bool losZ(const World& w, int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy,
        x = x0, y = y0;
    const double za = z0 + 0.5, zb = z1 + 0.5;
    const int n = std::max(dx, dy);
    auto slab = [&](int cx, int cy, double lo, double hi) { const int d = w.deck[cy][cx]; return d > 0 && std::min(lo, hi) < d && std::max(lo, hi) > d; };
    if (n == 0) return !slab(x0, y0, za, zb);   // одна клетка: стоящий на настиле и стоящий под ним
    if (slab(x0, y0, za, za + (zb - za) * 0.5 / n) || slab(x1, y1, zb - (zb - za) * 0.5 / n, zb)) return false;
    int i = 0;
    while (!(x == x1 && y == y1)) {
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
        i++;
        if (x == x1 && y == y1) break;
        const double zl = za + (zb - za) * i / n;
        const bool shut = w.wall[y][x] || w.door[y][x] == 1;
        const int top = w.height[y][x] + (shut ? WALL_UP : (w.tree[y][x] ? TREE_UP : 0));
        if ((shut || w.tree[y][x] || w.height[y][x] > 0) && top > zl) return false;
        if (slab(x, y, za + (zb - za) * (i - 0.5) / n, za + (zb - za) * (i + 0.5) / n)) return false;
    }
    return true;
}
bool los(const World& w, int x0, int y0, int x1, int y1) { return losZ(w, x0, y0, zAt(w, x0, y0), x1, y1, zAt(w, x1, y1)); }
// видимость между квадратами: есть линия от какой-нибудь клетки одного до какой-нибудь клетки другого (к каждой клетке b —
// от ближайшей к ней клетки a)
bool losFP(const World& w, int ax, int ay, int az, int an, int bx, int by, int bz, int bn) {
    if (an == 1 && bn == 1) return losZ(w, ax, ay, az, bx, by, bz);
    for (int dy = 0; dy < bn; dy++)
        for (int dx = 0; dx < bn; dx++) {
            const int x1 = bx + dx, y1 = by + dy;
            if (losZ(w, std::clamp(x1, ax, ax + an - 1), std::clamp(y1, ay, ay + an - 1), az, x1, y1, bz)) return true;
        }
    return false;
}
// видимость между бойцами (с их ярусом) и от бойца до клетки (земля клетки)
bool losF(const World& w, const Fighter& a, const Fighter& b) { return losFP(w, a.x, a.y, zF(w, a), fpN(a), b.x, b.y, zF(w, b), fpN(b)); }
bool losFC(const World& w, const Fighter& a, int x, int y) { return losFP(w, a.x, a.y, zF(w, a), fpN(a), x, y, zAt(w, x, y), 1); }
bool inShapeF(int shape, int size, int ox, int oy, int cx, int cy, const Fighter& e) {
    if (10 * e.alt > std::max(size, 5)) return false;   // летун выше области (она — от земли на свой размер)
    const int n = fpN(e);
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++)
            if (inShape(shape, size, ox, oy, cx, cy, e.x + dx, e.y + dy)) return true;
    return false;
}
// клетку (x, y) занимает чей-то квадрат (кроме self)
bool occupied(const World& w, int x, int y, const Fighter* self) {
    for (auto& g : w.f)
        if (&g != self && g.hp > 0 && fpOverlap(g.x, g.y, fpN(g), x, y, 1)) return true;
    return false;
}
// Бойцы мешают друг другу, только если их объёмы пересекаются: квадрат и высота — от уровня ног на рост (средний 10 фт,
// большой 20, огромный 30, громадный 40). Под стоящим на мосту, под летуном выше роста — пройти можно (v21)
inline bool vOverlap(int za, const Fighter& a, int zb, const Fighter& b) { return za < zb + sizeLv(b) && zb < za + sizeLv(a); }
// f, поставленный в узел (x, y, l) на своей высоте, задевает g
inline bool touchesAt(const World& w, const Fighter& f, int x, int y, int l, const Fighter& g) {
    return &g != &f && g.hp > 0 && fpOverlap(g.x, g.y, fpN(g), x, y, fpAt(w, f, x, y, l)) &&
           vOverlap(nodeZ(w, x, y, l) + std::min(f.alt, maxAltAt(w, f, x, y, l)), f, zF(w, g), g);
}
// квадрат f, поставленный в (x, y), задевает чужой
bool blockedAt(const World& w, const Fighter& f, int x, int y) {
    for (auto& g : w.f)
        if (touchesAt(w, f, x, y, f.up ? 1 : 0, g)) return true;
    return false;
}
// все клетки квадрата f в (x, y) удовлетворяют ok(cx, cy) (и лежат на арене)
template <class P>
inline bool fpAll(const World& w, const Fighter& f, int x, int y, P&& ok) {
    const int n = fpN(f);
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++) {
            const int cx = x + dx, cy = y + dy;
            if (cx < 0 || cy < 0 || cx >= w.aw || cy >= w.ah || !ok(cx, cy)) return false;
        }
    return true;
}
// чужой квадрат, задетый квадратом f в (x, y) (для удара при столкновении); nullptr — нет
Fighter* blockerAt(World& w, const Fighter& f, int x, int y) {
    for (auto& g : w.f)
        if (touchesAt(w, f, x, y, f.up ? 1 : 0, g)) return &g;
    return nullptr;
}
// не заканчивать ход в занятой клетке
void nudge(const World& w, Fighter& f) {
    if (!blockedAt(w, f, f.x, f.y)) return;
    for (int r = 1; r < 14; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                int nx = f.x + dx, ny = f.y + dy;
                if (!fpAll(w, f, nx, ny, [&](int cx, int cy) { return !w.wall[cy][cx] && w.door[cy][cx] != 1 && !w.pit[cy][cx]; }) || blockedAt(w, f, nx, ny)) continue;
                const bool up = f.up && w.deck[ny][nx] > 0;   // на настиле — по настилу, иначе на землю
                if (!up && !nodeOK(w, f, nx, ny, 0)) continue;
                f.x = nx;
                f.y = ny;
                f.up = up;
                return;
            }
}
// Кто сейчас наносит урон (индекс бойца, −1 — никто: яд, осложнения погони): для реакции «мне нанесли урон» (№23)
thread_local int g_srcIdx = -1;
void oppAttack(World& w, Fighter& g, Fighter& victim);
// Способность-реакция (№23): боец meIdx реакцией отвечает на событие trig от srcIdx; prevDist — расстояние до входа в зону (trig 3)
bool tryReact(World& w, int meIdx, int trig, int srcIdx, int prevDist = -1);
void riposte(World& w, Fighter& d, Fighter& a);
// Запись хода бота примитивами (v21, №4): пока g_rec задан, ход бойца g_recIdx пишется как последовательность примитивов
struct Prim;
thread_local std::vector<Prim>* g_rec = nullptr;
thread_local int g_recIdx = -1;
void recordMove(int idx, int x, int y, int budget);
// цена шага в клетку (x, y) из (fx, fy): лазалка и труднопроходимая местность 10 фт, подъём +5 фт за уровень
static int stepCostL(const World& w, const Fighter& f, int fx, int fy, int fl, int x, int y, int l) {
    int cost = 5;
    if (!f.b.fly) {
        // лазалка или труднопроходимая местность хоть под одной клеткой квадрата
        const bool rough = fpFull(f) == 1 ? roughCell(w, f, x, y, l) : (fitInfo(w, f, x, y, l) & 8) != 0;
        if (rough)
            cost = 10;
        const int up = nodeZ(w, x, y, l) - nodeZ(w, fx, fy, fl);   // подъём (лестница, дерево, на настил): +5 фт за уровень
        if (up > 0 && !f.b.climbS && !f.b.burrowS) cost += 5 * up;   // лазун и копатель — без надбавки
    }
    if (fpFull(f) > 1 && (fitInfo(w, f, x, y, l) & 7) < fpFull(f)) cost *= 2;   // протискивается: движение вдвое дороже
    if (!l && w.door[y][x] == 1) cost += 5;   // закрытую дверь по пути открывают (взаимодействие с предметом)
    return cost;
}
static int stepCost(const World& w, const Fighter& f, int fx, int fy, int x, int y) { return stepCostL(w, f, fx, fy, f.up, x, y, f.up); }
bool isKnown(const World& w, int team, int i);
void revealTo(World& w, int team, int i);
// путь f к клетке (tx, ty) — кратчайший по цене шага (лазалка, трудная местность, подъём), в обход стен, ущелий и
// известных врагов (в саму клетку цели можно, даже если там враг). Тот же поиск строит клетки-кандидаты сети
// (reachable в prim_net.h): клетка, до которой он насчитал N фт, достижима за N фт. Пути в обход врагов нет — путь
// без их учёта (упрётся во врага). Неизвестный враг (туман) не обходится: в него упираются.
struct PStep {   // шаг пути: клетка и ярус (0 — земля, 1 — настил)
    int first, second, l;
};
static thread_local int pdCost[2 * W * W], pdPrev[2 * W * W], pdStamp[2 * W * W], pdFoe[W * W], pdCur = 0;
static void pathTo(const World& w, const Fighter& f, int tx, int ty, std::vector<PStep>& path) {
    path.clear();
    if (f.x == tx && f.y == ty) return;
    const int goal = ty * W + tx, from = f.y * W + f.x + (f.up ? W * W : 0);
    // крупный идёт левой верхней клеткой в цель; если там не встать (стена, край, дерево) — до узла, где его квадрат накрывает цель
    const bool cover = fpFull(f) > 1 && !nodeOK(w, f, tx, ty, 0) && !nodeOK(w, f, tx, ty, 1);
    std::vector<int> foes;   // известные враги, которых путь обходит (первый проход)
    auto foeUnder = [&](int x, int y, int l, int st) {   // f в узле (x, y, l) задевает известного врага (квадрат и высота)
        const int fn = fpAt(w, f, x, y, l);
        bool near = false;   // быстрая проверка по клеткам, потом — точная с высотой
        for (int dy = 0; dy < fn && !near; dy++)
            for (int dx = 0; dx < fn && !near; dx++)
                near = x + dx < W && y + dy < W && pdFoe[(y + dy) * W + x + dx] == st;
        if (!near) return false;
        for (int i : foes)
            if (touchesAt(w, f, x, y, l, w.f[i])) return true;
        return false;
    };
    for (int pass = 0; pass < 2; pass++) {
        const int st = ++pdCur;
        if (pass == 0) {
            const int fi = (int)(&f - &w.f[0]);
            const bool inWorld = fi >= 0 && fi < (int)w.f.size();
            for (size_t i = 0; i < w.f.size(); i++) {
                const Fighter& g = w.f[i];
                // враг, чей квадрат накрывает цель пути, не обходится (к нему и идём — как раньше в саму клетку цели)
                if (g.hp > 0 && g.team != f.team && (!inWorld || isKnown(w, f.team, (int)i)) && !fpOverlap(g.x, g.y, fpN(g), tx, ty, 1) && (foes.push_back((int)i), true))
                    for (int dy = 0; dy < fpN(g); dy++)
                        for (int dx = 0; dx < fpN(g); dx++)
                            if (g.x + dx < W && g.y + dy < W) pdFoe[(g.y + dy) * W + g.x + dx] = st;
            }
        }
        std::vector<std::pair<int, int>> q;   // (цена, узел) — Дейкстра
        int end = -1;
        pdStamp[from] = st;
        pdCost[from] = 0;
        pdPrev[from] = -1;
        q.push_back({0, from});
        bool found = false, skipped = false;   // skipped — какой-то узел не взят из-за врага (иначе второй проход тот же)
        int best = from, bestGap = 1 << 20, bestCost = 0;   // ближе всего к цели (если до неё не дойти)
        while (!q.empty()) {
            std::pop_heap(q.begin(), q.end(), std::greater<>());
            const auto [v, c] = q.back();
            q.pop_back();
            if (v != pdCost[c]) continue;
            const int l = c / (W * W), x = c % W, y = (c % (W * W)) / W;
            if (c % (W * W) == goal || (cover && c != from && fpOverlap(x, y, fpAt(w, f, x, y, l), tx, ty, 1))) {   // крупный — квадрат накрыл цель
                found = true;
                end = c;
                break;
            }
            {
                const int m = fpFull(f) > 1 ? fpAt(w, f, x, y, l) : 1, gp = std::max(fpGap(x, m, tx, 1), fpGap(y, m, ty, 1));
                if (gp < bestGap || (gp == bestGap && v < bestCost)) best = c, bestGap = gp, bestCost = v;
            }
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    for (int nl = 0; nl < 2; nl++) {
                        const int nx = x + dx, ny = y + dy, cell = ny * W + nx, n = cell + nl * W * W;
                        if ((!dx && !dy) || !stepOK(w, f, x, y, l, nx, ny, nl)) continue;
                        if (pass == 0 && cell != goal && foeUnder(nx, ny, nl, st)) {
                            skipped = true;
                            continue;
                        }
                        const int haz = !nl && w.haz[ny][nx] && !(f.b.fly && HAZ[w.haz[ny][nx]].flySafe) ? 1 : 0;
                        const int nv = v + 64 * stepCostL(w, f, x, y, l, nx, ny, nl) + haz;   // футы × 64 + клетки зон: сначала короче, потом безопаснее
                        if (pdStamp[n] != st || nv < pdCost[n]) {
                            pdStamp[n] = st;
                            pdCost[n] = nv;
                            pdPrev[n] = c;
                            q.push_back({nv, n});
                            std::push_heap(q.begin(), q.end(), std::greater<>());
                        }
                    }
        }
        if (!found && pass == 0 && skipped) continue;   // враги мешали — второй проход без них
        if (!found) end = best;   // до цели не дойти: как можно ближе к ней (PHB — идёт, сколько может)
        if (end == from) return;
        for (int c = end; c != from; c = pdPrev[c]) path.push_back({c % W, (c % (W * W)) / W, c / (W * W)});
        std::reverse(path.begin(), path.end());
        return;
    }
}
static bool enemyAt(const World& w, const Fighter& f, int x, int y) {
    for (auto& g : w.f)
        if (g.team != f.team && touchesAt(w, f, x, y, f.up ? 1 : 0, g)) return true;
    return false;
}
// Куда дойдёт f, если идёт к t, пока не окажется в desiredFt от него (и, если needLos, не увидит его), не дальше speed фт:
// без провоцированных атак и реакций (их учитывает moveToCell). enterTarget — можно встать в саму клетку цели.
std::pair<int, int> planStop(const World& w, const Fighter& f, const Fighter& t, int desiredFt, int speed, bool needLos, bool enterTarget) {
    int px = f.x, py = f.y, pl = f.up;
    const int tz = zF(w, t);
    const int fn = fpN(f), tn = fpN(t);
    auto inPos = [&]() { return distFP(px, py, nodeZ(w, px, py, pl) + f.alt, fn, t.x, t.y, tz, tn) <= desiredFt && (!needLos || losFP(w, px, py, nodeZ(w, px, py, pl) + f.alt, fn, t.x, t.y, tz, tn)); };
    if (inPos() || speed <= 0) return {px, py};
    std::vector<PStep> path;
    pathTo(w, f, t.x, t.y, path);
    int left = speed;
    for (auto& c : path) {
        if (c.first == t.x && c.second == t.y && !enterTarget) break;
        const int cost = stepCostL(w, f, px, py, pl, c.first, c.second, c.l);
        if (left < cost || enemyAt(w, f, c.first, c.second)) break;
        px = c.first;
        py = c.second;
        pl = c.l;
        left -= cost;
        if (inPos()) break;
    }
    return {px, py};
}
// Примитив «иди в клетку» (v21, №4): f идёт к (cx, cy) не дальше speed фт. Каждый шаг, выводящий f из досягаемости
// ближнего врага (без Отхода, реакция не потрачена), даёт ему провоцированную атаку ДО шага (как в PHB); вход в зону
// врага может вызвать его реакцию. Возвращает потраченные футы.
// расстояние до ближайшего известного врага (для метрики скорости)
static int nearestKnownFoe(const World& w, const Fighter& f) {
    int d = 1 << 20;
    for (size_t i = 0; i < w.f.size(); i++)
        if (w.f[i].hp > 0 && w.f[i].team != f.team && isKnown(w, f.team, (int)i)) d = std::min(d, distFt(f, w.f[i]));
    return d;
}
int moveToCell(World& w, Fighter& f, int cx, int cy, int speed) {
    const int fi = (int)(&f - &w.f[0]);
    recordMove(fi, cx, cy, speed);
    if (speed <= 0 || (f.x == cx && f.y == cy)) return 0;
    std::vector<PStep> path;
    pathTo(w, f, cx, cy, path);
    int left = speed;
    for (auto& c : path) {
        const int cost = stepCostL(w, f, f.x, f.y, f.up, c.first, c.second, c.l);
        if (left < cost) break;
        if (enemyAt(w, f, c.first, c.second)) {
            // упёрся в невидимого (неизвестного) врага: он становится известен команде, попытка стоит 5 фт
            for (size_t i = 0; i < w.f.size(); i++) {
                const Fighter& g = w.f[i];
                if (g.team != f.team && touchesAt(w, f, c.first, c.second, c.l, g) && !isKnown(w, f.team, (int)i)) {
                    revealTo(w, f.team, (int)i);
                    left -= 5;
                    break;
                }
            }
            break;
        }
        if (!f.disengaged) {
            for (size_t i = 0; i < w.f.size(); i++) {
                Fighter& g = w.f[i];
                // провоцированная атака — ближняя атака оружием: ближнее или метательное, безоружный и лучник не атакуют
                if (g.hp <= 0 || g.team == f.team || g.reactionUsed || !meleeWeapon(g.b)) continue;
                // участники погони (главные боссы) одновременно бегут в одну сторону: провоцированных атак друг по другу нет (DMG)
                if (w.chaseOn && fi < 2 && (int)i < 2) continue;
                // недееспособный не реагирует
                if (g.cond[C_STUN] || g.cond[C_PARAL] || g.cond[C_INCAP]) continue;
                // не атакует очаровавшего
                if (g.cond[C_CHARM] > 0 && g.charmSrc == fi) continue;
                if (g.noOAvs == fi) continue;   // Подвижный (v21): я ударил g в ближнем бою — он не провоцирует от меня
                bool wasAdj = distFt(g, f) <= meleeReach(g.b);
                bool nowAdj = distFP(g.x, g.y, zF(w, g), fpN(g), c.first, c.second, nodeZ(w, c.first, c.second, c.l) + std::min(f.alt, maxAltAt(w, f, c.first, c.second, c.l)), fpN(f)) <= meleeReach(g.b);
                if (wasAdj && !nowAdj) {
                    spendReaction(g);
                    const int src0 = g_srcIdx;
                    g_srcIdx = (int)i;   // урон провоцированной атаки наносит g
                    oppAttack(w, g, f);
                    g_srcIdx = src0;
                    if (f.hp <= 0) return speed - left;
                }
            }
        }
        const int fx0 = f.x, fy0 = f.y, fz0 = zF(w, f);
        const bool useMe = fi >= 0 && fi < 2;
        const int dBefore = useMe ? nearestKnownFoe(w, f) : 0;
        if (!c.l && w.door[c.second][c.first] == 1) setDoor(w, c.first, c.second, 2);   // открыл дверь по пути
        f.x = c.first;
        f.y = c.second;
        f.up = c.l;
        f.alt = std::min(f.alt, maxAltAt(w, f, f.x, f.y, f.up));   // под настилом — ниже (бесплатно)
        left -= cost;
        hazardHit(w, f);
        if (f.hp <= 0) return speed - left;
        if (useMe) {   // метрика использования скорости (№6): шаг, который не приблизил к ближайшему известному врагу
            g_use[fi][U_FT] += cost;
            if (nearestKnownFoe(w, f) >= dBefore) g_use[fi][U_FTAWAY] += cost;
        }
        // Подготовка (v21): враг вошёл в досягаемость оружия того, кто подготовился, — удар реакцией
        for (size_t i = 0; i < w.f.size(); i++) {
            Fighter& g = w.f[i];
            if (!g.readied || g.readyTrig != 1 || g.hp <= 0 || g.team == f.team || !canReact(g)) continue;
            const bool ab = g.readyAbil >= 0 && g.readyAbil < (int)g.b.abil.size();
            if (!ab && (g.b.wN <= 0 || g.disarmed)) continue;
            const int reach = ab ? std::max(5, specReach(g.b, g.b.abil[g.readyAbil])) : atkDist(g.b), before = distFP(g.x, g.y, zF(w, g), fpN(g), fx0, fy0, fz0, fpN(f));
            if (before <= reach || distFt(g, f) > reach || !losF(w, g, f)) continue;
            spendReaction(g);
            g.readied = false;
            evPush("{\"k\":\"ready\",\"who\":" + std::to_string(i) + ",\"tgt\":" + std::to_string(fi) + "}");
            readyFire(w, (int)i, fi);
            if (f.hp <= 0) return speed - left;
        }
        // реакция врага «вошёл в зону» (№23)
        for (size_t i = 0; i < w.f.size(); i++) {
            const Fighter& g = w.f[i];
            if (g.hp <= 0 || g.team == f.team || !hasReact(g.b)) continue;
            tryReact(w, (int)i, 3, fi, distFP(g.x, g.y, zF(w, g), fpN(g), fx0, fy0, fz0, fpN(f)));
            if (f.hp <= 0) return speed - left;
        }
        if (f.x != c.first || f.y != c.second) break;   // реакция сдвинула (толчок): дальше по старому пути не идём
        if (f.x == cx && f.y == cy) break;
    }
    nudge(w, f);
    return speed - left;
}
// Смена высоты летуна (v21): подъём из досягаемости ближнего врага провоцирует его атаку (как шаг прочь), Отход — нет
void setAlt(World& w, Fighter& f, int a) {
    const int fi = (int)(&f - &w.f[0]);
    if (!f.disengaged)
        for (size_t i = 0; i < w.f.size() && f.hp > 0; i++) {
            Fighter& g = w.f[i];
            if (g.hp <= 0 || g.team == f.team || g.reactionUsed || !meleeWeapon(g.b)) continue;
            if (g.cond[C_STUN] || g.cond[C_PARAL] || g.cond[C_INCAP] || (g.cond[C_CHARM] > 0 && g.charmSrc == fi) || g.noOAvs == fi) continue;
            const bool wasAdj = distFt(g, f) <= meleeReach(g.b);
            const int a0 = f.alt;
            f.alt = a;
            const bool nowAdj = distFt(g, f) <= meleeReach(g.b);
            f.alt = a0;
            if (wasAdj && !nowAdj) {
                spendReaction(g);
                const int src0 = g_srcIdx;
                g_srcIdx = (int)i;
                oppAttack(w, g, f);
                g_srcIdx = src0;
            }
        }
    if (f.hp <= 0 || !canStayAloft(f)) return;   // упал от провоцированной атаки (сбит с ног) — высоты больше нет
    f.alt = a;
    hazardHit(w, f);
}
// Двигает f к t (до desiredFt от него), возвращает потраченные футы: решает, в какую клетку идти, и идёт туда (moveToCell)
int moveToward(World& w, Fighter& f, const Fighter& t, int desiredFt, int speed, bool needLos, bool enterTarget = false) {   // enterTarget: дойти до самой клетки цели (для игрока), иначе встать рядом
    if (speed <= 0) return 0;
    const auto c = planStop(w, f, t, desiredFt, speed, needLos, enterTarget);
    if (c.first == f.x && c.second == f.y) return 0;
    return moveToCell(w, f, c.first, c.second, speed);
}
