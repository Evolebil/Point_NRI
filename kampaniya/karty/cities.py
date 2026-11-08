"""Карта городов около 1200 года поверх государств 1208. Запуск: python3 cities.py выход.png список.md
Население — Чандлер (1987), оцифровка Reba, Reitsma, Seto (2016), chandlerV2.csv, CC BY 4.0."""
import os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from states import base, frame, X0, X1, Y0, Y1
from cities_data import build
TIERS = [(50000, "Великий город (больше 50 000)", 15, 10), (20000, "Крупный город (20 000–50 000)", 10, 8.5),
         (5000, "Город (5 000–20 000)", 4.5, 7), (1000, "Городок (1 000–5 000)", 2.2, 6.5)]
def tier(n):
    for t in TIERS:
        if n >= t[0]: return t
cities = [(n, name, lon, lat) for n, name, lon, lat, _ in build() if X0 < lon < X1 and Y0 < lat < Y1]
fig, ax = plt.subplots(figsize=(22, 15))
base(ax, alpha=.45)
for n, name, lon, lat in reversed(cities):
    t = tier(n)
    ax.plot(lon, lat, "o", ms=t[2], mfc="#b22222" if n >= 20000 else "#333", mec="white", mew=.6 if n < 5000 else 1, zorder=8)
    if n < 15000: continue  # подписи мелких — на интерактивной карте
    ax.text(lon + .25, lat + .12, name, fontsize=t[3], zorder=9, fontweight="bold" if n >= 50000 else "normal",
            bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="none", alpha=.6))
for t in TIERS:
    ax.plot([], [], "o", ms=t[2], mfc="#b22222" if t[0] >= 20000 else "#333", mec="white", label=t[1])
ax.legend(loc="upper right", fontsize=10, framealpha=.9, title="Ступени поселений")
frame(ax, "Города около 1200 года — мир кампании «Аномалии планов»",
      "Европа — Э. Buringh, «European urban population 700–2000» (2021, CC0); вне Европы — Т. Чандлер (Reba и др. 2016, CC BY 4.0).\n"
      "Подписаны города от 15 000 жителей; все подписи — на интерактивной карте. Сёла и деревни не показаны.")
plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
with open(sys.argv[2], "w") as out:
    for t in TIERS:
        out.write(f"\n### {t[1]}\n\n| Город | Жителей около 1200 |\n|---|---|\n")
        for n, name, lon, lat in cities:
            if tier(n) is t: out.write(f"| {name} | {n:,} |\n".replace(",", " "))
print(len(cities), "городов")
