// ============================================================================
// Кампания: мир (ТЗ docs/TZ_kampaniya_v2.md §2–3). Свой генератор региона (Р27), не world_gen.
//
// Координаты — километры от северо-западного угла (x — на восток, y — на юг). Клетка региона — 1000 фт (≈ 305 м,
// 200 × 200 клеток сцены, Р29); мир до 5000 × 5000 км. Целиком мир не хранится: клетка считается по запросу из
// непрерывных формул (высота, климат) и грубого плана (реки, города, дороги), поэтому обзор и подробный вид совпадают.
// Обзор уровня z — клетки 2^z × 2^z базовых, куски 256 × 256 клеток.
//
// Миры: GenWorld — по параметрам; ActWorld — готовая сетка Акта I (web/data/region_act1.json, долгота/широта).
// ============================================================================
#pragma once
#include <algorithm>
#include <memory>
#include <queue>
#include "kmp_core.h"

namespace kmp {

const double CELL_KM = 0.3048;   // клетка региона: 1000 фт
const int TILE = 256;            // кусок: 256 × 256 клеток

struct WCell {
    int terr = TERR_FIELDS;      // индекс terrains(); 7 — вода
    int level = 0;               // высота 0–9 (по 10 фт, как у сцены)
    int biome = 4;               // биом как в world_gen: 0 тундра … 8 джунгли
    bool road = false, river = false;
    int town = -1;               // индекс поселения
};
struct Town {
    std::string name, kind;      // хутор … великий город
    double x = 0, y = 0;         // км
    int pop = 0;
    double r = 0.3;              // радиус, км
};
struct World {
    std::string name;
    double w = 0, h = 0;         // км
    std::vector<Town> towns;
    virtual ~World() {}
    virtual WCell at(double x, double y, double tol) const = 0;   // tol — полклетки обзора: мелкое видно и издали
    virtual bool snapRoad(double& x, double& y, double maxd) const { (void)x, (void)y, (void)maxd; return false; }   // точку — на ось дороги
    int cellsW() const { return (int)std::ceil(w / CELL_KM); }
    int cellsH() const { return (int)std::ceil(h / CELL_KM); }
    int zMax() const {   // уровень, на котором весь мир помещается в один-два куска
        int z = 0;
        while (std::max(cellsW(), cellsH()) > (TILE << z)) z++;
        return z;
    }
};

// ---- шум: градиентный, свой (детерминирован по зерну) ----
inline double grad2(uint64_t s, int ix, int iy, double dx, double dy) {   // градиент — один из 16 направлений по хешу узла
    static const struct G16 { double c[16], s[16]; G16() { for (int k = 0; k < 16; k++) c[k] = std::cos(k * M_PI / 8), s[k] = std::sin(k * M_PI / 8); } } G;
    const uint64_t hsh = smix(s ^ ((uint64_t)(uint32_t)ix * 0x9E3779B1ull) ^ ((uint64_t)(uint32_t)iy << 32));
    const int k = (int)(hsh >> 60);
    return G.c[k] * dx + G.s[k] * dy;
}
inline double noise2(uint64_t s, double x, double y) {
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const double fx = x - ix, fy = y - iy, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    const double a = grad2(s, ix, iy, fx, fy), b = grad2(s, ix + 1, iy, fx - 1, fy), c = grad2(s, ix, iy + 1, fx, fy - 1),
                 d = grad2(s, ix + 1, iy + 1, fx - 1, fy - 1);
    return (a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v) * 1.4;   // ≈ −1…1
}
inline double fbm2(uint64_t s, double x, double y, int oct) {
    double t = 0, amp = 1, norm = 0;
    for (int o = 0; o < oct; o++) t += amp * noise2(s + o * 0x51ED27ull, x, y), norm += amp, amp *= 0.5, x *= 2.03, y *= 2.03;
    return t / norm;
}
inline int biomeOfTM(double t, double m) {   // как world_gen: холод/умеренно/жара × сухо/средне/сыро
    const int ti = t < 0.36 ? 0 : (t > 0.64 ? 2 : 1), mi = m < 0.36 ? 0 : (m > 0.64 ? 2 : 1);
    return ti * 3 + mi;
}
inline const char* biomeName(int b) {
    static const char* N[9] = {"тундра", "тайга", "ледник", "степь", "лес и поля", "болото", "пустыня", "саванна", "джунгли"};
    return N[std::clamp(b, 0, 8)];
}

// ---- отрезки (реки, дороги) с корзинами для быстрого поиска ближайшего ----
struct Seg {
    double x0, y0, x1, y1, w;   // км; w — полуширина
};
struct SegIndex {
    std::vector<Seg> segs;
    double bk = 10;   // сторона корзины, км
    int bw = 0, bh = 0;
    std::vector<std::vector<int>> B;
    void build(double W, double H, double bucket) {
        bk = bucket, bw = (int)(W / bk) + 1, bh = (int)(H / bk) + 1;
        B.assign((size_t)bw * bh, {});
        for (size_t i = 0; i < segs.size(); i++) {
            const Seg& g = segs[i];
            const int x0 = std::max(0, (int)((std::min(g.x0, g.x1) - g.w) / bk)), x1 = std::min(bw - 1, (int)((std::max(g.x0, g.x1) + g.w) / bk));
            const int y0 = std::max(0, (int)((std::min(g.y0, g.y1) - g.w) / bk)), y1 = std::min(bh - 1, (int)((std::max(g.y0, g.y1) + g.w) / bk));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) B[(size_t)y * bw + x].push_back((int)i);
        }
    }
    bool nearest(double& x, double& y, double maxd) const {   // ближайшая точка отрезков не дальше maxd — вместо (x, y)
        if (B.empty()) return false;
        const int r = (int)std::ceil(maxd / bk), cx = (int)(x / bk), cy = (int)(y / bk);
        double best = maxd * maxd, bx = x, by = y;
        bool hit = false;
        for (int yy = std::max(0, cy - r); yy <= std::min(bh - 1, cy + r); yy++)
            for (int xx = std::max(0, cx - r); xx <= std::min(bw - 1, cx + r); xx++)
                for (int i : B[(size_t)yy * bw + xx]) {
                    const Seg& g = segs[i];
                    const double dx = g.x1 - g.x0, dy = g.y1 - g.y0, L = dx * dx + dy * dy;
                    const double t = std::clamp(L > 0 ? ((x - g.x0) * dx + (y - g.y0) * dy) / L : 0.0, 0.0, 1.0);
                    const double qx = g.x0 + t * dx, qy = g.y0 + t * dy, d = (qx - x) * (qx - x) + (qy - y) * (qy - y);
                    if (d < best) best = d, bx = qx, by = qy, hit = true;
                }
        if (hit) x = bx, y = by;
        return hit;
    }
    bool near(double x, double y, double tol) const {   // точка ближе полуширины (не меньше tol) к какому-нибудь отрезку
        if (B.empty()) return false;
        const int r = (int)std::ceil(tol / bk);
        const int cx = (int)(x / bk), cy = (int)(y / bk);
        for (int by = std::max(0, cy - r); by <= std::min(bh - 1, cy + r); by++)
            for (int bx = std::max(0, cx - r); bx <= std::min(bw - 1, cx + r); bx++)
                for (int i : B[(size_t)by * bw + bx]) {
                    const Seg& g = segs[i];
                    const double dx = g.x1 - g.x0, dy = g.y1 - g.y0, L = dx * dx + dy * dy;
                    double t = L > 0 ? ((x - g.x0) * dx + (y - g.y0) * dy) / L : 0;
                    t = std::clamp(t, 0.0, 1.0);
                    const double qx = g.x0 + t * dx - x, qy = g.y0 + t * dy - y, lim = std::max(g.w, tol);
                    if (qx * qx + qy * qy <= lim * lim) return true;
                }
        return false;
    }
};

