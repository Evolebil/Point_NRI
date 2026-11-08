// ============================================================================
// Рекуррентная сеть для бойца: вход → эмбеддинг (tanh) → GRU → три головы (логиты) + ценность.
// Без зависимостей от движка. Прямой и обратный проход (по времени) написаны вручную; проверка градиентов — тест T137 (audit_tests20.cc).
//
//   e   = tanh(We·x + be)
//   z   = σ(Wz·e + Uz·h₋ + bz)          вентиль обновления
//   r   = σ(Wr·e + Ur·h₋ + br)          вентиль сброса
//   n   = tanh(Wn·e + r ⊙ (Un·h₋) + bn)
//   h   = (1 − z) ⊙ n + z ⊙ h₋
//   logits = Wo·h + bo,  value = wv·h + bv
// ============================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

template <class T>
struct GruNet {
    int D = 0, E = 0, H = 0, NA = 0;   // вход, эмбеддинг, скрытое состояние, число логитов
    std::vector<T> p;                   // все параметры одним вектором
    size_t oWe = 0, obe = 0, oWz = 0, oUz = 0, obz = 0, oWr = 0, oUr = 0, obr = 0, oWn = 0, oUn = 0, obn = 0, oWo = 0, obo = 0, oWv = 0, obv = 0;

    // Активации одного шага (нужны для обратного прохода).
    struct Step {
        std::vector<T> x, e, z, r, n, un, hprev, h;
    };

    static double rnd01(uint64_t& s) {   // splitmix64 → [0,1)
        s += 0x9E3779B97F4A7C15ULL;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z ^= z >> 31;
        return (double)(z >> 11) * (1.0 / 9007199254740992.0);
    }

    size_t size() const { return p.size(); }

    void init(int d, int e, int h, int na, uint64_t seed) {
        D = d; E = e; H = h; NA = na;
        size_t n = 0;
        auto take = [&](size_t& off, size_t cnt) { off = n; n += cnt; };
        take(oWe, (size_t)E * D); take(obe, E);
        take(oWz, (size_t)H * E); take(oUz, (size_t)H * H); take(obz, H);
        take(oWr, (size_t)H * E); take(oUr, (size_t)H * H); take(obr, H);
        take(oWn, (size_t)H * E); take(oUn, (size_t)H * H); take(obn, H);
        take(oWo, (size_t)NA * H); take(obo, NA);
        take(oWv, H); take(obv, 1);
        p.assign(n, T(0));
        uint64_t s = seed;
        auto fill = [&](size_t off, size_t cnt, double fanIn, double fanOut) {
            double lim = std::sqrt(6.0 / (fanIn + fanOut));
            for (size_t i = 0; i < cnt; i++) p[off + i] = (T)((rnd01(s) * 2 - 1) * lim);
        };
        fill(oWe, (size_t)E * D, D, E);
        fill(oWz, (size_t)H * E, E, H); fill(oUz, (size_t)H * H, H, H);
        fill(oWr, (size_t)H * E, E, H); fill(oUr, (size_t)H * H, H, H);
        fill(oWn, (size_t)H * E, E, H); fill(oUn, (size_t)H * H, H, H);
        fill(oWo, (size_t)NA * H, H, NA);
        for (int i = 0; i < H; i++) p[obz + i] = T(1);   // вентиль обновления смещён к «помнить»: память держится дольше
        for (int i = 0; i < H; i++) p[oWv + i] *= T(0.1);
    }

