#!/usr/bin/env python3
"""Переносит блок TD[] из tiers_fit.txt (результат ./builder tiers) в движок.
Использование: python3 apply_tiers.py boss_arena_v20.cc tiers_fit.txt"""
import re, sys
if len(sys.argv) < 3: sys.exit(__doc__)
eng, fit = sys.argv[1:3]
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