// ---- имена поселений по зерну ----
inline std::string townName(uint64_t s) {
    static const char* A[] = {"Бел", "Чер", "Кра", "Ост", "Вер", "Мир", "Сла", "Тор", "Гра", "Лис", "Дуб", "Ряб", "Зор", "Кам", "Вол", "Сос", "Бор", "Яр", "Ол", "Рад"};
    static const char* B[] = {"о", "а", "е", "и", "у", ""};
    static const char* C[] = {"град", "дол", "мост", "поль", "бор", "вец", "горье", "лесье", "брод", "стан", "холм", "ричь", "мир", "ль", "нск"};
    return std::string(A[smix(s) % 20]) + B[smix(s ^ 1) % 6] + C[smix(s ^ 2) % 15];
}

// ---- мир по параметрам (§3.1) ----
struct GenParams {
    double sizeKm = 800;   // сторона, км (до 5000)
    uint64_t seed = 1;
    double sea = 0.3;      // доля воды 0…0,9
    double mount = 0.5;    // гористость 0…1
    double rough = 0.5;    // изрезанность (мелкий рельеф, берег) 0…1
    double climate = 0;    // −1 холодно … +1 жарко
    double humid = 0;      // −1 сухо … +1 сыро
    double towns = 1;      // поселений на 100 × 100 км
    double rivers = 1;     // густота рек 0…2
    // карта высот (ТЗ v2 §3.1): серое 0…255, строка за строкой с севера; мир — hw × hh пикселей по kmpp км (до 5000 км
    // по стороне); hmSea — порог моря 0…1 (меньше нуля — по доле моря sea)
    std::vector<uint8_t> hmap;
    int hw = 0, hh = 0;
    double kmpp = 1, hmSea = -1;
};

struct GenWorld : World {
    GenParams p;
    int P = 0;              // сторона плана, клеток
    double pk = 1;          // км на клетку плана
    double seaH = 0, topH = 1;
    std::vector<float> ph;  // высота плана
    SegIndex rivers, roads;
    std::vector<std::vector<int>> townB;   // корзины поселений (сторона tbk)
    int tbw = 0;
    double tbk = 20;

