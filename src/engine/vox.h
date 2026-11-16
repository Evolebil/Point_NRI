// Объёмный мир (этап 1б, docs/TZ_3d.md): кубы 5×5×5 фт, чанки 16×16×16, материал куба — один байт.
// Часть движка (порядок частей — engine.h): отдельно не подключается.
//
// Хиты и КД — у материала (таблица MAT): целый куб нигде отдельно не хранится. Повреждённый куб попадает в разреженный
// список повреждений «номер куба → хиты осталось» (по замыслу владельца: блок «выделяется», только когда хиты падают);
// разрушенный становится воздухом и уходит из списка. Новые материалы на повреждение не заводятся — байт материала на
// 256 видов, а копии «надбитый камень» быстро бы его съели. Изменения куба копятся в changes для кадра боя
// ({"k":"vox","x","y","z","m"}) и поднимают версию чанка — клиент перестраивает только его.
// Координаты: x, y — клетка карты (как у World), z — куб по высоте снизу; уровень высоты карты (5 фт) = 1 куб.
#pragma once
#include <unordered_map>

enum VoxMat : unsigned char {
    VX_AIR, VX_BEDROCK, VX_DIRT, VX_GRASS, VX_SAND, VX_STONE, VX_STRONG, VX_WOOD, VX_LEAF, VX_WATER, VX_PLANK, VX_DOOR,
    VX_COVER, VX_RUBBLE, VX_LAVA, VX_ACID, VX_ICE, VX_FORCE, VX_N
};
// hp 0 — неразрушим (дно мира, жидкости, барьер способности); solid — опора и преграда движению; seeThru — обзор сквозь
// (листва и вода — пока насквозь: частичное укрытие листвой — этап 2); liquid — плавание (этап 2). Ствол дерева — тонкий, в углу
// клетки (решение владельца 2026-11-16): не твёрдый — мимо него проходят, обзор и области не держит, даёт полукрытие (vxCover),
// хиты есть — рубится; по нему лезут (vxByTree)
struct MatInfo { const char* name; short hp; unsigned char ac; bool solid, seeThru, liquid; };
// КД и хиты — как у объектов сейчас (DMG «Объекты»: дерево КД 15, камень КД 17): кубик стены — OBJ_WALL_HP, крепкой —
// OBJ_STRONG_HP, ствола — OBJ_TREE_HP, двери — OBJ_DOOR_HP, настила — DECK_HP; земля и песок — мягче камня
constexpr MatInfo MAT[VX_N] = {
    {"воздух", 0, 0, false, true, false},      {"дно мира", 0, 0, true, false, false},
    {"земля", 27, 10, true, false, false},       {"трава", 27, 10, true, false, false},
    {"песок", 18, 8, true, false, false},        {"камень", OBJ_WALL_HP, 17, true, false, false},
    {"крепкий камень", OBJ_STRONG_HP, 19, true, false, false},
    {"дерево", OBJ_TREE_HP, 15, false, true, false}, {"листва", 5, 10, true, true, false},
    {"вода", 0, 0, false, true, true},           {"доски", DECK_HP, 15, true, false, false},
    {"дверь", OBJ_DOOR_HP, 15, true, false, false}, {"укрытие", OBJ_CLIMB_HP, 17, true, true, false},
    {"обломки", 9, 10, true, false, false},      {"лава", 0, 0, false, true, true},
    {"кислота", 0, 0, false, true, true},        {"лёд", 18, 13, true, true, false},
    {"силовой барьер", 0, 0, true, true, false}};
constexpr int VCH = 16;     // сторона чанка
thread_local bool g_voxMode = false;   // бой в режиме кубов (игра по ?3d=1): мир из кубов строится в начале боя
constexpr int VH_MAX = 64;  // высота мира в кубах (320 фт, решение ТЗ 3.1)

