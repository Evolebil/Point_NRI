// ============================================================================
// Кампания: сервер правил исследования и социалки (ТЗ docs/TZ_issledovanie_socialka.md). Как arena_play — JSON-строки со
// стандартного ввода, ответы JSON-строками. Журнал хранит web/kampaniya.py; здесь — свёртка журнала и черновики событий.
//
//   {"cmd":"content","data":{"thr":{..},"table":{..},"ambush":{..}}}   -> {"type":"ok"}             таблицы из Obsidian
//   {"cmd":"world","id":"…","kind":"gen","params":{size,seed,sea,mount,rough,climate,humid,towns,rivers,name},"use":true}
//   {"cmd":"world","id":"act1","kind":"act","data":<web/data/region_act1.json>,"use":true}     -> {"type":"world","info":{…}}
//   {"cmd":"tile","id":"…","z":3,"tx":0,"ty":0}  -> {"type":"tile","tile":{…}}   кусок 256 × 256 клеток уровня z
//   {"cmd":"worldinfo","id":"…"}                 -> {"type":"worldinfo","info":{…}}  размер, уровни, поселения
//   {"cmd":"load","seed":42,"journal":[событие, …]}                     -> {"type":"state","state":{..}} свёртка с нуля
//   {"cmd":"propose","act":{..},"who":"мастер","attempt":0}             -> {"type":"draft","ev":{..}} черновик, состояние не меняется
//   {"cmd":"apply","ev":{..}}                                           -> {"type":"state","state":{..}} принятое событие
//   {"cmd":"state"}                                                     -> {"type":"state","state":{..}}
//   {"cmd":"quit"}
// Ошибка — {"type":"error","err":"…"}. Сборка: make kampaniya (программа — build/kampaniya)
// ============================================================================
#include "engine.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include "char_spec.h"
#include "kmp_events.h"

