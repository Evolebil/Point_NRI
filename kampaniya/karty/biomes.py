"""Биомы (приблизительно): каждой современной провинции — биом по широте, климатической области и физическим районам Natural Earth.
Тундра, тайга, умеренные леса и пашни, средиземноморье, степь, пустыня; горы — отдельным слоем по хребтам Natural Earth."""
import json, os
import numpy as np
from matplotlib.path import Path
HERE = os.path.dirname(os.path.abspath(__file__))
G = HERE + "/geodata"
BIOME = {  # код: (подпись, цвет)
 "tundra": ("Тундра и арктика", "#dfe9ef"), "taiga": ("Тайга (северный хвойный лес)", "#5f8a6a"),
 "temperate": ("Умеренный пояс: пашни и леса", "#cfe0a8"), "med": ("Средиземноморье: сады, виноградники, маквис", "#e8d48a"),
 "steppe": ("Степь", "#e0cf8f"), "desert": ("Пустыня", "#ead6a6"), "mount": ("Горы", "#a08060"),
 "wetland": ("Болота", "#7fbcae"), "oldforest": ("Древние леса", "#3f7f3f"), "waste": ("Пустоши", "#8f8f7a")}
_PH = None
def phys():
    global _PH
    if _PH is None:
        _PH = {}
        for f in json.load(open(G + "/ne_10m_geography_regions_polys.geojson"))["features"]:
            g = f["geometry"]; polys = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
            _PH[f["properties"]["NAME"]] = [Path(np.array(p[0])) for p in polys]
    return _PH
def inside(name, x, y): return any(p.contains_point((x, y)) for p in phys()[name])
STEPPE_TUR = set("Konya Aksaray Karaman Nevsehir Kirsehir Ankara Eskisehir Yozgat Kinkkale Nigde Kayseri Sivas Çorum Çankiri Afyonkarahisar".split())
MED_COUNTRIES = {"ITA", "GRC", "ALB", "MNE", "CYP", "MLT", "LBN", "ISR", "PSX", "SMR", "VAT", "MCO"}
def biome(p):
    a, n, x, y = p["adm0_a3"], p["name"] or "", p["longitude"], p["latitude"]
    if y >= 66.5 or n in ("Murmansk", "Finnmark", "Troms", "Lapland", "Norrbotten", "Nenets"): return "tundra"
    for d in ("SAHARA", "SYRIAN DESERT", "LIBYAN DESERT", "Negev Desert", "WESTERN DESERT", "Eastern Desert", "An Nafud Desert"):
        if d in phys() and inside(d, x, y): return "desert"
    if a == "EGY" and n not in ("Al Qahirah", "Al Jizah", "Al Iskandariyah", "Ad Daqahliyah", "Al Buhayrah", "Al Gharbiyah", "Al Minufiyah",
                                "Al Qalyubiyah", "Ash Sharqiyah", "Kafr ash Shaykh", "Dumyat", "Bur Sa`id", "Al Isma`iliyah", "Al Fayyum",
                                "Bani Suwayf", "Al Minya", "Asyut", "Suhaj", "Qina", "Luxor", "Aswan"): return "desert"
    if y >= 58.5 or (a == "RUS" and y >= 56.5): return "taiga"
    if inside("PONTIC STEPPE", x, y) and y < 50 or inside("Alföld", x, y) or (a == "TUR" and n in STEPPE_TUR) or a in ("KAZ",): return "steppe"
    if a == "IRQ" or (a == "SYR" and x > 38): return "steppe"
    med = a in MED_COUNTRIES and y < 44.6
    if a == "ESP" and not (y > 42.7 and x < -1.5): med = True
    if a == "PRT" and y < 41.5: med = True
    if a == "FRA" and y < 44.2 and x > 2.5: med = True
    if a in ("HRV", "BIH") and x < 18.5 and y < 44.2: med = True
    if a == "TUR" and (x < 30 and y < 40.5 or y < 37.3): med = True
    if a == "SYR" and x <= 38: med = True
    if a in ("MAR", "DZA", "TUN", "LBY") and y > 34.5: med = True
    if a == "BGR" and y < 42: med = True
    return "med" if med else "temperate"
