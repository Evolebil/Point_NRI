// Deterministic, data-only settlement plan for the campaign city view.
#pragma once
#include "kmp_core.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace kmp {

struct CityRect {
    double x = 0, y = 0, w = 0, h = 0; // feet
    int key = 0;
    std::string kind, district, name;
};

struct CityPlan {
    uint64_t seed = 0;
    int population = 0, wall = 0; // 0 none, 1 palisade, 2 stone
    double widthFt = 0, heightFt = 0, cellFt = 100;
    bool coast = false, river = false;
    std::vector<CityRect> roads, walls, buildings, fields;

    static uint64_t hash(uint64_t x) { return smix(x ^ 0xC17A5EED4B1Dull); }
    static uint64_t textHash(const std::string& s) { uint64_t h = 1469598103934665603ull; for (unsigned char c : s) h = (h ^ c) * 1099511628211ull; return h; }
    static double unit(uint64_t x) { return (hash(x) >> 11) * (1.0 / 9007199254740992.0); }
    static CityPlan generate(uint64_t worldSeed, uint64_t settlementId, int pop, bool coast = false,
                             bool river = false, int wallType = -1) {
        CityPlan p;
        p.seed = hash(worldSeed ^ hash(settlementId + 0x9e3779b97f4a7c15ull));
        p.population = std::max(15, pop);
        p.coast = coast, p.river = river;
        const double rank = std::sqrt(std::max(1.0, p.population / 500.0));
        p.widthFt = std::clamp(1200.0 + rank * 340.0, 1200.0, 7000.0);
        p.heightFt = std::clamp(1050.0 + rank * 310.0, 1050.0, 6200.0);
        p.wall = wallType >= 0 ? std::clamp(wallType, 0, 2) : (pop >= 5000 ? 2 : pop >= 1000 ? 1 : 0);

        const double cx = p.widthFt / 2, cy = p.heightFt / 2;
        const double inset = p.wall ? 90 : 60;
        const double step = p.population >= 5000 ? 210 : p.population >= 1000 ? 240 : 280;
        const double avenue = 42, street = 22;
        // Grid streets form one connected graph; the central cross links every block to the market.
        for (double x = inset; x <= p.widthFt - inset; x += step)
            p.roads.push_back({x - street / 2, inset, street, p.heightFt - 2 * inset, 0, "street", "", ""});
        for (double y = inset; y <= p.heightFt - inset; y += step)
            p.roads.push_back({inset, y - street / 2, p.widthFt - 2 * inset, street, 0, "street", "", ""});
        p.roads.push_back({cx - avenue / 2, inset, avenue, p.heightFt - 2 * inset, 0, "avenue", "", ""});
        p.roads.push_back({inset, cy - avenue / 2, p.widthFt - 2 * inset, avenue, 0, "avenue", "", ""});
        p.roads.push_back({cx - 105, cy - 105, 210, 210, 0, "market_square", "market", "Центральная площадь"});

        if (p.wall) {
            // Four wall sides are split around cardinal gates, leaving explicit openings.
            const double gap = 92, thick = p.wall == 2 ? 28 : 18;
            p.walls = {{inset, inset, cx - inset - gap / 2, thick, 0, "wall", "", ""},
                       {cx + gap / 2, inset, p.widthFt - cx - inset - gap / 2, thick, 0, "wall", "", ""},
                       {inset, p.heightFt - inset - thick, cx - inset - gap / 2, thick, 0, "wall", "", ""},
                       {cx + gap / 2, p.heightFt - inset - thick, p.widthFt - cx - inset - gap / 2, thick, 0, "wall", "", ""},
                       {inset, inset, thick, cy - inset - gap / 2, 0, "wall", "", ""},
                       {inset, cy + gap / 2, thick, p.heightFt - cy - inset - gap / 2, 0, "wall", "", ""},
                       {p.widthFt - inset - thick, inset, thick, cy - inset - gap / 2, 0, "wall", "", ""},
                       {p.widthFt - inset - thick, cy + gap / 2, thick, p.heightFt - cy - inset - gap / 2, 0, "wall", "", ""}};
            for (const auto& g : std::vector<CityRect>{{cx - gap / 2, inset - 18, gap, 54, 1, "gate", "", "Северные ворота"},
                 {cx - gap / 2, p.heightFt - inset - 36, gap, 54, 2, "gate", "", "Южные ворота"},
                 {inset - 18, cy - gap / 2, 54, gap, 3, "gate", "", "Западные ворота"},
                 {p.widthFt - inset - 36, cy - gap / 2, 54, gap, 4, "gate", "", "Восточные ворота"}}) p.buildings.push_back(g);
        }
        auto poi = [&](double x, double y, double w, double h, int key, const char* kind, const char* district, const char* name) {
            p.buildings.push_back({x, y, w, h, key, kind, district, name});
        };
        poi(cx - 76, cy - 76, 152, 152, 5, pop >= 5000 ? "citadel" : "hall", "civic", pop >= 5000 ? "Цитадель" : "Дом совета");
        poi(cx - 52, cy - 178, 104, 76, 6, "market", "market", "Рынок");
        if (pop >= 5000) poi(cx + 112, cy - 62, 96, 118, 7, "temple", "temple", "Храм");
        if (coast) poi(p.widthFt - inset - 220, cy + 110, 150, 96, 8, "warehouse", "harbor", "Портовые склады");
        if (river) poi(cx - 238, cy + 124, 84, 64, 9, "bridge", "bridge", "Мост");

        const int nx = (int)std::floor((p.widthFt - 2 * inset) / step), ny = (int)std::floor((p.heightFt - 2 * inset) / step);
        int key = 20;
        for (int iy = 0; iy < ny; ++iy) for (int ix = 0; ix < nx; ++ix) {
            const double x0 = inset + ix * step + street / 2 + 13, y0 = inset + iy * step + street / 2 + 13;
            const double x1 = inset + (ix + 1) * step - street / 2 - 13, y1 = inset + (iy + 1) * step - street / 2 - 13;
            if (x1 - x0 < 60 || y1 - y0 < 60) continue;
            const double bx = (x0 + x1) / 2, by = (y0 + y1) / 2;
            const double center = std::hypot((bx - cx) / cx, (by - cy) / cy);
            if (center < 0.19) continue; // reserved civic core
            const double fill = p.population < 1000 ? 0.36 : p.population < 5000 ? 0.58 : 0.82;
            if (unit(p.seed ^ ((uint64_t)iy << 32) ^ (uint64_t)ix) > fill) continue;
            const std::string district = bx > cx + p.widthFt * .22 ? "trade" : by > cy + p.heightFt * .18 ? "craft" : center < .45 ? "noble" : "residential";
            const double bw = std::min(x1 - x0, 30 + unit(p.seed ^ key * 37ull) * 16);
            const double bh = std::min(y1 - y0, 34 + unit(p.seed ^ key * 101ull) * 20);
            // Two or three plots per block, each kept clear of its roads and neighbors.
            for (int sub = 0; sub < 2; ++sub) {
                const double px = x0 + (sub ? (x1 - x0) * .59 : (x1 - x0) * .12);
                const double pw = std::min(bw, x1 - px), py = y0 + (sub ? (y1 - y0) * .52 : (y1 - y0) * .1);
                const double ph = std::min(bh, y1 - py);
                const bool onStreet = std::any_of(p.roads.begin(), p.roads.end(), [&](const CityRect& r) {
                    return px < r.x + r.w && px + pw > r.x && py < r.y + r.h && py + ph > r.y;
                });
                const bool onWall = std::any_of(p.walls.begin(), p.walls.end(), [&](const CityRect& r) {
                    return px < r.x + r.w + 2 && px + pw + 2 > r.x && py < r.y + r.h + 2 && py + ph + 2 > r.y;
                });
                const bool inCivicCore = px < cx + 110 && px + pw > cx - 110 && py < cy + 110 && py + ph > cy - 110;
                const bool onHouse = std::any_of(p.buildings.begin(), p.buildings.end(), [&](const CityRect& r) {
                    return r.kind == "house" && px < r.x + r.w + 2 && px + pw + 2 > r.x && py < r.y + r.h + 2 && py + ph + 2 > r.y;
                });
                if (!onStreet && !onWall && !inCivicCore && !onHouse) p.buildings.push_back({px, py, pw, ph, key++, "house", district, ""});
            }
        }
        // Farms stay outside the settlement perimeter and are separated into strips.
        const double fieldY = p.heightFt + 80;
        for (int i = 0; i < 8; ++i) p.fields.push_back({i * p.widthFt / 8, fieldY, p.widthFt / 8 - 8, (double)(150 + (i % 3) * 25), 0, "field", "farmland", ""});
        return p;
    }
};

