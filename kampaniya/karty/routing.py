"""Прокладка путей: по сети римских дорог (Itiner-e), по рекам (Natural Earth) и по морю (сетка, обход суши)."""
import heapq, json, math, os, sys
import numpy as np
from matplotlib.path import Path
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
G = HERE + "/geodata"
def hav(a, b):
    la1, la2 = math.radians(a[1]), math.radians(b[1]); dl = math.radians(b[0] - a[0])
    h = math.sin((la2 - la1) / 2) ** 2 + math.cos(la1) * math.cos(la2) * math.sin(dl / 2) ** 2
    return 12742 * math.asin(math.sqrt(h))
class Graph:
    def __init__(self, lines, snap=0.01):
        self.snap = snap; self.adj = {}; self.pos = {}
        for pts in lines:
            prev = None
            for p in pts:
                k = (round(p[0] / snap), round(p[1] / snap)); self.pos.setdefault(k, p)
                if prev is not None and prev != k:
                    d = hav(self.pos[prev], self.pos[k])
                    self.adj.setdefault(prev, []).append((k, d)); self.adj.setdefault(k, []).append((prev, d))
                prev = k
        self.keys = list(self.adj); self.arr = np.array([self.pos[k] for k in self.keys])
    def nearest(self, p):
        d = (self.arr[:, 0] - p[0]) ** 2 * math.cos(math.radians(p[1])) ** 2 + (self.arr[:, 1] - p[1]) ** 2
        i = int(d.argmin()); return self.keys[i], math.sqrt(d[i]) * 111
    def path(self, a, b):
        (s, ds), (t, dt) = self.nearest(a), self.nearest(b)
        dist = {s: 0}; prev = {}; pq = [(hav(self.pos[s], self.pos[t]), 0, s)]; tp = self.pos[t]
        while pq:
            _, d, u = heapq.heappop(pq)
            if u == t: break
            if d > dist.get(u, 1e18): continue
            for v, w in self.adj[u]:
                nd = d + w
                if nd < dist.get(v, 1e18):
                    dist[v] = nd; prev[v] = u; heapq.heappush(pq, (nd + hav(self.pos[v], tp), nd, v))
        if t not in dist: return None, None
        out = [t]
        while out[-1] != s: out.append(prev[out[-1]])
        return [a] + [self.pos[k] for k in reversed(out)] + [b], dist[t] + ds + dt
_roads = _rivers = None
def road_graph():
    global _roads
    if _roads is None:
        from itinere import roads
        _roads = Graph([pts for _, _, pts in roads()], snap=0.02)
    return _roads
def river_graph():
    global _rivers
    if _rivers is None:
        lines = []
        for f in json.load(open(G + "/ne_50m_rivers_lake_centerlines.geojson"))["features"]:
            g = f["geometry"]
            if not g: continue
            lines += [g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]
        _rivers = Graph(lines, snap=0.1)
    return _rivers
STRAITS = [((28.98, 40.98), (29.12, 41.25)),  # Босфор
           ((26.2, 40.0), (26.7, 40.42)),    # Дарданеллы
           ((12.6, 55.6), (12.6, 56.1)),     # Эресунн
           ((-5.8, 35.95), (-5.2, 36.0)),    # Гибралтар
           ((32.2, 46.55), (31.3, 46.5))]    # Днепровский лиман
