"""Карта рас по биомам (четвёртая редакция, 9 ноября 2026): владелец поручил забыть его расстановку и распределить расы
самому по законам D&D и «Властелина колец», по биомам. Основа: биомы — biomes.py; горы — Natural Earth;
древние леса, болота, пустоши — приблизительные области. Запуск: python3 races.py выход.png"""
import json, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Ellipse
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import states
from states import rings, frame, G, _provinces
from biomes import BIOME, biome
from realms import realm
# Кто где живёт по биому (закон D&D / «Властелина колец»)
WHO = {
 "tundra": "ледяные великаны, белые драконы; люди-кочевники (саамы)",
 "taiga": "люди-охотники; тролли (Скандинавия), оборотни (северная Русь)",
 "temperate": "люди — большинство; полурослики на хуторах; полуэльфы у эльфийских лесов; в городах — тифлинги, аасимары, драконорождённые",
 "med": "люди — большинство; сатиры и дриады в рощах, кентавры Фессалии, минотавры Крита, гарпии на скалах",
 "steppe": "кентавры и люди-кочевники; хобгоблины (Анатолийское плато); гноллы (Месопотамия)",
 "desert": "гноллы, юань-ти в руинах; синие и латунные драконы; люди — в оазисах и по Нилу",
 "mount": "дварфы, великаны, голиафы, орки и тролли, драконы и кобольды — по хребтам (см. подписи)",
 "wetland": "ящеролюды, булливаги, карги; чёрные драконы",
 "oldforest": "эльфы (лесные и высшие), древни (энты), фирболги, феи; зелёные драконы",
 "waste": "орки — пустоши старых битв (как Мордор)"}
MOUNT = {  # хребет Natural Earth -> кто
 "KJØLEN MOUNTAINS": "горные дварфы, тролли", "Hardanger-vidda": "горные дварфы", "ALPS": "9 логов драконов, кобольды; глубоко — дварфы",
 "PYRENEES": "голиафы", "CARPATHIAN MOUNTAINS": "каменные великаны", "Transylvanian Alps": "каменные великаны", "Tatra Mts.": "горные дварфы (рудники)",
 "Erzgebirge": "холмовые дварфы", "Harz": "холмовые дварфы", "Sudetes Mts.": "холмовые дварфы", "Cord. Cantábrica": "горные дварфы",
 "Dinaric Alps": "орки и тролли", "Balkan Mts.": "орки, холмовые великаны", "Taurus Mts.": "орки", "CAUCASUS MTS.": "облачные великаны",
 "Vosges": "скальные гномы", "Jura": "скальные гномы", "APPENNINI": "гоблины в пещерах", "Massif Central": "гоблины и багбиры",
 "Ardennes": "лесные эльфы", "Böhmerwald": "лесные эльфы", "Grampian Mts.": "высшие эльфы", "Cambrian Mts.": "высшие эльфы",
 "Pindus Mts.": "сатиры, кентавры", "ATLAS MOUNTAINS": "леонины и табакси", "HAUT ATLAS": "леонины и табакси", "Lebanon Mts.": "горцы-люди",
 "Lesser Caucasus": "облачные великаны"}
MLAB = {  # где подписывать хребты
 "KJØLEN MOUNTAINS": (13.5, 64.6), "ALPS": (10.5, 47.2), "PYRENEES": (-1.0, 43.0), "CARPATHIAN MOUNTAINS": (24.8, 47.9), "Tatra Mts.": (19.9, 49.7),
 "Erzgebirge": (13.0, 50.75), "Harz": (10.6, 52.05), "Cord. Cantábrica": (-5.0, 43.4), "Dinaric Alps": (17.2, 44.0), "Balkan Mts.": (25.0, 43.0),
 "Taurus Mts.": (33.0, 36.7), "CAUCASUS MTS.": (43.5, 43.6), "Vosges": (7.0, 48.3), "APPENNINI": (13.6, 42.0), "Massif Central": (3.0, 45.0),
 "Ardennes": (5.4, 49.9), "Böhmerwald": (13.2, 48.6), "Grampian Mts.": (-4.0, 56.8), "Cambrian Mts.": (-3.7, 52.3), "Pindus Mts.": (21.4, 39.6),
 "ATLAS MOUNTAINS": (-5.5, 31.6), "Hardanger-vidda": (8.0, 60.1)}
