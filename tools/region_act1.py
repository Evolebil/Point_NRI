#!/usr/bin/env python3
"""Карта региона Акта I для кампании (ТЗ Б1): сетка местности Париж — Каркассон в web/data/region_act1.json.

Разметка — та же, что на карте Акта I в Obsidian (kampaniya/karty/act1.py): горы и холмы — области Natural Earth,
леса и болота — эллипсы с подписями, римские дороги Itiner-e, реки и озёра Natural Earth, города около 1200 года.
Остальная суша — «Пашни и сёла». Запускается один раз, когда меняется разметка: python3 tools/region_act1.py
(единственное место, где кампания читает kampaniya/; в игре — только готовый JSON).
"""
import json
import math
import os
import sys

import numpy as np
from matplotlib.path import Path

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K = os.path.join(ROOT, "kampaniya", "karty")
sys.path.insert(0, K)
import act1  # noqa: E402  (разметка местности Акта I)
import states  # noqa: E402
from cities_data import build  # noqa: E402
from itinere import roads  # noqa: E402

# коды местности — как kmp::terrains() в src/kampaniya/kmp_core.h; 7 — вода (не пройти)
TERR = ["Пашни и сёла", "Лес", "Холмы и плато", "Горы", "Болото", "Река и переправа", "Город", "вода"]
FIELDS, FOREST, HILLS, MOUNT, SWAMP, RIVER, TOWN, WATER = range(8)
X0, X1, Y0, Y1 = -3.2, 7.0, 42.4, 49.6        # рамка карты Акта I
DLON, DLAT = 0.05, 0.035                       # клетка ~3,9 × 3,9 км на широте 46°


def polys(path):
    with open(path, encoding="utf-8") as f:
        feats = json.load(f)["features"]
    return [(f["properties"], list(states.rings(f["geometry"]))) for f in feats if f["geometry"]]


def inside(pts, rings):
    m = np.zeros(len(pts), bool)
    for r in rings:
        a = np.array(r)
        if a[:, 0].max() < X0 or a[:, 0].min() > X1 or a[:, 1].max() < Y0 or a[:, 1].min() > Y1:
            continue
        m |= Path(a).contains_points(pts)
    return m


def main(out):
    W, H = int(round((X1 - X0) / DLON)), int(round((Y1 - Y0) / DLAT))
    lon = X0 + (np.arange(W) + 0.5) * DLON
    lat = Y1 - (np.arange(H) + 0.5) * DLAT        # строка 0 — север
    LO, LA = np.meshgrid(lon, lat)
    pts = np.column_stack([LO.ravel(), LA.ravel()])
    G = states.G
    t = np.full(W * H, WATER, np.int8)
    t[inside(pts, [r for _, rs in polys(G + "/ne_50m_land.geojson") for r in rs])] = FIELDS
    phys = {p["NAME"]: rs for p, rs in polys(G + "/ne_10m_geography_regions_polys.geojson")}
    for code, key in ((HILLS, "hills"), (MOUNT, "mount")):
        for n in act1.PHYS[key]:
            t[inside(pts, phys[n]) & (t != WATER)] = code
    labels = []
    for code, key in ((FOREST, "forest"), (SWAMP, "swamp")):
        for x, y, w, h, lab in act1.ELL[key]:
            m = ((pts[:, 0] - x) / (w / 2)) ** 2 + ((pts[:, 1] - y) / (h / 2)) ** 2 <= 1
            t[m & (t != WATER)] = code
            labels.append({"text": lab, "lon": x, "lat": y, "terrain": TERR[code]})
    t[inside(pts, [r for _, rs in polys(G + "/ne_50m_lakes.geojson") for r in rs])] = WATER

    def cell(lo, la):
        i, j = int((lo - X0) / DLON), int((Y1 - la) / DLAT)
        return j * W + i if 0 <= i < W and 0 <= j < H else -1

    def raster(lines, flag):
        for pl in lines:
            for (a, b), (c, d) in zip(pl, pl[1:]):
                n = max(1, int(math.hypot((c - a) / DLON, (d - b) / DLAT) * 3))
                for k in range(n + 1):
                    q = cell(a + (c - a) * k / n, b + (d - b) * k / n)
                    if q >= 0:
                        flag[q] = 1

    road = np.zeros(W * H, np.int8)
    rl = [pl for _, _, pl in roads()]
    raster(rl, road)
    river = np.zeros(W * H, np.int8)
    with open(G + "/ne_50m_rivers_lake_centerlines.geojson", encoding="utf-8") as f:
        rv = []
        for ft in json.load(f)["features"]:
            g = ft["geometry"]
            if g:
                rv += [g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]
    raster(rv, river)
    towns = []
    for n, name, lo, la, _ in build():
        if not (X0 < lo < X1 and Y0 < la < Y1):
            continue
        r = 1 if n >= 10000 else 0                 # большой город — клетка и соседние
        for dj in range(-r, r + 1):
            for di in range(-r, r + 1):
                q = cell(lo + di * DLON, la + dj * DLAT)
                if q >= 0 and t[q] != WATER:
                    t[q] = TOWN
        towns.append({"name": name, "lon": round(lo, 4), "lat": round(la, 4), "pop": int(n)})
    road[t == WATER] = 0
    river[t == WATER] = 0

    data = {"name": "Акт I: Париж — Каркассон", "lon0": X0, "lat0": Y1, "dlon": DLON, "dlat": DLAT, "w": W, "h": H,
            "terrains": TERR, "terrain": "".join(map(str, t.tolist())), "road": "".join(map(str, road.tolist())),
            "river": "".join(map(str, river.tolist())), "towns": sorted(towns, key=lambda c: -c["pop"]), "labels": labels,
            "source": "kampaniya/karty/act1.py: Natural Earth, Itiner-e (CC BY 4.0), Buringh (CC0), Чандлер (CC BY 4.0)"}
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
    cnt = {TERR[k]: int((t == k).sum()) for k in range(8)}
    print(f"{out}: {W}×{H} клеток, {os.path.getsize(out) // 1024} КБ; {cnt}; дорог {int(road.sum())}, рек {int(river.sum())}, городов {len(towns)}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "web", "data", "region_act1.json"))