static void emitLine(const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

int main() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    kmp::State st;
    kmp::Content ct;
    std::map<std::string, std::shared_ptr<kmp::World>> worlds;   // миры по id: мир кампании и предпросмотры генератора
    std::string line;
    while (std::getline(std::cin, line)) {
        bool ok = true;
        const Json cmd = parseJson(line, &ok);
        if (!ok) { emitLine("{\"type\":\"error\",\"err\":\"плохой JSON\"}"); continue; }
        const std::string c = cmd["cmd"].str();
        std::string err;
        if (c == "quit") break;
        if (c == "content") {
            const Json& d = cmd["data"];
            const auto keep = ct.world;
            ct = kmp::Content();
            ct.world = keep;
            for (auto& kv : d["thr"].o) ct.thr[kv.first] = kv.second.num(20);
            for (auto& kv : d["table"].o)
                for (auto& x : kv.second.a) ct.table[kv.first].push_back(x.str());
            for (auto& kv : d["ambush"].o) ct.ambush[kv.first] = kv.second.str();
            if (d.has("roadThr")) ct.roadThr = d["roadThr"].num(2);
            if (d.has("returnThr")) ct.returnThr = d["returnThr"].num(-4);
            if (d.has("ambushMax")) ct.ambushMax = d["ambushMax"].num(3);
            emitLine("{\"type\":\"ok\"}");
        } else if (c == "world") {   // мир: "kind":"gen" + "params" (генератор) или "act" + "data" (сетка Акта I); "use" — сделать миром кампании
            const std::string id = cmd["id"].str("w");
            std::shared_ptr<kmp::World> W;
            if (cmd["kind"].str() == "act") {
                auto A = std::make_shared<kmp::ActWorld>();
                const Json& d = cmd["data"];
                kmp::Region& R = A->R;
                R.w = d["w"].num(0), R.h = d["h"].num(0), R.lon0 = d["lon0"].dbl(0), R.lat0 = d["lat0"].dbl(0), R.dlon = d["dlon"].dbl(1), R.dlat = d["dlat"].dbl(1);
                R.terr = d["terrain"].str(), R.road = d["road"].str(), R.river = d["river"].str();
                if (!R.ok()) { emitLine("{\"type\":\"error\",\"err\":\"карта региона не разобрана\"}"); continue; }
                A->build({});
                for (auto& t : d["towns"].a) {
                    kmp::Town tw;
                    tw.name = t["name"].str(), tw.pop = t["pop"].num(0), tw.kind = tw.pop >= 20000 ? "крупный город" : tw.pop >= 5000 ? "город" : "городок";
                    A->toKm(t["lon"].dbl(0), t["lat"].dbl(0), tw.x, tw.y);
                    A->towns.push_back(tw);
                }
                A->name = d["name"].str("Акт I");
                W = A;
            } else {
                auto G = std::make_shared<kmp::GenWorld>();
                const Json& p = cmd["params"];
                G->p.sizeKm = p["size"].dbl(800), G->p.seed = (uint64_t)p["seed"].dbl(1), G->p.sea = p["sea"].dbl(0.3), G->p.mount = p["mount"].dbl(0.5);
                G->p.rough = p["rough"].dbl(0.5), G->p.climate = p["climate"].dbl(0), G->p.humid = p["humid"].dbl(0), G->p.towns = p["towns"].dbl(1), G->p.rivers = p["rivers"].dbl(1);
                G->name = p["name"].str("Мир");
                if (p["hmap"].t == Json::STR) {   // карта высот: файл серого hw × hh байт (путь даёт web/kampaniya.py)
                    G->p.hw = p["hw"].num(0), G->p.hh = p["hh"].num(0), G->p.kmpp = p["kmpp"].dbl(1), G->p.hmSea = p["hmSea"].dbl(-1);
                    std::ifstream f(p["hmap"].str(), std::ios::binary);
                    G->p.hmap.assign(std::istreambuf_iterator<char>(f), {});
                    if (G->p.hw < 2 || G->p.hh < 2 || (long long)G->p.hmap.size() != (long long)G->p.hw * G->p.hh) {
                        emitLine("{\"type\":\"error\",\"err\":\"карта высот не прочитана\"}");
                        continue;
                    }
                }
                G->build();
                W = G;
            }
            worlds[id] = W;
            if (cmd["use"].flag()) ct.world = W;
            emitLine("{\"type\":\"world\",\"id\":" + kmp::q(id) + ",\"info\":" + kmp::worldInfoJson(*W) + "}");
        } else if (c == "tile" || c == "worldinfo") {   // кусок обзора / сведения о мире (по id; без id — мир кампании)
            auto it = worlds.find(cmd["id"].str());
            std::shared_ptr<kmp::World> W = it != worlds.end() ? it->second : ct.world;
            if (!W) { emitLine("{\"type\":\"error\",\"err\":\"мира нет\"}"); continue; }
            if (c == "worldinfo") { emitLine("{\"type\":\"worldinfo\",\"info\":" + kmp::worldInfoJson(*W) + "}"); continue; }
            const int z = std::clamp(cmd["z"].num(0), 0, 12);
            emitLine("{\"type\":\"tile\",\"tile\":" + kmp::tileJson(*W, z, cmd["tx"].num(0), cmd["ty"].num(0)) + "}");
        } else if (c == "load") {
            st = kmp::State();
            st.seed = (uint64_t)cmd["seed"].dbl(1);
            for (auto& ev : cmd["journal"].a)
                if (!kmp::apply(st, C, ev, err)) { err = "событие " + std::to_string(ev["n"].num()) + ": " + err; break; }
            if (!err.empty()) emitLine("{\"type\":\"error\",\"err\":" + kmp::q(err) + "}");
            else emitLine("{\"type\":\"state\",\"state\":" + kmp::stateJson(st) + "}");
        } else if (c == "propose") {
            const std::string ev = kmp::propose(st, ct, cmd["act"], cmd["who"].str("мастер"), cmd["attempt"].num(0), err);
            if (ev.empty()) emitLine("{\"type\":\"error\",\"err\":" + kmp::q(err.empty() ? "пустое событие" : err) + "}");
            else emitLine("{\"type\":\"draft\",\"ev\":" + ev + "}");
        } else if (c == "apply") {
            kmp::State bak = st;   // ошибка применения не портит состояние
            if (!kmp::apply(st, C, cmd["ev"], err)) {
                st = bak;
                emitLine("{\"type\":\"error\",\"err\":" + kmp::q(err) + "}");
            } else emitLine("{\"type\":\"state\",\"state\":" + kmp::stateJson(st) + "}");
        } else if (c == "state") {
            emitLine("{\"type\":\"state\",\"state\":" + kmp::stateJson(st) + "}");
        } else emitLine("{\"type\":\"error\",\"err\":" + kmp::q("неизвестная команда: " + c) + "}");
    }
    return 0;
}