    // непрерывные поля: высота (сырая), температура, влажность, «лес среди полей»
    // tol — полклетки обзора: детали мельче её не считаются (иначе обзор рябит), высота подробного вида та же
    double rawH(double x, double y, double tol = 0) const {
        const double S = p.sizeKm;
        if (!p.hmap.empty()) {   // карта высот: серое между пикселями — плавно, поверх — мелкий рельеф, чтобы вблизи не было ступенек
            if (x < 0 || y < 0 || x > w || y > h) return -1;
            const double u = std::clamp(x / p.kmpp - 0.5, 0.0, p.hw - 1.0), v = std::clamp(y / p.kmpp - 0.5, 0.0, p.hh - 1.0);
            const int i = std::min((int)u, p.hw - 2 < 0 ? 0 : p.hw - 2), j = std::min((int)v, p.hh - 2 < 0 ? 0 : p.hh - 2);
            const double fu = u - i, fv = v - j;
            auto g = [&](int a, int b) { return p.hmap[(size_t)std::min(b, p.hh - 1) * p.hw + std::min(a, p.hw - 1)] / 255.0; };
            const double base = (g(i, j) * (1 - fu) + g(i + 1, j) * fu) * (1 - fv) + (g(i, j + 1) * (1 - fu) + g(i + 1, j + 1) * fu) * fv;
            const double kh = std::clamp(1.5 - tol / 8.0, 0.0, 1.0), kf = std::clamp(1.5 - tol / 1.0, 0.0, 1.0);
            return base + (0.01 + 0.025 * p.rough) * fbm2(p.seed ^ 0x4B, x / 25.0, y / 25.0, 4) * kh + 0.005 * fbm2(p.seed ^ 0xF1, x / 3.0, y / 3.0, 3) * kf;
        }
        double c = fbm2(p.seed ^ 0xC0, x / (S * 0.45) + 13.1, y / (S * 0.45) + 7.7, 4);
        const double ridge = 1 - std::abs(fbm2(p.seed ^ 0x51, x / std::max(60.0, S * 0.12), y / std::max(60.0, S * 0.12), 4));
        const double hill = fbm2(p.seed ^ 0x4B, x / 25.0, y / 25.0, 4), fine = fbm2(p.seed ^ 0xF1, x / 3.0, y / 3.0, 3);
        const double up = std::clamp((c + 0.1) * 2.5, 0.0, 1.0);   // горы — на суше, не в море
        const double kh = std::clamp(1.5 - tol / 8.0, 0.0, 1.0), kf = std::clamp(1.5 - tol / 1.0, 0.0, 1.0);
        return c + p.mount * up * ridge * ridge * 0.55 + (0.04 + 0.08 * p.rough) * hill * kh + 0.012 * fine * kf;
    }
    int levelOf(double H) const {
        if (H < seaH) return 0;
        const double k = std::clamp((H - seaH) / std::max(1e-9, topH - seaH), 0.0, 1.0);
        return std::clamp((int)std::floor(std::pow(k, 1.8) * 10), 0, 9);   // больше низин: холмы (4+) и горы (6+) — верхняя треть суши
    }
    double temp(double x, double y, int level) const {
        return 0.5 + 0.28 * p.climate + 0.32 * (y / std::max(1.0, h) - 0.5) - 0.045 * level + 0.12 * fbm2(p.seed ^ 0x7E, x / (p.sizeKm * 0.3), y / (p.sizeKm * 0.3), 3);
    }
    double moist(double x, double y) const { return 0.5 + 0.22 * p.humid + 0.35 * fbm2(p.seed ^ 0x3D, x / (p.sizeKm * 0.22), y / (p.sizeKm * 0.22), 4); }

