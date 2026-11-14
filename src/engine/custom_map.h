// ============================================================================
// Карты: запись арены в JSON (для просмотрщика и редактора) и пользовательские карты игрока.
// Подключается из движка (fight.h) перед fight().
//
// Карта (JSON): {"aw":20,"ah":15,"theme":0,"wall":[строки],"pit":[..],"water":[..],"diff":[..],"climb":[..],"dark":[..],"height":[..],"tree":[..],"haz":[..],"ladder":[..],"deck":[..],"door":[..],"strong":[..],"starts":[[x,y],[x,y]]}
// Каждый слой — ah строк по aw символов: '1' — есть (для height — '0','1','2' уровень). Недостающие строки и символы считаются пустыми.
//   wall — стена; pit — ущелье (пешком не пройти, обзор не закрывает, мост — клетки без ущелья в нём); water — ручей (труднопроходим);
//   diff — труднопроходимая местность; climb — укрытие (10 фт движения, полукрытие); dark — тьма; height — возвышенность;
//   tree — дерево (обзор закрывает, внизу не пройти — только залезть на него: вершина на 20 фт выше);
//   haz — опасная зона: '0' нет, '1' лава, '2' кислота, '3' ядовитый дым, '4' ледяное поле, '5' грозовое поле, '6' некротический туман;
//   height — уровень земли '0'..'9' (по 10 фт; перепад больше 1 уровня — обрыв: только по лестнице или летуну под небом);
//   ladder — лестница ('1'); deck — второй ярус '0'..'9' (мост, балкон, навес; без доступа — потолок), выше земли клетки;
//   door — дверь '0' нет, '1' закрыта, '2' открыта (в проёме: на стене не ставится); strong — крепкая стена ('1');
//   starts — клетки начала боя для сторон A и B (необязательно: тогда случайные).
// ============================================================================
#pragma once
#include "mini_json.h"

static std::string mapToJson(const World& w) {
    auto layer = [&](const char* name, auto cell) {
        std::string s = std::string("\"") + name + "\":[";
        for (int y = 0; y < w.ah; y++) {
            s += y ? ",\"" : "\"";
            for (int x = 0; x < w.aw; x++) s += cell(y, x);
            s += "\"";
        }
        return s + "]";
    };
    std::string s = "{\"w\":" + std::to_string(W) + ",\"aw\":" + std::to_string(w.aw) + ",\"ah\":" + std::to_string(w.ah) + ",\"theme\":" + std::to_string(w.theme) + ",\"day\":" + (w.day ? "1" : "0") + ",";
    s += layer("wall", [&](int y, int x) { return w.wall[y][x] ? '1' : '0'; }) + ",";
    s += layer("climb", [&](int y, int x) { return w.climb[y][x] ? '1' : '0'; }) + ",";
    s += layer("diff", [&](int y, int x) { return w.diff[y][x] && !w.water[y][x] ? '1' : '0'; }) + ",";
    s += layer("dark", [&](int y, int x) { return w.dark[y][x] ? '1' : '0'; }) + ",";
    s += layer("water", [&](int y, int x) { return w.water[y][x] ? '1' : '0'; }) + ",";
    s += layer("pit", [&](int y, int x) { return w.pit[y][x] ? '1' : '0'; }) + ",";
    s += layer("height", [&](int y, int x) { return (char)('0' + w.height[y][x]); }) + ",";
    s += layer("tree", [&](int y, int x) { return w.tree[y][x] ? '1' : '0'; }) + ",";
    s += layer("haz", [&](int y, int x) { return (char)('0' + w.haz[y][x]); }) + ",";
    s += layer("ladder", [&](int y, int x) { return w.ladder[y][x] ? '1' : '0'; }) + ",";
    s += layer("deck", [&](int y, int x) { return (char)('0' + w.deck[y][x]); }) + ",";
    s += layer("door", [&](int y, int x) { return (char)('0' + w.door[y][x]); }) + ",";
    s += layer("strong", [&](int y, int x) { return w.strong[y][x] ? '1' : '0'; });
    if (w.wseed) s += ",\"world\":{\"seed\":\"" + std::to_string(w.wseed) + "\",\"size\":" + std::to_string(w.wsize) + ",\"x\":" + std::to_string(w.wx) + ",\"y\":" + std::to_string(w.wy) + ",\"step\":" + std::to_string(w.wstep) + "}";
    return s + "}";
}

