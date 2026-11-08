// Где искать файлы проекта (limity.json, веса сетей, шаблоны): аудит кода, раздел Д, шаг 3.
// Относительный путь ищется так: DND_ROOT (если задан) → текущая папка → папка программы. Абсолютный путь — как есть.
// Если файла нет нигде, возвращается путь от DND_ROOT или текущей папки — тот, кто открывает, пишет громкую ошибку.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

inline bool dndExists(const std::string& p) { return std::ifstream(p).good(); }
inline std::string dndPath(const std::string& rel) {
    if (rel.empty() || rel[0] == '/') return rel;
    if (const char* r = std::getenv("DND_ROOT"); r && *r) return std::string(r) + "/" + rel;
    if (dndExists(rel)) return rel;
    char exe[1024];
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) {
        exe[n] = 0;
        std::string d = exe;
        d = d.substr(0, d.find_last_of('/') + 1) + rel;
        if (dndExists(d)) return d;
    }
    return rel;
}
// файл, без которого работа идёт по умолчаниям: сказать об этом один раз в stderr (раньше подмена была молчаливой)
inline void dndMissing(const std::string& path, const char* fallback) {
    std::fprintf(stderr, "ВНИМАНИЕ: нет файла %s — %s (папку проекта можно задать DND_ROOT)\n", path.c_str(), fallback);
}
