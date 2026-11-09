// ============================================================================
// Кампания (исследование и социалка, ТЗ docs/TZ_issledovanie_socialka.md): общее — кости, деньги, местность, состояние.
// Подключается после engine.h и char_spec.h. Бой не трогает: ничего отсюда движок боя не видит.
//
// Определённость (НФ-01): бросок k события n при попытке a — kmp::roll(зерно, n, a, k); переброс мастера — та же
// пара (n, a+1). Состояние кампании — свёртка журнала: событие несёт свои броски и итоги, применение их не перебрасывает.
// ============================================================================
#pragma once
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace kmp {

// ---- кости ----
inline uint64_t smix(uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
// поток бросков одного черновика события: зерно кампании, номер события, попытка (переброс мастера)
struct Dice {
    uint64_t s;
    int k = 0;
    Dice(uint64_t seed, int n, int attempt) : s(smix(smix(smix(seed) ^ (uint64_t)n) ^ ((uint64_t)attempt << 32))) {}
    int d(int sides) { return (int)(smix(s ^ (uint64_t)(++k) * 0xD6E8FEB86659FD93ull) % (uint64_t)sides) + 1; }
    int nd(int n, int sides) {
        int t = 0;
        for (int i = 0; i < n; i++) t += d(sides);
        return t;
    }
};

// ---- JSON-ответ: строки собираются вручную (как в char_spec.h) ----
inline std::string q(const std::string& s) {
    std::string r = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') r += '\\', r += (char)c;
        else if (c == '\n') r += "\\n";
        else if (c < 0x20) r += ' ';
        else r += (char)c;
    }
    return r + "\"";
}
inline std::string num(long long v) { return std::to_string(v); }
inline std::string bl(bool v) { return v ? "true" : "false"; }
// Json (mini_json.h) обратно в строку: запрос в событие журнала переносится как есть
inline std::string jsonOut(const Json& j) {
    switch (j.t) {
        case Json::NUL: return "null";
        case Json::BOOL: return bl(j.b);
        case Json::NUM: {
            if (j.d == std::floor(j.d) && std::fabs(j.d) < 9e15) return num((long long)j.d);
            char b[32];
            std::snprintf(b, sizeof b, "%.10g", j.d);
            return b;
        }
        case Json::STR: return q(j.s);
        case Json::ARR: {
            std::string r = "[";
            for (size_t i = 0; i < j.a.size(); i++) r += (i ? "," : "") + jsonOut(j.a[i]);
            return r + "]";
        }
        case Json::OBJ: {
            std::string r = "{";
            for (size_t i = 0; i < j.o.size(); i++) r += (i ? "," : "") + q(j.o[i].first) + ":" + jsonOut(j.o[i].second);
            return r + "}";
        }
    }
    return "null";
}

// ---- деньги (ТОР-01): всё в медных; 1 зм = 10 см = 100 мм ----
inline std::string money(long long cp) {
    if (cp == 0) return "0 мм";
    std::string r = cp < 0 ? "−" : "";
    cp = std::llabs(cp);
    const long long g = cp / 100, s = cp / 10 % 10, c = cp % 10;
    auto add = [&](long long v, const char* u) {
        if (!v) return;
        if (r.size() > 3) r += " ";
        r += std::to_string(v) + " " + u;
    };
    add(g, "зм"), add(s, "см"), add(c, "мм");
    return r;
}
// «12 зм», «5 см», «3 мм», «1 зм 5 см», число — медные
inline long long parseMoney(const std::string& t) {
    long long tot = 0, v = 0;
    bool have = false;
    for (size_t i = 0; i < t.size();) {
        const unsigned char c = t[i];
        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0'), have = true, i++;
            continue;
        }
        auto at = [&](const char* u) { return t.compare(i, std::strlen(u), u) == 0; };
        if (at("зм")) tot += v * 100, v = 0, have = false, i += std::strlen("зм");
        else if (at("см")) tot += v * 10, v = 0, have = false, i += std::strlen("см");
        else if (at("мм")) tot += v, v = 0, have = false, i += std::strlen("мм");
        else i++;
    }
    return tot + (have ? v : 0);
}

// ---- уровень по бюджету (решение 4 ноября 2026: 200 → 8600 за 50 уровней, «×1,08 за уровень») ----
// Шаг нормирован так, чтобы 50-й был ровно 8600: 43^(1/49) ≈ 1,0798 (чистое ×1,08 дало бы 8654); 2-й уровень — с 216,
// как в «Масштабах и ориентирах»
inline double levelBudget(int L) { return 200.0 * std::pow(43.0, (L - 1) / 49.0); }
inline int levelOf(long long budget) {
    if (budget <= 200) return 1;
    return 1 + (int)std::floor(std::log(budget / 200.0) / std::log(43.0) * 49.0 + 1e-9);
}

