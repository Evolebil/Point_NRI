# Распределение современных провинций Natural Earth по государствам 1208 года (мир кампании)
R = {  # код: (подпись, цвет)
 "FRA": ("Королевство Франция", "#4a7bd0"), "FRA_V": ("Великие вассалы Франции\n(Шампань, Бургундия, Бретань)", "#8fb0e6"),
 "FLA": ("Графство Фландрия\n(вассал Франции)", "#9fc0f0"), "TOU": ("Графство Тулузское\n(вассал Франции)", "#7fa3dc"),
 "ENG": ("Королевство Англия", "#d04a4a"), "GAS": ("Гасконь\n(Англия)", "#d04a4a"), "IRL_E": ("Ирландия\n(владение Англии)", "#e08a8a"),
 "WAL": ("Уэльские княжества", "#b9a06a"), "SCO": ("Королевство Шотландия", "#6aa07a"), "GAEL": ("Гэльские королевства\nИрландии", "#9ab88a"),
 "GER": ("Священная Римская Империя\n(фактически германские герцогства)", "#c9b458"), "ARL": ("Арелат: Бургундия, Прованс, Савойя (в составе СРИ)", "#dccb80"),
 "BOH": ("Королевство Богемия\n(в составе СРИ)", "#b89b3a"), "ITA": ("Итальянская Империя", "#9b59b6"),
 "POR": ("Королевство Португалия", "#3aa0a0"), "LEO": ("Королевство Леон", "#5bb6b6"), "CAS": ("Королевство Кастилия", "#2f8a8a"),
 "NAV": ("Королевство Наварра", "#8fd0d0"), "ARA": ("Арагонская корона", "#e0a23a"), "ALM": ("Альмохадский халифат", "#7a9a4a"),
 "DEN": ("Королевство Дания", "#e07a5a"), "NOR": ("Королевство Норвегия", "#c06a8a"), "SWE": ("Королевство Швеция", "#5a7ab0"),
 "POL": ("Польские княжества", "#e0c0c0"), "PRU": ("Пруссы", "#c0b0a0"), "BAL": ("Балтийские и финские племена", "#b0b8a0"),
 "HUN": ("Королевство Венгрия", "#5aa05a"), "BOS": ("Боснийский банат\n(вассал Венгрии)", "#8ac08a"),
 "SER": ("Великое княжество Сербия", "#c08060"), "BUL": ("Болгарское царство", "#a05a3a"),
 "BYZ": ("Византийская империя", "#d4a017"), "RUM": ("Румский султанат", "#a0a050"), "ARM": ("Киликийская Армения", "#c070c0"),
 "ANT": ("Княжество Антиохия", "#e05050"), "TRI": ("Графство Триполи", "#e06060"), "JER": ("Королевство Иерусалим (Акра)", "#e07070"),
 "CYP": ("Королевство Кипр", "#e08080"), "AYY": ("Айюбиды", "#c8a878"), "ABB": ("Аббасидский халифат", "#a08060"),
 "MOS": ("Атабеки Мосула и Джазиры", "#b09070"), "GEO": ("Королевство Грузия", "#d07080"), "ILD": ("Ильдегизиды", "#909070"),
 "RUS": ("Русские княжества", "#80a0c0"), "CUM": ("Половцы (куманы)", "#c0c090"), "VBU": ("Волжская Булгария", "#a0b090"),
}
DEFAULT = {"FRA": "FRA", "GBR": "ENG", "IRL": "GAEL", "DEU": "GER", "AUT": "GER", "CHE": "ITA", "NLD": "GER", "BEL": "GER", "LUX": "GER",
 "SVN": "ITA", "CZE": "BOH", "ITA": "ITA", "SMR": "ITA", "VAT": "ITA", "MLT": "ITA", "PRT": "POR", "ESP": "CAS", "AND": "ARA", "MCO": "ARL",
 "DNK": "DEN", "NOR": "NOR", "SWE": "SWE", "FIN": "BAL", "EST": "BAL", "LVA": "BAL", "LTU": "BAL", "POL": "POL", "HUN": "HUN", "SVK": "HUN",
 "HRV": "ITA", "BIH": "ITA", "SRB": "ITA", "KOS": "ITA", "MNE": "ITA", "MKD": "ITA", "BGR": "BUL", "ALB": "BYZ", "GRC": "BYZ", "ROU": "CUM",
 "MDA": "CUM", "UKR": "RUS", "BLR": "RUS", "RUS": "RUS", "TUR": "RUM", "CYP": "CYP", "SYR": "AYY", "LBN": "AYY", "ISR": "AYY", "PSX": "AYY",
 "JOR": "AYY", "EGY": "AYY", "IRQ": "ABB", "GEO": "GEO", "ARM": "GEO", "AZE": "ILD", "MAR": "ALM", "DZA": "ALM", "TUN": "ALM", "LBY": "ALM"}
