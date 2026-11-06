"""Карта государств 1208 года. Запуск: python3 states.py выход.png"""
import json, math, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from realms import R, realm
G = HERE + "/geodata"
X0, X1, Y0, Y1 = -11, 50, 27, 63
def rings(g):
    if g["type"] == "Polygon": yield g["coordinates"][0]
    elif g["type"] == "MultiPolygon":
        for p in g["coordinates"]: yield p[0]
def visible(r):
    xs = [p[0] for p in r]; ys = [p[1] for p in r]
    return max(xs) > X0 and min(xs) < X1 and max(ys) > Y0 and min(ys) < Y1
# провинции, разделённые по широте: (страна, провинция) -> (широта, к югу, к северу)
SPLIT = {("GRC", "Peloponnisos"): (37.45, "ITA", "BYZ")}  # юг Мореи — Империи
def clip(r, lat, below):  # отсечение многоугольника полуплоскостью y < lat (или y >= lat)
    inside = (lambda q: q[1] < lat) if below else (lambda q: q[1] >= lat)
    out = []
    for i in range(len(r)):
        a, b = r[i - 1], r[i]
        if inside(b):
            if not inside(a): out.append(cross(a, b, lat))
            out.append(b)
        elif inside(a): out.append(cross(a, b, lat))
    return out
def cross(a, b, lat):
    t = (lat - a[1]) / (b[1] - a[1]); return [a[0] + t * (b[0] - a[0]), lat]
def base(ax, alpha=1.0):
    """Суша, государства, озёра, берега. Возвращает центры государств для подписей."""
    ax.set_facecolor("#cfe3f0")
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="#e6e1d6", ec="#7a8a99", lw=.6, zorder=1))
    cent = {}
    unknown = set()
    for f in json.load(open(G + "/ne_10m_admin_1.geojson"))["features"]:
        p = f["properties"]; k = realm(p)
        if k is None: continue
        if k not in R: unknown.add(k); continue
        col = R[k][1]
        split = SPLIT.get((p["adm0_a3"], p["name"]))
        for r in rings(f["geometry"]):
            if not visible(r): continue
            if split:
                lat, south, north = split
                for part, kk in ((clip(r, lat, True), south), (clip(r, lat, False), north)):
                    if len(part) > 2: ax.add_patch(Polygon(part, fc=R[kk][1], ec=R[kk][1] if alpha == 1 else "none", lw=.4, zorder=2, alpha=alpha))
            else:
                ax.add_patch(Polygon(r, fc=col, ec=col if alpha == 1 else "none", lw=.4, zorder=2, alpha=alpha))
        w = p["area_sqkm"] or 1
        if not (X0 < p["longitude"] < X1 and Y0 < p["latitude"] < Y1): continue
        c = cent.setdefault(k, [0, 0, 0]); c[0] += p["longitude"] * w; c[1] += p["latitude"] * w; c[2] += w
    for f in json.load(open(G + "/ne_50m_lakes.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="#cfe3f0", ec="#7a8a99", lw=.4, zorder=3))
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="none", ec="#5a6a79", lw=.7, zorder=4))
    if unknown: print("неизвестные коды:", unknown)
    return cent
CALL = {  # мелкие государства: подпись в стороне со стрелкой (долгота, широта подписи)
 "GAS": (-6.0, 46.6), "FLA": (2.8, 53.2), "TOU": (2.0, 40.4), "NAV": (-8.6, 44.5), "WAL": (-7.2, 50.2), "IRL_E": (-10.6, 51.3),
 "TRI": (29.5, 33.2), "JER": (29.5, 31.8), "ANT": (31.0, 38.3), "ARM": (31.5, 37.2), "CYP": (29.5, 34.5), "BOS": (14.0, 41.6),
 "PRU": (17.0, 57.5), "FRA_V": (-8.0, 48.6), "ARL": (6.8, 41.7)}
def realm_labels(ax, cent, big=10.5, small=8):
    for k, (sx, sy, w) in cent.items():
        x, y = sx / w, sy / w
        name = R[k][0]
        if k in CALL:
            tx, ty = CALL[k]
            ax.annotate(name, xy=(x, y), xytext=(tx, ty), fontsize=small, ha="center", va="center", zorder=6,
                        arrowprops=dict(arrowstyle="-", color="#333", lw=.7),
                        bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="#888", alpha=.8))
        else:
            ax.text(x, y, name, fontsize=big, ha="center", va="center", zorder=6, fontweight="bold", color="#1a1a1a",
                    bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none", alpha=.55))
def frame(ax, title, note):
    ax.set_xlim(X0, X1); ax.set_ylim(Y0, Y1); ax.set_aspect(1 / math.cos(math.radians(45)))
    ax.set_xticks([]); ax.set_yticks([])
    ax.set_title(title, fontsize=17, pad=12)
    ax.text(X0 + .3, Y0 + .3, note, fontsize=8.5, zorder=7, bbox=dict(fc="white", ec="#999", alpha=.85))
if __name__ == "__main__":
    fig, ax = plt.subplots(figsize=(22, 15))
    realm_labels(ax, base(ax))
    frame(ax, "Государства, лето 1208 года — мир кампании «Аномалии планов»",
          "Основа: провинции и берега Natural Earth; государства распределены по истории 1208 года с правками кампании.\n"
          "Границы приблизительные: проведены по современным провинциям.")
    plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