    void build() {
        if (!p.hmap.empty()) {   // мир по карте высот: стороны — по картинке, не больше 5000 км
            p.kmpp = std::clamp(p.kmpp, 0.05, 5000.0 / std::max(p.hw, p.hh));
            w = p.hw * p.kmpp, h = p.hh * p.kmpp, p.sizeKm = std::max(w, h);
        } else w = h = p.sizeKm = std::clamp(p.sizeKm, 10.0, 5000.0);
        const double S = std::max(w, h);   // план — квадрат на большую сторону; что за краем — море
        P = std::max(32, std::min(512, (int)std::ceil(S / CELL_KM)));
        pk = S / P;
        ph.assign((size_t)P * P, 0);
        std::vector<float> srt;
        for (int j = 0; j < P; j++)
            for (int i = 0; i < P; i++) {
                const double x = (i + 0.5) * pk, y = (j + 0.5) * pk;
                ph[(size_t)j * P + i] = (float)rawH(x, y);
                if (x <= w && y <= h) srt.push_back(ph[(size_t)j * P + i]);
            }
        std::sort(srt.begin(), srt.end());
        seaH = !p.hmap.empty() && p.hmSea >= 0 ? p.hmSea : srt[(size_t)(std::clamp(p.sea, 0.0, 0.9) * (srt.size() - 1))];
        topH = srt[(size_t)(0.999 * (srt.size() - 1))];
        auto idx = [&](int i, int j) { return (size_t)j * P + i; };
        auto land = [&](int i, int j) { return ph[idx(i, j)] >= seaH; };
        // реки: заливка впадин (priority flood) → сток по крутейшему → накопление; река там, где стекает много
        std::vector<float> fh(ph);
        std::vector<char> done((size_t)P * P, 0);
        typedef std::pair<float, int> QE;
        std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
        for (int j = 0; j < P; j++)
            for (int i = 0; i < P; i++)
                if (!land(i, j) || i == 0 || j == 0 || i == P - 1 || j == P - 1) pq.push({fh[idx(i, j)], j * P + i}), done[idx(i, j)] = 1;
        std::vector<int> order;
        order.reserve((size_t)P * P);
        std::vector<int> down((size_t)P * P, -1);
        const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        while (!pq.empty()) {
            const auto [hv, c] = pq.top();
            pq.pop();
            order.push_back(c);
            const int ci = c % P, cj = c / P;
            for (int k = 0; k < 8; k++) {
                const int ni = ci + DX[k], nj = cj + DY[k];
                if (ni < 0 || nj < 0 || ni >= P || nj >= P || done[idx(ni, nj)]) continue;
                done[idx(ni, nj)] = 1;
                fh[idx(ni, nj)] = std::max(fh[idx(ni, nj)], hv + 1e-6f);
                down[idx(ni, nj)] = c;   // течёт туда, откуда пришла заливка — к морю или краю
                pq.push({fh[idx(ni, nj)], nj * P + ni});
            }
        }
        std::vector<float> acc((size_t)P * P, 0);
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            const int c = *it;
            const int ci = c % P, cj = c / P;
            acc[c] += (float)std::clamp(moist((ci + 0.5) * pk, (cj + 0.5) * pk), 0.1, 1.2);
            if (down[c] >= 0) acc[down[c]] += acc[c];
        }
        const double thr = (double)srt.size() * 0.0025 / std::max(0.05, p.rivers);   // река — где стекает с 1/400 площади мира (при густоте 1)
        for (int c = 0; c < P * P; c++) {
            if (acc[c] < thr || down[c] < 0 || !land(c % P, c / P)) continue;
            const int d = down[c];
            const double wkm = std::min(1.2, 0.05 + 0.02 * std::sqrt(acc[c] / thr) * std::sqrt(pk));
            rivers.segs.push_back({(c % P + 0.5) * pk, (c / P + 0.5) * pk, (d % P + 0.5) * pk, (d / P + 0.5) * pk, wkm});
        }
        rivers.build(S, S, std::max(1.5, pk));
        // поселения: пригодность (ровно, умеренно, у воды), шаг между ними — от плотности
        struct Cand { double sc; int c; };
        std::vector<Cand> cs;
        for (int c = 0; c < P * P; c++) {
            const int ci = c % P, cj = c / P;
            if (!land(ci, cj)) continue;
            const int lv = levelOf(ph[c]);
            if (lv > 5) continue;
            const double x = (ci + 0.5) * pk, y = (cj + 0.5) * pk, t = temp(x, y, lv);
            double sc = 1.2 - std::abs(t - 0.5) * 2 - lv * 0.08 + (acc[c] >= thr * 0.5 ? 0.5 : 0);
            for (int k = 0; k < 8; k++) {
                const int ni = ci + DX[k], nj = cj + DY[k];
                if (ni >= 0 && nj >= 0 && ni < P && nj < P && !land(ni, nj)) { sc += 0.3; break; }
            }
            cs.push_back({sc + 0.6 * ((smix(p.seed ^ (uint64_t)c * 77) >> 11) * (1.0 / 9007199254740992.0)), c});
        }
        std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.sc > b.sc || (a.sc == b.sc && a.c < b.c); });
        const double spacing = 100.0 / std::sqrt(std::max(0.05, p.towns)) * 0.7;
        const size_t cap = 3000;
        tbk = std::max(spacing, pk), tbw = (int)(S / tbk) + 1;
        std::vector<std::vector<int>> tb((size_t)tbw * tbw);
        for (const Cand& cd : cs) {
            if (towns.size() >= cap) break;
            const double x = (cd.c % P + 0.5) * pk, y = (cd.c / P + 0.5) * pk;
            const int bx = (int)(x / tbk), by = (int)(y / tbk);
            bool ok = true;
            for (int yy = std::max(0, by - 1); yy <= std::min(tbw - 1, by + 1) && ok; yy++)
                for (int xx = std::max(0, bx - 1); xx <= std::min(tbw - 1, bx + 1) && ok; xx++)
                    for (int ti : tb[(size_t)yy * tbw + xx])
                        if (std::hypot(towns[ti].x - x, towns[ti].y - y) < spacing) { ok = false; break; }
            if (!ok) continue;
            Town t;
            t.x = x, t.y = y, t.name = townName(p.seed ^ ((uint64_t)cd.c << 8));
            tb[(size_t)by * tbw + bx].push_back((int)towns.size());
            towns.push_back(t);
        }
        // размеры по месту в списке (лучшие места — крупнейшие): шкала «Масштабов»
        for (size_t i = 0; i < towns.size(); i++) {
            const double q = (double)i / std::max<size_t>(1, towns.size());
            Town& t = towns[i];
            if (q < 0.02) t.kind = "великий город", t.pop = 60000, t.r = 2.5;
            else if (q < 0.07) t.kind = "крупный город", t.pop = 25000, t.r = 1.6;
            else if (q < 0.2) t.kind = "город", t.pop = 8000, t.r = 1.0;
            else if (q < 0.5) t.kind = "городок", t.pop = 2000, t.r = 0.6;
            else t.kind = "село", t.pop = 500, t.r = 0.35;
        }
        townB.clear();
        buildTownIndex();
        // дороги: каждое поселение — к двум ближайшим; путь по плану, дешевле по уже проложенным дорогам
        std::vector<char> roadC((size_t)P * P, 0);
        std::vector<std::pair<int, int>> edges;
        for (size_t a = 0; a < towns.size(); a++) {
            std::vector<std::pair<double, int>> nb;
            const int bx = (int)(towns[a].x / tbk), by = (int)(towns[a].y / tbk);
            for (int yy = std::max(0, by - 3); yy <= std::min(tbw - 1, by + 3); yy++)
                for (int xx = std::max(0, bx - 3); xx <= std::min(tbw - 1, bx + 3); xx++)
                    for (int ti : tb[(size_t)yy * tbw + xx])
                        if ((size_t)ti != a) nb.push_back({std::hypot(towns[ti].x - towns[a].x, towns[ti].y - towns[a].y), ti});
            std::sort(nb.begin(), nb.end());
            for (size_t k = 0; k < nb.size() && k < 2; k++)
                if (nb[k].first < spacing * 4) edges.push_back({(int)std::min<size_t>(a, nb[k].second), (int)std::max<size_t>(a, nb[k].second)});
        }
        std::sort(edges.begin(), edges.end());
        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
        std::vector<double> mcost((size_t)P * P);
        for (int c = 0; c < P * P; c++) {
            const int lv = levelOf(ph[c]);
            mcost[c] = !land(c % P, c / P) ? -1 : (lv >= 6 ? 3.0 : lv >= 4 ? 2.0 : 1.3);
        }
        std::vector<double> dist((size_t)P * P);
        std::vector<int> from((size_t)P * P), stamp((size_t)P * P, -1), closed((size_t)P * P, -1);
        int run = 0;
        for (auto [a, b] : edges) {
            run++;
            const int ax = std::clamp((int)(towns[a].x / pk), 0, P - 1), ay = std::clamp((int)(towns[a].y / pk), 0, P - 1);
            const int bx = std::clamp((int)(towns[b].x / pk), 0, P - 1), by = std::clamp((int)(towns[b].y / pk), 0, P - 1);
            const int m = 12, x0 = std::max(0, std::min(ax, bx) - m), x1 = std::min(P - 1, std::max(ax, bx) + m),
                      y0 = std::max(0, std::min(ay, by) - m), y1 = std::min(P - 1, std::max(ay, by) + m);
            typedef std::pair<double, int> E;
            std::priority_queue<E, std::vector<E>, std::greater<E>> q;
            const int s = ay * P + ax, g = by * P + bx;
            stamp[s] = run, dist[s] = 0, from[s] = -1;
            q.push({0, s});
            bool found = false;
            while (!q.empty()) {
                const int c = q.top().second;
                q.pop();
                if (closed[c] == run) continue;
                closed[c] = run;
                if (c == g) { found = true; break; }
                const double d = dist[c];
                const int ci = c % P, cj = c / P;
                for (int k = 0; k < 8; k++) {
                    const int ni = ci + DX[k], nj = cj + DY[k];
                    if (ni < x0 || nj < y0 || ni > x1 || nj > y1) continue;
                    const int n = nj * P + ni;
                    if (mcost[n] < 0) continue;
                    const double nd = d + (k < 4 ? 1.0 : 1.4142) * (roadC[n] ? 0.6 : mcost[n]);
                    if (stamp[n] != run || nd < dist[n] - 1e-9) {
                        stamp[n] = run, dist[n] = nd, from[n] = c;
                        q.push({nd + 0.6 * std::hypot(ni - bx, nj - by), n});   // A*: оценка снизу — по прямой с дорожной ценой
                    }
                }
            }
            if (!found) continue;
            for (int c = g; from[c] >= 0; c = from[c]) {
                const int d = from[c];
                if (roadC[c] && roadC[d]) continue;
                roadC[c] = roadC[d] = 1;
                roads.segs.push_back({(c % P + 0.5) * pk, (c / P + 0.5) * pk, (d % P + 0.5) * pk, (d / P + 0.5) * pk, 0.06});
            }
        }
        roads.build(S, S, std::max(1.5, pk));
    }
    void buildTownIndex() {
        tbw = (int)(std::max(w, h) / tbk) + 1;
        townB.assign((size_t)tbw * tbw, {});
        for (size_t i = 0; i < towns.size(); i++)
            townB[(size_t)std::min(tbw - 1, (int)(towns[i].y / tbk)) * tbw + std::min(tbw - 1, (int)(towns[i].x / tbk))].push_back((int)i);
    }
    int townAt(double x, double y, double tol) const {
        const int bx = (int)(x / tbk), by = (int)(y / tbk);
        for (int yy = std::max(0, by - 1); yy <= std::min(tbw - 1, by + 1); yy++)
            for (int xx = std::max(0, bx - 1); xx <= std::min(tbw - 1, bx + 1); xx++)
                for (int t : townB[(size_t)yy * tbw + xx])
                    if (std::hypot(towns[t].x - x, towns[t].y - y) <= std::max(towns[t].r, tol)) return t;
        return -1;
    }
    bool snapRoad(double& x, double& y, double maxd) const override { return roads.nearest(x, y, maxd); }
    WCell at(double x, double y, double tol) const override {
        WCell c;
        if (x < 0 || y < 0 || x > w || y > h) { c.terr = TERR_WATER; return c; }
        const double H = rawH(x, y, tol * 0.5);
        c.level = levelOf(H);
        const double t = temp(x, y, c.level), m = moist(x, y);
        c.biome = biomeOfTM(t, m);
        if (H < seaH) { c.terr = TERR_WATER; return c; }
        c.road = roads.near(x, y, tol * 0.5);
        c.river = rivers.near(x, y, tol * 0.5);
        c.town = townAt(x, y, 0);   // поселение — своим радиусом; издали его видно значком из списка, а не закрашенной клеткой
        if (c.town >= 0) { c.terr = TERR_TOWN; return c; }
        if (c.level >= 6) c.terr = TERR_MOUNT;
        else if (c.biome == 6) c.terr = TERR_DESERT;
        else if (c.biome == 0 || c.biome == 2) c.terr = TERR_SNOW;
        else if (c.biome == 5) c.terr = TERR_SWAMP;
        else if (c.level >= 4) c.terr = TERR_HILLS;
        else if (c.biome == 1 || c.biome == 8) c.terr = TERR_FOREST;
        else if (c.biome == 4) c.terr = fbm2(p.seed ^ 0xF0, x / 30.0, y / 30.0, 3) > 0.05 ? TERR_FOREST : TERR_FIELDS;
        else c.terr = TERR_FIELDS;
        return c;
    }
};