OVR = {
 "FRA": {"TOU": "Haute-Garonne Tarn Tarn-et-Garonne Ariège Aude Hérault Gard Lot Aveyron Lozère",
         "GAS": "Gironde Landes Pyrénées-Atlantiques Lot-et-Garonne Gers Hautes-Pyrénées",
         "FRA_V": "Marne Aube Haute-Marne Ardennes Côte-d'Or Saône-et-Loire Nièvre Finistère Côtes-d'Armor Morbihan Ille-et-Vilaine Loire-Atlantique",
         "FLA": "Nord", "ARA": "Pyrénées-Orientales", "ITA": "Corse-du-Sud Haute-Corse",
         "ARL": "Doubs Jura Haute-Saône Territoire de Belfort Ain Savoie Haute-Savoie Isère Drôme Hautes-Alpes Vaucluse Bouches-du-Rhône Var Alpes-de-Haute-Provence Alpes-Maritimes Rhône Loire",
         "GER": "Bas-Rhin Haute-Rhin Moselle Meurthe-et-Moselle Meuse Vosges"},
 "BEL": {"FLA": "West Flanders East Flanders"},
 "AUT": {"ITA": "Tirol Vorarlberg Kärnten"},
 "ESP": {"LEO": "León Zamora Salamanca Asturias La Coruña Lugo Orense Pontevedra", "NAV": "Navarra",
         "ARA": "Huesca Zaragoza Teruel Lérida Barcelona Gerona Tarragona",
         "ALM": "Sevilla Cádiz Huelva Córdoba Jaén Granada Málaga Almería Murcia Valencia Alicante Castellón Badajoz Cáceres Ciudad Real Albacete Baleares Ceuta Melilla"},
 "PRT": {"ALM": "Faro Beja Setúbal"},
 "GBR": {}, "IRL": {"IRL_E": "Dublin Fingal South Dublin Dún Laoghaire–Rathdown Wicklow Wexford Kildare Carlow Kilkenny Meath Louth Waterford South Tipperary North Tipperary Limerick Cork Westmeath Laoighis Offaly"},
 "SWE": {"DEN": "Skåne Blekinge Halland"},
 "POL": {"PRU": "Warmian-Masurian"},
 "ROU": {"HUN": "Alba Arad Bihor Bistrita-Nasaud Brasov Caras-Severin Cluj Covasna Harghita Hunedoara Maramures Mures Salaj Satu Mare Sibiu Timis",
         "BUL": "Constanta Tulcea"},
 "GRC": {"ITA": "Ionioi Nisoi"},
 "TUR": {"BYZ": "Edirne Kirklareli Tekirdag Istanbul Kocaeli Yalova Sakarya Bursa Bilecik Balikesir Çanakkale Izmir Manisa Aydin Mugla Denizli Usak Kütahya Antalya Burdur Isparta Afyonkarahisar Düzce Bolu Zinguldak Bartın Karabük Sinop Samsun Ordu Giresun Trabzon Rize Gümüshane Artvin",
         "ARM": "Adana Mersin Osmaniye", "ANT": "Hatay", "GEO": "Ardahan Kars", "AYY": "Diyarbakir Mardin Sanliurfa Batman Siirt Gaziantep Kilis Adiyaman"},
 "SYR": {"TRI": "Tartus", "MOS": "Hasaka (Al Haksa) Ar Raqqah Dayr Az Zawr"},
 "LBN": {"TRI": "North Lebanon", "JER": "Beirut Mount Lebanon South Lebanon"},
 "ISR": {"JER": "HaZafon Haifa Tel Aviv HaMerkaz"},
 "IRQ": {"MOS": "Ninawa Dihok Arbil At-Ta'mim Sala ad-Din"},
 "LBY": {"AYY": "Al Butnan Al Jabal al Akhdar Al Marj Al Qubbah Benghazi Ajdabiya"},
 "RUS": {"PRU": "Kaliningrad", "CUM": "Krasnodar Rostov Stavropol' Adygey Crimea Sevastopol Kalmyk Volgograd Astrakhan' Karachay-Cherkess Kabardin-Balkar North Ossetia Ingush Chechnya Dagestan",
         "VBU": "Tatarstan Chuvash Ul'yanovsk Samara Mariy-El Udmurt Bashkortostan"},
 "UKR": {"CUM": "Odessa Mykolayiv Kherson Zaporizhzhya Donets'k Luhans'k Dnipropetrovs'k Kharkiv Kirovohrad"},
}
def realm(p):
    a, n = p["adm0_a3"], p["name"] or ""
    if a == "GBR":
        r = p["region"] or ""
        if r == "Northern Ireland":
            return "IRL_E" if n in "Antrim Down Ards North Down Belfast Carrickfergus Larne Newtownabbey Castlereagh Lisburn".split() else "GAEL"
        if "Wales" in r: return "WAL"
        if r in ("Eastern", "North Eastern", "Highlands and Islands", "South Western"): return "SCO"
        return "ENG"
    for k, names in OVR.get(a, {}).items():
        if n and (" " + n + " ") in (" " + names + " "): return k
    return DEFAULT.get(a)
