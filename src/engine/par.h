// Общие утилиты для программ обучения: хэш зерна и параллельный цикл (число потоков: DND_THREADS, по умолчанию ядра − 2).
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>

static uint64_t mix(uint64_t a, uint64_t b) {
    uint64_t z = a * 0x9E3779B97F4A7C15ULL + b + 0x7F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static int nThreads() {
    const char* e = std::getenv("DND_THREADS");
    return e ? std::max(1, std::atoi(e)) : std::max(1, (int)std::thread::hardware_concurrency() - 2);   // ТЗ v21: ядра − 2
}
static void parallelFor(int n, const std::function<void(int)>& f) {
    std::atomic<int> next(0);
    auto w = [&]() {
        for (;;) {
            int i = next++;
            if (i >= n) break;
            f(i);
        }
    };
    int nth = std::min(nThreads(), n);
    std::vector<std::thread> th;
    for (int k = 1; k < nth; k++) th.emplace_back(w);
    w();
    for (auto& t : th) t.join();
}
