// ============================================================================
// Игра за персонажа: сервер команд поверх движка. Читает JSON-строки со стандартного ввода, отвечает JSON-строками (протокол ниже).
// К нему подключается play_server.py (веб-страница создания персонажа и боя). Правила, цены и ход боя — те же, что у ботов.
//
//   {"cmd":"catalog","budget":800}                       -> {"type":"catalog","data":{...}}       перечисления и прайс при бюджете
//   {"cmd":"price","char":{...}}                         -> {"type":"price","data":{...}}         разбор цены персонажа по покупкам
//   {"cmd":"preview","budget":800,"seed":7,"opp":{...}}  -> {"type":"preview","boss":{...}}        соперник с разбором цены
//   {"cmd":"start","char":{...},"seed":7,"opp":{"mode":"generator"|"net","mul":1.0},"side":-1}
//        -> поток: {"type":"turn","init":{...}(только в первом),"frames":[...],"req":{...}} — ждёт команду хода (см. human_turn.h);
//           в конце {"type":"end","frames":[...],"result":0|1|2,"reason":..,"rounds":..,"me":idx,"state":[по бойцам: hp, maxHp, fled, exh, slots, uses]}
//   {"cmd":"quit"}
// Сборка: make (программа — build/arena_play)
// ============================================================================
#include "engine.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include "gru.h"
#include "par.h"
#include "builder_net.h"
#include "char_spec.h"

