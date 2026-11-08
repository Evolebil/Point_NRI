"""Чтение дорог Itiner-e (de Soto и др., 2025, CC BY 4.0) из GeoPackage в долготу/широту."""
import math, os, sqlite3, struct
HERE = os.path.dirname(os.path.abspath(__file__))
A, E = 6378137.0, 0.0818191908426  # эллипсоид WGS84; проекция EPSG:3395 (World Mercator)
def inv3395(x, y):
    lon = math.degrees(x / A); t = math.exp(-y / A); phi = math.pi / 2 - 2 * math.atan(t)
    for _ in range(6):
        es = E * math.sin(phi)
        phi = math.pi / 2 - 2 * math.atan(t * ((1 - es) / (1 + es)) ** (E / 2))
    return lon, math.degrees(phi)
def _wkb(b, o, out):
    le = b[o] == 1; f = "<" if le else ">"
    typ = struct.unpack_from(f + "I", b, o + 1)[0]; o += 5
    dim = 2 + (1 if (typ // 1000) in (1, 2) else 0) + (1 if (typ // 1000) == 3 else 0) * 2
    base = typ % 1000
    if base == 2:
        n = struct.unpack_from(f + "I", b, o)[0]; o += 4
        pts = []
        for _ in range(n):
            x, y = struct.unpack_from(f + "dd", b, o); o += 8 * dim; pts.append(inv3395(x, y))
        out.append(pts); return o
    if base == 5:
        n = struct.unpack_from(f + "I", b, o)[0]; o += 4
        for _ in range(n): o = _wkb(b, o, out)
        return o
    raise ValueError(f"тип геометрии {typ}")
def roads():
    """[(тип дороги, название, [(lon, lat), ...]), ...]"""
    c = sqlite3.connect(f"file:{HERE}/geodata/itinere/itinere_roads.gpkg?mode=ro", uri=True)
    srs = c.execute("select srs_id from gpkg_contents").fetchone()[0]
    assert srs == 3395, srs
    res = []
    for geom, name, typ in c.execute("select geom, Name, Type from itinere_roads_simplify"):
        if not geom: continue
        flags = geom[3]; env = {0: 0, 1: 32, 2: 48, 3: 48, 4: 64}[(flags >> 1) & 7]
        out = []; _wkb(geom, 8 + env, out)
        for pts in out: res.append((typ, name, pts))
    return res
if __name__ == "__main__":
    import collections
    r = roads(); print(len(r), collections.Counter(t for t, _, _ in r))
    print(r[0][1], r[0][2][:3])