E = lambda x, y, w, h, lab: (x, y, w, h, lab)
EXTRA = {  # приблизительные области: биом -> [(x, y, ширина, высота, кто)]
 "oldforest": [E(8.2, 48.3, 1.0, 1.3, "Шварцвальд: лесные эльфы"), E(-2.2, 48.0, .9, .5, "Броселианд: высшие эльфы и феи"),
               E(-1.1, 53.2, .7, .5, "Шервуд: лесные эльфы"), E(-1.6, 50.87, .6, .35, "Нью-Форест"), E(-2.55, 51.8, .45, .35, "лес Дин"),
               E(23.9, 52.7, 2.2, 1.2, "Беловежская пуща: древни, фирболги"), E(24.5, 55.3, 2.6, 1.0, "литовские пущи: феи"),
               E(23.5, 47.8, 2.0, .8, "Карпатские леса: лесные эльфы")],
 "wetland": [E(27.0, 52.0, 5.0, 1.4, "Полесье"), E(0.1, 52.6, .9, .5, "Фены"), E(3.6, 43.3, 1.4, .45, "лагуны Нарбонны"),
             E(12.2, 45.0, .8, .5, "дельта По"), E(29.3, 45.1, 1.2, .5, "дельта Дуная"), E(-1.0, 44.2, 1.0, .9, "Ланды"), E(1.8, 47.5, 1.1, .5, "Солонь"),
             E(31.0, 31.2, 1.2, .5, "дельта Нила")],
 "waste": [E(-3.6, 39.1, 3.0, 1.4, "Ла-Манча: Аларкос (1195)"), E(35.45, 32.8, 1.0, .7, "Галилея: Хаттин (1187)"),
           E(42.4, 39.1, 3.0, 1.3, "Манцикерт (1071)"), E(24.5, 41.4, 3.2, 1.0, "Фракия: война Византии и Империи (1204–1207)"),
           E(21.0, 54.3, 2.6, .9, "Прусское пограничье: набеги пруссов и мазовшан")],
}
PEOPLE = [  # отдельные народы и места внутри биомов
 ("гоблины", 4.05, 47.45), ("полурослики", -1.8, 51.85), ("полурослики", -0.6, 49.0), ("полурослики", 4.6, 47.0), ("полурослики", 10.8, 49.6),
 ("кентавры", 19.7, 46.6), ("кентавры и половцы", 36.0, 47.3), ("кентавры", 22.3, 39.5), ("минотавры", 24.9, 35.2),
 ("сатиры и дриады", 22.2, 37.5), ("огненные великаны", 15.0, 37.6), ("огненные великаны", 14.4, 40.8), ("хобгоблины", 33.5, 38.8),
 ("гноллы", 41.5, 35.5), ("юань-ти (руины)", 29.0, 28.5), ("ледяные великаны", 21.5, 68.5), ("тролли", 15.5, 61.0),
 ("оборотни", 38.0, 58.5), ("люди-саамы", 26.0, 68.8), ("гноллы", 6.0, 30.5)]
SEA = [(-9.5, 46.0, "тритоны, мерфолки, штормовые великаны"), (18.0, 35.0, "тритоны, мерфолки, морские эльфы"), (25.5, 38.2, "штормовые великаны"),
       (4.0, 57.5, "тритоны"), (20.0, 58.5, "мерфолки")]
