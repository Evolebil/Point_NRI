#!/usr/bin/env python3
"""Проверка протокола arena_play: цена персонажа и автоигра простой стратегией (идти к врагу и бить)."""
import json, subprocess, sys

CHAR = {"name": "Рыцарь", "budget": 800, "stats": [16, 10, 14, 8, 10, 8], "acV": 8,
        "weapon": {"n": 1, "s": 8, "range": 5, "att": 2, "reach": 5, "type": 11},
        "abilities": [{"delivery": 1, "saveStat": 1, "range": 5, "radius": 10, "dmgN": 2, "dmgS": 6, "dtype": 1, "cond": 1, "recharge": 5}],
        "passives": {"saveProf": [2]}}

def run(char=CHAR, seed=7, quiet=False):
    p = subprocess.Popen(["./arena_play"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    def send(o): p.stdin.write(json.dumps(o, ensure_ascii=False) + "\n"); p.stdin.flush()
    def recv(): return json.loads(p.stdout.readline())
    send({"cmd": "price", "char": char}); pr = recv()["data"]
    if not quiet: print("цена:", pr.get("ok"), pr.get("err"), "потрачено", pr.get("spent"), "осталось", pr.get("left"), "хп", pr.get("hp"))
    if not pr.get("ok"): return None
    send({"cmd": "start", "char": char, "seed": seed, "opp": {"mode": "generator"}})
    turns = 0; frames = 0; res = None; init = None
    while True:
        m = recv()
        if "init" in m: init = m["init"]
        frames += len(m.get("frames", []))
        if m["type"] == "end": res = m; break
        req = m["req"]; me = req["me"]
        last = m["frames"][-1]["f"] if m["frames"] else None
        if last is None: send({"c": "end"}); continue
        # состояние: берём из последнего кадра
        st = req.get("_st")
        pos = (last[me]["x"], last[me]["y"])
        tgt = None; best = 1e9
        for e in req["enemies"]:
            if e["dist"] < best: best = e["dist"]; tgt = e
        # стратегия: по шагам, состояние хода хранится в req
        if tgt is None: send({"c": "end"}); continue
        if req["actionFree"] and tgt["los"] and req["weapon"]["armed"] and tgt["dist"] <= req["weapon"]["range"]:
            send({"c": "attack", "tgt": tgt["i"]}); continue
        if req["speedLeft"] >= 5 and tgt["dist"] > req["weapon"]["range"]:
            ex = last[tgt["i"]]; send({"c": "move", "x": ex["x"], "y": ex["y"]}); 
            # обычный moveToward не заходит в клетку врага: вернётся «не хватило»
            continue
        if req["actionFree"] and tgt["dist"] > req["weapon"]["range"] and req["speedLeft"] < 5:
            send({"c": "dash"}); continue
        send({"c": "end"})
    p.stdin.write(json.dumps({"cmd": "quit"}) + "\n"); p.stdin.flush(); p.wait(timeout=5)
    if not quiet: print("ходов игрока:", turns, "кадров:", frames, "итог:", res["result"], "причина:", res["reason"], "раундов:", res["rounds"], "я:", res["me"])
    return res

if __name__ == "__main__":
    run(seed=int(sys.argv[1]) if len(sys.argv) > 1 else 7)