struct CustomMap {
    bool on = false;
    int aw = 0, ah = 0, theme = 0;
    bool parts = false;   // карта из генератора (участок по местности): несвязные карманы можно, бой — в самой большой части
    std::vector<std::string> wall, climb, diff, dark, water, pit, height, tree, haz, ladder, deck, door, strong;
    int start[2][2] = {{-1, -1}, {-1, -1}};
    bool hasStarts() const { return start[0][0] >= 0 && start[1][0] >= 0; }
};
thread_local CustomMap g_customMap;

static void readLayer(const Json& j, std::vector<std::string>& out, int aw, int ah) {
    out.assign(ah, std::string(aw, '0'));
    for (int y = 0; y < ah; y++) {
        const std::string row = j[(size_t)y].str();
        for (int x = 0; x < aw && x < (int)row.size(); x++) out[y][x] = row[x];
    }
}

static void applyCustomMap(World& w, const CustomMap& m);
// Разбор и проверка пользовательской карты. Правила: размер 5–100, свободные клетки связаны, начальные клетки свободны
static bool parseCustomMap(const Json& j, CustomMap& m, std::string& err) {
    m = CustomMap();
    m.aw = j["aw"].num(0);
    m.ah = j["ah"].num(0);
    m.theme = j["theme"].num(0) ? 1 : 0;
    m.parts = j["parts"].flag(false);
    if (m.aw < 5 || m.ah < 5 || m.aw > W || m.ah > W) { err = "размер карты от 5×5 до " + std::to_string(W) + "×" + std::to_string(W) + " клеток"; return false; }
    readLayer(j["wall"], m.wall, m.aw, m.ah);
    readLayer(j["climb"], m.climb, m.aw, m.ah);
    readLayer(j["diff"], m.diff, m.aw, m.ah);
    readLayer(j["dark"], m.dark, m.aw, m.ah);
    readLayer(j["water"], m.water, m.aw, m.ah);
    readLayer(j["pit"], m.pit, m.aw, m.ah);
    readLayer(j["height"], m.height, m.aw, m.ah);
    readLayer(j["tree"], m.tree, m.aw, m.ah);
    readLayer(j["haz"], m.haz, m.aw, m.ah);
    readLayer(j["ladder"], m.ladder, m.aw, m.ah);
    readLayer(j["deck"], m.deck, m.aw, m.ah);
    readLayer(j["door"], m.door, m.aw, m.ah);
    readLayer(j["strong"], m.strong, m.aw, m.ah);
    auto blocked = [&](int x, int y) { return m.wall[y][x] == '1' || m.pit[y][x] == '1'; };
    for (int i = 0; i < 2; i++) {
        const Json& p = j["starts"][(size_t)i];
        if (p.t != Json::ARR) continue;
        const int x = p[0].num(-1), y = p[1].num(-1);
        if (x < 0 || y < 0 || x >= m.aw || y >= m.ah || blocked(x, y) || m.tree[y][x] == '1' || (m.door[y][x] != '0' && m.door[y][x] != 0)) { err = std::string("начальная клетка стороны ") + (i ? "B" : "A") + " на стене, дереве, в ущелье или за краем карты"; return false; }
        m.start[i][0] = x;
        m.start[i][1] = y;
    }
    if ((m.start[0][0] >= 0) != (m.start[1][0] >= 0)) { err = "задайте обе начальные клетки (A и B) или ни одной"; return false; }
    if (m.hasStarts() && m.start[0][0] == m.start[1][0] && m.start[0][1] == m.start[1][1]) { err = "начальные клетки A и B совпадают"; return false; }
    // связность: средний пешеход по правилам движка (обрывы — только лестницей, на дерево — залезть, под настилом — по росту)
    // доходит от любой клетки земли до любой; настил без доступа — просто потолок
    static thread_local World tw;
    m.on = true;
    applyCustomMap(tw, m);
    m.on = false;
    Fighter walker;
    walker.b.size = 2;
    std::vector<char> seen((size_t)m.aw * m.ah * 2, 0);
    auto id = [&](int x, int y, int l) { return (l * m.ah + y) * m.aw + x; };
    int freeN = 0, sx = -1, sy = -1;
    for (int y = 0; y < m.ah; y++)
        for (int x = 0; x < m.aw; x++)
            if (nodeOK(tw, walker, x, y, 0)) {
                freeN++;
                if (sx < 0 && !tw.tree[y][x]) { sx = x; sy = y; }
            }
    if (freeN < 4 || sx < 0) { err = "на карте меньше четырёх свободных клеток"; return false; }
    if (m.parts) {   // как арены из мира: обрезанные окном дома и замкнутые уступы — карманы; бой идёт в основной части
        if (markMain(tw) < std::max(4, freeN / 5) || startCellsN(tw) < 2) { err = "на участке мало связного места для боя"; return false; }
        m.on = true;
        return true;
    }
    std::vector<std::array<int, 3>> st{{sx, sy, 0}};
    seen[id(sx, sy, 0)] = 1;
    int reach = 0;
    while (!st.empty()) {
        const auto c = st.back();
        st.pop_back();
        if (!c[2]) reach++;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                for (int nl = 0; nl < 2; nl++) {
                    const int nx = c[0] + dx, ny = c[1] + dy;
                    if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= m.aw || ny >= m.ah || seen[id(nx, ny, nl)] || !stepOK(tw, walker, c[0], c[1], c[2], nx, ny, nl)) continue;
                    seen[id(nx, ny, nl)] = 1;
                    st.push_back({nx, ny, nl});
                }
    }
    if (reach != freeN) {
        int ux = -1, uy = -1;
        for (int y = 0; y < m.ah && ux < 0; y++)
            for (int x = 0; x < m.aw; x++)
                if (nodeOK(tw, walker, x, y, 0) && !seen[id(x, y, 0)]) { ux = x; uy = y; break; }
        err = "карта разделена на части: " + std::to_string(freeN - reach) + " клеток недостижимы пешком (например, клетка " + std::to_string(ux) + "," + std::to_string(uy) + "). Добавьте проход, мост, подъём уступами или лестницу у обрыва";
        return false;
    }
    m.on = true;
    return true;
}

