#!/usr/bin/env python3
"""Запекает цены из двух калибровок в таблицу AD[] движка.
Использование: python3 bake.py boss_arena_v20.cc calibA.txt calibB.txt [выход.cc]
Без 4-го аргумента перезаписывает движок. Проверяет, что число атомов в таблице равно числу цен в обоих файлах."""
import re, sys
if len(sys.argv) < 4: sys.exit(__doc__)
eng, fa, fb = sys.argv[1:4]; out = sys.argv[4] if len(sys.argv) > 4 else eng
def prices(f):
    t = open(f, encoding='utf-8', errors='replace').read()
    blk = t.split("для вставки в AD[].init (по порядку) ---")[1].split("=== Примеры")[0]
    p = [float(m.group(1)) for m in re.finditer(r"^([\d.]+), // ", blk, re.M)]
    g = re.search(r"g_share init: \{([^}]*)\}; g_nmShare = ([\d.]+)", t)
    return p, [float(x) for x in g.group(1).split(",")], float(g.group(2))
pa, ga, na = prices(fa); pb, gb, nb = prices(fb)
s = open(eng, encoding='utf-8').read()
i0 = s.index("const AtomDef AD[AT_N] = {"); i1 = s.index("typedef std::array<double, AT_N> Coef;")
tab = s[i0:i1]; rows = list(re.finditer(r'(\{"[^"]*", )([\d.]+)(, )', tab))
if not (len(rows) == len(pa) == len(pb)): sys.exit(f"ОШИБКА: атомов в таблице {len(rows)}, цен в A {len(pa)}, в B {len(pb)}. Калибровку запускали другим движком?")
o = []; last = 0
for r, a, b in zip(rows, pa, pb):
    o.append(tab[last:r.start(2)]); o.append("%.4f" % ((a + b) / 2)); last = r.end(2)
o.append(tab[last:]); s = s[:i0] + "".join(o) + s[i1:]
g = ", ".join("%.3f" % ((x + y) / 2) for x, y in zip(ga, gb))
s, n1 = re.subn(r"double g_share\[DT_N\] = \{[^}]*\};", "double g_share[DT_N] = {" + g + "};", s); assert n1 == 1
s, n2 = re.subn(r"double g_nmShare = [\d.]+;", "double g_nmShare = %.3f;" % ((na + nb) / 2), s); assert n2 == 1
open(out, 'w', encoding='utf-8').write(s); print("запечено", len(rows), "атомов ->", out)
