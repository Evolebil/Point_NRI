"""Карта Акта I: местность от Парижа до Каркассона (игроки прокладывают путь сами).
Местность — для встреч по среде; леса и болота — приблизительные области. Запуск: python3 act1.py выход.png"""
import json, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Ellipse
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import states
states.X0, states.X1, states.Y0, states.Y1 = -3.2, 7.0, 42.4, 49.6
from states import base, frame, rings, G
import races
from itinere import roads
from cities_data import build
TER = {  # местность: (подпись, цвет)
 "forest": ("Лес", "#3f8f3f"), "hills": ("Холмы и плато", "#b5a46a"), "mount": ("Горы", "#8a6a4a"), "swamp": ("Болото", "#2e8b8b")}
PHYS = {"mount": ["ALPS", "PYRENEES"], "hills": ["Massif Central", "Vosges", "Jura"], "forest": ["Ardennes"]}
E = lambda x, y, w, h, lab: (x, y, w, h, lab)
ELL = {
 "forest": [E(2.1, 47.95, .7, .35, "Орлеанский лес: разбойники"), E(2.65, 48.4, .4, .25, "Фонтенбло"), E(2.85, 49.38, .45, .25, "Компьенский лес"),
            E(4.05, 47.15, .9, .7, "Морван: гоблины, зелёный вирмлинг"), E(-2.2, 48.0, .7, .4, "Броселианд: эльфы"), E(4.9, 49.15, .5, .5, "Аргонна"),
            E(0.7, 48.4, .7, .4, "Перш"), E(1.4, 45.6, 1.4, .8, "Лимузенские леса"), E(5.5, 46.4, .9, .6, "Бресс и Юра: гномы")],
 "swamp": [E(1.8, 47.5, 1.1, .5, "Солонь: булливаги"), E(-1.0, 46.35, 1.0, .35, "Пуатевенские болота"), E(-1.0, 44.2, 1.0, .9, "Ланды: ящеролюды"),
           E(3.6, 43.3, 1.4, .45, "лагуны Нарбонны и Камарги: булливаги, чёрный вирмлинг"), E(5.0, 46.0, .5, .35, "Домб"), E(-2.2, 47.35, .4, .25, "Бриер")],
}
RACE_IN = [("Центральный массив: гоблины и багбиры", 3.0, 45.0), ("Пиренеи: голиафы", -1.6, 43.1), ("Альпы: драконы и кобольды", 5.7, 44.5),
           ("Арденны: гоблиноиды, зелёный дракон (взрослый)", 5.1, 49.95), ("Нормандия: хутора полуросликов", -0.5, 49.15)]
if __name__ == "__main__":
    fig, ax = plt.subplots(figsize=(18, 15))
    base(ax, alpha=.18)
    phys = {f["properties"]["NAME"]: f for f in json.load(open(G + "/ne_10m_geography_regions_polys.geojson"))["features"]}
    for k, names in PHYS.items():
        for n in names:
            for r in rings(phys[n]["geometry"]): ax.add_patch(Polygon(r, fc=TER[k][1], ec=TER[k][1], alpha=.55, zorder=3))
    for k, ells in ELL.items():
        for x, y, w, h, lab in ells:
            ax.add_patch(Ellipse((x, y), w, h, fc=TER[k][1], ec="#222", lw=.6, alpha=.6, zorder=3))
            ax.text(x, y - h / 2 - .04, lab, fontsize=8, ha="center", va="top", zorder=10, clip_on=True,
                    bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="none", alpha=.7))
    for typ, _, pts in roads():
        ax.plot([p[0] for p in pts], [p[1] for p in pts], color="#5a4a3a", lw=1.3 if typ == "Main Road" else .6, alpha=.6, zorder=4)
    for f in json.load(open(G + "/ne_50m_rivers_lake_centerlines.geojson"))["features"]:
        g = f["geometry"]
        if not g: continue
        for l in ([g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]):
            ax.plot([p[0] for p in l], [p[1] for p in l], color="#2a6fb0", lw=2, zorder=5)
    for lab, x, y in RACE_IN:
        ax.text(x, y, lab, fontsize=10, ha="center", fontweight="bold", zorder=10, clip_on=True,
                bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="#444", alpha=.85))
    for col, age, place, x, y in races.DRAGONS:
        if states.X0 < x < states.X1 and states.Y0 < y < states.Y1:
            ax.plot(x, y, "*", ms=races.AGE[age] + 4, mfc=races.DCOL[col], mec="#222", mew=1.2, zorder=11)
    for n, name, lon, lat, _ in build():
        if not (states.X0 < lon < states.X1 and states.Y0 < lat < states.Y1): continue
        big = n >= 10000
        ax.plot(lon, lat, "o", ms=7 if big else 3.5 if n >= 3000 else 2.2, mfc="#b22222" if big else "#333", mec="white", mew=.6, zorder=9)
        if n >= 3000 or name in ("Каркассон", "Нарбонна"):
            ax.text(lon + .05, lat + .03, name, fontsize=9 if big else 7, zorder=9, fontweight="bold" if big else "normal", clip_on=True,
                    bbox=dict(boxstyle="round,pad=0.08", fc="white", ec="none", alpha=.55))
    for x, y in ((2.35, 48.86), (2.35, 43.21)):
        ax.plot(x, y, "o", ms=22, mfc="none", mec="#d10000", mew=3.5, zorder=12)
    for k, (lab, col) in TER.items(): ax.plot([], [], "s", ms=14, mfc=col, alpha=.7, mec="#222", label=lab)
    ax.plot([], [], color="#5a4a3a", lw=1.3, label="Старая римская дорога: путь ×1")
    ax.plot([], [], color="#2a6fb0", lw=2, label="Река: брод, мост или лодка")
    ax.plot([], [], "*", ms=14, mfc="#999", mec="#222", label="Логово дракона (вирмлинг/взрослый)")
    ax.plot([], [], "o", ms=14, mfc="none", mec="#d10000", mew=3, label="Париж и Каркассон")
    ax.legend(loc="upper left", bbox_to_anchor=(1.005, 1), fontsize=11, framealpha=.93,
              title="Местность Акта I\n(путь прокладывают игроки)")
    frame(ax, "Акт I — Каркассон: местность от Парижа до юга",
          "По прямой Париж — Каркассон ~600 км. Скорость: по дороге ×1, равнина ×0,75, лес и холмы ×0,5, горы и болота ×0,33 (см. «Масштабы и ориентиры»).\n"
          "Встречи — по местности, где отряд провёл день (таблицы в Акте I). Леса и болота — приблизительные области.")
    plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
