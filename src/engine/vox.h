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
// (листва и вода — пока насквозь: частичное укрытие листвой — этап 2); liquid — плавание (этап 2)
struct MatInfo { const char* name; short hp; unsigned char ac; bool solid, seeThru, liquid; };
// КД и хиты — как у объектов сейчас (DMG «Объекты»: дерево КД 15, камень КД 17): кубик стены — OBJ_WALL_HP, крепкой —
// OBJ_STRONG_HP, ствола — OBJ_TREE_HP, двери — OBJ_DOOR_HP, настила — DECK_HP; земля и песок — мягче камня
constexpr MatInfo MAT[VX_N] = {
    {"воздух", 0, 0, false, true, false},      {"дно мира", 0, 0, true, false, false},
    {"земля", 27, 10, true, false, false},       {"трава", 27, 10, true, false, false},
    {"песок", 18, 8, true, false, false},        {"камень", OBJ_WALL_HP, 17, true, false, false},
    {"крепкий камень", OBJ_STRONG_HP, 19, true, false, false},
    {"дерево", OBJ_TREE_HP, 15, true, false, false}, {"листва", 5, 10, true, true, false},
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
inline bool vxDiggable(unsigned char m) { return m == VX_DIRT || m == VX_GRASS || m == VX_SAND || m == VX_RUBBLE || m == VX_LEAF; }
// верхняя опора клетки: куб над самым верхним твёрдым (стоять на стене, на кроне, на крыше)
inline int vxTopZ(const World& w, int x, int y) {
    const VoxWorld& v = *w.vx;
    for (int z = v.sz - 1; z >= 1; z--)
        if (MAT[v.at(x, y, z - 1)].solid && !MAT[v.at(x, y, z)].solid) return z;
    return 1;
}
// Можно ли f стоять ногами в кубе z клетки (x, y) (квадрат n×n, рост — bodyZ: средний 10 фт = 2 куба): тело — в воздухе или
// жидкости (копатель — и в копаемом), опора — твёрдое или жидкость под ногами хоть одной клетки, лестница (до верха соседей),
// стена рядом (лазун), земля вокруг (копатель); летуну опора не нужна
// fly — держится ли в воздухе (vxFixAll проверяет опору и у летуна, сбитого с полёта)
inline bool vxStandEx(const World& w, const Fighter& f, int x, int y, int z, bool fly) {
    const VoxWorld& v = *w.vx;
    const int n = fpFull(f), h = bodyZ(sizeLv(f));
    if (x < 0 || y < 0 || x + n > w.aw || y + n > w.ah || z < 1 || z + h > v.sz) return false;
    bool sup = fly;
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++) {
            const int cx = x + dx, cy = y + dy;
            for (int k = 0; k < h; k++) {
                const unsigned char m = v.at(cx, cy, z + k);
                if (!MAT[m].solid) continue;
                if (!(f.b.burrowS && vxDiggable(m))) return false;
                sup = true;   // копатель в земле держится за неё
            }
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
            if (!sup && f.b.climbS)
                for (int k = 0; k < 4 && !sup; k++) {
                    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
                    sup = MAT[v.at(cx + DX[k], cy + DY[k], z)].solid || MAT[v.at(cx + DX[k], cy + DY[k], z + 1)].solid;
                }
        }
    return sup;
}
inline bool vxStand(const World& w, const Fighter& f, int x, int y, int z) { return vxStandEx(w, f, x, y, z, f.b.fly); }
// тело f в кубе z свободно (воздух или жидкость; копателю — и копаемое): туда можно упасть
inline bool vxBodyFree(const World& w, const Fighter& f, int x, int y, int z) {
    const VoxWorld& v = *w.vx;
    const int n = fpFull(f), h = bodyZ(sizeLv(f));
    if (z < 1 || z + h > v.sz) return false;
    for (int dy = 0; dy < n; dy++)
        for (int dx = 0; dx < n; dx++)
            for (int k = 0; k < h; k++) {
                const unsigned char m = v.at(x + dx, y + dy, z + k);
                if (MAT[m].solid && !(f.b.burrowS && vxDiggable(m))) return false;
            }
    return true;
}
// Шаг между узлами: в соседнюю клетку — уступ до 10 фт вверх или вниз (больше — обрыв, как прежде); по вертикали на куб —
// летун, лазун у стены, лестница, копатель. Наискосок — не сквозь угол стены (кроме копателя)
inline bool vxStep(const World& w, const Fighter& f, int x, int y, int z, int nx, int ny, int nz) {
    const int dx = nx - x, dy = ny - y, dz = nz - z;
    if (std::abs(dx) > 1 || std::abs(dy) > 1 || (!dx && !dy && !dz) || !vxStand(w, f, nx, ny, nz)) return false;
    if (!dx && !dy) return std::abs(dz) == 1 && (f.b.fly || f.b.climbS || f.b.burrowS || w.ladder[y][x]);
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
    return cost;
}
// Путь в клетку (tx, ty) — Дейкстра по узлам-кубам (цена — футы, при равенстве — меньше клеток зон); известных врагов
// обходит (кроме того, кто стоит в цели). Цель — любой узел клетки, а если игрок щёлкнул по высоте (g_vxTz) — этот куб;
// не дойти — как можно ближе к цели (как прежний pathTo)
void vxPathTo(const World& w, const Fighter& f, int tx, int ty, std::vector<PStep>& path) {
    const VoxWorld& v = *w.vx;
    const int SX = v.sx, SY = v.sy, SZ = v.sz, N = SX * SY * SZ;
    if (f.x < 0 || f.y < 0 || f.x >= SX || f.y >= SY || f.z < 0 || f.z >= SZ) return;
    static thread_local std::vector<int> cost, prev, stamp;
    static thread_local int cur = 0;
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
        if (g.hp > 0 && g.team != f.team && (!inWorld || isKnown(w, f.team, (int)i)) && !fpOverlap(g.x, g.y, fpN(g), tx, ty, 1)) foes.push_back((int)i);
    }
    const int from = id(f.x, f.y, f.z);
    std::vector<std::pair<int, int>> q;
    stamp[(size_t)from] = cur;
    cost[(size_t)from] = 0;
    prev[(size_t)from] = -1;
    q.push_back({0, from});
    int end = -1, best = from, bestGap = 1 << 20, bestCost = 0;
    while (!q.empty()) {
        std::pop_heap(q.begin(), q.end(), std::greater<>());
        const auto [c0, c] = q.back();
        q.pop_back();
        if (c0 != cost[(size_t)c]) continue;
        const int x = c % SX, y = (c / SX) % SY, z = c / (SX * SY);
        if (x == tx && y == ty && c != from && (g_vxTz < 0 || z == g_vxTz)) {
            end = c;
            break;
        }
        const int gp = std::max(std::abs(x - tx), std::abs(y - ty));
        if (gp < bestGap || (gp == bestGap && c0 < bestCost)) best = c, bestGap = gp, bestCost = c0;
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
    if (end < 0) end = best;
    if (end == from) return;
    for (int c = end; c != from; c = prev[(size_t)c]) path.push_back({c % SX, (c / SX) % SY, c / (SX * SY)});
    std::reverse(path.begin(), path.end());
}
// Обзор лучом по кубам (Amanatides–Woo): от глаз (куб ног z + 1,5 — в верхнем кубе роста среднего) клетки (x0, y0) к глазам
// в (x1, y1); закрывают твёрдые непрозрачные кубы (листва, укрытие, лёд — насквозь), кроме кубов самих концов
bool vxLos(const World& w, int x0, int y0, int z0, int x1, int y1, int z1) {
    const VoxWorld& v = *w.vx;
    const double ax = x0 + 0.5, ay = y0 + 0.5, az = z0 + 1.5, bx = x1 + 0.5, by = y1 + 0.5, bz = z1 + 1.5;
    const double ddx = bx - ax, ddy = by - ay, ddz = bz - az;
    int x = x0, y = y0, z = (int)std::floor(az);
    const int ex = x1, ey = y1, ez = (int)std::floor(bz);
    const int sx = ddx > 0 ? 1 : (ddx < 0 ? -1 : 0), sy = ddy > 0 ? 1 : (ddy < 0 ? -1 : 0), sz = ddz > 0 ? 1 : (ddz < 0 ? -1 : 0);
    const double tdx = sx ? std::abs(1 / ddx) : 1e9, tdy = sy ? std::abs(1 / ddy) : 1e9, tdz = sz ? std::abs(1 / ddz) : 1e9;
    double tx = sx ? (sx > 0 ? (x + 1 - ax) : (ax - x)) * tdx : 1e9, ty = sy ? (sy > 0 ? (y + 1 - ay) : (ay - y)) * tdy : 1e9,
           tz = sz ? (sz > 0 ? (z + 1 - az) : (az - z)) * tdz : 1e9;
    for (int i = 0; i < 4 * (W + VH_MAX); i++) {
        if (tx < ty && tx < tz) { x += sx; if (tx > 1) break; tx += tdx; }
        else if (ty < tz) { y += sy; if (ty > 1) break; ty += tdy; }
        else { z += sz; if (tz > 1) break; tz += tdz; }
        if (x == ex && y == ey && z == ez) break;
        const unsigned char m = v.at(x, y, z);
        if (MAT[m].solid && !MAT[m].seeThru) return false;
    }
    return true;
}
// копатель прошёл: кубы его тела, которые были землёй, стали воздухом (ход остаётся; событие vox в кадр)
inline void vxDig(World& w, Fighter& f) {
    if (!w.vx || !f.b.burrowS) return;
    VoxWorld& v = *w.vx;
    const int n = fpFull(f), h = bodyZ(sizeLv(f));
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