inline std::string cityPlanJson(const CityPlan& p) {
    auto rects = [](const std::vector<CityRect>& rs) {
        std::string s = "[";
        for (size_t i = 0; i < rs.size(); ++i) {
            const auto& r = rs[i];
            s += (i ? "," : "") + std::string("{\"x\":") + std::to_string((int)r.x) + ",\"y\":" + std::to_string((int)r.y) +
                 ",\"w\":" + std::to_string((int)r.w) + ",\"h\":" + std::to_string((int)r.h) + ",\"key\":" + std::to_string(r.key) +
                 ",\"kind\":" + q(r.kind) + ",\"district\":" + q(r.district) + ",\"name\":" + q(r.name) + "}";
        }
        return s + "]";
    };
    return "{\"seed\":" + std::to_string(p.seed) + ",\"population\":" + std::to_string(p.population) + ",\"wall\":" + std::to_string(p.wall) +
           ",\"widthFt\":" + std::to_string((int)p.widthFt) + ",\"heightFt\":" + std::to_string((int)p.heightFt) +
           ",\"cellFt\":100,\"coast\":" + bl(p.coast) + ",\"river\":" + bl(p.river) + ",\"roads\":" + rects(p.roads) +
           ",\"walls\":" + rects(p.walls) + ",\"buildings\":" + rects(p.buildings) + ",\"fields\":" + rects(p.fields) + "}";
}

} // namespace kmp