struct VoxWorld {
    int sx = 0, sy = 0, sz = 0;        // размер в кубах: x, y — как арена, z — высота
    int cx = 0, cy = 0, cz = 0;        // чанков по осям
    std::vector<unsigned char> mat;    // материал: чанк за чанком, внутри чанка — z, y, x
    std::vector<unsigned> chunkVer;    // версия чанка: растёт при каждом изменении куба в нём
    std::unordered_map<int, short> dmg;   // повреждённые кубы: номер куба (idx) → хиты осталось
    std::vector<std::array<int, 4>> changes;   // изменения для кадра: x, y, z, новый материал
    std::unordered_map<int, unsigned char> edits;   // кубы, изменённые в бою (копание, разрушение): после пересборки столбца — снова
    int idx(int x, int y, int z) const {
        const int c = ((z / VCH) * cy + y / VCH) * cx + x / VCH;
        return c * VCH * VCH * VCH + ((z % VCH) * VCH + y % VCH) * VCH + x % VCH;
    }
    bool in(int x, int y, int z) const { return x >= 0 && y >= 0 && z >= 0 && x < sx && y < sy && z < sz; }
    // за пределами — воздух сверху и по краям, ниже нуля — дно мира
    unsigned char at(int x, int y, int z) const { return z < 0 ? VX_BEDROCK : (in(x, y, z) ? mat[(size_t)idx(x, y, z)] : VX_AIR); }
    void init(int w, int h, int z) {
        sx = w; sy = h; sz = std::min(z, VH_MAX);
        cx = (sx + VCH - 1) / VCH; cy = (sy + VCH - 1) / VCH; cz = (sz + VCH - 1) / VCH;
        mat.assign((size_t)cx * cy * cz * VCH * VCH * VCH, VX_AIR);
        chunkVer.assign((size_t)cx * cy * cz, 0);
        dmg.clear();
        changes.clear();
        edits.clear();
    }
    // поставить материал (стройка, конвертер, разрушение): старые повреждения куба сбрасываются
    void set(int x, int y, int z, unsigned char m, bool log = false) {
        if (!in(x, y, z)) return;
        const int i = idx(x, y, z);
        if (mat[(size_t)i] == m) return;
        mat[(size_t)i] = m;
        dmg.erase(i);
        chunkVer[(size_t)(i / (VCH * VCH * VCH))]++;
        if (log) {   // изменение боя: в кадр (событие vox) и в правки, переживающие пересборку столбца
            changes.push_back({x, y, z, m});
            edits[i] = m;
            evPush("{\"k\":\"vox\",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + ",\"z\":" + std::to_string(z) + ",\"m\":" + std::to_string(m) + "}");
        }
    }
    int hpAt(int x, int y, int z) const {
        if (!in(x, y, z)) return 0;
        const int i = idx(x, y, z);
        const auto it = dmg.find(i);
        return it != dmg.end() ? it->second : MAT[mat[(size_t)i]].hp;
    }
    int acAt(int x, int y, int z) const { return MAT[at(x, y, z)].ac; }
};

// Урон кубу (попадание уже решено бросить против acAt). Яд и психический — не действуют (DMG), неразрушимое (hp 0) — тоже.
// Возвращает: 0 — не задело, 1 — повреждён, 2 — разрушен (стал воздухом, событие в changes)
inline int damageVox(VoxWorld& v, int x, int y, int z, int amount, int dtype) {
    if (!v.in(x, y, z) || amount <= 0 || dtype == DT_POISON || dtype == DT_PSYCHIC) return 0;
    const unsigned char m = v.at(x, y, z);
    if (MAT[m].hp <= 0) return 0;
    const int left = v.hpAt(x, y, z) - amount;
    if (left <= 0) {
        v.set(x, y, z, VX_AIR, true);
        return 2;
    }
    v.dmg[v.idx(x, y, z)] = (short)left;
    return 1;
}

