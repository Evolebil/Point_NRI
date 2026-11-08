"""Карта рек и путей около 1208 года. Сначала: python3 build_routes.py. Запуск: python3 roads.py выход.png
Дороги — Itiner-e (римская сеть, CC BY 4.0), реки — Natural Earth; выбор путей — справка по памяти."""
import json, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from states import base, frame, G, X0, X1, Y0, Y1
from itinere import roads
RIVER_RU = {"Danube": "Дунай", "Volga": "Волга", "Euphrates": "Евфрат", "Dnipro": "Днепр", "Rhine": "Рейн", "Tigris": "Тигр",
 "Oder": "Одер", "Seine": "Сена", "Vistula": "Висла", "Daugava": "Двина", "Ebro": "Эбро", "Elbe": "Эльба", "Tajo": "Тахо",
 "Loire": "Луара", "Dniester": "Днестр", "Don": "Дон", "Rhône": "Рона", "Duero": "Дуэро", "Po": "По", "Thames": "Темза",
 "Tisza": "Тиса", "Garonne": "Гаронна", "Nile": "Нил"}
STYLE = {"land": dict(color="#8b1a1a", lw=2.2), "sea": dict(color="#1a4f8b", lw=1.8, ls=(0, (6, 3))),
         "river": dict(color="#0f7a6b", lw=2.6, ls=(0, (2, 1.5))), "act1": dict(color="#9b27b0", lw=3.2)}
def draw(ax, labels=True):
    for typ, _, pts in roads():  # вся римская сеть — тонко: эти дороги ещё в ходу
        ax.plot([p[0] for p in pts], [p[1] for p in pts], color="#7a6a5a", lw=.9 if typ == "Main Road" else .45, alpha=.55, zorder=4)
    for f in json.load(open(G + "/ne_50m_rivers_lake_centerlines.geojson"))["features"]:
        g, q = f["geometry"], f["properties"]
        if not g or "Canal" in (q.get("name") or "") or "Suez" in (q.get("name") or ""): continue
        for l in ([g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]):
            ax.plot([p[0] for p in l], [p[1] for p in l], color="#3a7fc0", lw=1.4, zorder=5)
    d = json.load(open(HERE + "/geodata/routes.json"))
    for leg in d["legs"]:
        st = dict(STYLE["act1" if leg["group"] == "act1" else leg["kind"]])
        if not leg["exact"]: st.update(ls=(0, (1, 2)), alpha=.8)
        xs = [p[0] for p in leg["line"]]; ys = [p[1] for p in leg["line"]]
        ax.plot(xs, ys, zorder=7 if leg["group"] != "act1" else 8, solid_capstyle="round", **st)
    if not labels: return d
    seen = set()
    for leg in d["legs"]:
        n = leg["route"]
        if not n or n in seen: continue
        seen.add(n); ln = leg["line"]; x, y = ln[len(ln) // 2]
        col = STYLE["act1" if leg["group"] == "act1" else leg["kind"]]["color"]
        ax.text(x, y, n, fontsize=8, zorder=10, color=col, bbox=dict(boxstyle="round,pad=0.15", fc="white", ec=col, alpha=.85))
    for name, (x, y) in d["points"].items():
        ax.plot(x, y, "o", ms=3.5, mfc="#222", mec="white", zorder=9)
        ax.text(x + .12, y - .28, name, fontsize=6.3, zorder=9, color="#222")
    return d
if __name__ == "__main__":
    fig, ax = plt.subplots(figsize=(22, 15))
    base(ax, alpha=.25)
    draw(ax)
    for k, lab in (("land", "Сухопутный путь (по римской дороге)"), ("river", "Речной путь"), ("sea", "Морской путь"), ("act1", "Варианты пути Акта I")):
        ax.plot([], [], label=lab, **STYLE[k])
    ax.plot([], [], color="#8b1a1a", lw=2, ls=(0, (1, 2)), label="Путь без известной трассы (напрямую)")
    ax.plot([], [], color="#7a6a5a", lw=.9, alpha=.6, label="Старые римские дороги")
    ax.plot([], [], color="#3a7fc0", lw=1.4, label="Река")
    ax.legend(loc="upper right", fontsize=10, framealpha=.92, title="Пути")
    frame(ax, "Реки и главные пути около 1208 года — мир кампании «Аномалии планов»",
          "Сухопутные пути проложены по сети римских дорог Itiner-e (de Soto и др., 2025, CC BY 4.0); морские — по морю в обход суши; реки — Natural Earth.\n"
          "Где римских дорог не было (Германия за Рейном, Польша, Русь, Месопотамия), путь показан пунктиром напрямую. Выбор путей — по памяти.")
    plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
