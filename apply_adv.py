#!/usr/bin/env python3
"""Состязательная калибровка: поднимает цены атомов, которые сильный Сборщик берёт слишком часто.
Использование: ./builder adv <6 файлов> > adv.txt; python3 apply_adv.py boss_arena_v20.cc adv.txt
Множитель применяется только к цене (init) в AD[]. Нижнюю границу lo скрипт не поднимает (v21, №15): обычная калибровка
может вернуть цену ниже, если обученный боец покажет, что атом не так силён. Чтобы сохранить lo <= init <= hi (№14),
потолок hi при необходимости поднимается до новой цены."""
import re, sys
if len(sys.argv) < 3: sys.exit(__doc__)
eng, advf = sys.argv[1:3]
s = open(eng, encoding='utf-8').read()
i0 = s.index("const AtomDef AD[AT_N] = {"); i1 = s.index("typedef std::array<double, AT_N> Coef;")
rows = list(re.finditer(r'(\{"[^"]*",\s*)([\d.]+)(,\s*)([\d.]+)(,\s*)([\d.]+)(,\s*)(\d)(,\s*)(\d)\}', s[i0:i1]))
edits = []
for m in re.finditer(r"^ADV (\d+) ([\d.]+)", open(advf, encoding='utf-8').read(), re.M):
    idx, k = int(m.group(1)), float(m.group(2)); r = rows[idx]
    if r.group(8) == '5' or 'множитель' in r.group(1): continue   # показатели и множители (скидки/надбавки) не трогаем
    if r.group(8) == '4':   # возврат очков (уязвимость): атом берут слишком часто, потому что возврат завышен — возврат уменьшается
        init = float(r.group(2)) / k; hi = min(float(r.group(6)), init); lo = min(float(r.group(4)), init)
        edits.append((i0 + r.start(2), i0 + r.end(2), "%.4f" % init)); edits.append((i0 + r.start(6), i0 + r.end(6), "%.4g" % hi))
        if lo != float(r.group(4)): edits.append((i0 + r.start(4), i0 + r.end(4), "%.4g" % lo))
        print("атом %d (возврат): ×1/%.3f -> %.4f, потолок %.4g" % (idx, k, init, hi)); continue
    init = float(r.group(2)) * k; hi = max(float(r.group(6)), init)
    edits.append((i0 + r.start(2), i0 + r.end(2), "%.4f" % init))
    if hi != float(r.group(6)): edits.append((i0 + r.start(6), i0 + r.end(6), "%.4g" % hi))
    print("атом %d: цена ×%.3f -> %.4f (нижняя граница %s без изменений)" % (idx, k, init, r.group(4)))
for a, b, t in sorted(edits, reverse=True): s = s[:a] + t + s[b:]
open(eng, 'w', encoding='utf-8').write(s)