// Перевод нынешней карты (World, уровни по 5 фт) в кубы — те же правила, что у клиента (web/js/55_render3d.js r3Column):
// земля до VZ0 + height (верхний куб — трава, песок у воды, опасность — её вещество), ущелье — только дно, стена — 6 кубов
// (30 фт), дерево — ствол и 2 куба листвы (у куста 1 и 1), дверь — 2 куба и перемычка до 30 фт, настил — доски на уровне
// своего верха, укрытие — куб «укрытие»; изменения арены (dyn): временная стена, обломки, двери, рухнувший настил
// изменения арены в клетке (dyn): бит вида; двери — последнее состояние
inline int voxDynBits(const World& w, int x, int y) {
    int b = 0;
    for (const auto& d : w.dyn)
        if (d[0] == x && d[1] == y) {
            if (d[2] >= 6 && d[2] <= 8) b &= ~((1 << 6) | (1 << 7) | (1 << 8));
            b |= 1 << d[2];
        }
    return b;
}
// столбец (x, y) по слоям карты и изменениям арены b
inline void voxColumn(VoxWorld& v, const World& w, int x, int y, int b) {
    const int g = VZ0 + w.height[y][x];
    auto put = [&](int z0, int z1, unsigned char m) { for (int z = std::max(0, z0); z < std::min(v.sz, z1); z++) v.set(x, y, z, m); };
    put(0, v.sz, VX_AIR);
    v.set(x, y, 0, VX_BEDROCK);
    if (!w.pit[y][x]) {
        unsigned char surf = w.theme ? VX_STONE : VX_GRASS;
        if (w.water[y][x]) surf = VX_WATER;
        else if (w.haz[y][x] == 1) surf = VX_LAVA;
        else if (w.haz[y][x] == 2) surf = VX_ACID;
        else if (w.haz[y][x] == 4) surf = VX_ICE;
        if (b & (1 << 4)) surf = VX_RUBBLE;
        put(1, g - 1, VX_DIRT);
        put(g - 1, g, surf);
    }
    const bool broken = b & (1 << 4);
    if (!broken && w.wall[y][x]) put(g, g + WALL_UP, w.strong[y][x] ? VX_STRONG : VX_STONE);
    if (!broken && w.tree[y][x]) {
        const int wd = std::max(1, (int)w.tree[y][x] - 2);
        put(g, g + wd, VX_WOOD);
        put(g + wd, g + w.tree[y][x], VX_LEAF);
    }
    if (!broken && w.climb[y][x]) put(g, g + 1, VX_COVER);
    if (w.door[y][x] || (b & (1 << 8))) {
        const int st = (b & (1 << 6)) ? 6 : (b & (1 << 7)) ? 7 : (b & (1 << 8)) ? 8 : 0;
        const bool open = st ? st == 6 : w.door[y][x] == 2;
        put(g + 2, g + WALL_UP, VX_STONE);
        if (!open && st != 8) put(g, g + 2, VX_DOOR);
    }
    if (w.deck[y][x] && !(b & (1 << 5)) && VZ0 + w.deck[y][x] - 1 >= g) put(VZ0 + w.deck[y][x] - 1, VZ0 + w.deck[y][x], VX_PLANK);
    if (b & (1 << 1)) put(g, g + 4, VX_FORCE);
}
// Перевод нынешней карты (World, уровни по 5 фт) в кубы — те же правила, что у клиента (web/js/55_render3d.js r3Column):
// земля до VZ0 + height (верхний куб — трава, у города камень, опасность — её вещество), ущелье — только дно, стена — 6 кубов
// (30 фт), дерево — ствол и 2 куба листвы (у куста 1 и 1), дверь — 2 куба и перемычка до 30 фт, настил — доски на уровне
// своего верха, укрытие — куб «укрытие»; изменения арены (dyn): временная стена, обломки, двери, рухнувший настил
inline void voxFromWorld(VoxWorld& v, const World& w) {
    int top = 0;
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) top = std::max({top, (int)w.height[y][x] + std::max(WALL_UP, (int)w.tree[y][x]), (int)w.deck[y][x]});
    v.init(w.aw, w.ah, VZ0 + top + 6);   // запас над самым высоким: летуну есть куда подняться
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) voxColumn(v, w, x, y, voxDynBits(w, x, y));
    v.changes.clear();
}
// Столбец заново по слоям и изменениям арены (дверь, временная стена, обломки, рухнувший настил), затем — правки боя
// (вырытое и разрушенное) поверх: то же делает клиент, поэтому картинка и правила совпадают
inline void vxResync(World& w, int x, int y) {
    if (!w.vx || x < 0 || y < 0 || x >= w.aw || y >= w.ah) return;
    VoxWorld& v = *w.vx;
    std::vector<std::pair<int, unsigned char>> keep;
    for (int z = 0; z < v.sz; z++) {
        const auto it = v.edits.find(v.idx(x, y, z));
        if (it != v.edits.end()) keep.push_back({z, it->second});
    }
    voxColumn(v, w, x, y, voxDynBits(w, x, y));
    for (auto& [z, m] : keep) {
        v.set(x, y, z, m);
        v.edits[v.idx(x, y, z)] = m;
    }
}