// ---- готовая сетка Акта I (долгота/широта) как мир в километрах ----
struct ActWorld : World {
    Region R;
    double latMid = 46, kx = 77, ky = 110.57;   // км на градус долготы и широты
    void build(const std::vector<Town>& tw) {
        latMid = R.lat0 - R.h * R.dlat * 0.5;
        kx = 111.32 * std::cos(latMid * M_PI / 180), ky = 110.57;
        w = R.w * R.dlon * kx, h = R.h * R.dlat * ky;
        towns = tw;
    }
    void toKm(double lon, double lat, double& x, double& y) const { x = (lon - R.lon0) * kx, y = (R.lat0 - lat) * ky; }
    WCell at(double x, double y, double tol) const override {
        (void)tol;
        WCell c;
        const int q = R.cell(R.lon0 + x / kx, R.lat0 - y / ky);
        c.terr = R.terrAt(q);
        c.road = R.roadAt(q), c.river = R.riverAt(q);
        c.level = c.terr == TERR_WATER ? 0 : c.terr == TERR_MOUNT ? 7 : c.terr == TERR_HILLS ? 4 : 1;
        c.biome = c.terr == TERR_SWAMP ? 5 : c.terr == TERR_FOREST ? 4 : 3;
        if (c.terr == TERR_TOWN)
            for (size_t i = 0; i < towns.size(); i++)
                if (std::hypot(towns[i].x - x, towns[i].y - y) < 6) { c.town = (int)i; break; }
        return c;
    }
};

