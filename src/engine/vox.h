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
constexpr int VZ0 = 2;      // кубов под уровнем 0 карты: дно ущелья (как в клиенте web/js/55_render3d.js)
constexpr int VCH = 16;     // сторона чанка
constexpr int VH_MAX = 64;  // высота мира в кубах (320 фт, решение ТЗ 3.1)

struct VoxWorld {
    int sx = 0, sy = 0, sz = 0;        // размер в кубах: x, y — как арена, z — высота
    int cx = 0, cy = 0, cz = 0;        // чанков по осям
    std::vector<unsigned char> mat;    // материал: чанк за чанком, внутри чанка — z, y, x
    std::vector<unsigned> chunkVer;    // версия чанка: растёт при каждом изменении куба в нём
    std::unordered_map<int, short> dmg;   // повреждённые кубы: номер куба (idx) → хиты осталось
    std::vector<std::array<int, 4>> changes;   // изменения для кадра: x, y, z, новый материал
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
    }
    // поставить материал (стройка, конвертер, разрушение): старые повреждения куба сбрасываются
    void set(int x, int y, int z, unsigned char m, bool log = false) {
        if (!in(x, y, z)) return;
        const int i = idx(x, y, z);
        if (mat[(size_t)i] == m) return;
        mat[(size_t)i] = m;
        dmg.erase(i);
        chunkVer[(size_t)(i / (VCH * VCH * VCH))]++;
        if (log) changes.push_back({x, y, z, m});
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
inline void voxFromWorld(VoxWorld& v, const World& w) {
    int top = 0;
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) top = std::max({top, (int)w.height[y][x] + std::max(WALL_UP, (int)w.tree[y][x]), (int)w.deck[y][x]});
    v.init(w.aw, w.ah, VZ0 + top + 2);
    static thread_local int dk[W][W];   // изменения арены в клетке: бит вида (не на стеке: 160 КБ)
    for (int y = 0; y < w.ah; y++) std::fill(dk[y], dk[y] + w.aw, 0);
    for (const auto& d : w.dyn)
        if (d[0] >= 0 && d[1] >= 0 && d[0] < w.aw && d[1] < w.ah) {
            if (d[2] >= 6 && d[2] <= 8) dk[d[1]][d[0]] &= ~((1 << 6) | (1 << 7) | (1 << 8));   // дверь — последнее состояние
            dk[d[1]][d[0]] |= 1 << d[2];
        }
    for (int y = 0; y < w.ah; y++)
        for (int x = 0; x < w.aw; x++) {
            const int g = VZ0 + w.height[y][x], b = dk[y][x];
            auto put = [&](int z0, int z1, unsigned char m) { for (int z = std::max(0, z0); z < std::min(v.sz, z1); z++) v.set(x, y, z, m); };
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
            if (w.door[y][x]) {
                const int st = (b & (1 << 6)) ? 6 : (b & (1 << 7)) ? 7 : (b & (1 << 8)) ? 8 : 0;
                const bool open = st ? st == 6 : w.door[y][x] == 2;
                put(g + 2, g + WALL_UP, VX_STONE);
                if (!open && st != 8) put(g, g + 2, VX_DOOR);
            }
            if (w.deck[y][x] && !(b & (1 << 5)) && VZ0 + w.deck[y][x] - 1 >= g) put(VZ0 + w.deck[y][x] - 1, VZ0 + w.deck[y][x], VX_PLANK);
            if (b & (1 << 1)) put(g, g + 4, VX_FORCE);
        }
    v.changes.clear();
}
