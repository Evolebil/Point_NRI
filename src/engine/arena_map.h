// Арена: генерация карты, основная связная часть, места старта
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// параметры генерации карты (по умолчанию — прежняя карта)

// Арена: прямоугольник от 5×5 до 100×100 клеток (по 5 фт), тема (дикая местность или город) и особенности: стены, лазалки (укрытия), труднопроходимая
// местность, тьма, ручей (труднопроходим), ущелье с мостом, возвышенности. Всё за пределами прямоугольника — стена. Выход за арену — побег (поражение).
#include "world_gen.h"
// клетка мира → клетка арены (участок мира и участок по заданной местности)
inline void putWorldCell(World& w, int x, int y, const wg::Cell& c) {
    w.wall[y][x] = c.wall;
    w.climb[y][x] = c.climb && !c.wall;
    w.water[y][x] = c.water;
    w.diff[y][x] = c.diff || c.water;
    w.dark[y][x] = c.dark;
    w.pit[y][x] = c.pit && !c.wall;
    w.height[y][x] = c.height;
    w.tree[y][x] = c.tree && !c.wall && !c.pit && !c.water;
    w.haz[y][x] = c.wall || c.pit ? 0 : c.haz;
    w.ladder[y][x] = c.ladder && !c.wall && !c.pit;
    w.deck[y][x] = !c.wall && c.deck > c.height ? c.deck : 0;
    w.door[y][x] = c.wall ? 0 : c.door;
    w.strong[y][x] = c.strong && c.wall;
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
        if (stride == 1) wg::fillLevelCache(P, x0, y0, spanW, spanH);
        else wg::lvlCache().p = nullptr;
        for (int y = 0; y < W; y++)
            for (int x = 0; x < W; x++) {
                if (x >= w.aw || y >= w.ah) {   // за пределами арены — стена
                    w.wall[y][x] = true;
                    continue;
                }
                const wg::Cell c = wg::worldCell(P, x0 + x * stride, y0 + y * stride);
                putWorldCell(w, x, y, c);
                townN += c.town;
            }
        w.wseed = ws;
        w.wsize = wsz;
        w.wx = x0;
        w.wy = y0;
        w.wstep = stride;
        w.theme = g_style.theme >= 0 ? g_style.theme : (townN * 4 >= w.aw * w.ah ? 1 : 0);   // тема «город» — четверть участка и больше
        const int mainN = markMain(w), startN = startCellsN(w);   // мест для расстановки должно быть хотя бы 2
        if (startN >= 2 && (mainN >= std::max(2, (int)(0.3 * w.aw * w.ah)) || g_style.wx >= 0 || tries >= 40)) break;
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
    for (int y = 0; y < W; y++)
        for (int x = 0; x < W; x++) {
            if (x >= w.aw || y >= w.ah) {   // за пределами участка — стена (край арены)
                putWorldCell(w, x, y, wg::Cell{});
                w.wall[y][x] = true;
                continue;
            }
            const wg::Cell c = wg::worldCell(P, x0 + x, y0 + y);
            putWorldCell(w, x, y, c);
            townN += c.town;
        }
    wg::lvlCache().p = nullptr;   // кэш уровней — этого участка; следующий участок его перезаполнит
    w.wseed = 0;
    w.wsize = 0;
    w.wx = w.wy = 0;
    w.wstep = 1;
    w.theme = townN * 4 >= w.aw * w.ah ? 1 : 0;
    markMain(w);
}
