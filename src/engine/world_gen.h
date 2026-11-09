// ============================================================================
// Мир из шума (TZ_karta_shum.md). Мир — квадрат size×size клеток по 5 фт (обучение — 1000, игра — 1000..10000),
// задан зерном. Три плавных шума: высота, температура, влажность; по ним — биом, а по биому — объекты. Грубый план
// всего мира (реки, поселения, дороги) строится один раз на мир; любая клетка мира — функция зерна, плана и её
// координат (worldCell), поэтому участок по тому же зерну всегда одинаков, где бы его ни вырезали. Арена боя —
// участок мира (cutArena). Подключается из движка (arena_map.h) перед genMap.
// ============================================================================
#pragma once
#include <cctype>
#include <cmath>
#include <string>

namespace wg {

inline uint64_t mixh(uint64_t a, uint64_t b) {
    uint64_t z = a + 0x9E3779B97F4A7C15ULL * (b + 1);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
inline uint64_t hash3(uint64_t s, int x, int y, int salt = 0) { return mixh(mixh(mixh(s, (uint64_t)(uint32_t)x), (uint64_t)(uint32_t)y), (uint64_t)salt); }
inline double hash01(uint64_t s, int x, int y, int salt) { return (hash3(s, x, y, salt) >> 11) * (1.0 / 9007199254740992.0); }

// градиентный шум (Перлин) и его сумма по октавам; значения примерно в [-1, 1]
struct Grad {
    float gx[256], gy[256];
    Grad() {
        for (int i = 0; i < 256; i++) {
            gx[i] = (float)std::cos(i * 6.283185307179586 / 256);
            gy[i] = (float)std::sin(i * 6.283185307179586 / 256);
        }
    }
};
inline const Grad& grads() {
    static const Grad g;
    return g;
}
inline double noise(uint64_t s, double x, double y) {
    const Grad& G = grads();
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const double fx = x - ix, fy = y - iy;
    auto corner = [&](int cx, int cy) {
        const int h = (int)(hash3(s, ix + cx, iy + cy) & 255);
        return G.gx[h] * (fx - cx) + G.gy[h] * (fy - cy);
    };
    auto fade = [](double t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    const double u = fade(fx), v = fade(fy);
    const double a = corner(0, 0) + u * (corner(1, 0) - corner(0, 0)), b = corner(0, 1) + u * (corner(1, 1) - corner(0, 1));
    return 1.41 * (a + v * (b - a));
}
inline double fbm(uint64_t s, double x, double y, int oct) {
    double sum = 0, amp = 1, norm = 0, f = 1;
    for (int o = 0; o < oct; o++) {
        sum += amp * noise(s + o * 7919, x * f, y * f);
        norm += amp;
        amp *= 0.5;
        f *= 2.03;
    }
    return sum / norm;
}

// биомы: температура (холодно, умеренно, жарко) × влажность (сухо, средне, сыро)
enum Biome { B_TUNDRA, B_TAIGA, B_GLACIER, B_STEPPE, B_FOREST, B_SWAMP, B_DESERT, B_SAVANNA, B_JUNGLE, B_N };
inline const char* biomeName(int b) {
    static const char* N[B_N] = {"тундра", "тайга", "ледник", "степь", "лес и поля", "болото", "пустыня", "саванна", "джунгли"};
    return N[b];
}

struct Settle {
    int x, y, r, kind;   // центр, радиус в клетках; вид: 0 деревня, 1 город, 2 крепость, 3 большой город
    int level;           // уровень земли поселения (ровная площадка)
};
struct Seg {
    int x0, y0, x1, y1, w;   // отрезок (клетки мира) и ширина
};
struct Plan {
    uint64_t seed = 0;
    int size = 0;
    std::vector<Settle> towns;
    std::vector<Seg> rivers, roads;
    static constexpr int BK = 64;               // сетка отрезков: клетка BK×BK → номера отрезков рядом
    int bn = 0;
    std::vector<std::vector<int>> rivB, roadB, townB;
    // участок по заданной местности (кампания, задача Б-1): высота — уровень клетки региона с переходом к соседям,
    // биом и влажность — из описания; выключено — мир из шума как раньше
    struct Region {
        bool on = false;
        int level = 0, nb[4] = {0, 0, 0, 0};   // уровень клетки и соседей: север, восток, юг, запад
        int biome = B_FOREST;
        double moist = 0.5;
    } reg;
};

// ---- поле без рек и поселений: высота (0..1), температура, влажность ----
inline double rawHeight(uint64_t s, int x, int y) {
    double h = 0.5 + 0.9 * fbm(s ^ 0x48, x / 300.0, y / 300.0, 5);
    const double m = std::clamp((h - 0.55) / 0.25, 0.0, 1.0);   // горы: гребни
    const double ridge = 1.0 - std::abs(fbm(s ^ 0x52, x / 160.0, y / 160.0, 4));
    h += m * m * ridge * 0.35;
    return std::clamp(h, 0.0, 1.0);
}
inline int levelOf(double h) { return std::clamp((int)std::floor(std::pow(h, 1.35) * 10.0), 0, 9); }
// высота участка по заданной местности: в центре — уровень клетки, к краю — плавно к середине между своим уровнем и
// уровнем соседа (тогда соседние участки сходятся на общей границе) и мелкая волна шума
inline int regionLevel(const Plan& p, int x, int y) {
    const Plan::Region& r = p.reg;
    const double S = std::max(1, p.size - 1), u = x / S, v = y / S;   // 0..1 с запада на восток и с севера на юг
    const double d[4] = {v, 1 - u, 1 - v, u};                           // расстояние до краёв: С, В, Ю, З
    double h = r.level;
    for (int k = 0; k < 4; k++) {
        const double wgt = std::max(0.0, 1.0 - 2.0 * d[k]);   // 1 на краю, 0 в середине
        h += wgt * wgt * (r.nb[k] - r.level) * 0.5;
    }
    h += 0.45 * fbm(p.seed ^ 0x5245, x / 40.0, y / 40.0, 3);
    return std::clamp((int)std::lround(h), 0, 9);
}
inline double rawTemp(uint64_t s, int x, int y, int level) { return 0.6 + 1.1 * fbm(s ^ 0x54, x / 320.0, y / 320.0, 3) - 0.03 * level; }
inline double rawMoist(uint64_t s, int x, int y) { return 0.5 + 1.1 * fbm(s ^ 0x4D, x / 260.0, y / 260.0, 3); }
inline int biomeOf(double t, double m) {
    const int ti = t < 0.36 ? 0 : (t > 0.64 ? 2 : 1), mi = m < 0.36 ? 0 : (m > 0.64 ? 2 : 1);
    return ti * 3 + mi;
}

inline double segDist2(const Seg& g, double px, double py) {
    const double dx = g.x1 - g.x0, dy = g.y1 - g.y0, L = dx * dx + dy * dy;
    double t = L > 0 ? ((px - g.x0) * dx + (py - g.y0) * dy) / L : 0;
    t = std::clamp(t, 0.0, 1.0);
    const double qx = g.x0 + t * dx - px, qy = g.y0 + t * dy - py;
    return qx * qx + qy * qy;
}
inline void indexSegs(Plan& p, const std::vector<Seg>& v, std::vector<std::vector<int>>& B) {
    B.assign((size_t)p.bn * p.bn, {});
    for (size_t i = 0; i < v.size(); i++) {
        const Seg& g = v[i];
        const int pad = g.w + 2;
        const int bx0 = std::max(0, (std::min(g.x0, g.x1) - pad) / Plan::BK), bx1 = std::min(p.bn - 1, (std::max(g.x0, g.x1) + pad) / Plan::BK);
        const int by0 = std::max(0, (std::min(g.y0, g.y1) - pad) / Plan::BK), by1 = std::min(p.bn - 1, (std::max(g.y0, g.y1) + pad) / Plan::BK);
        for (int by = by0; by <= by1; by++)
            for (int bx = bx0; bx <= bx1; bx++) B[(size_t)by * p.bn + bx].push_back((int)i);
    }
}

// ---- план мира: реки (по склону вниз от истоков), поселения (ровно, умеренно, у воды; крепости — на возвышенности),
// дороги (дерево кратчайших связей между поселениями). Плотность — на квадратный километр, одинаковая для любого размера.
inline void buildPlan(Plan& p, uint64_t seed, int size) {
    p = Plan();
    p.seed = seed;
    p.size = size;
    p.bn = size / Plan::BK + 1;
    const int G = 16, gn = size / G + 1;
    std::vector<float> gh((size_t)gn * gn);
    for (int j = 0; j < gn; j++)
        for (int i = 0; i < gn; i++) gh[(size_t)j * gn + i] = (float)rawHeight(seed, i * G, j * G);
    auto H = [&](int i, int j) { return gh[(size_t)j * gn + i]; };
    // реки: истоки высоко и во влажном; спуск по крутейшему склону сетки до низины или края
    const int nSrc = std::max(2, size * size / (260 * 260));
    for (int k = 0; k < nSrc; k++) {
        int i = (int)(hash3(seed, k, 1, 11) % gn), j = (int)(hash3(seed, k, 2, 11) % gn);
        if (H(i, j) < 0.55 || rawMoist(seed, i * G, j * G) < 0.45) continue;
        std::vector<std::pair<int, int>> pts{{i, j}};
        std::vector<char> seen((size_t)gn * gn, 0);
        seen[(size_t)j * gn + i] = 1;
        for (int step = 0; step < 4 * gn; step++) {
            int bi = -1, bj = -1;
            float bh = 1e9f;
            for (int dj = -1; dj <= 1; dj++)
                for (int di = -1; di <= 1; di++) {
                    const int ni = i + di, nj = j + dj;
                    if ((!di && !dj) || ni < 0 || nj < 0 || ni >= gn || nj >= gn || seen[(size_t)nj * gn + ni]) continue;
                    const float v = H(ni, nj) + 0.004f * (float)hash01(seed, ni, nj, 13);
                    if (v < bh) { bh = v; bi = ni; bj = nj; }
                }
            if (bi < 0) break;
            i = bi;
            j = bj;
            seen[(size_t)j * gn + i] = 1;
            pts.push_back({i, j});
            if (H(i, j) < 0.2f || i == 0 || j == 0 || i == gn - 1 || j == gn - 1) break;
        }
        if (pts.size() < 6) continue;
        for (size_t q = 1; q < pts.size(); q++) {
            const int w = 2 + (int)std::min<size_t>(3, q / 12);   // река шире к устью
            p.rivers.push_back({pts[q - 1].first * G + G / 2, pts[q - 1].second * G + G / 2, pts[q].first * G + G / 2, pts[q].second * G + G / 2, w});
        }
    }
    indexSegs(p, p.rivers, p.rivB);
    // поселения: оценка мест на сетке, жадный выбор с отступом
    struct Cand { double score; int x, y, kind; };
    std::vector<Cand> cs;
    const int CG = 48;
    for (int y = CG; y < size - CG; y += CG)
        for (int x = CG; x < size - CG; x += CG) {
            const double h = rawHeight(seed, x, y), t = rawTemp(seed, x, y, levelOf(h)), m = rawMoist(seed, x, y);
            double rough = 0;
            for (int d = -1; d <= 1; d += 2) rough += std::abs(rawHeight(seed, x + d * 24, y) - h) + std::abs(rawHeight(seed, x, y + d * 24) - h);
            if (h > 0.82) continue;
            double water = 1e9;
            const int b = (y / Plan::BK) * p.bn + x / Plan::BK;
            for (int si : p.rivB[(size_t)b]) water = std::min(water, std::sqrt(segDist2(p.rivers[si], x, y)));
            const bool fort = h > 0.55 && rough < 0.08;
            double sc = 1.0 - 6 * rough - 2.5 * std::abs(t - 0.5) - std::abs(m - 0.5) + (water < 90 ? 0.6 : 0) + 0.3 * hash01(seed, x, y, 17);
            if (fort) sc = 0.6 - 4 * rough + 0.4 * hash01(seed, x, y, 19);
            cs.push_back({sc, x, y, fort ? 2 : 0});
        }
    std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
    const int want = std::max(2, (int)std::lround(size * (double)size / (330.0 * 330.0)));
    for (const Cand& c : cs) {
        if ((int)p.towns.size() >= want || c.score < 0.15) break;
        bool far = true;
        for (const Settle& o : p.towns) far = far && std::hypot(o.x - c.x, o.y - c.y) > 2.2 * o.r + 60;
        if (!far) continue;
        const double u = hash01(seed, c.x, c.y, 23);
        int kind = c.kind, r;
        if (kind == 2) r = 22 + (int)(u * 16);   // крепость
        else if (size >= 3000 && u > 0.93) { kind = 3; r = 160 + (int)(hash01(seed, c.x, c.y, 29) * 140); }   // большой город — только в больших мирах
        else if (u > 0.55) { kind = 1; r = 45 + (int)(hash01(seed, c.x, c.y, 29) * 45); }
        else r = 16 + (int)(hash01(seed, c.x, c.y, 29) * 14);   // деревня
        if (c.x - r - 8 < 0 || c.y - r - 8 < 0 || c.x + r + 8 >= size || c.y + r + 8 >= size) continue;
        bool clash = false;
        for (const Settle& o : p.towns) clash = clash || std::hypot(o.x - c.x, o.y - c.y) < o.r + r + 40;
        if (clash) continue;
        p.towns.push_back({c.x, c.y, r, kind, levelOf(rawHeight(seed, c.x, c.y))});
    }
    // дороги: дерево кратчайших связей (Прим)
    const size_t nt = p.towns.size();
    std::vector<char> in(nt, 0);
    if (nt) in[0] = 1;
    for (size_t added = 1; added < nt; added++) {
        double best = 1e18;
        int a = -1, b = -1;
        for (size_t i = 0; i < nt; i++)
            if (in[i])
                for (size_t j = 0; j < nt; j++)
                    if (!in[j]) {
                        const double d = std::hypot(p.towns[i].x - p.towns[j].x, p.towns[i].y - p.towns[j].y);
                        if (d < best) { best = d; a = (int)i; b = (int)j; }
                    }
        if (b < 0) break;
        in[b] = 1;
        // дорога — ломаная с лёгким изгибом в середине
        const Settle &A = p.towns[a], &B = p.towns[b];
        const int mx = (A.x + B.x) / 2 + (int)((hash01(seed, a, b, 31) - 0.5) * best * 0.25), my = (A.y + B.y) / 2 + (int)((hash01(seed, a, b, 37) - 0.5) * best * 0.25);
        p.roads.push_back({A.x, A.y, mx, my, 3});
        p.roads.push_back({mx, my, B.x, B.y, 3});
    }
    indexSegs(p, p.roads, p.roadB);
    p.townB.assign((size_t)p.bn * p.bn, {});
    for (size_t i = 0; i < p.towns.size(); i++) {
        const Settle& t = p.towns[i];
        const int pad = t.r + 12;
        for (int by = std::max(0, (t.y - pad) / Plan::BK); by <= std::min(p.bn - 1, (t.y + pad) / Plan::BK); by++)
            for (int bx = std::max(0, (t.x - pad) / Plan::BK); bx <= std::min(p.bn - 1, (t.x + pad) / Plan::BK); bx++) p.townB[(size_t)by * p.bn + bx].push_back((int)i);
    }
}

// ---- клетка мира ----
struct Cell {
    bool wall = false, climb = false, diff = false, dark = false, pit = false, water = false, tree = false, ladder = false, strong = false, town = false, road = false;
    unsigned char height = 0, haz = 0, deck = 0, door = 0;   // door: 0 нет, 1 закрыта, 2 открыта
    unsigned char biome = 0;
};

inline double nearSeg(const Plan& p, const std::vector<Seg>& v, const std::vector<std::vector<int>>& B, int x, int y, int* wOut = nullptr) {
    if (x < 0 || y < 0 || x >= p.size || y >= p.size) return 1e9;
    double best = 1e18;
    const int b = (y / Plan::BK) * p.bn + x / Plan::BK;
    for (int si : B[(size_t)b]) {
        const double d = segDist2(v[si], x, y);
        if (d < best) {
            best = d;
            if (wOut) *wOut = v[si].w;
        }
    }
    return std::sqrt(best);
}
inline const Settle* townAt(const Plan& p, int x, int y, double* dist = nullptr) {
    if (x < 0 || y < 0 || x >= p.size || y >= p.size) return nullptr;
    const Settle* best = nullptr;
    double bd = 1e18;
    for (int ti : p.townB[(size_t)((y / Plan::BK) * p.bn + x / Plan::BK)]) {
        const Settle& t = p.towns[ti];
        const double d = std::hypot(x - t.x, y - t.y) - t.r;
        if (d < bd) { bd = d; best = &t; }
    }
    if (dist) *dist = bd;
    return best;
}

// кэш уровней земли на вырезаемом участке (cutArena заполняет до клеток мира)
struct LevelCache {
    const Plan* p = nullptr;
    int x0 = 0, y0 = 0, w = 0, h = 0;
    std::vector<signed char> v;
};
inline LevelCache& lvlCache() {
    static thread_local LevelCache c;
    return c;
}
int groundLevelRaw(const Plan& p, int x, int y);
inline int groundLevel(const Plan& p, int x, int y) {
    const LevelCache& c = lvlCache();
    if (c.p == &p && x >= c.x0 && y >= c.y0 && x < c.x0 + c.w && y < c.y0 + c.h) return c.v[(size_t)(y - c.y0) * c.w + (x - c.x0)];
    return groundLevelRaw(p, x, y);
}
inline void fillLevelCache(const Plan& p, int x0, int y0, int w, int h) {
    LevelCache& c = lvlCache();
    c.p = nullptr;
    c.x0 = x0 - 1;
    c.y0 = y0 - 1;
    c.w = w + 2;
    c.h = h + 2;
    c.v.resize((size_t)c.w * c.h);
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) c.v[(size_t)y * c.w + x] = (signed char)groundLevelRaw(p, c.x0 + x, c.y0 + y);
    c.p = &p;
}
// уровень земли с поселениями: внутри — ровная площадка, на кольце 12 клеток — плавно к окружающему рельефу
inline int groundLevelRaw(const Plan& p, int x, int y) {
    int lv = p.reg.on ? regionLevel(p, x, y) : levelOf(rawHeight(p.seed, x, y));
    double d;
    const Settle* t = townAt(p, x, y, &d);
    if (t && d < 12) {
        if (d <= 0) return t->level;
        const double k = d / 12.0;
        lv = (int)std::lround(t->level * (1 - k) + lv * k);
    }
    // уступы со скальным краем: на холмах контур второго шума поднимает землю сразу на 2 уровня (обрыв)
    if (lv >= 3 && !t && !p.reg.on) {   // на участке по заданной местности перепады задаёт описание
        const double c = fbm(p.seed ^ 0x43, x / 70.0, y / 70.0, 3);
        if (c > 0.28) lv = std::min(9, lv + 2);
    }
    return lv;
}

// постройки поселения: кварталы между улицами, в квартале — дом (стены, дверь, иногда второй этаж с лестницей или зал
// с потолком); у города и крепости — стена по кругу с башнями и воротами на дорогах
inline bool townCell(const Plan& p, const Settle& t, int x, int y, Cell& c) {
    const double d = std::hypot(x - t.x, y - t.y);
    const uint64_t s = p.seed ^ (uint64_t)(t.x * 7919 + t.y);
    const int lv = t.level;
    if (t.kind == 0 && !p.reg.on && d > t.r) return false;   // деревня без стены — до своего радиуса
    if (t.kind == 0 && p.reg.on) {   // участок по местности: квартал деревни целиком (по его середине), без обрезков домов
        const int B = 18, lx = x - t.x + 10000 * B, ly = y - t.y + 10000 * B;
        const double cx = (lx / B) * B + B / 2.0 - 10000 * B, cy = (ly / B) * B + B / 2.0 - 10000 * B;
        if (std::hypot(cx, cy) > t.r) return false;
    }
    c.town = true;
    if (t.kind != 0) {   // городская стена (кольцо толщиной 2) с башнями и воротами
        const double rw = t.r + 1;
        if (d >= rw && d < rw + 2) {
            int rw2 = 0;
            if (nearSeg(p, p.roads, p.roadB, x, y, &rw2) <= 2.5) { c.door = 2; return true; }   // ворота — открытые створки
            c.wall = true;
            c.strong = t.kind == 2;
            return true;
        }
        const double ang = std::atan2(y - t.y, x - t.x);
        const int towers = std::max(6, t.r / 10);
        const double seg = 6.283185307179586 / towers, a0 = std::round(ang / seg) * seg;
        const double tx = t.x + std::cos(a0) * (rw + 1), ty = t.y + std::sin(a0) * (rw + 1);
        if (std::abs(x - tx) <= 2.5 && std::abs(y - ty) <= 2.5 && d >= rw - 3) {   // башня 5×5: стены и площадка наверху
            const bool edge = std::abs(x - tx) > 1.5 || std::abs(y - ty) > 1.5;
            if (edge) { c.wall = true; c.strong = true; }
            else {
                c.deck = (unsigned char)std::min(9, lv + 2);
                if ((int)std::lround(x - tx) == -1 && (int)std::lround(y - ty) == -1) c.ladder = true;
            }
            return true;
        }
        if (d >= rw) { c.town = false; return false; }
    }
    const int B = t.kind == 0 ? 18 : (t.kind == 2 ? 16 : 14), S = t.kind == 0 ? 4 : 3;   // квартал и ширина улицы
    const int lx = x - t.x + 10000 * B, ly = y - t.y + 10000 * B, bx = lx / B, by = ly / B, ix = lx % B, iy = ly % B;
    if (ix < S || iy < S) return true;   // улица — чистая
    if (d < t.r * 0.18 && t.kind != 0) {   // площадь: колодец и повозки
        c.climb = hash01(s, x, y, 41) < 0.02;
        return true;
    }
    const double u = hash01(s, bx, by, 43);
    if (t.kind == 0 && u < 0.45) {   // деревня: огород с редкими деревьями
        c.tree = hash01(s, x, y, 47) < 0.08;
        return true;
    }
    // дом: прямоугольник в квартале
    const int inner = B - S, w = std::max(5, inner - 1 - (int)(hash01(s, bx, by, 53) * 3)), h = std::max(5, inner - 1 - (int)(hash01(s, bx, by, 59) * 3));
    const int ox = S + (int)(hash01(s, bx, by, 61) * (inner - w)), oy = S + (int)(hash01(s, bx, by, 67) * (inner - h));
    const int qx = ix - ox, qy = iy - oy;
    if (qx < 0 || qy < 0 || qx >= w || qy >= h) {
        c.climb = hash01(s, x, y, 71) < 0.015;   // бочки и ящики у домов
        return true;
    }
    const bool border = qx == 0 || qy == 0 || qx == w - 1 || qy == h - 1;
    if (border) {
        const int side = (int)(hash01(s, bx, by, 73) * 4), mid = side < 2 ? w / 2 : h / 2;   // дверь посередине стороны
        const bool door = (side == 0 && qy == 0 && qx == mid) || (side == 1 && qy == h - 1 && qx == mid) || (side == 2 && qx == 0 && qy == mid) ||
                          (side == 3 && qx == w - 1 && qy == mid);
        if (door) c.door = hash01(s, bx, by, 79) < 0.65 ? 1 : 2;
        else {
            c.wall = true;
            c.strong = t.kind == 2 && d < t.r * 0.35;   // донжон крепости
        }
        return true;
    }
    const double v = hash01(s, bx, by, 83);
    const bool hall = v < (t.kind == 2 ? 0.35 : 0.12), twoFloors = !hall && v < (t.kind == 0 ? 0.3 : 0.7);
    if (hall) c.deck = (unsigned char)std::min(9, lv + 2);   // зал: потолок на 20 фт
    else if (twoFloors) {
        c.deck = (unsigned char)std::min(9, lv + 1);   // второй этаж (пол — потолок первого) и лестница в углу
        if (qx == 1 && qy == 1) c.ladder = true;
    }
    c.climb = !c.ladder && hash01(s, x, y, 89) < 0.03;   // мебель — укрытие
    return true;
}

inline Cell worldCell(const Plan& p, int x, int y) {
    Cell c;
    if (x < 0 || y < 0 || x >= p.size || y >= p.size) {
        c.wall = true;
        return c;
    }
    const uint64_t s = p.seed;
    const int lv = groundLevel(p, x, y);
    c.height = (unsigned char)lv;
    double rd = 1e9;
    int rw = 0;
    rd = nearSeg(p, p.rivers, p.rivB, x, y, &rw);
    int roadW = 0;
    const double roadD = nearSeg(p, p.roads, p.roadB, x, y, &roadW);
    const double t = rawTemp(s, x, y, lv), m = p.reg.on ? p.reg.moist : std::clamp(rawMoist(s, x, y) + (rd < 40 ? 0.15 * (1 - rd / 40) : 0), -1.0, 2.0);
    c.biome = (unsigned char)(p.reg.on ? p.reg.biome : biomeOf(t, m));
    const bool road = roadD <= roadW / 2.0;
    // обрыв: соседняя клетка на 2+ уровня ниже — на краю иногда лестница (на дороге — всегда: ступени)
    bool cliffEdge = false;
    for (int k = 0; k < 4 && !cliffEdge; k++) {
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        if (lv - groundLevel(p, x + DX[k], y + DY[k]) >= 2) cliffEdge = true;
    }
    if (cliffEdge && (road || hash01(s, x, y, 97) < 0.06)) c.ladder = true;
    double td;
    const Settle* town = townAt(p, x, y, &td);
    if (town && td <= (p.reg.on ? 13 : 3) && townCell(p, *town, x, y, c)) return c;   // участок: кварталы деревни целиком
    if (rd <= rw / 2.0 && !road) {   // река: ручей (труднопроходимо); дорога через реку — мост
        c.water = c.diff = true;
        return c;
    }
    if (road) { c.road = true; return c; }   // дорога: чистая
    const double clump = fbm(s ^ 0x6C, x / 18.0, y / 18.0, 2), r = hash01(s, x, y, 101), r2 = hash01(s, x, y, 103);
    // ущелья: узкие полосы в сухом и жарком на средней высоте
    if (lv >= 2 && lv <= 6 && m < 0.38 && std::abs(fbm(s ^ 0x55, x / 220.0, y / 220.0, 3)) < 0.008) {
        c.pit = true;
        return c;
    }
    if (lv <= 1 && m > 0.72 && fbm(s ^ 0x4C, x / 60.0, y / 60.0, 2) > 0.3) {   // озеро в сырой низине
        c.water = c.diff = true;
        return c;
    }
    switch (c.biome) {
        case B_TUNDRA: c.climb = r < 0.03; if (clump > 0.55 && r2 < 0.5) c.haz = 4; break;
        case B_TAIGA: c.tree = r < 0.10 + 0.25 * std::max(0.0, clump); c.diff = !c.tree && clump < -0.3; break;
        case B_GLACIER: if (clump > 0.1) c.haz = 4; c.diff = true; c.climb = r < 0.01; break;
        case B_STEPPE: c.climb = r < 0.012; c.tree = r2 < 0.015; if (r < 0.003) c.wall = true; break;   // редкие руины
        case B_FOREST: c.tree = r < (clump > 0.25 ? 0.42 : 0.04); c.dark = clump > 0.6; break;
        case B_SWAMP: c.diff = r < 0.55; c.tree = r2 < 0.08; if (clump > 0.5) c.haz = r < 0.5 ? 3 : 6; else if (clump < -0.55) c.water = c.diff = true; break;
        case B_DESERT: c.diff = clump > 0.2; c.climb = r < 0.02; if (r2 < 0.002) c.wall = true; if (lv >= 6 && clump > 0.45) c.haz = 1; break;   // лава у вулканов
        case B_SAVANNA: c.tree = r < 0.035; c.climb = r2 < 0.006; break;
        case B_JUNGLE: c.tree = r < 0.45 + 0.2 * clump; c.diff = !c.tree && r2 < 0.25; c.dark = clump > 0.5; if (clump < -0.5 && r2 < 0.6) c.haz = 2; break;
    }
    if (lv >= 6) {   // горы: скалы; во влажных — грозовые поля
        c.climb = c.climb || r < 0.05;
        if (m > 0.6 && clump > 0.55) c.haz = 5;
    }
    if (c.tree) c.climb = false;
    if (c.haz) c.tree = c.climb = c.wall = false;
    // навес: клетка под краем скалы (сосед выше на 2+) — иногда настил на уровне скалы
    if (!c.wall && !c.tree)
        for (int k = 0; k < 4; k++) {
            static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            const int up = groundLevel(p, x + DX[k], y + DY[k]);
            if (up - lv >= 2 && fbm(s ^ 0x4E, x / 9.0, y / 9.0, 1) > 0.35) { c.deck = (unsigned char)up; break; }
        }
    return c;
}

// ---- участок по заданной местности (кампания, задача Б-1) ----
// Описание клетки региона (1000 × 1000 фт = 200 × 200 клеток сцены): местность или биом, уровень высоты и уровни
// четырёх соседей, дорога (через какие края проходит), река, поселение, зерно. Тот же вход — тот же участок.
struct RegionDesc {
    std::string terrain;          // «Лес», «Горы», «Болото», … — если биом не задан числом
    int biome = -1;               // номер биома (Biome), −1 — по названию местности
    int level = 2, nb[4] = {2, 2, 2, 2};   // уровни: клетка; соседи — север, восток, юг, запад
    std::string road;             // края, через которые идёт дорога: буквы N, E, S, W (одна — сквозь участок)
    bool river = false;
    std::string settle;           // вид поселения: хутор, деревня, село, город, крепость (замок), большой город, великий город
    int pop = 0;                  // жителей (размер поселения; 0 — по виду)
    std::string seed = "1";
};
inline uint64_t seedOfText(const std::string& t) {   // FNV-1a: зерно клетки из строки
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char ch : t) h = (h ^ ch) * 1099511628211ULL;
    return mixh(h, 0x52454731) | 1;
}
inline int biomeOfTerrain(const std::string& t) {
    struct T { const char* key; int b; };
    static const T TAB[] = {{"тундр", B_TUNDRA}, {"тайг", B_TAIGA}, {"ледн", B_GLACIER}, {"снег", B_GLACIER}, {"степ", B_STEPPE}, {"равн", B_STEPPE},
                            {"луг", B_STEPPE}, {"пашн", B_STEPPE}, {"пол", B_STEPPE}, {"холм", B_STEPPE}, {"гор", B_STEPPE}, {"лес", B_FOREST},
                            {"болот", B_SWAMP}, {"топ", B_SWAMP}, {"пустын", B_DESERT}, {"саванн", B_SAVANNA}, {"джунгл", B_JUNGLE}};
    std::string low;
    for (size_t i = 0; i < t.size(); i++) low += t[i];
    // строчные русские буквы: «Л» (D0 9B) → «л» (D0 BB) и т. п.; латиница — tolower
    std::string q;
    for (size_t i = 0; i < low.size(); i++) {
        unsigned char c = (unsigned char)low[i];
        if (c == 0xD0 && i + 1 < low.size()) {
            unsigned char d = (unsigned char)low[i + 1];
            if (d >= 0x90 && d <= 0x9F) { q += (char)0xD0; q += (char)(d + 0x20); i++; continue; }
            if (d >= 0xA0 && d <= 0xAF) { q += (char)0xD1; q += (char)(d - 0x20); i++; continue; }
            if (d == 0x81) { q += (char)0xD1; q += (char)0x91; i++; continue; }
        }
        q += (char)std::tolower(c);
    }
    for (const T& e : TAB)
        if (q.find(e.key) != std::string::npos) return e.b;
    return B_FOREST;
}
inline void buildRegionPlan(Plan& p, const RegionDesc& d, int S = 200) {
    p = Plan();
    p.seed = seedOfText(d.seed);
    p.size = S;
    p.bn = S / Plan::BK + 1;
    p.reg.on = true;
    p.reg.level = std::clamp(d.level, 0, 9);
    for (int k = 0; k < 4; k++) p.reg.nb[k] = std::clamp(d.nb[k], 0, 9);
    p.reg.biome = d.biome >= 0 && d.biome < B_N ? d.biome : biomeOfTerrain(d.terrain);
    static const double MOIST[B_N] = {0.35, 0.55, 0.45, 0.30, 0.50, 0.85, 0.15, 0.30, 0.80};   // как в среднем у биома
    p.reg.moist = MOIST[p.reg.biome];
    const int c = S / 2;
    const uint64_t s = p.seed;
    // поселение в середине участка
    if (!d.settle.empty() || d.pop > 0) {
        const std::string& k = d.settle;
        // радиус по числу жителей (без числа — обычное для вида): деревня на 200 — ~25 клеток, город на 3000 — ~52
        auto has = [&](const char* w) { return k.find(w) != std::string::npos; };
        int kind = 0, pop = d.pop;
        double r0 = 10, k1 = 1.1;
        if (has("хутор")) { r0 = 6; k1 = 0.8; if (!pop) pop = 15; }
        else if (has("крепост") || has("замок")) { kind = 2; r0 = 22; k1 = 0.6; if (!pop) pop = 300; }
        else if (has("велик") || has("столиц")) { kind = 3; r0 = c; k1 = 0; }
        else if (has("больш")) { kind = 3; r0 = 40; k1 = 0.4; if (!pop) pop = 20000; }
        else if (has("город")) { kind = 1; r0 = 25; k1 = 0.5; if (!pop) pop = 3000; }
        else if (!pop) pop = 200;   // деревня, село и всё прочее
        const int r = std::clamp((int)std::lround(r0 + k1 * std::sqrt((double)pop)), 5, c - 6);
        p.towns.push_back({c, c, r, kind, p.reg.level});
    }
    // дорога: от каждого названного края к середине (одна буква — насквозь, к противоположному краю)
    auto edgePt = [&](char e, int& x, int& y) {
        switch (e) {
            case 'N': case 'n': x = c; y = 0; return true;
            case 'S': case 's': x = c; y = S - 1; return true;
            case 'E': case 'e': x = S - 1; y = c; return true;
            case 'W': case 'w': x = 0; y = c; return true;
        }
        return false;
    };
    std::string ends;
    for (char e : d.road)
        if (std::string("NESWnesw").find(e) != std::string::npos) ends += (char)std::toupper((unsigned char)e);
    if (ends.size() == 1) ends += ends[0] == 'N' ? 'S' : ends[0] == 'S' ? 'N' : ends[0] == 'E' ? 'W' : 'E';
    for (size_t i = 0; i < ends.size(); i++) {
        int x, y;
        if (!edgePt(ends[i], x, y)) continue;
        const int mx = (x + c) / 2 + (int)((hash01(s, (int)i, 1, 41) - 0.5) * S * 0.15), my = (y + c) / 2 + (int)((hash01(s, (int)i, 2, 41) - 0.5) * S * 0.15);
        p.roads.push_back({x, y, mx, my, 3});
        p.roads.push_back({mx, my, c, c, 3});
    }
    // река: поперёк участка, в стороне от середины; поперёк дороги, если она есть
    if (d.river) {
        const bool ns = !ends.empty() ? (ends[0] == 'E' || ends[0] == 'W') : hash01(s, 3, 3, 43) < 0.5;   // течёт с севера на юг
        const int away = p.towns.empty() ? 0 : p.towns[0].r + 14;   // в стороне от поселения (и от его кварталов)
        const int off = (int)((hash01(s, 4, 4, 43) < 0.5 ? -1 : 1) * std::min(S / 2 - 8.0, std::max((double)away, S * (0.22 + 0.1 * hash01(s, 5, 5, 43)))));
        const int sway = away ? 0 : 1;   // у поселения река прямее, чтобы не задеть кварталы
        const int a = c + off, b = c + off + sway * (int)((hash01(s, 6, 6, 43) - 0.5) * S * 0.3);
        const int m1 = (a + b) / 2 + sway * (int)((hash01(s, 7, 7, 43) - 0.5) * S * 0.2);
        if (ns) { p.rivers.push_back({a, 0, m1, c, 3}); p.rivers.push_back({m1, c, b, S - 1, 3}); }
        else { p.rivers.push_back({0, a, c, m1, 3}); p.rivers.push_back({c, m1, S - 1, b, 3}); }
    }
    indexSegs(p, p.rivers, p.rivB);
    indexSegs(p, p.roads, p.roadB);
    p.townB.assign((size_t)p.bn * p.bn, {});
    for (size_t i = 0; i < p.towns.size(); i++) {
        const Settle& t = p.towns[i];
        const int pad = t.r + 12;
        for (int by = std::max(0, (t.y - pad) / Plan::BK); by <= std::min(p.bn - 1, (t.y + pad) / Plan::BK); by++)
            for (int bx = std::max(0, (t.x - pad) / Plan::BK); bx <= std::min(p.bn - 1, (t.x + pad) / Plan::BK); bx++) p.townB[(size_t)by * p.bn + bx].push_back((int)i);
    }
}
}   // namespace wg
