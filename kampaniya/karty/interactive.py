"""Интерактивная карта (Leaflet): слои-картинки в проекции Меркатора + города, пути и ресурсы векторами.
Подписи городов появляются по мере приближения. Запуск: python3 interactive.py папка_вывода [--html]"""
import json, math, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from states import base, realm_labels, X0, X1, Y0, Y1
import races, langs, roads as roadsmod
from cities_data import build
from resources import SITES, KIND, MITHRIL
from races import DRAGONS, DCOL
W = 4800
def merc(lat): return math.log(math.tan(math.pi / 4 + math.radians(lat) / 2))
def render(draw, path, transparent=False):
    H = round(W * (Y1 - Y0) / (X1 - X0))
    fig = plt.figure(figsize=(W / 100, H / 100), dpi=100); ax = fig.add_axes([0, 0, 1, 1])
    if transparent: fig.patch.set_alpha(0); ax.set_facecolor("none")
    draw(ax)
    if transparent: ax.set_facecolor("none")
    ax.set_xlim(X0, X1); ax.set_ylim(Y0, Y1); ax.set_aspect("auto"); ax.set_xticks([]); ax.set_yticks([])
    for sp in ax.spines.values(): sp.set_visible(False)  # не axis("off"): он прячет и фон-море
    fig.canvas.draw(); a = np.asarray(fig.canvas.buffer_rgba()).copy(); plt.close(fig)
    m0, m1 = merc(Y0), merc(Y1); Hm = round(W * (m1 - m0) / math.radians(X1 - X0))
    my = m1 - (np.arange(Hm) + .5) / Hm * (m1 - m0)
    lat = np.degrees(2 * np.arctan(np.exp(my)) - math.pi / 2)
    src = np.clip(((Y1 - lat) / (Y1 - Y0) * a.shape[0]).astype(int), 0, a.shape[0] - 1)
    Image.fromarray(a[src]).save(path, optimize=True)
def roads_only(ax):
    from itinere import roads
    for typ, _, pts in roads():
        ax.plot([p[0] for p in pts], [p[1] for p in pts], color="#5a4a3a", lw=1.2 if typ == "Main Road" else .6, alpha=.7)
    for f in json.load(open(HERE + "/geodata/ne_50m_rivers_lake_centerlines.geojson"))["features"]:
        g, q = f["geometry"], f["properties"]
        if not g or "Canal" in (q.get("name") or "") or "Suez" in (q.get("name") or ""): continue
        for l in ([g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]):
            ax.plot([p[0] for p in l], [p[1] for p in l], color="#2a6fb0", lw=1.6)
def minzoom(n): return 4 if n >= 50000 else 5 if n >= 20000 else 6 if n >= 8000 else 7 if n >= 3000 else 8
if __name__ == "__main__":
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    if "--html" not in sys.argv:  # --html: пересобрать только страницу, картинки слоёв не трогать
      render(lambda ax: realm_labels(ax, base(ax), big=8, small=6.5), out + "/gosudarstva.png")
      render(races.draw, out + "/rasy.png")
      render(langs.draw, out + "/yazyki.png")
      render(roads_only, out + "/dorogi.png", transparent=True)
    cities = [dict(n=name, p=n, x=round(lon, 3), y=round(lat, 3), z=minzoom(n)) for n, name, lon, lat, _ in build() if X0 < lon < X1 and Y0 < lat < Y1]
    legs = json.load(open(HERE + "/geodata/routes.json"))["legs"]
    routes = [dict(r=l["route"], k=l["kind"], g=l["group"], a=l["a"], b=l["b"], km=l["km"], e=l["exact"], l=[[p[1], p[0]] for p in l["line"]]) for l in legs]
    res = [dict(k=k, n=n, x=x, y=y, c=KIND[k][0]) for k, n, x, y in SITES]
    res += [dict(k="Мифрил", n="Мифрил: " + n, x=x, y=y, c="#7fd3e6") for n, x, y, w, h in MITHRIL]
    dragons = [dict(col=c, a=a, p=p, x=x, y=y, c=DCOL[c]) for c, a, p, x, y in DRAGONS]
    data = dict(bounds=[[Y0, X0], [Y1, X1]], cities=cities, routes=routes, res=res, dragons=dragons)
    html = open(HERE + "/interactive_template.html", encoding="utf-8").read().replace("/*DATA*/null", json.dumps(data, ensure_ascii=False))
    open(out + "/index.html", "w", encoding="utf-8").write(html)
    print("готово:", out, len(cities), "городов,", len(routes), "отрезков путей,", len(res), "ресурсов")
