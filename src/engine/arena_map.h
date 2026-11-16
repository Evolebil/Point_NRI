// Арена: генерация карты, основная связная часть, места старта
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// параметры генерации карты (по умолчанию — прежняя карта)

// Арена: прямоугольник от 5×5 до 100×100 клеток (по 5 фт), тема (дикая местность или город) и особенности: стены, лазалки (укрытия), труднопроходимая
// местность, тьма, ручей (труднопроходим), ущелье с мостом, возвышенности. Всё за пределами прямоугольника — стена. Выход за арену — побег (поражение).
#include "world_gen.h"
// клетка мира → клетка арены (участок мира и участок по заданной местности)
// h5 — высота земли шагом 5 фт (wg::fineHeight); −1 — 2·c.height
inline void putWorldCell(World& w, int x, int y, const wg::Cell& c, int h5 = -1) {
    w.wall[y][x] = c.wall;
    w.climb[y][x] = c.climb && !c.wall;
    w.water[y][x] = c.water;
    w.diff[y][x] = c.diff || c.water;
    w.dark[y][x] = c.dark;
    w.pit[y][x] = c.pit && !c.wall;
    w.height[y][x] = (unsigned char)(h5 >= 0 ? h5 : 2 * c.height);   // мир — уровни по 10 фт, арена — шагом 5 фт (fineHeight)
    w.tree[y][x] = (unsigned char)(c.tree && !c.wall && !c.pit && !c.water ? (c.treeH ? c.treeH : TREE_DEF) : 0);   // дерево своей высоты (10–40 фт)
    w.haz[y][x] = c.wall || c.pit ? 0 : c.haz;
    if (w.haz[y][x] && HAZ[w.haz[y][x]].obscure) w.fogAny = true;
    w.ladder[y][x] = c.ladder && !c.wall && !c.pit;
    w.deck[y][x] = (unsigned char)(!c.wall && c.deck > c.height ? 2 * c.deck : 0);
    w.door[y][x] = c.wall ? 0 : c.door;
    w.strong[y][x] = c.strong && c.wall;
}
// ---- слой кубов арены (этап 4): пещеры и дома с этажами и крышами (только режим кубов; бой по слоям его не видит) ----
// Строится по зерну и мировым координатам, без общего генератора случайных чисел: бои по слоям — побитово прежние
thread_local bool g_volGen = false;   // строить слой кубов и вне боя в кубах (карта для редактора и кампании: genmap, regionmap)
// что собрал проход по клеткам мира: поселение (под ним пещер нет), дома (угол в клетках арены → клетка дома) и чей дом в
// клетке (номер угла y·4·W + x со сдвигом; −1 — не дом): обрезанный краем поселения дом строится только по своим клеткам
struct VolSrc {
    std::vector<unsigned char> town;
    std::vector<int> own;
    std::map<std::pair<int, int>, wg::Cell> houses;
    static int key(int ox, int oy) { return (oy + W) * 4 * W + ox + W; }
    void reset(const World& w) { town.assign((size_t)w.aw * w.ah, 0); own.assign((size_t)w.aw * w.ah, -1); houses.clear(); }
    void add(int x, int y, int aw, const wg::Cell& c) {
        town[(size_t)y * aw + x] = c.town;
        if (!c.hw) return;
        houses.emplace(std::make_pair(x - c.hx, y - c.hy), c);
        own[(size_t)y * aw + x] = key(x - c.hx, y - c.hy);
    }
};
// столбец слоя кубов из набора «куб → материал» (символ base36 VoxMat; чего нет — '.', как по слоям)
static void vxoPut(World& w, int x, int y, const std::map<int, char>& col) {
    if (col.empty()) return;
    const int z0 = col.begin()->first, z1 = col.rbegin()->first;
    std::string m((size_t)(z1 - z0 + 1), '.');
    for (const auto& [z, c] : col) m[(size_t)(z - z0)] = c;
    w.vxo[y * W + x] = {z0, m};
}
// Дом из кубов (решение владельца 2026-11-16): этаж — 2 куба (10 фт) до потолка, зал — 3; пол верхнего этажа — доски;
// ступени по кубу у стены (идут обычным шагом); двускатная крыша из досок уступами по кубу (по ней ходят, конёк — вдоль
// длинной стороны, не выше 3 кубов над карнизом, торцы — стеной); стены высотой дома: деревня — доски, город — камень.
// Снаружи у длинной стены — приставная лестница на карниз. Дом на неровной земле (клетки разной высоты) — по слоям, как был
static void houseVol(World& w, int x0, int y0, const wg::Cell& h, const VolSrc& src) {
    const int HW = h.hw, HH = h.hh, F = std::max(1, (int)h.floors), Hs = h.hall ? 3 : 2;
    int b = -1;
    for (int qy = 0; qy < HH; qy++)
        for (int qx = 0; qx < HW; qx++) {
            const int x = x0 + qx, y = y0 + qy;
            if (x < 0 || y < 0 || x >= w.aw || y >= w.ah || src.own[(size_t)y * w.aw + x] != VolSrc::key(x0, y0)) continue;
            const int g = VZ0 + w.height[y][x];
            if (b < 0) b = g;
            else if (g != b) return;
        }
    if (b < 0) return;
    const int zt = b + F * (Hs + 1) - 1;   // карниз: верх стен, нижний ряд крыши
    const bool ridgeX = HW >= HH;
    const int L = ridgeX ? HH : HW;
    auto dOf = [&](int qx, int qy) { const int q = ridgeX ? qy : qx; return std::min({q, L - 1 - q, 3}); };
    // ступени: по сторонам без двери (дверь с севера или юга — у западной и восточной стен, и наоборот), этаж k — своя сторона
    const int ds = h.hdoor, c0 = ds < 2 ? 2 : 0, pick = (HW * 7 + HH * 3 + ds) & 1;
    auto stairAt = [&](int k, int qx, int qy) {   // номер ступени (1..Hs+1) лестницы на этаж k+1 в клетке, 0 — нет
        const int sd = c0 + ((pick + k - 1) & 1);
        for (int i = 1; i <= Hs + 1; i++) {
            const int sx = sd == 0 ? i : sd == 1 ? HW - 1 - i : sd == 2 ? 1 : HW - 2, sy = sd == 0 ? 1 : sd == 1 ? HH - 2 : sd == 2 ? HH - 1 - i : i;
            if (sx == qx && sy == qy) return i;
        }
        return 0;
    };
    for (int qy = 0; qy < HH; qy++)
        for (int qx = 0; qx < HW; qx++) {
            const int x = x0 + qx, y = y0 + qy;
            if (x < 0 || y < 0 || x >= w.aw || y >= w.ah || src.own[(size_t)y * w.aw + x] != VolSrc::key(x0, y0)) continue;
            const char wallM = h.wood ? 'a' : (w.strong[y][x] ? '6' : '5');
            const int d = dOf(qx, qy), roof = zt + d;
            const bool border = qx == 0 || qy == 0 || qx == HW - 1 || qy == HH - 1;
            std::map<int, char> col;
            for (int z = b + 1; z < std::max(roof + 1, b + WALL_UP + 2); z++) col[z] = '0';
            if (border) {
                const bool door = w.door[y][x];
                for (int z = door ? b + 2 : b; z < roof; z++) col[z] = wallM;   // стена (над дверью — перемычка), торец — до крыши
                if (door) col.erase(b + 1);
            } else {
                for (int k = 1; k < F; k++) {
                    const int zf = b + (k - 1) * (Hs + 1), zs = zf + Hs, st = stairAt(k, qx, qy);
                    if (!st) col[zs] = 'a';   // пол этажа k+1
                    for (int z = zf; z < zf + st; z++) col[z] = 'a';   // ступень: st кубов от пола этажа k
                }
            }
            col[roof] = 'a';
            vxoPut(w, x, y, col);
        }
    // приставная лестница на карниз: снаружи у длинной стены, на ровной свободной земле
    const int cand[2][2] = {{ridgeX ? 1 : -1, ridgeX ? -1 : 1}, {ridgeX ? HW - 2 : HW, ridgeX ? HH : HH - 2}};
    for (const auto& q : cand) {
        const int x = x0 + q[0], y = y0 + q[1];
        if (x < 1 || y < 1 || x >= w.aw - 1 || y >= w.ah - 1 || src.own[(size_t)y * w.aw + x] >= 0 || VZ0 + w.height[y][x] != b) continue;
        if (w.wall[y][x] || w.water[y][x] || w.pit[y][x] || w.tree[y][x] || w.door[y][x] || w.deck[y][x] || w.haz[y][x]) continue;
        std::map<int, char> col;
        for (int z = b; z <= zt; z++) col[z] = 'i';
        vxoPut(w, x, y, col);
        break;
    }
}
// Пещеры (решение владельца 2026-11-16): ходы и залы из объёмного шума (wg::caveAir) в мировых координатах gx0 + x, gy0 + y,
// под землёй не меньше 2 кубов (нет под водой, ущельем, опасностью и поселением). У каждой пещеры (связной полости от 12
// кубов) — выход на поверхность: спуск ступенями по кубу из её места поближе к поверхности, по прямой в ту сторону, где он
// короче, не ближе 2 клеток к краю арены
static void caveVol(World& w, uint64_t seed, int gx0, int gy0, const std::vector<unsigned char>& town) {
    const int AW = w.aw, AH = w.ah;
    int Z = 0;
    for (int y = 0; y < AH; y++)
        for (int x = 0; x < AW; x++) Z = std::max(Z, VZ0 + (int)w.height[y][x] + 1);
    auto id = [&](int x, int y, int z) { return ((size_t)y * AW + x) * Z + z; };
    std::vector<unsigned char> air((size_t)AW * AH * Z, 0);
    auto ground = [&](int x, int y) { return VZ0 + (int)w.height[y][x]; };   // первый куб воздуха над землёй
    auto open = [&](int x, int y) { return !w.water[y][x] && !w.pit[y][x] && !w.haz[y][x] && !town[(size_t)y * AW + x]; };
    bool any = false;
    for (int y = 0; y < AH; y++)
        for (int x = 0; x < AW; x++) {
            if (!open(x, y)) continue;
            for (int z = 1; z <= ground(x, y) - 3; z++)
                if (wg::caveAir(seed, gx0 + x, gy0 + y, z)) air[id(x, y, z)] = 1, any = true;
        }
    if (!any) return;
    // связные полости (6 соседей) и выход у каждой
    std::vector<int> comp(air.size(), -1), st;
    for (size_t s0 = 0; s0 < air.size(); s0++) {
        if (!air[s0] || comp[s0] >= 0) continue;
        const int cid = (int)s0;
        std::vector<int> cubes;
        st.assign(1, (int)s0);
        comp[s0] = cid;
        while (!st.empty()) {
            const int c = st.back();
            st.pop_back();
            cubes.push_back(c);
            const int z = c % Z, x = (c / Z) % AW, y = c / Z / AW;
            static const int D[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            for (const auto& e : D) {
                const int nx = x + e[0], ny = y + e[1], nz = z + e[2];
                if (nx < 0 || ny < 0 || nz < 0 || nx >= AW || ny >= AH || nz >= Z) continue;
                const size_t n = id(nx, ny, nz);
                if (air[n] && comp[n] < 0) comp[n] = cid, st.push_back((int)n);
            }
        }
        if (cubes.size() < 12) continue;
        // место для выхода: куб пола (под ним не полость) поближе к поверхности; спуск — самый короткий из четырёх сторон
        std::vector<std::array<int, 3>> best;
        int bestLen = 1 << 30;
        for (int c : cubes) {
            const int z = c % Z, x = (c / Z) % AW, y = c / Z / AW;
            if (z > 1 && air[id(x, y, z - 1)]) continue;
            if (ground(x, y) - z > (int)(bestLen + 1)) continue;
            static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                std::vector<std::array<int, 3>> ramp;
                bool ok = false;
                for (int i = 1; i <= 40; i++) {
                    const int rx = x + DX[k] * i, ry = y + DY[k] * i, zs = z + i;
                    if (rx < 2 || ry < 2 || rx >= AW - 2 || ry >= AH - 2 || !open(rx, ry) || w.wall[ry][rx] || w.tree[ry][rx] || w.door[ry][rx] || w.deck[ry][rx]) break;
                    const int g = ground(rx, ry);
                    if (zs >= g) { ok = zs - 1 - g <= 2; break; }   // вышли на поверхность (уступ вниз — не больше 10 фт)
                    ramp.push_back({rx, ry, zs});
                }
                if (ok && (int)ramp.size() < bestLen) { bestLen = (int)ramp.size(); best = ramp; }
            }
        }
        for (const auto& r : best)   // спуск: высота хода 2 куба (поверху — до поверхности)
            for (int z = r[2]; z <= std::min(r[2] + 1, ground(r[0], r[1]) - 1); z++) air[id(r[0], r[1], z)] = 1;
    }
    for (int y = 0; y < AH; y++)
        for (int x = 0; x < AW; x++) {
            std::map<int, char> col;
            for (int z = 1; z < Z; z++)
                if (air[id(x, y, z)]) col[z] = '0';
            vxoPut(w, x, y, col);
        }
}
static void buildVolume(World& w, uint64_t caveSeed, int gx0, int gy0, const VolSrc& src) {
    w.vxo.clear();
    caveVol(w, caveSeed, gx0, gy0, src.town);
    for (const auto& [o, h] : src.houses) houseVol(w, o.first, o.second, h, src);
}
struct MapStyle {
    int fixedW = 0, fixedH = 0, theme = -1;   // заданный размер участка и тема (−1 — как получится), для тестов и редактора
    unsigned long long worldSeed = 0;         // мир по зерну (игра; 0 — обучение: новый мир каждые 500 боёв)
    int worldSize = 1000, wx = -1, wy = -1, worldStep = 1;   // размер мира, угол участка и шаг выборки (−1 — случайно)
};
MapStyle g_style;
// размер стороны арены из мира: 2–100, чаще средние (TZ_karta_shum.md)
static int pickArenaSide() {
    const int r = irand(100);
    if (r < 3) return 2 + irand(4);      // 2–5
    if (r < 10) return 6 + irand(4);     // 6–9
    if (r < 90) return 10 + irand(51);   // 10–60
    if (r < 95) return 61 + irand(19);   // 61–79
    return 80 + irand(21);               // 80–100
}
// основная связная часть арены: пешеход среднего роста по правилам движка (обрывы, лестницы, настилы); бой начинается в ней.
// Возвращает её размер (клеток земли)
int markMain(World& w) {
    static thread_local std::vector<int> comp, st;
    comp.assign((size_t)2 * W * W, -1);
    Fighter walker;
    walker.b.size = 2;
    int best = -1, bestN = 0, id = 0;
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) {
            if (comp[(size_t)y * W + x] >= 0 || !nodeOK(w, walker, x, y, 0)) continue;
            int n = 0;
            st.assign(1, y * W + x);
            comp[(size_t)y * W + x] = id;
            while (!st.empty()) {
                const int c = st.back();
                st.pop_back();
                const int l = c / (W * W), cx = c % W, cy = (c % (W * W)) / W;
                if (!l) n++;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        for (int nl = 0; nl < 2; nl++) {
                            const int nx = cx + dx, ny = cy + dy, nn = ny * W + nx + nl * W * W;
                            if ((!dx && !dy) || !stepOK(w, walker, cx, cy, l, nx, ny, nl) || comp[(size_t)nn] >= 0) continue;
                            comp[(size_t)nn] = id;
                            st.push_back(nn);
                        }
            }
            if (n > bestN) { bestN = n; best = id; }
            id++;
        }
    std::memset(w.mainc, 0, sizeof w.mainc);
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) w.mainc[y][x] = best >= 0 && comp[(size_t)y * W + x] == best;
    return bestN;
}
// клетка, где можно поставить бойца в начале боя: основная часть, не стена, яма, дерево, мост или дверь
static bool startCellOK(const World& w, int x, int y) {
    return !w.wall[y][x] && !w.pit[y][x] && !w.tree[y][x] && !w.deck[y][x] && !w.door[y][x] && w.mainc[y][x];
}
static int startCellsN(const World& w) {
    int n = 0;
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) n += startCellOK(w, x, y);
    return n;
}
// мир боя: в игре — заданный (g_style.worldSeed), иначе — мир эпохи g_worldEpoch. Эпоху выставляет главный поток до запуска боёв
// (обучение — номер боя / 500, т. е. новый мир каждые 500 боёв; проверки — номер блока). Бой зависит только от зерна и эпохи,
// а не от того, сколько боёв поток сыграл до него. План мира кэшируется в каждом потоке.
long long g_worldEpoch = 0;
thread_local wg::Plan g_wplan;
inline unsigned long long worldSeedOfEpoch(long long e) {
    unsigned long long z = (unsigned long long)e * 0x9E3779B97F4A7C15ULL + 0xD1B54A32D192ED03ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return (z ^ (z >> 31)) | 1;
}
void genMap(World& w) {
    std::memset(w.wall, 0, sizeof w.wall);
    std::memset(w.climb, 0, sizeof w.climb);
    std::memset(w.diff, 0, sizeof w.diff);
    std::memset(w.dark, 0, sizeof w.dark);
    std::memset(w.pit, 0, sizeof w.pit);
    std::memset(w.water, 0, sizeof w.water);
    std::memset(w.height, 0, sizeof w.height);
    std::memset(w.tree, 0, sizeof w.tree);
    std::memset(w.haz, 0, sizeof w.haz);
    std::memset(w.ladder, 0, sizeof w.ladder);
    std::memset(w.deck, 0, sizeof w.deck);
    std::memset(w.door, 0, sizeof w.door);
    std::memset(w.strong, 0, sizeof w.strong);
    unsigned long long ws;
    int wsz;
    if (g_style.worldSeed) {
        ws = g_style.worldSeed;
        wsz = std::clamp(g_style.worldSize, 1000, 10000);
    } else {
        ws = worldSeedOfEpoch(g_worldEpoch);
        wsz = 1000;
    }
    if (g_wplan.seed != ws || g_wplan.size != wsz) wg::buildPlan(g_wplan, ws, wsz);
    const wg::Plan& P = g_wplan;
    const bool vol = g_voxMode || g_volGen;
    static thread_local VolSrc vs;
    w.vxo.clear();
    for (int tries = 0;; tries++) {
        w.aw = g_style.fixedW > 0 ? std::min(W, g_style.fixedW) : pickArenaSide();
        w.ah = g_style.fixedH > 0 ? std::min(W, g_style.fixedH) : pickArenaSide();
        w.aw = std::min(w.aw, wsz);
        w.ah = std::min(w.ah, wsz);
        const int maxStride = std::max(1, std::min(w.aw > 1 ? (wsz - 1) / (w.aw - 1) : 200, w.ah > 1 ? (wsz - 1) / (w.ah - 1) : 200));
        const int stride = std::clamp(g_style.worldStep, 1, maxStride);
        const int spanW = (w.aw - 1) * stride + 1, spanH = (w.ah - 1) * stride + 1;
        const int maxX = std::max(0, wsz - spanW), maxY = std::max(0, wsz - spanH);
        int x0, y0;
        if (g_style.wx >= 0) {
            x0 = std::clamp(g_style.wx, 0, maxX);
            y0 = std::clamp(g_style.wy, 0, maxY);
        } else if (g_style.theme == 1 && !P.towns.empty()) {   // нужен город: участок у случайного поселения
            const wg::Settle& t = P.towns[(size_t)irand((int)P.towns.size())];
            x0 = std::clamp(t.x - spanW / 2 + irand(std::max(1, t.r)) - t.r / 2, 0, maxX);
            y0 = std::clamp(t.y - spanH / 2 + irand(std::max(1, t.r)) - t.r / 2, 0, maxY);
        } else {
            x0 = irand(maxX + 1);
            y0 = irand(maxY + 1);
        }
        int townN = 0;
        if (vol) vs.reset(w);
        if (stride == 1) wg::fillLevelCache(P, x0, y0, spanW, spanH);
        else wg::lvlCache().p = nullptr;
        for (int y = 0; y < W; y++)
            for (int x = 0; x < W; x++) {
                if (x >= w.aw || y >= w.ah) {   // за пределами арены — стена
                    w.wall[y][x] = true;
                    continue;
                }
                const wg::Cell c = wg::worldCell(P, x0 + x * stride, y0 + y * stride);
                putWorldCell(w, x, y, c, wg::fineHeight(P, x0 + x * stride, y0 + y * stride, c));
                townN += c.town;
                if (vol) vs.add(x, y, w.aw, c);
            }
        w.wseed = ws;
        w.wsize = wsz;
        w.wx = x0;
        w.wy = y0;
        w.wstep = stride;
        w.theme = g_style.theme >= 0 ? g_style.theme : (townN * 4 >= w.aw * w.ah ? 1 : 0);   // тема «город» — четверть участка и больше
        const int mainN = markMain(w), startN = startCellsN(w);   // мест для расстановки должно быть хотя бы 2
        if (startN >= 2 && (mainN >= std::max(2, (int)(0.3 * w.aw * w.ah)) || g_style.wx >= 0 || tries >= 40)) {
            if (vol && stride == 1) buildVolume(w, ws, x0, y0, vs);   // слой кубов — по клеткам мира (выборка с шагом — без него)
            break;
        }
        if (tries >= 200) {   // совсем не вышло: пустая арена
            for (int y = 0; y < w.ah; y++)
                for (int x = 0; x < w.aw; x++) w.wall[y][x] = w.pit[y][x] = w.tree[y][x] = false, w.deck[y][x] = w.door[y][x] = 0, w.height[y][x] = 0;
            markMain(w);
            break;
        }
    }
}
// Участок по заданной местности (кампания, задача Б-1): клетка региона 200 × 200 клеток сцены, из неё — окно aw × ah
// по центру (меньший участок — середина той же клетки). Тот же вход — тот же участок; миры и арены обучения не меняются
void regionArena(World& w, const wg::RegionDesc& d, int aw, int ah) {
    static thread_local wg::Plan P;
    wg::buildRegionPlan(P, d, 200);
    w.aw = std::clamp(aw, 5, std::min(W, P.size));
    w.ah = std::clamp(ah, 5, std::min(W, P.size));
    const int x0 = (P.size - w.aw) / 2, y0 = (P.size - w.ah) / 2;
    wg::fillLevelCache(P, x0, y0, w.aw, w.ah);
    int townN = 0;
    const bool vol = g_voxMode || g_volGen;
    static thread_local VolSrc vs;
    w.vxo.clear();
    if (vol) vs.reset(w);
    for (int y = 0; y < W; y++)
        for (int x = 0; x < W; x++) {
            if (x >= w.aw || y >= w.ah) {   // за пределами участка — стена (край арены)
                putWorldCell(w, x, y, wg::Cell{});
                w.wall[y][x] = true;
                continue;
            }
            const wg::Cell c = wg::worldCell(P, x0 + x, y0 + y);
            putWorldCell(w, x, y, c, wg::fineHeight(P, x0 + x, y0 + y, c));
            townN += c.town;
            if (vol) vs.add(x, y, w.aw, c);
        }
    wg::lvlCache().p = nullptr;   // кэш уровней — этого участка; следующий участок его перезаполнит
    // слой кубов: пещеры — в координатах региона (клетка региона gx, gy по 200 клеток и зерно мира), тогда ходы соседних
    // участков сходятся на границе; без места в регионе — в своих координатах по зерну участка
    if (vol) {
        const bool inReg = d.gx >= 0 && d.gy >= 0;
        buildVolume(w, inReg ? wg::seedOfText("пещеры:" + d.wseed) : P.seed ^ 0x43415645, inReg ? d.gx * P.size + x0 : x0, inReg ? d.gy * P.size + y0 : y0, vs);
    }
    w.wseed = 0;
    w.wsize = 0;
    w.wx = w.wy = 0;
    w.wstep = 1;
    w.theme = townN * 4 >= w.aw * w.ah ? 1 : 0;
    markMain(w);
}