from dragons import DRAGONS, DCOL, AGE  # логова драконов — отдельный модуль
def draw(ax, dragon_labels=True, labels=True):
    ax.set_facecolor("#cfe3f0")
    vis = states.visible
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if vis(r): ax.add_patch(Polygon(r, fc=BIOME["temperate"][1], ec="none", zorder=1))
    for f in _provinces():
        p = f["properties"]
        if realm(p) is None and not (-11 < p["longitude"] < 50 and 27 < p["latitude"] < 72): continue
        col = BIOME[biome(p)][1]
        for r in rings(f["geometry"]):
            if vis(r): ax.add_patch(Polygon(r, fc=col, ec=col, lw=.3, zorder=2))
    phys = {f["properties"]["NAME"]: f for f in json.load(open(G + "/ne_10m_geography_regions_polys.geojson"))["features"]}
    for name in MOUNT:
        for r in rings(phys[name]["geometry"]):
            if vis(r): ax.add_patch(Polygon(r, fc=BIOME["mount"][1], ec="#5a4030", lw=.8, alpha=.75, zorder=3))
    for k, ells in EXTRA.items():
        for x, y, w, h, lab in ells:
            ax.add_patch(Ellipse((x, y), w, h, fc=BIOME[k][1], ec="#222", lw=.6, alpha=.85, zorder=3))
            if labels: ax.text(x, y - h / 2 - .08, lab, fontsize=6.6, ha="center", va="top", zorder=9,
                               bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="none", alpha=.65))
    for fn in ("ne_50m_lakes", "ne_50m_land"):
        for f in json.load(open(G + f"/{fn}.geojson"))["features"]:
            for r in rings(f["geometry"]):
                if vis(r): ax.add_patch(Polygon(r, fc="#cfe3f0" if fn.endswith("lakes") else "none", ec="#5a6a79", lw=.6, zorder=5))
    for col, age, place, x, y in DRAGONS:
        ax.plot(x, y, "*", ms=AGE[age], mfc=DCOL[col], mec="#222", mew=1.1, zorder=11)
        alps = 5.5 < x < 14 and 45 < y < 48  # в Альпах логова теснятся — их список в заметке и на интерактивной карте
        if dragon_labels and not alps: ax.text(x + .25, y + .12, f"{col} {age}", fontsize=6.3, zorder=11,
                                  bbox=dict(boxstyle="round,pad=0.06", fc="white", ec="none", alpha=.6))
    if not labels: return
    for name, (x, y) in MLAB.items():
        ax.text(x, y, MOUNT[name], fontsize=8.5, ha="center", va="center", fontweight="bold", zorder=10, color="#3a2410",
                bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="#7a5a3a", lw=1, alpha=.85))
    for t, x, y in PEOPLE:
        ax.text(x, y, t, fontsize=8.5, ha="center", va="center", zorder=10, color="#222", style="italic",
                bbox=dict(boxstyle="round,pad=0.12", fc="#fffbe8", ec="#999", lw=.6, alpha=.85))
    for x, y, t in SEA: ax.text(x, y, t, fontsize=8.5, color="#1a4f8b", style="italic", ha="center", zorder=6)
if __name__ == "__main__":
    states.Y1 = 71
    fig, ax = plt.subplots(figsize=(22, 17)); draw(ax)
    for k, (lab, col) in BIOME.items():
        ax.plot([], [], "s", ms=13, mfc=col, mec="#333", label=f"{lab}: {WHO[k]}")
    for age, ms in AGE.items(): ax.plot([], [], "*", ms=ms, mfc="#999", mec="#222", label=f"дракон: {age}")
    ax.legend(loc="upper left", bbox_to_anchor=(1.005, 1), fontsize=8.6, framealpha=.93, title="Биомы и кто в них живёт (D&D, «Властелин колец»)")
    frame(ax, "Расы по биомам — мир кампании «Аномалии планов»",
          "Каждая раса — в своём биоме: люди — пашни, долины, побережья; эльфы — древние леса; дварфы — горы с рудами; орки — горы и пустоши битв; кентавры — степи.\n"
          "Биомы приблизительные (по провинциям, широте и хребтам Natural Earth). Решение ИИ по поручению владельца, 9 ноября 2026.")
    plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
