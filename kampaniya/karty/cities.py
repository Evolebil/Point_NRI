"""Карта городов около 1200 года поверх государств 1208. Запуск: python3 cities.py выход.png список.md
Население — Чандлер (1987), оцифровка Reba, Reitsma, Seto (2016), chandlerV2.csv, CC BY 4.0."""
import csv, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from states import base, frame, X0, X1, Y0, Y1
RU = {"Fez": "Фес", "Cairo": "Каир", "Seville": "Севилья", "Palermo": "Палермо", "Marrakech": "Марракеш", "Istanbul": "Константинополь",
 "Paris": "Париж", "Dumyat": "Дамиетта", "Baghdad": "Багдад", "Damascus": "Дамаск", "Tbilisi": "Тбилиси", "Venice": "Венеция",
 "Konya": "Конья", "Milan": "Милан", "Granada": "Гранада", "Cordoba": "Кордова", "Rabat": "Рабат", "Mosul": "Мосул", "Cologne": "Кёльн",
 "Bejaia": "Беджая", "Basra": "Басра", "Alexandria": "Александрия", "Sivas": "Сивас", "Aleppo": "Алеппо", "Rouen": "Руан",
 "London": "Лондон", "Leon": "Леон", "Kish": "Киш", "Kayseri": "Кайсери", "Ceuta": "Сеута", "Acre": "Акра", "Veliko Tarnovo": "Тырново",
 "Smolensk": "Смоленск", "Rome": "Рим", "Naples": "Неаполь", "Messina": "Мессина", "Bologna": "Болонья", "Verona": "Верона",
 "Toledo": "Толедо", "Narbonne": "Нарбонна", "Thessaloniki": "Фессалоники", "Speyer": "Шпайер", "Kiev": "Киев", "Genova": "Генуя",
 "Worms": "Вормс", "Orleans": "Орлеан", "Trier": "Трир", "Poitiers": "Пуатье", "Pisa": "Пиза", "Pavia": "Павия", "Novgorod": "Новгород",
 "Mainz": "Майнц", "Gent": "Гент", "Cremona": "Кремона", "Coimbra": "Коимбра", "Asyut": "Асьют", "Palma": "Пальма", "Metz": "Мец",
 "Prague": "Прага", "Lyon": "Лион", "Erfurt": "Эрфурт", "Braunschweig": "Брауншвейг", "Troyes": "Труа", "Tours": "Тур",
 "Toulouse": "Тулуза", "Saint-Omer": "Сент-Омер", "Provins": "Провен", "Padua": "Падуя", "Marseille": "Марсель", "Liege": "Льеж",
 "Caen": "Кан", "Beziers": "Безье", "Angers": "Анже", "Vladimir": "Владимир", "Ferrara": "Феррара", "Esztergom": "Эстергом",
 "Arles": "Арль", "Zaragoza": "Сарагоса", "Wiener Neustadt": "Винер-Нойштадт", "Soest": "Зост", "Siena": "Сиена", "Lisbon": "Лиссабон",
 "Jerez de la Frontera": "Херес", "Florence": "Флоренция", "Bordeaux": "Бордо", "Barcelona": "Барселона", "Alessandria": "Алессандрия",
 "Montpellier": "Монпелье", "Vienna": "Вена", "Utrecht": "Утрехт", "Reims": "Реймс", "Mantova": "Мантуя", "Augsberg": "Аугсбург",
 "Lucca": "Лукка", "Louveigne": "Лувен", "Avignon": "Авиньон", "Geneva": "Женева", "Frankfurt am Main": "Франкфурт"}
SKIP = {"Gelibolu"}  # 150 000 у Галлиполи — вероятная ошибка набора (дубль Константинополя)
TIERS = [(50000, "Великий город (больше 50 000)", 15, 10), (20000, "Крупный город (20 000–50 000)", 10, 8.5),
         (5000, "Город (5 000–20 000)", 6.5, 7), (1000, "Городок (1 000–5 000)", 4.5, 6.5)]
def tier(n):
    for t in TIERS:
        if n >= t[0]: return t
rows = list(csv.reader(open(HERE + "/geodata/chandlerV2.csv", encoding="latin-1")))
h = rows[0]; i1200 = h.index("AD_1200")
cities = []
for r in rows[1:]:
    try: lat, lon = float(r[3]), float(r[4])
    except ValueError: continue
    if r[i1200] and X0 < lon < X1 and Y0 < lat < Y1 and r[0] not in SKIP:
        cities.append((int(float(r[i1200])), RU.get(r[0], r[0]), lon, lat))
cities.sort(reverse=True)
fig, ax = plt.subplots(figsize=(22, 15))
base(ax, alpha=.45)
for n, name, lon, lat in cities:
    t = tier(n)
    ax.plot(lon, lat, "o", ms=t[2], mfc="#b22222" if n >= 20000 else "#333", mec="white", mew=1, zorder=8)
    ax.text(lon + .25, lat + .12, name, fontsize=t[3], zorder=9, fontweight="bold" if n >= 50000 else "normal",
            bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="none", alpha=.6))
for t in TIERS[:3]:
    ax.plot([], [], "o", ms=t[2], mfc="#b22222" if t[0] >= 20000 else "#333", mec="white", label=t[1])
ax.legend(loc="upper right", fontsize=10, framealpha=.9, title="Ступени поселений")
frame(ax, "Города около 1200 года — мир кампании «Аномалии планов»",
      "Население — Т. Чандлер (1987), оцифровка Reba, Reitsma, Seto (2016), CC BY 4.0. В наборе в основном города от ~10 000 жителей;\n"
      "городки и сёла не показаны. Фон — государства 1208 года.")
plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
with open(sys.argv[2], "w") as out:
    for t in TIERS[:3]:
        out.write(f"\n### {t[1]}\n\n| Город | Жителей около 1200 |\n|---|---|\n")
        for n, name, lon, lat in cities:
            if tier(n) is t: out.write(f"| {name} | {n:,} |\n".replace(",", " "))
print(len(cities), "городов")
