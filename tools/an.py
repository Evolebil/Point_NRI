#!/usr/bin/env python3
"""Короткая сводка двух калибровок (то, что нужно прислать Claude).
Использование: python3 tools/an.py results/calib/calibA.txt results/calib/calibB.txt [слова-фильтры для атомов]
Печатает: длину боя по бюджетам, атомы вне коридора 46–54%, расхождения зёрен, наклон призывателей и выбранные атомы."""
import re, sys
def load(f):
    t = open(f, encoding='utf-8', errors='replace').read(); d = {}; order = []
    m = t.split("=== Итог")[1].split("Длина боя")[0]
    for l in m.splitlines()[1:]:
        r = re.match(r"\s+(.+?)\s+([\d.]+) ->\s+([\d.]+)\s+([\d.]+)% \[\s*([\d.]+)\.\.\s*([\d.]+)\]\s+n=(\d+)", l)
        if r: order.append(r.group(1)); d[r.group(1)] = (float(r.group(2)), float(r.group(3)), float(r.group(4)), float(r.group(5)), float(r.group(6)), int(r.group(7)))
    return d, order, t
if len(sys.argv) < 3: sys.exit(__doc__)
A, o, ta = load(sys.argv[1]); B, _, tb = load(sys.argv[2]); keys = sys.argv[3:]
print(f"атомов: {len(o)}")
for nm, t in (("A", ta), ("B", tb)):
    print(f"длина боя {nm}:", re.search(r"Длина боя по бюджетам:(.*)", t).group(1).strip())
    m = re.search(r"Винрейт призывателя минус остальные.*", t)
    if m: print(f"призыватели {nm}:", m.group(0)[:160])
print("\nвне коридора 46–54% (n>200) в любом зерне:")
bad = [n for n in o if A[n][5] > 200 and (abs(A[n][2] - 50) > 4 or abs(B[n][2] - 50) > 4)]
for n in bad: print(f"  {n[:60]:60} A {A[n][2]:.1f}%  B {B[n][2]:.1f}%  цена {A[n][1]:.3g}/{B[n][1]:.3g}")
if not bad: print("  нет")
print("\nрасхождение цен между зёрнами >25% (цена >2):")
dis = [n for n in o if max(A[n][1], B[n][1]) > 2 and abs(A[n][1] - B[n][1]) / max(A[n][1], B[n][1]) > 0.25]
for n in dis: print(f"  {n[:60]:60} A {A[n][1]:.3g}  B {B[n][1]:.3g}")
if not dis: print("  нет")
print("\nна нижней границе цены (коэф ≤ 1.1, n>200):", [n[:38] for n in o if A[n][1] <= 1.1 and B[n][1] <= 1.1 and A[n][5] > 200][:20])
if keys:
    print("\nвыбранные атомы:")
    for n in o:
        if any(k in n for k in keys): print(f"  {n[:55]:55} A {A[n][1]:8.3f} {A[n][2]:5.1f}% [{A[n][3]:.0f}..{A[n][4]:.0f}] | B {B[n][1]:8.3f} {B[n][2]:5.1f}%")
