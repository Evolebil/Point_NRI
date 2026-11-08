"""Прокладывает все пути и сохраняет их в geodata/routes.json (для PNG и интерактивной карты)."""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from routing import road_graph, river_graph, Sea, hav
from routes_data import P, ROUTES
GAP = 40  # км: если до ближайшей дороги дальше — точной трассы нет, идём напрямую
roads, rivers, sea = road_graph(), river_graph(), Sea()
out = []
for name, kind, pts, grp in ROUTES:
    for a, b in zip(pts, pts[1:]):
        pa, pb = P[a], P[b]; exact = True
        if kind == "sea":
            line = sea.path(pa, pb); km = sum(hav(p, q) for p, q in zip(line, line[1:]))
        else:
            g = roads if kind == "land" else rivers
            (sa, da), (sb, db) = g.nearest(pa), g.nearest(pb)
            line, km = (g.path(pa, pb) if max(da, db) < GAP else (None, None))
            if line is None or km > 2.2 * hav(pa, pb):
                line, km, exact = [pa, pb], hav(pa, pb), False
        out.append(dict(route=name, kind=kind, group=grp, a=a, b=b, km=round(km), exact=exact, line=[[round(x, 4), round(y, 4)] for x, y in line]))
        print(f"{name or '·'}: {a} — {b}: {round(km)} км{'' if exact else ' (напрямую, трассы нет)'}")
json.dump(dict(points=P, legs=out), open(HERE + "/geodata/routes.json", "w"), ensure_ascii=False)
