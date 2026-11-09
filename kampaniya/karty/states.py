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
# разделённые провинции: (страна, провинция) -> (широта, к югу, к северу) или ("lon", долгота, к западу, к востоку)
SPLIT = {("GRC", "Peloponnisos"): (37.45, "ITA", "BYZ"),        # юг Мореи — Империи
         ("POL", "Podlachian"): (53.6, "POL", "PRU"),          # север Подляшья — ятвяги (Судовия)
         ("POL", "Pomeranian"): ("lon", 18.9, "POL", "PRU")}   # к востоку от Вислы — Помезания, земля пруссов
def clip(r, lat, below, ax=1):  # отсечение многоугольника полуплоскостью: координата ax < lat (или >= lat)
    inside = (lambda q: q[ax] < lat) if below else (lambda q: q[ax] >= lat)
    out = []
    for i in range(len(r)):
        a, b = r[i - 1], r[i]
        if inside(b):
            if not inside(a): out.append(cross(a, b, lat, ax))
            out.append(b)
        elif inside(a): out.append(cross(a, b, lat, ax))
    return out
def cross(a, b, v, ax=1):
    t = (v - a[ax]) / (b[ax] - a[ax]); p = [a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])]; p[ax] = v; return p
_PROV = None
def _provinces():
    global _PROV
    if _PROV is None: _PROV = json.load(open(G + "/ne_10m_admin_1.geojson"))["features"]
    return _PROV
def province_polys():
    """(государство, контур) для всех видимых провинций; разделённые по широте — двумя частями"""
    for f in _provinces():
        p = f["properties"]; k = realm(p)
        if k is None or k not in R: continue
        split = SPLIT.get((p["adm0_a3"], p["name"]))
        for r in rings(f["geometry"]):
            if not visible(r): continue
            if split:
                ax_, (lat, south, north) = (0, split[1:]) if split[0] == "lon" else (1, split)
                for part, kk in ((clip(r, lat, True, ax_), south), (clip(r, lat, False, ax_), north)):
                    if len(part) > 2: yield kk, part
            else: yield k, r
def province_centers():
    for f in _provinces():
        p = f["properties"]; k = realm(p)
        if k in R and X0 < p["longitude"] < X1 and Y0 < p["latitude"] < Y1: yield k, p["longitude"], p["latitude"], p["area_sqkm"] or 1
def draw_borders(ax, px=3000):
    """Границы государств: рисуем карту номеров государств без сглаживания и обводим места, где номер меняется."""
    import numpy as np
    keys = {k: i + 1 for i, k in enumerate(R)}
    H = round(px * (Y1 - Y0) / (X1 - X0))
    f2 = plt.figure(figsize=(px / 100, H / 100), dpi=100); a2 = f2.add_axes([0, 0, 1, 1])
    for k, r in province_polys():
        i = keys[k]; a2.add_patch(Polygon(r, fc=(i / 255, 0, 0), ec=(i / 255, 0, 0), lw=.3, antialiased=False))
    a2.set_xlim(X0, X1); a2.set_ylim(Y0, Y1); a2.set_facecolor((0, 0, 0)); a2.set_xticks([]); a2.set_yticks([])
    for sp in a2.spines.values(): sp.set_visible(False)
    f2.canvas.draw(); idm = np.asarray(f2.canvas.buffer_rgba())[:, :, 0].astype(int); plt.close(f2)
    m = np.zeros_like(idm, bool)
    for d in ((0, 1), (1, 0)):
        a, b = idm[: idm.shape[0] - d[0], : idm.shape[1] - d[1]], idm[d[0]:, d[1]:]
        e = (a != b) & (a > 0) & (b > 0); m[: e.shape[0], : e.shape[1]] |= e
    m = m | np.roll(m, 1, 0) | np.roll(m, 1, 1)  # потолще
    img = np.zeros(m.shape + (4,)); img[m] = (.25, .2, .15, .85)
    ax.imshow(img, extent=(X0, X1, Y0, Y1), origin="upper", interpolation="nearest", aspect="auto", zorder=3.5)
def base(ax, alpha=1.0, borders=True):
    """Суша, государства, озёра, берега. Возвращает центры государств для подписей."""
    ax.set_facecolor("#cfe3f0")
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="#e6e1d6", ec="#7a8a99", lw=.6, zorder=1))
    cent = {}
    for k, r in province_polys():
        col = R[k][1]
        ax.add_patch(Polygon(r, fc=col, ec=col if alpha == 1 else "none", lw=.4, zorder=2, alpha=alpha))
    for k, x, y, w in province_centers():
        c = cent.setdefault(k, [0, 0, 0]); c[0] += x * w; c[1] += y * w; c[2] += w
    if borders: draw_borders(ax)
    for f in json.load(open(G + "/ne_50m_lakes.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="#cfe3f0", ec="#7a8a99", lw=.4, zorder=3))
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="none", ec="#5a6a79", lw=.7, zorder=4))
    return cent
CALL = {  # мелкие государства: подпись в стороне со стрелкой (долгота, широта подписи)
 "GAS": (-6.0, 46.6), "FLA": (2.8, 53.2), "TOU": (1.4, 39.2), "NAV": (-8.6, 44.5), "WAL": (-7.2, 50.2), "IRL_E": (-10.6, 51.3),
 "TRI": (29.5, 33.2), "JER": (29.5, 31.8), "ANT": (31.0, 38.3), "ARM": (31.5, 37.2), "CYP": (29.5, 34.5), "BOS": (14.0, 41.6),
 "PRU": (17.2, 55.9), "FRA_V": (-8.0, 48.6), "ARL": (5.6, 38.4)}
POS = {"GER": (10.3, 52.4), "BOH": (15.2, 49.3), "LEO": (-6.2, 42.4), "CAS": (-3.3, 40.4), "POR": (-8.2, 38.6), "ARA": (0.4, 41.6)}  # ручные места подписей
def realm_labels(ax, cent, big=10.5, small=8):
    for k, (sx, sy, w) in cent.items():
        x, y = POS.get(k, (sx / w, sy / w))
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