static void applyCustomMap(World& w, const CustomMap& m) {
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
    w.wseed = 0;
    w.wstep = 1;
    w.aw = m.aw;
    w.ah = m.ah;
    w.theme = m.theme;
    for (int y = 0; y < W; y++)
        for (int x = 0; x < W; x++) {
            if (x >= m.aw || y >= m.ah) { w.wall[y][x] = true; continue; }
            w.wall[y][x] = m.wall[y][x] == '1';
            w.pit[y][x] = m.pit[y][x] == '1' && !w.wall[y][x];
            w.water[y][x] = m.water[y][x] == '1' && !w.wall[y][x] && !w.pit[y][x];
            w.diff[y][x] = (m.diff[y][x] == '1' || w.water[y][x]) && !w.wall[y][x];
            w.climb[y][x] = m.climb[y][x] == '1' && !w.wall[y][x] && !w.pit[y][x];
            w.dark[y][x] = m.dark[y][x] == '1';
            const int h = m.height[y][x] - '0';
            w.height[y][x] = (unsigned char)(h >= 1 && h <= 9 && !w.pit[y][x] ? h : 0);
            w.ladder[y][x] = m.ladder[y][x] == '1' && !w.wall[y][x] && !w.pit[y][x];
            const int dr = m.door[y][x] - '0';
            w.door[y][x] = (unsigned char)(dr >= 1 && dr <= 2 && !w.wall[y][x] && !w.pit[y][x] ? dr : 0);
            w.strong[y][x] = m.strong[y][x] == '1' && w.wall[y][x];
            const int dk = m.deck[y][x] - '0';
            w.deck[y][x] = (unsigned char)(dk >= 1 && dk <= 9 && !w.wall[y][x] && dk > w.height[y][x] ? dk : 0);
            const int hz = m.haz[y][x] - '0';
            w.haz[y][x] = (unsigned char)(hz >= 1 && hz < HAZ_N && !w.wall[y][x] && !w.pit[y][x] ? hz : 0);
            w.tree[y][x] = m.tree[y][x] == '1' && !w.wall[y][x] && !w.pit[y][x] && !w.water[y][x] && !w.climb[y][x];
        }
}
