#!/usr/bin/env python3
"""Собирает мир Европы 1208 года из локальных геоданных кампании.

Результат — gzip JSON, который C++-движок кампании читает как ActWorld.
Одна ячейка около 1 км; города, дороги, реки, биомы и подписи берутся из
kampaniya/karty и геоданных Natural Earth / Itiner-e.
"""
import gzip
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K = os.path.join(ROOT, "kampaniya", "karty")
sys.path.insert(0, K)

import biomes  # noqa: E402
import races  # noqa: E402
import states  # noqa: E402
from cities_data import build as build_cities  # noqa: E402
from itinere import roads as itiner_e_roads  # noqa: E402

G = states.G
X0, X1, Y0, Y1 = -12.0, 65.0, 27.0, 72.0
DLAT = 0.01
DLON = DLAT / math.cos(math.radians((Y0 + Y1) * 0.5))
W, H = round((X1 - X0) / DLON), round((Y1 - Y0) / DLAT)

FIELDS, FOREST, HILLS, MOUNT, SWAMP, RIVER, TOWN, WATER, DESERT, SNOW = range(10)
BIOME_CODES = {name: i for i, name in enumerate(("tundra", "taiga", "temperate", "med", "steppe", "desert", "mount", "wetland", "oldforest", "waste"))}
BIOME_TERRAIN = {"tundra": SNOW, "taiga": FOREST, "temperate": FIELDS, "med": FIELDS,
                 "steppe": FIELDS, "desert": DESERT, "mount": MOUNT, "wetland": SWAMP,
                 "oldforest": FOREST, "waste": HILLS}
REALM_KEYS = list(states.R)
REALM_CODES = {name: i + 1 for i, name in enumerate(REALM_KEYS)}
REALM_SYMBOLS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
PEOPLE_SYMBOLS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"


def xy(point):
    return (round((point[0] - X0) / DLON), round((Y1 - point[1]) / DLAT))


def line_parts(geometry):
    if not geometry:
        return
    kind, coords = geometry["type"], geometry["coordinates"]
    if kind == "LineString":
        yield coords
    elif kind == "MultiLineString":
        yield from coords


def polygon_parts(geometry):
    if not geometry:
        return
    kind, coords = geometry["type"], geometry["coordinates"]
    if kind == "Polygon":
        yield coords
    elif kind == "MultiPolygon":
        yield from coords


def paint_polygons(draw, geometry, fill, hole_fill=None):
    for polygon in polygon_parts(geometry):
        if not polygon:
            continue
        draw.polygon([xy(p) for p in polygon[0]], fill=fill)
        if hole_fill is not None:
            for ring in polygon[1:]:
                draw.polygon([xy(p) for p in ring], fill=hole_fill)


def geojson(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)["features"]


def encode(image):
    raw = np.frombuffer(image.tobytes(), dtype=np.uint8)
    return (raw + ord("0")).astype(np.uint8).tobytes().decode("ascii")


def encode_realm(image):
    table = bytearray([ord("0")] * 256)
    for i, symbol in enumerate(REALM_SYMBOLS[:len(REALM_KEYS)], 1):
        table[i] = ord(symbol)
    return image.tobytes().translate(bytes(table)).decode("ascii")


def encode_people(image):
    table = bytearray([ord("0")] * 256)
    for i, symbol in enumerate(PEOPLE_SYMBOLS, 1):
        table[i] = ord(symbol)
    return image.tobytes().translate(bytes(table)).decode("ascii")


