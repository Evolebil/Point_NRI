#!/usr/bin/env python3
"""Запекает цены из двух калибровок в движок: таблица AD[] (src/engine/atoms.h) и доли типов урона g_share (src/engine/core.h).
Использование: python3 tools/bake.py results/calib/calibA.txt results/calib/calibB.txt [папка движка=src/engine]
Проверяет, что число атомов в таблице равно числу цен в обоих файлах."""
import os, re, sys
if len(sys.argv) < 3: sys.exit(__doc__)
fa, fb = sys.argv[1:3]
eng_dir = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "engine")
eng = os.path.join(eng_dir, "atoms.h"); core = os.path.join(eng_dir, "core.h")
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
c = open(core, encoding='utf-8').read()
g = ", ".join("%.3f" % ((x + y) / 2) for x, y in zip(ga, gb))
c, n1 = re.subn(r"double g_share\[DT_N\] = \{[^}]*\};", "double g_share[DT_N] = {" + g + "};", c); assert n1 == 1
c, n2 = re.subn(r"double g_nmShare = [\d.]+;", "double g_nmShare = %.3f;" % ((na + nb) / 2), c); assert n2 == 1
open(eng, 'w', encoding='utf-8').write(s); open(core, 'w', encoding='utf-8').write(c)
print("запечено", len(rows), "атомов ->", eng, "и", core)
