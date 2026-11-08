"""Города около 1200 года: Европа — Buringh (2021, CC0), за пределами Европы — Чандлер (Reba и др. 2016, CC BY 4.0).
build() -> [(жителей, название, долгота, широта, источник)]"""
import csv, json, os
import numpy as np
from matplotlib.path import Path
HERE = os.path.dirname(os.path.abspath(__file__))
RU = {"Istanbul": "Константинополь", "Sevilla": "Севилья", "Cordoba": "Кордова", "Milano": "Милан", "Venezia": "Венеция", "Paris": "Париж",
 "Smolensk": "Смоленск", "London": "Лондон", "Leon": "Леон", "Koeln": "Кёльн", "Napoli": "Неаполь", "Toledo": "Толедо", "Bologna": "Болонья",
 "Saloniki": "Фессалоники", "Roma": "Рим", "Pavia": "Павия", "Palermo": "Палермо", "Novgorod": "Новгород", "Genova": "Генуя", "Gent": "Гент",
 "Cernigov": "Чернигов", "Verona": "Верона", "Barcelona": "Барселона", "Bolgary": "Булгар", "Athens": "Афины", "Viterbo": "Витербо",
 "Toulouse": "Тулуза", "Padova": "Падуя", "Kiev": "Киев", "Granada": "Гранада", "Burgos": "Бургос", "Brescia": "Брешиа", "Arras": "Аррас",
 "Valencia": "Валенсия", "Messina": "Мессина", "Almeria": "Альмерия", "Wien": "Вена", "Tournai": "Турне", "Pisa": "Пиза", "Jaen": "Хаэн",
 "Cartagena": "Картахена", "Valladolid": "Вальядолид", "Regensburg": "Регенсбург", "Merida": "Мерида", "Magdeburg": "Магдебург",
 "Sudak": "Сурож", "Rouen": "Руан", "Mantua": "Мантуя", "Lisboa": "Лиссабон", "Jerez de la Frontera": "Херес", "Firenze": "Флоренция",
 "Erfurt": "Эрфурт", "Bordeaux": "Бордо", "Piacenza": "Пьяченца", "Belgrade": "Белград", "Trier": "Трир", "Montpellier": "Монпелье",
 "Augsburg": "Аугсбург", "Aachen": "Ахен", "Worms": "Вормс", "Tours": "Тур", "Soest": "Зост", "Siena": "Сиена", "Rostov": "Ростов",
 "Ravenna": "Равенна", "Palma": "Пальма", "Marseille": "Марсель", "Lille": "Лилль", "Ferrara": "Феррара", "Feodosija": "Кафа",
 "Chalons-sur-Marne": "Шалон", "Bourges": "Бурж", "Zaragoza": "Сарагоса", "York": "Йорк", "Salamanca": "Саламанка", "Reims": "Реймс",
 "Poitiers": "Пуатье", "Plovdiv": "Филиппополь", "Pistoia": "Пистойя", "Parma": "Парма", "Orleans": "Орлеан", "Murcia": "Мурсия",
 "Metz": "Мец", "Luebeck": "Любек", "Derbent": "Дербент", "Cremona": "Кремона", "Braunschweig": "Брауншвейг", "Badajoz": "Бадахос",
 "Velez Malaga": "Велес-Малага", "Trapani": "Трапани", "Strasbourg": "Страсбург", "St Omer": "Сент-Омер", "Reggio nell Emilia": "Реджо",
 "Praha": "Прага", "Perugia": "Перуджа", "Osnabrueck": "Оснабрюк", "Norwich": "Норидж", "Nicosia": "Никосия", "Muenster": "Мюнстер",
 "Modena": "Модена", "Lyon": "Лион", "Lucca": "Лукка", "Krakow": "Краков", "Forli": "Форли", "Ecija": "Эсиха", "Dijon": "Дижон",
 "Brugge": "Брюгге", "Bergamo": "Бергамо", "Belgorod": "Белгород", "Barletta": "Барлетта", "Auxerre": "Осер", "Alessandria": "Алессандрия",
 "Vladimir": "Владимир", "Tortosa": "Тортоса", "Sliven": "Сливен", "Malaga": "Малага", "Mainz": "Майнц", "Faenza": "Фаэнца",
 "Evora": "Эвора", "Dublin": "Дублин", "Corleone": "Корлеоне", "Como": "Комо", "Baeza": "Баэса", "Veliko Turnovo": "Тырново",
 "Tarragona": "Таррагона", "Prizrend": "Призрен", "Lueneburg": "Люнебург", "Lucera": "Лучера", "Lincoln": "Линкольн", "Liege": "Льеж",
 "Lerida": "Лерида", "Halberstadt": "Хальберштадт", "Freiburg": "Фрайбург", "Douai": "Дуэ", "Cesena": "Чезена", "Carcassonne": "Каркассон",
 "Bruxelles": "Брюссель", "Bremen": "Бремен", "Bamberg": "Бамберг", "Arezzo": "Ареццо",
 # за пределами Европы (Чандлер)
 "Fez": "Фес", "Cairo": "Каир", "Marrakech": "Марракеш", "Dumyat": "Дамиетта", "Baghdad": "Багдад", "Damascus": "Дамаск", "Tbilisi": "Тбилиси",
 "Konya": "Конья", "Rabat": "Рабат", "Mosul": "Мосул", "Bejaia": "Беджая", "Basra": "Басра", "Alexandria": "Александрия", "Sivas": "Сивас",
 "Aleppo": "Алеппо", "Kish": "Киш", "Kayseri": "Кайсери", "Ceuta": "Сеута", "Acre": "Акра", "Asyut": "Асьют"}