// путь за день по маршруту (км): budget — км «по дороге» (путник × темп × часы/8); шаг 0,5 км стоит шаг / множитель
// клетки (дорога и город — ×1). Местность дня — где отряд провёл больше всего времени; ночёвка в городе — «Город»;
// пересёк реку не по дороге среди пашен — «Река и переправа». Вода не пропускает: отряд встаёт у берега.
struct Walk {
    double x = 0, y = 0, km = 0;
    size_t next = 0;           // первый ещё не достигнутый пункт маршрута
    int terrain = TERR_FIELDS;
    bool road = false, blocked = false, river = false;
};
inline Walk walkDay(const World& W, double x, double y, const std::vector<std::pair<double, double>>& route, double budget, const double* mult) {
    Walk w;
    w.x = x, w.y = y;
    double tt[TERR_N] = {0}, roadT = 0, total = 0;
    while (budget > 1e-9 && w.next < route.size()) {
        const double tx = route[w.next].first, ty = route[w.next].second, d = std::hypot(tx - w.x, ty - w.y);
        if (d < 1e-6) { w.next++; continue; }
        double mv = std::min(0.5, d);
        const WCell c = W.at(w.x + (tx - w.x) * mv / d, w.y + (ty - w.y) * mv / d, CELL_KM);   // дорога — если в пределах клетки
        if (c.terr == TERR_WATER) { w.blocked = true; break; }
        const double m = c.road ? 1.0 : mult[c.terr];
        double cost = mv / m;
        if (cost > budget) mv = budget * m, cost = budget;
        const double g = mv / d;
        w.x += (tx - w.x) * g, w.y += (ty - w.y) * g, w.km += mv, budget -= cost;
        tt[c.terr] += cost, total += cost, roadT += c.road ? cost : 0;
        if (!c.road && c.river) w.river = true;
        if (mv >= d - 1e-9) w.next++;
    }
    int best = TERR_FIELDS;
    for (int t = 0; t < TERR_N; t++)
        if (t != TERR_TOWN && t != TERR_WATER && tt[t] > tt[best]) best = t;
    w.terrain = best;
    if (W.at(w.x, w.y, CELL_KM).terr == TERR_TOWN) w.terrain = TERR_TOWN;
    else if (best == TERR_FIELDS && w.river) w.terrain = TERR_RIVER;
    w.road = total > 0 && roadT * 2 > total;
    return w;
}

