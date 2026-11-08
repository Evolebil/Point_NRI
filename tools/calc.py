#!/usr/bin/env python3
"""Калькулятор персонажа: файл описания (JSON, как «Экспорт» в игре) -> разбор цены по покупкам.
Использование: python3 calc.py character.json"""
import json, os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
if os.path.basename(HERE) in ("web", "tools", "tests"):   # запуск не через симлинк в корне — корень проекта выше
    HERE = os.path.dirname(HERE)
ch = json.load(open(sys.argv[1], encoding="utf-8")) if len(sys.argv) > 1 else sys.exit(__doc__)
w = ch.get("weapon", {})
if w.get("armed") is False: ch["weapon"] = {"n": 0}
ch["abilities"] = [dict({k: v for k, v in a.items() if not k.startswith("_")}, self=a.get("_kind") == "self", delivery={"save": 1, "auto": 2}.get(a.get("_kind"), 0)) if "_kind" in a else a for a in ch.get("abilities", [])]
p = subprocess.run([os.path.join(HERE, "arena_play")], input=json.dumps({"cmd": "price", "char": ch}) + "\n", capture_output=True, text=True, encoding="utf-8", cwd=HERE, timeout=30)
d = json.loads(p.stdout.splitlines()[0])["data"]
b = d.get("boss")
if b:
    for it in b["items"]:
        print("%-46s %6d   %s" % (it["label"], it["cost"], it["detail"]))
    print("-" * 60)
print("Потрачено %s из %s, осталось %s%s" % (d.get("spent"), ch.get("budget"), d.get("left"), "" if d["ok"] else "   ОШИБКА: " + d["err"]))
