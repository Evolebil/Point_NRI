// ============================================================================
// Кампания: сервер правил исследования и социалки (ТЗ docs/TZ_issledovanie_socialka.md). Как arena_play — JSON-строки со
// стандартного ввода, ответы JSON-строками. Журнал хранит web/kampaniya.py; здесь — свёртка журнала и черновики событий.
//
//   {"cmd":"content","data":{"thr":{..},"table":{..},"ambush":{..}}}   -> {"type":"ok"}             таблицы из Obsidian
//   {"cmd":"region","data":{w,h,lon0,lat0,dlon,dlat,terrain,road,river}} -> {"type":"ok"}             карта региона (web/data)
//   {"cmd":"load","seed":42,"journal":[событие, …]}                     -> {"type":"state","state":{..}} свёртка с нуля
//   {"cmd":"propose","act":{..},"who":"мастер","attempt":0}             -> {"type":"draft","ev":{..}} черновик, состояние не меняется
//   {"cmd":"apply","ev":{..}}                                           -> {"type":"state","state":{..}} принятое событие
//   {"cmd":"state"}                                                     -> {"type":"state","state":{..}}
//   {"cmd":"quit"}
// Ошибка — {"type":"error","err":"…"}. Сборка: make kampaniya (программа — build/kampaniya)
// ============================================================================
#include "engine.h"
#include <cstdio>
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
            const kmp::Region keep = ct.region;
            ct = kmp::Content();
            ct.region = keep;
            for (auto& kv : d["thr"].o) ct.thr[kv.first] = kv.second.num(20);
            for (auto& kv : d["table"].o)
                for (auto& x : kv.second.a) ct.table[kv.first].push_back(x.str());
            for (auto& kv : d["ambush"].o) ct.ambush[kv.first] = kv.second.str();
            if (d.has("roadThr")) ct.roadThr = d["roadThr"].num(2);
            if (d.has("returnThr")) ct.returnThr = d["returnThr"].num(-4);
            if (d.has("ambushMax")) ct.ambushMax = d["ambushMax"].num(3);
            emitLine("{\"type\":\"ok\"}");
        } else if (c == "region") {   // карта региона (Б1): один раз, отдельно от таблиц — она большая
            const Json& d = cmd["data"];
            kmp::Region& R = ct.region;
            R = kmp::Region();
            R.w = d["w"].num(0), R.h = d["h"].num(0), R.lon0 = d["lon0"].dbl(0), R.lat0 = d["lat0"].dbl(0), R.dlon = d["dlon"].dbl(1), R.dlat = d["dlat"].dbl(1);
            R.terr = d["terrain"].str(), R.road = d["road"].str(), R.river = d["river"].str();
            if (!R.ok()) { R = kmp::Region(); emitLine("{\"type\":\"error\",\"err\":\"карта региона не разобрана\"}"); }
            else emitLine("{\"type\":\"ok\"}");
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