class Sea:
    """Морской путь по сетке 0.2°: клетки суши запрещены, ход в 8 сторон."""
    def __init__(self, x0=-12, x1=50, y0=27, y1=64, step=0.2):
        self.x0, self.y0, self.step = x0, y0, step
        xs = np.arange(x0, x1, step) + step / 2; ys = np.arange(y0, y1, step) + step / 2
        self.nx, self.ny = len(xs), len(ys)
        X, Y = np.meshgrid(xs, ys); pts = np.c_[X.ravel(), Y.ravel()]
        land = np.zeros(len(pts), bool)
        for f in json.load(open(G + "/ne_50m_land.geojson"))["features"]:
            g = f["geometry"]; polys = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
            for poly in polys:
                r = np.array(poly[0])
                if r[:, 0].max() < x0 or r[:, 0].min() > x1 or r[:, 1].max() < y0 or r[:, 1].min() > y1: continue
                land |= Path(r).contains_points(pts)
        self.land = land.reshape(self.ny, self.nx)
        for a, b in STRAITS:  # узкие проливы на сетке 0.2° закрыты сушей — прорезаем вручную
            for i in range(41):
                t = i / 40; c = self.cell((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
                if 0 <= c[0] < self.ny and 0 <= c[1] < self.nx: self.land[c] = False
        # связные куски моря: порт привязываем только к большому (открытому) морю, а не к лиману или озеру
        self.comp = np.full(self.land.shape, -1); sizes = []
        for i0 in range(self.ny):
            for j0 in range(self.nx):
                if self.land[i0, j0] or self.comp[i0, j0] >= 0: continue
                cid = len(sizes); stack = [(i0, j0)]; self.comp[i0, j0] = cid; n = 0
                while stack:
                    i, j = stack.pop(); n += 1
                    for di in (-1, 0, 1):
                        for dj in (-1, 0, 1):
                            a, b = i + di, j + dj
                            if 0 <= a < self.ny and 0 <= b < self.nx and not self.land[a, b] and self.comp[a, b] < 0:
                                self.comp[a, b] = cid; stack.append((a, b))
                sizes.append(n)
        self.big = {c for c, n in enumerate(sizes) if n > 300}
    def cell(self, p): return int((p[1] - self.y0) / self.step), int((p[0] - self.x0) / self.step)
    def free(self, c):
        """ближайшая морская клетка (порт может стоять на суше)"""
        if not self.land[c] and self.comp[c] in self.big: return c
        for r in range(1, 40):
            best = None
            for di in range(-r, r + 1):
                for dj in range(-r, r + 1):
                    i, j = c[0] + di, c[1] + dj
                    if 0 <= i < self.ny and 0 <= j < self.nx and not self.land[i, j] and self.comp[i, j] in self.big:
                        d = di * di + dj * dj
                        if best is None or d < best[0]: best = (d, (i, j))
            if best: return best[1]
    def path(self, a, b):
        s, t = self.free(self.cell(a)), self.free(self.cell(b))
        dist = {s: 0}; prev = {}; pq = [(0, 0, s)]
        while pq:
            _, d, u = heapq.heappop(pq)
            if u == t: break
            if d > dist.get(u, 1e18): continue
            for di in (-1, 0, 1):
                for dj in (-1, 0, 1):
                    v = (u[0] + di, u[1] + dj)
                    if v == u or not (0 <= v[0] < self.ny and 0 <= v[1] < self.nx) or self.land[v]: continue
                    nd = d + math.hypot(di, dj * math.cos(math.radians(self.y0 + v[0] * self.step)))
                    if nd < dist.get(v, 1e18):
                        dist[v] = nd; prev[v] = u
                        heapq.heappush(pq, (nd + math.hypot(t[0] - v[0], t[1] - v[1]) * .7, nd, v))
        out = [t]
        while out[-1] != s: out.append(prev[out[-1]])
        cells = list(reversed(out))
        pts = [(self.x0 + (j + .5) * self.step, self.y0 + (i + .5) * self.step) for i, j in cells]
        return [a] + smooth(pts) + [b]
def smooth(pts, k=5):
    if len(pts) < 3: return pts
    a = np.array(pts); out = a.copy()
    for i in range(len(a)):
        lo, hi = max(0, i - k), min(len(a), i + k + 1); out[i] = a[lo:hi].mean(0)
    return [tuple(p) for p in out[::2]] + [tuple(out[-1])]
if __name__ == "__main__":
    g = road_graph(); print("узлов дорог:", len(g.keys))
    p, d = g.path((2.35, 48.86), (2.35, 43.21)); print("Париж—Каркассон по дорогам:", round(d), "км,", len(p), "точек")
    s = Sea(); p = s.path((12.32, 45.44), (35.08, 32.93)); print("Венеция—Акра морем:", len(p), "точек")