// ---- местность пути (канон «Масштабы и ориентиры» + DMG; названия — как в таблицах Акта I) ----
struct Terrain {
    const char* name;
    double mult;     // множитель скорости вне дороги (по дороге — ×1)
    int navDC;       // навигация вне дороги (DMG: 5 / 10 / 15), 0 — не сбиться
    int forageDC;    // поиск пропитания (DMG: 10 изобилие / 15 мало / 20 почти нет), 0 — негде
    int encounter;   // порог встречи к20 (Акт I)
};
inline const std::vector<Terrain>& terrains() {
    static const std::vector<Terrain> T = {
        {"Пашни и сёла", 0.75, 5, 10, 17}, {"Лес", 0.5, 15, 10, 14},   {"Холмы и плато", 0.5, 10, 15, 15},
        {"Горы", 0.33, 15, 20, 14},        {"Болото", 0.33, 15, 15, 13}, {"Река и переправа", 0.75, 10, 10, 15},
        {"Город", 1.0, 0, 0, 16},
        // 7 — вода (не пройти: путь по ней не ведёт, днём сюда не попасть); 8, 9 — для миров из генератора (Р23)
        {"Вода", 0.0, 0, 0, 20},           {"Пустыня", 0.5, 10, 20, 15}, {"Снега и льды", 0.5, 10, 20, 15},
    };
    return T;
}
inline int terrainIdx(const std::string& n) {
    const auto& T = terrains();
    for (size_t i = 0; i < T.size(); i++)
        if (n == T[i].name) return (int)i;
    return -1;
}

// ---- карта региона (Б1): сетка местности в долготе/широте (web/data/region_*.json, tools/region_act1.py) ----
// коды клеток — индексы terrains(), 7 — вода (не пройти); road, river — флаги клеток
struct Region {
    int w = 0, h = 0;
    double lon0 = 0, lat0 = 0, dlon = 1, dlat = 1;   // lat0 — северный край, строка 0 — север
    std::string terr, road, river;
    bool ok() const { return w > 0 && h > 0 && (int)terr.size() == w * h; }
    int cell(double lon, double lat) const {
        const int i = (int)std::floor((lon - lon0) / dlon), j = (int)std::floor((lat0 - lat) / dlat);
        return i >= 0 && i < w && j >= 0 && j < h ? j * w + i : -1;
    }
    int terrAt(int c) const { return c < 0 ? 7 : terr[c] - '0'; }
    bool roadAt(int c) const { return c >= 0 && road.size() == terr.size() && road[c] == '1'; }
    bool riverAt(int c) const { return c >= 0 && river.size() == terr.size() && river[c] == '1'; }
};
const int TERR_FIELDS = 0, TERR_FOREST = 1, TERR_HILLS = 2, TERR_MOUNT = 3, TERR_SWAMP = 4, TERR_RIVER = 5, TERR_TOWN = 6, TERR_WATER = 7,
          TERR_DESERT = 8, TERR_SNOW = 9, TERR_N = 10;
inline double kmBetween(double lo1, double la1, double lo2, double la2) {   // равнопромежуточная проекция: на сотнях км хватает
    const double k = M_PI / 180.0, x = (lo2 - lo1) * std::cos((la1 + la2) * 0.5 * k) * 111.32, y = (la2 - la1) * 110.57;
    return std::sqrt(x * x + y * y);
}

// путник: км в день по дороге (канон); отряд идёт со скоростью самого медленного
struct Traveler {
    const char* name;
    int km;
};
inline const std::vector<Traveler>& travelers() {
    static const std::vector<Traveler> V = {{"пеший", 35}, {"всадник", 47}, {"гонец", 85}, {"армия", 25}, {"обоз", 15}};
    return V;
}
// темп (PHB): медленный 18 миль / обычный 24 / быстрый 30 — множитель к канону
enum Pace { PACE_SLOW, PACE_NORMAL, PACE_FAST };
inline double paceMult(int p) { return p == PACE_SLOW ? 0.75 : (p == PACE_FAST ? 1.25 : 1.0); }
inline const char* paceName(int p) { return p == PACE_SLOW ? "медленный" : (p == PACE_FAST ? "быстрый" : "обычный"); }

// уровень жизни (PHB), медных в день
struct Lifestyle {
    const char* name;
    int cp;
};
inline const std::vector<Lifestyle>& lifestyles() {
    static const std::vector<Lifestyle> L = {{"нищий", 0},       {"жалкий", 10},  {"бедный", 20},          {"скромный", 100},
                                             {"комфортный", 200}, {"богатый", 400}, {"аристократический", 1000}};
    return L;
}

// роли в пути (Р18, PHB)
enum Role { R_NONE, R_GUIDE, R_LOOKOUT, R_FORAGER, R_MAPPER, R_TRACKER, R_HELP, R_N };
inline const char* roleName(int r) {
    static const char* N[R_N] = {"без занятия", "проводник", "дозорный", "добытчик", "картограф", "следопыт", "помощь"};
    return N[r];
}
inline int roleIdx(const std::string& n) {
    for (int r = 0; r < R_N; r++)
        if (n == roleName(r)) return r;
    return R_NONE;
}

// отношение NPC (DMG): ступень −1 враждебный / 0 безразличный / +1 дружелюбный, внутри шкала −2…+2
inline const char* attName(int tier) { return tier < 0 ? "враждебный" : (tier > 0 ? "дружелюбный" : "безразличный"); }
inline int attTier(const std::string& n) { return n == "враждебный" ? -1 : (n == "дружелюбный" ? 1 : 0); }

}  // namespace kmp