    static T sig(T x) { return T(1) / (T(1) + std::exp(-x)); }
    // скалярное произведение восемью частичными суммами: компилятор складывает их векторами (порядок сложения
    // фиксирован — результат одинаков при любом числе потоков)
    static T dot(const T* a, const T* b, int n) {
        T acc[8] = {};
        int j = 0;
        for (; j + 8 <= n; j += 8)
            for (int k = 0; k < 8; k++) acc[k] += a[j + k] * b[j + k];
        T r = ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7]));
        for (; j < n; j++) r += a[j] * b[j];
        return r;
    }

    // Один шаг вперёд: x[D], hprev[H] → шаг s (в нём s.h — новое скрытое состояние), logits[NA], value.
    void forward(const T* x, const T* hprev, Step& s, T* logits, T& value) const {
        s.x.assign(x, x + D);
        s.hprev.assign(hprev, hprev + H);
        s.e.assign(E, 0); s.z.assign(H, 0); s.r.assign(H, 0); s.n.assign(H, 0); s.un.assign(H, 0); s.h.assign(H, 0);
        // вход в основном из нулей (окно карты, пустые слоты): умножаем только ненулевые — сумма та же до бита (w·0 = ±0)
        thread_local std::vector<int> nz;
        nz.clear();
        for (int j = 0; j < D; j++)
            if (x[j] != T(0)) nz.push_back(j);
        for (int i = 0; i < E; i++) {
            T a = p[obe + i];
            const T* w = &p[oWe + (size_t)i * D];
            for (int j : nz) a += w[j] * x[j];
            s.e[i] = std::tanh(a);
        }
        for (int i = 0; i < H; i++) {
            T az = p[obz + i], ar = p[obr + i], an = p[obn + i], au = 0;
            const T *wz = &p[oWz + (size_t)i * E], *wr = &p[oWr + (size_t)i * E], *wn = &p[oWn + (size_t)i * E];
            az += dot(wz, s.e.data(), E);
            ar += dot(wr, s.e.data(), E);
            an += dot(wn, s.e.data(), E);
            const T *uz = &p[oUz + (size_t)i * H], *ur = &p[oUr + (size_t)i * H], *un = &p[oUn + (size_t)i * H];
            az += dot(uz, hprev, H);
            ar += dot(ur, hprev, H);
            au += dot(un, hprev, H);
            s.z[i] = sig(az); s.r[i] = sig(ar); s.un[i] = au;
            s.n[i] = std::tanh(an + s.r[i] * au);
            s.h[i] = (T(1) - s.z[i]) * s.n[i] + s.z[i] * hprev[i];
        }
        for (int k = 0; k < NA; k++) {
            T a = p[obo + k];
            const T* w = &p[oWo + (size_t)k * H];
            logits[k] = a + dot(w, s.h.data(), H);
        }
        T v = p[obv];
        value = v + dot(&p[oWv], s.h.data(), H);
    }

    // Один шаг назад. dlogits[NA], dvalue — градиенты по выходам этого шага; dhnext[H] — градиент, пришедший по времени из следующего шага.
    // Градиенты по параметрам прибавляются в g; dhprev[H] — градиент по скрытому состоянию предыдущего шага.
    void backward(const Step& s, const T* dlogits, T dvalue, const T* dhnext, std::vector<T>& g, T* dhprev) const {
        std::vector<T> dh(H), dn(H), dz(H), dnp(H), dzp(H), drp(H), dun(H), de(E, 0), dep(E);
        for (int j = 0; j < H; j++) dh[j] = dhnext[j] + dvalue * p[oWv + j];
        for (int k = 0; k < NA; k++) {
            const T* w = &p[oWo + (size_t)k * H];
            T* gw = &g[oWo + (size_t)k * H];
            for (int j = 0; j < H; j++) { dh[j] += dlogits[k] * w[j]; gw[j] += dlogits[k] * s.h[j]; }
            g[obo + k] += dlogits[k];
        }
        for (int j = 0; j < H; j++) g[oWv + j] += dvalue * s.h[j];
        g[obv] += dvalue;
        for (int j = 0; j < H; j++) {
            dn[j] = dh[j] * (T(1) - s.z[j]);
            dz[j] = dh[j] * (s.hprev[j] - s.n[j]);
            dhprev[j] = dh[j] * s.z[j];
            dnp[j] = dn[j] * (T(1) - s.n[j] * s.n[j]);
            dzp[j] = dz[j] * s.z[j] * (T(1) - s.z[j]);
            T dr = dnp[j] * s.un[j];
            dun[j] = dnp[j] * s.r[j];
            drp[j] = dr * s.r[j] * (T(1) - s.r[j]);
        }
        for (int i = 0; i < H; i++) {
            T *gWn = &g[oWn + (size_t)i * E], *gWz = &g[oWz + (size_t)i * E], *gWr = &g[oWr + (size_t)i * E];
            const T *Wn = &p[oWn + (size_t)i * E], *Wz = &p[oWz + (size_t)i * E], *Wr = &p[oWr + (size_t)i * E];
            for (int j = 0; j < E; j++) {
                gWn[j] += dnp[i] * s.e[j]; gWz[j] += dzp[i] * s.e[j]; gWr[j] += drp[i] * s.e[j];
                de[j] += Wn[j] * dnp[i] + Wz[j] * dzp[i] + Wr[j] * drp[i];
            }
            T *gUn = &g[oUn + (size_t)i * H], *gUz = &g[oUz + (size_t)i * H], *gUr = &g[oUr + (size_t)i * H];
            const T *Un = &p[oUn + (size_t)i * H], *Uz = &p[oUz + (size_t)i * H], *Ur = &p[oUr + (size_t)i * H];
            for (int j = 0; j < H; j++) {
                gUn[j] += dun[i] * s.hprev[j]; gUz[j] += dzp[i] * s.hprev[j]; gUr[j] += drp[i] * s.hprev[j];
                dhprev[j] += Un[j] * dun[i] + Uz[j] * dzp[i] + Ur[j] * drp[i];
            }
            g[obn + i] += dnp[i]; g[obz + i] += dzp[i]; g[obr + i] += drp[i];
        }
        thread_local std::vector<int> nz;   // градиент по весам эмбеддинга — только у ненулевых входов (у нулевых он 0)
        nz.clear();
        for (int j = 0; j < D; j++)
            if (s.x[j] != T(0)) nz.push_back(j);
        for (int i = 0; i < E; i++) {
            dep[i] = de[i] * (T(1) - s.e[i] * s.e[i]);
            g[obe + i] += dep[i];
            T* gw = &g[oWe + (size_t)i * D];
            for (int j : nz) gw[j] += dep[i] * s.x[j];
        }
    }
};

// Adam для вектора параметров.
struct AdamState {
    std::vector<double> m, v;
    long long t = 0;
    void init(size_t n) { m.assign(n, 0.0); v.assign(n, 0.0); t = 0; }
    template <class T>
    void step(std::vector<T>& p, const std::vector<T>& g, double lr, double clipNorm) {
        double nrm = 0;
        for (T x : g) nrm += (double)x * x;
        nrm = std::sqrt(nrm);
        double sc = (clipNorm > 0 && nrm > clipNorm) ? clipNorm / nrm : 1.0;
        t++;
        const double b1 = 0.9, b2 = 0.999, eps = 1e-8;
        double c1 = 1 - std::pow(b1, (double)t), c2 = 1 - std::pow(b2, (double)t);
        for (size_t i = 0; i < p.size(); i++) {
            double gi = g[i] * sc;
            m[i] = b1 * m[i] + (1 - b1) * gi;
            v[i] = b2 * v[i] + (1 - b2) * gi * gi;
            p[i] -= (T)(lr * (m[i] / c1) / (std::sqrt(v[i] / c2) + eps));   // минимизируем потерю
        }
    }
};
