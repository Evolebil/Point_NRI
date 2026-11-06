"""Карта языков около 1208 года (по провинциям Natural Earth). Запуск: python3 langs.py выход.png
Распределение — историческая справка по памяти, упрощённо: в каждой провинции — язык большинства."""
import json, os, sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from states import rings, visible, frame, X0, X1, Y0, Y1, G
L = {  # язык: (подпись, семья, цвет)
 "oil": ("Французский (ойль)", "Романские", "#4a7bd0"), "oc": ("Окситанский", "Романские", "#7aa6e8"),
 "ita": ("Итальянские наречия", "Романские", "#9b59b6"), "sard": ("Сардинский", "Романские", "#c39bd3"),
 "cast": ("Кастильский", "Романские", "#2f8a8a"), "galpt": ("Галисийско-португальский", "Романские", "#3aa0a0"),
 "cat": ("Каталанский", "Романские", "#e0a23a"), "astleo": ("Астурлеонский", "Романские", "#5bb6b6"),
 "vlah": ("Влашский (румынский)", "Романские", "#d98880"),
 "deu": ("Немецкий", "Германские", "#c9b458"), "nld": ("Нидерландский (фламандский)", "Германские", "#e3d27a"),
 "eng": ("Английский (знать — по-французски)", "Германские", "#d04a4a"), "scots": ("Северный английский (скотс)", "Германские", "#e08a8a"),
 "nord": ("Скандинавские", "Германские", "#5a7ab0"),
 "gael": ("Гэльский", "Кельтские", "#6aa07a"), "wels": ("Валлийский", "Кельтские", "#b9a06a"), "bret": ("Бретонский", "Кельтские", "#8fbf8f"),
 "eus": ("Баскский", "Отдельный", "#7a5a3a"), "alb": ("Албанский", "Отдельный", "#a0522d"), "grc": ("Греческий", "Греческий", "#d4a017"),
 "slw": ("Западнославянские (польский, чешский, венды)", "Славянские", "#e0b0b0"), "sle": ("Древнерусский", "Славянские", "#80a0c0"),
 "sls": ("Южнославянские", "Славянские", "#c08060"), "bul": ("Болгарский", "Славянские", "#a05a3a"),
 "hun": ("Венгерский", "Угорские", "#5aa05a"), "fin": ("Финские (финский, эстонский)", "Финские", "#b0b8a0"), "sami": ("Саамский", "Финские", "#d0d8c0"),
 "balt": ("Балтские (литовский, латышский, прусский)", "Балтские", "#c0b0a0"),
 "turk": ("Тюркские (сельджуки, туркмены)", "Тюркские", "#a0a050"), "kipch": ("Кипчакский (половцы)", "Тюркские", "#c0c090"),
 "bulg": ("Волжско-булгарский", "Тюркские", "#a0b090"), "arm": ("Армянский", "Армянский", "#c070c0"),
 "kart": ("Грузинский", "Картвельские", "#d07080"), "kurd": ("Курдский", "Иранские", "#b09070"), "per": ("Персидский и азербайджанские наречия", "Иранские", "#909070"),
 "arab": ("Арабский", "Семитские", "#c8a878"), "andal": ("Арабский (андалусский)", "Семитские", "#7a9a4a"), "berb": ("Берберские", "Берберские", "#9ab060"),
}
DEF = {"FRA": "oil", "GBR": "eng", "IRL": "gael", "DEU": "deu", "AUT": "deu", "CHE": "deu", "NLD": "nld", "BEL": "nld", "LUX": "deu",
 "SVN": "sls", "CZE": "slw", "SVK": "slw", "POL": "slw", "ITA": "ita", "SMR": "ita", "VAT": "ita", "MLT": "arab", "PRT": "galpt", "ESP": "andal",
 "AND": "cat", "MCO": "oc", "DNK": "nord", "NOR": "nord", "SWE": "nord", "FIN": "fin", "EST": "fin", "LVA": "balt", "LTU": "balt",
 "HUN": "hun", "HRV": "sls", "BIH": "sls", "SRB": "sls", "KOS": "sls", "MNE": "sls", "MKD": "bul", "BGR": "bul", "ALB": "alb", "GRC": "grc",
 "ROU": "vlah", "MDA": "kipch", "UKR": "sle", "BLR": "sle", "RUS": "sle", "TUR": "turk", "CYP": "grc", "SYR": "arab", "LBN": "arab",
 "ISR": "arab", "PSX": "arab", "JOR": "arab", "EGY": "arab", "IRQ": "arab", "GEO": "kart", "ARM": "arm", "AZE": "per", "MAR": "berb",
 "DZA": "berb", "TUN": "arab", "LBY": "arab"}