def main(out):
    terrain = Image.new("L", (W, H), WATER)
    biome = Image.new("L", (W, H), BIOME_CODES["temperate"])
    cover = Image.new("L", (W, H), 5)  # единый cover-код: поля/луга
    relief = Image.new("L", (W, H), 0) # 0 равнина, 1 холмы, 2 плато, 3 горы, 4 высокие горы, 5 ущелья
    realm = Image.new("L", (W, H), 0)
    people = Image.new("L", (W, H), 0)
    draw_terrain, draw_biome = ImageDraw.Draw(terrain), ImageDraw.Draw(biome)
    draw_cover, draw_relief, draw_realm, draw_people = ImageDraw.Draw(cover), ImageDraw.Draw(relief), ImageDraw.Draw(realm), ImageDraw.Draw(people)

    for feature in geojson(os.path.join(G, "ne_50m_land.geojson")):
        paint_polygons(draw_terrain, feature["geometry"], FIELDS, WATER)

    # Биомы и государства привязаны к исторической разметке провинций.
    for feature in states._provinces():
        props = feature["properties"]
        name = biomes.biome(props)
        code = BIOME_CODES[name]
        paint_polygons(draw_biome, feature["geometry"], code)
        paint_polygons(draw_terrain, feature["geometry"], BIOME_TERRAIN[name])
        cover_code = {"tundra": 1, "taiga": 2, "temperate": 5, "med": 5, "steppe": 4,
                      "desert": 7, "mount": 5, "wetland": 9, "oldforest": 3, "waste": 4}[name]
        paint_polygons(draw_cover, feature["geometry"], cover_code)

    for name, ring in states.province_polys():
        draw_realm.polygon([xy(p) for p in ring], fill=REALM_CODES[name])

    # Крупные физические районы и авторские области из карты рас.
    physical = {f["properties"].get("NAME"): f["geometry"] for f in geojson(os.path.join(G, "ne_10m_geography_regions_polys.geojson"))}
    for name in races.MOUNT:
        geom = physical.get(name)
        if geom:
            paint_polygons(draw_terrain, geom, MOUNT)
            paint_polygons(draw_biome, geom, BIOME_CODES["mount"])
            paint_polygons(draw_relief, geom, 3)

    # Broad, cartographic land cover: Europe has no forest-cover source in this
    # bundle, so use a deterministic low-frequency field within temperate land.
    # It creates coherent woodland belts instead of tinting whole countries.
    rng = np.random.default_rng(1208)
    coarse = Image.fromarray(rng.integers(0, 256, (180, 160), dtype=np.uint8)).resize((W, H), Image.Resampling.BICUBIC)
    forest_field = np.asarray(coarse, dtype=np.uint8)
    terr_arr = np.frombuffer(terrain.tobytes(), dtype=np.uint8).copy().reshape(H, W)
    bio_arr = np.frombuffer(biome.tobytes(), dtype=np.uint8).reshape(H, W)
    cover_arr = np.frombuffer(cover.tobytes(), dtype=np.uint8).copy().reshape(H, W)
    relief_arr = np.frombuffer(relief.tobytes(), dtype=np.uint8).copy().reshape(H, W)
    wooded = (terr_arr == FIELDS) & np.isin(bio_arr, [BIOME_CODES["temperate"], BIOME_CODES["taiga"]]) & (forest_field > 178)
    terr_arr[wooded] = FOREST
    cover_arr[wooded] = 3
    terrain = Image.fromarray(terr_arr, "L")
    cover = Image.fromarray(cover_arr, "L")
    draw_terrain = ImageDraw.Draw(terrain)

    # Named European uplands and plateaus add a distinct hill belt beside the
    # mountain ranges; this remains physical terrain, not a race overlay.
    uplands = {"Volga Upland", "Dnieper Upland", "Azov Upland", "Stavropol’ Plateau", "Central Russian Upland",
               "NORTHERN EUROPEAN PLAIN", "Khomas Highland", "Jos Plateau"}
    for name in uplands:
        geom = physical.get(name)
        if geom:
            paint_polygons(draw_terrain, geom, HILLS)
            paint_polygons(ImageDraw.Draw(relief), geom, 1)

    # Расовые ареалы остаются отдельными подписями и не перекрашивают географию.

    # Озёра остаются непроходимой водой. Реки хранятся отдельным линейным слоем.
    for feature in geojson(os.path.join(G, "ne_50m_lakes.geojson")):
        paint_polygons(draw_terrain, feature["geometry"], WATER)

    road = Image.new("L", (W, H), 0)
    river = Image.new("L", (W, H), 0)
    draw_road, draw_river = ImageDraw.Draw(road), ImageDraw.Draw(river)
    for _kind, _name, points in itiner_e_roads():
        draw_road.line([xy(p) for p in points], fill=1, width=1)
    for feature in geojson(os.path.join(G, "ne_50m_rivers_lake_centerlines.geojson")):
        for points in line_parts(feature["geometry"]):
            draw_river.line([xy(p) for p in points], fill=1, width=1)

    towns = []
    for population, name, lon, lat, source in build_cities():
        if not (X0 < lon < X1 and Y0 < lat < Y1):
            continue
        x, y = xy((lon, lat))
        if 0 <= x < W and 0 <= y < H and terrain.getpixel((x, y)) != WATER:
            towns.append({"name": name, "lon": round(lon, 4), "lat": round(lat, 4), "pop": population, "source": source})
            if population >= 5000:
                terrain.putpixel((x, y), TOWN)

    # Существующие расовые подписи переносятся на интерактивную карту как отдельные места.
    labels = []
    people_meta = [{"id": i + 1, "name": label} for i, (_x, _y, _w, _h, label) in enumerate(
        item for rows in races.EXTRA.values() for item in rows)]
    if len(people_meta) > len(PEOPLE_SYMBOLS):
        raise ValueError("слишком много ареалов народов для слоя")
    pid = 0
    for category, ellipses in races.EXTRA.items():
        for lon, lat, width, height, label in ellipses:
            pid += 1
            x0, y0 = xy((lon - width / 2, lat + height / 2))
            x1, y1 = xy((lon + width / 2, lat - height / 2))
            draw_people.ellipse((x0, y0, x1, y1), fill=pid)
            if X0 < lon < X1 and Y0 < lat < Y1:
                labels.append({"text": label, "lon": lon, "lat": lat, "kind": "people", "peopleId": pid})
    for name, lon, lat in races.PEOPLE:
        if X0 < lon < X1 and Y0 < lat < Y1:
            pid += 1
            if pid > len(PEOPLE_SYMBOLS):
                raise ValueError("слишком много ареалов народов для слоя")
            x, y = xy((lon, lat))
            radius = max(2, round(0.65 / DLAT))
            draw_people.ellipse((x-radius, y-radius, x+radius, y+radius), fill=pid)
            people_meta.append({"id": pid, "name": name})
            labels.append({"text": name, "lon": lon, "lat": lat, "kind": "people", "peopleId": pid})

    centers = {}
    for name, lon, lat, area in states.province_centers():
        sx, sy, total = centers.setdefault(name, [0.0, 0.0, 0.0])
        centers[name] = [sx + lon * area, sy + lat * area, total + area]
    for name, (sx, sy, total) in centers.items():
        if total:
            labels.append({"text": states.R[name][0].replace("\n", " "), "lon": sx / total, "lat": sy / total, "kind": "realm"})

    # Дороги и реки поверх суши; из морских ячеек их удаляем.
    t = np.frombuffer(terrain.tobytes(), dtype=np.uint8)
    r = np.frombuffer(road.tobytes(), dtype=np.uint8).copy()
    v = np.frombuffer(river.tobytes(), dtype=np.uint8).copy()
    sea = t == WATER
    cover_arr = np.frombuffer(cover.tobytes(), dtype=np.uint8).copy().reshape(H, W)
    cover_arr[sea.reshape(H, W)] = 0
    cover = Image.fromarray(cover_arr, "L")
    r[sea] = 0
    v[sea] = 0
    road = Image.frombytes("L", (W, H), r.tobytes())
    river = Image.frombytes("L", (W, H), v.tobytes())

    data = {
        "name": "Европа и Средиземноморье 1208",
        "lon0": X0, "lat0": Y1, "dlon": DLON, "dlat": DLAT, "w": W, "h": H,
        "schema": 2, "terrain": encode(terrain), "cover": encode(cover), "relief": encode(relief),
        "road": encode(road), "river": encode(river), "biome": encode(biome), "people": encode_people(people),
        "realm": encode_realm(realm), "realmLayer": True, "peopleLayer": True, "peoples": people_meta,
        "towns": sorted(towns, key=lambda c: -c["pop"]), "labels": labels,
        "biomes": list(BIOME_CODES),
        "source": "Natural Earth, Itiner-e (CC BY 4.0), Buringh (CC0), Чандлер (CC BY 4.0); растительность — детерминированная схематизация, расы — отдельные подписи",
    }
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with gzip.open(out, "wt", encoding="utf-8", compresslevel=6) as f:
        json.dump(data, f, ensure_ascii=False, separators=(",", ":"))

    terrain_counts = np.bincount(t, minlength=10)
    biome_counts = np.bincount(np.frombuffer(biome.tobytes(), dtype=np.uint8), minlength=10)
    print(f"{out}: {W}×{H}, {os.path.getsize(out) // 1024} КБ gzip; суша {int((~sea).sum())}, "
          f"дороги {int(r.sum())}, реки {int(v.sum())}, государства {len(centers)}, города {len(towns)}, подписи {len(labels)}, "
          f"биомы {biome_counts.tolist()}, типы местности {terrain_counts.tolist()}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "web", "data", "region_europe.json.gz"))
