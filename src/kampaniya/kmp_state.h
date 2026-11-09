// ============================================================================
// Кампания: состояние (герои, отряд, NPC, задания) и проверки характеристик и навыков (А1, НАВ-01, НАВ-02).
// ============================================================================
#pragma once
#include "kmp_core.h"

namespace kmp {

struct Hero {
    std::string name, charJson;   // описание персонажа — как в конструкторе (char_spec.h)
    Boss b;
    long long budget = 0;
    int level = 1, hp = 1, hpMax = 1, hd = 1, hdMax = 1, exh = 0, daysNoFood = 0;   // hd — кости хитов (Р13)
    long long purse = 0;                    // медные
    std::map<std::string, int> inv;         // вещь → число
    std::vector<std::string> tools, langs;  // названные инструменты и языки (поле "tools"/"langs" описания, если список)
    int role = R_NONE, helpTo = -1;         // занятие в пути; кому помогает
    bool dead = false;
};

struct Quest {   // каскад выплат за сведения (Акт I): первая новая — полная, дальше × шаг, подтверждение — × доля
    std::string id;
    long long tier = 0;   // сколько сейчас платят за новые сведения, медные
    double step = 0.9, confirm = 0.5;
    std::set<std::string> turnedIn;   // какие сведения уже сданы (кем угодно)
};

struct Npc {
    std::string name;
    int tier = 0, shift = 0;          // отношение: ступень и шкала внутри (−2…+2)
    std::map<int, int> skillMod;      // итоговые модификаторы навыков (из карточки), если нет сборки
};

struct State {
    uint64_t seed = 1;
    int n = 0;                        // событий в журнале
    int day = 1, hour = 8;
    std::vector<Hero> heroes;
    long long purse = 0;              // кошелёк отряда
    std::map<std::string, int> inv;   // общий груз
    double food = 0, water = 0;       // фунты еды, галлоны воды (общие)
    int pace = PACE_NORMAL, traveler = 0, lifestyle = 3;
    bool returnTrip = false;
    double km = 0;                    // пройдено всего
    std::string place;                // где отряд (место региона или описание)
    bool hasPos = false;              // позиция на карте региона (Б1) и маршрут — пункты, которые ещё впереди (Б2)
    double lon = 0, lat = 0;
    std::vector<std::pair<double, double>> route;
    std::set<std::string> facts;      // сведения отряда
    std::map<std::string, Npc> npcs;
    std::map<std::string, int> rep;   // фракция → известность
    std::map<std::string, Quest> quests;
    std::string dlgNpc, dlgNode;      // разговор: с кем и в каком узле дерева (пусто — разговора нет)
    bool dlgShop = false;             // в разговоре открыта лавка NPC
    std::vector<std::string> log;     // короткие строки для экрана
};

inline int heroIdx(const State& s, const std::string& name) {
    for (size_t i = 0; i < s.heroes.size(); i++)
        if (s.heroes[i].name == name) return (int)i;
    return -1;
}

// ---- проверки ----
inline int skillIdx(const std::string& n) {
    for (int k = 0; k < SK_N; k++)
        if (n == SKN[k]) return k;
    return -1;
}
inline int statIdx(const std::string& n) {
    for (int k = 0; k < STAT_N; k++)
        if (n == SN[k]) return k;
    return -1;
}

// модификатор навыка — та же формула, что Fighter::skill (world.h): характеристика + мастерство (владение) + ещё раз (экспертиза)
inline int heroSkill(const Boss& b, int k) {
    return b.mod(SKST[k]) + (((b.skillProf >> k) & 1u) ? b.prof : 0) + (((b.skillExp >> k) & 1u) ? b.prof : 0);
}
// правило «когда → что: преимущество на навык» (rules.h, skillAdv) вне боя: условия о себе, без противника
inline bool heroSkillAdv(const Hero& h, int k) {
    for (const Rule& r : h.b.rules) {
        if (r.what != E_SKILLADV || r.n != k) continue;
        if (r.when == W_ALWAYS || r.when == W_NOCOND) return true;
        if (r.when == W_HPLOW && h.hp * 2 <= h.hpMax) return true;
        if (r.when == W_HPHIGH && h.hp * 2 > h.hpMax) return true;
    }
    return false;
}

struct Check {
    std::string who, what;
    int r1 = 0, r2 = 0, die = 0, mod = 0, dc = 0, total = 0;
    bool adv = false, dis = false, ok = false;
    std::string parts;   // из чего сложен модификатор — для журнала
    std::string json() const {
        return "{\"who\":" + q(who) + ",\"what\":" + q(what) + ",\"d20\":[" + num(r1) + (r2 ? "," + num(r2) : "") + "],\"die\":" + num(die) +
               ",\"mod\":" + num(mod) + ",\"parts\":" + q(parts) + ",\"adv\":" + bl(adv) + ",\"dis\":" + bl(dis) + ",\"dc\":" + num(dc) +
               ",\"total\":" + num(total) + ",\"ok\":" + bl(ok) + "}";
    }
};

// модификатор и преимущество/помеха проверки героя: what — навык, характеристика или «инструмент:Название»
inline bool checkMod(const Hero& h, const std::string& what, int& mod, bool& adv, bool& dis, std::string& parts, std::string& err) {
    const int k = skillIdx(what), st = statIdx(what);
    if (k >= 0) {
        const int sm = h.b.mod(SKST[k]);
        mod = heroSkill(h.b, k);
        parts = std::string(SN[SKST[k]]) + " " + (sm >= 0 ? "+" : "") + num(sm);
        if ((h.b.skillProf >> k) & 1u) parts += ", владение +" + num(h.b.prof);
        if ((h.b.skillExp >> k) & 1u) parts += ", экспертиза +" + num(h.b.prof);
        if (heroSkillAdv(h, k)) adv = true, parts += ", правило: преимущество";
    } else if (st >= 0) {
        mod = h.b.mod(st);
        parts = std::string(SN[st]) + " " + (mod >= 0 ? "+" : "") + num(mod);
    } else if (what.rfind("инструмент:", 0) == 0) {
        // проверка инструментом: характеристику выбирает мастер (поле stat), иначе ЛОВ; +мастерство, если владеет
        mod = 0, parts = what;
        const std::string tool = what.substr(std::strlen("инструмент:"));
        for (auto& t : h.tools)
            if (t == tool) mod += h.b.prof, parts += ", владение +" + num(h.b.prof);
    } else {
        err = "неизвестная проверка: " + what;
        return false;
    }
    if (h.exh >= 1) dis = true, parts += ", истощение: помеха";   // истощение 1+ — помеха на проверки (Р12)
    return true;
}

inline int d20(Dice& dc, bool adv, bool dis, int& r1, int& r2) {
    r1 = dc.d(20), r2 = 0;
    if (adv == dis) return r1;   // гасятся (правила §7)
    r2 = dc.d(20);
    return adv ? std::max(r1, r2) : std::min(r1, r2);
}

// проверка против СЛ; extraMod — поправка мастера или обстоятельств
inline bool rollCheck(Dice& dc, const Hero& h, const std::string& what, int DC, bool adv, bool dis, int extraMod, Check& c, std::string& err) {
    c = Check();
    c.who = h.name, c.what = what, c.dc = DC;
    bool a = adv, d = dis;
    if (!checkMod(h, what, c.mod, a, d, c.parts, err)) return false;
    if (extraMod) c.mod += extraMod, c.parts += ", поправка " + std::string(extraMod > 0 ? "+" : "") + num(extraMod);
    c.adv = a, c.dis = d;
    c.die = d20(dc, a, d, c.r1, c.r2);
    c.total = c.die + c.mod;
    c.ok = c.total >= DC;
    return true;
}

// пассивная проверка (PHB): 10 + модификатор, ±5 за преимущество / помеху
inline int passive(const Hero& h, const std::string& what, int extraMod = 0) {
    int mod = 0;
    bool a = false, d = false;
    std::string p, e;
    if (!checkMod(h, what, mod, a, d, p, e)) return 0;
    return 10 + mod + extraMod + (a && !d ? 5 : 0) - (d && !a ? 5 : 0);
}

}  // namespace kmp