OVR = {
 "FRA": {"oc": "Gironde Landes Pyrénées-Atlantiques Lot-et-Garonne Gers Hautes-Pyrénées Haute-Garonne Tarn Tarn-et-Garonne Ariège Aude Hérault Gard Lot Aveyron Lozère Dordogne Corrèze Haute-Vienne Creuse Cantal Puy-de-Dôme Haute-Loire Ardèche Drôme Vaucluse Bouches-du-Rhône Var Alpes-de-Haute-Provence Hautes-Alpes Alpes-Maritimes Charente",
         "bret": "Finistère Côtes-d'Armor Morbihan", "cat": "Pyrénées-Orientales", "ita": "Corse-du-Sud Haute-Corse",
         "deu": "Bas-Rhin Haute-Rhin Moselle", "nld": "Nord"},
 "BEL": {"oil": "Hainaut Liege Namur Luxembourg Walloon Brabant"},
 "CHE": {"oil": "Genève Vaud Neuchâtel Jura Fribourg Valais", "ita": "Ticino"},
 "ITA": {"sard": "Cagliari Carbonia-Iglesias Medio Campidano Nuoro Ogliastra Olbia-Tempio Oristrano Sassari"},
 "ESP": {"cast": "Burgos Palencia Valladolid Soria Segovia Ávila Madrid Toledo Guadalajara Cuenca Cantabria La Rioja",
         "astleo": "León Zamora Salamanca Asturias", "galpt": "La Coruña Lugo Orense Pontevedra", "eus": "Álava Gipuzkoa Bizkaia Navarra",
         "cat": "Lérida Barcelona Gerona Tarragona", "ces": ""},
 "PRT": {"andal": "Faro Beja Setúbal"},
 "GBR": {}, "DEU": {"slw": "Brandenburg Berlin Mecklenburg-Vorpommern Sachsen"},
 "SWE": {"sami": "Norrbotten Västerbotten"}, "NOR": {"sami": "Finnmark Troms"}, "FIN": {"sami": "Lapland"},
 "POL": {"balt": "Warmian-Masurian"},
 "ROU": {"hun": "Bihor Satu Mare Covasna Harghita Mures", "kipch": "Braila Galati Vaslui Iasi Botosani Bacau Vrancea Buzau Ialomita Calarasi Constanta Tulcea"},
 "SRB": {"hun": "Severno-Backi"},
 "TUR": {"grc": "Edirne Kirklareli Tekirdag Istanbul Kocaeli Yalova Sakarya Bursa Balikesir Çanakkale Izmir Manisa Aydin Mugla Antalya Trabzon Rize Giresun Ordu Samsun Sinop Zinguldak Bartın Düzce",
         "arm": "Adana Mersin Osmaniye Kars Ardahan Iğdir Agri Van Bitlis Mus Erzurum Erzincan Bingöl Tunceli Elazig Malatya",
         "kurd": "Diyarbakir Batman Siirt Sirnak Hakkari Mardin", "arab": "Sanliurfa Kilis Gaziantep Hatay", "kart": "Artvin"},
 "IRQ": {"kurd": "Dihok Arbil As-Sulaymaniyah"}, "AZE": {}, "MAR": {"arab": "Grand Casablanca Rabat - Salé - Zemmour - Zaer Fès - Boulemane"},
 "DZA": {"arab": "Alger Oran Annaba Constantine Tlemcen"},
 "RUS": {"balt": "Kaliningrad", "kipch": "Krasnodar Rostov Stavropol' Adygey Kalmyk Volgograd Astrakhan' Crimea Sevastopol",
         "bulg": "Tatarstan Chuvash Ul'yanovsk Samara Mariy-El Udmurt Bashkortostan",
         "fin": "Kareliya Leningrad Murmansk Arkhangel'sk Komi Mordovia Vologda"},
 "UKR": {"kipch": "Odessa Mykolayiv Kherson Zaporizhzhya Donets'k Luhans'k Dnipropetrovs'k Kharkiv Kirovohrad"},
}
def lang(p):
    a, n = p["adm0_a3"], p["name"] or ""
    if a == "GBR":
        r = p["region"] or ""
        if "Wales" in r: return "wels"
        if r == "Highlands and Islands": return "gael"
        if r == "Northern Ireland": return "gael"
        if r in ("Eastern", "North Eastern", "South Western"): return "scots"
        if n == "Cornwall": return "wels"
        return "eng"
    for k, names in OVR.get(a, {}).items():
        if n and (" " + n + " ") in (" " + names + " "): return k
    return DEF.get(a)