// ---- правила режима кубов (этап 1б) ----
// копатель идёт сквозь землю, песок, обломки и листву (камень — только разбить действием)
inline bool vxTreeMat(unsigned char m) { return m == VX_WOOD || m == VX_LEAF; }   // дерево: за него держится любой (лазание)
inline bool vxDiggable(unsigned char m) { return m == VX_DIRT || m == VX_GRASS || m == VX_SAND || m == VX_RUBBLE || m == VX_LEAF; }
// верхняя опора клетки: куб над самым верхним твёрдым (стоять на стене, на кроне, на крыше)
inline int vxTopZ(const World& w, int x, int y) {
    const VoxWorld& v = *w.vx;
    for (int z = v.sz - 1; z >= 1; z--)
        if (MAT[v.at(x, y, z - 1)].solid && !MAT[v.at(x, y, z)].solid) return z;
    return 1;
}
// Тело f — m×m×m кубов от куба ног z клетки (x, y) (рост — vxEyeH в world.h: до среднего 1 куб, большой 2 …): все кубы на
// арене и в воздухе или жидкости (копателю — и в копаемом; dug — тело в земле: копатель держится за неё)
inline bool vxBodyIn(const World& w, const Fighter& f, int x, int y, int z, int m, bool* dug = nullptr) {
    const VoxWorld& v = *w.vx;
    if (x < 0 || y < 0 || x + m > w.aw || y + m > w.ah || z < 1 || z + m > v.sz) return false;
    for (int dy = 0; dy < m; dy++)
        for (int dx = 0; dx < m; dx++)
            for (int k = 0; k < m; k++) {
                const unsigned char c = v.at(x + dx, y + dy, z + k);
                if (!MAT[c].solid) continue;
                if (!(f.b.burrowS && vxDiggable(c))) return false;
                if (dug) *dug = true;
            }
    return true;
}
// какое тело помещается в узел: полное n×n×n, на размер меньше (протиснулся, PHB: только крупные — до среднего тело и так
// 1 куб) или никакое (0)
inline int vxFitN(const World& w, const Fighter& f, int x, int y, int z) {
    const int n = fpFull(f);
    if (vxBodyIn(w, f, x, y, z, n)) return n;
    return n > 1 && vxBodyIn(w, f, x, y, z, n - 1) ? n - 1 : 0;
}
// Можно ли f стоять ногами в кубе z клетки (x, y): тело (полное или протиснувшееся) свободно, опора — твёрдое или жидкость
// под ногами хоть одной клетки, лестница (до верха соседей), стена рядом (лазун), земля вокруг (копатель); летуну опора не нужна
// fly — держится ли в воздухе (vxFixAll проверяет опору и у летуна, сбитого с полёта)
inline bool vxStandEx(const World& w, const Fighter& f, int x, int y, int z, bool fly) {
    const VoxWorld& v = *w.vx;
    const int n = vxFitN(w, f, x, y, z);
    bool sup = fly;
    if (!n || !vxBodyIn(w, f, x, y, z, n, &sup)) return false;
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++) {
            const int cx = x + dx, cy = y + dy;
            const unsigned char below = v.at(cx, cy, z - 1);
            if (MAT[below].solid || MAT[below].liquid) sup = true;
            if (!sup && w.ladder[cy][cx]) {   // лестница: от земли до верха соседних клеток
                int top = 0;
                for (int k = 0; k < 8; k++) {
                    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
                    const int nx = cx + DX[k], ny = cy + DY[k];
                    if (nx >= 0 && ny >= 0 && nx < w.aw && ny < w.ah) top = std::max(top, vxTopZ(w, nx, ny));
                }
                sup = z <= top;
            }
            if (!sup)   // лазун держится за любую стену; любой — за дерево (ствол в своей клетке, крона рядом: лезут, как в 2D)
                for (int k = 0; k < 5 && !sup; k++) {
                    static const int DX[5] = {0, 1, -1, 0, 0}, DY[5] = {0, 0, 0, 1, -1};
                    const unsigned char a = v.at(cx + DX[k], cy + DY[k], z), b = v.at(cx + DX[k], cy + DY[k], z + 1);
                    sup = vxTreeMat(a) || vxTreeMat(b) || (f.b.climbS && (MAT[a].solid || MAT[b].solid));
                }
        }
    return sup;
}
inline bool vxStand(const World& w, const Fighter& f, int x, int y, int z) { return vxStandEx(w, f, x, y, z, f.b.fly); }
// тело f в кубе z свободно (воздух или жидкость; копателю — и копаемое; крупному — хотя бы протиснувшись): туда можно упасть
inline bool vxBodyFree(const World& w, const Fighter& f, int x, int y, int z) { return vxFitN(w, f, x, y, z) > 0; }
// у клетки (x, y) на высоте z — дерево (ствол или крона) в ней или с какой-нибудь стороны: по нему лезут вверх и вниз
inline bool vxByTree(const World& w, int x, int y, int z) {
    static const int DX[5] = {0, 1, -1, 0, 0}, DY[5] = {0, 0, 0, 1, -1};   // и ствол в своей клетке
    for (int k = 0; k < 5; k++)
        if (vxTreeMat(w.vx->at(x + DX[k], y + DY[k], z)) || vxTreeMat(w.vx->at(x + DX[k], y + DY[k], z - 1))) return true;
    return false;
}
// Шаг между узлами: в соседнюю клетку — уступ до 10 фт вверх или вниз (больше — обрыв, как прежде); по вертикали на куб —
// летун, лазун у стены, лестница, копатель. Наискосок — не сквозь угол стены (кроме копателя)
inline bool vxStep(const World& w, const Fighter& f, int x, int y, int z, int nx, int ny, int nz) {
    const int dx = nx - x, dy = ny - y, dz = nz - z;
    if (std::abs(dx) > 1 || std::abs(dy) > 1 || (!dx && !dy && !dz) || !vxStand(w, f, nx, ny, nz)) return false;
    if (!dx && !dy) return std::abs(dz) == 1 && (f.b.fly || f.b.climbS || f.b.burrowS || w.ladder[y][x] || vxByTree(w, x, y, std::max(z, nz)));
    if (std::abs(dz) > (f.b.fly ? 1 : 2)) return false;
    if (dx && dy && !f.b.burrowS) {
        const VoxWorld& v = *w.vx;
        const int zz = std::max(z, nz);
        if (MAT[v.at(x + dx, y, zz)].solid || MAT[v.at(x, y + dy, zz)].solid) return false;
    }
    return true;
}
// цена шага (футы): 5; труднопроходимо (грязь, вода — у земли клетки) — 10; подъём — +5 фт за каждые 10 фт (лазун, летун,
// копатель — без надбавки); вертикальный шаг лестницей — как подъём
inline int vxStepCost(const World& w, const Fighter& f, int x, int y, int z, int nx, int ny, int nz) {
    int cost = 5;
    if (!f.b.fly && !f.b.burrowS && w.diff[ny][nx] && !f.b.ignDiff && !(f.b.swimS && w.water[ny][nx]) && nz == vxTopZ(w, nx, ny)) cost = 10;
    const int up = nz - z;
    if (up > 0 && !f.b.fly && !f.b.climbS && !f.b.burrowS) cost += climbFt(up);
    if (fpFull(f) > 1 && vxFitN(w, f, nx, ny, nz) < fpFull(f)) cost *= 2;   // протискивается: движение вдвое дороже
    return cost;
}
// Путь в клетку (tx, ty) — Дейкстра по узлам-кубам (цена — футы, при равенстве — меньше клеток зон); известных врагов
// обходит (кроме того, кто стоит в цели). Цель — любой узел клетки, а если игрок щёлкнул по высоте (g_vxTz) — этот куб;
// не дойти — как можно ближе к цели (как прежний pathTo)
// Дейкстра по узлам-кубам от f (общая для пути и дерева путей): цена — 64 × футы + клетки зон; известных врагов обходит (кроме
// того, кто стоит в (tx, ty); tx < 0 — никого не исключать). visit(c, цена, x, y, z) — при извлечении узла, true — стоп.
// Массивы цены и предков — VXD (живут до следующего поиска)
struct VxDijk { std::vector<int> cost, prev, stamp; int cur = 0, SX = 0, SY = 0; };
static thread_local VxDijk VXD;
template <class Visit>
void vxDijkstra(const World& w, const Fighter& f, int tx, int ty, Visit&& visit) {
    const VoxWorld& v = *w.vx;
    const int SX = v.sx, SY = v.sy, SZ = v.sz, N = SX * SY * SZ;
    if (f.x < 0 || f.y < 0 || f.x >= SX || f.y >= SY || f.z < 0 || f.z >= SZ) return;
    auto& cost = VXD.cost; auto& prev = VXD.prev; auto& stamp = VXD.stamp; auto& cur = VXD.cur;
    VXD.SX = SX; VXD.SY = SY;
    if ((int)cost.size() < N) {
        cost.assign((size_t)N, 0);
        prev.assign((size_t)N, -1);
        stamp.assign((size_t)N, 0);
    }
    if (++cur == 0) {
        std::fill(stamp.begin(), stamp.end(), 0);
        cur = 1;
    }
    auto id = [&](int x, int y, int z) { return (z * SY + y) * SX + x; };
    const int fi = (int)(&f - &w.f[0]);
    const bool inWorld = fi >= 0 && fi < (int)w.f.size();
    std::vector<int> foes;
    for (size_t i = 0; i < w.f.size(); i++) {
        const Fighter& g = w.f[i];
        if (g.hp > 0 && g.team != f.team && (!inWorld || isKnown(w, f.team, (int)i)) && !(tx >= 0 && fpOverlap(g.x, g.y, fpN(g), tx, ty, 1))) foes.push_back((int)i);
    }
    const int from = id(f.x, f.y, f.z);
    std::vector<std::pair<int, int>> q;
    stamp[(size_t)from] = cur;
    cost[(size_t)from] = 0;
    prev[(size_t)from] = -1;
    q.push_back({0, from});
    while (!q.empty()) {
        std::pop_heap(q.begin(), q.end(), std::greater<>());
        const auto [c0, c] = q.back();
        q.pop_back();
        if (c0 != cost[(size_t)c]) continue;
        const int x = c % SX, y = (c / SX) % SY, z = c / (SX * SY);
        if (visit(c, c0, x, y, z)) return;
        for (int dz = -2; dz <= 2; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    const int nx = x + dx, ny = y + dy, nz = z + dz;
                    if (nx < 0 || ny < 0 || nz < 0 || nx >= SX || ny >= SY || nz >= SZ || !vxStep(w, f, x, y, z, nx, ny, nz)) continue;
                    bool foe = false;
                    for (int i : foes) foe = foe || touchesAt(w, f, nx, ny, nz, w.f[i]);
                    if (foe) continue;
                    const int n = id(nx, ny, nz), haz = w.haz[ny][nx] && nz == vxTopZ(w, nx, ny) && !(f.b.fly && HAZ[w.haz[ny][nx]].flySafe) ? 1 : 0;
                    const int nv = c0 + 64 * vxStepCost(w, f, x, y, z, nx, ny, nz) + haz;
                    if (stamp[(size_t)n] != cur || nv < cost[(size_t)n]) {
                        stamp[(size_t)n] = cur;
                        cost[(size_t)n] = nv;
                        prev[(size_t)n] = c;
                        q.push_back({nv, n});
                        std::push_heap(q.begin(), q.end(), std::greater<>());
                    }
                }
    }
}
void vxPathTo(const World& w, const Fighter& f, int tx, int ty, std::vector<PStep>& path) {
    const VoxWorld& v = *w.vx;
    const int SX = v.sx, SY = v.sy;
    if (f.x < 0 || f.y < 0 || f.x >= SX || f.y >= SY || f.z < 0 || f.z >= v.sz) return;
    const int from = (f.z * SY + f.y) * SX + f.x;
    int end = -1, best = from, bestGap = 1 << 20, bestCost = 0;
    vxDijkstra(w, f, tx, ty, [&](int c, int c0, int x, int y, int z) {
        if (x == tx && y == ty && c != from && (g_vxTz < 0 || z == g_vxTz)) {
            end = c;
            return true;
        }
        const int gp = std::max(std::abs(x - tx), std::abs(y - ty));
        if (gp < bestGap || (gp == bestGap && c0 < bestCost)) best = c, bestGap = gp, bestCost = c0;
        return false;
    });
    if (end < 0) end = best;
    if (end == from) return;
    for (int c = end; c != from; c = VXD.prev[(size_t)c]) path.push_back({c % SX, (c / SX) % SY, c / (SX * SY)});
    std::reverse(path.begin(), path.end());
}
// Дерево путей игрока в режиме кубов (путь под мышью): та же Дейкстра, что у хода; узлы в порядке извлечения (первый узел
// клетки — тот, куда придёт ход без высоты), до maxFt футов. JSON: [x, y, z, номер предка (−1), футы, …]
std::string vxPathTreeJson(const World& w, const Fighter& f, int maxFt) {
    std::vector<int> order;
    std::unordered_map<int, int> num;
    vxDijkstra(w, f, -1, -1, [&](int c, int c0, int, int, int) {
        if (c0 > 64 * maxFt + 63) return true;
        num[c] = (int)order.size();
        order.push_back(c);
        return false;
    });
    std::string o = ",\"vt\":[";
    const int SX = VXD.SX, SY = VXD.SY;
    for (size_t i = 0; i < order.size(); i++) {
        const int c = order[i], p = VXD.prev[(size_t)c];
        const auto it = p >= 0 ? num.find(p) : num.end();
        o += (i ? "," : "") + std::to_string(c % SX) + "," + std::to_string((c / SX) % SY) + "," + std::to_string(c / (SX * SY)) + "," +
             std::to_string(it != num.end() ? it->second : -1) + "," + std::to_string(VXD.cost[(size_t)c] / 64);
    }
    return o + "]";
}
// Луч по кубам (Amanatides–Woo) от точки a к точке b (координаты в кубах); кубы самих концов не считаются. f(материал) —
// код куба на луче: 0 — пропустить, ≥ 100 — преграда (дальше не идём). Возвращает наибольший код
template <class P>
int vxRay(const World& w, double ax, double ay, double az, double bx, double by, double bz, P&& f) {
    const VoxWorld& v = *w.vx;
    const double ddx = bx - ax, ddy = by - ay, ddz = bz - az;
    int x = (int)std::floor(ax), y = (int)std::floor(ay), z = (int)std::floor(az), best = 0;
    const int ex = (int)std::floor(bx), ey = (int)std::floor(by), ez = (int)std::floor(bz);
    const int sx = ddx > 0 ? 1 : (ddx < 0 ? -1 : 0), sy = ddy > 0 ? 1 : (ddy < 0 ? -1 : 0), sz = ddz > 0 ? 1 : (ddz < 0 ? -1 : 0);
    const double tdx = sx ? std::abs(1 / ddx) : 1e9, tdy = sy ? std::abs(1 / ddy) : 1e9, tdz = sz ? std::abs(1 / ddz) : 1e9;
    double tx = sx ? (sx > 0 ? (x + 1 - ax) : (ax - x)) * tdx : 1e9, ty = sy ? (sy > 0 ? (y + 1 - ay) : (ay - y)) * tdy : 1e9,
           tz = sz ? (sz > 0 ? (z + 1 - az) : (az - z)) * tdz : 1e9;
    for (int i = 0; i < 4 * (W + VH_MAX); i++) {
        if (tx < ty && tx < tz) { x += sx; if (tx > 1) break; tx += tdx; }
        else if (ty < tz) { y += sy; if (ty > 1) break; ty += tdy; }
        else { z += sz; if (tz > 1) break; tz += tdz; }
        if (x == ex && y == ey && z == ez) break;
        best = std::max(best, f(v.at(x, y, z)));
        if (best >= 100) break;
    }
    return best;
}
// Обзор лучом по кубам: от глаз (низ куба ног z + e0, vxEyeH: средний — 0,8 куба) клетки (x0, y0) к глазам в (x1, y1);
// закрывают твёрдые непрозрачные кубы (листва, укрытие, лёд — насквозь), кроме кубов самих концов
bool vxLos(const World& w, int x0, int y0, int z0, int x1, int y1, int z1, double e0, double e1) {
    return vxRay(w, x0 + 0.5, y0 + 0.5, z0 + e0, x1 + 0.5, y1 + 0.5, z1 + e1,
                 [](unsigned char m) { return MAT[m].solid && !MAT[m].seeThru ? 100 : 0; }) < 100;
}
// середина тела над низом куба ног (в кубах): половина роста — крошечный 0,125, маленький 0,25, средний 0,5, большой 1 …
inline double vxMidH(const Fighter& f, int n) { return f.b.size == 0 ? .125 : (f.b.size == 1 ? .25 : .5 * n); }
// Область в объёме (PHB «Области»): распространяется от точки начала по прямым; полное укрытие её останавливает — твёрдое,
// кроме листвы и ящика-укрытия (лёд и силовой барьер — преграда, как стекло)
inline bool vxBlocksEffect(unsigned char m) { return MAT[m].solid && m != VX_LEAF && m != VX_COVER; }
// точка p (центр куба) в форме: сфера и куб — вокруг c, конус и линия — от o в сторону c (как inShape, но в объёме)
inline bool vxInShape3(int shape, int size, double ox, double oy, double oz, double cx, double cy, double cz, double px, double py, double pz) {
    if (shape == SH_CUBE) return 5 * std::max({std::fabs(px - cx), std::fabs(py - cy), std::fabs(pz - cz)}) <= size + 1e-9;
    if (shape == SH_SPHERE) {
        const double dx = 5 * (px - cx), dy = 5 * (py - cy), dz = 5 * (pz - cz), r = size + 2.5;
        return dx * dx + dy * dy + dz * dz <= r * r;
    }
    const double ux = cx - ox, uy = cy - oy, uz = cz - oz, len = std::sqrt(ux * ux + uy * uy + uz * uz);
    if (len <= 0) return false;
    const double vx = 5 * (px - ox), vy = 5 * (py - oy), vz = 5 * (pz - oz);
    const double t = (vx * ux + vy * uy + vz * uz) / len;
    const double qx = vy * uz - vz * uy, qy = vz * ux - vx * uz, qz = vx * uy - vy * ux, perp = std::sqrt(qx * qx + qy * qy + qz * qz) / len;
    if (t <= 0 || t > size + 2.5) return false;
    return shape == SH_LINE ? perp <= 3.0 : perp <= t / 2.0 + 2.5;
}
// точка начала области: сфера и куб — середина куба (cx, cy, cz), конус и линия — глаза применившего
inline void vxAreaOrigin(int shape, const Fighter& me, int cx, int cy, int cz, double& ox, double& oy, double& oz) {
    if (shape == SH_CONE || shape == SH_LINE) {
        const int n = fpN(me);
        ox = me.x + n / 2.0, oy = me.y + n / 2.0, oz = me.z + vxEyeH(me, n);
    } else ox = cx + .5, oy = cy + .5, oz = cz + .5;
}
// куб p задет областью: в форме и до него от точки начала нет полного укрытия
inline bool vxAreaCube(const World& w, int shape, int size, const Fighter& me, int cx, int cy, int cz, int px, int py, int pz) {
    double ox, oy, oz;
    vxAreaOrigin(shape, me, cx, cy, cz, ox, oy, oz);
    if (!vxInShape3(shape, size, ox, oy, oz, cx + .5, cy + .5, cz + .5, px + .5, py + .5, pz + .5)) return false;
    if (vxBlocksEffect(w.vx->at(px, py, pz))) return false;
    return vxRay(w, ox, oy, oz, px + .5, py + .5, pz + .5, [](unsigned char m) { return vxBlocksEffect(m) ? 100 : 0; }) < 100;
}
// Область задевает бойца e (центр — куб (cx, cy, cz)): прежний бой — по клеткам (inShapeF); режим кубов — хоть один куб его
// тела в форме и без полного укрытия от точки начала
inline bool areaHits(const World& w, int shape, int size, const Fighter& me, int cx, int cy, int cz, const Fighter& e) {
    if (!w.vx) return inShapeF(shape, size, me.x, me.y, cx, cy, e);
    const int n = fpN(e);
    for (int k = 0; k < n; k++)
        for (int dy = 0; dy < n; dy++)
            for (int dx = 0; dx < n; dx++)
                if (vxAreaCube(w, shape, size, me, cx, cy, cz, e.x + dx, e.y + dy, e.z + k)) return true;
    return false;
}
// высота центра области в клетке (режим кубов): живой боец, чей квадрат начинается в клетке, — его куб ног, иначе — воздух
// над землёй; прежний бой — 0 (не используется)
inline int vxAreaZ(const World& w, int cx, int cy) {
    if (!w.vx) return 0;
    for (auto& g : w.f)
        if (g.hp > 0 && g.x == cx && g.y == cy) return g.z;
    return vxTopZ(w, cx, cy);
}
// Укрытие (решение ТЗ: один луч): от глаз a к середине тела d. На луче листва или ящик-укрытие — полукрытие (+2 КД),
// камень, стена и прочее твёрдое (лёд, барьер) — укрытие на три четверти (+5); полное укрытие — цель не видна (обзор)
inline int vxCover(const World& w, const Fighter& a, const Fighter& d) {
    const int na = fpN(a), nd = fpN(d);
    return vxRay(w, a.x + na / 2.0, a.y + na / 2.0, a.z + vxEyeH(a, na), d.x + nd / 2.0, d.y + nd / 2.0, d.z + vxMidH(d, nd),
                 [](unsigned char m) { return m == VX_WOOD || m == VX_LEAF || m == VX_COVER ? 2 : (!MAT[m].solid ? 0 : 5); });
}
// копатель прошёл: кубы его тела, которые были землёй, стали воздухом (ход остаётся; событие vox в кадр)
inline void vxDig(World& w, Fighter& f) {
    if (!w.vx || !f.b.burrowS) return;
    VoxWorld& v = *w.vx;
    const int n = fpFull(f), h = n;   // тело n×n×n: тоннель среднего — 1 куб
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++)
            for (int k = 0; k < h; k++)
                if (vxDiggable(v.at(f.x + dx, f.y + dy, f.z + k))) v.set(f.x + dx, f.y + dy, f.z + k, VX_AIR, true);
}
// после разрушения куба: если от объекта клетки (стена, дерево, укрытие, дверь) не осталось ни куба — объекта больше нет и в
// слоях карты (заклинания, укрытие по-прежнему смотрят на слои)
inline void vxAfterBreak(World& w, int x, int y) {
    if (!w.vx || !isObject(w, x, y)) return;
    const VoxWorld& v = *w.vx;
    const int g = VZ0 + w.height[y][x];
    for (int z = g; z < std::min(v.sz, g + std::max(WALL_UP, (int)w.tree[y][x])); z++) {
        const unsigned char m = v.at(x, y, z);
        if (m == VX_STONE || m == VX_STRONG || m == VX_WOOD || m == VX_LEAF || m == VX_COVER || m == VX_DOOR) return;
    }
    w.wall[y][x] = w.climb[y][x] = false;
    w.tree[y][x] = 0;
    w.door[y][x] = 0;
    w.ohp[y][x] = 0;
    w.mapVer = ++g_mapSerial;
}