// ---- поиск пути (ТЗ v2 §4) ----
// Режимы: быстрее всего — минимум времени по множителям местности; по дорогам — вне дороги ×5; напрямик — кратчайший
// в обход воды, затем выпрямляется. Сетка вокруг отрезка, клетка — чтобы клеток было не больше ~250 тыс.
enum PathMode { PM_FAST, PM_ROADS, PM_STRAIGHT };
inline int pathModeOf(const std::string& s) { return s == "дороги" || s == "по дорогам" ? PM_ROADS : s == "напрямик" ? PM_STRAIGHT : PM_FAST; }
typedef std::vector<std::pair<double, double>> Pts;
inline bool findPath(const World& W, double ax, double ay, double bx, double by, int mode, const double* mult, Pts& out, std::string& err) {
    const double d = std::hypot(bx - ax, by - ay), m = std::max(20.0, 0.35 * d);
    const double x0 = std::max(0.0, std::min(ax, bx) - m), x1 = std::min(W.w, std::max(ax, bx) + m);
    const double y0 = std::max(0.0, std::min(ay, by) - m), y1 = std::min(W.h, std::max(ay, by) + m);
    const double cs = std::max(CELL_KM, std::sqrt((x1 - x0) * (y1 - y0) / 250000.0));
    const int nx = std::max(1, (int)std::ceil((x1 - x0) / cs)), ny = std::max(1, (int)std::ceil((y1 - y0) / cs));
    std::vector<float> cost((size_t)nx * ny, -2.f);   // −2 — не посчитано, −1 — вода; иначе цена км
    auto cellCost = [&](int i, int j) -> float {
        float& c = cost[(size_t)j * nx + i];
        if (c > -1.5f) return c;
        const WCell w = W.at(x0 + (i + 0.5) * cs, y0 + (j + 0.5) * cs, cs);
        if (w.terr == TERR_WATER) return c = -1.f;
        const double mm = w.road || w.terr == TERR_TOWN ? 1.0 : mult[w.terr];
        return c = (float)(mode == PM_STRAIGHT ? 1.0 : mode == PM_ROADS ? (w.road || w.terr == TERR_TOWN ? 1.0 : 5.0 / mm) : 1.0 / mm);
    };
    auto cellOf = [&](double x, double y, int& i, int& j) { i = std::clamp((int)((x - x0) / cs), 0, nx - 1), j = std::clamp((int)((y - y0) / cs), 0, ny - 1); };
    int si, sj, gi, gj;
    cellOf(ax, ay, si, sj), cellOf(bx, by, gi, gj);
    if (cellCost(gi, gj) < 0) { err = "цель в воде"; return false; }
    const int N = nx * ny, S = sj * nx + si, G = gj * nx + gi;
    std::vector<float> dist((size_t)N, 1e30f);
    std::vector<int> from((size_t)N, -1);
    std::vector<char> closed((size_t)N, 0);
    typedef std::pair<double, int> E;
    std::priority_queue<E, std::vector<E>, std::greater<E>> q;
    dist[S] = 0, q.push({0, S});
    const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    bool found = false;
    while (!q.empty()) {
        const int c = q.top().second;
        q.pop();
        if (closed[c]) continue;
        closed[c] = 1;
        if (c == G) { found = true; break; }
        const int ci = c % nx, cj = c / nx;
        for (int k = 0; k < 8; k++) {
            const int ni = ci + DX[k], nj = cj + DY[k];
            if (ni < 0 || nj < 0 || ni >= nx || nj >= ny) continue;
            const int n = nj * nx + ni;
            if (closed[n]) continue;
            const float cn = cellCost(ni, nj);
            if (cn < 0) continue;
            const float cc = c == S ? cn : cost[c];
            const double nd = dist[c] + (k < 4 ? 1.0 : 1.41421356) * cs * 0.5 * (cc + cn);
            if (nd < dist[n]) dist[n] = (float)nd, from[n] = c, q.push({nd + std::hypot(gi - ni, gj - nj) * cs, n});   // оценка снизу: цена км ≥ 1
        }
    }
    if (!found) { err = "пути нет: вода кругом"; return false; }
    Pts p;
    for (int c = G; c >= 0; c = from[c]) p.push_back({x0 + (c % nx + 0.5) * cs, y0 + (c / nx + 0.5) * cs});
    std::reverse(p.begin(), p.end());
    p.front() = {ax, ay}, p.back() = {bx, by};
    auto dry = [&](const std::pair<double, double>& a, const std::pair<double, double>& b) {   // прямая без воды
        const double L = std::hypot(b.first - a.first, b.second - a.second);
        const int n = (int)(L / (cs * 0.5)) + 1;
        for (int k = 1; k < n; k++)
            if (W.at(a.first + (b.first - a.first) * k / n, a.second + (b.second - a.second) * k / n, cs).terr == TERR_WATER) return false;
        return true;
    };
    Pts r;
    if (mode == PM_STRAIGHT) {   // выпрямить: от точки — к самой дальней видимой по суше
        size_t i = 0;
        r.push_back(p[0]);
        while (i + 1 < p.size()) {
            size_t j = p.size() - 1;
            while (j > i + 1 && !dry(p[i], p[j])) j = i + 1 + (j - i - 1) / 2;
            r.push_back(p[j]), i = j;
        }
    } else {   // убрать точки на прямых участках; точки дороги — на ось дороги
        r.push_back(p[0]);
        for (size_t i = 1; i + 1 < p.size(); i++) {
            const double dx1 = p[i].first - r.back().first, dy1 = p[i].second - r.back().second, dx2 = p[i + 1].first - p[i].first, dy2 = p[i + 1].second - p[i].second;
            if (std::abs(dx1 * dy2 - dy1 * dx2) > 1e-9 * cs * cs || cost[(size_t)((int)((p[i].second - y0) / cs)) * nx + (int)((p[i].first - x0) / cs)] == 1.f) {
                auto q2 = p[i];
                W.snapRoad(q2.first, q2.second, cs);
                r.push_back(q2);
            }
        }
        r.push_back(p.back());
    }
    out.insert(out.end(), r.begin() + (out.empty() ? 0 : 1), r.end());
    return true;
}
// маршрут через пункты: путь каждого перехода — своим поиском
inline bool findRoute(const World& W, double ax, double ay, const Pts& way, int mode, const double* mult, Pts& out, std::string& err) {
    out.clear();
    double x = ax, y = ay;
    for (auto& w : way) {
        Pts leg;
        if (!findPath(W, x, y, w.first, w.second, mode, mult, leg, err)) return false;
        out.insert(out.end(), leg.begin() + 1, leg.end());   // начало перехода — там, где отряд уже стоит
        x = w.first, y = w.second;
    }
    return true;
}
// сводка: длина, дни (обычный темп, 8 ч), доля дороги, местности по км
struct RouteSum {
    double km = 0, roadKm = 0;
    int days = 0;
    bool blocked = false;
    double terrKm[TERR_N] = {0};
};
inline RouteSum routeSummary(const World& W, double ax, double ay, const Pts& path, double kmDay, const double* mult) {
    RouteSum s;
    double x = ax, y = ay;
    for (auto& p : path) {
        const double L = std::hypot(p.first - x, p.second - y);
        const int n = std::max(1, (int)(L / 0.5));
        for (int k = 0; k < n; k++) {
            const WCell c = W.at(x + (p.first - x) * (k + 0.5) / n, y + (p.second - y) * (k + 0.5) / n, CELL_KM);
            s.terrKm[c.terr] += L / n, s.roadKm += c.road ? L / n : 0;
        }
        s.km += L, x = p.first, y = p.second;
    }
    Walk w;
    w.x = ax, w.y = ay;
    for (s.days = 0; w.next < path.size() && s.days < 2000; s.days++) {
        Pts rest(path.begin() + w.next, path.end());
        const Walk n = walkDay(W, w.x, w.y, rest, kmDay, mult);
        if (n.blocked && n.km < 1e-6) { s.blocked = true; break; }
        w.x = n.x, w.y = n.y, w.next += n.next;
    }
    return s;
}
inline std::string routeSumJson(const RouteSum& s) {
    std::string o = "{\"km\":" + num(std::llround(s.km)) + ",\"days\":" + num(s.days) + ",\"road\":" + num(s.km > 0 ? std::llround(100 * s.roadKm / s.km) : 0) +
                    ",\"blocked\":" + bl(s.blocked) + ",\"terr\":{";
    int k = 0;
    for (int t = 0; t < TERR_N; t++)
        if (s.terrKm[t] >= 0.5) o += std::string(k++ ? "," : "") + q(terrains()[t].name) + ":" + num(std::llround(s.terrKm[t]));
    return o + "}}";
}

