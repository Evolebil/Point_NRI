#!/usr/bin/env python3
"""Переносит блок TD[] и множители лестниц из результата ./build/tiers в движок (src/engine/atoms.h).
Использование: python3 tools/apply_tiers.py results/tiers/tiers_fit.txt [atoms.h=src/engine/atoms.h]"""
import os, re, sys
if len(sys.argv) < 2: sys.exit(__doc__)
fit = sys.argv[1]
eng = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "engine", "atoms.h")
s = open(eng, encoding='utf-8').read(); blk = open(fit, encoding='utf-8').read().strip()
tdblk = blk.split("// SCALE")[0].strip()
a = s.index("TierDef TD[TR_N] = {"); b = s.index("};", a) + 2
s = s[:a] + tdblk + s[b:]
# множители единицы цены: коэффициенты атомов лестниц в AD[] (по индексу строки)
i0 = s.index("const AtomDef AD[AT_N] = {"); i1 = s.index("typedef std::array<double, AT_N> Coef;")
rows = list(re.finditer(r'(\{"[^"]*", )([\d.]+)(, )', s[i0:i1]))
for m in re.finditer(r"// SCALE (\d+) ([\d.]+)", blk):
    idx, k = int(m.group(1)), float(m.group(2)); r = rows[idx]
    st, en = i0 + r.start(2), i0 + r.end(2)
    s = s[:st] + "%.4f" % (float(r.group(2)) * k) + s[en:]
    rows = list(re.finditer(r'(\{"[^"]*", )([\d.]+)(, )', s[i0:s.index("typedef std::array<double, AT_N> Coef;")]))
open(eng, 'w', encoding='utf-8').write(s); print("TD[] обновлён из", fit)
