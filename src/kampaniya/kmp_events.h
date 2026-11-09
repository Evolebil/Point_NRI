// ============================================================================
// Кампания: события журнала — черновик (propose) и применение (apply). ТЗ §2.1, §3.1.
//
// Черновик бросает кости (Dice: зерно, номер события, попытка) и считает итог, ничего не меняя. Мастер принимает его,
// перебрасывает (попытка +1), правит итог (поле "edit":true) или отменяет. Принятое событие пишется в журнал и
// применяется: apply берёт готовые итоги из события и костей не бросает — повтор журнала даёт то же состояние.
//
// Виды ("type"): setup, hero, check, contest, group, day, rest, buy, sell, quest, learn, turnin, attitude, rep, gm, note,
// dialog. Разговор (Д3): дерево разбирает web/kampaniya.py; событие dialog несёт узел и действия узла ("do" — список
// событий без бросков: learn, gm, attitude, rep, note), а проверка из разговора — "dlg":{"npc","ok","okDo","fail","failDo"}:
// по её итогу разговор уходит в узел ok или fail и выполняются действия этого узла.
// ============================================================================
#pragma once
#include "kmp_state.h"

namespace kmp {

struct Content {   // из Obsidian (web/kampaniya.py): таблицы встреч Акта I и правила к ним
    std::map<std::string, int> thr;                            // местность → порог встречи
    std::map<std::string, std::vector<std::string>> table;     // местность → 12 строк к12
    std::map<std::string, std::string> ambush;                 // местность → «где засада — кто»
    int roadThr = 2, returnThr = -4, ambushMax = 3;            // на дороге порог +2, обратный путь −4, засада на 1–3
    std::shared_ptr<World> world;                              // мир кампании (может не быть: тогда местность дня задаёт мастер)
};
inline std::vector<std::pair<double, double>> routeOf(const Json& a) {
    std::vector<std::pair<double, double>> r;
    for (auto& p : a.a) r.push_back({p[0].dbl(0), p[1].dbl(0)});
    return r;
}
inline std::string ptJson(double x, double y) {   // точка, км
    char b[48];
    std::snprintf(b, sizeof b, "[%.3f,%.3f]", x, y);
    return b;
}
inline std::string routeJson(const std::vector<std::pair<double, double>>& r, size_t from = 0) {
    std::string o = "[";
    char b[64];
    for (size_t i = from; i < r.size(); i++) {
        std::snprintf(b, sizeof b, "%s[%.3f,%.3f]", i > from ? "," : "", r[i].first, r[i].second);
        o += b;
    }
    return o + "]";
}

inline int saveMod(const Hero& h, int st) { return h.b.mod(st) + (((h.b.saveProf >> st) & 1u) ? h.b.prof : 0); }
// спасбросок героя против СЛ; истощение 3+ — помеха на спасброски
inline bool rollSave(Dice& dc, const Hero& h, int st, int DC, Check& c) {
    c = Check();
    c.who = h.name, c.what = std::string("спасбросок ") + SN[st], c.dc = DC, c.mod = saveMod(h, st);
    c.parts = std::string(SN[st]) + " " + num(c.mod);
    c.dis = h.exh >= 3;
    if (c.dis) c.parts += ", истощение: помеха";
    c.die = d20(dc, false, c.dis, c.r1, c.r2);
    c.total = c.die + c.mod, c.ok = c.total >= DC;
    return c.ok;
}

inline std::string checksJson(const std::vector<Check>& v) {
    std::string r = "[";
    for (size_t i = 0; i < v.size(); i++) r += (i ? "," : "") + v[i].json();
    return r + "]";
}

// ---- черновик ----
// act — запрос игрока или мастера; who — кто просит (имя игрока или «мастер»); attempt — номер переброса
inline std::string propose(const State& s, const Content& ct, const Json& act, const std::string& who, int attempt, std::string& err) {
    const std::string type = act["type"].str();
    const int n = s.n + 1;
    Dice dc(s.seed, n, attempt);
    std::vector<Check> cs;
    std::string res;
    auto hero = [&](const std::string& name) -> const Hero* {
        const int i = heroIdx(s, name);
        if (i < 0) err = "нет героя «" + name + "»";
        return i < 0 ? nullptr : &s.heroes[i];
    };

    if (type == "check") {   // проверка навыка / характеристики / инструмента против СЛ
        const Hero* h = hero(act["hero"].str());
        if (!h) return "";
        Check c;
        if (!rollCheck(dc, *h, act["what"].str(), act["dc"].num(10), act["adv"].flag(), act["dis"].flag(), act["mod"].num(0), c, err)) return "";
        cs.push_back(c);
        res = "{\"ok\":" + bl(c.ok) + "}";
    } else if (type == "contest") {   // состязание: герой против NPC (модификатор с карточки) или против героя
        const Hero* h = hero(act["hero"].str());
        if (!h) return "";
        Check a, b;
        if (!rollCheck(dc, *h, act["what"].str(), 0, act["adv"].flag(), act["dis"].flag(), act["mod"].num(0), a, err)) return "";
        if (act["vsHero"].t == Json::STR) {
            const Hero* g = hero(act["vsHero"].str());
            if (!g || !rollCheck(dc, *g, act["vsWhat"].str(), 0, false, false, 0, b, err)) return "";
        } else {
            b.who = act["vs"].str("противник"), b.what = act["vsWhat"].str(), b.mod = act["vsMod"].num(0);
            b.parts = "с карточки " + num(b.mod);
            b.die = d20(dc, act["vsAdv"].flag(), act["vsDis"].flag(), b.r1, b.r2);
            b.total = b.die + b.mod;
        }
        a.dc = b.total, b.dc = a.total, a.ok = a.total > b.total, b.ok = b.total > a.total;   // ничья — никто (PHB: всё как было)
        cs.push_back(a), cs.push_back(b);
        res = "{\"win\":" + q(a.ok ? a.who : (b.ok ? b.who : "")) + ",\"diff\":" + num(a.total - b.total) + "}";
    } else if (type == "group") {   // групповая проверка: успех, если успешна хотя бы половина
        int ok = 0, cnt = 0;
        for (auto& h : s.heroes) {
            if (h.dead) continue;
            Check c;
            if (!rollCheck(dc, h, act["what"].str(), act["dc"].num(10), false, false, 0, c, err)) return "";
            cs.push_back(c), cnt++, ok += c.ok;
        }
        res = "{\"ok\":" + bl(cnt && ok * 2 >= cnt) + ",\"passed\":" + num(ok) + ",\"of\":" + num(cnt) + "}";
    } else if (type == "day") {   // день пути (Б3): путь, навигация, пропитание, встреча, еда и вода, траты на жизнь
        // по карте (Б2): есть карта, позиция и маршрут, а мастер не задал местность сам — местность и дорога из пути
        const bool byMap = ct.world && s.hasPos && !s.route.empty() && act["terrain"].t != Json::STR;
        std::string tn = act["terrain"].str("Пашни и сёла");
        bool road = act["road"].flag();
        const bool hot = act["hot"].flag();
        const int hours = std::clamp(act["hours"].num(8), 0, 24);
        // роли: {"имя": "проводник", ...}, помощь — {"имя": "помощь:кому"}
        std::vector<int> role(s.heroes.size(), R_NONE), helpTo(s.heroes.size(), -1);
        for (auto& kv : act["roles"].o) {
            const int i = heroIdx(s, kv.first);
            if (i < 0) { err = "нет героя «" + kv.first + "»"; return ""; }
            std::string r = kv.second.str();
            const size_t c = r.find(':');
            if (c != std::string::npos) helpTo[i] = heroIdx(s, r.substr(c + 1)), r = r.substr(0, c);
            role[i] = roleIdx(r);
        }
        auto helped = [&](int i) {
            for (size_t j = 0; j < role.size(); j++)
                if (role[j] == R_HELP && helpTo[j] == i) return true;
            return false;
        };
        auto has = [&](int r) {
            for (size_t i = 0; i < role.size(); i++)
                if (role[i] == r && !s.heroes[i].dead) return true;
            return false;
        };
        // путь: самый медленный путник × местность × темп × часы/8
        int pace = act["pace"].t == Json::STR ? (act["pace"].str() == "быстрый" ? PACE_FAST : (act["pace"].str() == "медленный" ? PACE_SLOW : PACE_NORMAL)) : s.pace;
        for (auto& h : s.heroes)   // перегруз (Р15) и истощение 5+ — подробности в Б9; пока только истощение
            if (!h.dead && h.exh >= 5) pace = PACE_SLOW;
        double budget = travelers()[s.traveler].km * paceMult(pace) * hours / 8.0;   // км «по дороге» на день
        for (auto& h : s.heroes)
            if (!h.dead && h.exh >= 2) { budget *= 0.5; break; }   // истощение 2 — скорость вдвое
        double mult[TERR_N];
        for (int t = 0; t < TERR_N; t++) mult[t] = std::max(0.01, terrains()[t].mult);
        mult[TERR_TOWN] = 1.0;
        Walk wk;
        if (byMap) {
            wk = walkDay(*ct.world, s.x, s.y, s.route, budget, mult);
            tn = terrains()[wk.terrain].name, road = wk.road;
        }
        const int ti = terrainIdx(tn);
        if (ti < 0) { err = "неизвестная местность: " + tn; return ""; }
        const Terrain& T = terrains()[ti];
        double km = byMap ? wk.km : budget * (road ? 1.0 : T.mult);
        // навигация вне дороги (DMG): проводник, иначе лучший по Выживанию с помехой; быстрый темп −5, картограф +2
        std::string nav = "null";
        bool lost = false;
        if (!road && T.navDC > 0) {
            int g = -1;
            for (size_t i = 0; i < role.size(); i++)
                if (role[i] == R_GUIDE && !s.heroes[i].dead) g = (int)i;
            bool noGuide = g < 0;
            if (noGuide)
                for (size_t i = 0; i < s.heroes.size(); i++)
                    if (!s.heroes[i].dead && (g < 0 || heroSkill(s.heroes[i].b, SK_SURVIVAL) > heroSkill(s.heroes[g].b, SK_SURVIVAL))) g = (int)i;
            if (g >= 0) {
                Check c;
                const int m = (pace == PACE_FAST ? -5 : 0) + (has(R_MAPPER) ? 2 : 0);
                rollCheck(dc, s.heroes[g], "Выживание", T.navDC, helped(g), noGuide, m, c, err);
                cs.push_back(c);
                lost = !c.ok;
                nav = "{\"by\":" + q(c.who) + ",\"ok\":" + bl(c.ok) + "}";
            }
        }
        if (lost) {   // сбились: день кружили — пройдена половина пути (мастер видит, где отряд на самом деле)
            if (byMap) wk = walkDay(*ct.world, s.x, s.y, s.route, budget * 0.5, mult), km = wk.km;
            else km *= 0.5;
        }
        // пропитание (DMG): только обычный и медленный темп; каждый добытчик — Выживание, успех: 1к6 + МДР фунтов и галлонов
        double gotF = 0, gotW = 0;
        if (pace != PACE_FAST && T.forageDC > 0)
            for (size_t i = 0; i < role.size(); i++) {
                if (role[i] != R_FORAGER || s.heroes[i].dead) continue;
                Check c;
                rollCheck(dc, s.heroes[i], "Выживание", act["forageDC"].num(T.forageDC), helped((int)i), false, 0, c, err);
                cs.push_back(c);
                if (c.ok) {
                    const int got = std::max(0, dc.d(6) + s.heroes[i].b.mod(WIS));
                    gotF += got, gotW += got;
                }
            }
        // дозор: лучшая пассивная Внимательность дозорных (быстрый темп −5) — мастер сравнивает со Скрытностью засады
        int look = 0;
        for (size_t i = 0; i < role.size(); i++)
            if (role[i] == R_LOOKOUT && !s.heroes[i].dead) look = std::max(look, passive(s.heroes[i], "Внимательность", pace == PACE_FAST ? -5 : 0));
        // встреча (Акт I): к20 ≥ порог местности (+2 на дороге, −4 на обратном пути), затем к12; обратный путь, 1–3 — засада
        const int thr = (ct.thr.count(tn) ? ct.thr.at(tn) : T.encounter) + (road ? ct.roadThr : 0) + (s.returnTrip ? ct.returnThr : 0);
        const int e20 = dc.d(20), e12 = dc.d(12);
        const bool hit = e20 >= thr, amb = hit && s.returnTrip && e12 <= ct.ambushMax;
        std::string text;
        if (hit) {
            if (amb) text = ct.ambush.count(tn) ? ct.ambush.at(tn) : "засада";
            else if (ct.table.count(tn) && (int)ct.table.at(tn).size() >= e12) text = ct.table.at(tn)[e12 - 1];
        }
        // еда и вода (PHB): по 1 фунту и 1 галлону (в жару 2) на героя; голод — 3 + ТЕЛ дней без истощения
        int alive = 0;
        for (auto& h : s.heroes) alive += !h.dead;
        const double needF = alive, needW = alive * (hot ? 2.0 : 1.0);
        double food = s.food + gotF, water = s.water + gotW;
        int fed = (int)std::min<double>(alive, std::floor(food + 1e-9));
        const double wShare = needW > 0 ? std::min(1.0, water / needW) : 1.0;
        std::string hs = "[";
        int hi = 0;
        for (auto& h : s.heroes) {
            if (h.dead) continue;
            int dExh = 0, dnf = h.daysNoFood;
            if (fed > 0) fed--, dnf = 0;
            else if (++dnf > std::max(1, 3 + h.b.mod(CON))) dExh++;
            Check c;
            if (wShare < 0.5) dExh++;
            else if (wShare < 1.0 && !rollSave(dc, h, CON, 15, c)) dExh++;
            if (c.who.size()) cs.push_back(c);
            for (int x = 1; x <= hours - 8; x++) {   // форсированный марш: каждый час сверх 8 — спасбросок ТЕЛ СЛ 10 + час
                Check m;
                if (!rollSave(dc, h, CON, 10 + x, m)) dExh++;
                cs.push_back(m);
            }
            hs += std::string(hi++ ? "," : "") + "{\"name\":" + q(h.name) + ",\"exh\":" + num(dExh) + ",\"daysNoFood\":" + num(dnf) + "}";
        }
        hs += "]";
        const long long pay = (long long)lifestyles()[s.lifestyle].cp * alive;
        res = "{\"terrain\":" + q(tn) + ",\"road\":" + bl(road) + ",\"pace\":" + num(pace) + ",\"hours\":" + num(hours) + ",\"km\":" + num(std::llround(km)) +
              ",\"nav\":" + nav + ",\"lost\":" + bl(lost) + ",\"enc\":{\"d20\":" + num(e20) + ",\"thr\":" + num(thr) + ",\"hit\":" + bl(hit) +
              ",\"d12\":" + num(e12) + ",\"ambush\":" + bl(amb) + ",\"text\":" + q(text) + "},\"lookout\":" + num(look) +
              ",\"forage\":{\"food\":" + num((long long)gotF) + ",\"water\":" + num((long long)gotW) + "},\"food\":" +
              num(std::llround(std::min(needF, food) - gotF) * -1) + ",\"water\":" + num(std::llround(std::min(needW, water) - gotW) * -1) +
              ",\"heroes\":" + hs + ",\"pay\":" + num(std::min(pay, s.purse)) + ",\"payShort\":" + bl(pay > s.purse) +
              (byMap ? ",\"pos\":" + ptJson(wk.x, wk.y) + ",\"route\":" + routeJson(s.route, wk.next) +
                           ",\"blocked\":" + bl(wk.blocked) : std::string()) + "}";
    } else if (type == "rest") {   // отдых (Б4): короткий — кости хитов по выбору, долгий — всё
        const bool lng = act["kind"].str() == "долгий";
        std::string hs = "[";
        int hi = 0;
        for (auto& h : s.heroes) {
            if (h.dead) continue;
            int hp = h.hp, hd = h.hd, exh = h.exh;
            if (lng) {
                hp = h.hpMax, hd = std::min(h.hdMax, h.hd + std::max(1, h.hdMax / 2));
                if (exh > 0 && h.daysNoFood == 0) exh--;   // −1 истощение при еде и воде
            } else {
                const int k = std::min(hd, act["spend"][h.name].num(0));
                for (int i = 0; i < k; i++) hp += std::max(0, dc.d(8) + h.b.mod(CON));   // кость хитов: к8 + ТЕЛ (Р13)
                hp = std::min(hp, h.hpMax), hd -= k;
            }
            hs += std::string(hi++ ? "," : "") + "{\"name\":" + q(h.name) + ",\"hp\":" + num(hp) + ",\"hd\":" + num(hd) + ",\"exh\":" + num(exh) + "}";
        }
        res = "{\"kind\":" + q(lng ? "долгий" : "короткий") + ",\"hours\":" + num(lng ? 8 : 1) + ",\"heroes\":" + hs + "]}";
    } else if (type == "buy" || type == "sell") {   // сделка (Г4): цена за штуку, продажа — за половину; торг (Р14)
        const int qty = std::max(1, act["qty"].num(1));
        long long unit = act["price"].t == Json::STR ? parseMoney(act["price"].str()) : (long long)act["price"].dbl(0);
        if (type == "sell") unit /= 2;
        int pct = 0;
        bool offend = false;
        if (act["haggle"].t == Json::OBJ) {   // состязание: Убеждение / Обман / Запугивание против Проницательности торговца
            const Json& hg = act["haggle"];
            const Hero* h = hero(hg["hero"].str());
            if (!h) return "";
            const std::string sk = hg["what"].str("Убеждение");
            Check a, b;
            if (!rollCheck(dc, *h, sk, 0, false, false, 0, a, err)) return "";
            b.who = hg["vs"].str("торговец"), b.what = "Проницательность", b.mod = hg["vsMod"].num(0), b.parts = "с карточки " + num(b.mod);
            b.die = d20(dc, false, false, b.r1, b.r2), b.total = b.die + b.mod;
            const int diff = a.total - b.total;
            pct = std::clamp((diff >= 0 ? diff / 5 : -((-diff + 4) / 5)) * 10, -30, 30);   // ±10% за каждые 5 разницы
            offend = diff <= -10 || sk == "Запугивание";
            a.dc = b.total, b.dc = a.total, a.ok = diff > 0, b.ok = diff < 0;
            cs.push_back(a), cs.push_back(b);
        }
        // покупка: выигрыш торга — дешевле; продажа — дороже
        const long long total = std::max(0LL, std::llround(unit * qty * (1.0 + (type == "buy" ? -pct : pct) / 100.0)));
        res = "{\"who\":" + q(act["who"].str("отряд")) + ",\"item\":" + q(act["item"].str()) + ",\"qty\":" + num(qty) + ",\"total\":" + num(total) +
              ",\"pct\":" + num(pct) + ",\"npc\":" + q(act["npc"].str()) + ",\"offend\":" + bl(offend) + "}";
    } else if (type == "route" && act["way"].t == Json::ARR) {   // маршрут через пункты (ТЗ v2 §4): путь ищет движок по режиму, к событию — путь и сводка
        if (!ct.world) { err = "у кампании нет мира"; return ""; }
        const Pts way = routeOf(act["way"]);
        if (way.empty()) { err = "в маршруте нет пунктов"; return ""; }
        const int mode = pathModeOf(act["mode"].str());
        double mult[TERR_N];
        for (int t = 0; t < TERR_N; t++) mult[t] = std::max(0.01, terrains()[t].mult);
        mult[TERR_TOWN] = 1.0;
        const double ax = s.hasPos ? s.x : way[0].first, ay = s.hasPos ? s.y : way[0].second;
        Pts path;
        if (!findRoute(*ct.world, ax, ay, way, mode, mult, path, err)) return "";
        const RouteSum sum = routeSummary(*ct.world, ax, ay, path, travelers()[s.traveler].km, mult);
        res = "{\"mode\":" + q(mode == PM_ROADS ? "по дорогам" : mode == PM_STRAIGHT ? "напрямик" : "быстрее всего") + ",\"path\":" + routeJson(path) +
              ",\"sum\":" + routeSumJson(sum) + "}";
    } else if (type == "turnin") {   // сдача сведений по каскаду (Е1); party — «мы» или имя соперника
        const std::string qid = act["quest"].str(), f = act["fact"].str();
        auto it = s.quests.find(qid);
        if (it == s.quests.end()) { err = "нет задания «" + qid + "»"; return ""; }
        const Quest& Q = it->second;
        const bool confirm = Q.turnedIn.count(f) > 0;
        const long long pay = confirm ? std::llround(Q.tier * Q.confirm) : Q.tier;
        const long long next = confirm ? Q.tier : std::llround(Q.tier * Q.step);
        res = "{\"quest\":" + q(qid) + ",\"fact\":" + q(f) + ",\"party\":" + q(act["party"].str("мы")) + ",\"confirm\":" + bl(confirm) + ",\"pay\":" + num(pay) +
              ",\"tier\":" + num(next) + "}";
    } else if (type == "setup" || type == "hero" || type == "quest" || type == "learn" || type == "attitude" || type == "rep" || type == "gm" || type == "note" ||
               type == "dialog" || type == "fog" || (type == "route" && act["way"].t != Json::ARR)) {
        // без бросков: событие = запрос как есть
        std::string body;
        for (auto& kv : act.o)
            if (kv.first != "type") body += ",\"" + kv.first + "\":" + jsonOut(kv.second);
        return "{\"n\":" + num(n) + ",\"type\":" + q(type) + ",\"by\":" + q(who) + ",\"attempt\":" + num(attempt) + body + "}";
    } else {
        err = "неизвестное событие: " + type;
        return "";
    }
    if (!err.empty()) return "";
    return "{\"n\":" + num(n) + ",\"type\":" + q(type) + ",\"by\":" + q(who) + ",\"attempt\":" + num(attempt) + ",\"hidden\":" + bl(act["hidden"].flag()) +
           ",\"act\":" + jsonOut(act) + ",\"checks\":" + checksJson(cs) + ",\"res\":" + res + "}";
}

// ---- применение принятого события ----
inline bool addHero(State& s, const Coef& C, const Json& ev, std::string& err) {
    Hero h;
    h.charJson = jsonOut(ev["char"]);
    if (!chr::buildCharacter(ev["char"], C, h.b, h.budget, err)) return false;
    h.name = ev["name"].str(ev["char"]["name"].str("герой"));
    if (heroIdx(s, h.name) >= 0) { err = "герой «" + h.name + "» уже есть"; return false; }
    h.level = levelOf(h.budget);
    h.hpMax = h.hp = h.b.hp;
    h.hdMax = h.hd = h.level;   // кости хитов: число = уровень (Р13)
    h.purse = ev["purse"].t == Json::STR ? parseMoney(ev["purse"].str()) : (long long)ev["purse"].dbl(0);
    for (auto& t : ev["char"]["passives"]["tools"].a) h.tools.push_back(t.str());
    for (auto& t : ev["char"]["langs"].a) h.langs.push_back(t.str());
    s.heroes.push_back(h);
    return true;
}

inline long long cpOf(const Json& j) { return j.t == Json::STR ? parseMoney(j.str()) : (long long)j.dbl(0); }

inline bool apply(State& s, const Coef& C, const Json& ev, std::string& err);
// действия узла разговора: события без бросков, номер — тот же, что у события разговора
inline bool applyDo(State& s, const Coef& C, const Json& list, int n, std::string& err) {
    for (auto& d : list.a) {
        const std::string t = d["type"].str();
        if (t != "learn" && t != "gm" && t != "attitude" && t != "rep" && t != "note") { err = "в разговоре нельзя: " + t; return false; }
        std::string j = jsonOut(d);
        j = "{\"n\":" + num(n) + "," + j.substr(1);
        if (!apply(s, C, parseJson(j), err)) return false;
    }
    return true;
}
// переход разговора: узел, лавка, конец
inline void dlgGo(State& s, const Json& d, const std::string& node) {
    if (d.has("npc")) s.dlgNpc = d["npc"].str();
    s.dlgNode = node, s.dlgShop = false;
    if (node.empty()) s.dlgNpc.clear();
}

inline bool apply(State& s, const Coef& C, const Json& ev, std::string& err) {
    const std::string type = ev["type"].str();
    const Json& r = ev["res"];
    auto say = [&](const std::string& t) { s.log.push_back("день " + num(s.day) + ": " + t); };
    if (type == "setup") {
        if (ev["seed"].t == Json::NUM) s.seed = (uint64_t)ev["seed"].dbl(1);
        for (size_t i = 0; i < travelers().size(); i++)
            if (ev["traveler"].str() == travelers()[i].name) s.traveler = (int)i;
        for (size_t i = 0; i < lifestyles().size(); i++)
            if (ev["lifestyle"].str() == lifestyles()[i].name) s.lifestyle = (int)i;
        if (ev["pace"].t == Json::STR) s.pace = ev["pace"].str() == "быстрый" ? PACE_FAST : (ev["pace"].str() == "медленный" ? PACE_SLOW : PACE_NORMAL);
        if (ev.has("food")) s.food = ev["food"].dbl(0);
        if (ev.has("water")) s.water = ev["water"].dbl(0);
        if (ev.has("purse")) s.purse = cpOf(ev["purse"]);
        if (ev.has("place")) s.place = ev["place"].str();
        if (ev.has("returnTrip")) s.returnTrip = ev["returnTrip"].flag();
        if (ev["pos"].t == Json::ARR) s.hasPos = true, s.x = ev["pos"][0].dbl(0), s.y = ev["pos"][1].dbl(0), s.route.clear(), s.fog.push_back({0, s.x, s.y, 0, 0, FOG_SIGHT});
    } else if (type == "route") {   // маршрут отряда: найденный движком путь (res.path) или, в старых событиях, пункты как есть
        s.route = routeOf(r["path"].t == Json::ARR ? r["path"] : ev["path"]);
        if (r["sum"].t == Json::OBJ) say("маршрут " + r["mode"].str() + ": " + num(r["sum"]["km"].num()) + " км, ≈ " + num(r["sum"]["days"].num()) + " дн.");
    } else if (type == "hero") {
        if (!addHero(s, C, ev, err)) return false;
        say("в отряде " + s.heroes.back().name);
    } else if (type == "check" || type == "contest" || type == "group") {
        for (auto& c : ev["checks"].a)
            say(c["who"].str() + ": " + c["what"].str() + " " + num(c["total"].num()) + (type == "check" ? (c["ok"].flag() ? " — успех" : " — провал") : ""));
        const Json& d = ev["act"]["dlg"];
        if (d.t == Json::OBJ) {   // проверка из разговора: успех (у состязания — победа говорящего) → ok, иначе → fail
            const bool ok = type == "contest" ? r["win"].str() == ev["act"]["hero"].str() : r["ok"].flag();
            dlgGo(s, d, d[ok ? "ok" : "fail"].str());
            if (!applyDo(s, C, d[ok ? "okDo" : "failDo"], ev["n"].num(s.n + 1), err)) return false;
            if (d[ok ? "okShop" : "failShop"].flag()) s.dlgShop = true;
        }
    } else if (type == "day") {
        s.day++, s.hour = 8 - 0;   // утро следующего дня
        s.km += r["km"].dbl(0);
        s.food = std::max(0.0, s.food + r["food"].dbl(0)), s.water = std::max(0.0, s.water + r["water"].dbl(0));
        s.purse -= (long long)r["pay"].dbl(0);
        s.pace = r["pace"].num(s.pace);
        if (r["pos"].t == Json::ARR) {   // путь дня открывает туман полосой вдоль пройденного
            const double ox = s.x, oy = s.y, had = s.hasPos;
            s.hasPos = true, s.x = r["pos"][0].dbl(s.x), s.y = r["pos"][1].dbl(s.y), s.route = routeOf(r["route"]);
            FogOp f;
            f.kind = had ? 2 : 0, f.x = had ? ox : s.x, f.y = had ? oy : s.y, f.x2 = s.x, f.y2 = s.y, f.r = FOG_SIGHT;
            s.fog.push_back(f);
        }
        for (auto& x : r["heroes"].a) {
            const int i = heroIdx(s, x["name"].str());
            if (i < 0) continue;
            Hero& h = s.heroes[i];
            h.exh = std::min(6, h.exh + x["exh"].num(0)), h.daysNoFood = x["daysNoFood"].num(h.daysNoFood);
            if (h.exh >= 6) h.dead = true;   // истощение 6 — смерть (Р12)
        }
        say(num(r["km"].num()) + " км, " + r["terrain"].str() + (r["lost"].flag() ? ", сбились с пути" : "") +
            (r["enc"]["hit"].flag() ? (r["enc"]["ambush"].flag() ? ", засада: " : ", встреча: ") + r["enc"]["text"].str() : ""));
    } else if (type == "rest") {
        for (auto& x : r["heroes"].a) {
            const int i = heroIdx(s, x["name"].str());
            if (i < 0) continue;
            s.heroes[i].hp = x["hp"].num(), s.heroes[i].hd = x["hd"].num(), s.heroes[i].exh = x["exh"].num();
        }
        s.hour += r["hours"].num(1);
        while (s.hour >= 24) s.hour -= 24, s.day++;
        say(r["kind"].str() + " отдых");
    } else if (type == "buy" || type == "sell") {
        const std::string who = r["who"].str("отряд"), item = r["item"].str();
        const long long tot = (long long)r["total"].dbl(0);
        const int qty = r["qty"].num(1), sg = type == "buy" ? 1 : -1;
        long long* purse = &s.purse;
        std::map<std::string, int>* inv = &s.inv;
        if (who != "отряд") {
            const int i = heroIdx(s, who);
            if (i < 0) { err = "нет героя «" + who + "»"; return false; }
            purse = &s.heroes[i].purse, inv = &s.heroes[i].inv;
        }
        if (sg > 0 && *purse < tot) { err = "не хватает денег: нужно " + money(tot) + ", есть " + money(*purse); return false; }
        if (sg < 0 && (*inv)[item] < qty) { err = "нечего продать: " + item; return false; }
        *purse -= sg * tot;
        if (((*inv)[item] += sg * qty) <= 0) inv->erase(item);
        if (r["offend"].flag() && r["npc"].str().size()) s.npcs[r["npc"].str()].shift--;
        say(std::string(sg > 0 ? "куплено: " : "продано: ") + item + " ×" + num(qty) + " за " + money(tot));
    } else if (type == "quest") {
        Quest Q;
        Q.id = ev["id"].str(), Q.tier = cpOf(ev["base"]), Q.step = ev["step"].dbl(0.9), Q.confirm = ev["confirm"].dbl(0.5);
        s.quests[Q.id] = Q;
    } else if (type == "learn") {
        s.facts.insert(ev["fact"].str());
        say("узнали: " + ev["fact"].str());
    } else if (type == "turnin") {
        Quest& Q = s.quests[r["quest"].str()];
        Q.tier = (long long)r["tier"].dbl(Q.tier);
        Q.turnedIn.insert(r["fact"].str());
        const long long pay = (long long)r["pay"].dbl(0);
        if (r["party"].str("мы") == "мы") s.purse += pay;
        say(r["party"].str("мы") + " сдали «" + r["fact"].str() + "»" + (r["confirm"].flag() ? " (подтверждение)" : "") + ": " + money(pay));
    } else if (type == "attitude") {
        Npc& p = s.npcs[ev["npc"].str()];
        p.name = ev["npc"].str();
        if (ev["tier"].t == Json::STR) p.tier = attTier(ev["tier"].str()), p.shift = 0;
        p.shift += ev["delta"].num(0);
        while (p.shift > 2 && p.tier < 1) p.tier++, p.shift -= 5;   // шкала −2…+2 внутри ступени: перелив — следующая ступень
        while (p.shift < -2 && p.tier > -1) p.tier--, p.shift += 5;
        p.shift = std::clamp(p.shift, -2, 2);
    } else if (type == "rep") {
        s.rep[ev["faction"].str()] += ev["delta"].num(0);
    } else if (type == "gm") {   // прямое действие мастера (МАС-04): любое из полей
        Hero* h = nullptr;
        if (ev["hero"].t == Json::STR) {
            const int i = heroIdx(s, ev["hero"].str());
            if (i < 0) { err = "нет героя «" + ev["hero"].str() + "»"; return false; }
            h = &s.heroes[i];
        }
        if (ev.has("purse")) (h ? h->purse : s.purse) += cpOf(ev["purse"]);
        if (ev.has("item")) {
            auto& inv = h ? h->inv : s.inv;
            if ((inv[ev["item"].str()] += ev["qty"].num(1)) <= 0) inv.erase(ev["item"].str());
        }
        if (h && ev.has("hp")) h->hp = std::clamp(ev["hp"].num(h->hp), 0, h->hpMax);
        if (h && ev.has("exh")) h->exh = std::clamp(ev["exh"].num(h->exh), 0, 6), h->dead = h->exh >= 6;
        if (h && ev.has("dead")) h->dead = ev["dead"].flag();
        if (ev.has("days")) s.day += ev["days"].num(0);
        if (ev.has("hours")) {
            s.hour += ev["hours"].num(0);
            while (s.hour >= 24) s.hour -= 24, s.day++;
        }
        if (ev.has("place")) s.place = ev["place"].str();
        if (ev["pos"].t == Json::ARR) s.hasPos = true, s.x = ev["pos"][0].dbl(0), s.y = ev["pos"][1].dbl(0), s.fog.push_back({0, s.x, s.y, 0, 0, FOG_SIGHT});   // телепорт отряда
        if (ev.has("food")) s.food = std::max(0.0, s.food + ev["food"].dbl(0));
        if (ev.has("water")) s.water = std::max(0.0, s.water + ev["water"].dbl(0));
        if (ev.has("fact")) s.facts.insert(ev["fact"].str());
        if (ev.has("returnTrip")) s.returnTrip = ev["returnTrip"].flag();
        if (ev.has("text")) say("мастер: " + ev["text"].str());
    } else if (type == "note") {
        say(ev["text"].str());
    } else if (type == "fog") {   // туман (Р28): мастер открывает или скрывает круг, открывает или скрывает всё
        const std::string op = ev["op"].str();
        FogOp f;
        f.kind = op == "скрыть" ? 1 : op == "всё" ? 3 : op == "ничего" ? 4 : 0;
        f.x = ev["x"].dbl(0), f.y = ev["y"].dbl(0), f.r = std::max(0.0, ev["r"].dbl(0));
        if (ev["pts"].t == Json::ARR && f.kind <= 1)   // мазок кисти: круги по точкам
            for (auto& p : ev["pts"].a) f.x = p[0].dbl(0), f.y = p[1].dbl(0), s.fog.push_back(f);
        else s.fog.push_back(f);
        if (f.kind >= 3) say(f.kind == 3 ? "игрокам открыта вся карта" : "карта скрыта от игроков");
    } else if (type == "dialog") {   // разговор: начать, перейти в узел (с его действиями), закончить (узел пустой)
        if (ev["npc"].t == Json::STR && ev["npc"].str() != s.dlgNpc && ev["node"].str().size()) {
            Npc& p = s.npcs[ev["npc"].str()];
            if (p.name.empty()) p.name = ev["npc"].str(), p.tier = attTier(ev["attitude"].str("безразличный"));   // первое знакомство — отношение с карточки
            say("разговор: " + ev["npc"].str());
        }
        dlgGo(s, ev, ev["node"].str());
        if (!applyDo(s, C, ev["do"], ev["n"].num(s.n + 1), err)) return false;
        s.dlgShop = ev["shop"].flag();
    } else {
        err = "неизвестное событие: " + type;
        return false;
    }
    s.n = std::max(s.n, ev["n"].num(s.n + 1));
    return true;
}

// ---- состояние для экрана ----
inline std::string stateJson(const State& s) {
    std::string o = "{\"n\":" + num(s.n) + ",\"day\":" + num(s.day) + ",\"hour\":" + num(s.hour) + ",\"place\":" + q(s.place) + ",\"km\":" + num(std::llround(s.km)) +
                    ",\"pace\":" + q(paceName(s.pace)) + ",\"traveler\":" + q(travelers()[s.traveler].name) + ",\"lifestyle\":" + q(lifestyles()[s.lifestyle].name) +
                    ",\"returnTrip\":" + bl(s.returnTrip) + ",\"pos\":" + (s.hasPos ? ptJson(s.x, s.y) : std::string("null")) +
                    ",\"route\":" + routeJson(s.route) + ",\"fogN\":" + num((long long)s.fog.size()) + ",\"purse\":" + num(s.purse) + ",\"purseText\":" + q(money(s.purse)) +
                    ",\"food\":" + num(std::llround(s.food)) + ",\"water\":" + num(std::llround(s.water)) + ",\"heroes\":[";
    for (size_t i = 0; i < s.heroes.size(); i++) {
        const Hero& h = s.heroes[i];
        o += std::string(i ? "," : "") + "{\"name\":" + q(h.name) + ",\"budget\":" + num(h.budget) + ",\"level\":" + num(h.level) + ",\"hp\":" + num(h.hp) +
             ",\"hpMax\":" + num(h.hpMax) + ",\"hd\":" + num(h.hd) + ",\"hdMax\":" + num(h.hdMax) + ",\"exh\":" + num(h.exh) + ",\"daysNoFood\":" + num(h.daysNoFood) +
             ",\"dead\":" + bl(h.dead) + ",\"purse\":" + num(h.purse) + ",\"inv\":{";
        int k = 0;
        for (auto& kv : h.inv) o += std::string(k++ ? "," : "") + q(kv.first) + ":" + num(kv.second);
        o += "},\"skills\":{";
        for (int sk = 0; sk < SK_N; sk++) o += std::string(sk ? "," : "") + q(SKN[sk]) + ":" + num(heroSkill(h.b, sk));
        o += "},\"passivePerception\":" + num(passive(h, "Внимательность")) + ",\"langs\":[";
        for (size_t j = 0; j < h.langs.size(); j++) o += std::string(j ? "," : "") + q(h.langs[j]);
        o += "],\"tools\":[";
        for (size_t j = 0; j < h.tools.size(); j++) o += std::string(j ? "," : "") + q(h.tools[j]);
        o += "]}";
    }
    o += "],\"inv\":{";
    int k = 0;
    for (auto& kv : s.inv) o += std::string(k++ ? "," : "") + q(kv.first) + ":" + num(kv.second);
    o += "},\"facts\":[";
    k = 0;
    for (auto& f : s.facts) o += std::string(k++ ? "," : "") + q(f);
    o += "],\"npcs\":{";
    k = 0;
    for (auto& kv : s.npcs) o += std::string(k++ ? "," : "") + q(kv.first) + ":{\"attitude\":" + q(attName(kv.second.tier)) + ",\"shift\":" + num(kv.second.shift) + "}";
    o += "},\"rep\":{";
    k = 0;
    for (auto& kv : s.rep) o += std::string(k++ ? "," : "") + q(kv.first) + ":" + num(kv.second);
    o += "},\"quests\":{";
    k = 0;
    for (auto& kv : s.quests) o += std::string(k++ ? "," : "") + q(kv.first) + ":{\"tier\":" + num(kv.second.tier) + ",\"turnedIn\":" + num((long long)kv.second.turnedIn.size()) + "}";
    o += "},\"dialog\":{\"npc\":" + q(s.dlgNpc) + ",\"node\":" + q(s.dlgNode) + ",\"shop\":" + bl(s.dlgShop) + "},\"log\":[";
    const size_t from = s.log.size() > 50 ? s.log.size() - 50 : 0;
    for (size_t i = from; i < s.log.size(); i++) o += std::string(i > from ? "," : "") + q(s.log[i]);
    return o + "]}";
}

}  // namespace kmp