// кусок обзора: 256 × 256 клеток уровня z, строки по клеткам: местность, высота, флаги (1 дорога, 2 река)
inline std::string tileJson(const World& W, int z, int tx, int ty) {
    const double ck = CELL_KM * (1 << z);
    std::string t(TILE * TILE, '7'), l(TILE * TILE, '0'), f(TILE * TILE, '0');
    for (int j = 0; j < TILE; j++)
        for (int i = 0; i < TILE; i++) {
            const double x = ((double)tx * TILE + i + 0.5) * ck, y = ((double)ty * TILE + j + 0.5) * ck;
            if (x > W.w || y > W.h) continue;
            const WCell c = W.at(x, y, ck);
            const size_t k = (size_t)j * TILE + i;
            t[k] = (char)('0' + c.terr), l[k] = (char)('0' + c.level), f[k] = (char)('0' + (c.road ? 1 : 0) + (c.river ? 2 : 0));
        }
    char ckb[32];
    std::snprintf(ckb, sizeof ckb, "%.4f", ck);
    return "{\"z\":" + num(z) + ",\"tx\":" + num(tx) + ",\"ty\":" + num(ty) + ",\"cellKm\":" + ckb +
           ",\"terr\":\"" + t + "\",\"level\":\"" + l + "\",\"flags\":\"" + f + "\"}";
}
inline std::string worldInfoJson(const World& W) {
    std::string o = "{\"name\":" + q(W.name) + ",\"w\":" + num(std::llround(W.w)) + ",\"h\":" + num(std::llround(W.h)) + ",\"cellKm\":0.3048,\"tile\":256,\"zMax\":" +
                    num(W.zMax()) + ",\"terrains\":[";
    for (size_t i = 0; i < terrains().size(); i++) o += std::string(i ? "," : "") + q(terrains()[i].name);
    o += "],\"towns\":[";
    char b[64];
    for (size_t i = 0; i < W.towns.size(); i++) {
        const Town& t = W.towns[i];
        std::snprintf(b, sizeof b, "%.2f,%.2f", t.x, t.y);
        o += std::string(i ? "," : "") + "[" + q(t.name) + "," + b + "," + num(t.pop) + "," + q(t.kind) + "]";
    }
    return o + "]}";
}

}  // namespace kmp
