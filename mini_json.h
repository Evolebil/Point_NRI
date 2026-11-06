// ============================================================================
// Минимальный разбор JSON (для описания персонажа и команд игры). Числа хранятся как double, объекты — как список пар.
// ============================================================================
#pragma once
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

static std::string jq(const std::string& s) {   // строка JSON
    std::string o = "\"";
    for (unsigned char ch : s) {
        if (ch == '"' || ch == '\\') { o += '\\'; o += (char)ch; }
        else if (ch == '\n') o += "\\n";
        else if (ch < 0x20) o += ' ';
        else o += (char)ch;
    }
    return o + "\"";
}

struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
    bool b = false;
    double d = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    bool has(const std::string& k) const {
        for (auto& p : o)
            if (p.first == k) return true;
        return false;
    }
    const Json& operator[](const std::string& k) const {
        static const Json nul;
        for (auto& p : o)
            if (p.first == k) return p.second;
        return nul;
    }
    const Json& operator[](size_t i) const {
        static const Json nul;
        return i < a.size() ? a[i] : nul;
    }
    size_t size() const { return a.size(); }
    int num(int def = 0) const { return t == NUM ? (int)(d < 0 ? d - 0.5 : d + 0.5) : (t == BOOL ? (b ? 1 : 0) : def); }
    double dbl(double def = 0) const { return t == NUM ? d : def; }
    bool flag(bool def = false) const { return t == BOOL ? b : (t == NUM ? d != 0 : def); }
    std::string str(const std::string& def = "") const { return t == STR ? s : def; }
};

struct JsonParser {
    const std::string& src;
    size_t i = 0;
    bool ok = true;
    explicit JsonParser(const std::string& s) : src(s) {}
    void ws() {
        while (i < src.size() && (src[i] == ' ' || src[i] == '\n' || src[i] == '\t' || src[i] == '\r')) i++;
    }
    bool eat(char c) {
        ws();
        if (i < src.size() && src[i] == c) {
            i++;
            return true;
        }
        return false;
    }
    std::string str() {
        std::string r;
        i++;   // открывающая кавычка
        while (i < src.size() && src[i] != '"') {
            if (src[i] == '\\' && i + 1 < src.size()) {
                i++;
                switch (src[i]) {
                    case 'n': r += '\n'; break;
                    case 't': r += '\t'; break;
                    case 'u': {   // \uXXXX -> UTF-8
                        unsigned cp = (unsigned)std::strtoul(src.substr(i + 1, 4).c_str(), nullptr, 16);
                        i += 4;
                        if (cp < 0x80) r += (char)cp;
                        else if (cp < 0x800) { r += (char)(0xC0 | (cp >> 6)); r += (char)(0x80 | (cp & 63)); }
                        else { r += (char)(0xE0 | (cp >> 12)); r += (char)(0x80 | ((cp >> 6) & 63)); r += (char)(0x80 | (cp & 63)); }
                        break;
                    }
                    default: r += src[i];
                }
            } else
                r += src[i];
            i++;
        }
        i++;   // закрывающая
        return r;
    }
    Json value() {
        Json j;
        ws();
        if (i >= src.size()) { ok = false; return j; }
        const char c = src[i];
        if (c == '{') {
            i++;
            j.t = Json::OBJ;
            if (eat('}')) return j;
            do {
                ws();
                if (i >= src.size() || src[i] != '"') { ok = false; return j; }
                std::string k = str();
                if (!eat(':')) { ok = false; return j; }
                j.o.push_back({k, value()});
            } while (ok && eat(','));
            if (!eat('}')) ok = false;
        } else if (c == '[') {
            i++;
            j.t = Json::ARR;
            if (eat(']')) return j;
            do j.a.push_back(value());
            while (ok && eat(','));
            if (!eat(']')) ok = false;
        } else if (c == '"') {
            j.t = Json::STR;
            j.s = str();
        } else if (src.compare(i, 4, "true") == 0) {
            j.t = Json::BOOL; j.b = true; i += 4;
        } else if (src.compare(i, 5, "false") == 0) {
            j.t = Json::BOOL; j.b = false; i += 5;
        } else if (src.compare(i, 4, "null") == 0) {
            i += 4;
        } else {
            char* e = nullptr;
            j.d = std::strtod(src.c_str() + i, &e);
            if (e == src.c_str() + i) { ok = false; return j; }
            j.t = Json::NUM;
            i = (size_t)(e - src.c_str());
        }
        return j;
    }
};

static Json parseJson(const std::string& s, bool* ok = nullptr) {
    JsonParser p(s);
    Json j = p.value();
    p.ws();
    if (ok) *ok = p.ok && p.i == s.size();
    return j;
}