if __name__ == "__main__":
    fig, ax = plt.subplots(figsize=(22, 15)); ax.set_facecolor("#cfe3f0")
    for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc="#e6e1d6", ec="none", zorder=1))
    cent = {}
    for f in json.load(open(G + "/ne_10m_admin_1.geojson"))["features"]:
        p = f["properties"]; k = lang(p)
        if k is None: continue
        col = L[k][2]
        for r in rings(f["geometry"]):
            if visible(r): ax.add_patch(Polygon(r, fc=col, ec=col, lw=.4, zorder=2))
        if X0 < p["longitude"] < X1 and Y0 < p["latitude"] < Y1:
            w = p["area_sqkm"] or 1; c = cent.setdefault(k, [0, 0, 0]); c[0] += p["longitude"] * w; c[1] += p["latitude"] * w; c[2] += w
    for fn in ("ne_50m_lakes", "ne_50m_land"):
        for f in json.load(open(G + f"/{fn}.geojson"))["features"]:
            for r in rings(f["geometry"]):
                if visible(r): ax.add_patch(Polygon(r, fc="#cfe3f0" if fn.endswith("lakes") else "none", ec="#5a6a79", lw=.6, zorder=3))
    POS = {"arab": [(30.5, 28.8), (38.0, 34.8), (44.0, 31.5), (12.0, 31.0)], "grc": [(22.0, 39.6), (28.0, 38.6)],
           "galpt": [(-8.0, 41.2)], "bulg": [(48.0, 55.0)], "kipch": [(36.0, 47.5)]}
    for k, (sx, sy, w) in cent.items():
      for x, y in POS.get(k, [(sx / w, sy / w)]):
        ax.text(x, y, L[k][0], fontsize=8.5, ha="center", va="center", zorder=6,
                bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none", alpha=.6))
    fams = {}
    for k, (name, fam, col) in L.items(): fams.setdefault(fam, []).append(col)
    for fam, cols in fams.items(): ax.plot([], [], "s", ms=12, mfc=cols[0], mec="#333", label=fam)
    ax.legend(loc="upper left", bbox_to_anchor=(1.005, 1), fontsize=9.5, framealpha=.92, title="Семьи языков")
    frame(ax, "Языки около 1208 года — мир кампании «Аномалии планов»",
          "Упрощённо: каждой современной провинции дан язык большинства около 1208 года. Справка по памяти, по источникам не сверена.\n"
          "Знать Англии, Ирландии и крестоносных государств говорит по-французски; латынь — язык церкви и учёных везде на Западе, греческий — на Востоке.")
    plt.savefig(sys.argv[1], dpi=110, bbox_inches="tight")