static void emitLine(const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// Соперник: генератор или нейросеть-Сборщик, тот же бюджет с множителем сложности
static Boss makeOpponent(const Json& opp, long long budget, unsigned seed, const Coef& C, std::string& label, long long& ob) {
    const double mul = opp["mul"].dbl(1.0);
    ob = std::clamp<long long>((long long)std::llround(budget * std::max(0.1, std::min(4.0, mul))), MIN_BUDGET, MAX_BUDGET);
    const std::string mode = opp["mode"].str("generator");
    rng.seed(mix(seed, 31));
    if (mode == "net") {
        bld::BNet net;
        const std::string path = dndPath(opp["builder"].str("models/nets/builder_B.bin"));
        if (bld::loadNet(path, net)) {
            bld::BrainNet b(&net, nullptr);
            g_bb = &b;
            Boss o = makeBoss(ob, C, true, false);
            g_bb = nullptr;
            label = "нейросеть-Сборщик";
            return o;
        }
        std::fprintf(stderr, "ВНИМАНИЕ: веса Сборщика %s не прочитаны — соперника собирает генератор\n", path.c_str());
    }
    label = "Случайный генератор";
    return makeBoss(ob, C, true, false);
}

// состояние главных бойцов после боя (Б-3) — по порядку участников боя
static std::string endStatesJson() {
    std::string s = "[";
    for (size_t i = 0; i < g_endState.size(); i++) s += (i ? "," : "") + endStateJson(g_endState[i]);
    return s + "]";
}

int main() {
    anyFighterNet();   // сеть-боец для соперников с ИНТ >= 12 (если файл весов найден)
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    std::string line;
    while (std::getline(std::cin, line)) {
        bool ok = true;
        const Json cmd = parseJson(line, &ok);
        if (!ok) { emitLine("{\"type\":\"error\",\"err\":\"не JSON\"}"); continue; }
        const std::string c = cmd["cmd"].str();
        if (c == "quit") break;
        if (c == "catalog") {
            emitLine("{\"type\":\"catalog\",\"data\":" + chr::catalogReply((long long)cmd["budget"].dbl(800), C) + "}");
        } else if (c == "price") {
            emitLine("{\"type\":\"price\",\"data\":" + chr::priceReply(cmd["char"], C) + "}");
        } else if (c == "worldmap") {   // мир целиком, грубо: по клетке на каждые step клеток мира (выбор места боя)
            const unsigned long long ws = std::strtoull(cmd["seed"].str("1").c_str(), nullptr, 10) | 1;
            const int size = std::clamp(cmd["size"].num(1000), 1000, 10000), step = std::max(4, size / 250), n = size / step;
            static wg::Plan wp;
            if (wp.seed != ws || wp.size != size) wg::buildPlan(wp, ws, size);
            wg::lvlCache().p = nullptr;
            std::string bio = "[", hgt = "[";
            for (int j = 0; j < n; j++) {
                std::string rb, rh;
                for (int i = 0; i < n; i++) {
                    const wg::Cell cl = wg::worldCell(wp, i * step + step / 2, j * step + step / 2);
                    rb += cl.town ? (cl.wall ? '#' : '+') : (cl.water ? '~' : (cl.pit ? 'V' : (cl.road ? '=' : (char)('a' + cl.biome))));
                    rh += (char)('0' + cl.height);
                }
                bio += (j ? ",\"" : "\"") + rb + "\"";
                hgt += (j ? ",\"" : "\"") + rh + "\"";
            }
            std::string towns = "[";
            for (size_t k = 0; k < wp.towns.size(); k++)
                towns += std::string(k ? "," : "") + "[" + std::to_string(wp.towns[k].x) + "," + std::to_string(wp.towns[k].y) + "," + std::to_string(wp.towns[k].r) + "," + std::to_string(wp.towns[k].kind) + "]";
            emitLine("{\"type\":\"worldmap\",\"seed\":\"" + std::to_string(ws) + "\",\"size\":" + std::to_string(size) + ",\"step\":" + std::to_string(step) + ",\"n\":" + std::to_string(n) +
                     ",\"biome\":" + bio + "],\"height\":" + hgt + "],\"towns\":" + towns + "]}");
        } else if (c == "genmap") {   // случайная карта в формате редактора (заготовка для правки): участок случайного мира
            rng.seed(mix((unsigned)cmd["seed"].dbl(1), 77));
            g_style.worldSeed = rng.next() | 1;
            g_style.fixedW = cmd["w"].num(0);
            g_style.fixedH = cmd["h"].num(0);
            g_style.theme = cmd.has("theme") ? cmd["theme"].num(0) : -1;
            static World gw;
            genMap(gw);
            g_style = MapStyle();
            emitLine("{\"type\":\"map\",\"map\":" + mapToJson(gw) + "}");
        } else if (c == "regionmap") {   // участок по заданной местности (кампания, задача Б-1): карта в формате редактора
            const auto& q = cmd["cell"];
            wg::RegionDesc d;
            d.terrain = q["terrain"].str("");
            d.biome = q.has("biome") ? q["biome"].num(-1) : -1;
            d.level = q["level"].num(2);
            for (int k = 0; k < 4; k++) d.nb[k] = d.level;
            if (q.has("nb"))
                for (int k = 0; k < 4 && k < (int)q["nb"].size(); k++) d.nb[k] = q["nb"][k].num(d.level);
            d.road = q["road"].str("");
            d.river = q["river"].flag(false);
            if (q.has("settle")) {
                d.settle = q["settle"]["kind"].str("");
                d.pop = q["settle"]["pop"].num(0);
            }
            d.seed = q["seed"].str("1");
            static World rw;
            regionArena(rw, d, cmd["w"].num(200), cmd["h"].num(200));
            emitLine("{\"type\":\"map\",\"map\":{\"parts\":1," + mapToJson(rw).substr(1) + "}");
        } else if (c == "checkmap") {
            CustomMap m;
            std::string err;
            const bool ok = parseCustomMap(cmd["map"], m, err);
            emitLine(std::string("{\"type\":\"mapcheck\",\"ok\":") + (ok ? "true" : "false") + ",\"err\":" + jq(err) + "}");
        } else if (c == "preview") {
            std::string label;
            const long long budget = (long long)cmd["budget"].dbl(800);
            const unsigned seed = (unsigned)cmd["seed"].dbl(1);
            long long ob = 0;
            Boss o = makeOpponent(cmd["opp"], budget, seed, C, label, ob);
            emitLine("{\"type\":\"preview\",\"label\":" + jq(label) + ",\"boss\":" + bossViewJson(o, C, (double)ob, bossTitle(o, seed + 7)) + "}");
        } else if (c == "start") {
            Boss me;
            long long budget = 0;
            std::string err;
            g_customMap = CustomMap();
            if (cmd.has("map") && cmd["map"].t == Json::OBJ && !parseCustomMap(cmd["map"], g_customMap, err)) { emitLine("{\"type\":\"error\",\"err\":" + jq("Карта: " + err) + "}"); continue; }
            if (!chr::buildCharacter(cmd["char"], C, me, budget, err)) { emitLine("{\"type\":\"error\",\"err\":" + jq(err) + "}"); continue; }
            const unsigned seed = (unsigned)cmd["seed"].dbl(1);
            std::string label;
            long long oppBudget = 0;
            Boss opp = makeOpponent(cmd["opp"], budget, seed, C, label, oppBudget);
            rng.seed(mix(seed, 99));
            // мир: выбранный игроком (зерно, размер, участок) или по зерну боя (мир 1000, случайный участок)
            const Json& wj = cmd["world"];
            g_style = MapStyle();
            if (wj.t == Json::OBJ) {
                g_style.worldSeed = std::strtoull(wj["seed"].str("1").c_str(), nullptr, 10) | 1;
                g_style.worldSize = std::clamp(wj["size"].num(1000), 1000, 10000);
                g_style.wx = wj["x"].num(-1);
                g_style.wy = wj["y"].num(-1);
                g_style.worldStep = std::clamp(wj["step"].num(1), 1, 200);
                g_style.fixedW = std::clamp(wj["w"].num(0), 0, 100);
                g_style.fixedH = std::clamp(wj["h"].num(0), 0, 100);
            } else
                g_style.worldSeed = mix(seed, 555) | 1;
            int side = cmd["side"].num(-1);
            if (side < 0 || side > 1) side = irand(2);
            const Boss& A = side == 0 ? me : opp;
            const Boss& B = side == 0 ? opp : me;
            const std::string myName = cmd["char"]["name"].str("Безымянный");
            // мозг соперника: нейросеть-боец (если веса загружены) или правило-бот по его ИНТ
            const bool netBrain = cmd["opp"]["brain"].str("net") == "net" && anyFighterNet();
            g_forceNetMask = netBrain ? (1 << (1 - side)) : 0;
            const std::string enemyBrain = netBrain ? "нейросеть-боец (GRU)" : "правило-бот";
            std::vector<std::string> frames;
            std::string mapJson;
            size_t sent = 0;
            bool sentInit = false;
            auto initJson = [&]() {
                return "{\"seed\":" + std::to_string(seed) + ",\"budget\":" + std::to_string(budget) + ",\"me\":" + std::to_string(side) + ",\"enemyBrain\":" + jq(enemyBrain) + ",\"bosses\":[" +
                       bossViewJson(A, C, (double)(side == 0 ? budget : oppBudget), side == 0 ? myName : bossTitle(A, seed + 7)) + "," +
                       bossViewJson(B, C, (double)(side == 1 ? budget : oppBudget), side == 1 ? myName : bossTitle(B, seed + 7)) +
                       "],\"builders\":[" + jq(side == 0 ? "Игрок" : label) + "," + jq(side == 1 ? "Игрок" : label) + "],\"map\":" + mapJson + "}";
            };
            auto flushFrames = [&]() {
                std::string o = "[";
                for (size_t i = sent; i < frames.size(); i++) o += (i > sent ? "," : "") + frames[i];
                sent = frames.size();
                return o + "]";
            };
            g_human.idx = side;
            g_human.ask = [&](const std::string& req) {
                std::string out = "{\"type\":\"turn\",";
                if (!sentInit) { out += "\"init\":" + initJson() + ","; sentInit = true; }
                out += "\"frames\":" + flushFrames() + ",\"req\":" + req + "}";
                emitLine(out);
                std::string reply;
                if (!std::getline(std::cin, reply)) return std::string("{\"c\":\"end\"}");
                return reply;
            };
            const int res = fight(A, B, &frames, &mapJson);
            g_human.idx = -1;
            g_human.ask = nullptr;
            g_forceNetMask = 0;
            g_customMap = CustomMap();
            g_style = MapStyle();
            std::string out = "{\"type\":\"end\",";
            if (!sentInit) out += "\"init\":" + initJson() + ",";
            out += "\"frames\":" + flushFrames() + ",\"result\":" + std::to_string(res) + ",\"reason\":" + std::to_string(g_fightInfo.reason) + ",\"rounds\":" + std::to_string(g_fightInfo.rounds) +
                   ",\"me\":" + std::to_string(side) + ",\"state\":" + endStatesJson() + "}";
            emitLine(out);
        } else if (c == "teams") {   // бой нескольких команд (v21): состав — люди (свой персонаж, место seat) и боты (сборка генератором)
            const Json& M = cmd["members"];
            const unsigned seed = (unsigned)cmd["seed"].dbl(1);
            std::vector<Boss> bs;
            std::vector<int> team, seat;
            std::vector<std::string> names;
            std::vector<std::string> brains;
            std::vector<long long> budgets;
            std::string err;
            g_customMap = CustomMap();
            if (cmd.has("map") && cmd["map"].t == Json::OBJ && !parseCustomMap(cmd["map"], g_customMap, err)) { emitLine("{\"type\":\"error\",\"err\":" + jq("Карта: " + err) + "}"); continue; }
            bool bad = false;
            for (size_t i = 0; i < M.size() && !bad; i++) {
                Boss b;
                long long bud = 0;
                if (M[i]["char"].t == Json::OBJ) {
                    if (!chr::buildCharacter(M[i]["char"], C, b, bud, err)) { err = M[i]["name"].str("игрок") + ": " + err; bad = true; break; }
                } else {
                    bud = std::clamp<long long>((long long)M[i]["budget"].dbl(500), MIN_BUDGET, MAX_BUDGET);
                    rng.seed(mix(seed, 1000 + (unsigned)i));
                    b = makeBoss(bud, C, true, false);
                }
                bs.push_back(b);
                budgets.push_back(bud);
                team.push_back(std::clamp(M[i]["team"].num(0), 0, MAXT - 1));
                seat.push_back(M[i]["char"].t == Json::OBJ ? M[i]["seat"].num(-1) : -1);
                names.push_back(M[i]["name"].str(M[i]["char"].t == Json::OBJ ? M[i]["char"]["name"].str("Игрок") : bossTitle(b, seed + (unsigned)i)));
                brains.push_back(M[i]["brain"].str("rules"));
            }
            if (bad || bs.size() < 2) { emitLine("{\"type\":\"error\",\"err\":" + jq(bad ? err : "нужно хотя бы два бойца") + "}"); continue; }
            {   // нужно хотя бы две команды
                bool two = false;
                for (int t : team) two = two || t != team[0];
                if (!two) { emitLine("{\"type\":\"error\",\"err\":\"все бойцы в одной команде — нужна хотя бы вторая\"}"); continue; }
            }
            rng.seed(mix(seed, 99));
            const Json& wj = cmd["world"];
            g_style = MapStyle();
            if (wj.t == Json::OBJ) {
                g_style.worldSeed = std::strtoull(wj["seed"].str("1").c_str(), nullptr, 10) | 1;
                g_style.worldSize = std::clamp(wj["size"].num(1000), 1000, 10000);
                g_style.wx = wj["x"].num(-1);
                g_style.wy = wj["y"].num(-1);
                g_style.worldStep = std::clamp(wj["step"].num(1), 1, 200);
                g_style.fixedW = std::clamp(wj["w"].num(0), 0, 100);
                g_style.fixedH = std::clamp(wj["h"].num(0), 0, 100);
            } else
                g_style.worldSeed = mix(seed, 555) | 1;
            const int rounds = cmd["rounds"].num(0);
            const int maxR0 = g_maxRounds;
            g_maxRounds = rounds > 0 ? std::min(rounds, 500) : 500;
            std::vector<RosterEntry> R;
            for (size_t i = 0; i < bs.size(); i++) R.push_back({&bs[i], team[i]});
            std::vector<std::string> frames;
            std::string mapJson;
            size_t sent = 0;
            bool sentInit = false;
            auto initJson = [&]() {
                std::string o = "{\"seed\":" + std::to_string(seed) + ",\"teamsMode\":1,\"rounds\":" + std::to_string(g_maxRounds) + ",\"bosses\":[";
                for (size_t i = 0; i < bs.size(); i++) o += std::string(i ? "," : "") + bossViewJson(bs[i], C, (double)budgets[i], names[i]);
                o += "],\"team\":[";
                for (size_t i = 0; i < bs.size(); i++) o += std::string(i ? "," : "") + std::to_string(team[i]);
                o += "],\"seat\":[";
                for (size_t i = 0; i < bs.size(); i++) o += std::string(i ? "," : "") + std::to_string(seat[i]);
                return o + "],\"map\":" + mapJson + "}";
            };
            auto flushFrames = [&]() {
                std::string o = "[";
                for (size_t i = sent; i < frames.size(); i++) o += (i > sent ? "," : "") + frames[i];
                sent = frames.size();
                return o + "]";
            };
            g_human.seats = seat;
            g_human.multi = true;
            g_human.idx = -1;
            for (size_t i = 0; i < seat.size(); i++)
                if (seat[i] >= 0 && g_human.idx < 0) g_human.idx = (int)i;
            g_forceNetMask = 0;
            if (anyFighterNet())
                for (size_t i = 0; i < brains.size() && i < 31; i++)
                    if (seat[i] < 0 && brains[i] == "net") g_forceNetMask |= (1 << i);
            World* wp = nullptr;
            (void)wp;
            g_human.ask = [&](const std::string& req) {
                bool okq = true;
                const Json rq = parseJson(req, &okq);
                const int me = rq["me"].num(-1);
                const int st = g_curW && me >= 0 && me < (int)g_curW->f.size() ? humanSeat(*g_curW, me) : -1;
                std::string out = "{\"type\":\"turn\",\"seat\":" + std::to_string(st) + ",";
                if (!sentInit) { out += "\"init\":" + initJson() + ","; sentInit = true; }
                out += "\"frames\":" + flushFrames() + ",\"req\":" + req + "}";
                emitLine(out);
                std::string reply;
                if (!std::getline(std::cin, reply)) return std::string("{\"c\":\"end\"}");
                return reply;
            };
            const int res = fightRoster(R, &frames, &mapJson, nullptr, nullptr);
            g_human.ask = nullptr;
            g_human.seats.clear();
            g_human.multi = false;
            g_human.idx = -1;
            g_forceNetMask = 0;
            g_maxRounds = maxR0;
            g_customMap = CustomMap();
            g_style = MapStyle();
            int nT = 0;
            for (int t : team) nT = std::max(nT, t + 1);
            std::string sc = "[";
            for (int t = 0; t < nT; t++) sc += std::string(t ? "," : "") + std::to_string(g_teamScore[t]);
            std::string out = "{\"type\":\"end\",\"teamsMode\":1,";
            if (!sentInit) out += "\"init\":" + initJson() + ",";
            out += "\"frames\":" + flushFrames() + ",\"result\":" + std::to_string(res) + ",\"reason\":" + std::to_string(g_fightInfo.reason) + ",\"rounds\":" + std::to_string(g_fightInfo.rounds) +
                   ",\"scores\":" + sc + "],\"state\":" + endStatesJson() + "}";
            emitLine(out);
        } else
            emitLine("{\"type\":\"error\",\"err\":\"неизвестная команда\"}");
    }
    return 0;
}
