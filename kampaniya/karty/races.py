"""Карта рас (решение владельца 4 ноября 2026). Запуск: python3 races.py выход.png"""
import json, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from realms import realm
from states import rings, visible, frame, X0, X1, Y0, Y1, G
RACE = {  # раса: (подпись, цвет)
 "dwarf": ("Дварфы", "#7a5a3a"), "giant": ("Великаны", "#8e6fb0"), "dragon": ("Драконы", "#c0392b"),
 "elf": ("Высшие эльфы\nи их подвиды\n(правят\nполуэльфы)", "#2e9e6b"), "human": ("Люди", "#e8c98a"),
 "beast": ("Животноподобные расы", "#d08a3a"), "orc": ("Кентавры и орки", "#5d7a2a"), "none": ("Не задано", "#d9d9d9")}
MOUNTAIN = {  # горы перекрывают страну: (страна, провинции)
 "dragon": {"CHE": None, "AUT": "Tirol Vorarlberg Salzburg Kärnten", "ITA": "Aoste Bozen Trento Sondrio Belluno Verbano-Cusio-Ossola Bergamo Brescia Udine Cuneo Turin Biella",
            "FRA": "Savoie Haute-Savoie Hautes-Alpes Alpes-de-Haute-Provence Alpes-Maritimes Isère", "SVN": None},
 "giant": {"ROU": "Maramures Suceava Neamt Harghita Covasna Brasov Prahova Buzau Vrancea Arges Vâlcea Sibiu Hunedoara Gorj Caras-Severin Alba Bistrita-Nasaud Bacau Dâmbovita Mures",
           "SVK": "Žilinský Prešov Banskobystrický Košický", "UKR": "Transcarpathia Ivano-Frankivs'k Chernivtsi L'viv", "POL": "Subcarpathian Lesser Poland"}}
def race(p):
    a, n = p["adm0_a3"], p["name"] or ""
    for r, m in MOUNTAIN.items():
        if a in m and (m[a] is None or (" " + n + " ") in (" " + m[a] + " ")): return r
    if a in ("NOR", "SWE"): return "dwarf"
    if a == "GBR" and (p["region"] or "") != "Northern Ireland": return "elf"
    if a in ("ITA", "SMR", "VAT", "FRA", "MCO", "RUS", "UKR", "BLR"): return "human"
    if a in ("EGY", "LBY", "TUN", "DZA", "MAR"): return "beast"
    if realm(p) == "GER": return "orc"
    return "human"  # остальные земли — люди (решение владельца 4 ноября 2026)
if __name__ == "__main__":
    fig, ax = plt.subplots(figsize=(22, 15)); ax.set_facecolor("#cfe3f0")
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc=RACE["none"][1], ec="none", zorder=1))
    cent = {}
    for f in json.load(open(G + "/ne_10m_admin_1.geojson"))["features"]:
        p = f["properties"]
        if realm(p) is None: continue
        k = race(p); col = RACE[k][1]
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc=col, ec=col, lw=.4, zorder=2, hatch="//" if k == "none" else None))
        if X0 < p["longitude"] < X1 and Y0 < p["latitude"] < Y1 and k != "none":
            w = p["area_sqkm"] or 1; c = cent.setdefault(k, [0, 0, 0]); c[0] += p["longitude"] * w; c[1] += p["latitude"] * w; c[2] += w
    for fn in ("ne_50m_lakes", "ne_50m_land"):
        for f in json.load(open(G + f"/{fn}.geojson"))["features"]:
            for r in rings(f["geometry"]):
                if visible(r): ax.add_patch(Polygon(r, fc="#cfe3f0" if fn.endswith("lakes") else "none", ec="#5a6a79", lw=.6, zorder=3))
    POS = {"human": [(2.5, 46.8), (12.5, 42.6), (36.0, 54.0), (-4.0, 40.0), (33.0, 39.0), (20.0, 44.5), (40.0, 33.0)], "beast": [(3.0, 31.0), (29.5, 30.2)], "elf": [(-1.4, 52.9)], "dragon": [(10.0, 46.6)]}
    for k, (sx, sy, w) in cent.items():
        for x, y in POS.get(k, [(sx / w, sy / w)]):
            ax.text(x, y, RACE[k][0], fontsize=11, ha="center", va="center", fontweight="bold", zorder=6,
                    bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none", alpha=.7))
    for k, (lab, col) in RACE.items():
        ax.plot([], [], "s", ms=12, mfc=col, mec="#333", label=lab.replace("\n", " "))
    ax.legend(loc="upper left", bbox_to_anchor=(1.005, 1), fontsize=10, framealpha=.92, title="Раса большинства")
    frame(ax, "Расы — мир кампании «Аномалии планов»",
          "Решение владельца, 4 ноября 2026. Показана раса большинства; остальные малочисленные расы живут по всем землям равномерно.\n"
          "Голиафы — выходцы из Южной Америки (за краем карты). Во всех остальных землях большинство — люди.")
    plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