NON_EUROPE = {"Morocco", "Egypt", "Iraq", "Syria", "Georgia", "Israel", "Iran", "Algeria", "Tunisia", "Libya", "Lebanon"}
A3 = {"Kosovo": "KOS", "Bulgaria": "BGR", "Belarus": "BLR", "Croatia": "HRV", "Cyprus": "CYP", "Andorra": "AND", "Czech rep.": "CZE",
 "Denmark": "DNK", "Estonia": "EST", "Finland": "FIN", "France": "FRA", "Germany": "DEU", "Greece": "GRC", "Hungary": "HUN", "Iceland": "ISL",
 "Ireland": "IRL", "UK": "GBR", "Italy": "ITA", "Austria": "AUT", "Latvia": "LVA", "Lithuania": "LTU", "Luxemburg": "LUX", "Malta": "MLT",
 "Moldova": "MDA", "Montenegro": "MNE", "Netherlands": "NLD", "Belgium": "BEL", "Norway": "NOR", "Poland": "POL", "Portugal": "PRT",
 "Romenia": "ROU", "Russia": "RUS", "Serbia": "SRB", "Bosnia Herzegovina": "BIH", "Slovakia": "SVK", "Slovenia": "SVN", "Spain": "ESP",
 "Sweden": "SWE", "Switzerland": "CHE", "Turkey": "TUR", "Albania": "ALB", "Ukraine": "UKR", "North Macedonia": "MKD"}
_COUNTRY = None
def _country_paths():
    global _COUNTRY
    if _COUNTRY is None:
        _COUNTRY = {}
        for f in json.load(open(HERE + "/geodata/ne_10m_admin_1.geojson"))["features"]:
            g = f["geometry"]; a = f["properties"]["adm0_a3"]
            for poly in ([g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]):
                _COUNTRY.setdefault(a, []).append(Path(np.array(poly[0])))
    return _COUNTRY
def _inside(a3, lon, lat):
    pts = [(lon + dx, lat + dy) for dx in (-.35, 0, .35) for dy in (-.25, 0, .25)]  # у моря точка может стоять чуть за берегом
    return any(p.contains_point(q) for p in _country_paths().get(a3, []) for q in pts)
MANUAL = {"Quimper": (-4.1, 48.0), "Coutances": (-1.43, 49.05), "Perpignan": (2.88, 42.7), "Minsk": (27.57, 53.9),
          "Carrickfergus": (-5.81, 54.72), "Downpatrick": (-5.72, 54.33), "Atessa": (14.45, 42.07), "Cervinara": (14.62, 41.02), "Gibraltar": (-5.35, 36.14)}  # ошибки Buringh, найденные на карте
FIXES = []  # что поправлено в координатах Buringh: (город, было, стало)
def _scale(v, lim):
    """варианты числа с потерянной запятой: v, v/10, v/100, ... в допустимых пределах"""
    return [v / 10 ** k for k in range(0, 18) if abs(v / 10 ** k) <= lim and (k == 0 or abs(v / 10 ** k) >= 1)]
def check(name, country, lon, lat):
    """Сверка координат со страной: пропущенную запятую и ошибку знака долготы чиним, безнадёжные точки выбрасываем (None)."""
    if name in MANUAL: FIXES.append((name, (lon, lat), MANUAL[name])); return MANUAL[name]
    a3 = A3.get(country)
    if a3 is None or a3 not in _country_paths(): return lon, lat
    ok = (lambda x, y: _inside(a3, x, y) or _inside("RUS", x, y)) if a3 == "UKR" else (lambda x, y: _inside(a3, x, y))  # Крым в данных — у RUS
    if abs(lon) <= 180 and abs(lat) <= 90 and ok(lon, lat): return lon, lat
    for x0 in _scale(lon, 180):
        for y in _scale(lat, 90):
            for x in (x0, -x0):
                if ok(x, y): FIXES.append((name, (lon, lat), (round(x, 3), round(y, 3)))); return x, y
    FIXES.append((name, (lon, lat), None)); return None
def build():
    out = []
    rows = list(csv.reader(open(HERE + "/geodata/buringh/buringh.tab", encoding="utf-8"), delimiter="\t"))
    for r in rows[1:]:
        if r[8] != "1200.0" or r[9] in ("", "0.0"): continue
        ll = check(r[0], r[3], float(r[6].replace(",", ".")), float(r[5].replace(",", ".")))
        if ll: out.append((int(float(r[9]) * 1000), RU.get(r[0], r[0]), ll[0], ll[1], "Buringh"))
    rows = list(csv.reader(open(HERE + "/geodata/chandlerV2.csv", encoding="latin-1")))
    i = rows[0].index("AD_1200")
    for r in rows[1:]:
        if r[i] and (r[2] in NON_EUROPE or (r[2] in ("Turkey", "Spain") and r[0] in ("Konya", "Sivas", "Kayseri", "Ceuta"))):
            out.append((int(float(r[i])), RU.get(r[0], r[0]), float(r[4]), float(r[3]), "Чандлер"))
    out.sort(reverse=True)
    return out
if __name__ == "__main__":
    c = build(); print(len(c)); print("поправлено/выброшено:", len(FIXES)); [print(x) for x in FIXES]; print([x[:2] for x in c[:25]])
    print("без русского имени среди 8000+:", [x[1] for x in c if x[0] >= 8000 and not any("а" <= ch <= "я" for ch in x[1].lower())])
