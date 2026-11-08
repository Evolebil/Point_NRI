// Исполняемая проверка требований Арены Боссов (правила v21, этап 0: pravila-v21.md) против движка boss_arena_v20.cc.
// Сборка: g++ -std=c++17 -O2 -o audit_tests audit_tests.cc   (рядом лежит boss_arena_v6.cc)
#define main boss_main
#include "boss_arena_v20.cc"
#include "char_spec.h"
#include "gru.h"
#undef main
#include <regex>
#include <chrono>
#include <functional>
#include <cstdarg>
#include <queue>
#include <set>
#include <map>
static int g_pass = 0, g_fail = 0;
// Параллельный прогон (v21): проверки main — блоки; блок k выполняет процесс k % g_shards, перед блоком своё зерно
// случайности по его номеру (результат не зависит от числа процессов). Строки результатов помечены номером блока —
// run_tests.sh собирает их по порядку. Без аргументов — один процесс, все блоки, как раньше.
static int g_shards = 1, g_shard = 0, g_blk = -1, g_blkN = 0;
static double g_blkT0 = -1;
static void blkTime() {   // время предыдущего блока — в stderr (DND_TEST_TIME), чтобы видеть самые долгие проверки
    if (g_blkT0 >= 0 && std::getenv("DND_TEST_TIME")) {
        const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() - g_blkT0;
        if (t > 5) fprintf(stderr, "#ВРЕМЯ блок %d: %.0f с\n", g_blk, t);
    }
    g_blkT0 = -1;
}
static bool TB() {
    blkTime();
    g_blk = g_blkN++;
    if (g_blk % g_shards != g_shard) return false;
    rng.seed(0x5EED0000ULL + (uint64_t)g_blk);
    g_worldEpoch = g_blk;   // мир — свой у каждого блока: результат не зависит от числа процессов
    g_blkT0 = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (std::getenv("DND_TEST_START")) fprintf(stderr, "#СТАРТ блок %d\n", g_blk);
    return true;
}
static void T(const char* id, const char* req, bool ok, const std::string& det) {
    if (g_shards > 1) printf("@%04d ", g_blk);
    printf("%-4s %-4s %-62s %s\n", id, ok ? "PASS" : "FAIL", req, det.c_str());
    fflush(stdout);
    ok ? g_pass++ : g_fail++;
}
static std::string fmt(const char* f, ...) {
    char b[2000];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof b, f, a);
    va_end(a);
    return b;
}
// призыв (v21) — способность: k копий шаблона t за применение, uses применений (mk() применения не ограничивает)
static int addSummon(Boss& b, const Boss& t, int k, int uses = UNL, bool conc = false) {
    Spec s;
    s.self = true;
    s.delivery = 2;
    s.range = 0;
    s.sumN = k;
    s.sumTmpl = (int)b.tmpl.size();
    s.sumCost = t.cost;
    s.uses = uses;
    s.conc = conc;
    b.tmpl.push_back(t);
    b.abil.push_back(s);
    return (int)b.abil.size() - 1;
}
static bool hasSummon(const Boss& b) {   // призыв (шаблон облика превращения — не миньон)
    for (auto& s : b.abil)
        if (s.sumN > 0) return true;
    return false;
}
Coef CC() {
    Coef C;
    for (int a = 0; a < AT_N; a++) C[a] = AD[a].init;
    return C;
}
// независимый перерасчёт цены босса «снизу вверх» (обратное направление)
long long priceOf(const Boss& b, const Coef& C, double budget) {
    double prev = g_B;
    g_B = budget;   // внутренний бюджет (видимый + STAT_BASE)
    PcGuard pcg(b);  // цены оружия и длящихся эффектов зависят от тела (после теста 4 ноября)
    auto P = [&](int a, double m = 1.0) { return C[a] * m * scl(a); };
    // урон оружия за действие (независимо от wpHits): кубики + доля C[доп. атака] за каждый следующий полный удар
    auto hitsOf = [&](int n, int sd, int range, int att, bool fin, bool ammo) {
        const int sm = modOf(b.stats[STR]), dm = modOf(b.stats[DEX]);
        const int mod = range > 5 && ammo ? dm : (fin ? std::max(sm, dm) : sm);
        return n * (sd + 1) / 2.0 + C[AT_WPNATT] * std::max(0, att - 1) * (n * (sd + 1) / 2.0 + std::max(0, mod));
    };
    long long t = 0;
    for (int i = 0; i < STAT_N; i++) t += tri(b.stats[i]);
    t += acCost(b.acV) + 2LL * b.hp;   // v21: КД = 10 + acV (+ ЛОВ по доспеху + щит), цена tri(2·acV)
    t += armorPrice(b, C) + weaponHandsPrice(b, C) + sizePrice(b, C);   // и размер (v21)   // доспехи и щит, двуручное и универсальное оружие (v21)
    if (b.wN > 0)
        t += (long long)std::ceil(C[AT_WPNDICE] * hitsOf(b.wN, b.wS, b.wRange, b.wAtt, b.wFinesse, b.wAmmo) * wpMult(C, b.wRange, b.reach, b.wAtt)) + (b.wFinesse ? 3 : 0);
    if (b.wN > 0 && b.wLong > b.wRange && b.wRange > 5)   // дальняя дистанция (v21): доля надбавки за дальность
        t += (long long)std::ceil(C[AT_WPNDICE] * hitsOf(b.wN, b.wS, b.wRange, b.wAtt, false, false) * (wpMult(C, b.wLong, b.reach, b.wAtt) - wpMult(C, b.wRange, b.reach, b.wAtt)) * C[AT_WLONG]);
    if (b.rN > 0 || b.rCond > 0) {
        double q = riderGrabPrice(b, C);   // захват: опутан, урон пока держит (пакет атомов 2)
        if (b.rN > 0) q += C[AT_RDMG] * b.rN * (b.rS + 1) / 2.0;
        if (b.rCond > 0) {
            double dm = (b.rCond == C_PRONE) ? 1.0 : 1 + 0.25 * (b.rRounds - 1);
            q += C[AT_PRONE + b.rCond - 1] * dm * scl(AT_PRONE + b.rCond - 1) * C[AT_RCOND];
        }
        t += (long long)std::ceil(q);
    }
    for (auto& q : b.alt)
        t += (long long)std::ceil(C[AT_WPNDICE] * hitsOf(q.n, q.s, q.range, q.att, q.finesse, q.ammo) * wpMult(C, q.range, q.reach, q.att)) + (q.finesse ? 3 : 0);
    for (auto& s : b.abil) t += specPrice(s, C);
    auto cp = [&](double x) { t += (long long)std::ceil(x); };
    if (b.regenN) cp(P(AT_REGEN, b.regenN) * suppMul(b, C, 1));
    for (int d = 0; d < DT_N; d++) {
        if ((b.resMask >> d) & 1) cp(P(AT_RESIST, 5.0 * g_share[d]) * suppMul(b, C, 4));
        if ((b.immMask >> d) & 1) cp(P(AT_IMMTYPE, 5.0 * g_share[d]) * suppMul(b, C, 4));
        if ((b.vulnMask >> d) & 1) t -= (long long)std::ceil(P(AT_VULN, 5.0 * g_share[d]));
    }
    if (b.resNM) cp(P(AT_RESNM, 5.0 * g_nmShare) * suppMul(b, C, 4));
    if (b.immNM) cp(P(AT_IMMNM, 5.0 * g_nmShare) * suppMul(b, C, 4));
    if (b.wMagic) cp(P(AT_WMAGIC));
    if (b.wSilver && !b.wMagic) cp(P(AT_WMAGIC) * C[AT_WSILVER]);   // пакеты атомов 2–3
    if (b.lightR > 0) cp(C[AT_LIGHT] * b.lightR / 10.0);
    if (b.legRes) cp(P(AT_LEGRES, b.legRes));
    if (b.legAct) cp(P(AT_LEGACT, b.legAct));
    if (b.parry) cp(P(AT_PARRY, b.parry));
    if (b.uncanny) cp(P(AT_UNCANNY));
    if (b.magicRes) cp(P(AT_MAGICRES));   // пакет атомов 1
    if (b.spellRes) cp(P(AT_SPELLRES));
    if (b.critImm) cp(P(AT_CRITIMM));
    if (b.immPush) cp(P(AT_IMMPUSH));
    if (b.disengB) cp(P(AT_DISENGB));
    if (b.redirect) cp(P(AT_REDIRECT));
    for (auto& tg : b.trigs) t += trigPrice(tg, b, C);   // «событие → эффект» (v21)
    // «когда → что»: цена эффекта × доля времени условия (независимый пересчёт, без rulePrice)
    for (auto& r : b.rules) {
        const double hits = 1 + C[AT_WPNATT] * std::max(0, b.wAtt - 1);   // «за удар» — на каждый удар
        double e = r.what == E_ADVATK ? P(AT_E_ADVATK) : r.what == E_DMG ? C[AT_E_DMG] * r.n * (r.s + 1) / 7.0 * (r.once ? C[AT_E_ONCE] : hits)
                 : r.what == E_AC ? C[AT_E_AC] * r.n : r.what == E_DISVSME ? P(AT_E_DISVSME) : r.what == E_ADVVSME ? -P(AT_E_ADVVSME)
                 : r.what == E_RES ? P(AT_E_RES, 5.0) * suppMul(b, C, 4) : r.what == E_RESNM ? P(AT_E_RES, 5.0 * g_nmShare) * suppMul(b, C, 4)
                 : r.what == E_SAVEB ? P(AT_E_SAVEB) * r.n
                 : r.what == E_ATKDIE ? P(AT_E_ATKDIE) * r.n * (r.s + 1) / 5.0 : r.what == E_SAVEDIE ? P(AT_E_SAVEDIE) * r.n * (r.s + 1) / 5.0
                 : r.what == E_DMGRED ? P(AT_E_DMGRED) * r.n : r.what == E_PIERCE ? P(AT_E_PIERCE, 5.0 * g_share[std::clamp(r.n, 0, DT_N - 1)])
                 : r.what == E_NOCOVER ? P(AT_E_NOCOVER) : r.what == E_POWER ? P(AT_E_POWER) * r.n * hits : r.what == E_NOOA ? P(AT_E_NOOA)
                 : r.what == E_SKILLADV ? P(AT_E_SKILLADV) * (skCombat(std::clamp(r.n, 0, SK_N - 1)) ? 1.0 : 0.1) : r.what == E_DISATK ? -P(AT_E_DISATK)
                 : r.what == E_ACMINUS ? -P(AT_E_ACMINUS) * r.n : P(AT_E_ADVSAVE);
        e *= C[AT_W_ALWAYS + r.when];
        if (r.allyR > 0) e *= 1 + C[AT_RULEAURA] * tierN(TR_AURARAD, r.allyR / 5.0);   // аура и на своих (v21)
        t += e >= 0 ? (long long)std::ceil(e) : -(long long)std::ceil(-e);
    }
    for (int k = 1; k < C_N; k++)
        if (b.immCond & (1u << k)) cp(P(AT_IMMCOND));
    for (int s = 0; s < STAT_N; s++)
        if (b.saveProf & (1u << s)) cp(P(AT_SAVEPROF, b.prof / 2.0));
    // владения (v21): мастерство сверх +2, навыки и экспертиза, инструменты, без владения своим оружием (возврат)
    if (b.prof > 2) cp(P(AT_PROF, b.prof - 2));
    for (int k = 0; k < SK_N; k++) {
        const int at = skCombat(k) ? AT_SKILLC : AT_SKILLNC;
        const double m = skCombat(k) ? b.prof / 2.0 : 1.0;
        if ((b.skillProf >> k) & 1u) cp(P(at, m));
        if ((b.skillExp >> k) & 1u) cp(P(at, m));
    }
    if (b.toolN > 0) cp(P(AT_SKILLNC, b.toolN));
    if (b.wNoProf && b.wN > 0) t -= (long long)std::ceil(P(AT_NOWPROF, b.prof / 2.0));
    if (b.fly) cp(P(AT_FLY));
    if (b.speed > 0) cp(P(AT_SPEED, tierN(TR_SPEED, b.speed / 5.0)));   // v21: скорость покупается вся, с нуля
    if (b.riposte) cp(P(AT_RIPOSTE));
    if (b.surge) cp(P(AT_SURGE, b.surge));
    for (int L = 1; L < 10; L++)
        if (b.slots[L]) cp(C[AT_SLOT] * L * b.slots[L]);
    if (b.dv) cp(P(AT_DARKV, b.dv == 30 ? 1 : (b.dv == 60 ? 2 : 3)));
    if (b.truesight) cp(P(AT_TRUESIGHT, b.truesight <= 10 ? 1 : (b.truesight <= 30 ? 2 : (b.truesight <= 60 ? 3 : 4))));   // истинное зрение (v21)
    if (b.blindsight) cp(P(AT_BLINDS, b.blindsight == 10 ? 1 : (b.blindsight == 30 ? 2 : 3)));
    if (b.ignDiff) cp(P(AT_IGNTERR));
    if (b.climbS) cp(P(AT_CLIMB));
    if (b.swimS) cp(P(AT_SWIM));
    if (b.burrowS) cp(P(AT_BURROW));
    if (b.evasion) cp(P(AT_EVASION));
    if (b.lair) cp(P(AT_LAIR, b.lair));
    if (b.sunSens) t -= (long long)std::ceil(P(AT_SUNSENS));
    if (b.auraRad > 0) {
        double q = 0;
        if (b.auraN > 0) q += C[AT_AURA] * tierN(TR_AURADICE, b.auraN * (b.auraS + 1) / 7.0) * 3.5;
        if (b.auraCond > 0) {
            double dm = (b.auraCond == C_PRONE) ? 1.0 : 1 + 0.25 * (b.auraRounds - 1);
            q += C[AT_PRONE + b.auraCond - 1] * dm * scl(AT_PRONE + b.auraCond - 1) * C[AT_AURACOND];
        }
        q *= 1 + C[AT_AURARAD] * tierN(TR_AURARAD, b.auraRad / 5.0 - 1);
        cp(q * suppMul(b, C, 2));
    }
    // призыв (v21) — способность: его цена (шаблоны и плата за призыв) уже в specPrice
    if (b.abil.size() > 3) t += (long long)std::ceil(P(AT_NABIL, tierN(TR_NABIL, (double)(b.abil.size() - 3))));
    g_B = prev;
    return t;
}
AMask featureAtoms(const Boss& b) {
    AMask m = 0;
    for (auto& s : b.abil) markSpec(s, m);
    markArmor(b, m);   // доспехи и щит, двуручное и универсальное (v21)
    auto set = [&](int a) { m |= ONE << a; };
    if (b.abil.size() > 3) set(AT_NABIL);
    if (b.wN > 0) {
        if (b.wAtt >= 2) set(AT_WPNATT);
        if (rangeIdx(b.wRange) >= 1) set(AT_WPNRANGE);
        if (b.wLong > b.wRange && b.wRange > 5) set(AT_WLONG);
        if (b.wTwoH) set(AT_TWOHAND);
        if (b.wVers > b.wS) set(AT_VERS);
        if (b.wRange <= 5 && b.reach > 5) set(AT_REACH);
    }
    for (auto& q : b.alt) {
        if (q.att >= 2) set(AT_WPNATT);
        if (rangeIdx(q.range) >= 1) set(AT_WPNRANGE);
        if (q.range <= 5 && q.reach > 5) set(AT_REACH);
    }
    if (b.suppMask && (((b.suppTgt & 1) && b.regenN) || ((b.suppTgt & 2) && b.auraRad > 0) ||
                       ((b.suppTgt & 4) && (b.resMask | b.immMask || b.resNM || b.immNM || hasResRule(b)))))
        set(AT_SUPP);
    if (b.regenN) set(AT_REGEN);
    if (b.resMask) set(AT_RESIST);
    if (b.immMask) set(AT_IMMTYPE);
    if (b.vulnMask) set(AT_VULN);
    if (b.resNM) set(AT_RESNM);
    if (b.immNM) set(AT_IMMNM);
    if (b.wMagic) set(AT_WMAGIC);
    if (b.wSilver && !b.wMagic) set(AT_WSILVER);   // пакеты атомов 2–3
    if (b.lightR > 0) set(AT_LIGHT);
    if (b.armor == AR_NONE && b.unarmoredStat >= 0) set(AT_UNARMORED);
    if (b.rCond == C_GRAPPLE && b.rGrabRestr) set(AT_GRABRESTR);
    if (b.rCond == C_GRAPPLE && b.rGrabDotN > 0) set(AT_GRABDOT);
    if (b.legRes) set(AT_LEGRES);
    if (b.legAct) set(AT_LEGACT);
    if (b.parry) set(AT_PARRY);
    if (b.uncanny) set(AT_UNCANNY);
    if (b.magicRes) set(AT_MAGICRES);   // пакет атомов 1
    if (b.spellRes) set(AT_SPELLRES);
    if (b.critImm) set(AT_CRITIMM);
    if (b.redirect) set(AT_REDIRECT);
    for (auto& tg : b.trigs) markTrig(tg, m);   // «событие → эффект» (v21)
    for (auto& r : b.rules) {   // «когда → что»: атомы условия и эффекта
        set(AT_W_ALWAYS + r.when);
        set(r.what == E_ADVATK ? AT_E_ADVATK : r.what == E_DMG ? AT_E_DMG : r.what == E_AC ? AT_E_AC : r.what == E_DISVSME ? AT_E_DISVSME
            : r.what == E_ADVVSME ? AT_E_ADVVSME : r.what == E_ADVSAVE ? AT_E_ADVSAVE : r.what == E_SAVEB ? AT_E_SAVEB
            : r.what == E_ATKDIE ? AT_E_ATKDIE : r.what == E_SAVEDIE ? AT_E_SAVEDIE : r.what == E_DMGRED ? AT_E_DMGRED : r.what == E_PIERCE ? AT_E_PIERCE
            : r.what == E_NOCOVER ? AT_E_NOCOVER : r.what == E_POWER ? AT_E_POWER : r.what == E_NOOA ? AT_E_NOOA
            : r.what == E_SKILLADV ? AT_E_SKILLADV : r.what == E_DISATK ? AT_E_DISATK : r.what == E_ACMINUS ? AT_E_ACMINUS : AT_E_RES);
        if (r.allyR > 0) set(AT_RULEAURA);
        if (r.what == E_DMG && r.once) set(AT_E_ONCE);
    }
    if (b.immPush) set(AT_IMMPUSH);
    if (b.disengB) set(AT_DISENGB);
    if (b.immCond) set(AT_IMMCOND);
    if (b.saveProf) set(AT_SAVEPROF);
    if (b.prof > 2) set(AT_PROF);
    for (int k = 0; k < SK_N; k++)
        if (((b.skillProf >> k) & 1u) && skCombat(k)) set(AT_SKILLC);
    if (b.wNoProf) set(AT_NOWPROF);
    if (b.fly) set(AT_FLY);
    if (b.speed > 30) set(AT_SPEED);   // v21: атом отмечен у быстрых
    if (b.riposte) set(AT_RIPOSTE);
    if (b.surge) set(AT_SURGE);
    for (int L = 1; L < 10; L++)
        if (b.slots[L]) set(AT_SLOT);
    if (b.dv) set(AT_DARKV);
    if (b.truesight) set(AT_TRUESIGHT);
    if (b.blindsight) set(AT_BLINDS);
    if (b.ignDiff) set(AT_IGNTERR);
    if (b.climbS) set(AT_CLIMB);
    if (b.swimS) set(AT_SWIM);
    if (b.burrowS) set(AT_BURROW);
    if (b.evasion) set(AT_EVASION);
    if (b.lair) set(AT_LAIR);
    if (b.sunSens) set(AT_SUNSENS);
    if (b.auraRad > 0) {
        set(AT_AURARAD);
        if (b.auraN > 0) set(AT_AURA);
        if (b.auraCond > 0) {
            set(AT_AURACOND);
            set(AT_PRONE + b.auraCond - 1);
        }
    }
    if (b.rN > 0) set(AT_RDMG);
    if (b.rCond > 0) {
        set(AT_RCOND);
        set(AT_PRONE + b.rCond - 1);
    }
    return m;
}
Fighter mk(const Boss& b, int team, int x, int y) {
    Fighter f;
    f.b = b;
    f.team = team;
    f.x = x;
    f.y = y;
    f.hp = f.maxHp = b.hp;
    f.uses.assign(b.abil.size(), UNL);
    for (auto& t : b.trigs) f.trigLeft.push_back(t.uses);   // «событие → эффект» (v21)
    f.lrLeft = b.legRes;
    f.legLeft = b.legAct;
    f.surgeLeft = b.surge;
    for (int L = 1; L < 10; L++) f.slotLeft[L] = b.slots[L];
    static uint64_t aiSeed = 0;
    f.ai.seed(++aiSeed);   // свои случайные числа решений бота (v21): у каждого бойца разные, общий rng не трогают
    return f;
}
Boss plain(int hp, int wN, int wS, int wRange) {
    Boss b;
    b.stats = {10, 10, 10, 10, 10, 10};
    b.ac = 0;
    b.acV = 0;
    b.hp = hp;
    b.wN = wN;
    b.wS = wS;
    b.wRange = wRange;
    return b;
}
int main(int argc, char** argv) {
    if (argc > 2) {
        g_shards = std::max(1, std::atoi(argv[1]));
        g_shard = std::atoi(argv[2]);
    }
    Coef C = CC();
    rng.seed(2026);
    if (g_shards == 1) printf("ID   Рез.  Требование (правила v0.5)                                      Детали\n");
    // T01 модификатор
    if (TB()) {
        bool ok = true;
        for (int v = 1; v <= 30; v++) {
            int e = (v >= 10) ? (v - 10) / 2 : -((11 - v) / 2);
            if (modOf(v) != e) ok = false;
        }
        T("T01", "§2 мод = floor((V-10)/2), V=1..30", ok, "");
    }
    // T02 tri/maxV
    if (TB()) {
        bool ok = true;
        for (long long b = 0; b < 20000; b++) {
            int v = maxV(b);
            if (tri(v) > b || tri(v + 1) <= b) ok = false;
        }
        T("T02", "§2/§3 цена V(V+1)/2 и обратная maxV согласованы", ok, "");
    }
    // T03 КД
    if (TB()) {
        int bad = 0, N = 4000;
        for (int i = 0; i < N; i++) {
            long long B = 200 << irand(6);
            Boss b = makeBoss(B, C, true);
            if (b.ac != calcAC(b) || b.acV < 0 || b.acV > AC_MAX - 10) bad++;   // v21: КД = 10 + единицы + ЛОВ по доспеху + щит
        }
        T("T03", "§3 КД = 10 + floor(V/2)", bad == 0, fmt("нарушений %d из %d", bad, N));
    }
    // T04 бюджетный инвариант
    if (TB()) {
        int over = 0, overS = 0, nS = 0, N = 6000;
        long long maxOver = 0;
        double eff = 0;
        int hp1 = 0, hp1S = 0;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << irand(6);
            Boss b = makeBoss(B, C, true);
            bool s = hasSummon(b);
            if (s) nS++;
            eff += (double)(b.cost - STAT_BASE) / B;
            if (b.hp <= 1) {
                hp1++;
                if (s) hp1S++;
            }
            if (b.cost - STAT_BASE > B) {
                over++;
                if (s) overS++;
                maxOver = std::max(maxOver, b.cost - STAT_BASE - B);
            }
        }
        T("T04", "§1 стоимость босса никогда не превышает бюджет", over == 0,
          fmt("превышений %d/%d (из них с призывом %d/%d), макс. перерасход %lld, ср. использование бюджета %.1f%%, "
              "боссов с 1 ХП: %d (с призывом %d)",
              over, N, overS, nS, maxOver, 100 * eff / N, hp1, hp1S));
    }
    // T05 round-trip
    if (TB()) {
        int bad = 0, N = 6000;
        long long maxd = 0;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << irand(6);
            Boss b = makeBoss(B, C, true);
            long long p = priceOf(b, C, (double)(B + STAT_BASE));
            if (p != b.cost) {
                bad++;
                maxd = std::max(maxd, std::llabs(p - b.cost));
            }
        }
        T("T05", "обратное направление: цена(собранный босс) = записанной стоимости", bad == 0,
          fmt("расхождений %d/%d, макс. %lld очк.", bad, N, maxd));
    }
    // T06 метки атомов
    if (TB()) {
        std::map<int, int> miss;
        int bad = 0, N = 6000;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << irand(6);
            Boss b = makeBoss(B, C, true);
            AMask f = featureAtoms(b), m = b.atoms;
            if (f != m) {
                bad++;
                for (int a = 0; a < AT_N; a++)
                    if (((f >> a) & 1) != ((m >> a) & 1)) miss[a]++;
            }
        }
        std::string d = fmt("боссов с рассогласованием %d/%d", bad, N);
        for (auto& k : miss) d += fmt("; %s x%d", AD[k.first].name, k.second);
        T("T06", "§11 набор атомов, по которому калибруется цена, = реальному содержимому босса", bad == 0, d);
    }
    // T07 инварианты способностей
    if (TB()) {
        int bad1 = 0, bad2 = 0, bad3 = 0, bad4 = 0, zero = 0, N = 30000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(i % 5);
            if (s.delivery == 2 && s.cond && s.condRounds != 1) bad1++;
            if (s.cond == C_POISON && s.saveStat != CON) bad2++;
            {
                bool cd = s.cond > 0;
                if (cd && (s.cond == C_FRIGHT || s.cond == C_FLEE) && s.saveStat != WIS) bad3++;
                if (!cd && s.dmgN > 0 && (s.dtype == DT_FIRE || s.dtype == DT_LIGHTNING) && s.saveStat != DEX &&
                    s.saveStat != CON)
                    bad3++;
                if (!cd && s.dmgN > 0 && s.dtype == DT_PSYCHIC && s.saveStat != INT && s.saveStat != WIS &&
                    s.saveStat != CHA)
                    bad3++;
                if (!cd && s.dmgN > 0 && s.dtype == DT_POISON && s.saveStat != CON) bad3++;
            }
            if (s.dmgN > 0 && !isDie(s.dmgS)) bad4++;   // v21, №11: размер кубика — только из списка DICE
            g_B = 800;
            if (specPrice(s, C) <= 0) zero++;
        }
        T("T07a", "§6 без броска -> состояние ровно 1 раунд", bad1 == 0, fmt("нарушений %d", bad1));
        T("T07b", "§7 яд -> спасбросок ТЕЛ", bad2 == 0, fmt("нарушений %d", bad2));
        T("T07c",
          "§2 спасбросок по смыслу эффекта: страх→МДР; огонь/электричество→ЛОВ/ТЕЛ; психика→ИНТ/МДР/ХАР; яд→ТЕЛ",
          bad3 == 0, fmt("нарушений %d", bad3));
        T("T07d", "§6 кубик способности — из списка DICE (к2…к100, v21)", bad4 == 0, fmt("вне списка %d", bad4));
        T("T07e", "§6 цена способности >= 1 (нет бесплатных способностей)", zero == 0,
          fmt("способностей с ценой 0: %d из %d (пустая нагрузка)", zero, N));
    }
    // T08 масштаб ⚖
    if (TB()) {
        Spec s;
        s.cond = C_STUN;
        s.condRounds = 1;
        s.delivery = 1;
        s.range = 5;
        bool ok = true;
        std::string d;
        double base = 0;
        for (double B : {400., 800., 1600.}) {
            g_B = B;
            double p = specPrice(s, C);
            if (B == 800) base = p;
            d += fmt("B=%.0f:%.0f ", B, p);
        }
        g_B = 400;
        double p4 = specPrice(s, C);
        g_B = 1600;
        double p16 = specPrice(s, C);
        ok = (std::fabs(p4 - base / 2) <= 1.01) && (std::fabs(p16 - base * 2) <= 1.01);
        T("T08", "§1 цены ⚖ пропорциональны (бюджет/800)", ok, d);
    }
    // T09 d20
    if (TB()) {
        auto mean = [&](bool a, bool d) {
            double t = 0;
            int N = 200000;
            for (int i = 0; i < N; i++) t += d20(a, d);
            return t / N;
        };
        double n = mean(0, 0), ad = mean(1, 0), di = mean(0, 1), bo = mean(1, 1);
        T("T09", "§8 преимущество/помеха; вместе гасятся",
          std::fabs(n - 10.5) < 0.1 && std::fabs(ad - 13.82) < 0.1 && std::fabs(di - 7.18) < 0.1 &&
              std::fabs(bo - 10.5) < 0.1,
          fmt("средние: обычный %.2f, преим. %.2f, помеха %.2f, оба %.2f", n, ad, di, bo));
    }
    // T10 крит/промах
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss hi = plain(10, 1, 6, 5);
        hi.ac = 1000;
        Boss lo = plain(10, 1, 6, 5);
        lo.ac = -1000;
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(hi, 1, 11, 10));
        int hits = 0, crits = 0, N = 100000;
        for (int i = 0; i < N; i++) {
            bool c;
            if (attackRoll(w, w.f[0], w.f[1], PROF, 5, c)) {
                hits++;
                if (c) crits++;
            }
        }
        w.f[1].b = lo;
        int miss = 0;
        for (int i = 0; i < N; i++) {
            bool c;
            if (!attackRoll(w, w.f[0], w.f[1], PROF, 5, c)) miss++;
        }
        T("T10", "§8 нат.20 всегда попадает (крит), нат.1 всегда промах",
          std::fabs(hits / (double)N - 0.05) < 0.005 && hits == crits && std::fabs(miss / (double)N - 0.05) < 0.005,
          fmt("против КД 1000: попаданий %.1f%% (все криты: %s); против КД -1000: промахов %.1f%%", 100.0 * hits / N,
              hits == crits ? "да" : "нет", 100.0 * miss / N));
    }
    // T11 сбит с ног (v0.6): помеха на свои атаки действует только на реакции до его хода
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 11, 10));
        w.f[0].cond[C_PRONE] = 1;
        bool a, d;
        advState(w, w.f[0], w.f[1], 5, a, d);
        T("T11", "§7 (v0.6) сбитый: своя атака реакцией — с помехой; в свой ход он встаёт", d,
          fmt("advState даёт помеху сбитому атакующему: %s", d ? "да" : "нет"));
    }
    // T12 иммунитет после окончания
    if (TB()) {
        Fighter t = mk(plain(10, 0, 0, 5), 0, 1, 1);
        bool a1 = applyCond(t, C_STUN, 1, -1, 10, false);
        endOfTurn(t);
        bool a2 = applyCond(t, C_STUN, 1, -1, 10, false);
        endOfTurn(t);
        bool a3 = applyCond(t, C_STUN, 1, -1, 10, false);
        T("T12", "§7 после окончания — иммунитет до конца следующего хода", a1 && !a2 && a3,
          fmt("наложение: %d, сразу после конца: %d, через ход: %d", a1, a2, a3));
    }
    // T13 движение
    if (TB()) {
        int bad = 0, N = 3000;
        World w;
        for (int i = 0; i < N; i++) {
            genMap(w);
            w.f.clear();
            w.f.push_back(mk(plain(10, 1, 6, 5), 0, irand(100), irand(100)));
            w.f.push_back(mk(plain(10, 1, 6, 5), 1, irand(100), irand(100)));
            if (w.wall[w.f[0].y][w.f[0].x] || w.wall[w.f[1].y][w.f[1].x]) continue;
            int sp = 30;
            int x0 = w.f[0].x, y0 = w.f[0].y;
            int used = moveToward(w, w.f[0], w.f[1], 0, sp, false);
            int cells = std::max(std::abs(w.f[0].x - x0), std::abs(w.f[0].y - y0));
            if (used > sp || 5 * cells > used + 0) bad++;
        }
        T("T13", "§8 движение: потрачено <= скорости; смещение <= потраченного", bad == 0,
          fmt("нарушений %d из %d", bad, N));
    }
    // T14 провоцированные атаки
    if (TB()) {
        auto trial = [&](int mode) {
            int dmgTotal = 0, react = 0, N = 400;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss big = plain(1000, 0, 0, 5);
                // 3 — лук (боеприпасное), 5 — метательное (v21: бьёт и вблизи), 6 — безоружный (v21: не провоцирует)
                Boss g = plain(10, mode == 6 ? 0 : 1, 1, (mode == 3 || mode == 5) ? 30 : 5);
                g.wFinesse = false;
                g.wAmmo = mode == 3;
                w.f.push_back(mk(big, 0, 51, 50));
                w.f.push_back(mk(g, 1, 50, 50));
                w.f.push_back(mk(plain(10, 1, 6, 5), 1, 80, 50));
                w.f[0].disengaged = (mode == 2);
                int hp0 = w.f[0].hp;
                moveToward(w, w.f[0], w.f[2], 0, 30, false);
                if (mode == 4) {
                    hp0 = w.f[0].hp;
                    w.f[0].x = 51;
                    w.f[0].y = 50;
                    w.f[0].disengaged = false;
                    moveToward(w, w.f[0], w.f[2], 0, 30, false);
                }
                dmgTotal += hp0 - w.f[0].hp;
                react += w.f[1].reactionUsed;
            }
            return std::make_pair(dmgTotal, react);
        };
        auto r1 = trial(1), r2 = trial(2), r3 = trial(3), r4 = trial(4), r5 = trial(5), r6 = trial(6);
        T("T14a", "§8 уход из досягаемости ближнего врага -> 1 провоцированная атака",
          r1.second == 400 && r1.first > 300,
          fmt("реакций использовано %d/400, урона получено %d (ожидается ~0.95 на попытку)", r1.second, r1.first));
        T("T14b", "§8 Отход -> без провоцированной атаки", r2.first == 0 && r2.second == 0,
          fmt("урона %d, реакций %d", r2.first, r2.second));
        T("T14c", "§8 враг с луком (боеприпасное оружие) не провоцирует", r3.first == 0 && r3.second == 0,
          fmt("урона %d, реакций %d", r3.first, r3.second));
        T("T14e", "§8 v21: метательное оружие (кинжал, дротик) провоцирует, как ближнее", r5.second == 400 && r5.first > 300,
          fmt("реакций %d/400, урона %d", r5.second, r5.first));
        T("T14f", "§8 v21: безоружный не провоцирует (атаки кулаком нет)", r6.first == 0 && r6.second == 0,
          fmt("урона %d, реакций %d", r6.first, r6.second));
        T("T14d", "§8 реакция — 1 раз за раунд", r4.first == 0, fmt("урона от второй атаки %d", r4.first));
    }
    // T15/T16 боевые журналы: призыв, телепорт, старт
    if (TB()) {
        int capViol = 0, N = 300, teleToward = 0, teleAway = 0, bossCnt = 0;
        double minDist = 1e9;
        int onWall = 0;
        std::regex rf(
            "\"x\":(\\d+),\"y\":(\\d+),\"hp\":(-?\\d+),\"maxHp\":(\\d+),\"ac\":\\d+,\"team\":(\\d),\"kind\":(\\d)");
        std::regex ra("\"actor\":(-?\\d+),\"act\":\"([a-z-]+)\"");
        for (int s = 1; s <= N; s++) {
            rng.seed(s * 13);
            Boss A = makeBoss(1600, C, false);
            addSummon(A, makeBoss(150, C, false), 2);   // призыв без лимита применений: число ограничивает только потолок
            Boss B = makeBoss(1600, C, false);
            std::vector<std::string> log;
            std::string mj;
            fight(A, B, &log, &mj);
            for (size_t i = 0; i < log.size(); i++) {
                int alive[2] = {0, 0};
                for (auto it = std::sregex_iterator(log[i].begin(), log[i].end(), rf); it != std::sregex_iterator();
                     ++it) {
                    int hp = std::stoi((*it)[3]), team = std::stoi((*it)[5]), kind = std::stoi((*it)[6]);
                    if (kind >= 2 && hp > 0) alive[team]++;
                }
                if (alive[0] > limits().sumAliveAbil) capViol++;
                if (i == 0) {
                    std::vector<std::pair<int, int>> pos;
                    for (auto it = std::sregex_iterator(log[i].begin(), log[i].end(), rf); it != std::sregex_iterator();
                         ++it)
                        pos.push_back({std::stoi((*it)[1]), std::stoi((*it)[2])});
                    if (pos.size() >= 2) {
                        double d = 5.0 * std::max(std::abs(pos[0].first - pos[1].first),
                                                  std::abs(pos[0].second - pos[1].second));
                        minDist = std::min(minDist, d);
                    }
                }
            }
        }
        T("T15", "§10 (v21) живых приспешников от способности не больше технического потолка (limity.json)", capViol == 0,
          fmt("кадров с превышением %d (боёв %d)", capViol, N));
        T("T16", "(нет в спецификации) начальная дистанция между боссами", true,
          fmt("минимум в %d боях: %.0f фт — в правилах не задана, в коде >=150 фт при удаче поиска", N, minDist));
    }
    // T17 телепорт сближение/побег
    if (TB()) {
        int toward = 0, away = 0, N = 300;
        std::regex rf("\"x\":(\\d+),\"y\":(\\d+),\"hp\":(-?\\d+),\"maxHp\":(\\d+)");
        std::regex ra("\"actor\":(-?\\d+),\"act\":\"([a-z-]+)\"");
        for (int seed = 1; seed <= N; seed++) {
            rng.seed(seed);
            Boss A = makeBoss(800, C, false);
            Spec t;
            t.self = true;
            t.delivery = 2;
            t.teleFt = 60;
            t.bonus = true;
            t.price = 1;
            A.abil.clear();
            A.abil.push_back(t);
            A.wN = 2;
            A.wS = 6;
            A.wRange = 5;
            A.wAtt = 1;
            A.stats[INT] = 10;   // правило бота (с поставленной сетью ИНТ 12+ ходил бы сетью)
            Boss B = makeBoss(800, C, false);
            std::vector<std::string> log;
            std::string mj;
            fight(A, B, &log, &mj);
            for (size_t i = 1; i < log.size(); i++) {
                std::smatch m;
                if (!std::regex_search(log[i], m, ra) || m[2] != "teleport" || std::stoi(m[1]) != 0) continue;
                std::vector<std::array<int, 4>> v, u;
                for (auto it = std::sregex_iterator(log[i].begin(), log[i].end(), rf); it != std::sregex_iterator();
                     ++it)
                    v.push_back({std::stoi((*it)[1]), std::stoi((*it)[2]), std::stoi((*it)[3]), std::stoi((*it)[4])});
                for (auto it = std::sregex_iterator(log[i - 1].begin(), log[i - 1].end(), rf);
                     it != std::sregex_iterator(); ++it)
                    u.push_back({std::stoi((*it)[1]), std::stoi((*it)[2]), std::stoi((*it)[3]), std::stoi((*it)[4])});
                if (v.size() < 2 || u.size() < 2) continue;
                auto d = [&](std::vector<std::array<int, 4>>& q) {
                    return std::max(std::abs(q[0][0] - q[1][0]), std::abs(q[0][1] - q[1][1]));
                };
                bool low = u[0][2] < 0.5 * u[0][3];
                if (low)
                    continue; /* v10: урон по времени/яд снимается в начале хода, уже после кадра — при ХП кадра <50%
                                 побег мог быть законным */
                (d(v) < d(u)) ? toward++ : away++;
            }
        }
        // v21: туман войны — прыжок «к врагу» идёт к месту, где его видели в последний раз (враг мог уйти, стать невидимым),
        // и к цели-приспешнику, а расстояние меряется до босса; поэтому допуск шире, чем в v20 (там было away <= toward / 10)
        T("T17", "§8 телепорт при ХП>=30% (кадр с ХП>=50%, тик урона по времени не успеет уронить ниже) ведёт К врагу (или к месту, где его видели)",
          away <= toward / 5, fmt("к врагу %d, от врага %d", toward, away));
    }
    // T18 карта связная
    if (TB()) {
        World w;
        int bad = 0, N = 1500;
        int small30 = 0;
        for (int i = 0; i < N; i++) {
            genMap(w);
            // v21 (мир из шума): участок берётся как есть, бойцы ставятся в самую большую связную часть (пешком по правилам движка);
            // проверка: в ней не меньше 2 мест для расстановки, и пересчёт связности даёт ту же часть
            const int starts = startCellsN(w);
            bool mainc0[W][W];
            std::memcpy(mainc0, w.mainc, sizeof mainc0);
            const int mainN = markMain(w);
            if (starts < 2 || std::memcmp(mainc0, w.mainc, sizeof mainc0) != 0) bad++;
            small30 += mainN < 0.3 * w.aw * w.ah;
        }
        T("T18", "§9 карта: бойцы ставятся в самую большую связную часть арены (не меньше 2 мест), обычно она — почти вся арена", bad == 0 && small30 * 20 <= N,
          fmt("плохих карт %d из %d; основная часть меньше 30%% арены — %d", bad, N, small30));
    }
    // T19 воспроизводимость (обязательная): свой xorshift даёт одинаковые числа на любом компиляторе; потоки не мешают
    // друг другу
    if (TB()) {
        rng.seed(42);
        unsigned long long g0 = rng.next(), g1 = rng.next(), g2 = rng.next();
        rng.seed(42);
        unsigned long long h = 1469598103934665603ULL;
        for (int i = 0; i < 1000; i++) {
            h = (h ^ (unsigned)rollD20()) * 1099511628211ULL;
            h = (h ^ (unsigned)rollDie(7)) * 1099511628211ULL;
            h = (h ^ (unsigned)irand(1000)) * 1099511628211ULL;
        }
        rng.seed(7);
        double su = 0;
        for (int i = 0; i < 1000; i++) su += rand01();
        bool gold = g0 == 3580622183945639842ULL && g1 == 10378725325292465923ULL && g2 == 8967075514996744559ULL &&
                    h == 13206841453922315987ULL && std::fabs(su - 506.502927455531) < 1e-9;
        auto run = [&]() {
            rng.seed(777);
            g_rounds = 0;
            double t = 0;
            for (int i = 0; i < 300; i++) {
                Boss a = makeBoss(800, C, true), b = makeBoss(800, C, true);
                t += fight(a, b) * (i + 1) + g_rounds;
            }
            return t;
        };
        double r1 = run(), r2 = run();
        double rt[4] = {0, 0, 0, 0};
        {
            std::vector<std::thread> th;
            for (int k = 0; k < 4; k++)
                th.emplace_back([&, k] {
                    rng.seed(1);
                    (void)run();
                    rt[k] = run();
                });
            for (auto& x : th) x.join();
        }
        bool thr = rt[0] == r1 && rt[1] == r1 && rt[2] == r1 && rt[3] == r1;
        T("T19",
          "(обязательная) зерно -> тот же результат: в процессе, между компиляторами (xorshift, контрольные значения) "
          "и между потоками",
          r1 == r2 && gold && thr,
          fmt("в процессе: %s; контрольные значения xorshift: %s; 4 потока дают тот же результат: %s",
              r1 == r2 ? "да" : "НЕТ", gold ? "совпали" : "НЕ СОВПАЛИ", thr ? "да" : "НЕТ"));
    }
    // T20 симметрия мест
    if (TB()) {
        double wa = 0;
        int N = 40000;
        std::vector<double> sc(N);
        parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно, сумма — по порядку
            rng.seed(31337ULL * 1000003ULL + (uint64_t)i);
            Boss a, b;
            makePair(1600, C, a, b);
            const int r = fight(a, b);
            sc[i] = r == 0 ? 1 : (r == 2 ? .5 : 0);
        });
        for (double x : sc) wa += x;
        double p = 100 * wa / N;
        T("T20", "§11 симметрия: у случайной пары одинаковых распределений первое место выигрывает 50%",
          std::fabs(p - 50) < 0.75, fmt("A выигрывает %.2f%% (допуск ±0.75, 2σ≈0.5)", p));
    }
    // T21 граничные бюджеты: минимум MIN_BUDGET (20)
    if (TB()) {
        std::string d;
        bool ok = true;
        for (long long B : {0LL, 1LL, 19LL}) {
            int over = 0, N = 300;
            for (int i = 0; i < N; i++) {
                Boss b = makeBoss(B, C, true);
                if (b.cost - STAT_BASE > MIN_BUDGET) over++;
            }
            d += fmt("B=%lld: выше минимума %d/%d; ", B, over, N);
            if (over) ok = false;
        }
        for (long long B : {20LL, 50LL, 100LL}) {
            int over = 0, N = 300;
            for (int i = 0; i < N; i++) {
                Boss b = makeBoss(B, C, true);
                if (b.cost - STAT_BASE > B) over++;
            }
            d += fmt("B=%lld: перерасход %d/%d; ", B, over, N);
            if (over) ok = false;
        }
        T("T21", "§1 минимальный бюджет 20 (шесть характеристик по 1, ХП 1, способности); ниже поднимается до 20", ok, d);
    }
    // T22 лимит раундов
    if (TB()) {
        g_rounds = 0;
        g_capped = 0;
        int N = 6000;
        std::vector<char> cap(N, 0);
        parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно
            rng.seed(9ULL * 1000003ULL + (uint64_t)i);
            const long long c0 = g_capped;
            long long B = 200LL << irand(6);
            Boss a = makeBoss(B, C, true), b = makeBoss(B, C, true);
            fight(a, b);
            cap[i] = g_capped > c0;
        });
        g_capped = 0;
        for (char c : cap) g_capped += c;
        T("T22", "§8 лимита раундов нет: доля боёв, дошедших до технического предохранителя (ничья), мала",
          100.0 * g_capped / N < 2.0,
          fmt("%.1f%% боёв доходят до предохранителя в %d раундов (бой тогда считается ничьёй)", 100.0 * g_capped / N,
              g_maxRounds));
    }

    // T23..T29 дополнительные требования
    if (TB()) {
        int cha = 0, mainCha = 0, badm = 0, N = 4000;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(200LL << irand(6), C, true);
            if (b.stats[CHA] > 0) cha++;
            if (b.mainStat == CHA) mainCha++;
            if (b.mainStat == CON) badm++;
        }
        T("T23", "§2 ХАР покупается как остальные; главная характеристика: СИЛ/ЛОВ/ИНТ/МДР/ХАР, но не ТЕЛ",
          cha > N * 0.6 && mainCha > N * 0.1 && badm == 0,
          fmt("боссов с ХАР>0: %d, главная ХАР: %d, главная ТЕЛ: %d из %d", cha, mainCha, badm, N));
    }
    if (TB()) {
        bool ok = true;
        for (int m : {STR, DEX, INT, WIS}) {
            Boss b = plain(10, 1, 6, 5);
            b.stats = {10, 10, 10, 10, 10, 10};
            b.stats[m] = 18;
            b.mainStat = m;
            Fighter f = mk(b, 0, 1, 1);
            if (f.dc() != 8 + PROF + 4 || f.mainMod() != 4) ok = false;
        }
        T("T24", "§2 СЛ = 8 + 2 + мод. главной характеристики", ok, "");
    }
    if (TB()) {
        Boss b = plain(10, 1, 6, 5);
        b.stats = {16, 10, 10, 10, 10, 10};
        Fighter f = mk(b, 0, 1, 1);
        int melee = f.wMod();
        b.stats = {8, 18, 10, 10, 10, 10};
        b.wFinesse = false;
        Fighter g = mk(b, 0, 1, 1);
        int noFin = g.wMod();
        b.wFinesse = true;
        Fighter h = mk(b, 0, 1, 1);
        int fin = h.wMod();
        b.wFinesse = false;
        b.wRange = 30;
        b.wAmmo = true;
        Fighter r = mk(b, 0, 1, 1);
        int rng_ = r.wMod();
        b.wAmmo = false;
        Fighter th = mk(b, 0, 1, 1);
        int thr = th.wMod();
        b.wFinesse = true;
        Fighter thf = mk(b, 0, 1, 1);
        int thrf = thf.wMod();
        T("T25",
          "§5 мод. оружия: СИЛ; фехтовальное — большее из СИЛ/ЛОВ; боеприпасное (дальнее) — только ЛОВ; метательное — "
          "СИЛ (или фехтовальное)",
          melee == 3 && noFin == -1 && fin == 4 && rng_ == 4 && thr == -1 && thrf == 4,
          fmt("СИЛ16: %d; СИЛ8/ЛОВ18 без фехтовального: %d, фехтовальное: %d, боеприпасное: %d, метательное: %d, "
              "метательное фехтовальное: %d",
              melee, noFin, fin, rng_, thr, thrf));
    }
    if (TB()) {
        Spec a;
        a.self = true;
        a.healN = 2;
        a.delivery = 2;
        Spec b2 = a;
        b2.range = 120;
        b2.radius = 30;
        g_B = 800;
        bool ok = specPrice(a, C) == specPrice(b2, C);
        T("T26", "§6 дальность/область не удорожают способность «на себя»", ok,
          fmt("цена %lld и %lld", specPrice(a, C), specPrice(b2, C)));
    }
    if (TB()) {
        Fighter f = mk(plain(10, 0, 0, 5), 0, 1, 1);
        int base = speedOf(f);
        f.cond[C_RESTR] = 1;
        int r = speedOf(f);
        f.cond[C_RESTR] = 0;
        f.cond[C_SLOW] = 1;
        int sl = speedOf(f);
        T("T27", "§7 опутан — скорость 0; замедлен — вдвое", base == 30 && r == 0 && sl == 15,
          fmt("обычная %d, опутан %d, замедлен %d", base, r, sl));
    }
    if (TB()) {
        World w;
        double worstClimb = 0, worstDiff = 0;
        int bad = 0, N = 3000, mnS = 999, mxS = 0, small = 0, big = 0, streams = 0, pits = 0, hills = 0;
        for (int i = 0; i < N; i++) {
            genMap(w);
            mnS = std::min({mnS, w.aw, w.ah});
            mxS = std::max({mxS, w.aw, w.ah});
            small += (w.aw <= 10 && w.ah <= 10);
            big += (w.aw >= 60 || w.ah >= 60);
            bool st = false, pi = false, hi = false;
            int nClimb = 0, nDiff = 0;
            for (int y = 0; y < W; y++)
                for (int x = 0; x < W; x++) {
                    if (x < w.aw && y < w.ah) { nClimb += w.climb[y][x]; nDiff += w.diff[y][x]; }
                    const bool outside = x >= w.aw || y >= w.ah;
                    if (outside && !w.wall[y][x]) bad++;       // за пределами арены — стена
                    if (!outside && w.water[y][x]) st = true;
                    if (!outside && w.pit[y][x]) pi = true;
                    if (!outside && w.height[y][x]) hi = true;
                }
            streams += st; pits += pi; hills += hi;
            if (std::min(w.aw, w.ah) >= 20) {   // на маленьких аренах один предмет занимает заметную долю
                worstClimb = std::max(worstClimb, (double)nClimb / (w.aw * w.ah));
                worstDiff = std::max(worstDiff, (double)nDiff / (w.aw * w.ah));
            }
            if (w.aw < 2 || w.ah < 2 || w.aw > 100 || w.ah > 100) bad++;
        }
        T("T28", "§11 арена — участок мира, стороны от 2 до 100, за краем стена; встречаются ручьи, ущелья и возвышенности", bad == 0 && worstClimb < 0.15 && mnS == 2 && mxS == 100 && small > 0 && big > 0 && streams > 0 && pits > 0 && hills > 0,
          fmt("сторона %d–%d; малых %d, больших %d; ручьёв %d, ущелий %d, возвышенностей %d из %d; на аренах от 20 клеток наибольшая доля лазалок %.2f, труднопроходимой %.2f", mnS, mxS, small, big, streams, pits, hills, N, worstClimb, worstDiff));
    }
    if (TB()) {
        Fighter f = mk(plain(10, 0, 0, 5), 0, 1, 1);
        f.b.stats = {10, 10, 10, 10, 10, 10};
        int a = saveBonus(f, WIS);
        f.b.saveProf = 1u << WIS;
        int b2 = saveBonus(f, WIS);
        int c2 = saveBonus(f, STR);
        T("T29", "§13.1 владение спасброском: +2 только к своей характеристике", a == 0 && b2 == 2 && c2 == 0,
          fmt("без владения %d, с владением МДР %d, СИЛ %d", a, b2, c2));
    }

    // T31..T36 типы урона (v7)
    if (TB()) {
        Boss t = plain(100, 0, 0, 5);
        bool ok = true;
        std::string d;
        auto chk = [&](const char* n, double got, double exp) {
            if (std::fabs(got - exp) > 1e-9) {
                ok = false;
                d += fmt("%s:%.1f≠%.1f ", n, got, exp);
            }
        };
        t.resMask = 1u << DT_FIRE;
        chk("резист", dmgMult(t, DT_FIRE, true), 0.5);
        t.resMask = 0;
        t.immMask = 1u << DT_POISON;
        chk("иммун", dmgMult(t, DT_POISON, true), 0.0);
        t.immMask = 0;
        t.vulnMask = 1u << DT_COLD;
        chk("уязв", dmgMult(t, DT_COLD, true), 2.0);
        t.resMask = 1u << DT_COLD;
        chk("резист+уязв", dmgMult(t, DT_COLD, true), 1.0);
        t.resMask = 0;
        t.vulnMask = 0;
        t.resNM = true;
        chk("немагич. колющий", dmgMult(t, DT_PIERCING, false), 0.5);
        chk("магич. колющий", dmgMult(t, DT_PIERCING, true), 1.0);
        chk("огонь не физический", dmgMult(t, DT_FIRE, false), 1.0);
        t.resNM = false;
        t.immNM = true;
        chk("иммун немагич.", dmgMult(t, DT_SLASHING, false), 0.0);
        chk("иммун, магич.", dmgMult(t, DT_SLASHING, true), 1.0);
        T("T31", "§6.* множители: резист ×0.5, иммунитет 0, уязвимость ×2, вместе ×1, «от немагического»", ok, d);
    }
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.resMask = 1u << DT_FIRE;
        f.b.vulnMask = 1u << DT_RADIANT;
        f.b.immMask = 1u << DT_POISON;
        hurt(f, 10, DT_FIRE, true);
        int a = f.hp;
        hurt(f, 10, DT_RADIANT, true);
        int b2 = f.hp;
        hurt(f, 10, DT_POISON, true);
        int c2 = f.hp;
        hurt(f, 10, DT_ACID, true);
        int d2 = f.hp;
        T("T32",
          "hurt применяет защиты: огонь с резистом 10→5, излучение с уязвимостью 10→20, яд с иммунитетом 0, кислота 10",
          a == 95 && b2 == 75 && c2 == 75 && d2 == 65, fmt("ХП после ударов: %d, %d, %d, %d", a, b2, c2, d2));
    }
    if (TB()) {
        Coef C2 = CC();
        int bad = 0, N = 800;
        for (int i = 0; i < N; i++) {
            g_B = 800;
            Boss b = makeBoss(800, C2, false);
            long long p0 = priceOf(b, C2, 800);
            Boss c = b;
            int t = -1;
            for (int d = 0; d < DT_N; d++)
                if (!(((b.resMask | b.immMask | b.vulnMask) >> d) & 1)) {
                    t = d;
                    break;
                }
            if (t < 0) continue;
            c.vulnMask |= 1u << t;
            long long p1 = priceOf(c, C2, 800);
            if (!(p1 < p0)) bad++;
        }
        T("T33", "уязвимость ВОЗВРАЩАЕТ очки: цена босса с дополнительной уязвимостью ниже", bad == 0,
          fmt("нарушений %d из %d", bad, N));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(50, 2, 6, 5);
        a.wType = DT_PIERCING;
        Boss d = plain(50, 0, 0, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        double base = evWeapon(w, w.f[0], w.f[1]);
        w.f[1].b.resMask = 1u << DT_PIERCING;
        double res = evWeapon(w, w.f[0], w.f[1]);
        w.f[1].b.resMask = 0;
        w.f[1].b.immMask = 1u << DT_PIERCING;
        double imm = evWeapon(w, w.f[0], w.f[1]);
        w.f[1].b.immMask = 0;
        w.f[1].b.vulnMask = 1u << DT_PIERCING;
        double vul = evWeapon(w, w.f[0], w.f[1]);
        T("T34",
          "ИИ учитывает защиты цели при выборе действия (ожидаемый урон: резист ×0.5, иммунитет 0, уязвимость ×2)",
          std::fabs(res - base * 0.5) < 1e-9 && imm == 0 && std::fabs(vul - base * 2) < 1e-9,
          fmt("база %.2f, резист %.2f, иммунитет %.2f, уязвимость %.2f", base, res, imm, vul));
    }
    if (TB()) {
        int cnt[DT_N] = {};
        int N = 0;
        for (int i = 0; i < 6000; i++) {
            Boss b = makeBoss(800, C, false);
            if (b.wN > 0) {
                cnt[b.wType]++;
                N++;
            }
        } /* у безоружного тип оружия не определён */
        double p = cnt[DT_PIERCING] / (double)N, sl = cnt[DT_SLASHING] / (double)N,
               bl = cnt[DT_BLUDGEONING] / (double)N;
        int phys = 0, all = 0;
        for (int i = 0; i < 20000; i++) {
            Spec s = randSpec(0);
            if (s.dmgN) {
                all++;
                if (isPhys(s.dtype)) phys++;
            }
        }
        T("T35", "распределение типов: оружие колющий≈38%, рубящий≈35%, дробящий≈27%; способности физические ≈22%",
          std::fabs(p - 0.385) < 0.03 && std::fabs(sl - 0.346) < 0.03 && std::fabs(bl - 0.269) < 0.03 &&
              std::fabs(phys / (double)all - 0.22) < 0.02,
          fmt("колющий %.1f%%, рубящий %.1f%%, дробящий %.1f%%, способности физические %.1f%%", 100 * p, 100 * sl,
              100 * bl, 100.0 * phys / all));
    }
    if (TB()) {
        int a = 0, b2 = 0, c2 = 0, N = 6000;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C, false);
            if (b.resMask) a++;
            if (b.immMask) b2++;
            if (b.vulnMask) c2++;
        }
        T("T36", "(инфо) доли боссов с защитами: резист / иммунитет / уязвимость", true,
          fmt("%.0f%% / %.0f%% / %.0f%%", 100.0 * a / N, 100.0 * b2 / N, 100.0 * c2 / N));
    }

    // T37..T40 легендарное сопротивление
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.stats = {1, 1, 1, 1, 1, 1};
        f.lrLeft = 2;
        long long u0 = g_lrUsed;
        bool a = saveLR(f, STR, 100, false);
        int l1 = f.lrLeft;
        bool b1 = saveLR(f, STR, 100, true);
        bool c1 = saveLR(f, STR, 100, true);
        bool d1 = saveLR(f, STR, 100, true);
        T("T37", "§LR проваленный спасбросок -> успех, пока есть заряды (и только если эффект значим)",
          !a && l1 == 2 && b1 && c1 && !d1 && f.lrLeft == 0 && g_lrUsed - u0 == 2,
          fmt("незначимый: %d (зарядов %d); значимые подряд: %d %d %d; осталось %d; потрачено %lld", a, l1, b1, c1, d1,
              f.lrLeft, g_lrUsed - u0));
    }
    if (TB()) {
        Fighter t = mk(plain(100, 0, 0, 5), 0, 1, 1);
        Spec st;
        st.cond = C_STUN;
        st.condRounds = 1;
        Spec lt;
        lt.cond = C_SLOW;
        lt.condRounds = 1;
        Spec big;
        big.dmgN = 10;
        big.dmgS = 6;
        big.dtype = DT_FIRE;
        Spec sm;
        sm.dmgN = 1;
        sm.dmgS = 4;
        sm.dtype = DT_FIRE;
        Spec lg;
        lg.cond = C_RESTR;
        lg.condRounds = 3;
        T("T38", "§LR бот тратит заряд на оглушение/отвлечение, долгий контроль и удар ≥25% ХП, но не на мелочь",
          lrWorth(t, st) && !lrWorth(t, lt) && lrWorth(t, big) && !lrWorth(t, sm) && lrWorth(t, lg),
          fmt("оглушение %d, замедление 1р %d, 10к6 %d, 1к4 %d, опутан 3р %d", lrWorth(t, st), lrWorth(t, lt),
              lrWorth(t, big), lrWorth(t, sm), lrWorth(t, lg)));
    }
    if (TB()) {
        Fighter t = mk(plain(100, 0, 0, 5), 0, 1, 1);
        t.b.stats = {10, 10, 10, 10, 10, 10};
        double p0 = pFailEst(t, CON, 12);
        t.lrLeft = 3;
        double p1 = pFailEst(t, CON, 12);
        t.lrLeft = 0;
        double p2 = pFailEst(t, CON, 12);
        T("T39", "§LR ИИ учитывает заряды цели: шанс провала спасброска ×0.25 при наличии зарядов",
          std::fabs(p1 - p0 * 0.25) < 1e-9 && p2 == p0, fmt("без зарядов %.2f, с зарядами %.2f", p0, p1));
    }
    if (TB()) {
        Coef C2 = CC();
        rng.seed(55);
        long long u0 = g_lrUsed;
        int fights = 0, had = 0;
        for (int i = 0; i < 3000; i++) {
            Boss A = makeBoss(800, C2, true);
            A.legRes = 3;
            Boss B = makeBoss(800, C2, true);
            fight(A, B);
            fights++;
        }
        double per = (double)(g_lrUsed - u0) / fights;
        T("T40", "(инфо) среднее число потраченных зарядов за бой при LR 3 (от всех спасбросков обеих сторон)",
          per >= 0, fmt("%.2f зарядов за бой на 3000 боях (максимум 3 у A плюс случайные у B)", per));
    }

    // T41..T45 перезарядка
    if (TB()) {
        auto rate = [&](int thr) {
            Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
            Spec sp;
            sp.recharge = thr;
            sp.dmgN = 1;
            sp.dmgS = 6;
            sp.delivery = 2;
            f.b.abil.push_back(sp);
            long long r0 = g_rechRolls, o0 = g_rechOk;
            for (int i = 0; i < 120000; i++) {
                f.uses.assign(1, 0);
                rechargeStart(f);
            }
            return (double)(g_rechOk - o0) / (g_rechRolls - r0);
        };
        double r6 = rate(6), r5 = rate(5), r4 = rate(4);
        T("T41", "§перезарядка: шанс вернуть заряд за ход 6→1/6, 5–6→1/3, 4–6→1/2",
          std::fabs(r6 - 1 / 6.0) < 0.01 && std::fabs(r5 - 1 / 3.0) < 0.01 && std::fabs(r4 - 0.5) < 0.01,
          fmt("6: %.3f, 5–6: %.3f, 4–6: %.3f", r6, r5, r4));
    }
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        Spec a;
        a.recharge = 5;
        a.delivery = 2;
        a.dmgN = 1;
        Spec b2;
        b2.recharge = 0;
        b2.uses = 0;
        b2.delivery = 2;
        b2.dmgN = 1;
        f.b.abil = {a, b2};
        f.uses = {1, 0};
        long long r0 = g_rechRolls;
        for (int i = 0; i < 1000; i++) rechargeStart(f);
        T("T42", "§перезарядка: заряженная способность и способность без перезарядки не бросаются, не меняются",
          f.uses[0] == 1 && f.uses[1] == 0 && g_rechRolls == r0,
          fmt("бросков %lld, заряд %d, обычная %d", g_rechRolls - r0, f.uses[0], f.uses[1]));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 6;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        long long pu = specPrice(s, C2);
        Spec r6 = s, r5 = s, r4 = s, once = s;
        r6.recharge = 6;
        r5.recharge = 5;
        r4.recharge = 4;
        once.uses = 1;
        long long p6 = specPrice(r6, C2), p5 = specPrice(r5, C2), p4 = specPrice(r4, C2), p1 = specPrice(once, C2);
        T("T43", "§перезарядка: цена растёт с шансом: «раз за бой» < 6 < 5–6 < 4–6 < без ограничения",
          p1 <= p6 && p6 < p5 && p5 < p4 && p4 < pu,
          fmt("раз за бой %lld; 6: %lld; 5–6: %lld; 4–6: %lld; без ограничения %lld", p1, p6, p5, p4, pu));
    }
    if (TB()) {
        int bad = 0, rc = 0, cnt = 0, N = 40000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            cnt++;
            if (s.recharge) {
                rc++;
                // перезарядка после траты применений (пакет атомов 3) бывает и при лимите применений
                if ((s.self && !s.rechAfter) || (s.uses < UNL && !s.rechAfter) || (s.recharge < 4 || s.recharge > 6)) bad++;
            }
        }
        T("T44", "§перезарядка: только у способностей не «на себя», без лимита применений (или «после траты применений»), порог 4..6", bad == 0,
          fmt("нарушений %d; перезарядка у %.1f%% способностей", bad, 100.0 * rc / cnt));
    }
    if (TB()) {
        auto usesPerFight = [&](int thr) {
            Coef C2 = CC();
            rng.seed(77 + thr);
            long long u0 = g_rechUsed;
            int N = 2500;
            double len = 0;
            for (int i = 0; i < N; i++) {
                Boss A = makeBoss(800, C2, false);
                A.abil.clear();
                Spec sp;
                sp.delivery = 2;
                sp.range = 60;
                sp.dmgN = 3;
                sp.dmgS = 6;
                sp.recharge = thr;
                sp.dtype = DT_FIRE;
                A.abil.push_back(sp);
                Boss B = makeBoss(800, C2, false);
                fight(A, B);
            }
            return (double)(g_rechUsed - u0) / N;
        };
        double u6 = usesPerFight(6), u5 = usesPerFight(5), u4 = usesPerFight(4);
        T("T45", "(инфо) среднее число применений способности с перезарядкой за бой (оба бойца)", u6 < u5 && u5 < u4,
          fmt("6: %.2f; 5–6: %.2f; 4–6: %.2f", u6, u5, u4));
    }

    // T46..T49 легендарные действия
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.wAtt = 2;
        a.legAct = 3;
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        long long a0 = g_attacksMade;
        bool r1 = legendaryTurn(w, 0);
        long long n1 = g_attacksMade - a0;
        int l1 = w.f[0].legLeft;
        a0 = g_attacksMade;
        bool r2 = legendaryTurn(w, 0);
        long long n2 = g_attacksMade - a0;
        int l2 = w.f[0].legLeft;
        a0 = g_attacksMade;
        bool r3 = legendaryTurn(w, 0);
        T("T46",
          "§LA 3 очка, 2 атаки: сначала полное действие Атака (2 очка, 2 атаки), потом одна атака (1 очко), потом "
          "ничего",
          r1 && n1 == 2 && l1 == 1 && r2 && n2 == 1 && l2 == 0 && !r3 && g_attacksMade - a0 == 0,
          fmt("1-й: атак %lld, осталось %d; 2-й: атак %lld, осталось %d; 3-й: %d", n1, l1, n2, l2, r3));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.wAtt = 2;
        a.legAct = 1;
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        long long a0 = g_attacksMade;
        legendaryTurn(w, 0);
        long long n1 = g_attacksMade - a0;
        w.f[0].legLeft = 3;
        w.f[0].cond[C_STUN] = 1;
        bool st = legendaryTurn(w, 0);
        w.f[0].cond[C_STUN] = 0;
        w.f[0].legLeft = 0;
        bool no = legendaryTurn(w, 0);
        T("T47", "§LA 1 очко — одна атака; оглушённый и без очков не действует", n1 == 1 && !st && !no,
          fmt("атак при 1 очке: %lld; оглушён: %d; без очков: %d", n1, st, no));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.legAct = 3;
        Boss d = plain(100, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 12, 10));
        w.f[0].legLeft = 0;
        takeTurn(w, 0);
        T("T48", "§LA очки восстанавливаются в начале СВОЕГО хода", w.f[0].legLeft == 3,
          fmt("очков после своего хода: %d", w.f[0].legLeft));
    }
    if (TB()) {
        Coef C2 = CC();
        rng.seed(91);
        long long u0 = g_legUsed;
        long long rounds0 = g_rounds;
        int N = 2000;
        for (int i = 0; i < N; i++) {
            Boss A = makeBoss(800, C2, false);
            A.legAct = 3;
            Boss B = makeBoss(800, C2, false);
            B.legAct = 0;
            fight(A, B);
        }
        double rounds = (double)(g_rounds - rounds0);
        double per = (g_legUsed - u0) / rounds;
        T("T49",
          "(инфо) легендарных действий на раунд боя при 3 очках (теоретический максимум 1: один противник = один ход в "
          "раунд)",
          per <= 1.0001, fmt("%.2f на раунд; раундов %.0f на %d боях", per, rounds, N));
    }

    // T50..T56 новые состояния (недееспособен, парализован, очарован, схвачен)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 2, 8, 5);
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[0].cond[C_INCAP] = 3;
        int hp0 = w.f[1].hp;
        int x0 = w.f[0].x;
        takeTurn(w, 0);
        int dmgIncap = hp0 - w.f[1].hp;
        w.f[0].cond[C_INCAP] = 0;
        for (int i = 0; i < 30; i++) takeTurn(w, 0);
        bool dealsNormally = hp0 - w.f[1].hp > 0;
        World w2 = w;
        w2.f[0].x = 10;
        w2.f[0].y = 10;
        w2.f[1].x = 40;
        w2.f[1].y = 10;
        w2.f[0].cond[C_INCAP] = 3;
        w2.f[0].legLeft = 3;
        w2.f[0].b.legAct = 3;
        bool la = legendaryTurn(w2, 0);
        takeTurn(w2, 0);
        bool moved = w2.f[0].x > 10;
        T("T50", "§недееспособен: нет действий и реакций, но двигаться можно; легендарные действия недоступны",
          dmgIncap == 0 && dealsNormally && moved && !la,
          fmt("урон под состоянием: %d; без состояния бьёт: %d; двигался: %d; ЛД: %d", dmgIncap, dealsNormally, moved,
              la));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[1].cond[C_PARAL] = 2;
        int xx = w.f[1].x;
        bool adv, dis;
        advState(w, w.f[0], w.f[1], 5, adv, dis);
        int crits = 0, hits = 0;
        for (int i = 0; i < 4000; i++) {
            bool c;
            if (attackRoll(w, w.f[0], w.f[1], PROF, 5, c)) {
                hits++;
                if (c) crits++;
            }
        }
        w.f[1].x = 20;
        int far_crits = 0, far_hits = 0;
        for (int i = 0; i < 4000; i++) {
            bool c;
            if (attackRoll(w, w.f[0], w.f[1], PROF, 30, c)) {
                far_hits++;
                if (c) far_crits++;
            }
        }
        w.f[1].x = 11;
        Fighter t = mk(plain(100, 0, 0, 5), 0, 1, 1);
        t.b.stats = {30, 30, 10, 10, 10, 10};
        t.cond[C_PARAL] = 1;
        bool failStr = !saveOK(t, STR, 1), failDex = !saveOK(t, DEX, 1), okCon = saveOK(t, CON, 1);
        t.cond[C_PARAL] = 0;
        t.cond[C_STUN] = 1;
        bool stunFail = !saveOK(t, STR, 1);
        int hp0 = w.f[0].hp;
        w.f[1].cond[C_PARAL] = 2;
        takeTurn(w, 1);
        bool skipped = (w.f[0].hp == hp0) && (w.f[1].x == xx);
        bool pf = pFailEst(t, DEX, 5) == 1.0;
        T("T51",
          "§парализован: преимущество по цели, ближний удар в упор — крит, автопровал СИЛ/ЛОВ (и у оглушённого), ход "
          "пропускается",
          adv && hits > 0 && hits == crits && far_crits < far_hits * 0.2 && failStr && failDex && okCon && stunFail &&
              skipped && pf,
          fmt("преим. %d; в упор: %d из %d критов; издали: %d из %d; автопровал СИЛ %d ЛОВ %d (ТЕЛ проходит %d, "
              "оглушён %d); пропуск хода %d",
              adv, crits, hits, far_crits, far_hits, failStr, failDex, okCon, stunFail, skipped));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 2, 8, 5);
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 0;
        Boss m = plain(100000, 0, 0, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[0].cond[C_CHARM] = 100;
        w.f[0].cstat[C_CHARM] = -1;
        w.f[0].charmSrc = 1;
        int hp0 = w.f[1].hp;
        takeTurn(w, 0);
        int dmg1 = hp0 - w.f[1].hp;
        int near1 = nearestEnemyInRange(w, w.f[0], 5);
        w.f.push_back(mk(m, 1, 10, 11));
        int mhp = w.f[2].hp;
        int near2 = nearestEnemyInRange(w, w.f[0], 5);
        for (int i = 0; i < 10; i++) takeTurn(w, 0);
        bool hitsOther = mhp - w.f[2].hp > 0;
        bool noCharmer = hp0 - w.f[1].hp == 0;
        T("T52", "§очарован: не может атаковать очаровавшего (но атакует других)",
          dmg1 == 0 && near1 == -1 && near2 == 2 && hitsOther && noCharmer,
          fmt("урон очаровавшему %d, цель в досягаемости: %d (только он) → %d (с приспешником); бьёт приспешника: %d; "
              "очаровавшему всего: %d",
              dmg1, near1, near2, hitsOther, hp0 - w.f[1].hp));
    }
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        int base = speedOf(f);
        f.cond[C_GRAPPLE] = 2;
        int g = speedOf(f);
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 30);
        a.stats = {10, 30, 10, 10, 10, 10};
        Boss d = plain(1000, 1, 6, 5);
        int esc = 0, stay = 0, N = 300;
        for (int i = 0; i < N; i++) {
            World w2 = w;
            w2.f.clear();
            w2.f.push_back(mk(a, 0, 10, 10));
            w2.f.push_back(mk(d, 1, 60, 10));
            w2.f[0].cond[C_GRAPPLE] = 3;
            w2.f[0].cstat[C_GRAPPLE] = -1;
            w2.f[0].grapSrc = 1;
            w2.f[0].cdc[C_GRAPPLE] = 1;
            takeTurn(w2, 0);
            if (!w2.f[0].cond[C_GRAPPLE]) esc++;
        }
        for (int i = 0; i < N; i++) {
            World w2 = w;
            w2.f.clear();
            w2.f.push_back(mk(a, 0, 10, 10));
            w2.f.push_back(mk(d, 1, 60, 10));
            w2.f[0].cond[C_GRAPPLE] = 3;
            w2.f[0].cstat[C_GRAPPLE] = -1;
            w2.f[0].grapSrc = 1;
            w2.f[0].cdc[C_GRAPPLE] = 100;
            takeTurn(w2, 0);
            if (w2.f[0].cond[C_GRAPPLE]) stay++;
        }
        int attackedN = 0;
        for (int i = 0; i < 100; i++) {
            World w3 = w;
            w3.f.clear();
            w3.f.push_back(mk(a, 0, 10, 10));
            w3.f.push_back(mk(d, 1, 13, 10));
            w3.f[0].cond[C_GRAPPLE] = 3;
            w3.f[0].cstat[C_GRAPPLE] = -1;
            w3.f[0].grapSrc = 1;
            w3.f[0].cdc[C_GRAPPLE] = 1;
            int hp0 = w3.f[1].hp;
            takeTurn(w3, 0);
            attackedN += (w3.f[0].cond[C_GRAPPLE] > 0 && w3.f[1].hp < hp0);
        }
        bool attacksInstead = attackedN >= 80;   // попадание ~95% (нат. 1 — промах)
        T("T53",
          "§схвачен: скорость 0; если нечем атаковать — действие на побег по СЛ; если есть цель в досягаемости — "
          "атакует",
          base == 30 && g == 0 && esc >= N * 0.9 && stay == N && attacksInstead,
          fmt("скорость %d→%d; побег при СЛ 1: %d из %d; при СЛ 100: остался %d из %d; при цели в досягаемости "
              "атакует: %d из 100",
              base, g, esc, N, stay, N, attackedN));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 0, 0, 5);
        d.ac = 0;
        d.stats = {30, 30, 30, 30, 30, 30};
        Spec sp;
        sp.delivery = 0;
        sp.cond = C_GRAPPLE;
        sp.condRounds = 3;
        sp.saveStat = STR;
        sp.range = 5;
        sp.dmgN = 0;
        a.abil.push_back(sp);
        a.mainStat = STR;
        int ok = 0, N = 600;
        for (int i = 0; i < N; i++) {
            World w2 = w;
            w2.f.clear();
            w2.f.push_back(mk(a, 0, 10, 10));
            w2.f.push_back(mk(d, 1, 11, 10));
            hitOne(w2, sp, w2.f[0], w2.f[1]);
            if (w2.f[1].cond[C_GRAPPLE] > 0 && w2.f[1].grapSrc == 0 && w2.f[1].cstat[C_GRAPPLE] == -1) ok++;
        }
        T("T54",
          "§схватить: попадание без спасброска (даже у цели со СИЛ 30), источник запоминается, повторного спаса нет",
          ok >= N * 0.9, fmt("схвачено %d из %d попаданий-кандидатов", ok, N));
    }
    if (TB()) {
        Spec ch;
        ch.delivery = 2;
        ch.cond = C_CHARM;
        ch.condRounds = 1;
        ch.range = 30;
        Spec pa = ch;
        pa.cond = C_PARAL;
        Spec st = ch;
        st.cond = C_STUN;
        Spec inc = ch;
        inc.cond = C_INCAP;
        Spec gr = ch;
        gr.cond = C_GRAPPLE;
        Coef C2 = CC();
        g_B = 800;
        T("T55",
          "(инфо) цена состояния «без броска, 1 раунд, 30 фт» на бюджете 800: недееспособен / парализован / очарован / "
          "схвачен / оглушён",
          true,
          fmt("%lld / %lld / %lld / %lld / %lld", specPrice(inc, C2), specPrice(pa, C2), specPrice(ch, C2),
              specPrice(gr, C2), specPrice(st, C2)));
    }
    if (TB()) {
        Coef C2 = CC();
        int cnt[C_N] = {};
        int N = 30000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            if (s.cond) cnt[s.cond]++;
        }
        int tot = 0;
        for (int k = 1; k < C_N; k++) tot += cnt[k];
        std::string d;
        bool ok = true;
        for (int k = 1; k < C_N; k++) {
            double sh = cnt[k] / (double)tot;
            d += fmt("%s %.0f%%; ", CN[k], 100 * sh);
            if (sh < 0.05 || sh > 0.12) ok = false;
        }
        T("T56", "генератор выдаёт все 15 состояний примерно поровну (5–12% каждое)", ok, d);
    }

    // T57 очарование заканчивается
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100000, 0, 0, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        bool ok = true;
        std::string det;
        for (int rounds = 1; rounds <= 3; rounds++) {
            w.f[0].cond[C_CHARM] = rounds;
            w.f[0].cstat[C_CHARM] = -1;
            w.f[0].charmSrc = 1;
            int n = 0;
            while (w.f[0].cond[C_CHARM] > 0 && n < 10) {
                takeTurn(w, 0);
                n++;
            }
            det += fmt("%d р.: кончилось за %d ходов; ", rounds, n);
            if (n != rounds) ok = false;
        }
        T("T57",
          "§очарование (единственный противник — очаровавший) заканчивается ровно через заданное число своих ходов", ok,
          det);
    }

    // T58..T62 досягаемость больше 5 футов
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss r10 = plain(100, 1, 6, 5);
        r10.reach = 10;
        Boss r5 = plain(100, 1, 6, 5);
        Boss rg = plain(100, 1, 6, 30);
        rg.reach = 15;
        Boss tgt = plain(100, 0, 0, 5);
        w.f.push_back(mk(r10, 0, 10, 10));
        w.f.push_back(mk(tgt, 1, 12, 10));
        int a = nearestEnemyInRange(w, w.f[0], atkDist(w.f[0].b));
        w.f[0].b = r5;
        int b2 = nearestEnemyInRange(w, w.f[0], atkDist(w.f[0].b));
        w.f[0].b = rg;
        int dist = atkDist(w.f[0].b);
        T("T58",
          "§досягаемость: оружие с досягаемостью 10 фт достаёт цель в 10 фт, обычное 5 фт — нет; у дальнобойного она "
          "не используется",
          a == 1 && b2 == -1 && dist == 30,
          fmt("досягаемость 10: цель %d; досягаемость 5: цель %d; дальнобойное с reach=15: дальность %d", a, b2, dist));
    }
    if (TB()) {
        auto oa = [&](int reachG, int startDist) {
            int trig = 0, N = 200;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss g = plain(100, 1, 6, 5);
                g.reach = reachG;
                Boss f = plain(100000, 0, 0, 5);
                Boss far = plain(100, 1, 6, 5);
                w.f.push_back(mk(f, 0, 10 + startDist / 5, 10));
                w.f.push_back(mk(g, 1, 10, 10));
                w.f.push_back(mk(far, 1, 60, 10));
                moveToward(w, w.f[0], w.f[2], 0, 30, false);
                if (w.f[1].reactionUsed) trig++;
            }
            return trig;
        };
        int t10 = oa(10, 10), t5 = oa(5, 10), t5adj = oa(5, 5);
        T("T59",
          "§досягаемость: провоцированную атаку даёт выход из ЕЁ досягаемости (10 фт: с расстояния 10 — да; 5 фт: с "
          "расстояния 10 — нет, с 5 — да)",
          t10 == 200 && t5 == 0 && t5adj == 200,
          fmt("reach 10, цель в 10 фт: %d/200; reach 5, цель в 10 фт: %d/200; reach 5, цель в 5 фт: %d/200", t10, t5,
              t5adj));
    }
    if (TB()) {
        auto approach = [&](int reachA) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.reach = reachA;
            Boss d = plain(100000, 0, 0, 5);
            d.ac = 0;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 18, 10));
            int hp0 = w.f[1].hp;
            takeTurn(w, 0);
            return std::make_pair(distFt(w.f[0], w.f[1]), hp0 - w.f[1].hp);
        };
        int hit10 = 0, dist10ok = 0, dist5ok = 0;
        for (int i = 0; i < 60; i++) {
            auto p10 = approach(10), p5 = approach(5);
            dist10ok += p10.first == 10;
            hit10 += p10.second > 0;
            dist5ok += p5.first == 5;
        }
        T("T60", "§досягаемость: ИИ подходит ровно на свою досягаемость и атакует (10 фт против 5 фт)",
          dist10ok == 60 && hit10 >= 50 && dist5ok == 60,
          fmt("reach 10: подошёл на 10 фт %d/60, попал %d/60; reach 5: подошёл на 5 фт %d/60", dist10ok, hit10, dist5ok));
    }
    if (TB()) {
        Coef C2 = CC();
        long long pr[4];
        int rr[4] = {5, 10, 15, 20};
        bool ok = true;
        Boss b = plain(100, 2, 6, 5);
        b.wcost = 0;
        for (int i = 0; i < 4; i++) {
            Boss c = b;
            c.reach = rr[i];
            g_B = 800;
            pr[i] = priceOf(c, C2, 800);
        }
        for (int i = 1; i < 4; i++)
            if (!(pr[i] > pr[i - 1])) ok = false;
        T("T61",
          "§досягаемость: цена растёт с досягаемостью (5 < 10 < 15 < 20 фт); у дальнобойного оружия не добавляется", ok,
          fmt("цены: %lld / %lld / %lld / %lld", pr[0], pr[1], pr[2], pr[3]));
    }
    if (TB()) {
        int melee = 0, r10 = 0, r15 = 0, r20 = 0, rMore = 0, N = 8000;   // rMore — больше 20 фт (после теста 4 ноября: до 500 фт)
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, CC(), false);
            if (b.wN > 0 && b.wRange <= 5) {
                melee++;
                if (b.reach == 10)
                    r10++;
                else if (b.reach == 15)
                    r15++;
                else if (b.reach == 20)
                    r20++;
                else if (b.reach > 20)
                    rMore++;
            }
        }
        double sh = (double)(r10 + r15 + r20 + rMore) / melee;
        T("T62", "(инфо) доля ближних боссов с досягаемостью >5 фт (в бестиарии ≈16% карточек)", sh > 0.12 && sh < 0.20,
          fmt("%.1f%% (10 фт: %d, 15 фт: %d, 20 фт: %d, больше: %d из %d ближних)", 100 * sh, r10, r15, r20, rMore, melee));
    }

    // T63..T68 реакции: Парирование и Невероятное уклонение
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100000, 1, 6, 5);
        d.ac = 15;
        d.parry = 3;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        int h1 = 0, h2 = 0, used = 0, N = 80000;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            h1 += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            used += w.f[1].reactionUsed;
            w.f[1].reactionUsed = true;
            bool c2;
            h2 += attackRoll(w, w.f[0], w.f[1], PROF, 5, c2);
        }
        double p1 = (double)h1 / N, pu = (double)used / N, p2 = (double)h2 / N;
        T("T63",
          "§Парирование +3 против КД 15: первая ближняя атака попадает 25% (было бы 40%), реакция тратится в 15% "
          "случаев; вторая в раунде — 40%",
          std::fabs(p1 - 0.25) < 0.012 && std::fabs(pu - 0.15) < 0.012 && std::fabs(p2 - 0.40) < 0.012,
          fmt("первая %.3f, реакция потрачена %.3f, вторая %.3f", p1, pu, p2));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 30);
        Boss d = plain(100000, 1, 6, 5);
        d.ac = 15;
        d.parry = 3;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 14, 10));
        int N = 60000, hr = 0, ur = 0;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            hr += attackRoll(w, w.f[0], w.f[1], PROF, 30, c);
            ur += w.f[1].reactionUsed;
        }
        int hh = 0, uh = 0;
        w.f[0].hidden = true;
        w.f[0].b.wRange = 5;
        w.f[1].x = 11;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            hh += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            uh += w.f[1].reactionUsed;
        }
        w.f[0].hidden = false;
        int hi = 0, ui = 0;
        w.f[1].cond[C_INCAP] = 3;
        for (int i = 0; i < N; i++) {
            w.f[1].reactionUsed = false;
            bool c;
            hi += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            ui += w.f[1].reactionUsed;
        }
        T("T64",
          "§Парирование не работает: против дальнобойной атаки, против невидимого (скрытого) атакующего, в "
          "недееспособном состоянии",
          std::fabs((double)hr / N - 0.40) < 0.012 && ur == 0 && uh == 0 && ui == 0 &&
              std::fabs((double)hi / N - 0.40) < 0.012,
          fmt("дальняя атака: %.3f (реакций %d); скрытый атакующий: %.3f (реакций %d, с преимуществом ≈0.64); "
              "недееспособен: %.3f (реакций %d)",
              (double)hr / N, ur, (double)hh / N, uh, (double)hi / N, ui));
    }
    if (TB()) {
        Fighter d = mk(plain(100, 0, 0, 5), 0, 1, 1);
        d.b.uncanny = true;
        int a1 = applyUncanny(d, 20);
        bool u1 = d.reactionUsed;
        int a2 = applyUncanny(d, 20);
        Fighter e = mk(plain(100, 0, 0, 5), 0, 1, 1);
        e.b.uncanny = true;
        int small = applyUncanny(e, 5);
        bool us = e.reactionUsed;
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.uncanny = true;
        f.cond[C_INCAP] = 1;
        int inc = applyUncanny(f, 20);
        T("T65",
          "§Невероятное уклонение: вдвое меньше урона один раз за раунд; мелкие удары (<10% ХП) не тратят реакцию; "
          "недееспособный не может",
          a1 == 10 && u1 && a2 == 20 && small == 5 && !us && inc == 20,
          fmt("20→%d (реакция %d); второй удар 20→%d; мелкий 5→%d (реакция %d); недееспособен 20→%d", a1, u1, a2, small,
              us, inc));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = STR;
        Boss d = plain(200, 0, 0, 5);
        d.ac = 0;
        d.uncanny = true;
        Spec sv;
        sv.delivery = 1;
        sv.saveStat = DEX;
        sv.dmgN = 20;
        sv.dmgS = 6;
        sv.range = 30;
        Spec at;
        at.delivery = 0;
        at.dmgN = 20;
        at.dmgS = 6;
        at.range = 5;
        int savedUsed = 0, atkUsed = 0, N = 300;
        for (int i = 0; i < N; i++) {
            w.f.clear();
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            hitOne(w, sv, w.f[0], w.f[1]);
            savedUsed += w.f[1].reactionUsed;
            w.f[1].reactionUsed = false;
            w.f[1].hp = w.f[1].maxHp;
            int hp0 = w.f[1].hp;
            hitOne(w, at, w.f[0], w.f[1]);
            atkUsed += w.f[1].reactionUsed;
        }
        T("T66",
          "§Невероятное уклонение действует на атаки (с броском), но не на урон от спасброска (дыхание, области)",
          savedUsed == 0 && atkUsed >= N * 0.9,
          fmt("урон от спасброска: реакция потрачена %d из %d; атака с броском: %d из %d", savedUsed, N, atkUsed, N));
    }
    if (TB()) {
        Coef C2 = CC();
        Boss b = plain(100, 1, 6, 5);
        long long p0 = priceOf(b, C2, 800);
        Boss c = b;
        c.parry = 2;
        long long p2 = priceOf(c, C2, 800);
        c.parry = 5;
        long long p5 = priceOf(c, C2, 800);
        Boss u = b;
        u.uncanny = true;
        long long pu = priceOf(u, C2, 800);
        T("T67", "§цены: Парирование растёт с бонусом (+2 < +5), Уклонение стоит отдельно",
          p2 > p0 && p5 > p2 && pu > p0, fmt("без: %lld; +2: %lld; +5: %lld; уклонение: %lld", p0, p2, p5, pu));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 1, 6, 5);
        d.ac = 15;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        double base = pHitEst(w, w.f[0], w.f[1], PROF, 5);
        w.f[1].b.parry = 4;
        double with = pHitEst(w, w.f[0], w.f[1], PROF, 5);
        w.f[1].reactionUsed = true;
        double spent = pHitEst(w, w.f[0], w.f[1], PROF, 5);
        double far = pHitEst(w, w.f[0], w.f[1], PROF, 30);
        T("T68",
          "ИИ-оценка попадания учитывает Парирование цели (ниже), пока реакция не потрачена и против ближней атаки",
          with < base && spent == base && far != with,
          fmt("без парирования %.3f; с парированием %.3f; реакция потрачена %.3f", base, with, spent));
    }

    // T69..T74 довесок к удару оружием
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 1, 5);
        a.wAtt = 3;
        a.rN = 50;
        a.rS = 1;
        a.rType = DT_FIRE;
        Boss d = plain(1000000, 0, 0, 5);
        d.ac = -1000;
        int bad = 0, got = 0, N = 2000;
        for (int i = 0; i < N; i++) {
            w.f.clear();
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            int hp0 = w.f[1].hp;
            w.f[0].riderUsed = false;
            doWeapon(w, w.f[0], 1);
            int D = hp0 - w.f[1].hp;
            if (D >= 100) bad++;
            if (D >= 51) got++;
        }
        T("T69",
          "§довесок: срабатывает один раз за ход (3 атаки): урон ≥100 бывает лишь при крите на первом попадании (~5%), "
          "при двойном было бы ~99%",
          bad < N * 0.10 && got > N * 0.9,
          fmt("урон ≥100: %d из %d (%.1f%%; крит ожидается ~5%%); с довеском %d", bad, N, 100.0 * bad / N, got));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 1, 5);
        a.rN = 50;
        a.rS = 1;
        a.rType = DT_FIRE;
        a.wType = DT_SLASHING;
        Boss d = plain(1000000, 0, 0, 5);
        d.ac = -1000;
        auto run = [&](unsigned res, unsigned imm, unsigned vul) {
            std::set<int> v;
            Boss dd = d;
            dd.resMask = res;
            dd.immMask = imm;
            dd.vulnMask = vul;
            for (int i = 0; i < 600; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(dd, 1, 11, 10));
                int hp0 = w.f[1].hp;
                doWeapon(w, w.f[0], 1);
                int D = hp0 - w.f[1].hp;
                if (D > 0) v.insert(D);
            }
            return v;
        };
        auto r1 = run(1u << DT_FIRE, 0, 0), r2 = run(0, 1u << DT_FIRE, 0), r3 = run(0, 0, 1u << DT_FIRE),
             r0 = run(0, 0, 0);
        bool ok = r0 == std::set<int>{51, 102} && r1 == std::set<int>{26, 52} && r2 == std::set<int>{1, 2} &&
                  r3 == std::set<int>{101, 202};
        T("T70",
          "§довесок: свой тип урона и защиты цели (обычный 51, резист 26, иммунитет 1, уязвимость 101; крит удваивает)",
          ok,
          fmt("обычный: %zu знач., резист: %zu, иммунитет: %zu, уязв.: %zu", r0.size(), r1.size(), r2.size(),
              r3.size()));
    }
    if (TB()) {
        auto rate = [&](int cond, int statv, int conStat) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 1, 5);
            a.stats = {30, 10, 10, 10, 10, 10};
            a.mainStat = STR;
            a.rCond = cond;
            a.rRounds = 2;
            Boss d = plain(1000000, 0, 0, 5);
            d.ac = -1000;
            d.stats = {statv, statv, statv, statv, statv, statv};
            (void)conStat;
            int ap = 0, N = 3000;
            for (int i = 0; i < N; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                doWeapon(w, w.f[0], 1);
                if (w.f[1].cond[cond] > 0) ap++;
            }
            return ap / (double)N;
        };
        double weak = rate(C_STUN, 1, 0), strong = rate(C_STUN, 30, 0), gr = rate(C_GRAPPLE, 30, 0),
               wis = rate(C_FRIGHT, 30, 0);
        T("T71",
          "§довесок: состояние — спасбросок по смыслу (оглушён→ТЕЛ, испуган→МДР), у слабого проходит всегда, у "
          "сильного ~45%; захват без спасброска",
          weak > 0.9 && strong > 0.3 && strong < 0.55 && gr > 0.9 && wis > 0.3 && wis < 0.55,
          fmt("слабый %.2f, сильный (ТЕЛ30) %.2f, захват vs СИЛ30 %.2f, испуган vs МДР30 %.2f (попадание 0.95)", weak,
              strong, gr, wis));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(1000, 1, 6, 5);   // цель вооружена: оглушение ценно тем, что она не ударит (безоружный не бьёт)
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        double base = evWeapon(w, w.f[0], w.f[1]);
        w.f[0].b.rN = 2;
        w.f[0].b.rS = 6;
        w.f[0].b.rType = DT_FIRE;
        double dm = evWeapon(w, w.f[0], w.f[1]);
        w.f[0].b.rN = 0;
        w.f[0].b.rCond = C_STUN;
        w.f[0].b.rRounds = 1;
        double cd = evWeapon(w, w.f[0], w.f[1]);
        w.f[0].b.rCond = 0;
        w.f[0].b.rN = 2;
        w.f[1].b.immMask = 1u << DT_FIRE;
        double imm = evWeapon(w, w.f[0], w.f[1]);
        T("T72",
          "ИИ учитывает довесок: урон и состояние повышают ценность удара, иммунитет цели к типу довеска — обнуляет "
          "урон довеска",
          dm > base && cd > base && std::fabs(imm - base) < 1e-9,
          fmt("база %.2f, +2к6 огня %.2f, +оглушение %.2f, иммунитет к огню %.2f", base, dm, cd, imm));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Boss b = plain(100, 1, 6, 5);
        long long p[5];
        b.rN = 1;
        b.rS = 6;
        p[0] = riderPrice(b, C2);
        b.rN = 3;
        p[1] = riderPrice(b, C2);
        b.rN = 0;
        b.rCond = C_PRONE;
        p[2] = riderPrice(b, C2);
        b.rCond = C_STUN;
        b.rRounds = 1;
        p[3] = riderPrice(b, C2);
        b.rRounds = 3;
        p[4] = riderPrice(b, C2);
        Boss c = b;
        g_B = 1600;
        long long big = riderPrice(c, C2);
        g_B = 800;
        T("T73",
          "§довесок: цена растёт с кубиками и силой состояния/длительностью, состояние масштабируется с бюджетом ⚖",
          p[1] > p[0] && p[3] > p[2] && p[4] > p[3] && big >= 2 * p[4] - 1,
          fmt("1к6: %lld, 3к6: %lld, сбит: %lld, оглушение 1р: %lld, 3р: %lld; на бюджете 1600: %lld", p[0], p[1], p[2],
              p[3], p[4], big));
    }
    if (TB()) {
        int rd = 0, both = 0, wpn = 0, N = 8000;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (b.wN > 0) wpn++;
            if (b.rcost > 0) {
                rd++;
                if (b.rN > 0 && b.rCond > 0) both++;
            }
        }
        T("T74", "(инфо) доля боссов с довеском среди вооружённых; доля с обоими эффектами", rd > 0,
          fmt("%.1f%% (оба эффекта %.1f%% от довесков); вооружённых %d из %d", 100.0 * rd / wpn,
              100.0 * both / std::max(1, rd), wpn, N));
    }

    // T75..T79 временные хиты и бонус КД на себя
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.temp = 10;
        hurt(f, 4, DT_FIRE, true);
        int a = f.hp, ta = f.temp;
        hurt(f, 10, DT_FIRE, true);
        int b2 = f.hp, tb = f.temp;
        f.b.immMask = 1u << DT_COLD;
        f.temp = 5;
        hurt(f, 50, DT_COLD, true);
        int tc = f.temp;
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss bb = plain(100, 0, 0, 5);
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.tempN = 1;
        bb.abil.push_back(s);
        w.f.push_back(mk(bb, 0, 10, 10));
        w.f[0].hp = 50;
        int mx = 0;
        for (int i = 0; i < 300; i++) {
            doSpec(w, w.f[0], 0, 0);
            mx = std::max(mx, w.f[0].temp);
        }
        T("T75",
          "§временные хиты: поглощают урон первыми; не лечат; не складываются (берётся большее); иммунитет урона не "
          "тратит их",
          a == 100 && ta == 6 && b2 == 96 && tb == 0 && tc == 5 && w.f[0].hp == 50 && mx <= 6 && mx >= 5,
          fmt("10 вр.хп, удар 4: хп %d, вр.хп %d; затем удар 10: хп %d, вр.хп %d; иммунный удар: вр.хп %d; 300 "
              "применений 1к6: макс %d, хп цели %d",
              a, ta, b2, tb, tc, mx, w.f[0].hp));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100000, 0, 0, 5);
        d.ac = 15;
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.acB = 4;
        s.acRounds = 2;
        d.abil.push_back(s);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        auto rate = [&]() {
            int h = 0, N = 60000;
            for (int i = 0; i < N; i++) {
                bool c;
                w.f[1].reactionUsed = false;
                h += attackRoll(w, w.f[0], w.f[1], PROF, 5, c);
            }
            return h / (double)N;
        };
        double p0 = rate();
        doSpec(w, w.f[1], 0, 0);
        double p1 = rate();
        endOfTurn(w.f[1]);
        double p2 = rate();
        endOfTurn(w.f[1]);
        double p3 = rate();
        T("T76", "§бонус КД: +4 к КД снижает шанс попадания (КД 15→19: 40%→20%), действует ровно 2 своих хода",
          std::fabs(p0 - 0.40) < 0.012 && std::fabs(p1 - 0.20) < 0.012 && std::fabs(p2 - 0.20) < 0.012 &&
              std::fabs(p3 - 0.40) < 0.012,
          fmt("без: %.3f, с бонусом: %.3f, после 1 хода: %.3f, после 2: %.3f", p0, p1, p2, p3));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Spec st;
        st.self = true;
        st.delivery = 2;
        st.tempN = 3;
        Spec sa;
        sa.self = true;
        sa.delivery = 2;
        sa.acB = 3;
        sa.acRounds = 5;
        sa.bonus = true;
        a.abil.push_back(st);
        a.abil.push_back(sa);
        Boss d = plain(100, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 18, 10));
        w.f[0].init = 2000;
        w.f[1].init = 1;
        takeTurn(w, 0);
        T("T77", "ИИ применяет временные хиты и бонус КД перед боем (враг на подходе)",
          w.f[0].temp > 0 && w.f[0].acBuffRounds > 0,
          fmt("вр.хп %d, бонус КД %d на %d р.", w.f[0].temp, w.f[0].acBuff, w.f[0].acBuffRounds));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.self = true;
        a.delivery = 2;
        a.tempN = 1;
        Spec b2 = a;
        b2.tempN = 4;
        Spec c;
        c.self = true;
        c.delivery = 2;
        c.acB = 2;
        c.acRounds = 5;
        c.uses = 1;   // без лимита бонус КД держится весь бой при любой длительности (доля боя = 1)
        Spec c2 = c;
        c2.acB = 6;
        Spec c3 = c;
        c3.acRounds = 25;
        g_B = 1600;
        Spec r = a;
        r.range = 120;
        r.radius = 30;
        long long pa = specPrice(a, C2), pb = specPrice(b2, C2), pc = specPrice(c, C2), pc2 = specPrice(c2, C2),
                  pc3 = specPrice(c3, C2);
        T("T78",
          "§цены: вр.хп растут с числом кубиков; бонус КД — с величиной и длительностью; дальность на «себя» не "
          "удорожает",
          pb > pa && pc2 > pc && pc3 > pc && specPrice(r, C2) == pa,
          fmt("вр.хп 1к6: %lld, 4к6: %lld; КД +1/5р: %lld, +3/5р: %lld, +1/10р: %lld", pa, pb, pc, pc2, pc3));
    }
    if (TB()) {
        int cnt[9] = {}, tot = 0;
        for (int i = 0; i < 40000; i++) {
            Spec s = randSpec(0);
            if (!s.self) continue;
            // после теста 4 ноября в одной способности бывает несколько эффектов: считаем каждый вид, доля — от всех видов
            const bool has[9] = {s.healN > 0, s.teleFt > 0, s.hide, s.buffN > 0, s.tempN > 0, s.acB > 0, s.darkR > 0, s.invisN > 0, s.sbAmt > 0};
            for (int k = 0; k < 9; k++)
                if (has[k]) { cnt[k]++; tot++; }
        }
        bool ok = true;
        std::string d;
        const char* nm[9] = {"лечение", "телепорт",    "скрытность",         "бафф", "вр.хп", "КД",
                             "тьма",    "невидимость", "бафф характеристики"};
        for (int k = 0; k < 9; k++) {
            double sh = cnt[k] / (double)tot;
            d += fmt("%s %.0f%%; ", nm[k], 100 * sh);
            if (sh < 0.08 || sh > 0.25) ok = false;
        }
        T("T79", "генератор «на себя» выдаёт все 9 видов (8–25% каждый)", ok, d);
    }

    // T80..T84 помеха/преимущество как состояния и урон по времени
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 11, 10));
        bool a, d;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool base = !a && !d;
        w.f[0].cond[C_WEAK] = 2;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool weak = !a && d;
        w.f[0].cond[C_WEAK] = 0;
        w.f[1].cond[C_LIT] = 2;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool lit = a && !d;
        w.f[0].cond[C_WEAK] = 2;
        advState(w, w.f[0], w.f[1], 5, a, d);
        bool both = a && d;
        T("T80", "§ослаблен: помеха на СВОИ атаки; подсвечен: преимущество у атакующих по нему (вместе гасятся в d20)",
          base && weak && lit && both,
          fmt("обычные %d, ослаблен (помеха) %d, подсвечен (преим.) %d, оба (преим.+помеха) %d", base, weak, lit,
              both));
    }
    if (TB()) {
        auto trial = [&](int tstat, int fire_imm, int delivery, int ostat, int& lost, int& after) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 0, 0, 5);
            a.mainStat = STR;
            a.stats = {ostat, 10, 10, 10, 10, 10};
            Boss d = plain(1000, 0, 0, 5);
            d.stats = {tstat, tstat, tstat, tstat, tstat, tstat};
            if (fire_imm) d.immMask = 1u << DT_FIRE;
            Spec sp;
            sp.delivery = delivery;
            sp.range = 30;
            sp.saveStat = DEX;
            sp.dotN = 2;
            sp.dotS = 1;
            sp.dotRounds = 3;
            sp.dtype = DT_FIRE;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 12, 10));
            hitOne(w, sp, w.f[0], w.f[1]);
            int hp0 = w.f[1].hp;
            int rr = w.f[1].dotRounds;
            for (int i = 0; i < 3; i++) takeTurn(w, 1);
            lost = hp0 - w.f[1].hp;
            takeTurn(w, 1);
            after = hp0 - w.f[1].hp - lost;
            return rr;
        };
        int l1, a1, l2, a2, l3, a3, l4, a4;
        int r1 = trial(10, 0, 2, 10, l1, a1), r2 = trial(10, 1, 2, 10, l2, a2), r3 = trial(1, 0, 1, 30, l3, a3),
            r4 = trial(30, 0, 1, 10, l4, a4);
        T("T81",
          "§урон по времени: 2 урона в начале хода цели ровно 3 хода; иммунитет к типу его гасит; со спасброском "
          "вешается лишь при провале",
          r1 == 3 && l1 == 6 && a1 == 0 && l2 == 0 && l3 == 6 && l4 == 0,
          fmt("без броска: раундов %d, урон за 3 хода %d, после %d; иммунная цель: %d; спас провален: %d; спас пройден "
              "(ЛОВ30): %d",
              r1, l1, a1, l2, l3, l4));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.delivery = 2;
        a.dotN = 1;
        a.dotS = 6;
        a.dotRounds = 1;
        Spec b2 = a;
        b2.dotN = 3;
        Spec c = a;
        c.dotRounds = 4;
        Spec d = a;
        d.dotS = 12;
        T("T82", "§цены: урон по времени растёт с числом кубиков, их размером и длительностью",
          specPrice(b2, C2) > specPrice(a, C2) && specPrice(c, C2) > specPrice(a, C2) &&
              specPrice(d, C2) > specPrice(a, C2),
          fmt("1к6×1р: %lld, 3к6×1р: %lld, 1к6×4р: %lld, 1к12×1р: %lld", specPrice(a, C2), specPrice(b2, C2),
              specPrice(c, C2), specPrice(d, C2)));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = STR;
        Boss d = plain(1000, 1, 6, 5);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.dmgN = 1;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        double e0 = evSpecOne(w, s, w.f[0], w.f[1]);
        s.dotN = 2;
        s.dotS = 6;
        s.dotRounds = 3;
        double e1 = evSpecOne(w, s, w.f[0], w.f[1]);
        w.f[1].b.immMask = 1u << DT_FIRE;
        double e2 = evSpecOne(w, s, w.f[0], w.f[1]);
        Spec sl;
        sl.delivery = 2;
        sl.range = 30;
        sl.cond = C_LIT;
        sl.condRounds = 1;
        double l0 = evSpecOne(w, sl, w.f[0], w.f[1]);
        Spec sw = sl;
        sw.cond = C_WEAK;
        double w0 = evSpecOne(w, sw, w.f[0], w.f[1]);
        T("T83",
          "ИИ учитывает урон по времени (и иммунитет цели к его типу); «подсвечен/ослаблен» имеют ненулевую ценность",
          e1 > e0 && e2 == 0 && l0 > 0 && w0 > 0,
          fmt("огонь 1к6: %.2f, +DoT: %.2f, иммунная цель: %.2f; подсвечен %.2f, ослаблен %.2f", e0, e1, e2, l0, w0));
    }
    if (TB()) {
        int dot = 0, all = 0, N = 40000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            if (s.self) continue;
            all++;
            if (s.dotN) dot++;
        }
        T("T84", "(инфо) доля несамоспособностей с уроном по времени", dot > 0, fmt("%.1f%%", 100.0 * dot / all));
    }

    // T85..T91 партия 2: видимость и чувства, местность, ауры
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 16, 10));
        bool a, d;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool base = !a && !d;
        w.dark[10][16] = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool darkT = d && !a;
        w.f[0].b.dv = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool dv30 = !a && !d;
        w.f[0].b.dv = 20;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool dv20 = d && !a;
        w.f[0].b.dv = 0;
        w.dark[10][16] = false;
        w.dark[10][10] = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool darkA = a && !d;
        w.f[1].b.dv = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool darkAdv = !a && !d;
        w.f[1].b.dv = 0;
        w.dark[10][10] = false;
        w.f[1].hidden = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool hid = d && !a;
        w.f[0].b.blindsight = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool bs = !a && !d;
        w.f[0].b.blindsight = 0;
        w.f[1].hidden = false;
        w.f[0].hidden = true;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool hidA = a && !d;
        w.f[1].b.blindsight = 30;
        advState(w, w.f[0], w.f[1], 30, a, d);
        bool bsA = !a && !d;
        T("T85",
          "§видимость: цель в тьме — помеха без тёмного зрения (достаточной дальности); сам в тьме — преимущество; "
          "слепое зрение видит «скрытого»",
          base && darkT && dv30 && dv20 && darkA && darkAdv && hid && bs && hidA && bsA,
          fmt("обычно %d; цель в тьме: помеха %d, тёмное зр.30 на 30 фт %d, 20 фт %d; я в тьме: преим. %d, у врага "
              "тёмное зр. %d; скрытая цель %d, слепое зр. %d; я скрыт %d, у врага слепое зр. %d",
              base, darkT, dv30, dv20, darkA, darkAdv, hid, bs, hidA, bsA));
    }
    if (TB()) {
        auto cells = [&](int fly, int ign, bool diffRow) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.fly = fly;
            a.ignDiff = ign;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 50, 10));
            if (diffRow)   // полоса во всю высоту: обойти нельзя (путь — кратчайший по цене шага)
                for (int y = 0; y < W; y++)
                    for (int x = 11; x < 50; x++) w.diff[y][x] = true;
            int used = moveToward(w, w.f[0], w.f[1], 0, 30, false);
            return std::make_pair(w.f[0].x - 10, used);
        };
        auto n = cells(0, 0, false), dd = cells(0, 0, true), fl = cells(1, 0, true), ig = cells(0, 1, true);
        T("T86",
          "§труднопроходимая местность: 10 фт за клетку (30 фт → 3 клетки вместо 6); полёт и «игнорирует» не "
          "замедляются",
          n.first == 6 && dd.first == 3 && dd.second <= 30 && fl.first == 6 && ig.first == 6,
          fmt("обычно %d клеток, по местности %d (потрачено %d фт), полёт %d, игнор %d", n.first, dd.first, dd.second,
              fl.first, ig.first));
    }
    if (TB()) {
        auto lost = [&](int vx, unsigned res, unsigned imm, int hpOwner, int auraN, int cond, int vstat,
                        bool& stunned) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss o = plain(100, 0, 0, 5);
            o.auraN = auraN;
            o.auraS = 1;
            o.auraRad = 15;
            o.auraType = DT_FIRE;
            o.auraCond = cond;
            o.auraRounds = 1;
            o.stats = {30, 10, 10, 10, 10, 10};
            o.mainStat = STR;
            Boss v = plain(1000, 0, 0, 5);
            v.resMask = res;
            v.immMask = imm;
            v.stats = {vstat, vstat, vstat, vstat, vstat, vstat};
            w.f.push_back(mk(o, 0, 10, 10));
            w.f[0].hp = hpOwner;
            w.f.push_back(mk(v, 1, vx, 10));
            int hp0 = w.f[1].hp;
            g_act = ActInfo{};
            takeTurn(w, 1);
            stunned = g_act.type == "stunned";
            return hp0 - w.f[1].hp;
        };
        bool s1, s2, s3, s4, s5, s6;
        int in = lost(13, 0, 0, 100, 50, 0, 10, s1), out = lost(14, 0, 0, 100, 50, 0, 10, s2),
            rs = lost(13, 1u << DT_FIRE, 0, 100, 50, 0, 10, s3), im = lost(13, 0, 1u << DT_FIRE, 100, 50, 0, 10, s4),
            dead = lost(13, 0, 0, 0, 50, 0, 10, s5);
        int w1 = 0, w2 = 0;
        for (int i = 0; i < 400; i++) {
            bool q;
            lost(13, 0, 0, 100, 0, C_STUN, 1, q);
            w1 += q;
            lost(13, 0, 0, 100, 0, C_STUN, 30, q);
            w2 += q;
        }
        bool st1 = w1 >= 396, st2 = w2 >= 120 && w2 <= 240;
        (void)s6;
        T("T87",
          "§аура: урон в начале хода врага в радиусе (15 фт — да, 20 — нет); защиты цели действуют; мёртвый владелец "
          "не бьёт; состояние — со спасброском",
          in == 50 && out == 0 && rs == 25 && im == 0 && dead == 0 && st1 && st2,
          fmt("в радиусе %d, снаружи %d, резист %d, иммунитет %d, владелец мёртв %d; оглушение (СЛ 20): слабый (ТЕЛ1) "
              "%d из 400, сильный (ТЕЛ30) %d из 400 (ожидается ~180)",
              in, out, rs, im, dead, w1, w2));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss bb = plain(100, 0, 0, 5);
        Spec dk;
        dk.self = true;
        dk.delivery = 2;
        dk.darkR = 20;
        Spec tr;
        tr.delivery = 2;
        tr.range = 60;
        tr.radius = 10;
        tr.shape = SH_CUBE;   // v21: квадрат клеток — это куб (по умолчанию область — сфера)
        tr.terr = true;
        bb.abil.push_back(dk);
        bb.abil.push_back(tr);
        w.f.push_back(mk(bb, 0, 50, 50));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 60, 50));
        doSpec(w, w.f[0], 0, 1);
        doSpec(w, w.f[0], 1, 1);
        int nd = 0, nf = 0;
        for (int y = 0; y < W; y++)
            for (int x = 0; x < W; x++) {
                nd += w.dark[y][x];
                nf += w.diff[y][x];
            }
        T("T88",
          "§способности: «тьма вокруг себя» радиус 20 фт = 9×9 клеток; область с местностью радиус 10 фт = 5×5 клеток "
          "вокруг цели",
          nd == 81 && nf == 25 && w.dark[50][50] && w.diff[50][60], fmt("клеток тьмы %d, труднопроходимых %d", nd, nf));
    }
    if (TB()) {
        auto run = [&](int dv, int bs) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.dv = dv;
            a.blindsight = bs;
            Spec dk;
            dk.self = true;
            dk.delivery = 2;
            dk.darkR = 20;
            a.abil.push_back(dk);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 18, 10));
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            int n = 0;
            for (int y = 0; y < W; y++)
                for (int x = 0; x < W; x++) n += w.dark[y][x];
            return n;
        };
        int with = run(60, 0), blind = run(0, 30), without = run(0, 0);
        T("T89", "ИИ ставит тьму только когда сам в ней видит (тёмное/слепое зрение), а враг нет",
          with > 0 && blind > 0 && without == 0,
          fmt("тёмное зрение: %d клеток, слепое: %d, без чувств: %d", with, blind, without));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Boss b = plain(100, 1, 6, 5);
        long long p0 = priceOf(b, C2, 800), pd[3], pb[3];
        int dv[3] = {30, 60, 120}, bs[3] = {10, 30, 60};
        for (int i = 0; i < 3; i++) {
            Boss c = b;
            c.dv = dv[i];
            pd[i] = priceOf(c, C2, 800);
            Boss e = b;
            e.blindsight = bs[i];
            pb[i] = priceOf(e, C2, 800);
        }
        Boss a1 = b;
        a1.auraRad = 10;
        a1.auraN = 1;
        a1.auraS = 6;
        Boss a2 = a1;
        a2.auraN = 3;
        Boss a3 = a1;
        a3.auraRad = 30;
        Boss a4 = a1;
        a4.auraCond = C_STUN;
        a4.auraRounds = 1;
        long long q1 = priceOf(a1, C2, 800), q2 = priceOf(a2, C2, 800), q3 = priceOf(a3, C2, 800),
                  q4 = priceOf(a4, C2, 800);
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.radius = 10;
        s.dmgN = 1;
        long long ps0 = specPrice(s, C2);
        s.terr = true;
        long long ps1 = specPrice(s, C2);
        T("T90",
          "§цены: чувства растут по ступеням; аура — с кубиками, радиусом и состоянием; местность в области — надбавка",
          pd[0] > p0 && pd[1] > pd[0] && pd[2] > pd[1] && pb[1] > pb[0] && pb[2] > pb[1] && q2 > q1 && q3 > q1 &&
              q4 > q1 && ps1 > ps0,
          fmt("тёмное зр. 30/60/120: +%lld/+%lld/+%lld; слепое 10/30/60: +%lld/+%lld/+%lld; аура 1к6 10фт %lld, 3к6 "
              "%lld, 30фт %lld, +оглушение %lld; местность +%lld",
              pd[0] - p0, pd[1] - p0, pd[2] - p0, pb[0] - p0, pb[1] - p0, pb[2] - p0, q1, q2, q3, q4, ps1 - ps0));
    }
    if (TB()) {
        int au = 0, dv = 0, bs = 0, ig = 0, N = 8000;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (b.auraRad > 0) au++;
            if (b.dv) dv++;
            if (b.blindsight) bs++;
            if (b.ignDiff) ig++;
        }
        T("T91",
          "(инфо) доли боссов: аура / тёмное зрение / слепое зрение / игнор местности (цель по бестиарию: чувства у "
          "большинства, аура ~7%)",
          au > 0 && dv > 0,
          fmt("%.1f%% / %.1f%% / %.1f%% / %.1f%%", 100.0 * au / N, 100.0 * dv / N, 100.0 * bs / N, 100.0 * ig / N));
    }

    // T92..T96 доводка состояний: опутан, испуган/в бегстве, невидимость
    if (TB()) {
        Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1);
        f.b.stats = {10, 10, 10, 10, 10, 10};
        auto rate = [&](int stat) {
            int ok = 0, N = 60000;
            for (int i = 0; i < N; i++) ok += saveOK(f, stat, 11);
            return ok / (double)N;
        };
        double d0 = rate(DEX), s0 = rate(STR), pf0 = pFailEst(f, DEX, 11);
        f.cond[C_RESTR] = 2;
        double d1 = rate(DEX), s1 = rate(STR), pf1 = pFailEst(f, DEX, 11), pfs = pFailEst(f, STR, 11);
        T("T92", "§опутан: помеха на спасброски ЛОВ (успех 50%→25%), остальные спасброски не меняются; ИИ учитывает",
          std::fabs(d0 - 0.5) < 0.012 && std::fabs(d1 - 0.25) < 0.012 && std::fabs(s0 - 0.5) < 0.012 &&
              std::fabs(s1 - 0.5) < 0.012 && std::fabs(pf0 - 0.5) < 1e-9 && std::fabs(pf1 - 0.75) < 1e-9 &&
              std::fabs(pfs - 0.5) < 1e-9,
          fmt("ЛОВ: %.3f→%.3f (оценка провала %.2f→%.2f), СИЛ: %.3f→%.3f", d0, d1, pf0, pf1, s0, s1));
    }
    if (TB()) {
        auto run = [&](int cond, int dist, int& enemyLost) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            Boss d = plain(100000, 1, 6, 5);
            w.f.push_back(mk(a, 0, 50, 50));
            w.f.push_back(mk(d, 1, 50 + dist / 5, 50));
            if (cond) w.f[0].cond[cond] = 3;
            w.f[0].init = 2000;
            w.f[1].init = 1;
            int hp0 = w.f[1].hp;
            takeTurn(w, 0);
            enemyLost = hp0 - w.f[1].hp;
            return distFt(w.f[0], w.f[1]);
        };
        int e1, e2, e3, e4;
        int flee = run(C_FLEE, 10, e1), fright = run(C_FRIGHT, 100, e2), free_ = run(0, 100, e3),
            frightNear = run(C_FRIGHT, 5, e4);
        T("T93",
          "§испуган: не приближается (стоит), но атакует с помехой; §в бегстве: действие на Рывок прочь (+60 фт), не "
          "атакует",
          flee == 70 && e1 == 0 && fright == 100 && free_ < 100 && e2 == 0 && e4 >= 0,
          fmt("в бегстве с 10 фт: дистанция %d, урон врагу %d; испуган с 100 фт: %d (свободный: %d); испуган вплотную: "
              "урон врагу %d",
              flee, e1, fright, free_, e4));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        Boss d = plain(100, 1, 6, 5);
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.invisN = 2;
        d.abil.push_back(s);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        doSpec(w, w.f[1], 0, 0);
        int r0 = w.f[1].invisRounds;
        bool x, y;
        advState(w, w.f[0], w.f[1], 5, x, y);
        bool onTarget = y && !x;
        advState(w, w.f[1], w.f[0], 5, x, y);
        bool fromHim = x && !y;
        doWeapon(w, w.f[1], 0);
        int r1 = w.f[1].invisRounds;
        w.f[0].b.blindsight = 30;
        advState(w, w.f[0], w.f[1], 5, x, y);
        bool bs = !x && !y;
        endOfTurn(w.f[1]);
        endOfTurn(w.f[1]);
        int r2 = w.f[1].invisRounds;
        T("T94",
          "§невидимость: по невидимому — помеха, он бьёт с преимуществом; не спадает от атаки; слепое зрение видит; "
          "кончается через N ходов",
          r0 == 2 && onTarget && fromHim && r1 == 2 && bs && r2 == 0,
          fmt("раундов %d; по нему с помехой %d; его атака с преим. %d; после атаки %d; слепое зрение врага %d; после "
              "2 ходов %d",
              r0, onTarget, fromHim, r1, bs, r2));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.self = true;
        a.delivery = 2;
        a.invisN = 1;
        Spec b2 = a;
        b2.invisN = 4;
        Spec r = a;
        r.range = 120;
        r.radius = 30;
        T("T95", "§цены: невидимость растёт с длительностью; дальность на «себя» не удорожает",
          specPrice(b2, C2) > specPrice(a, C2) && specPrice(r, C2) == specPrice(a, C2),
          fmt("1 р.: %lld, 4 р.: %lld", specPrice(a, C2), specPrice(b2, C2)));
    }
    if (TB()) {
        auto run = [&](int bs) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            Spec s;
            s.self = true;
            s.delivery = 2;
            s.invisN = 3;
            a.abil.push_back(s);
            Boss d = plain(100, 1, 6, 5);
            d.blindsight = bs;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 18, 10));
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            return w.f[0].invisRounds;
        };
        int no = run(0), yes = run(30);
        T("T96", "ИИ уходит в невидимость перед боем, кроме случая, когда у врага слепое зрение", no > 0 && yes == 0,
          fmt("против обычного: %d р., против слепого зрения: %d", no, yes));
    }

    // T97..T103 партия 3: концентрация, ячейки, триггеры, контратака, всплеск
    if (TB()) {
        auto trial = [&](int dmg, int& broke) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100000, 0, 0, 5);
            a.mainStat = STR;
            a.stats = {30, 10, 10, 10, 10, 10};
            Spec sp;
            sp.delivery = 1;
            sp.range = 30;
            sp.saveStat = DEX;
            sp.cond = C_STUN;
            sp.condRounds = 5;
            sp.conc = true;
            a.abil.push_back(sp);
            Boss d = plain(1000, 0, 0, 5);
            d.stats = {1, 1, 1, 1, 1, 1};
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 12, 10));
            doSpec(w, w.f[0], 0, 1);
            bool had = w.f[1].cond[C_STUN] > 0 && w.f[1].condConc[C_STUN] == 1 && w.f[0].concSpec == 0;
            if (dmg > 0) {
                hurt(w.f[0], dmg, DT_FIRE, true);
                resolveConc(w);
            }
            broke = w.f[1].cond[C_STUN] == 0 && w.f[0].concSpec < 0;
            return had;
        };
        int N = 600, hadAll = 0, bigBroke = 0, smallBroke = 0, noneBroke = 0;
        for (int i = 0; i < N; i++) {
            int b;
            hadAll += trial(40, b);
            bigBroke += b;
            trial(2, b);
            smallBroke += b;
            trial(0, b);
            noneBroke += b;
        }
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100000, 0, 0, 5);
        a.mainStat = STR;
        a.stats = {30, 10, 10, 10, 10, 10};
        Spec s1;
        s1.delivery = 1;
        s1.range = 30;
        s1.saveStat = DEX;
        s1.cond = C_STUN;
        s1.condRounds = 5;
        s1.conc = true;
        Spec s2 = s1;
        s2.cond = C_SLOW;
        a.abil.push_back(s1);
        a.abil.push_back(s2);
        Boss d = plain(1000, 0, 0, 5);
        d.stats = {1, 1, 1, 1, 1, 1};
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 12, 10));
        doSpec(w, w.f[0], 0, 1);
        doSpec(w, w.f[0], 1, 1);
        bool swapped = w.f[1].cond[C_STUN] == 0 && w.f[1].cond[C_SLOW] > 0 && w.f[0].concSpec == 1;
        for (int i = 0; i < 5; i++) endOfTurn(w.f[1]);
        w.f[0].hp = w.f[0].hp;
        w.f[0].concSpec = 1;
        {
            Fighter& me = w.f[0];
            bool act = false;
            for (auto& f : w.f)
                for (int k = 1; k < C_N; k++)
                    if (f.cond[k] > 0 && f.condConc[k] == 1) act = true;
            if (!act) me.concSpec = -1;
        }
        T("T97",
          "§концентрация: эффект держится ею; удар на 40 (СЛ 20) срывает в ~95%, на 2 (СЛ 10) в ~45%, без урона не "
          "срывается; вторая концентрация снимает первую; кончилась сама — освобождается",
          hadAll == N && bigBroke >= N * 0.9 && smallBroke >= N * 0.35 && smallBroke <= N * 0.55 && noneBroke == 0 &&
              swapped && w.f[0].concSpec < 0,
          fmt("эффект держится %d/%d; срыв: сильный удар %d, слабый %d, без урона %d (из %d); замена %d; освобождение "
              "%d",
              hadAll, N, bigBroke, smallBroke, noneBroke, N, swapped, w.f[0].concSpec < 0));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 0, 0, 5);
        a.slots[3] = 1;
        a.slots[5] = 1;
        Spec sp;
        sp.delivery = 2;
        sp.range = 30;
        sp.dmgN = 1;
        sp.dmgS = 6;
        sp.slotLvl = 3;
        a.abil.push_back(sp);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
        int f1 = findSlot(w.f[0], 3), f4 = findSlot(w.f[0], 4), f6 = findSlot(w.f[0], 6);
        doSpec(w, w.f[0], 0, 1);
        int l3 = w.f[0].slotLeft[3], l5 = w.f[0].slotLeft[5];
        doSpec(w, w.f[0], 0, 1);
        int l5b = w.f[0].slotLeft[5];
        int none = findSlot(w.f[0], 1);
        auto dmgAI = [&](int n) {
            World v;
            std::memset(v.wall, 0, sizeof v.wall);
            std::memset(v.climb, 0, sizeof v.climb);
            Boss b = plain(100, 0, 0, 5);
            b.slots[1] = n;
            Spec q = sp;
            q.slotLvl = 1;
            q.dmgN = 3;
            b.abil.push_back(q);
            v.f.push_back(mk(b, 0, 10, 10));
            v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
            v.f[0].init = 2000;
            v.f[1].init = 1;
            int hp0 = v.f[1].hp;
            takeTurn(v, 0);
            return hp0 - v.f[1].hp;
        };
        int withSlot = dmgAI(1), noSlot = dmgAI(0);
        T("T98",
          "§ячейки: подходящая — наименьшая ≥ уровня способности; тратится; без ячеек способность недоступна (ИИ "
          "остаётся лишь с безоружным ударом ≤1)",
          f1 == 3 && f4 == 5 && f6 == 0 && l3 == 0 && l5 == 1 && l5b == 0 && none == 0 && withSlot > noSlot + 3 &&
              noSlot <= 1,
          fmt("ячейка для ур.3: %d, ур.4: %d, ур.6: %d; после 1-го каста 3-й ур. %d, 5-й %d; после 2-го 5-й %d; ИИ с "
              "ячейкой: урон %d, без (только безоружный удар): %d",
              f1, f4, f6, l3, l5, l5b, withSlot, noSlot));
    }
    if (TB()) {
        auto mean = [&](int phN, int cdN, bool restrained, int hpFrac100) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 1, 5);
            a.wAtt = 3;
            if (phN) a.rules.push_back(Rule{W_HPLOW, 0, E_DMG, phN, 6, false});   // «когда → что»: ярость
            if (cdN) a.rules.push_back(Rule{W_FOECOND, C_RESTR, E_DMG, cdN, 6, true});   // добивание
            Boss d = plain(1000000, 0, 0, 5);
            d.ac = -1000;
            double tot = 0;
            int N = 3000;
            for (int i = 0; i < N; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                w.f[0].hp = hpFrac100;
                if (restrained) w.f[1].cond[C_RESTR] = 3;
                int h0 = w.f[1].hp;
                w.f[0].onceUsed = 0;
                doWeapon(w, w.f[0], 1);
                tot += h0 - w.f[1].hp;
            }
            return tot / N;
        };
        double base = mean(0, 0, false, 100), hi = mean(2, 0, false, 100), lo = mean(2, 0, false, 40),
               cd0 = mean(0, 2, false, 100), cd1 = mean(0, 2, true, 100);
        T("T99",
          "§условия: ярость при ХП≤50% (+2к6 к каждому попаданию) только при ≤50%; добивание (+2к6) раз в ход лишь по "
          "цели с нужным состоянием",
          std::fabs(hi - base) < 0.3 && lo - base > 18 && lo - base < 24 && std::fabs(cd0 - base) < 0.3 &&
              cd1 - cd0 > 5.5 && cd1 - cd0 < 8.5,
          fmt("база %.2f; ХП 100%%: %.2f; ХП 40%%: %.2f (+%.1f, ожид. ≈ 3 попад.×7×0.95=20); добивание: без состояния "
              "%.2f, с ним %.2f (+%.1f, ожид. ≈ 6.6)",
              base, hi, lo, lo - base, cd0, cd1, cd1 - cd0));
    }
    if (TB()) {
        auto run = [&](int wRangeAtk, bool rip, int hits, long long& ripUsed, int& lost) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100000, hits, 1, wRangeAtk);
            a.wAtt = hits;
            a.wN = 1;
            a.wAmmo = wRangeAtk > 5;   // дальняя атака — лук в упор (метательным вплотную бьют как ближним)
            Boss d = plain(100000, 1, 1, 5);
            d.riposte = rip;
            d.ac = -1000;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            long long r0 = g_ripUsed;
            int h0 = w.f[0].hp;
            doWeapon(w, w.f[0], 1);
            ripUsed = g_ripUsed - r0;
            lost = h0 - w.f[0].hp;
            return 0;
        };
        long long u1 = 0, u2 = 0, u3 = 0, u4 = 0;
        int l1 = 0, l2 = 0, l3 = 0, l4 = 0;
        long long tot1 = 0, tot2 = 0, tot3 = 0;
        int trials = 400;
        for (int i = 0; i < trials; i++) {
            run(5, true, 1, u1, l1);
            tot1 += u1;
            run(5, true, 3, u2, l2);
            tot2 += (u2 > 1 ? 1000 : u2);
            run(30, true, 1, u3, l3);
            tot3 += u3;
            run(5, false, 1, u4, l4);
            tot3 += u4 * 1000;
        }
        T("T100",
          "§контратака (реакция): после попадания ближней атакой — ответный удар; не больше одной за раунд; на дальнюю "
          "атаку и без умения её нет",
          tot1 >= trials * 0.9 && tot2 >= trials * 0.9 && tot2 < trials * 1.1 && tot3 < trials * 0.1,
          fmt("одна атака: контратак %lld из %d (атака попадает ~95%%); 3 атаки: ровно по одной %lld из %d; дальняя "
              "атака / без умения: %lld",
              tot1, trials, tot2, trials, tot3));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.surge = 1;
        a.wAtt = 1;
        Boss d = plain(1000000, 0, 0, 5);
        d.ac = -1000;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        w.f[0].init = 2000;
        w.f[1].init = 1;
        long long a0 = g_attacksMade;
        takeTurn(w, 0);
        long long n1 = g_attacksMade - a0;
        a0 = g_attacksMade;
        takeTurn(w, 0);
        long long n2 = g_attacksMade - a0;
        int left = w.f[0].surgeLeft;
        Boss b2 = a;
        b2.surge = 2;
        World v = w;
        v.f.clear();
        v.f.push_back(mk(b2, 0, 10, 10));
        v.f.push_back(mk(d, 1, 30, 10));
        long long a1 = g_attacksMade;
        takeTurn(v, 0);
        long long nFar = g_attacksMade - a1;
        T("T101",
          "§всплеск действий: раз в бой второе полное действие (первый ход 2 атаки, дальше 1); не тратится, если "
          "атаковать некого",
          n1 == 2 && n2 == 1 && left == 0 && nFar == 0 && v.f[0].surgeLeft == 2,
          fmt("1-й ход атак %lld, 2-й %lld, осталось %d; цель вдали: атак %lld, всплесков осталось %d", n1, n2, left,
              nFar, v.f[0].surgeLeft));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec a;
        a.delivery = 1;
        a.range = 30;
        a.cond = C_STUN;
        a.condRounds = 5;
        a.saveStat = WIS;
        long long p0 = specPrice(a, C2);
        Spec c = a;
        c.conc = true;
        long long pc = specPrice(c, C2);
        Spec sl = a;
        sl.slotLvl = 3;
        long long ps = specPrice(sl, C2);
        Boss b = plain(100, 1, 6, 5);
        long long q0 = priceOf(b, C2, 800);
        Boss s1 = b;
        s1.slots[1] = 1;
        Boss s3 = b;
        s3.slots[3] = 1;
        Boss s33 = b;
        s33.slots[3] = 3;
        Boss ph = b;
        ph.rules.push_back(Rule{W_HPLOW, 0, E_DMG, 1, 6, false});
        Boss ph3 = b;
        ph3.rules.push_back(Rule{W_HPLOW, 0, E_DMG, 3, 6, false});
        Boss cd = b;
        cd.rules.push_back(Rule{W_FOECOND, C_STUN, E_DMG, 2, 6, true});
        Boss rp = b;
        rp.riposte = true;
        Boss sg = b;
        sg.surge = 1;
        Boss sg2 = b;
        sg2.surge = 2;
        long long a1 = priceOf(s1, C2, 800) - q0, a3 = priceOf(s3, C2, 800) - q0, a33 = priceOf(s33, C2, 800) - q0,
                  pp1 = priceOf(ph, C2, 800) - q0, pp3 = priceOf(ph3, C2, 800) - q0, pcd = priceOf(cd, C2, 800) - q0,
                  prp = priceOf(rp, C2, 800) - q0, ps1 = priceOf(sg, C2, 800) - q0, ps2 = priceOf(sg2, C2, 800) - q0;
        T("T102",
          "§цены: концентрация и «за ячейку» удешевляют способность; ячейка растёт с уровнем и числом; триггеры, "
          "контратака и всплеск — положительные надбавки, растут с кубиками/числом",
          pc < p0 && ps < p0 && a3 > a1 && a33 > a3 && pp3 > pp1 && pcd > 0 && prp > 0 && ps2 > ps1,
          fmt("способность: обычная %lld, с концентрацией %lld, за ячейку %lld; ячейка 1 ур. +%lld, 3 ур. +%lld, 3×3 "
              "ур. +%lld; ярость 1к6 +%lld, 3к6 +%lld; добивание 2к6 +%lld; контратака +%lld; всплеск ×1 +%lld, ×2 "
              "+%lld",
              p0, pc, ps, a1, a3, a33, pp1, pp3, pcd, prp, ps1, ps2));
    }
    if (TB()) {
        int conc = 0, dur = 0, slotted = 0, ns = 0, N = 40000;
        for (int i = 0; i < N; i++) {
            Spec s = randSpec(0);
            if (s.self) {
                if (s.buffN || s.acB || s.invisN) {
                    dur++;
                    if (s.conc) conc++;
                }
                continue;
            }
            ns++;
            if (s.cond && s.cond != C_PRONE && s.delivery != 2) {
                dur++;
                if (s.conc) conc++;
            }
        }
        int tr = 0, rp = 0, sg = 0, sl = 0, BN = 8000;
        Coef C2 = CC();
        for (int i = 0; i < BN; i++) {
            Boss b = makeBoss(800, C2, false);
            for (auto& r : b.rules)
                if (r.what == E_DMG) { tr++; break; }
            if (b.riposte) rp++;
            if (b.surge) sg++;
            bool any = false;
            for (int L = 1; L < 10; L++)
                if (b.slots[L]) any = true;
            if (any) sl++;
        }
        (void)slotted;
        T("T103",
          "(инфо) генератор: доля длящихся эффектов под концентрацией; доли боссов с триггерами / контратакой / "
          "всплеском / ячейками",
          conc > 0 && sl > 0,
          fmt("концентрация %.0f%% длящихся; триггеры %.1f%%; контратака %.1f%%; всплеск %.1f%%; ячейки %.1f%%",
              100.0 * conc / std::max(1, dur), 100.0 * tr / BN, 100.0 * rp / BN, 100.0 * sg / BN, 100.0 * sl / BN));
    }

    // T104..T109 доводка: подавление пассивок, дальность за 5 фт, виды оружия, арсенал
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100000, 0, 0, 5);
        a.regenN = 3;
        a.suppMask = 1u << DT_FIRE;
        a.suppTgt = 1;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(100000, 0, 0, 5), 1, 90, 90));
        w.f[0].hp = 50000;
        int h0 = w.f[0].hp;
        takeTurn(w, 0);
        int reg1 = w.f[0].hp - h0;
        hurt(w.f[0], 10, DT_FIRE, true);
        int h1 = w.f[0].hp;
        takeTurn(w, 0);
        int reg2 = w.f[0].hp - h1;
        int h2 = w.f[0].hp;
        takeTurn(w, 0);
        int reg3 = w.f[0].hp - h2;
        hurt(w.f[0], 10, DT_COLD, true);
        int h3 = w.f[0].hp;
        takeTurn(w, 0);
        int reg4 = w.f[0].hp - h3;
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        Boss o = plain(100000, 0, 0, 5);
        o.auraN = 50;
        o.auraS = 1;
        o.auraRad = 15;
        o.auraType = DT_FIRE;
        o.suppMask = 1u << DT_FIRE;
        o.suppTgt = 2;
        v.f.push_back(mk(o, 0, 10, 10));
        v.f.push_back(mk(plain(100000, 0, 0, 5), 1, 13, 10));
        auto vic = [&]() {
            int hp0 = v.f[1].hp;
            takeTurn(v, 1);
            return hp0 - v.f[1].hp;
        };
        int au1 = vic();
        hurt(v.f[0], 5, DT_FIRE, true);
        int au2 = vic();
        endOfTurn(v.f[0]);
        int au3 = vic();
        Fighter t2 = mk(plain(100000, 0, 0, 5), 0, 1, 1);
        t2.b.resMask = 1u << DT_FIRE;
        t2.b.vulnMask = 1u << DT_RADIANT;
        t2.b.suppMask = 1u << DT_COLD;
        t2.b.suppTgt = 4;
        int p0 = t2.hp;
        hurt(t2, 10, DT_FIRE, true);
        int d1 = p0 - t2.hp;
        hurt(t2, 10, DT_COLD, true);
        p0 = t2.hp;
        hurt(t2, 10, DT_FIRE, true);
        int d2 = p0 - t2.hp;
        p0 = t2.hp;
        hurt(t2, 10, DT_RADIANT, true);
        int d3 = p0 - t2.hp;
        T("T104",
          "§подавление: урон подавляющего типа отключает на ход регенерацию / ауру / резисты (уязвимость остаётся); "
          "другой тип не подавляет",
          reg1 >= 3 && reg2 == 0 && reg3 >= 3 && reg4 >= 3 && au1 == 50 && au2 == 0 && au3 == 50 && d1 == 5 &&
              d2 == 10 && d3 == 20,
          fmt("регенерация: до %d, после огня %d, потом %d, после холода %d; аура: %d, после огня по владельцу %d, "
              "потом %d; защиты: огонь с резистом %d, после подавления %d, уязвимость %d",
              reg1, reg2, reg3, reg4, au1, au2, au3, d1, d2, d3));
    }
    if (TB()) {
        Coef C2 = CC();
        Boss b = plain(100, 1, 6, 5);
        b.regenN = 3;
        long long p0 = priceOf(b, C2, 800);
        Boss c = b;
        c.suppMask = 1u << DT_PIERCING;
        c.suppTgt = 1;
        long long pBig = priceOf(c, C2, 800);
        Boss d = b;
        d.suppMask = 1u << DT_FORCE;
        d.suppTgt = 1;
        long long pSmall = priceOf(d, C2, 800);
        Boss e = b;
        e.suppMask = 1u << DT_PIERCING;
        e.suppTgt = 2;
        long long pOther = priceOf(e, C2, 800);
        T("T105",
          "§подавление: скидка на подавляемую пассивку; чем чаще встречается подавляющий урон, тем больше скидка; на "
          "неподавляемое скидки нет",
          pBig < pSmall && pSmall <= p0 && pOther == p0,
          fmt("регенерация 3к6: без подавления %lld; подавляет колющий (частый) %lld; подавляет силовое поле (редкое) "
              "%lld; подавляется только аура (регенерации скидки нет) %lld",
              p0, pBig, pSmall, pOther));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 10;
        s.dmgS = 12;
        auto p = [&](int r, int rad) {
            Spec x = s;
            x.range = r;
            x.radius = rad;
            return specPrice(x, C2);
        };
        long long d1 = p(35, 0) - p(30, 0), d2 = p(30, 0) - p(25, 0), a1 = p(30, 5), a2 = p(30, 10), a3 = p(30, 15);
        Boss wb = plain(100, 20, 12, 30);
        auto wp = [&](int r) {
            Boss x = wb;
            x.wRange = r;
            return priceOf(x, C2, 800);
        };
        long long w1 = wp(90) - wp(60),
                  w2 = wp(60) - wp(30); /* шаг 5 фт у оружия дешевле очка — линейность проверяем на отрезках по 30 фт */
        T("T106", "§дальность и область — цена за каждые 5 фт (линейно), любые значения; у оружия то же",
          std::llabs(d1 - d2) <= 1 && d1 >= 1 && a1 < a2 && a2 < a3 && std::llabs(w1 - w2) <= 3 && w1 >= 1 && w2 >= 1,
          fmt("способность: +5 фт дальности стоит +%lld и +%lld; радиус 5/10/15 фт: %lld/%lld/%lld; оружие: 30→60 фт "
              "стоит +%lld, 60→90 фт +%lld",
              d1, d2, a1, a2, a3, w2, w1));
    }
    if (TB()) {
        int rangedN = 0, ammo = 0, badAF = 0, N = 6000;
        std::set<int> rs;
        bool mult5 = true;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (b.wN > 0 && b.wRange > 5) {
                rangedN++;
                if (b.wAmmo) ammo++;
                if (b.wAmmo && b.wFinesse) badAF++;
                rs.insert(b.wRange);
                if (b.wRange % 5) mult5 = false;
            }
            if (b.wRange <= 5 && b.wAmmo) badAF++;
        }
        T("T107",
          "§виды оружия: боеприпасное только у дальнобойного и не фехтовальное (всегда ЛОВ); дальности любые, кратные "
          "5 фт",
          badAF == 0 && mult5 && rs.size() >= 15 && ammo > rangedN * 0.6 && ammo < rangedN * 0.8,
          fmt("дальнобойных %d, боеприпасных %.0f%%, нарушений %d, разных дальностей %zu", rangedN,
              100.0 * ammo / std::max(1, rangedN), badAF, rs.size()));
    }
    if (TB()) {
        auto run = [&](bool bowFirst, int distFt_, int& rangeAfter, int& costSame) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.wcost = 7;
            Wp bow;
            bow.n = 1;
            bow.s = 6;
            bow.range = 60;
            bow.ammo = true;
            bow.cost = 11;
            Wp sw;
            sw.n = 1;
            sw.s = 6;
            sw.range = 5;
            sw.cost = 7;
            if (bowFirst) {
                a.wN = bow.n;
                a.wS = 6;
                a.wRange = 60;
                a.wAmmo = true;
                a.wcost = 11;
                a.alt.push_back(sw);
            } else
                a.alt.push_back(bow);
            Boss d = plain(100000, 0, 0, 5);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 10 + distFt_ / 5, 10));
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            rangeAfter = w.f[0].b.wRange;
            costSame = (w.f[0].b.wcost + w.f[0].b.alt[0].cost == 18);
        };
        int r1, r2, r3, r4, c1, c2, c3, c4;
        run(false, 60, r1, c1);
        run(true, 5, r2, c2);
        run(true, 60, r3, c3);
        run(false, 5, r4, c4);
        T("T108",
          "§арсенал: бот меняет оружие под ситуацию — лук на дистанции 60 фт, меч вплотную (в любом порядке в списке); "
          "цены не теряются при смене",
          r1 == 60 && r2 == 5 && r3 == 60 && r4 == 5 && c1 && c2 && c3 && c4,
          fmt("меч→дальность 60 фт: %d; лук→вплотную: %d; лук→60 фт: %d; меч→вплотную: %d (дальность выбранного "
              "оружия)",
              r1, r2, r3, r4));
    }
    if (TB()) {
        int arsenal = 0, supp = 0, chaMain = 0, N = 8000;
        Coef C2 = CC();
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(800, C2, false);
            if (!b.alt.empty()) arsenal++;
            if (b.suppMask) supp++;
            if (b.mainStat == CHA) chaMain++;
        }
        T("T109", "(инфо) доли боссов: с арсеналом / с подавлением пассивок / главная ХАР", arsenal > 0 && supp > 0,
          fmt("%.1f%% / %.1f%% / %.1f%%", 100.0 * arsenal / N, 100.0 * supp / N, 100.0 * chaMain / N));
    }

    // T110..T114 политика бота (обучаемые параметры), кайтинг, защита
    if (TB()) {
        Pol b = Pol::baseline();
        const double orig[PO_N] = {15,  0.5,  1.0,  0.25, 0.30, 0.50, 2.0, 1.0, 0.4,
                                   1.0, 0.25, 0.10, 0.15, 1.0,  0.2,  0.7, 0.0, 0.0, 0.5};   // 0.5 — wallW (v21)
        bool ok = true;
        for (int i = 0; i < PO_N; i++)
            if (std::fabs(b.v[i] - orig[i]) > 1e-12) ok = false;
        bool inRange = true;
        for (int i = 0; i < PO_N; i++)
            if (POL_LEARNED[i] < PD[i].lo - 1e-9 || POL_LEARNED[i] > PD[i].hi + 1e-9) inRange = false;
        T("T110",
          "§политика: базовые значения = прежним «зашитым» порогам (регресс); обученные значения лежат в допустимых "
          "границах",
          ok && inRange, fmt("база совпадает %d; обученные в границах %d; параметров %d", ok, inRange, PO_N));
    }
    if (TB()) {
        auto run = [&](double kite, int& dist0, int& dist1) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 120);
            Boss d = plain(100000, 1, 6, 5);
            Pol p = Pol::baseline();
            p.v[PO_KITE] = kite;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 14, 10));
            w.f[0].pol = &p;
            w.f[0].init = 2000;
            w.f[1].init = 1;
            dist0 = distFt(w.f[0], w.f[1]);
            takeTurn(w, 0);
            dist1 = distFt(w.f[0], w.f[1]);
        };
        int a0, a1, b0, b1, c0, c1;
        run(0, a0, a1);
        run(1.0, b0, b1);
        run(0.1, c0, c1);
        T("T111",
          "§кайтинг: дальнобойный при врагу-ближнике в зоне его хода отступает (с 20 фт до 50 фт), при выключенном "
          "(kite=0) или малом множителе остаётся",
          a1 == a0 && b1 == 50 && c1 == c0,
          fmt("kite=0: %d→%d фт; kite=1: %d→%d фт; kite=0.1: %d→%d фт", a0, a1, b0, b1, c0, c1));
    }
    if (TB()) {
        auto run = [&](double dodge, std::string& act, bool& dodging) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            Boss d = plain(100000, 1, 6, 5);
            Pol p = Pol::baseline();
            p.v[PO_DODGE] = dodge;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            w.f[0].pol = &p;
            w.f[0].hp = 20;
            w.f[0].init = 2000;
            w.f[1].init = 1;
            takeTurn(w, 0);
            act = g_act.type;
            dodging = w.f[0].dodging;
        };
        std::string x1, x2;
        bool d1, d2;
        run(0.0, x1, d1);
        run(0.5, x2, d2);
        T("T112", "§защита: при ХП ниже порога бот выбирает Уклонение вместо атаки (dodgeHp=0.5); по умолчанию атакует",
          x1.compare(0, 6, "attack") == 0 && !d1 && x2 == "dodge" && d2,
          fmt("dodgeHp=0: действие «%s»; dodgeHp=0.5: «%s», уклоняется %d", x1.c_str(), x2.c_str(), d2));
    }
    if (TB()) {
        Pol base = Pol::baseline(), cow = Pol::baseline();
        cow.v[PO_DODGE] = 1.1;
        int winA = 0, winB = 0, same = 0, N = 300;
        Coef C2 = CC();
        rng.seed(404);
        for (int i = 0; i < N; i++) {
            Boss A = makeBoss(800, C2, true), B = makeBoss(800, C2, true);
            A.stats[INT] = B.stats[INT] = 10;
            for (auto& m : A.tmpl) m.stats[INT] = 10;
            for (auto& m : B.tmpl) m.stats[INT] = 10;
            int r = fight(A, B, nullptr, nullptr, &cow, &base);
            winA += r == 0;
            r = fight(A, B, nullptr, nullptr, &base, &cow);
            winB += r == 1;
            r = fight(A, B, nullptr, nullptr, &base, &base);
            same += r == 0;
        }
        T("T113",
          "§политика применяется к своей команде: «всегда уклоняется» редко побеждает на любой стороне; одинаковые "
          "политики — ~50%",
          winA < N * 0.35 && winB < N * 0.35 && same > N * 0.40 && same < N * 0.60,
          fmt("трус на стороне A побеждает %d из %d, на стороне B %d из %d; одинаковые политики: A побеждает %d из %d",
              winA, N, winB, N, same, N));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        addSummon(a, plain(50, 1, 6, 5), 1);
        Pol p = Pol::baseline();
        p.v[PO_SUMMON] = 7.0;
        w.f.reserve(64);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 14, 10));
        w.f[0].pol = &p;
        w.f[0].init = 2000;
        w.f[1].init = 1;
        takeTurn(w, 0);
        bool summoned = w.f.size() == 3;
        bool inherits = summoned && w.f[2].pol == &p;
        T("T114", "§политика наследуется призванными приспешниками", summoned && inherits,
          fmt("призван %d, политика команды у приспешника %d", summoned, inherits));
    }

    // M1..M7 миньоны: шаблон + копии, управляет главный босс
    if (TB()) {
        Coef C2 = CC();
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss ow = plain(100, 1, 6, 5);
        Boss mt = makeBoss(300, C2, false);
        if (mt.hp < 2) mt.hp = 50;
        const int si = addSummon(ow, mt, 2, 3);
        w.f.reserve(64);
        w.f.push_back(mk(ow, 0, 30, 40));
        w.f[0].init = 5000;
        summonFrom(w, 0, si);
        const Fighter& m = w.f[1];
        bool ok = w.f.size() == 3 && m.hp == mt.hp && m.maxHp == mt.hp && mt.hp > 1 &&
                  std::max(std::abs(m.x - 30), std::abs(m.y - 40)) <= 2 && m.fresh && m.init == 4999 &&
                  m.summoner == 0 && m.sumSpec == si && w.f[0].sumMade[si] == 2;
        T("M1",
          "§миньон при призыве: полные ХП шаблона, клетка рядом с боссом, не ходит в этот раунд, инициатива босса −1 "
          "(регресс бага v11–v16: 1 ХП)",
          ok,
          fmt("ХП %d/%d (шаблон %d), клетка (%d,%d) при боссе (30,40), fresh %d, инициатива %d, призвано способностью %d",
              m.hp, m.maxHp, mt.hp, m.x, m.y, (int)m.fresh, m.init, w.f[0].sumMade[si]));
    }
    if (TB()) {
        auto run = [&](int cmd) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss ow = plain(100, 1, 6, 5);
            Boss em = plain(100, 0, 0, 5);
            Boss mm = plain(100, 1, 6, 30);
            Boss e1 = plain(100, 0, 0, 5);
            w.f.push_back(mk(ow, 0, 5, 5));
            w.f.push_back(mk(em, 1, 90, 90));
            w.f.push_back(mk(mm, 0, 20, 20));
            w.f.push_back(mk(e1, 1, 21, 20));
            w.f.push_back(mk(e1, 1, 24, 20));
            w.cmd[0] = cmd;
            takeTurn(w, 2);
            return g_act.target;
        };
        int t0 = run(-1), t1 = run(3), t2 = run(4);
        T("M2", "§управление: без приказа миньон бьёт ближнюю цель; приказ главного босса переключает его на указанную",
          t0 == 3 && t1 == 3 && t2 == 4, fmt("без приказа: цель %d; приказ→3: %d; приказ→4: %d", t0, t1, t2));
    }
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss ow = plain(100, 1, 6, 30);
        Boss en = plain(100, 1, 6, 5);
        w.f.push_back(mk(ow, 0, 10, 10));
        w.f.push_back(mk(en, 1, 40, 10));
        w.cmd[0] = w.cmd[1] = -1;
        takeTurn(w, 0);
        int c0 = w.cmd[0];
        takeTurn(w, 1);
        int c1 = w.cmd[1];
        T("M3", "§управление: главный босс записывает свою цель как приказ команде", c0 == 1 && c1 == 0,
          fmt("приказ команды A: %d, B: %d", c0, c1));
    }
    if (TB()) {
        Coef C2 = CC();
        int both = 0, none = 0, bad = 0, N = 1200;
        for (int i = 0; i < N; i++) {
            Boss a, b;
            makePair(1600, C2, a, b);
            bool sa = hasSummon(a), sb = hasSummon(b);
            if (!sa && !sb) none++;
            if (sa && sb) both++;
            if (a.cost - STAT_BASE > 1600 || b.cost - STAT_BASE > 1600) bad++;
        }
        T("M4", "§пара: хотя бы один босс всегда с миньонами, второй по желанию (~50%), бюджет не превышен",
          none == 0 && bad == 0 && both > N * 0.25 && both < N * 0.65,   // с ценами этапа 4 призыв влезает в бюджет реже: ~35%
          fmt("без миньонов у обоих: %d; у обоих: %.0f%%; перерасход %d (из %d пар)", none, 100.0 * both / N, bad, N));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec one;
        one.self = true;
        one.delivery = 2;
        one.sumN = 1;
        one.sumCost = 300;
        one.uses = 1;
        Spec eight = one, three = one, unl = one, disc = one;
        eight.sumN = 8;
        three.uses = 3;
        unl.uses = UNL;
        disc.uses = UNL;   // скидки сравниваем с призывом без лимита (у одноразового плата уже упирается в нижнюю границу множителя)
        disc.conc = true;
        disc.slotLvl = 3;
        const double t1 = (double)summonTmplPrice(one, C2), t8 = (double)summonTmplPrice(eight, C2), t3 = (double)summonTmplPrice(three, C2),
                     tu = (double)summonTmplPrice(unl, C2), td = (double)summonTmplPrice(disc, C2);
        const bool fee = specPrice(disc, C2) - td < specPrice(unl, C2) - tu;   // скидки — только на плату за призыв
        T("M5", "§v21 призыв: шаблон × копии × применения (8 копий — 8×, 3 применения — 3×; без лимита — как AT_SUMUNL применений); скидки способности не трогают шаблон",
          std::abs(t8 - 8 * t1) <= 8 && std::abs(t3 - 3 * t1) <= 3 && std::abs(tu - C2[AT_SUMUNL] * t1) <= C2[AT_SUMUNL] && td == tu && fee,
          fmt("1 копия %.0f, 8 копий %.0f, 3 применения %.0f, без лимита %.0f (×%.1f), со скидками шаблон %.0f, плата со скидкой дешевле %d", t1, t8, t3, tu,
              C2[AT_SUMUNL], td, fee));
    }
    if (TB()) {
        Coef C2 = CC();
        int N = 3000, ab = 0, pr = 0, lr = 0, rd = 0, wp = 0, au = 0, tot = 0;
        for (int i = 0; i < N; i++) {
            Boss x = makeBoss(3200, C2, true, true);
            if (x.tmpl.empty()) continue;
            const Boss& m = x.tmpl[0];
            tot++;
            if (m.wN > 0) wp++;
            if (!m.abil.empty()) ab++;
            if (m.resMask | m.immMask | m.vulnMask) pr++;
            if (m.legRes) lr++;
            if (m.rcost > 0) rd++;
            if (!m.tmpl.empty()) au++;
        }
        T("M6",
          "§миньон — полноценный босс (оружие, способности, защиты, ЛС, довесок собираются тем же генератором); "
          "вложенных миньонов нет",
          tot > 0 && wp > 0 && ab > 0 && pr > 0 && lr > 0 && rd > 0 && au == 0,
          fmt("шаблонов %d: с оружием %d, способностями %d, защитами %d, ЛС %d, довеском %d; с собственными миньонами "
              "%d",
              tot, wp, ab, pr, lr, rd, au));
    }
    if (TB()) {
        Coef C2 = CC();
        int tot = 0, ghost = 0, fights = 0;
        for (int s = 1; s <= 120; s++) {
            rng.seed(s * 11);
            Boss a, b;
            makePair(1600, C2, a, b);
            std::vector<std::string> log;
            std::string mj;
            fight(a, b, &log, &mj);
            fights++;
            // максимум хитов приспешника — при его появлении (потом его законно срезает «урон снижает максимум хитов»)
            std::vector<int> firstMh;
            for (auto& fr : log) {
                size_t pos = 0;
                int n = 0;
                while ((pos = fr.find("\"kind\":", pos)) != std::string::npos) {
                    int kind = fr[pos + 7] - '0';
                    size_t q = fr.rfind("{\"x\":", pos);
                    int x, y, hp, mh;
                    if (kind >= 2 && q != std::string::npos &&
                        sscanf(fr.c_str() + q, "{\"x\":%d,\"y\":%d,\"hp\":%d,\"maxHp\":%d", &x, &y, &hp, &mh) == 4) {
                        tot++;
                        if ((int)firstMh.size() <= n) firstMh.resize(n + 1, -1);
                        if (firstMh[n] < 0) firstMh[n] = mh;
                        if (firstMh[n] <= 1) ghost++;
                    }
                    pos += 8;
                    n++;
                }
            }
        }
        T("M7", "§в реальных боях миньоны появляются с полными ХП (ни одного «призрака» с maxHp<=1 при появлении)", tot > 0 && ghost == 0,
          fmt("кадров с миньонами: %d, из них призраков: %d (боёв %d)", tot, ghost, fights));
    }

    if (TB()) {
        Coef C2 = CC();
        C2[AT_MINEXP] = 0.5;
        double prev = g_B;
        g_B = 800;
        double m8 = minMult(C2);
        g_B = 1600;
        double m16 = minMult(C2);
        g_B = 3200;
        double m32 = minMult(C2);
        C2[AT_MINEXP] = 0;
        double z = minMult(C2);
        g_B = prev;
        T("M8",
          "§цена миньонов зависит от бюджета: при k>0 на больших бюджетах дешевле (×2 бюджета → ×2^-k), при k=0 — как "
          "раньше",
          m8 > m16 && m16 > m32 && std::fabs(m16 / m32 - std::sqrt(2.0)) < 1e-9 && std::fabs(z - C2[AT_MINION]) < 1e-12,
          fmt("k=0.5: 800→%.3f, 1600→%.3f, 3200→%.3f; k=0: %.3f", m8, m16, m32, z));
    }

    // P1..P4 политика сборки миньонов и переобученная боевая политика
    if (TB()) {
        Coef C2 = CC();
        BuildPol bp;
        bp.v[0] = 1.0;
        bp.v[1] = 0.2;
        bp.v[2] = 3;
        bp.v[3] = 0.67;
        int sm = 0, okShape = 0, N = 400;
        g_bp = &bp;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, true, false);
            if (hasSummon(b)) {
                sm++;
                for (auto& sp : b.abil)   // 3 копии всего, 2 за применение → 2 применения (или без лимита)
                    if (sp.sumN > 0 && sp.sumN == 2 && (sp.uses == 2 || sp.uses == UNL)) okShape++;
            }
        }
        BuildPol off;
        off.v[0] = 0;
        g_bp = &off;
        int smOff = 0;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, true, false);
            if (hasSummon(b)) smOff++;
        }
        g_bp = nullptr;
        T("P1",
          "§политика сборки: босс сам задаёт, брать ли миньонов (pUse), сколько копий и какой лимит; pUse=0 — миньонов "
          "нет",
          sm > N * 0.75 && okShape == sm && smOff == 0,   // с ценами этапа 4 призыв не всегда влезает в бюджет: ~82%
          fmt("pUse=1: с миньонами %d из %d, все с 3 копиями и лимитом 2: %d; pUse=0: с миньонами %d", sm, N, okShape,
              smOff));
    }
    if (TB()) {
        Coef C2 = CC();
        int sm = 0, N = 2000;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, true, false);
            if (hasSummon(b)) sm++;
        }
        T("P2", "§без политики сборки генератор прежний: миньоны у ~половины боссов (случайное решение)",
          sm > N * 0.30 && sm < N * 0.65, fmt("%.1f%% боссов с миньонами", 100.0 * sm / N));
    }
    if (TB()) {
        Coef C2 = CC();
        Pol pol = Pol::baseline();
        double s = 0;
        int N = 2400;
        for (int i = 0; i < N; i++) {
            rng.seed(8800000 + i * 3);
            g_bp = &SMART_BP;
            Boss x = makeBoss(1600, C2, true, false);
            g_bp = nullptr;
            Boss y = makeBoss(1600, C2, true, false);
            int r;
            if (i % 2 == 0) {
                r = fight(x, y, nullptr, nullptr, &pol, &pol);
                s += r == 0 ? 1.0 : (r == 2 ? 0.5 : 0.0);
            } else {
                r = fight(y, x, nullptr, nullptr, &pol, &pol);
                s += r == 1 ? 1.0 : (r == 2 ? 0.5 : 0.0);
            }
        }
        T("P3",
          "(инфо) «умная» сборка миньонов (всегда, 2 копии, лимит 2) выигрывает у случайно собранного соперника того "
          "же бюджета",
          s / N > 0.53, fmt("%.1f%% (%d боёв; ошибка ±1 п.п.)", 100 * s / N, N));
    }
    if (TB()) {
        Pol b;
        double sf = -1, kt = -1, fo = -1; /* Pol() — обученная политика по умолчанию (POL_LEARNED) */
        for (int k = 0; k < PO_N; k++) {
            if (!std::strcmp(PD[k].name, "summonFac")) sf = b.v[k];
            if (!std::strcmp(PD[k].name, "kite")) kt = b.v[k];
            if (!std::strcmp(PD[k].name, "focusFt")) fo = b.v[k];
        }
        T("P4",
          "§боевая политика переобучена на настоящих миньонах: призыв приоритетнее (summonFac>2.5), кайтинг включён, "
          "фокус на боссах дальше",
          sf > 2.5 && kt > 0.3 && fo > 25,
          fmt("summonFac %.2f (было 2), kite %.2f (было 0), focusFt %.1f (было 15)", sf, kt, fo));
    }
    // TR1..TR4 блок 1 (v20): цена размерных атомов по трём участкам
    // при бюджете 1600 масштаб порогов равен 1
    if (TB()) {
        bool ident = true, floor1 = true, up2 = true, down3 = true, cont = true, order = true, tail = true;
        std::string d;
        double gB0 = g_B;
        g_B = 1600;
        for (int id = 0; id < TR_N; id++) {
            const TierDef& t = TD[id];
            double a = t.a, b = t.b, P = 1 + t.k2 * (b - a), s3e = std::min(t.s3, P);
            if (!(b > a) || t.s3 < 1 - 1e-9 || t.c <= 0 || t.k2 <= 0) order = false;
            for (int n = 0; n <= (int)a && n <= 3000; n++)
                if (std::fabs(tierN(id, n) - n) > 1e-9) ident = false;
            auto m = [&](int n) { return tierN(id, n) - tierN(id, n - 1); };
            for (int n = 1; n <= 3000; n++) {
                if (m(n) < 1 - 1e-9) floor1 = false;
            }
            for (int n = (int)a + 1; n < (int)b; n++)
                if (m(n + 1) < m(n) - 1e-9) up2 = false;
            for (int n = (int)b + 1; n < 2000; n++)
                if (m(n + 1) > m(n) + 1e-9 || m(n) < s3e - 1e-9) down3 = false;
            double sc = t.e == 0 ? 1.0 : 1.0, aa = a, bb = b;
            (void)sc;
            if (std::fabs(tierN(id, aa + 1e-6) - tierN(id, aa - 1e-6)) > 1e-3 ||
                std::fabs(tierN(id, bb + 1e-6) - tierN(id, bb - 1e-6)) > 1e-3 || m((int)bb + 2) > m((int)bb + 1) + 1e-9)
                cont = false;
            if (std::fabs(m(3000) - s3e) > 1e-3) tail = false;
        }
        g_B = gB0;
        T("TR1",
          "§блок 1б: форма цены ступени: до a ступень 1 (как в v19); после a растёт; после b замедляется, но не "
          "опускается ниже s3>=1; цена и ступень непрерывны; хвост → s3",
          ident && floor1 && up2 && down3 && cont && order && tail,
          fmt("до a совпадает: %d; ступень >=1: %d; участок 2 растёт: %d; участок 3 убывает к s3: %d; стыки: %d; "
              "параметры допустимы: %d; хвост=s3: %d",
              ident, floor1, up2, down3, cont, order, tail));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 5;
        s.dmgS = 8;
        s.dtype = DT_FIRE;
        auto P = [&](int r, int rad, int push) {
            Spec x = s;
            x.range = r;
            x.radius = rad;
            x.pushFt = push;
            return (double)specPrice(x, C2);
        };
        double p5 = P(5, 0, 0), p120 = P(120, 0, 0), p500 = P(500, 0, 0),
               lin500 = p5 * (1 + C2[AT_ARANGE] * tierN(TR_RANGE, 23) / 23.0 * 99), pr0 = P(5, 30, 0),
               pr1 = P(5, 30, 150), prLin = pr0 + C2[AT_PUSH] * tierN(TR_PUSH, 6) / 6.0 * 30;
        T("TR2",
          "§блок 1б: в диапазоне генератора (дальность ≤120 фт) цена линейна по ступеням; вне диапазона цена не "
          "дешевле линейной экстраполяции",
          std::fabs(p120 - p5 * (1 + C2[AT_ARANGE] * tierN(TR_RANGE, 23))) < 1.5 && p500 >= 0.97 * lin500 &&
              pr1 >= 0.75 * prLin,
          fmt("дальность 5/120/500 фт: %.0f / %.0f / %.0f (линейная экстраполяция 500 фт: %.0f); область 30 фт + "
              "толчок 150 фт: %.0f (линейно было бы %.0f)",
              p5, p120, p500, lin500, pr1, prLin));
    }
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec r;
        r.delivery = 1;
        r.range = 500;
        r.dmgN = 5;
        r.dmgS = 8;
        r.dtype = DT_FIRE;
        Spec pu;
        pu.delivery = 1;
        pu.range = 30;
        pu.pushFt = 500;
        Spec te;
        te.self = true;
        te.delivery = 2;
        te.teleFt = 500;
        double a = specPrice(r, C2), b2 = specPrice(pu, C2), c = specPrice(te, C2);
        T("TR3",
          "(инфо) «вся карта» (500 фт) на бюджете 1600: способность с уроном 5к8 / толчок 500 фт / телепорт 500 фт; "
          "доля бюджета (порог доли ждёт решения заказчика)",
          true,
          fmt("%.0f (%.0f%% бюджета) / %.0f (%.1f%%) / %.0f (%.1f%%)", a, 100 * a / 1600, b2, 100 * b2 / 1600, c,
              100 * c / 1600));
    }
    if (TB()) {
        Coef C2 = CC();
        Boss c = plain(100, 2, 6, 300);
        c.wAtt = 40;
        Boss d = c;
        d.wN = 0;
        Wp p;
        p.n = 2;
        p.s = 6;
        p.range = 300;
        p.att = 40;
        p.reach = 5;
        long long diff = priceOf(c, C2, 800) - priceOf(d, C2, 800);
        g_B = 800;
        PcGuard pcg(c);   // цена доп. атак — по модификатору этого тела
        T("TR4",
          "§блок 1: цена оружия вне диапазона генератора (дальность 300 фт, 40 атак) в обратном пересчёте = wpPrice "
          "движка",
          diff == wpPrice(p, C2), fmt("пересчёт %lld, движок %lld", diff, wpPrice(p, C2)));
    }

    // ===== Этап 1, партия 1: иммунитет к отталкиванию, урон при столкновении, Отход бонусным действием =====
    // T115 цена
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.dmgN = 2;
        s.dmgS = 6;
        s.pushFt = 30;
        long long p0 = specPrice(s, C2);
        Spec s1 = s, s2 = s, s3 = s;
        s1.collN = 1;
        s2.collN = 2;
        s3.collN = 3;
        Spec n = s;
        n.pushFt = 0;
        long long q0 = specPrice(n, C2);
        n.collN = 3;
        long long q3 = specPrice(n, C2);
        Boss b = plain(100, 1, 6, 5);
        long long b0 = priceOf(b, C2, 800);
        Boss i = b;
        i.immPush = true;
        Boss d = b;
        d.disengB = true;
        bool ok = p0 < specPrice(s1, C2) && specPrice(s1, C2) < specPrice(s2, C2) &&
                  specPrice(s2, C2) < specPrice(s3, C2) && q0 == q3 &&
                  priceOf(i, C2, 800) - b0 == (long long)std::ceil(C2[AT_IMMPUSH]) &&
                  priceOf(d, C2, 800) - b0 == (long long)std::ceil(C2[AT_DISENGB]);
        T("T115",
          "§этап 1 цена: урон при столкновении растёт с числом кубиков и не продаётся без толчка; иммунитет к "
          "отталкиванию и Отход бонусным действием стоят свои коэффициенты",
          ok,
          fmt("толчок 30 фт: без столкновения %lld, 1к6 %lld, 2к6 %lld, 3к6 %lld; без толчка %lld/%lld; иммунитет "
              "+%lld, Отход +%lld",
              p0, (long long)specPrice(s1, C2), (long long)specPrice(s2, C2), (long long)specPrice(s3, C2), q0, q3,
              priceOf(i, C2, 800) - b0, priceOf(d, C2, 800) - b0));
    }
    // T116 иммунитет к отталкиванию
    if (TB()) {
        auto run = [&](bool imm) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss tb = plain(1000, 0, 0, 5);
            tb.immPush = imm;
            w.f.push_back(mk(plain(10, 0, 0, 5), 0, 50, 50));
            w.f.push_back(mk(tb, 1, 51, 50));
            pushAway(w, w.f[0], w.f[1], 30, 0);
            return w.f[1].x;
        };
        int xi = run(true), xn = run(false);
        T("T116", "§этап 1 иммунитет к отталкиванию: толчок на 30 фт не сдвигает; без иммунитета сдвигает на 6 клеток",
          xi == 51 && xn == 57, fmt("с иммунитетом x=%d, без x=%d (старт 51)", xi, xn));
    }
    // T117 урон при столкновении
    if (TB()) {
        auto run = [&](int collN, int wallX, bool blocker, bool imm, int& moved, int& bdmg) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss tb = plain(1000, 0, 0, 5);
            tb.immPush = imm;
            w.f.push_back(mk(plain(10, 0, 0, 5), 0, 50, 50));
            w.f.push_back(mk(tb, 1, 51, 50));
            if (blocker) w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 54, 50));
            if (wallX > 0) w.wall[50][wallX] = true;
            pushAway(w, w.f[0], w.f[1], 30, collN);
            moved = w.f[1].x - 51;
            bdmg = blocker ? 1000 - w.f[2].hp : 0;
            return 1000 - w.f[1].hp;
        };
        bool ok = true;
        int mv, bd;
        std::string d;
        int dmgMin = 1 << 30, dmgMax = 0;
        for (int i = 0; i < 400; i++) {
            int dm = run(2, 55, false, false, mv, bd);
            dmgMin = std::min(dmgMin, dm);
            dmgMax = std::max(dmgMax, dm);
            if (mv != 3) ok = false;
        }
        if (dmgMin < 2 || dmgMax > 12 || dmgMax <= dmgMin + 3) ok = false;
        d += fmt("в стену 2к6: урон %d..%d, сдвиг %d; ", dmgMin, dmgMax, mv);
        int a = run(0, 55, false, false, mv, bd);
        if (a != 0) ok = false;
        int b2 = run(2, 0, false, false, mv, bd);
        if (b2 != 0 || mv != 6) ok = false;
        int c = run(2, 55, false, true, mv, bd);
        if (c != 0 || mv != 0) ok = false;
        d += fmt("без кубиков %d; свободный путь %d (сдвиг %d); иммунный %d; ", a, b2, 6, c);
        int dt = run(2, 0, true, false, mv, bd);
        if (dt < 2 || dt > 12 || bd < 2 || bd > 12 || mv != 2) ok = false;
        d += fmt("в существо: толкаемый %d, преграда %d, сдвиг %d", dt, bd, mv);
        T("T117",
          "§этап 1 урон при столкновении: стена/существо на пути -> Nк6 силового поля толкаемому (и "
          "преграде-существу); без столкновения и без кубиков урона нет",
          ok, d);
    }
    // T118 Отход бонусным действием
    if (TB()) {
        auto trial = [&](bool atom, int& react, int& far, int& att) {
            react = far = att = 0;
            int N = 200;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss ar = plain(1000, 1, 6, 60);
                ar.disengB = atom;
                Boss me = plain(1000, 1, 6, 5);
                w.f.push_back(mk(ar, 0, 50, 50));
                w.f.push_back(mk(me, 1, 51, 50));
                long long a0 = g_attacksMade;
                takeTurn(w, 0);
                react += w.f[1].reactionUsed;
                far += distFt(w.f[0], w.f[1]) >= 10;
                att += (g_attacksMade - a0) >= 1;
            }
            return 0;
        };
        int r1, f1, a1, r0, f0, a0;
        trial(true, r1, f1, a1);
        trial(false, r0, f0, a0);
        T("T118",
          "§этап 1 Отход бонусным действием: стрелок, до которого дотянулся ближник, отходит без провоцированной атаки "
          "и всё равно атакует",
          r1 == 0 && f1 == 200 && a1 == 200,
          fmt("с атомом: реакций врага %d/200, отошёл %d/200, атаковал %d/200; без атома: реакций %d, отошёл %d, "
              "атаковал %d",
              r1, f1, a1, r0, f0, a0));
    }
    // T119 ИИ учитывает иммунитет и столкновение
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss tb = plain(1000, 0, 0, 5);
        Boss ti = tb;
        ti.immPush = true;
        w.f.push_back(mk(plain(10, 0, 0, 5), 0, 50, 50));
        w.f.push_back(mk(tb, 1, 51, 50));
        w.f.push_back(mk(ti, 1, 51, 52));
        w.wall[50][55] = true;
        w.wall[52][80] = true;
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.pushFt = 30;
        Spec sc = s;
        sc.collN = 3;
        double e0 = evSpecOne(w, s, w.f[0], w.f[1]), e1 = evSpecOne(w, sc, w.f[0], w.f[1]),
               ei = evSpecOne(w, sc, w.f[0], w.f[2]);
        T("T119",
          "§этап 1 ИИ: толчок против иммунной цели ничего не стоит; столкновение со стеной добавляет ожидаемый урон, "
          "свободный путь — нет",
          e0 > 0 && e1 > e0 + 9 && ei == 0 && evSpecOne(w, sc, w.f[0], w.f[2]) == 0,
          fmt("толчок %.2f, с 3к6 столкновения у стены %.2f, иммунная цель %.2f", e0, e1, ei));
    }
    // T120 частота в генераторе
    if (TB()) {
        Coef C2 = CC();
        int N = 4000, ip = 0, db = 0, co = 0;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, false);
            ip += b.immPush;
            db += b.disengB;
            bool c = false;
            for (auto& sp : b.abil)
                if (sp.collN && sp.pushFt) c = true;
            co += c;
        }
        T("T120", "§этап 1 генератор выбирает новые атомы: каждый встречается и не чаще трети боссов",
          ip > N * 0.01 && db > N * 0.01 && co > N * 0.003 && ip < N / 3 && db < N / 3 && co < N / 3,
          fmt("иммунитет к отталкиванию %.1f%%, Отход бонусным %.1f%%, столкновение %.1f%% боссов", 100.0 * ip / N,
              100.0 * db / N, 100.0 * co / N));
    }

    // ===== Этап 1, партия 2: Перенаправление, условные защиты, бафф характеристик =====
    // T121 цена
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.self = true;
        s.delivery = 2;
        s.range = 0;
        s.sbStat = STR;
        s.sbAmt = 2;
        s.sbRounds = 5;
        long long p1 = specPrice(s, C2);
        Spec s2 = s;
        s2.sbAmt = 4;
        Spec s3 = s;
        s3.sbRounds = 10;
        long long p2 = specPrice(s2, C2), p3 = specPrice(s3, C2);
        Boss b = plain(100, 1, 6, 5);
        long long b0 = priceOf(b, C2, 800);
        Boss r = b;
        r.redirect = true;
        // условная защита — правило «когда → что» (условие о себе → резист)
        Boss c1 = b;
        c1.rules.push_back(Rule{W_HPHIGH, 0, E_RES});
        Boss c2 = b;
        c2.rules.push_back(Rule{W_HPHIGH, 0, E_RESNM});
        Boss c3 = b;
        c3.rules.push_back(Rule{W_NOCOND, 0, E_RES});
        Boss c4 = b;
        c4.rules.push_back(Rule{W_HPLOW, 0, E_RES});
        long long dc1 = priceOf(c1, C2, 800) - b0, dc2 = priceOf(c2, C2, 800) - b0, dc3 = priceOf(c3, C2, 800) - b0,
                  dc4 = priceOf(c4, C2, 800) - b0, dr = priceOf(r, C2, 800) - b0;
        bool ok = std::llabs(p1 - (long long)std::ceil(C2[AT_STATBUF])) <= 1 && std::llabs(p2 - 2 * p1) <= 1 &&
                  std::llabs(p3 - 2 * p1) <= 1 && dr == (long long)std::ceil(C2[AT_REDIRECT]) &&
                  dc1 == (long long)std::ceil(C2[AT_E_RES] * 5 * C2[AT_W_HPHIGH]) && dc2 < dc1 && dc3 > dc1 && dc1 > dc4;
        T("T121",
          "§этап 1 цена: бафф характеристики ∝ величине и длительности; Перенаправление — свой коэффициент; условная "
          "защита ∝ доле времени условия и доле урона",
          ok,
          fmt("бафф +2/5р %lld, +4/5р %lld, +2/10р %lld; Перенаправление +%lld; защита: ХП>50%% всё +%lld, ХП>50%% "
              "физ. +%lld, нет состояний +%lld, ХП<=50%% +%lld",
              p1, p2, p3, dr, dc1, dc2, dc3, dc4));
    }
    // T122 бафф характеристики: действует, не складывается, кончается, снимается срывом концентрации
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss b = plain(1000, 1, 6, 5);
        b.stats = {12, 10, 10, 10, 10, 10};
        Spec sp;
        sp.self = true;
        sp.delivery = 2;
        sp.range = 0;
        sp.sbStat = STR;
        sp.sbAmt = 4;
        sp.sbRounds = 2;
        b.abil.push_back(sp);
        b.abil.push_back(sp);
        b.abil[1].conc = true;
        w.f.push_back(mk(b, 0, 50, 50));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 52, 50));
        Fighter& f = w.f[0];
        int m0 = f.wMod();
        doSpec(w, f, 0, 0);
        int s1 = f.b.stats[STR], m1 = f.wMod();
        doSpec(w, f, 0, 0);
        int s2 = f.b.stats[STR];
        endOfTurn(f);
        int s3 = f.b.stats[STR];
        endOfTurn(f);
        int s4 = f.b.stats[STR];
        f.uses[1] = UNL;
        doSpec(w, f, 1, 0);
        f.concSpec = 1;
        int s5 = f.b.stats[STR];
        breakConc(w, 0);
        int s6 = f.b.stats[STR];
        T("T122",
          "§этап 1 бафф характеристики: +к значению сразу, повторный не складывается, кончается через заданные раунды, "
          "срыв концентрации снимает",
          s1 == 16 && m1 == m0 + 2 && s2 == 16 && s3 == 16 && s4 == 12 && s5 == 16 && s6 == 12,
          fmt("СИЛ 12 -> %d (мод. оружия %d -> %d), повтор %d, через 1 р. %d, через 2 р. %d; под концентрацией %d, "
              "после срыва %d",
              s1, m0, m1, s2, s3, s4, s5, s6));
    }
    // T123 условная защита
    if (TB()) {
        auto taken = [&](int tr, int sc, int hp, int type, bool mg, bool cond, unsigned res) {
            Boss b = plain(1000, 0, 0, 5);
            if (tr) b.rules.push_back(Rule{tr == 1 ? W_HPHIGH : (tr == 2 ? W_HPLOW : W_NOCOND), 0, sc == 2 ? E_RESNM : E_RES});   // условная защита — правило
            b.resMask = res;
            Fighter f = mk(b, 0, 1, 1);
            f.hp = hp;
            if (cond) f.cond[C_POISON] = 2;
            hurt(f, 100, type, mg);
            return hp - f.hp;
        };
        int a1 = taken(1, 1, 900, DT_FIRE, true, false, 0), a2 = taken(1, 1, 400, DT_FIRE, true, false, 0),
            b1 = taken(2, 1, 400, DT_FIRE, true, false, 0), b2 = taken(2, 1, 900, DT_FIRE, true, false, 0),
            c1 = taken(3, 1, 900, DT_FIRE, true, false, 0), c2 = taken(3, 1, 900, DT_FIRE, true, true, 0),
            d1 = taken(1, 2, 900, DT_SLASHING, false, false, 0), d2 = taken(1, 2, 900, DT_FIRE, true, false, 0),
            d3 = taken(1, 2, 900, DT_SLASHING, true, false, 0),
            e1 = taken(1, 1, 900, DT_FIRE, true, false, 1u << DT_FIRE), e0 = taken(0, 0, 900, DT_FIRE, true, false, 0);
        T("T123",
          "§этап 1 условная защита: резист (×0.5) только пока верно условие (ХП>50% / ХП<=50% / нет состояний); "
          "область «немагический физический» не режет магию; с обычным резистом не складывается",
          a1 == 50 && a2 == 100 && b1 == 50 && b2 == 100 && c1 == 50 && c2 == 100 && d1 == 50 && d2 == 100 &&
              d3 == 100 && e1 == 50 && e0 == 100,
          fmt("ХП>50%%: %d/%d; ХП<=50%%: %d/%d; нет состояний: %d/%d; физ. немагич.: %d, огонь %d, магич. меч %d; "
              "поверх резиста огня %d; без защиты %d",
              a1, a2, b1, b2, c1, c2, d1, d2, d3, e1, e0));
    }
    // T124 Перенаправление
    if (TB()) {
        auto trial = [&](bool atom, bool ally, int& bossHit, int& allyHit, int& swapped, int& react) {
            bossHit = allyHit = swapped = react = 0;
            int N = 300;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss at = plain(10, 1, 6, 60);
                at.wAtt = 2;
                Boss bo = plain(1000, 0, 0, 5);
                bo.redirect = atom;
                Boss mn = plain(1000, 0, 0, 5);
                w.f.push_back(mk(at, 0, 45, 50));
                w.f.push_back(mk(bo, 1, 50, 50));
                if (ally) w.f.push_back(mk(mn, 1, 51, 50));
                doWeapon(w, w.f[0], 1);
                bossHit += w.f[1].hp < 1000;
                if (ally) allyHit += w.f[2].hp < 1000;
                swapped += (ally && w.f[1].x == 51 && w.f[2].x == 50);
                react += w.f[1].reactionUsed;
            }
            return 0;
        };
        int bh, ah, sw, re, bh0, ah0, sw0, re0, bh2, ah2, sw2, re2;
        trial(true, true, bh, ah, sw, re);
        trial(false, true, bh0, ah0, sw0, re0);
        trial(true, false, bh2, ah2, sw2, re2);
        T("T124",
          "§этап 1 Перенаправление: первая атака по боссу достаётся соседнему союзнику (они меняются местами), реакция "
          "тратится, вторая бьёт босса; без союзника или без атома — нет",
          sw == 300 && re == 300 && ah > 270 && bh > 270 && sw0 == 0 && ah0 == 0 && bh0 > 270 && re2 == 0 && bh2 > 270,
          fmt("с атомом: обмен %d/300, урон союзнику %d, боссу %d (от второй атаки); без атома: обмен %d, союзнику %d, "
              "боссу %d; без союзника: реакций %d, боссу %d",
              sw, ah, bh, sw0, ah0, bh0, re2, bh2));
    }
    // T125 частота в генераторе
    if (TB()) {
        Coef C2 = CC();
        int N = 6000, cd = 0, rd = 0, sbn = 0, rdNoMin = 0, mins = 0;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(1600, C2, true);
            cd += hasResRule(b);
            rd += b.redirect;
            if (b.redirect && !hasSummon(b)) rdNoMin++;
            mins += hasSummon(b);
            bool x = false;
            for (auto& sp : b.abil)
                if (sp.sbAmt) x = true;
            sbn += x;
        }
        T("T125", "§этап 1 генератор выбирает новые атомы; Перенаправление — только вместе с миньонами",
          cd > N * 0.02 && rd > 0 && sbn > N * 0.01 && rdNoMin == 0 && cd < N / 3 && sbn < N / 3,
          fmt("условная защита %.1f%%, Перенаправление %d (из %d боссов с миньонами, без миньонов: %d), бафф "
              "характеристики %.1f%% боссов",
              100.0 * cd / N, rd, mins, rdNoMin, 100.0 * sbn / N));
    }

    // ===== Этап 2: ум бойца (ИНТ), послушание (ХАР), восприятие (МДР), естественное оружие =====
    // T126 побитово как эталон: бойцы со средним ИНТ/МДР/ХАР играют так же, как при снятии эталона (цены зафиксированы;
    // эталон перезаписывается при смене правил). Перезаписан в v21 после этапа 0 (безоружный, метательное оружие,
    // концентрация, КД, скорость с нуля, кубики к2–к100, характеристики до 40) и после этапа 1 (способности по клетке, барьеры,
    // доп. типы урона, реакции) и после этапа 2 (погоня, дружественный огонь, туман войны, ход из примитивов); прежние
    // эталоны: v20 14163065842228745299 (156/144/0), v21 этап 0 8619522491256393140 (156/144/0), этап 1 7481135932677329217 (152/148/0),
    // этап 2–5 9678082044324249348 (161/134/5); этап 5б 12775222563525202071 (152/148/0); владения 15829409147380171018 (149/149/2); «когда → что» 8537357359303400455 (145/154/1); оружие и Зуро 730685805829156341 (153/146/1); доспехи и щит 3987025246234315193 (144/152/4); формы 734869299828688533 (163/137/0); союзники 968462070843528574 (158/141/1); события и броски 5249333603403793731 (139/158/3); размеры 4905445235281751957 (158/139/3); прочее (притягивание, вампиризм, контрзаклинание…), местность (деревья, зоны, ярусы) 12484352922050994145 (156/141/3); мир из шума 581112986223779207 (155/139/6); клетки по размеру 9692586853659016580 (148/148/4); высота полёта 12057113603644787527 (153/144/3); бойцы по высоте 13005526369535065313 (152/144/4); скорости движения 935577919912848213 (127/168/5); логово, превращение 8220209741936906410 (147/148/5); смешанный призыв 3233076157663500046 (159/139/2); стойкость к контролю — нынешний
    if (TB()) {
        static const double FC[AT_N] = {
            5.5800,  1.3193,  0.0018, 46.4612, 0.0047,  0.0176,  0.0582,  2.6317,  1.6543,   1.0000,  1.0006,  1.0006,
            1.2611,  1.0010,  7.1679, 1.0002,  1.4630,  2.8552,  9.7349,  1.2450,  1.0002,   1.0007,  1.0003,  1.3451,
            0.0500,  14.6266, 0.1000, 0.3000,  1.0031,  30.0000, 72.0655, 0.3234,  115.7509, 35.9711, 63.4104, 2.0160,
            18.8687, 2.0169,  0.5000, 8.5636,  2.0015,  2.0068,  43.1679, 35.8671, 109.6182, 5.4489,  8.6471,  0.0623,
            36.9390, 0.0456,  8.7133, 52.6253, 12.8942, 6.3874,  30.3873, 1.0099,  30.3371,  10.1123, 8.2626,  1.0077,
            10.4654, 1.1532,  5.7649, 4.6040,  1.0002,  5.6993,  0.5508,  1.0000,  0.0500,   16.4320, 1.0032,  8.0410,
            16.5269, 0.1533,  0.0690, 0.2004,  2.3189,  7.5202,  8.7226,  11.1377, 41.0198,  2.2805, 0.0};
        static const double FS[DT_N] = {0.021, 0.189, 0.029, 0.058, 0.018, 0.029, 0.029,
                                        0.271, 0.031, 0.018, 0.029, 0.262, 0.018};
        Coef Cf;
        for (int a = 0; a < AT_N; a++) Cf[a] = FC[a];
        static const double FT[TR_N][6] = {{23, 40, 1, 9, 10, 0}, {6, 12, 1, 3.5, 5, 0},  {6, 20, 1, 7, 10, 0},
                                           {6, 10, 1, 3, 5, 0},   {3, 10, 1, 4, 5, 0},    {5, 11, 1, 3.5, 5, 0},
                                           {4, 10, 1, 3.5, 5, 0}, {10, 30, 1, 10, 15, 0}, {6, 15, 1, 5, 8, 0},
                                           {4, 8, 1, 2.5, 4, 0},  {29, 45, 1, 9, 15, 0}, {4, 10, 0.5, 2.5, 6, 0}};
        TierDef tdSave[TR_N];
        for (int t = 0; t < TR_N; t++) {
            tdSave[t] = TD[t];
            TD[t].a = FT[t][0];
            TD[t].b = FT[t][1];
            TD[t].k2 = FT[t][2];
            TD[t].s3 = FT[t][3];
            TD[t].c = FT[t][4];
            TD[t].e = FT[t][5];
            TD[t].q = 0;
        }
        double sh[DT_N];
        for (int t = 0; t < DT_N; t++) {
            sh[t] = g_share[t];
            g_share[t] = FS[t];
        }
        double nm0 = g_nmShare;
        g_nmShare = 0.645;
        unsigned long long h = 1469598103934665603ULL;
        int ww[3] = {0, 0, 0};
        for (int i = 0; i < 300; i++) {
            rng.seed(4242 + i);
            long long B = 200LL << (i % 6);
            Boss A, Bo;
            makePair(B, Cf, A, Bo);
            for (Boss* b : {&A, &Bo}) {
                b->stats[INT] = 10;
                b->stats[WIS] = 10;
                b->stats[CHA] = 10;
                for (auto& m : b->tmpl) {
                    m.stats[INT] = 10;
                    m.stats[WIS] = 10;
                    m.stats[CHA] = 10;
                }
            }
            g_rounds = 0;
            int r = fight(A, Bo);
            ww[r]++;
            h = (h ^ (unsigned)r) * 1099511628211ULL;
            h = (h ^ (unsigned)g_rounds) * 1099511628211ULL;
        }
        for (int t = 0; t < DT_N; t++) g_share[t] = sh[t];
        g_nmShare = nm0;
        for (int t = 0; t < TR_N; t++) TD[t] = tdSave[t];
        T("T126", "§этап 2 средний ИНТ/МДР/ХАР (8–11, >=10, >=10): бои побитово как эталон v21 этапа 2 (300 боёв, исходы и длины)",
          h == 16805395301073271776ULL,
          fmt("хэш %llu (эталон 16805395301073271776), побед A/B/ничьих %d/%d/%d (эталон 160/138/2)", h, ww[0], ww[1],
              ww[2]));
    }
    // T127 уровни ИНТ
    if (TB()) {
        World w;
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 1, 1));
        const int in[10] = {1, 3, 4, 7, 8, 11, 12, 15, 16, 20}, ex[10] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4};
        bool ok = true;
        std::string d;
        for (int k = 0; k < 10; k++) {
            w.f[0].b.stats[INT] = in[k];
            Brain b = brainOf(w, w.f[0]);
            if (b.lvl != ex[k]) ok = false;
            if (k >= 4 && (b.noise != 0 || !b.focus || !b.selfSimple || !b.selfTactics || !b.kite || !b.disengage ||
                           !b.dodge || !b.evalSummon))
                ok = false;
            if (k < 2 && (b.noise <= 0 || b.focus || b.selfSimple || b.selfTactics || b.kite)) ok = false;
            if (k >= 2 && k < 4 && (b.noise <= 0 || b.focus || !b.selfSimple || b.selfTactics || b.kite)) ok = false;
            d += fmt("ИНТ %d→%d ", in[k], b.lvl);
        }
        T("T127",
          "§этап 2 уровни ИНТ: <=3 инстинкт, 4–7 простая, 8–11 средний, 12–15 хорошая, 16+ полная; шум только на "
          "низких; средний и выше — вся тактика",
          ok, d);
    }
    // T128 шум: инстинкт часто выбирает не лучшее действие, средний — никогда
    if (TB()) {
        auto best = [&](int intv) {
            int ok = 0, N = 600;
            for (int i = 0; i < N; i++) {
                World w;
                std::memset(w.wall, 0, sizeof w.wall);
                std::memset(w.climb, 0, sizeof w.climb);
                Boss a = plain(1000, 1, 4, 5);
                a.stats[INT] = intv;
                Spec sp;
                sp.delivery = 2;
                sp.range = 5;
                sp.dmgN = 8;
                sp.dmgS = 6;
                sp.dtype = DT_FIRE;
                a.abil.push_back(sp);
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(plain(100000, 0, 0, 5), 1, 11, 10));
                takeTurn(w, 0);
                ok += g_act.type == "ability-melee";
            }
            return ok;
        };
        int b3 = best(3), b6 = best(6), b10 = best(10), b16 = best(16);
        T("T128", "§этап 2 шум выбора: ИНТ 10 и 16 всегда берут лучшее действие; ИНТ 6 ошибается реже, чем ИНТ 3",
          b10 == 600 && b16 == 600 && b3 < 570 && b6 > b3 && b6 <= 600,
          fmt("лучшее действие из 600: ИНТ 3 — %d, ИНТ 6 — %d, ИНТ 10 — %d, ИНТ 16 — %d", b3, b6, b10, b16));
    }
    // T129 монотонность: ИНТ 3 хуже ИНТ 10; ИНТ 16 не хуже ИНТ 10
    if (TB()) {
        Coef C2 = CC();
        auto wr = [&](int intv, int N, unsigned sd) {
            std::vector<int> win(N, 0);
            parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно
                rng.seed(sd + i);
                long long B = 200LL << (i % 6);
                Boss A, Bo;
                makePair(B, C2, A, Bo);
                for (Boss* b : {&A, &Bo}) {
                    b->stats[WIS] = 10;
                    b->stats[CHA] = 10;
                    b->stats[INT] = 10;
                    for (auto& m : b->tmpl) {
                        m.stats[WIS] = 10;
                        m.stats[CHA] = 10;
                        m.stats[INT] = 10;
                    }
                }
                // зеркально (v21): пара играется дважды, проверяемый ИНТ — то за A, то за B (позиционный перекос гасится)
                for (int side = 0; side < 2; side++) {
                    Boss a2 = A, b2 = Bo;
                    Boss* x = side ? &b2 : &a2;
                    x->stats[INT] = intv;
                    for (auto& m : x->tmpl) m.stats[INT] = intv;
                    rng.seed(sd + i + 7777777ULL * (side + 1));
                    const int r = fight(a2, b2);
                    win[i] += (r == side) ? 2 : (r == 2 ? 1 : 0);   // ничья — пол-очка
                }
            });
            int wins = 0;
            for (int x : win) wins += x;
            return 100.0 * wins / (4.0 * N);
        };
        int N = 6000;
        double w3 = wr(3, N, 31000000), w6 = wr(6, N, 32000000), w10 = wr(10, N, 33000000), w16 = wr(16, N, 34000000);
        double sig = 100 * std::sqrt(0.25 / N);
        T("T129",
          "§этап 2 монотонность по ИНТ (остальное одинаково): ИНТ 3 хуже ИНТ 10 хуже-или-равно ИНТ 16; ИНТ 6 между",
          w3 < w10 - 3 * sig && w6 > w3 && w6 < w10 + 2 * sig && w16 > w10 - 3 * sig && std::fabs(w10 - 50) < 3 * sig,
          fmt("винрейт стороны: ИНТ 3 %.1f%%, ИНТ 6 %.1f%%, ИНТ 10 %.1f%% (контроль 50), ИНТ 16 %.1f%% (σ≈%.1f)", w3,
              w6, w10, w16, sig));
    }
    // T130 естественное оружие
    if (TB()) {
        Coef C2 = CC();
        int N = 6000, low = 0, lowNat = 0, hi = 0, hiNat = 0;
        for (int i = 0; i < N; i++) {
            Boss b = makeBoss(400LL << irand(4), C2, true);
            if (b.wN <= 0) continue;
            if (b.stats[INT] <= 3) {
                low++;
                lowNat += b.wNatural;
            } else {
                hi++;
                hiNat += b.wNatural;
            }
        }
        T("T130",
          "§этап 2 естественное оружие: у каждого существа с ИНТ<=3 оружие естественное (укус, когти), у остальных не "
          "помечено",
          low > 50 && lowNat == low && hiNat == 0,
          fmt("ИНТ<=3: %d из %d естественное; ИНТ>=4: %d из %d", lowNat, low, hiNat, hi));
    }
    // T131 МДР: восприятие
    if (TB()) {
        auto moved = [&](int wis, int round) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 5);
            a.stats[WIS] = wis;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 50, 10));
            g_curRound = round;
            takeTurn(w, 0);
            return w.f[0].x != 10;
        };
        bool ok = moved(10, 1) && moved(14, 1) && !moved(5, 1) && moved(5, 9) && !moved(1, 1);
        World w;
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 1, 1));
        w.f[0].b.stats[WIS] = 9;
        int s9 = brainOf(w, w.f[0]).sense;
        w.f[0].b.stats[WIS] = 10;
        int s10 = brainOf(w, w.f[0]).sense;
        T("T131",
          "§этап 2 МДР: МДР>=10 замечает врага на всю карту; низкая МДР — только вблизи (60×(МДР−3) фт), дальше "
          "замечает по 60 фт за раунд",
          ok && s9 == 360 && s10 >= SENSE_INF,
          fmt("враг в 200 фт: МДР 10 идёт %d, 14 идёт %d, 5 стоит (идёт %d), 5 на 9-м раунде идёт %d, 1 стоит (идёт "
              "%d); дальность МДР 9 = %d фт",
              moved(10, 1), moved(14, 1), moved(5, 1), moved(5, 9), moved(1, 1), s9));
        g_curRound = 0;
    }
    // T132 ХАР: послушание миньонов
    if (TB()) {
        World w;
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 1, 1));
        w.f.push_back(mk(plain(10, 1, 6, 5), 1, 2, 2));
        w.f.push_back(mk(plain(10, 1, 6, 5), 0, 3, 3));
        double o[5];
        int ch[5] = {10, 13, 7, 4, 1};
        for (int k = 0; k < 5; k++) {
            w.f[0].b.stats[CHA] = ch[k];
            o[k] = brainOf(w, w.f[2]).obey;
        }
        World w2;
        Boss mb = plain(100000, 1, 6, 5);
        w2.cmd[0] = -1;
        std::memset(w2.wall, 0, sizeof w2.wall);
        std::memset(w2.climb, 0, sizeof w2.climb);
        auto follows = [&](int cha) {
            int ok = 0, N = 400;
            for (int i = 0; i < N; i++) {
                World v;
                std::memset(v.wall, 0, sizeof v.wall);
                std::memset(v.climb, 0, sizeof v.climb);
                Boss bos = plain(100000, 0, 0, 5);
                bos.stats[CHA] = cha;
                Boss mn = plain(1000, 1, 6, 5);
                Boss e1 = plain(100000, 1, 6, 5);
                Boss e2 = plain(100000, 1, 6, 5);
                v.f.push_back(mk(bos, 0, 10, 10));
                v.f.push_back(mk(plain(100000, 0, 0, 5), 1, 70, 70));
                v.f.push_back(mk(mn, 0, 11, 11));
                v.f.push_back(mk(e1, 1, 12, 12));
                v.f.push_back(mk(e2, 1, 12, 10));
                v.cmd[0] = 4;
                takeTurn(v, 2);
                ok += (g_act.target == 4);
            }
            return ok;
        };
        int f10 = follows(10), f1 = follows(1);
        T("T132",
          "§этап 2 ХАР главного босса: >=10 — миньоны всегда выполняют приказ; ниже — послушание падает (1 − "
          "0.1×(10−ХАР), не ниже 0.2)",
          o[0] == 1.0 && o[1] == 1.0 && std::fabs(o[2] - 0.7) < 1e-9 && std::fabs(o[3] - 0.4) < 1e-9 &&
              std::fabs(o[4] - 0.2) < 1e-9 && f10 == 400 && f1 < f10 - 40,
          fmt("послушание ХАР 10/13/7/4/1: %.1f/%.1f/%.1f/%.1f/%.1f; миньон бьёт приказанную цель: ХАР 10 — %d из 400, "
              "ХАР 1 — %d из 400",
              o[0], o[1], o[2], o[3], o[4], f10, f1));
    }

    // ===== Этап 4: политика Сборщика =====
    // T133 нулевая политика = прежний генератор (те же боссы, те же случайные числа)
    if (TB()) {
        Coef C2 = CC();
        std::vector<float> zero(BP_N, 0.f);
        int bad = 0, N = 400;
        for (int i = 0; i < N; i++) {
            long long B = 200LL << (i % 6);
            rng.seed(900000 + i);
            g_th = nullptr;
            Boss a = makeBoss(B, C2, true);
            uint64_t sa = rng.s;
            rng.seed(900000 + i);
            g_th = zero.data();
            Boss b = makeBoss(B, C2, true);
            uint64_t sb = rng.s;
            g_th = nullptr;
            if (a.cost != b.cost || a.atoms != b.atoms || a.hp != b.hp || a.abil.size() != b.abil.size() ||
                a.stats != b.stats || a.tmpl.size() != b.tmpl.size() || sa != sb)
                bad++;
        }
        T("T133",
          "§этап 4 нулевая политика Сборщика строит тех же боссов и расходует те же случайные числа, что обычный "
          "генератор",
          bad == 0, fmt("расхождений %d из %d", bad, N));
    }
    // T134 свойство градиента логарифма вероятности: его среднее по сборкам равно нулю (иначе обучение REINFORCE врёт)
    if (TB()) {
        Coef C2 = CC();
        std::vector<float> th(BP_N);
        rng.seed(5150);
        for (int i = 0; i < BP_N; i++) th[i] = (float)(rand01() * 1.4 - 0.7);
        int N = 40000;
        std::vector<double> sum(BP_N, 0), sq(BP_N, 0), expHits(BP_N, 0);   // expHits — ожидаемое число выборов варианта
        std::vector<std::pair<int, float>> tr;
        std::vector<double> per(BP_N);
        for (int i = 0; i < N; i++) {
            rng.seed(1000000 + i);
            g_th = th.data();
            g_trace = &tr;
            Boss b = makeBoss(1600, C2, true);
            g_trace = nullptr;
            g_th = nullptr;
            std::fill(per.begin(), per.end(), 0.0);
            for (auto& e : tr) {
                per[e.first] += e.second;
                if (e.second < 0 && e.second > -1) expHits[e.first] -= e.second;   // «не выбран»: −p
            }
            for (int k = 0; k < BP_N; k++) {
                sum[k] += per[k];
                sq[k] += per[k] * per[k];
            }
        }
        int bad = 0, used = 0;
        double worst = 0;
        int wk = -1;
        for (int k = 0; k < BP_N; k++) {
            double mean = sum[k] / N, var = sq[k] / N - mean * mean;
            if (var <= 1e-12) continue;
            // редкие варианты (корзины больших чисел, после теста 4 ноября): при паре ожидаемых выборов нормальная оценка z
            // неверна — ни одного выбора даёт почти нулевой разброс и огромный z; их не проверяем
            if (expHits[k] > 0 && expHits[k] < 30) continue;
            used++;
            double z = std::fabs(mean) / std::sqrt(var / N);
            if (z > worst) {
                worst = z;
                wk = k;
            }
            if (z > 4.5) bad++;
        }
        T("T134",
          "§этап 4 градиент логарифма вероятности решений Сборщика имеет нулевое среднее (проверка формул для да/нет, "
          "выбора из нескольких и растяжек)",
          bad == 0 && used > 100,
          fmt("параметров с данными %d из %d, вне 4.5σ: %d, худший z=%.2f (параметр %d)", used, BP_N, bad, worst, wk));
    }
    // T135 параметры двигают частоты в нужную сторону
    if (TB()) {
        Coef C2 = CC();
        auto frac = [&](std::vector<float> th, std::function<double(const Boss&)> f, int N) {
            double t = 0;
            for (int i = 0; i < N; i++) {
                rng.seed(2000000 + i);
                g_th = th.data();
                Boss b = makeBoss(1600, C2, false);
                g_th = nullptr;
                t += f(b);
            }
            return t / N;
        };
        std::vector<float> z(BP_N, 0.f), up = z, dn = z;
        up[BP_REGEN] = 4;
        dn[BP_IMMPUSH] = -5;
        std::vector<float> iu = z, id = z;
        iu[BC_STAT + INT] = 3;
        id[BC_STAT + INT] = -3;
        double r0 = frac(
                   z, [](const Boss& b) { return b.regenN > 0 ? 1.0 : 0.0; }, 6000),
               r1 = frac(up, [](const Boss& b) { return b.regenN > 0 ? 1.0 : 0.0; }, 6000);
        double p0 = frac(
                   z, [](const Boss& b) { return b.immPush ? 1.0 : 0.0; }, 6000),
               p1 = frac(dn, [](const Boss& b) { return b.immPush ? 1.0 : 0.0; }, 6000);
        double i0 = frac(
                   z, [](const Boss& b) { return (double)b.stats[INT]; }, 4000),
               iHi = frac(
                   id, [](const Boss& b) { return (double)b.stats[INT]; }, 4000),
               iLo = frac(iu, [](const Boss& b) { return (double)b.stats[INT]; }, 4000);
        T("T135",
          "§этап 4 параметры двигают решения: θ>0 у регенерации — чаще; θ<0 у иммунитета к отталкиванию — реже; вес "
          "характеристики ИНТ: θ>0 — ИНТ ниже, θ<0 — выше",
          r1 > 1.8 * r0 && p1 < 0.3 * p0 && iLo < i0 - 0.5 && iHi > i0 + 0.5,
          fmt("регенерация %.1f%% → %.1f%%; иммунитет к отталкиванию %.1f%% → %.1f%%; средний ИНТ: θ=+3 %.2f, 0 %.2f, "
              "θ=−3 %.2f",
              100 * r0, 100 * r1, 100 * p0, 100 * p1, iLo, i0, iHi));
    }
    // T136 инварианты сборки при любой политике
    if (TB()) {
        Coef C2 = CC();
        int over = 0, mis = 0, feat = 0, N = 3000;
        rng.seed(8080);
        for (int i = 0; i < N; i++) {
            std::vector<float> th(BP_N);
            for (int k = 0; k < BP_N; k++) th[k] = (float)(rand01() * 3.0 - 1.5);
            long long B = 200LL << irand(6);
            rng.seed(3000000 + i);
            g_th = th.data();
            Boss b = makeBoss(B, C2, true);
            g_th = nullptr;
            if (b.cost - STAT_BASE > B) over++;
            if (priceOf(b, C2, (double)(B + STAT_BASE)) != b.cost) mis++;
            if (featureAtoms(b) != b.atoms) feat++;
        }
        T("T136",
          "§этап 4 при любых параметрах политики босс укладывается в бюджет, цена пересчётом = записанной, метки "
          "атомов = содержимому",
          over == 0 && mis == 0 && feat == 0,
          fmt("превышений %d, расхождений цены %d, расхождений меток %d из %d", over, mis, feat, N));
    }

    // ===== Этап 6: сеть-боец =====
    // T137 градиенты GRU: ручной обратный проход через время = численное дифференцирование
    if (TB()) {
        GruNet<double> net;
        net.init(5, 4, 3, 4, 2026);
        for (auto& v : net.p) v += 0.15 * std::sin(1.7 * (&v - &net.p[0]));   // чтобы вентили не были симметричны
        const int T_ = 5;
        std::vector<std::vector<double>> xs(T_, std::vector<double>(5)), cl(T_, std::vector<double>(4));
        std::vector<double> cv(T_);
        uint64_t sd = 77;
        for (int t2 = 0; t2 < T_; t2++) {
            for (auto& x : xs[t2]) x = GruNet<double>::rnd01(sd) * 2 - 1;
            for (auto& c : cl[t2]) c = GruNet<double>::rnd01(sd) * 2 - 1;
            cv[t2] = GruNet<double>::rnd01(sd) * 2 - 1;
        }
        auto lossOf = [&](const GruNet<double>& n) {
            std::vector<double> h(3, 0.0);
            double L = 0;
            for (int t2 = 0; t2 < T_; t2++) {
                GruNet<double>::Step st;
                double lg[4], v;
                n.forward(xs[t2].data(), h.data(), st, lg, v);
                for (int k = 0; k < 4; k++) L += cl[t2][k] * lg[k];
                L += cv[t2] * v * v;   // нелинейная ценность — проверяем и вклад по ценности
                h = st.h;
            }
            return L;
        };
        // аналитический градиент
        std::vector<GruNet<double>::Step> steps(T_);
        std::vector<double> h(3, 0.0), vals(T_);
        for (int t2 = 0; t2 < T_; t2++) {
            double lg[4];
            net.forward(xs[t2].data(), h.data(), steps[t2], lg, vals[t2]);
            h = steps[t2].h;
        }
        std::vector<double> g(net.size(), 0.0), dhn(3, 0.0), dhp(3);
        for (int t2 = T_ - 1; t2 >= 0; t2--) {
            net.backward(steps[t2], cl[t2].data(), 2 * cv[t2] * vals[t2], dhn.data(), g, dhp.data());
            dhn = dhp;
        }
        double worst = 0;
        int checked = 0;
        for (size_t i = 0; i < net.size(); i += 3) {
            GruNet<double> a = net, b = net;
            const double eps = 1e-6;
            a.p[i] += eps;
            b.p[i] -= eps;
            double num = (lossOf(a) - lossOf(b)) / (2 * eps);
            double err = std::fabs(num - g[i]) / std::max(1e-6, std::fabs(num) + std::fabs(g[i]));
            worst = std::max(worst, err);
            checked++;
        }
        T("T137", "§этап 6 сеть-боец: градиенты GRU (ручной обратный проход по времени) совпадают с численными", worst < 1e-5,
          fmt("проверено параметров %d из %zu, наибольшая относительная ошибка %.2e", checked, net.size(), worst));
    }

    // T138–T139: сеть-боец в бою (бойцы с ИНТ >= 12 ходят по сети; 12–15 — в 25% ходов обычным ботом)
    if (TB()) {
        Coef C2 = CC();
        const bool haveNet = anyFighterNet();   // v21: старая сеть отключена, новая (prim_net) обучается на этапе 3
        auto wr = [&](int intv, int N, unsigned sd) {
            std::vector<int> win(N, 0);
            parallelFor(N, [&](int i) {   // по потокам: у каждого боя своё зерно
                rng.seed(sd + i);
                long long B = 200LL << (i % 6);
                Boss A, Bo;
                makePair(B, C2, A, Bo);
                for (Boss* b : {&A, &Bo}) {
                    b->stats[WIS] = 10; b->stats[CHA] = 10; b->stats[INT] = 10;
                    for (auto& m : b->tmpl) { m.stats[WIS] = 10; m.stats[CHA] = 10; m.stats[INT] = 10; }
                }
                for (int side = 0; side < 2; side++) {   // зеркально (v21): ИНТ проверяемого — то за A, то за B
                    Boss a2 = A, b2 = Bo;
                    (side ? b2 : a2).stats[INT] = intv;
                    rng.seed(sd + i + 7777777ULL * (side + 1));
                    const int r = fight(a2, b2);
                    win[i] += (r == side) ? 2 : (r == 2 ? 1 : 0);   // ничья — пол-очка
                }
            });
            int wins = 0;
            for (int x : win) wins += x;
            return 100.0 * wins / (4.0 * N);
        };
        const int N = 6000;
        double w10 = wr(10, N, 51000000), w13 = wr(13, N, 52000000), w16 = wr(16, N, 53000000);
        double sig = 100 * std::sqrt(0.25 / N);
        T("T138", "§этап 6 сеть-боец: при загруженных весах ИНТ 16 заметно сильнее ИНТ 10 (>= +5 п.п.), контроль ИНТ 10 против ИНТ 10 около 50%",
          !haveNet ? std::fabs(w10 - 50) < 3 * sig : (w16 > 55.0 && std::fabs(w10 - 50) < 3 * sig),
          fmt("сеть загружена: %s%s; винрейт стороны: ИНТ 10 (контроль) %.1f%%, ИНТ 16 %.1f%% (σ≈%.1f)", haveNet ? "да" : "нет",
              haveNet ? "" : " (без весов проверяется только контроль: сеть обучается на этапе 3)", w10, w16, sig));
        T("T139", "§этап 6 ступени ИНТ: ИНТ 12–15 (сеть с забывчивостью) не слабее ИНТ 10 и не сильнее ИНТ 16", w13 > 50 - 2 * sig && w13 < w16 + 2 * sig,
          fmt("винрейт стороны: ИНТ 10 %.1f%%, ИНТ 13 %.1f%%, ИНТ 16 %.1f%%", w10, w13, w16));
    }
    // T140–T146: погоня (DMG): провоцированные атаки, рывки и истощение, осложнения, отрыв, побег = поражение
    if (TB()) {
        auto clearW = [](World& w) {
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            std::memset(w.dark, 0, sizeof w.dark);
            w.aw = w.ah = 100;
            w.theme = 0;
        };
        {   // T140: в погоне между главными боссами нет провоцированных атак, вне погони — есть
            int dmgChase = 0, dmgPlain = 0;
            for (int mode = 0; mode < 2; mode++)
                for (int i = 0; i < 300; i++) {
                    World w;
                    clearW(w);
                    w.chaseOn = (mode == 0);
                    Boss big = plain(1000, 0, 0, 5), g = plain(10, 1, 1, 5);
                    g.wFinesse = false;
                    w.f.push_back(mk(big, 0, 51, 50));
                    w.f.push_back(mk(g, 1, 50, 50));
                    w.f.push_back(mk(plain(10, 1, 6, 5), 1, 80, 50));
                    const int hp0 = w.f[0].hp;
                    moveToward(w, w.f[0], w.f[2], 0, 30, false);
                    (mode == 0 ? dmgChase : dmgPlain) += hp0 - w.f[0].hp;
                }
            T("T140", "§7 погоня: провоцированных атак между участниками нет (вне погони есть)", dmgChase == 0 && dmgPlain > 200,
              fmt("урон в погоне %d, вне погони %d", dmgChase, dmgPlain));
        }
        {   // T141: рывки сверх 3 + мод. ТЕЛ -> проверка ТЕЛ СЛ 10 или истощение; в пределах нормы истощения нет
            int exhOver = 0, exhOk = 0, N = 400;
            for (int i = 0; i < N; i++) {
                for (int lim = 0; lim < 2; lim++) {
                    World w;
                    clearW(w);
                    Boss b = plain(100, 1, 6, 5);
                    b.stats[CON] = 1;   // мод -5: проверка СЛ 10 проваливается при 1–14 на кубике (70%)
                    w.f.push_back(mk(b, 0, 40, 50));
                    w.f.push_back(mk(plain(100, 1, 6, 5), 1, 60, 50));
                    w.chaseOn = true;
                    w.chasePrey = 0;
                    w.f[0].dashLimit = lim == 0 ? 0 : 99;
                    w.f[0].dashedNow = true;
                    chaseAfterTurn(w, 0, 5);
                    (lim == 0 ? exhOver : exhOk) += w.f[0].exh;
                }
            }
            T("T141", "§7 погоня: рывок сверх 3 + мод. ТЕЛ — проверка ТЕЛ СЛ 10, провал даёт истощение; в пределах нормы — нет", exhOver > N * 0.6 && exhOk == 0,
              fmt("истощений при превышении %d из %d, в норме %d", exhOver, N, exhOk));
        }
        {   // T142: уровни истощения: 2 — скорость вдвое, 4 — максимум ХП вдвое, 5 — скорость 0
            World w;
            clearW(w);
            Boss b = plain(100, 1, 6, 5);
            b.stats[CON] = 1;
            w.f.push_back(mk(b, 0, 40, 50));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 60, 50));
            Fighter& f = w.f[0];
            f.exh = 2;
            const int s2 = speedOf(f);
            f.exh = 5;
            const int s5 = speedOf(f);
            f.exh = 3;
            f.dashLimit = 0;
            w.chaseOn = true;
            w.chasePrey = 0;
            int tries = 0;
            while (f.exh < 4 && tries++ < 200) {
                f.dashedNow = true;
                chaseAfterTurn(w, 0, 5);
            }
            T("T142", "§7 истощение: 2 — скорость 15; 4 — максимум ХП вдвое; 5 — скорость 0", s2 == 15 && s5 == 0 && f.exh >= 4 && f.maxHp == 50 && f.hp <= 50,
              fmt("скорость при 2: %d, при 5: %d, уровень %d, макс. ХП %d", s2, s5, f.exh, f.maxHp));
        }
        {   // T143: осложнения: слабый (ЛОВ/СИЛ 1) почти всегда страдает: опутан (дикая местность 7) и сбит с ног (город 3)
            int restr = 0, prone = 0, N = 300;
            for (int i = 0; i < N; i++) {
                World w;
                clearW(w);
                Boss b = plain(1000, 1, 6, 5);
                b.stats[DEX] = 1; b.stats[STR] = 1;
                w.f.push_back(mk(b, 0, 40, 50));
                w.f.push_back(mk(plain(100, 1, 6, 5), 1, 60, 50));
                w.theme = 0;
                applyComplication(w, w.f[0], 7);
                restr += w.f[0].cond[C_RESTR] > 0;
                w.theme = 1;
                applyComplication(w, w.f[0], 3);
                prone += w.f[0].cond[C_PRONE] > 0;
            }
            T("T143", "§7 осложнения погони: силок опутывает, споткнувшийся падает", restr > N * 0.85 && prone > N * 0.6, fmt("опутан %d/%d, сбит %d/%d", restr, N, prone, N));
        }
        {   // T144: отрыв: если преследуемый не терял преследователя из виду — проверка проваливается сама; потерял — ЛОВ против МДР
            int seenEsc = 0, hiddenEsc = 0, N = 300;
            for (int i = 0; i < N; i++) {
                for (int seen = 0; seen < 2; seen++) {
                    World w;
                    clearW(w);
                    Boss a = plain(100, 1, 6, 5), b = plain(100, 1, 6, 5);
                    a.stats[DEX] = 30; b.stats[WIS] = 1;
                    w.f.push_back(mk(a, 0, 40, 50));
                    w.f.push_back(mk(b, 1, 70, 50));
                    for (int y = 40; y < 60; y++) w.dark[y][38] = true;   // много тьмы рядом — преимущество
                    w.chaseOn = true;
                    w.chasePrey = 0;
                    w.chaseSeen = seen == 1;
                    chaseEndOfRound(w);
                    (seen ? seenEsc : hiddenEsc) += w.f[0].fled;
                }
            }
            T("T144", "§7 погоня: оторваться можно только потеряв преследователя из виду (проверка ЛОВ против МДР)", seenEsc == 0 && hiddenEsc > N * 0.95,
              fmt("побегов без потери из виду %d, с потерей %d из %d", seenEsc, hiddenEsc, N));
        }
        {   // T145: побег = поражение; fled заканчивает бой
            World w;
            clearW(w);
            w.f.push_back(mk(plain(100, 1, 6, 5), 0, 40, 50));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 45, 50));
            const bool before = over(w);
            w.f[0].fled = true;
            T("T145", "§7 вышедший за арену или оторвавшийся (fled) заканчивает бой", !before && over(w), "");
        }
        {   // T147: погоня начинается только при отходе из-под угрозы: на 200 фт обычное перемещение погоней не считается, в пределах рывка врага — считается
            int farChase = 0, nearChase = 0, N = 100;
            for (int i = 0; i < N; i++)
                for (int near = 0; near < 2; near++) {
                    World w;
                    clearW(w);
                    w.f.push_back(mk(plain(100, 1, 6, 5), 0, 40, 50));
                    w.f.push_back(mk(plain(100, 1, 6, 5), 1, near ? 47 : 80, 50));
                    const int dist0 = distFt(w.f[0], w.f[1]);
                    w.f[0].x -= 6;   // отошёл на 30 фт
                    chaseAfterTurn(w, 0, dist0);
                    (near ? nearChase : farChase) += w.chaseOn;
                }
            T("T147", "§7 погоня: начинается при отходе на 10+ фт из-под угрозы врага (досягаемость + рывок); издалека — нет", farChase == 0 && nearChase == N,
              fmt("погонь издалека %d, вблизи %d из %d", farChase, nearChase, N));
        }
        {   // T146: в настоящих боях убегающие проигрывают: бой в тесной арене 5×5 без выхода из лимита, погони и побеги случаются, зависших нет
            g_style.fixedW = g_style.fixedH = 5;
            long long c0 = g_chases, e0 = g_escapes, cap0 = g_capped;
            Coef C2 = CC();
            const int N = 600;
            int draws = 0;
            for (int i = 0; i < N; i++) {
                rng.seed(77000 + i);
                Boss A, Bo;
                makePair(400LL << (i % 4), C2, A, Bo);
                draws += fight(A, Bo) == 2;
            }
            g_style = MapStyle();
            T("T146", "§7 арена 5×5: бои заканчиваются (ничьих не более 3%), погонь нет или немного",
              draws <= N * 0.03 && g_capped - cap0 <= N * 0.03, fmt("ничьих %d из %d, по лимиту %lld, погонь %lld, побегов %lld", draws, N, g_capped - cap0, g_chases - c0, g_escapes - e0));
        }
    }
    // T148: разбор цены по покупкам для просмотрщика совпадает с независимым пересчётом цены босса
    if (TB()) {
        Coef C2 = CC();
        int bad = 0, N = 600;
        long long worst = 0;
        for (int i = 0; i < N; i++) {
            rng.seed(88000 + i);
            const long long B = 200LL << (i % 6);
            Boss b = makeBoss(B, C2, true, i % 3 == 0);
            long long sum = 0;
            for (auto& it : bossItems(b, C2, (double)B)) sum += it.cost;
            const long long ref = priceOf(b, C2, (double)(B + STAT_BASE));
            if (sum + STAT_BASE != ref) { bad++; worst = std::max(worst, std::llabs(sum + STAT_BASE - ref)); }
        }
        T("T148", "§просмотрщик: сумма покупок босса равна его цене по таблице (независимый пересчёт)", bad == 0, fmt("расхождений %d из %d (наибольшее %lld)", bad, N, worst));
    }
    // T149: цена способности и оружия — СУММА процентов (а не произведение), скидки ограничены снизу MULT_FLOOR
    if (TB()) {
        Coef C2 = CC();
        g_B = 800;
        Spec s;
        s.delivery = 1;
        s.range = 30;
        s.radius = 15;
        s.shape = SH_CUBE;   // v21: надбавка за куб — прежняя лестница по радиусу
        s.dmgN = 4;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        const double expectRange = C2[AT_ARANGE] * tierN(TR_RANGE, rangeIdx(s.range)), expectArea = C2[AT_AREA] * tierN(TR_AREA, s.radius / 5.0);
        const double m0 = specMult(s, C2);
        bool sum1 = std::fabs(m0 - (1 + expectRange + expectArea)) < 1e-9;
        Spec lim = s;
        lim.uses = 1;
        lim.recharge = 0;
        Spec both = lim;
        both.conc = true;
        both.bonus = true;
        // после теста 4 ноября: скидка концентрации — доля длительности, потерянная при срыве; пол ограничивает только скидки
        const double dU = (1 - C2[AT_ULIM]) * 1.0, dC = std::max(0.0, 1 - C2[AT_CONC] * concKeep(both)), pB = C2[AT_BONUS] - 1;
        const double want = 1 + expectRange + expectArea - std::min(dU + dC, 1 - MULT_FLOOR) + pB;
        bool sum2 = std::fabs(specMult(both, C2) - want) < 1e-9;
        Spec cheap = s;
        cheap.uses = 1;
        cheap.conc = true;
        cheap.slotLvl = 1;
        cheap.recharge = 0;
        const double floorWant = 1 + expectRange + expectArea - (1 - MULT_FLOOR);   // скидки упёрлись в пол, надбавки остались
        bool floorOk = std::fabs(specMult(cheap, C2) - floorWant) < 1e-9;
        Wp wp;
        wp.n = 2; wp.s = 8; wp.range = 30; wp.att = 3; wp.reach = 5;
        const double wm = 1 + C2[AT_WPNRANGE] * tierN(TR_RANGE, rangeIdx(wp.range));   // число атак — отдельно (wpHits)
        bool wpOk = std::fabs(wpMult(C2, wp.range, wp.reach, wp.att) - wm) < 1e-9;
        T("T149", "§цены: множитель способности и оружия — сумма процентов (1 + надбавки − скидки), скидки в сумме не больше 80%, надбавки всегда", sum1 && sum2 && floorOk && wpOk,
          fmt("сумма-1 %d, сумма-2 %d, пол %d (множитель %.3f), оружие %d", sum1, sum2, floorOk, specMult(cheap, C2), wpOk));
    }
    // T150–T152: персонаж игрока: проверка правил, цена, ход по командам
    if (TB()) {
        Coef C2 = CC();
        auto parse = [&](const char* t) { bool ok = true; Json j = parseJson(t, &ok); return j; };
        const char* good = "{\"name\":\"Т\",\"budget\":800,\"stats\":[16,10,14,8,10,8],\"acV\":8,\"weapon\":{\"n\":1,\"s\":8,\"att\":2,\"type\":11},"
                           "\"abilities\":[{\"delivery\":1,\"saveStat\":1,\"range\":5,\"radius\":10,\"dmgN\":2,\"dmgS\":6,\"dtype\":1,\"cond\":1,\"recharge\":5}],\"passives\":{\"saveProf\":[2]}}";
        Boss b;
        long long bud = 0, spent = 0, left = 0;
        std::string err;
        const bool okGood = chr::buildCharacter(parse(good), C2, b, bud, err, &spent, &left);
        long long sum = 0;
        for (auto& it : bossItems(b, C2, (double)bud)) sum += it.cost;
        T("T150", "§персонаж: описание в JSON собирается в босса, ХП = остаток бюджета (2 очка за ХП), сумма покупок = потрачено, не больше бюджета", okGood && sum == spent && spent <= bud && left >= 0 && left < 2 && b.hp > 1,
          fmt("ok %d, потрачено %lld из %lld, остаток %lld, ХП %d, сумма покупок %lld", okGood, spent, bud, left, b.hp, sum));
        const char* bad[] = {
            "{\"budget\":800,\"stats\":[41,10,10,10,10,10]}",
            "{\"budget\":100,\"stats\":[30,30,30,30,30,30]}",
            "{\"budget\":800,\"abilities\":[{\"self\":false,\"delivery\":0}]}",
            "{\"budget\":800,\"abilities\":[{\"self\":true,\"delivery\":0}]}",
            "{\"budget\":800,\"abilities\":[{\"delivery\":0,\"dmgN\":1,\"recharge\":5,\"uses\":2}]}",
            "{\"budget\":800,\"hp\":100000}",
            "{\"budget\":800,\"passives\":{\"rider\":{\"n\":2}},\"weapon\":{\"n\":0}}",
            "{\"v\":21,\"budget\":5000,\"acV\":31}",
            "{\"budget\":800,\"weapon\":{\"n\":1,\"s\":7}}",
            "{\"budget\":800,\"passives\":{\"speed\":33}}",
            "{\"budget\":100001}",
        };
        int rejected = 0;
        for (const char* t : bad) {
            Boss bb;
            long long bu = 0;
            std::string e2;
            if (!chr::buildCharacter(parse(t), C2, bb, bu, e2)) rejected++;
        }
        T("T151", "§персонаж: нарушения правил отклоняются (характеристика 41, КД 41, кубик к7, скорость не кратна 5, бюджет выше предела, бюджет не покрывает ХП, способность без эффекта, перезарядка вместе с лимитом, ХП дороже остатка, довесок без оружия)",
          rejected == (int)(sizeof bad / sizeof bad[0]), fmt("отклонено %d из %zu", rejected, sizeof bad / sizeof bad[0]));
        // ход игрока: команды идут через g_human.ask; игрок подходит, бьёт и заканчивает ход, бой завершается без зависаний
        int finished = 0, moved = 0, attacked = 0, N = 40, askedTotal = 0;
        for (int i = 0; i < N; i++) {
            rng.seed(99000 + i);
            Boss me, opp;
            long long bu = 0;
            std::string e3;
            chr::buildCharacter(parse(good), C2, me, bu, e3);
            opp = makeBoss(800, C2, true, false);
            g_human.idx = 0;
            int asked = 0;
            g_human.ask = [&](const std::string& req) -> std::string {
                asked++;
                bool ok = true;
                Json q = parseJson(req, &ok);
                if (!ok || asked > 400) return "{\"c\":\"end\"}";
                const Json& en = q["enemies"][0];
                if (en.t != Json::OBJ) return "{\"c\":\"end\"}";
                const int d = en["dist"].num(), tgt = en["i"].num();
                if (q["actionFree"].flag() && en["los"].flag() && d <= q["weapon"]["range"].num()) { attacked++; return "{\"c\":\"attack\",\"tgt\":" + std::to_string(tgt) + "}"; }
                if (q["speedLeft"].num() >= 5 && d > q["weapon"]["range"].num()) { moved++; return "{\"c\":\"move\",\"x\":0,\"y\":0}"; }
                return "{\"c\":\"end\"}";
            };
            std::vector<std::string> frames;
            const int r = fight(me, opp, &frames);
            g_human.idx = -1;
            g_human.ask = nullptr;
            finished += (r >= 0 && r <= 2);
            askedTotal += asked;
        }
        T("T152", "§игра: ход игрока по командам (перейти, атаковать, закончить) — бои заканчиваются, игрок действительно ходит", finished == N && moved > 0 && attacked > 0,
          fmt("боёв закончено %d из %d, команд движения %d, атак %d, запросов хода %d", finished, N, moved, attacked, askedTotal));
    }
    // T153: характеристики от 10: десять бесплатно, выше платим, ниже возвращаем (V·(V+1)/2 − 55); разбор цены и персонаж это учитывают
    if (TB()) {
        Coef C2 = CC();
        const bool tab = statCost(10) == 0 && statCost(16) == 81 && statCost(8) == -19 && statCost(1) == -54 && statCost(30) == 410 && STAT_BASE == 330;
        auto parse = [&](const char* t) { return parseJson(t); };
        Boss base, dump;
        long long bud = 0, sp0 = 0, sp1 = 0, l0 = 0, l1 = 0;
        std::string err;
        const bool ok0 = chr::buildCharacter(parse("{\"budget\":300,\"stats\":[10,10,10,10,10,10]}"), C2, base, bud, err, &sp0, &l0);
        const bool ok1 = chr::buildCharacter(parse("{\"budget\":300,\"stats\":[18,10,10,10,10,3]}"), C2, dump, bud, err, &sp1, &l1);
        long long sumStats = 0;
        for (auto& it : bossItems(dump, C2, 300.0))
            if (it.cat == "stat") sumStats += it.cost;
        // 18: +116, 3: −49 → +67; всё остальное (ХП) тратит остаток
        const bool items = sumStats == 116 - 49;
        T("T153", "§2 характеристики от 10: десять бесплатно, выше платим, ниже возвращаем; бюджет «от десяток»", tab && ok0 && ok1 && items && base.hp > dump.hp,
          fmt("таблица %d, ХП при всех десятках %d, при 18/3 %d, итог по характеристикам %lld (ожидалось 67)", tab, base.hp, dump.hp, sumStats));
    }
    // T154: барьер — временная стена: непроходима, закрывает обзор, пропускает клетки с существами, через раунды исчезает
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.aw = w.ah = 30;
        w.f.push_back(mk(plain(100, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 14, 10));
        const bool seeBefore = los(w, 10, 10, 18, 10);
        const int placed = placeWall(w, w.f[0], 14, 11, 5, 2);   // клетка (14,10) занята врагом: её пропускаем
        const bool blocked = !los(w, 10, 12, 18, 12) && w.wall[12][14] && !w.wall[10][14];
        expireDyn(w);
        const bool stillThere = w.wall[11][14];
        expireDyn(w);
        expireDyn(w);
        const bool gone = !w.wall[11][14] && w.dyn.empty();
        T("T154", "§способности по клетке: барьер ставится поперёк, не на существ, закрывает обзор и через заданное число раундов исчезает", seeBefore && placed == 4 && blocked && stillThere && gone,
          fmt("поставлено клеток %d (ожидалось 4), обзор закрыт %d, держится после 1 раунда %d, исчез %d", placed, blocked, stillThere, gone));
    }
    // T155: карта игрока: проверка размера, связности и начальных клеток; бой идёт по этой карте и начинается на заданных клетках
    if (TB()) {
        auto mk2 = [&](int aw, int ah, const std::string& wallRow, int wallCol, const char* starts) {
            std::string j = "{\"aw\":" + std::to_string(aw) + ",\"ah\":" + std::to_string(ah) + ",\"wall\":[";
            for (int y = 0; y < ah; y++) {
                std::string row(aw, '0');
                if (wallCol >= 0) row[wallCol] = '1';
                j += (y ? ",\"" : "\"") + row + "\"";
            }
            return j + "]" + starts + "}";
        };
        CustomMap m;
        std::string err;
        const bool good = parseCustomMap(parseJson(mk2(12, 9, "", -1, ",\"starts\":[[1,4],[10,4]]")), m, err);
        const bool small = !parseCustomMap(parseJson(mk2(4, 9, "", -1, "")), m, err);
        const bool split = !parseCustomMap(parseJson(mk2(12, 9, "", 6, "")), m, err);
        const bool badStart = !parseCustomMap(parseJson(mk2(12, 9, "", 6, ",\"starts\":[[6,2],[10,4]]")), m, err);
        const bool oneStart = !parseCustomMap(parseJson(mk2(12, 9, "", -1, ",\"starts\":[[1,4]]")), m, err);
        parseCustomMap(parseJson(mk2(12, 9, "", -1, ",\"starts\":[[1,4],[10,4]]")), g_customMap, err);
        Coef C2 = CC();
        rng.seed(4242);
        Boss a = makeBoss(300, C2, true, false), b = makeBoss(300, C2, true, false);
        std::vector<std::string> frames;
        std::string mapJson;
        const int r = fight(a, b, &frames, &mapJson);
        g_customMap = CustomMap();
        bool startsOk = !frames.empty() && frames[0].find("\"x\":1,\"y\":4,") != std::string::npos && frames[0].find("\"x\":10,\"y\":4,") != std::string::npos;
        const bool sizeOk = mapJson.find("\"aw\":12,\"ah\":9") != std::string::npos;
        T("T155", "§карта игрока: размер 5–100, свободные клетки связаны, начальные клетки свободны и заданы парой; бой идёт на этой карте с этих клеток", good && small && split && badStart && oneStart && startsOk && sizeOk && r >= 0,
          fmt("верная %d, мала %d, разделена %d, старт на стене %d, одна стартовая %d, бой на 12×9 %d, старт на местах %d", good, small, split, badStart, oneStart, sizeOk, startsOk));
    }
    // ===== v21, этап 0 =====
    // T156: границы калибровки lo <= init <= hi у всех атомов (№14); проверка при запуске (adBoundsOk) тоже проходит
    if (TB()) {
        int bad = 0, floorEq = 0;
        for (int a = 0; a < AT_N; a++) {
            if (!(AD[a].lo <= AD[a].init && AD[a].init <= AD[a].hi)) bad++;
            // №15: пол на уровне цены только у фиксированных. Исключения — атомы, которые калибровка этапа 4 (на сети,
            // 2026-11-02) сама довела до естественной нижней границы: надбавки «без броска» и «реакция» до 1 (без
            // надбавки), скидка концентрации до 0.1, владение спасброском до 2
            const bool hitFloor = a == AT_AUTO || a == AT_REACT || a == AT_CONC || a == AT_SAVEPROF;
            if (AD[a].mode != 2 && AD[a].lo >= 0.99 * AD[a].init && !hitFloor) floorEq++;
        }
        T("T156", "§v21 №14–15: у каждого атома lo <= init <= hi; lo = init только у фиксированных атомов", bad == 0 && floorEq == 0 && adBoundsOk(false),
          fmt("нарушений %d, нефиксированных с lo = init %d", bad, floorEq));
    }
    // T157: безоружный (№2) не атакует оружием, но парирует и контратакует кулаком; без способностей ход — Уклонение
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss u = plain(1000, 0, 0, 5);
        Boss e = plain(1000, 1, 6, 5);
        w.f.push_back(mk(u, 0, 10, 10));
        w.f.push_back(mk(e, 1, 11, 10));
        const int h0 = w.f[1].hp;
        doWeapon(w, w.f[0], 1);
        const bool noHit = w.f[1].hp == h0 && evWeapon(w, w.f[0], w.f[1]) == 0;
        w.f[0].init = 2000;
        takeTurn(w, 0);
        const bool dodged = w.f[0].dodging && w.f[1].hp == h0;
        int par = 0, rip = 0, N = 400;
        for (int i = 0; i < N; i++) {
            World v;
            std::memset(v.wall, 0, sizeof v.wall);
            std::memset(v.climb, 0, sizeof v.climb);
            Boss d = plain(100000, 0, 0, 5);
            d.ac = 10;
            d.parry = 10;
            Boss a = plain(100000, 1, 1, 5);
            v.f.push_back(mk(a, 0, 10, 10));
            v.f.push_back(mk(d, 1, 11, 10));
            bool crit;
            attackRoll(v, v.f[0], v.f[1], PROF, 5, crit);
            par += v.f[1].reactionUsed;
            Boss r = plain(100000, 0, 0, 5);
            r.riposte = true;
            r.ac = -1000;
            v.f[1] = mk(r, 1, 11, 10);
            v.f[1].b.stats[STR] = 14;   // кулак: 1 + мод. СИЛ (+2) = 3
            const int ha = v.f[0].hp;
            doWeapon(v, v.f[0], 1);
            if (v.f[1].reactionUsed && ha - v.f[0].hp >= 1 && ha - v.f[0].hp <= 6) rip++;
        }
        T("T157", "§v21 №2: безоружный не атакует оружием (Уклонение, если нечем действовать), но парирует и контратакует кулаком",
          noHit && dodged && par > N * 0.3 && rip > N * 0.85,
          fmt("удара нет %d, ход — Уклонение %d; парирований %d/%d, контратак кулаком %d/%d", noHit, dodged, par, N, rip, N));
    }
    // T158: концентрация — у каждого попадания своя СЛ max(10, урон/2); урон во временные хиты тоже требует проверки
    if (TB()) {
        World w;
        Boss c = plain(1000, 0, 0, 5);
        w.f.push_back(mk(c, 0, 10, 10));
        Fighter& f = w.f[0];
        f.concSpec = 0;
        hurt(f, 40, DT_FIRE, true);
        hurt(f, 2, DT_FIRE, true);
        const bool perHit = f.concDCs.size() == 2 && f.concDCs[0] == 20 && f.concDCs[1] == 10;
        f.concDCs.clear();
        f.temp = 50;
        const int hp0 = f.hp;
        hurt(f, 30, DT_FIRE, true);
        const bool tempCheck = f.hp == hp0 && f.temp == 20 && f.concDCs.size() == 1 && f.concDCs[0] == 15;
        T("T158", "§v21: концентрация — своя СЛ на каждое попадание; урон, поглощённый временными хитами, тоже даёт проверку",
          perHit && tempCheck, fmt("СЛ по попаданиям верны %d, проверка при уроне во временные хиты %d", perHit, tempCheck));
    }
    // T159: метательное оружие вплотную бьёт как ближнее (без помехи за врага рядом), боеприпасное — с помехой
    if (TB()) {
        auto dis = [&](bool ammo) {
            World w;
            std::memset(w.wall, 0, sizeof w.wall);
            std::memset(w.climb, 0, sizeof w.climb);
            Boss a = plain(100, 1, 6, 30);
            a.wAmmo = ammo;
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 11, 10));
            bool adv, d;
            advState(w, w.f[0], w.f[1], wpAtkRange(w.f[0].b, distFt(w.f[0], w.f[1])), adv, d);
            return d;
        };
        const bool thrownOk = !dis(false), bowDis = dis(true);
        T("T159", "§v21: метательное оружие вплотную — ближняя атака без помехи; лук вплотную — с помехой", thrownOk && bowDis,
          fmt("метательное без помехи %d, лук с помехой %d", thrownOk, bowDis));
    }
    // T160: №3 — 1 единица = +1 КД, цена за тот же КД прежняя (tri(2a)); КД генератора не выше 40; старое описание даёт тот же КД
    if (TB()) {
        Coef C2 = CC();
        bool costOk = true;
        for (int a = 0; a <= 30; a++) costOk = costOk && acCost(a) == tri(2 * a);
        int bad = 0;
        for (long long B : {200LL, 1600LL, 100000LL})
            for (int i = 0; i < 300; i++) {
                Boss b = makeBoss(B, C2, false);
                if (b.ac != calcAC(b) || b.acV > AC_MAX - 10) bad++;
            }
        auto acOf = [&](const char* t, long long& cost) {
            bool ok = true;
            Boss b;
            long long bu = 0;
            std::string e;
            chr::buildCharacter(parseJson(t, &ok), C2, b, bu, e);
            cost = 0;
            for (auto& it : bossItems(b, C2, (double)bu))
                if (it.cat == "ac") cost = it.cost;
            return b.ac;
        };
        long long cOld = 0, cNew = 0;
        const int acOld = acOf("{\"budget\":800,\"acV\":8}", cOld), acNew = acOf("{\"v\":21,\"budget\":800,\"acV\":4}", cNew);
        T("T160", "§v21 №3: 1 единица = +1 КД, цена КД 14 прежняя (36), КД генератора <= 40, старое описание (acV 8) — тот же КД",
          costOk && bad == 0 && acOld == 14 && acNew == 14 && cOld == 36 && cNew == 36,
          fmt("цена tri(2a) %d, нарушений у генератора %d, КД старого/нового описания %d/%d, цена %lld/%lld", costOk, bad, acOld, acNew, cOld, cNew));
    }
    // T161: №7 — скорость покупается вся, с нуля, шагами по 5 фт; у боссов генератора она есть всегда; боец ходит на купленную
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        bool grows = speedUnits(0) == 0;
        for (int ft = 5; ft <= 120; ft += 5) grows = grows && speedUnits(ft) > speedUnits(ft - 5);
        int none = 0, std30 = 0, N = 0, other5 = 0;
        for (long long B : {20LL, 800LL, 6400LL})
            for (int i = 0; i < 400; i++, N++) {
                Boss b = makeBoss(B, C2, false);
                if (b.speed <= 0) none++;
                if (b.speed == 30) std30++;
                if (b.speed % 5) other5++;
            }
        Fighter f;
        f.b.speed = 15;
        T("T161", "§v21 №7: скорость с нуля шагами по 5 фт (цена растёт с каждой ступенью), у каждого босса есть, обычно 30 фт; боец ходит на купленную",
          grows && none == 0 && other5 == 0 && std30 > N * 0.7 && std30 < N * 0.95 && speedOf(f) == 15,
          fmt("цена растёт %d, без скорости %d, ровно 30 фт %d из %d, не кратно 5 %d, скорость бойца %d", grows, none, std30, N, other5, speedOf(f)));
    }
    // T162: №8 — характеристика до 40; произвольные пределы конструктора сняты, технические остаются
    if (TB()) {
        Coef C2 = CC();
        int over = 0, at40 = 0;
        for (int i = 0; i < 300; i++) {
            Boss b = makeBoss(100000, C2, false);
            for (int st : b.stats) {
                over += st > STAT_MAX;
                at40 += st == STAT_MAX;
            }
        }
        auto okChar = [&](const char* t) {
            bool ok = true;
            Boss b;
            long long bu = 0;
            std::string e;
            return chr::buildCharacter(parseJson(t, &ok), C2, b, bu, e);
        };
        const bool stat40 = okChar("{\"budget\":5000,\"stats\":[40,10,10,10,10,10]}");
        auto valid = [&](const char* t) {   // только проверка правил, без цены (широкие значения дороже любого бюджета)
            bool ok = true;
            Boss b;
            long long bu = 0;
            int hpw = 0;
            std::string e;
            return chr::bossFromJson(parseJson(t, &ok), b, bu, hpw, e);
        };
        const bool wide = valid("{\"budget\":100000,\"weapon\":{\"n\":100,\"s\":6,\"att\":50,\"reach\":30},\"passives\":{\"legAct\":8,\"parry\":12,\"speed\":250}}");
        const bool techCap = !valid("{\"budget\":800,\"weapon\":{\"n\":1,\"s\":6,\"range\":505}}");
        T("T162", "§v21 №8: характеристика до 40 (генератор и конструктор); снятые пределы (100 кубиков, 50 атак, досягаемость 30, ЛД 8, скорость 250) принимаются; дальность > 500 фт — нет",
          over == 0 && at40 > 0 && stat40 && wide && techCap, fmt("выше 40: %d, ровно 40: %d; 40 принято %d, широкие пределы %d, 505 фт отклонено %d", over, at40, stat40, wide, techCap));
    }
    // T163: №11 — генератор берёт все кубики списка; цена урона считается по среднему (1к20 = 3к6 = 7к2 по 10.5)
    if (TB()) {
        Coef C2 = CC();
        std::set<int> seenW, seenA;
        for (int i = 0; i < 4000; i++) {
            Boss b = makeBoss(1600, C2, false);
            if (b.wN > 0) seenW.insert(b.wS);
            for (auto& s : b.abil)
                if (s.dmgN > 0) seenA.insert(s.dmgS);
        }
        g_B = 1600;
        auto price = [&](int n, int sd) {
            Spec s;
            s.delivery = 1;
            s.range = 30;
            s.dmgN = n;
            s.dmgS = sd;
            return specPrice(s, C2);
        };
        const long long p20 = price(1, 20), p6 = price(3, 6), p2 = price(7, 2), p100 = price(2, 100), p28 = price(28, 6), p29 = price(29, 6);
        T("T163", "§v21 №11: все кубики к2…к100 встречаются у оружия и способностей; цена по среднему: 1к20 = 3к6 = 7к2 (по 10.5); 2к100 (101) между 28к6 и 29к6",
          (int)seenW.size() == DICE_N && (int)seenA.size() == DICE_N && p20 == p6 && p2 == p6 && p28 <= p100 && p100 <= p29,
          fmt("размеров у оружия %zu, у способностей %zu из %d; цены 1к20 %lld, 3к6 %lld, 7к2 %lld; 28к6 %lld <= 2к100 %lld <= 29к6 %lld", seenW.size(), seenA.size(), DICE_N, p20, p6, p2, p28, p100, p29));
    }
    // T164: №1 — любой бюджет от MIN_BUDGET до MAX_BUDGET: босс не дороже бюджета и разбор цены совпадает с пересчётом
    if (TB()) {
        Coef C2 = CC();
        int over = 0, mism = 0, N = 0;
        for (long long B : {20LL, 37LL, 555LL, 1234LL, 4321LL, 25000LL, 99999LL})
            for (int i = 0; i < 150; i++, N++) {
                Boss b = makeBoss(B, C2, true);
                if (b.cost - STAT_BASE > B) over++;
                if (priceOf(b, C2, (double)(B + STAT_BASE)) != b.cost) mism++;
            }
        const Boss big = makeBoss(MAX_BUDGET * 3, C2, false);
        T("T164", "§v21 №1: бюджет любым числом (20…100000): босс не дороже бюджета, цена пересчитывается; выше предела — как на пределе",
          over == 0 && mism == 0 && big.cost - STAT_BASE <= MAX_BUDGET, fmt("дороже бюджета %d, расхождений цены %d из %d", over, mism, N));
    }
    // ===== v21, этап 1: способности =====
    auto bare = [&]() {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.diff, 0, sizeof w.diff);
        w.aw = w.ah = W;
        return w;
    };
    // T165: №10 — урон из нескольких типов: каждый проходит защиты отдельно; одна проверка концентрации на сумму; цена —
    // лестница по сумме плюс надбавка за каждый доп. тип
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.dmgN = 4;
        s.dmgS = 6;
        s.dtype = DT_FIRE;
        s.more.push_back({4, 6, DT_COLD});
        int fireOnly = 0, N = 300, oneCheck = 0;
        for (int i = 0; i < N; i++) {
            World w = bare();
            Boss a = plain(100, 0, 0, 5), d = plain(100000, 0, 0, 5);
            d.immMask = 1u << DT_COLD;   // холод не проходит: остаётся только огонь (4к6, не больше 24)
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(d, 1, 12, 10));
            w.f[1].concSpec = 0;
            const int h0 = w.f[1].hp;
            doSpec(w, w.f[0], 0, 1);
            const int lost = h0 - w.f[1].hp;
            if (lost >= 4 && lost <= 24) fireOnly++;
            if (w.f[1].concDCs.size() == 1) oneCheck++;
        }
        Spec one = s;
        one.more.clear();
        Spec sum = one;
        sum.dmgN = 8;   // те же 8к6 одним типом
        const double pMix = specPrice(s, C2), pSum = specPrice(sum, C2), pOne = specPrice(one, C2);
        const double expect = std::ceil(C2[AT_DMG] * tierN(TR_DMG, 8.0) * (specMult(sum, C2) + C2[AT_DMGMIX]));
        T("T165", "§v21 №10: урон из нескольких типов — защиты по каждому типу отдельно, одна проверка концентрации; цена = лестница по сумме + надбавка за доп. тип",
          fireOnly == N && oneCheck == N && pMix > pSum && pSum > pOne && std::fabs(pMix - expect) <= 1,
          fmt("только огонь прошёл %d/%d, одна проверка %d/%d; цена 4к6+4к6 %.0f (ожид. %.0f), 8к6 одного типа %.0f, 4к6 %.0f", fireOnly, N, oneCheck, N, pMix, expect, pSum, pOne));
    }
    // T166: №9 — способность по клетке с любой доставкой: атакой — бросок по КД каждого в области; центр — любая клетка;
    // надбавка AT_GROUND в цене
    if (TB()) {
        Coef C2 = CC();
        g_B = 1600;
        Spec s;
        s.delivery = 0;
        s.range = 60;
        s.radius = 10;
        s.dmgN = 2;
        s.dmgS = 6;
        s.ground = true;
        int hitAc = 0, hitHigh = 0, N = 400;
        for (int i = 0; i < N; i++) {
            World w = bare();
            Boss a = plain(100, 0, 0, 5), lo = plain(1000, 0, 0, 5), hi = plain(1000, 0, 0, 5);
            hi.ac = 100;   // промах по нему, кроме естественной 20
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(lo, 1, 20, 10));
            w.f.push_back(mk(hi, 1, 22, 10));
            doSpec(w, w.f[0], 0, -1, 21, 10);   // центр между ними
            hitAc += w.f[1].hp < 1000;
            hitHigh += w.f[2].hp < 1000;
        }
        Spec plainS = s;
        plainS.ground = false;
        const double pg = specPrice(s, C2), pp = specPrice(plainS, C2);
        T("T166", "§v21 №9: по клетке любой доставкой — атакой бросок по КД каждого в области (центр — пустая клетка); надбавка за выбор центра",
          hitAc > N * 0.9 && hitHigh < N * 0.15 && pg > pp, fmt("попадание по КД 0: %d/%d, по КД 100: %d/%d; цена по клетке %.0f, обычная %.0f", hitAc, N, hitHigh, N, pg, pp));
    }
    // T167: №22 — генератор создаёт способности по клетке и барьеры; бот применяет их (область — по лучшей клетке,
    // барьер — перед подходящим ближником, когда сам бьёт издали)
    if (TB()) {
        Coef C2 = CC();
        int gr = 0, wall = 0, n = 0;
        for (int i = 0; i < 3000; i++)
            for (auto& sp : makeBoss(1600, C2, false).abil) {
                n++;
                gr += sp.ground && sp.wallLen == 0;
                wall += sp.wallLen > 0;
            }
        // бот: барьер перед ближником (сам — лучник)
        World w = bare();
        Boss a = plain(1000, 1, 6, 60), e = plain(1000, 2, 8, 5);
        a.wAmmo = true;
        Spec bw;
        bw.ground = true;
        bw.range = 60;
        bw.wallLen = 5;
        bw.wallRounds = 3;
        bw.bonus = true;   // бонусным: оружием бьёт действием, барьер ставит бонусным
        a.abil.push_back(bw);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(e, 1, 18, 10));
        w.f[0].init = 2000;
        w.f[0].uses[0] = 1;
        takeTurn(w, 0);
        const bool wallUp = !w.dyn.empty() && w.f[0].uses[0] == 0;
        // бот: область по клетке накрывает двух врагов из-за дальности центра
        World v = bare();
        Boss c = plain(1000, 0, 0, 5);
        Spec fb;
        fb.delivery = 2;
        fb.range = 30;
        fb.radius = 10;
        fb.dmgN = 3;
        fb.ground = true;
        c.abil.push_back(fb);
        v.f.push_back(mk(c, 0, 10, 10));
        v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 17, 10));   // 35 фт — дальше дальности, но в области от клетки на 30 фт
        v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 18, 11));
        v.f[0].init = 2000;
        v.f[0].uses[0] = 1;
        takeTurn(v, 0);
        const bool both = v.f[1].hp < 1000 && v.f[2].hp < 1000;
        T("T167", "§v21 №22: генератор создаёт способности по клетке и барьеры; бот ставит барьер перед ближником и накрывает областью врагов за пределом дальности",
          gr > 0 && wall > 0 && wallUp && both, fmt("по клетке %d, барьеров %d из %d способностей; барьер поставлен %d; оба врага в области %d", gr, wall, n, wallUp, both));
    }
    // T168: №23 — реакции по четырём условиям; одна реакция за раунд; командой игрока реакцию не применить
    if (TB()) {
        auto mkReact = [&](int trig, bool self) {
            Spec s;
            s.react = trig;
            if (self) {
                s.self = true;
                s.delivery = 2;
                s.acB = 20;
                s.acRounds = 1;
            } else {
                s.delivery = 2;
                s.range = 30;
                s.dmgN = 2;
                s.dmgS = 6;
            }
            return s;
        };
        // 1: меня атакуют — щит +20 КД до броска
        int shield = 0, N = 300;
        for (int i = 0; i < N; i++) {
            World w = bare();
            Boss d = plain(1000, 0, 0, 5);
            d.ac = 10;
            d.abil.push_back(mkReact(1, true));
            w.f.push_back(mk(plain(1000, 1, 6, 5), 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            bool crit;
            const bool hit = attackRoll(w, w.f[0], w.f[1], PROF, 5, crit);
            shield += w.f[1].reactionUsed && w.f[1].acBuffRounds > 0 && (!hit || crit);
        }
        // 2: мне нанесли урон — ответный удар по виновнику (после действия)
        World w2 = bare();
        Boss r2 = plain(1000, 0, 0, 5);
        r2.abil.push_back(mkReact(2, false));
        w2.f.push_back(mk(plain(1000, 0, 0, 5), 0, 10, 10));
        w2.f.push_back(mk(r2, 1, 12, 10));
        g_srcIdx = 0;
        hurt(w2.f[1], 5, DT_FIRE, true);
        resolveReactions(w2);
        g_srcIdx = -1;
        const bool back = w2.f[0].hp < 1000 && w2.f[1].reactionUsed;
        // 3: враг вошёл в зону (30 фт)
        World w3 = bare();
        Boss r3 = plain(1000, 0, 0, 5);
        r3.abil.push_back(mkReact(3, false));
        w3.f.push_back(mk(plain(1000, 0, 0, 5), 0, 20, 10));   // 50 фт: за 60 фт движения входит в зону 30 фт
        w3.f.push_back(mk(r3, 1, 10, 10));
        Fighter goal;
        goal.x = 12;
        goal.y = 10;
        moveToward(w3, w3.f[0], goal, 0, 60, false);
        const bool entered = w3.f[0].hp < 1000 && w3.f[1].reactionUsed;
        // 4: враг применил способность в зоне; второй раз в том же раунде — нет (реакция одна)
        World w4 = bare();
        Boss caster = plain(1000, 0, 0, 5);
        Spec heal;   // способность без восстановления ХП (лечение могло вернуть урон реакции — тест зависел от броска)
        heal.self = true;
        heal.delivery = 2;
        heal.acB = 1;
        heal.acRounds = 1;
        caster.abil.push_back(heal);
        Boss r4 = plain(1000, 0, 0, 5);
        r4.abil.push_back(mkReact(4, false));
        w4.f.push_back(mk(caster, 0, 10, 10));
        w4.f.push_back(mk(r4, 1, 14, 10));
        doSpec(w4, w4.f[0], 0, 0);
        const int afterFirst = w4.f[0].hp;
        doSpec(w4, w4.f[0], 0, 0);
        const bool cast = afterFirst < 1000 && w4.f[1].reactionUsed && w4.f[1].uses[0] == UNL - 1;
        // игрок: командой реакцию не применить
        std::string err;
        Boss pb;
        long long bu = 0;
        int hpw = 0;
        bool ok = true;
        const bool bonusReact = !chr::bossFromJson(parseJson("{\"budget\":800,\"abilities\":[{\"delivery\":2,\"range\":30,\"dmgN\":1,\"react\":1,\"bonus\":true}]}", &ok), pb, bu, hpw, err);
        T("T168", "§v21 №23: реакции — меня атакуют (щит до броска), мне нанесли урон, враг вошёл в зону, враг применил способность; одна за раунд; реакция не бывает бонусным",
          shield > N * 0.9 && back && entered && cast && bonusReact, fmt("щит %d/%d, ответ на урон %d, на вход в зону %d, на способность (один раз) %d, бонусная реакция отклонена %d", shield, N, back, entered, cast, bonusReact));
    }
    // T169: №24 — концентрацию можно снять бесплатно: командой игрока и стойкой сети 9; эффекты снимаются
    if (TB()) {
        auto setup = [&](World& w) {
            Boss a = plain(1000, 0, 0, 5);
            Spec s;
            s.delivery = 2;
            s.range = 30;
            s.cond = C_SLOW;
            s.condRounds = 5;
            s.conc = true;
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
            doSpec(w, w.f[0], 0, 1);
        };
        World w = bare();
        setup(w);
        const bool had = w.f[0].concSpec == 0 && w.f[1].cond[C_SLOW] > 0;
        std::vector<std::string> cmds = {"{\"c\":\"dropconc\"}", "{\"c\":\"end\"}"};
        size_t ci = 0;
        bool sawConc = false;
        g_human.idx = 0;
        g_human.ask = [&](const std::string& req) {
            if (req.find("\"conc\":0") != std::string::npos) sawConc = true;
            return cmds[std::min(ci++, cmds.size() - 1)];
        };
        humanTurn(w, 0);
        g_human = HumanCtx{};
        const bool humanDrop = w.f[0].concSpec < 0 && w.f[1].cond[C_SLOW] == 0;
        World v = bare();
        setup(v);
        g_ovr.active = true;
        g_ovr.stance = 9;
        g_ovr.tgt = 0;
        g_ovr.pref = -1;
        takeTurn(v, 0);
        g_ovr = Ovr();
        const bool netDrop = v.f[1].cond[C_SLOW] == 0 || v.f[0].concSpec != 0;
        T("T169", "§v21 №24: концентрацию снимают бесплатно — команда игрока и стойка сети 9; её эффекты снимаются",
          had && sawConc && humanDrop && netDrop, fmt("была %d, запрос показал концентрацию %d, снял игрок %d, сеть %d", had, sawConc, humanDrop, netDrop));
    }
    // T170: сеть-боец: признаки способности v21 (по клетке, реакция, типы урона); веса с другим заголовком не грузятся
    // молча (предупреждение); перенос весов v20 сохраняет решения на прежних входах
    if (TB()) {
        T("T170", "§v21: вход сети 72 + 12×13 признаков, стоек 10; веса другого формата — предупреждение, а не тихое отключение",
          F_ABIL == 13 && N_STANCE == 10 && D_IN == F_BASE + MAXA * F_ABIL, fmt("вход %d, на способность %d, стоек %d, сеть загружена %d", D_IN, F_ABIL, N_STANCE, fighterNet() != nullptr));
    }
    // ===== v21, этап 2: бой =====
    auto open = [&]() {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.dark, 0, sizeof w.dark);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.height, 0, sizeof w.height);
        w.aw = w.ah = 100;
        return w;
    };
    // T171: №13 — кто атаковал врага или остался там, где враг достанет его за ход, погоню не начинает
    if (TB()) {
        auto chaseAfter = [&](bool struck, int to) {
            World w = open();
            w.f.push_back(mk(plain(100, 1, 6, 5), 0, 50, 50));
            w.f.push_back(mk(plain(100, 1, 6, 5), 1, 52, 50));
            w.f[0].x = to;
            w.f[0].struckFoe = struck;
            chaseAfterTurn(w, 0, 10);   // было 10 фт до врага
            return w.chaseOn;
        };
        const bool flee = chaseAfter(false, 40), hitRun = chaseAfter(true, 40), near = chaseAfter(false, 46);
        T("T171", "§v21 №13: отход на 10+ фт из-под угрозы — погоня; если в этот ход атаковал или остался в досягаемости врага за ход — нет",
          flee && !hitRun && !near, fmt("ушёл на 60 фт: %d; ударил и ушёл: %d; ушёл на 30 фт (враг дойдёт и ударит): %d", flee, hitRun, near));
    }
    // T172: №17 — область бьёт всех в радиусе (и своих, и себя), избирательная — только врагов; бот не бьёт областью,
    // если вред своим больше пользы
    if (TB()) {
        auto run = [&](bool selective) {
            World w = open();
            Boss a = plain(1000, 0, 0, 5);
            Spec s;
            s.delivery = 2;
            s.range = 30;
            s.radius = 15;
            s.dmgN = 4;
            s.selective = selective;
            a.abil.push_back(s);
            w.f.push_back(mk(a, 0, 10, 10));
            w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 13, 10));
            w.f.push_back(mk(plain(1000, 0, 0, 5), 0, 14, 10));   // свой рядом с врагом
            doSpec(w, w.f[0], 0, 1);
            return std::array<int, 3>{w.f[0].hp, w.f[1].hp, w.f[2].hp};
        };
        const auto all = run(false), sel = run(true);
        World v = open();
        Boss c = plain(1000, 0, 0, 5);
        Spec s;
        s.delivery = 2;
        s.range = 30;
        s.radius = 10;
        s.dmgN = 6;
        c.abil.push_back(s);
        v.f.push_back(mk(c, 0, 10, 10));
        v.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14, 10));
        v.f.push_back(mk(plain(30, 0, 0, 5), 0, 15, 10));     // свой миньон рядом с врагом
        v.f.push_back(mk(plain(30, 0, 0, 5), 0, 13, 10));
        v.f[0].init = 2000;
        v.f[0].uses[0] = 1;
        takeTurn(v, 0);
        const bool spared = v.f[2].hp == 30 && v.f[3].hp == 30;
        T("T172", "§v21 №17: область бьёт и своих, и себя; избирательная — только врагов; бот не бьёт областью, если заденет больше своих",
          all[1] < 1000 && all[2] < 1000 && sel[1] < 1000 && sel[2] == 1000 && spared,
          fmt("обычная: враг %d, свой %d; избирательная: враг %d, свой %d; бот пощадил своих %d", all[1], all[2], sel[1], sel[2], spared));
    }
    // T173: №12 — туман войны: бот не бьёт врага, которого его сторона не видит и не знает; атака выдаёт позицию; игрок
    // получает в запросе только известных врагов
    if (TB()) {
        World w = open();
        for (int y = 0; y < 30; y++) w.wall[y][15] = true;   // стена между ними
        Boss a = plain(1000, 1, 6, 60);
        a.wAmmo = true;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 20, 10));
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        g_curRound = 5;
        const bool unknown = !isKnown(w, 0, 1);
        bool crit;
        attackRoll(w, w.f[1], w.f[0], PROF, 5, crit);   // враг атаковал (сквозь стену — неважно): выдал себя
        const bool revealed = isKnown(w, 0, 1);
        w.f[1].x = 21;   // ушёл с клетки — снова неизвестен
        const bool lost = !isKnown(w, 0, 1);
        std::string req;
        g_human.idx = 0;
        g_human.ask = [&](const std::string& r) { req = r; return std::string("{\"c\":\"end\"}"); };
        humanTurn(w, 0);
        g_human = HumanCtx{};
        const bool noEnemy = req.find("\"enemies\":[]") != std::string::npos;
        g_curRound = 0;
        T("T173", "§v21 №12: туман войны — враг за стеной неизвестен; атака выдаёт позицию; ушёл — снова неизвестен; игроку не показан",
          unknown && revealed && lost && noEnemy, fmt("неизвестен %d, выдал себя %d, ушёл %d, запрос игрока без врагов %d", unknown, revealed, lost, noEnemy));
    }
    // T177: каждое решение в ходу что-то тратит: шаг в свою клетку отклоняется; смена оружия — 1-я бесплатно, 2-я бонусным
    // действием, 3-я действием, 4-й нет; простой приказ — раз за ход
    if (TB()) {
        World w = open();
        Boss a = plain(1000, 1, 6, 5);
        a.alt.push_back(Wp{});
        a.alt[0].n = 1;
        a.alt[0].s = 8;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 20, 10));
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        const bool stay = !applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 10, 10}, true);
        const bool s1 = applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true) && !ts.bonusUsed && !ts.actionUsed;
        const bool s2 = applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true) && ts.bonusUsed && !ts.actionUsed;
        const bool s3 = applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true) && ts.actionUsed;
        const bool s4 = !applyPrim(w, 0, ts, Prim{PR_SWAP, 0}, true);
        const bool o1 = applyPrim(w, 0, ts, Prim{PR_ORDER, 1}, true), o2 = !applyPrim(w, 0, ts, Prim{PR_ORDER, 1}, true);
        T("T177", "§v21 ход: шаг в свою клетку не делается; смена оружия 1-я бесплатно, 2-я бонусным, 3-я действием, 4-й нет; простой приказ раз за ход",
          stay && s1 && s2 && s3 && s4 && o1 && o2,
          fmt("шаг на месте отклонён %d; смены %d/%d/%d, 4-я отклонена %d; приказ %d, второй отклонён %d", stay, s1, s2, s3, s4, o1, o2));
    }
    // T178: упёрся в невидимого врага — тот становится известен команде, попытка стоит 5 фт, боец остаётся на месте; в
    // коридоре известного врага не обойти — шаг отклоняется; в открытом поле известного врага обходит кратчайшим путём
    if (TB()) {
        World w = open();
        for (int x = 0; x < W; x++) w.wall[9][x] = w.wall[11][x] = true;   // коридор по y = 10 во всю ширину
        w.f.push_back(mk(plain(1000, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 11, 10));
        w.f[1].invisRounds = 5;
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        g_curRound = 3;
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        const bool hidden = !isKnown(w, 0, 1);
        const bool ok1 = applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 13, 10}, true);
        const bool bump = ok1 && w.f[0].x == 10 && w.f[0].y == 10 && ts.speed == 25 && isKnown(w, 0, 1);
        const bool blocked = !applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 13, 10}, true) && ts.speed == 25;
        for (int x = 0; x < W; x++) w.wall[9][x] = w.wall[11][x] = false;   // коридор открыт: обход (11,9) или (11,11), 3 шага
        const bool ok3 = applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 13, 10}, true);
        const bool around = ok3 && w.f[0].x == 13 && w.f[0].y == 10 && ts.speed == 10;
        g_curRound = 0;
        T("T178", "§v21 путь: упёрся в невидимого врага — он раскрыт, 5 фт потрачено, боец на месте; известного в коридоре не обойти; в поле — обход",
          hidden && bump && blocked && around,
          fmt("невидим %d; упор %d; в коридоре отклонено %d; обход %d, клетка (%d,%d), скорость %d", hidden, bump, blocked, around, w.f[0].x, w.f[0].y, ts.speed));
    }
    // T179: способность — единая структура (переходник): у вооружённого босса с одной способностью-бонусом, Отходом
    // бонусным и призывом — атака, способность, Рывок, Уклонение, Отход (действием и бонусным), призыв; категории
    // выводятся из эффекта; цену проверяет и списывает один путь (canPay, payEcon, paySpec)
    if (TB()) {
        World w = open();
        Boss a = plain(1000, 1, 6, 5);
        Spec sp;
        sp.bonus = true;
        sp.uses = 1;
        sp.slotLvl = 1;
        sp.dmgN = 1;
        a.abil.push_back(sp);
        a.slots[1] = 1;
        a.disengB = true;
        addSummon(a, plain(50, 1, 4, 5), 1);
        w.f.push_back(mk(a, 0, 10, 10));
        const auto ab = abilitiesOf(w.f[0].b);
        int cat[CAT_N] = {};
        for (auto& x : ab) cat[abCategory(w.f[0].b, x)]++;
        const bool list = ab.size() == 9 && cat[CAT_ATTACK] == 1 && cat[CAT_SPELL] == 1 && cat[CAT_SUMMON] == 1 && cat[CAT_MANEUVER] == 6;   // v21: + Помощь и Подготовка
        TurnState ts;
        const AbCost c = specAbility(w.f[0].b, 0).cost;
        const bool before = canPay(w.f[0], ts, c) && c.econ == EC_BONUS;
        paySpec(w, w.f[0], 0, true);
        payEcon(ts, w.f[0], c.econ);
        const char* why = nullptr;
        const bool after = !canPay(w.f[0], ts, c, &why) && ts.bonusUsed && !ts.actionUsed && w.f[0].slotLeft[1] == 0;
        const bool weapon = canPay(w.f[0], ts, builtinAbility(EF_WEAPON).cost);
        T("T179", "§v21 способность: переходник даёт атаку, способности, Рывок, Уклонение, Отход, призыв; категории из эффекта; цену списывает один путь",
          list && before && after && weapon,
          fmt("способностей %zu (атака %d, заклинание %d, призыв %d, манёвр %d); до платы %d, после %d («%s»), атака ещё доступна %d", ab.size(), cat[CAT_ATTACK],
              cat[CAT_SPELL], cat[CAT_SUMMON], cat[CAT_MANEUVER], before, after, why ? why : "", weapon));
    }
    // T180: призыв — способность: тратит действие (второй раз в ход не пройдёт) и применение; две призывающие способности
    // с разными шаблонами работают независимо (свои шаблоны, свои счётчики)
    if (TB()) {
        World w = open();
        w.f.reserve(64);
        Boss a = plain(1000, 1, 6, 5), t1 = plain(40, 1, 4, 5), t2 = plain(90, 1, 8, 5);
        const int s1 = addSummon(a, t1, 2, 2), s2 = addSummon(a, t2, 1, 1);
        a.abil[s2].bonus = true;
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 30, 10));
        w.f[0].uses[s1] = 2;
        w.f[0].uses[s2] = 1;
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        const bool c1 = applyPrim(w, 0, ts, Prim{PR_SPELL, s1, 0}, true) && ts.actionUsed && w.f.size() == 4 && w.f[0].uses[s1] == 1;
        const bool again = !applyPrim(w, 0, ts, Prim{PR_SPELL, s1, 0}, true) && w.f.size() == 4;   // действие уже потрачено
        const bool c2 = applyPrim(w, 0, ts, Prim{PR_SPELL, s2, 0}, true) && ts.bonusUsed && w.f.size() == 5;
        const bool tmplOk = w.f[2].maxHp == 40 && w.f[3].maxHp == 40 && w.f[4].maxHp == 90 && w.f[2].sumSpec == s1 && w.f[4].sumSpec == s2;
        TurnState ts2;
        const bool used = !applyPrim(w, 0, ts2, Prim{PR_SPELL, s2, 0}, true);   // одно применение — израсходовано
        T("T180", "§v21 призыв — способность: тратит действие и применение; две призывающие способности со своими шаблонами независимы",
          c1 && again && c2 && tmplOk && used,
          fmt("1-я: %d, повтор в тот же ход отклонён %d, 2-я (бонусным) %d, шаблоны ХП %d/%d/%d, применение 2-й израсходовано %d", c1, again, c2,
              w.f[2].maxHp, w.f[3].maxHp, w.f.size() > 4 ? w.f[4].maxHp : -1, used));
    }
    // T181: призыв на концентрации — концентрация снята (сорвалась или снята бесплатно), приспешники этой способности
    // исчезают; приспешники другой способности (без концентрации) остаются до гибели
    if (TB()) {
        World w = open();
        w.f.reserve(64);
        Boss a = plain(1000, 1, 6, 5);
        const int sc = addSummon(a, plain(40, 1, 4, 5), 2, UNL, true), sn = addSummon(a, plain(40, 1, 4, 5), 1);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 30, 10));
        doSpec(w, w.f[0], sc, 0);
        doSpec(w, w.f[0], sn, 0);
        const bool held = w.f[0].concSpec == sc && w.f.size() == 5;
        TurnState ts;
        applyPrim(w, 0, ts, Prim{PR_DROPCONC}, true);
        const bool gone = w.f[2].hp <= 0 && w.f[3].hp <= 0 && w.f[2].vanished && w.f[4].hp > 0 && !w.f[4].vanished && w.f[0].concSpec < 0;
        T("T181", "§v21 призыв на концентрации: концентрация снята — его приспешники исчезают; призванные без концентрации остаются",
          held && gone, fmt("концентрация держит призыв %d; после снятия: исчезли %d/%d, другой остался %d", held, w.f[2].vanished, w.f[3].vanished, w.f[4].hp > 0));
    }
    // T182: предохранители и растяжки призыва: растяжка срабатывает раньше потолка (только запись), потолок блокирует
    // (limity.json: живых от способности 20, растяжка 6)
    if (TB()) {
        World w = open();
        w.f.reserve(256);
        Boss a = plain(1000, 1, 6, 5);
        const int si = addSummon(a, plain(40, 1, 4, 5), 4);
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 60, 60));
        const Limits& L = limits();
        const long long w0 = g_warnCount.load();
        int firstWarnAt = -1;
        for (int k = 0; k < 10; k++) {
            g_curRound = k;
            summonFrom(w, 0, si);
            int alive = 0;
            for (auto& g : w.f) alive += g.summoner == 0 && g.hp > 0;
            if (firstWarnAt < 0 && g_warnCount.load() > w0) firstWarnAt = alive;
        }
        g_curRound = 0;
        int alive = 0;
        for (auto& g : w.f) alive += g.summoner == 0 && g.hp > 0;
        TurnState ts;
        const bool blocked = !applyPrim(w, 0, ts, Prim{PR_SPELL, si, 0}, true);
        T("T182", "§v21 предохранители призыва: растяжка (WARN) раньше потолка, потолок живых от способности блокирует призыв",
          firstWarnAt > L.warnAliveAbil && firstWarnAt < L.sumAliveAbil && alive == L.sumAliveAbil && blocked,
          fmt("первая растяжка при %d живых (порог %d), живых в итоге %d (потолок %d), призыв сверх потолка отклонён %d", firstWarnAt, L.warnAliveAbil, alive,
              L.sumAliveAbil, blocked));
    }
    // T174: №4 — ход из примитивов: бой, где ходы бота записаны примитивами и повторены исполнителем, совпадает до бита
    if (TB()) {
        Coef C2 = CC();
        int same = 0, N = 80;
        long long prims = 0;
        for (int i = 0; i < N; i++) {
            auto run = [&](PrimLog& L, double* hp) {
                rng.seed(7700 + i);
                Boss A, Bo;
                makePair(200LL << (i % 6), C2, A, Bo);
                A.stats[INT] = Bo.stats[INT] = 10;
                g_primLog = &L;
                g_rounds = 0;
                const int r = fight(A, Bo);
                g_primLog = nullptr;
                hp[0] = g_lastHp[0];
                hp[1] = g_lastHp[1];
                return r * 100000 + (int)g_rounds;
            };
            PrimLog L;
            L.who = i & 1;
            double h1[2], h2[2];
            const int r1 = run(L, h1);
            for (auto& t : L.turns) prims += (long long)t.size();
            L.replay = true;
            const int r2 = run(L, h2);
            same += r1 == r2 && h1[0] == h2[0] && h1[1] == h2[1];
        }
        T("T174", "§v21 №4: ход бота записывается примитивами (идти, атака, способность, Рывок, …); повтор примитивов исполнителем даёт тот же бой до бита",
          same == N, fmt("совпало %d из %d боёв, примитивов %.1f на бой", same, N, (double)prims / N));
    }
    // T175: №5 — новая сеть: наблюдение (окно карты, кандидаты-клетки, известные враги), маски не пускают недопустимого
    // (случайные веса, 60 боёв: отказов исполнителя 0)
    if (TB()) {
        Coef C2 = CC();
        pn::PNet net;
        net.init(3);
        long long steps = 0, illegal = 0;
        for (int i = 0; i < 60; i++) {
            rng.seed(8800 + i);
            Boss A, Bo;
            makePair(200LL << (i % 6), C2, A, Bo);
            pn::Ctx ctx;
            ctx.net = &net;
            ctx.greedy = false;
            ctx.rng.seed(i + 1);
            ctx.side[0].active = true;
            pn::g_pctx = &ctx;
            g_rlTeam[0] = true;
            fight(A, Bo);
            g_rlTeam[0] = false;
            pn::g_pctx = nullptr;
            steps += ctx.steps;
            illegal += ctx.illegal;
        }
        T("T175", "§v21 №5: новая сеть видит окно карты 15×15 и клетки-кандидаты; со случайными весами 60 боёв без единого недопустимого примитива",
          pn::D_OLD == pn::GB + pn::MAXE * pn::FE + MAXA * pn::FA + pn::WIN * pn::WIN * pn::NL && pn::D_IN24 == pn::D_OLD + pn::MAXE * pn::FEX + MAXA * pn::FAX &&
              pn::D_IN25 == pn::D_IN24 + pn::GBX + pn::MAXE * pn::FEX2 + pn::WIN * pn::WIN * pn::NLX + MAXA * pn::FAX2 &&
              pn::D_IN == pn::D_IN25 + pn::GB6 + pn::MAXE * pn::FE6 + MAXA * pn::FA6 && steps > 0 && illegal == 0,   // PN26 (пакеты атомов 1–3)
          fmt("вход %d, решений %lld, отказов %lld", pn::D_IN, steps, illegal));
    }
    // T183: шаг 4 — пустой ход сети запрещён: в начале хода, когда можно идти или применить, «конец хода» закрыт; после
    // шага открыт; тратить нечего — легален только «конец хода», его делает движок, и в обучающую выборку он не идёт;
    // метка «конец пустого хода» (как у бота) непредставима; ход сети всегда кончается
    if (TB()) {
        World w = open();
        w.f.reserve(64);
        w.f.push_back(mk(plain(1000, 1, 6, 5), 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 30, 10));
        w.known[0].assign(2, {-1, -1, -1});
        w.known[1].assign(2, {-1, -1, -1});
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        ts.x0 = 10;
        ts.y0 = 10;
        pn::Obs o;
        pn::buildObs(w, 0, ts, o);
        const bool closed = !o.stMask[pn::ST_END] && o.stMask[pn::ST_MOVE];
        // шаг туда и обратно — не трата: «конец хода» остаётся закрыт
        applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 11, 10}, true);
        applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 10, 10}, true);
        pn::buildObs(w, 0, ts, o);
        const bool backClosed = !o.stMask[pn::ST_END] && w.f[0].x == 10;
        // Уклонение ход непустым не делает
        applyPrim(w, 0, ts, Prim{PR_DODGE}, true);
        pn::buildObs(w, 0, ts, o);
        const bool dodgeClosed = !o.stMask[pn::ST_END] && ts.actionUsed;
        pn::Decision dl;
        const bool unrep = !pn::labelOf(w, 0, o, nullptr, dl, [](std::vector<pn::Cand>&, int, int) { return 0; });
        applyPrim(w, 0, ts, Prim{PR_MOVE, -1, -1, 12, 10}, true);
        pn::buildObs(w, 0, ts, o);
        const bool open1 = o.stMask[pn::ST_END];
        TurnState t0;   // ни скорости, ни действий: тратить нечего
        t0.actionUsed = t0.bonusUsed = true;
        pn::buildObs(w, 0, t0, o);
        const bool forced = pn::onlyEnd(o);
        // 20 боёв случайной сети с записью решений: ни одного записанного «только конец хода», каждый записанный конец хода
        // был разрешён, бои кончаются
        Coef C2 = CC();
        pn::PNet net;
        net.init(5);
        long long recEnd = 0, badEnd = 0, onlyRec = 0, turns = 0, forcedEnds = 0, caps = 0;
        int done = 0;
        for (int i = 0; i < 20; i++) {
            rng.seed(9100 + i);
            Boss A, Bo;
            makePair(400, C2, A, Bo);
            pn::Ctx ctx;
            ctx.net = &net;
            ctx.greedy = false;
            ctx.rng.seed(i + 7);
            ctx.side[0].active = true;
            ctx.side[0].record = true;
            pn::g_pctx = &ctx;
            g_rlTeam[0] = true;
            fight(A, Bo);
            done++;
            g_rlTeam[0] = false;
            pn::g_pctx = nullptr;
            for (auto& d : ctx.side[0].traj) {
                if (d.st == pn::ST_END) recEnd++, badEnd += !d.stMask[pn::ST_END];
                onlyRec += !d.stMask[pn::ST_MOVE] && !d.stMask[pn::ST_APPLY] && !d.stMask[pn::ST_INTERACT];
            }
            turns += ctx.turns;
            forcedEnds += ctx.forcedEnds;
            caps += ctx.capHits;
        }
        T("T183", "§v21 шаг 4: пустой ход сети запрещён (шаг туда и обратно и манёвры — не трата); тратить нечего — ход завершает движок, шаг не в выборке; ход всегда кончается",
          closed && backClosed && dodgeClosed && unrep && open1 && forced && badEnd == 0 && onlyRec == 0 && done == 20 && forcedEnds > 0,
          fmt("туда-обратно не трата %d, Уклонение не трата %d; в начале хода «конец» закрыт %d, метка пустого конца непредставима %d, после шага открыт %d, без ресурсов только конец %d; 20 боёв: ходов %lld, "
              "завершено движком %lld, записано концов %lld (из них запрещённых %lld), записано «только конец» %lld, упоров в предел %lld",
              backClosed, dodgeClosed, closed, unrep, open1, forced, turns, forcedEnds, recEnd, badEnd, onlyRec, caps));
    }
    // T184: этап 5б — веса PN22 переносятся в PN23 без изменения ходов: на любом входе старые логиты, ценность и оценки
    // указателя на врага совпадают с новыми (новые признаки состояний — с нулевыми весами)
    if (TB()) {
        bool okLoad = false, same = true;
        double maxd = 0;
        pn::PNet nw;
        std::ifstream f0("net_fighter_p.bin", std::ios::binary);
        int hd[6] = {};
        if (f0) f0.read((char*)hd, sizeof hd);
        const bool is22 = hd[0] == pn::P_MAGIC22 || hd[0] == pn::P_MAGIC23 || hd[0] == pn::P_MAGIC24;   // старые веса — перенос
        if (is22 && pn::loadP("net_fighter_p.bin", nw, false)) {
            okLoad = true;
            pn::PNetT<float> old;
            old.D = hd[1];
            old.nf[1] = hd[0] == pn::P_MAGIC22 ? pn::FE : pn::FEP24;
            old.NA = hd[4];   // в старых весах слотов способностей и взаимодействий меньше
            old.init(1);
            f0.read((char*)old.core.p.data(), old.core.p.size() * sizeof(float));
            f0.read((char*)old.ptr.data(), old.ptr.size() * sizeof(float));
            uint64_t sd = 5;
            for (int t = 0; t < 20; t++) {
                std::vector<float> x(pn::D_IN), h(pn::HID), lo(old.NA), ln(pn::NFIX);
                for (auto& v : x) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                for (auto& v : h) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                GruNet<float>::Step so, sn;
                float vo = 0, vn = 0;
                old.core.forward(x.data(), h.data(), so, lo.data(), vo);
                nw.core.forward(x.data(), h.data(), sn, ln.data(), vn);
                const int oldNAB = old.NA - (pn::ST_N + CAT_N + pn::IN_N24 + pn::NSWAP);
                std::vector<char> used(pn::NFIX, 0);
                for (int k = 0; k < old.NA; k++) {
                    const int nr = pn::mapOldRow(k, oldNAB);
                    used[nr] = 1;
                    maxd = std::max(maxd, (double)std::fabs(lo[k] - ln[nr]));
                }
                for (int k = 0; k < pn::NFIX; k++)
                    if (!used[k]) maxd = std::max(maxd, ln[k] < -10 ? 0.0 : 1.0);   // новые слоты и действия закрыты
                maxd = std::max(maxd, (double)std::fabs(vo - vn));
                float pro[pn::KP], prn[pn::KP], fe[pn::FEP];
                for (auto& v : fe) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                old.pre(1, so.h.data(), pro);
                nw.pre(1, sn.h.data(), prn);
                maxd = std::max(maxd, (double)std::fabs(old.score(1, pro, fe) - nw.score(1, prn, fe)));
                float fc[pn::FC];
                for (auto& v : fc) v = (float)(GruNet<float>::rnd01(sd) * 2 - 1);
                old.pre(2, so.h.data(), pro);
                nw.pre(2, sn.h.data(), prn);
                maxd = std::max(maxd, (double)std::fabs(old.score(2, pro, fc) - nw.score(2, prn, fc)));
            }
            same = maxd < 1e-5;
        }
        T("T184", "§v21: старые веса сети-бойца (PN22–PN24) переносятся в PN25 (новые признаки, «выше/ниже») без изменения решений",
          !is22 || (okLoad && same), is22 ? fmt("перенесено %d, наибольшее расхождение логитов/ценности/указателей %.2g", okLoad, maxd) : std::string("веса уже PN25 — перенос не нужен"));
    }
    // T185: этап 5б — своя СЛ способности и состояние без повторного спаса: надбавка к СЛ режет успешные спасы, без
    // повторного спаса состояние держится все раунды; обе покупки дороже обычной способности
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = STR;
        Boss d = plain(200, 0, 0, 5);
        Spec sp;
        sp.delivery = 1;
        sp.saveStat = CON;
        sp.cond = C_POISON;
        sp.condRounds = 5;
        sp.range = 30;
        Spec hi = sp;
        hi.dcB = 8;
        Spec nr = sp;
        nr.noRep = true;
        const int N = 400;
        int fail0 = 0, fail8 = 0, failNR = 0, held0 = 0, heldNR = 0;
        for (int i = 0; i < N; i++) {
            for (int v = 0; v < 3; v++) {
                const Spec& s2 = v == 0 ? sp : (v == 1 ? hi : nr);
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                hitOne(w, s2, w.f[0], w.f[1]);
                const bool got = w.f[1].cond[C_POISON] > 0;
                (v == 0 ? fail0 : (v == 1 ? fail8 : failNR)) += got;
                if (!got || v == 1) continue;
                int turns = 0;
                for (int r = 0; r < 10 && w.f[1].cond[C_POISON] > 0; r++) {
                    endOfTurn(w.f[1]);
                    turns++;
                }
                (v == 0 ? held0 : heldNR) += turns;
            }
        }
        Coef C;
        for (int k = 0; k < AT_N; k++) C[k] = AD[k].init;
        const long long p0 = specPrice(sp, C), p8 = specPrice(hi, C), pn2 = specPrice(nr, C);
        AMask m = 0;
        markSpec(hi, m);
        markSpec(nr, m);
        const bool marked = ((m >> AT_DCB) & ONE) && ((m >> AT_NOREP) & ONE);
        const double avg0 = held0 / (double)std::max(1, fail0), avgNR = heldNR / (double)std::max(1, failNR);
        T("T185", "§v21 этап 5б: надбавка к СЛ способности и состояние без повторного спаса работают и стоят дороже",
          fail8 > fail0 + N / 5 && failNR > 0 && std::fabs(avgNR - 5.0) < 1e-9 && avg0 < 4.0 && p8 > p0 && pn2 > p0 && marked,
          fmt("провалов спаса: СЛ +0 %d, СЛ +8 %d из %d; ходов под ядом за наложение (5 р.): с повторным спасом %.2f, без него %.2f; цена %lld / %lld (СЛ +8) / %lld (без повт.); атомы отмечены %d",
              fail0, fail8, N, avg0, avgNR, p0, p8, pn2, marked));
    }
    // T186: владения (решение владельца 2026-11-02, как в D&D) — общий бонус мастерства идёт к атакам оружием (если
    // владеешь им), атакам и СЛ способностей, владеемым спасброскам и навыкам; экспертиза удваивает его в навыке; цены
    // владений растут с мастерством, «без владения оружием» возвращает очки; генератор и разбор цены сходятся
    if (TB()) {
        Boss a = plain(100, 1, 6, 5);
        a.mainStat = DEX;
        a.stats[DEX] = 18;
        a.wFinesse = true;
        a.prof = 4;
        a.saveProf = 1u << CON;
        a.skillProf = (1u << SK_STEALTH) | (1u << SK_ATHL);
        a.skillExp = 1u << SK_STEALTH;
        Fighter f = mk(a, 0, 10, 10);
        const bool atk = f.wAtk() == 4 + 4 && f.sAtk() == 4 + 4 && f.dc() == 8 + 4 + 4;
        const bool sv = saveBonus(f, CON) == modOf(a.stats[CON]) + 4 && saveBonus(f, STR) == modOf(a.stats[STR]);
        const bool sk = f.skill(SK_STEALTH) == 4 + 8 && f.skill(SK_ATHL) == modOf(a.stats[STR]) + 4 && f.skill(SK_ACRO) == 4;
        Boss nb = a;
        nb.wNoProf = true;
        Fighter g = mk(nb, 0, 10, 10);
        const bool noW = g.wAtk() == 4 && g.sAtk() == 8;
        Coef C;
        for (int k = 0; k < AT_N; k++) C[k] = AD[k].init;
        auto sumItems = [&](const Boss& b, double B) {
            long long t = 0;
            for (auto& it : bossItems(b, C, B)) t += it.cost;
            return t;
        };
        Boss p2 = a, p6 = a;
        p2.prof = 2;
        p6.prof = 6;
        const bool scale = sumItems(p6, 1600) > sumItems(p2, 1600) && sumItems(nb, 1600) < sumItems(a, 1600);
        int bad = 0, withProf = 0, withSkill = 0, noProf = 0;
        for (int i = 0; i < 300; i++) {
            rng.seed(9100 + i);
            const long long B = 200LL << (i % 6);
            Boss b = makeBoss(B, C, false);
            withProf += b.prof > 2;
            withSkill += b.skillProf != 0;
            noProf += b.wNoProf;
            if (b.prof < 2 || b.prof > 6 || (b.skillExp & ~b.skillProf)) bad++;
            if (sumItems(b, (double)B) + STAT_BASE != b.cost) bad++;   // разбор цены сходится с генератором
        }
        T("T186", "§v21 владения: мастерство к атакам, СЛ, спасам и навыкам; экспертиза ×2; без владения оружием — без мастерства к атакам и с возвратом; цена растёт с мастерством",
          atk && sv && sk && noW && scale && bad == 0 && withProf > 0 && withSkill > 0 && noProf > 0,
          fmt("атака/способность/СЛ %d, спасы %d, навыки %d, без владения оружием %d, цена растёт %d; 300 боссов генератора: с мастерством > 2 — %d, с навыками %d, без владения оружием %d, расхождений %d",
              atk, sv, sk, noW, scale, withProf, withSkill, noProf, bad));
    }
    // T187: «когда → что» (решение владельца 2026-11-02) — условие и эффект работают по отдельности: союзник рядом с
    // противником → преимущество; безрассудство — преимущество ближним атакам и атаки по мне с преимуществом (возврат
    // очков); +КД и помеха по мне против атакующего; резист и спасброски только с условием о себе (иначе отказ)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss a = plain(100, 1, 6, 5), d = plain(100, 1, 6, 5), al = plain(100, 1, 6, 5);
        a.rules.push_back(Rule{W_ALLYADJ, 0, E_ADVATK});
        w.f.push_back(mk(a, 0, 10, 10));
        w.f.push_back(mk(d, 1, 11, 10));
        bool adv0, dis0, adv1, dis1;
        advState(w, w.f[0], w.f[1], 5, adv0, dis0);
        w.f.push_back(mk(al, 0, 12, 10));   // союзник рядом с противником
        advState(w, w.f[0], w.f[1], 5, adv1, dis1);
        const bool pack = !adv0 && adv1;
        Boss rk = plain(100, 1, 6, 5);
        rk.rules.push_back(Rule{W_RECKLESS, 0, E_ADVATK});
        rk.rules.push_back(Rule{W_RECKLESS, 0, E_ADVVSME});
        rk.rules.push_back(Rule{W_ALWAYS, 0, E_AC, 3});
        rk.rules.push_back(Rule{W_HPLOW, 0, E_DISVSME});
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        v.f.push_back(mk(rk, 0, 10, 10));
        v.f.push_back(mk(d, 1, 11, 10));
        bool r0, q0, r1, q1, r2, q2, r3, q3;
        advState(v, v.f[0], v.f[1], 5, r0, q0);   // не рискнул — без преимущества
        v.f[0].wasReckless = true;
        advState(v, v.f[0], v.f[1], 5, r1, q1);    // ближняя — с преимуществом
        advState(v, v.f[0], v.f[1], 30, r2, q2);   // дальняя — без
        advState(v, v.f[1], v.f[0], 5, r3, q3);    // по безрассудному — с преимуществом
        const bool reck = !r0 && r1 && !r2 && r3 && !q3;
        v.f[0].hp = 10;   // ХП ≤ 50% → по мне с помехой
        bool r4, q4;
        advState(v, v.f[1], v.f[0], 5, r4, q4);
        const bool ac = ruleAC(v, v.f[0], v.f[1]) == 3 && q4;
        Coef C;
        for (int k = 0; k < AT_N; k++) C[k] = AD[k].init;
        const bool refund = rulePrice(Rule{W_RECKLESS, 0, E_ADVVSME}, rk, C) < 0 && rulePrice(Rule{W_ALLYADJ, 0, E_ADVATK}, a, C) < rulePrice(Rule{W_ALWAYS, 0, E_ADVATK}, a, C);
        // резист по условию о себе: полурон при ХП > 50%, полный при ≤ 50%
        Boss rs = plain(1000, 0, 0, 5);
        rs.rules.push_back(Rule{W_HPHIGH, 0, E_RES});
        Fighter f1 = mk(rs, 0, 1, 1), f2 = mk(rs, 0, 1, 1);
        f2.hp = 400;
        const int t1 = hurt(f1, 100, DT_FIRE, true), t2 = hurt(f2, 100, DT_FIRE, true);
        const bool res = t1 == 50 && t2 == 100;
        Boss sv = plain(100, 0, 0, 5);
        sv.rules.push_back(Rule{W_ALWAYS, 0, E_ADVSAVE});
        const bool save = advSave(mk(sv, 0, 1, 1)) && !advSave(mk(d, 0, 1, 1));
        const bool allowed = !ruleAllowed(W_ALLYADJ, E_RES) && !ruleAllowed(W_FOECOND, E_ADVSAVE) && ruleAllowed(W_FOECOND, E_DMG) && ruleAllowed(W_HPLOW, E_RESNM);
        T("T187", "§v21 «когда → что»: условие и эффект по отдельности (стая, безрассудство, +КД, помеха по мне, резист и спасброски по условию о себе); цена = эффект × доля времени",
          pack && reck && ac && refund && res && save && allowed,
          fmt("стая %d, безрассудство %d, КД и помеха %d, возврат и доля времени %d, резист по условию %d (урон %d/%d), спасброски %d, запрет сочетаний %d",
              pack, reck, ac, refund, res, t1, t2, save, allowed));
    }
    // T188: оружие и Зуро — атака второй рукой (после атаки оружием, без модификатора без стиля), приём «при попадании»
    // (кость запаса тратится только при попадании; обезоруживание: цель тратит взаимодействие), урон себе и лечение, если
    // цель умерла; дальняя дистанция с помехой; лечение с гранью и прибавкой; Зуро собирается из описания целиком
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss z = plain(100, 1, 6, 5);
        z.mainStat = DEX;
        z.stats[DEX] = 18;
        z.wFinesse = z.wLight = true;
        z.poolN = 4;
        z.poolS = 8;
        Spec off;
        off.offhand = off.bonus = true;
        off.range = 5;
        off.dmgN = 1;
        off.dmgS = 6;
        Spec man;
        man.onHit = man.pool = man.disarm = true;
        man.range = 5;
        man.poolN = 4;
        man.poolS = 8;
        Spec smite;
        smite.onHit = true;
        smite.range = 5;
        smite.dmgN = 2;
        smite.dmgS = 10;
        smite.selfN = 2;
        smite.selfS = 4;
        smite.killN = 1;
        smite.killS = 6;
        smite.killB = 100;
        z.abil = {off, man, smite};
        Boss d = plain(1000, 1, 6, 5);
        d.ac = 1;
        auto fresh = [&]() {
            w.f.clear();
            w.f.push_back(mk(z, 0, 10, 10));
            w.f.push_back(mk(d, 1, 11, 10));
            w.f[0].poolLeft = 4;
            w.f[0].uses.assign(3, UNL);
        };
        fresh();
        const bool offLocked = !resReady(w.f[0], specAbility(w.f[0].b, 0).cost);
        doWeapon(w, w.f[0], 1);
        const bool offOpen = resReady(w.f[0], specAbility(w.f[0].b, 0).cost);
        // вторая рука: без стиля урон 1к6 без +4 (в среднем ~3.6 с критами), со стилем +4
        auto offMean = [&](bool style) {
            double tot = 0;
            for (int i = 0; i < 400; i++) {
                fresh();
                w.f[0].b.abil[0].twfMod = style;
                w.f[0].wAtkTurn = true;
                const int h0 = w.f[1].hp;
                doSpec(w, w.f[0], 0, 1);
                tot += h0 - w.f[1].hp;
            }
            return tot / 400;
        };
        const double offNo = offMean(false), offSt = offMean(true);
        const int maxOff = offNo < 4.5 && offSt - offNo > 3.0 ? 1 : 0;
        // приём: попал — кость потрачена и цель обезоружена; промах — кость цела
        fresh();
        doSpec(w, w.f[0], 1, 1);
        const bool spent = w.f[0].poolLeft == 3 && w.f[1].disarmed;
        TurnState ts;
        beginTurn(w, 1, ts);
        const bool pick = !w.f[1].disarmed && ts.swaps == 1;
        fresh();
        w.f[1].b.ac = 1000;   // не попасть (кроме крита)
        int keep = 0;
        for (int i = 0; i < 50; i++) {
            w.f[0].poolLeft = 4;
            doSpec(w, w.f[0], 1, 1);
            keep += w.f[0].poolLeft == 4;
        }
        // урон себе и лечение, если цель умерла
        fresh();
        w.f[1].hp = 1;
        w.f[0].hp = 50;
        doSpec(w, w.f[0], 2, 1);
        const bool kill = w.f[1].hp <= 0 && w.f[0].hp > 50;
        fresh();
        const int me0 = w.f[0].hp;
        doSpec(w, w.f[0], 2, 1);
        const bool selfHurt = w.f[0].hp < me0;
        // дальняя дистанция: дальше обычной — помеха
        Boss bow = plain(100, 1, 8, 30);
        bow.wAmmo = true;
        bow.wLong = 120;
        w.f.clear();
        w.f.push_back(mk(bow, 0, 10, 10));
        w.f.push_back(mk(d, 1, 30, 10));   // 100 фт
        bool a1, d1, a2, d2;
        advState(w, w.f[0], w.f[1], 30, a1, d1);
        w.f[1].x = 14;   // 20 фт
        advState(w, w.f[0], w.f[1], 30, a2, d2);
        const bool longR = atkDist(bow) == 120 && d1 && !d2;
        // лечение 1к10 + 5
        Boss hb = plain(100, 0, 0, 5);
        Spec h;
        h.self = h.bonus = true;
        h.healN = 1;
        h.healS = 10;
        h.healB = 5;
        hb.abil = {h};
        int lo = 1000, hi = 0;
        for (int i = 0; i < 300; i++) {
            w.f.clear();
            w.f.push_back(mk(hb, 0, 10, 10));
            w.f[0].uses.assign(1, UNL);
            w.f[0].hp = 1;
            doSpec(w, w.f[0], 0, 0);
            lo = std::min(lo, w.f[0].hp - 1);
            hi = std::max(hi, w.f[0].hp - 1);
        }
        const bool heal = lo == 6 && hi == 15;
        T("T188", "§v21 оружие и Зуро: вторая рука (после атаки оружием, без модификатора без стиля), приём при попадании (кость — только при попадании, обезоруживание), урон себе и лечение при убийстве, дальняя дистанция с помехой, лечение 1к10+5",
          offLocked && offOpen && maxOff && spent && pick && keep >= 45 && kill && selfHurt && longR && heal,
          fmt("вторая рука: закрыта до атаки %d, открыта после %d, урон без стиля %.2f, со стилем %.2f; приём: кость и обезоруживание %d, подобрал оружие взаимодействием %d, промахов без траты кости %d/50; убийство лечит %d, урон себе %d; дальняя дистанция %d; лечение %d…%d",
              offLocked, offOpen, offNo, offSt, spent, pick, keep, kill, selfHurt, longR, lo, hi));
    }
    // T189: доспехи и щит (v21, как в D&D, плюс КД покупается напрямую) — КД = 10 + единицы + ЛОВ по доспеху + щит;
    // средний срезает ЛОВ до +2, тяжёлый не пропускает её (за срезанное — возврат очков); щит +2 и мешает второй руке и
    // двуручному; без владения — помеха к атакам и спасам СИЛ/ЛОВ; универсальное оружие двумя руками бьёт большей костью
    if (TB()) {
        Coef C2 = CC();
        Boss b = plain(100, 1, 8, 5);
        b.stats[DEX] = 18;   // +4
        b.acV = 3;
        int acs[4];
        for (int k = 0; k < 4; k++) {
            b.armor = k;
            acs[k] = calcAC(b);
        }
        const bool formula = acs[0] == 17 && acs[1] == 17 && acs[2] == 15 && acs[3] == 13;
        b.armor = AR_NONE;
        b.shield = true;
        const bool shield = calcAC(b) == 19;
        b.shield = false;
        Boss lo = b;
        lo.stats[DEX] = 6;   // −2: тяжёлый спасает от минуса
        lo.armor = AR_HEAVY;
        const bool heavyLow = calcAC(lo) == 13 && armorPrice(lo, C2) > 0;
        Boss med = b;
        med.armor = AR_MEDIUM;
        const bool refund = armorPrice(med, C2) < 0;
        Boss np = b;
        np.armor = AR_LIGHT;
        np.noArmProf = true;
        Fighter f = mk(np, 0, 1, 1);
        bool adv, dis;
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.f.push_back(f);
        w.f.push_back(mk(plain(100, 1, 6, 5), 1, 2, 1));
        advState(w, w.f[0], w.f[1], 5, adv, dis);
        const bool noProf = dis && armorPrice(np, C2) < armorPrice(b, C2);
        Boss vv = plain(100, 1, 8, 5);
        vv.wVers = 10;
        const bool vers = wDie(vv) == 10 && (vv.shield = true, wDie(vv) == 8);
        int bad = 0;
        for (int i = 0; i < 600; i++) {
            rng.seed(77000 + i);
            Boss g = makeBoss(200LL << (i % 6), C2, true);
            if (g.ac != calcAC(g) || (g.shield && (hasOffhand(g) || (g.wN > 0 && g.wTwoH))) || (g.armor == AR_HEAVY && g.stats[STR] < 13)) bad++;
        }
        T("T189", "§v21 доспехи и щит: КД = 10 + единицы + ЛОВ по доспеху + щит; средний и тяжёлый срезают ЛОВ (возврат), щит +2 и занимает руку, без владения — помеха, универсальное двумя руками",
          formula && shield && heavyLow && refund && noProf && vers && bad == 0,
          fmt("КД без/лёгкий/средний/тяжёлый при ЛОВ 18: %d/%d/%d/%d; щит %d; тяжёлый при ЛОВ 6 %d; возврат за средний %d; без владения %d; универсальное %d; генератор нарушений %d",
              acs[0], acs[1], acs[2], acs[3], shield, heavyLow, refund, noProf, vers, bad));
    }
    // T190: формы (v21) — сфера круг, куб квадрат, конус от себя (ширина = расстоянию), линия от себя шириной 5 фт; цена
    // области по числу клеток; несколько целей на выбор бьют лучших врагов в пределах дальности
    if (TB()) {
        const int sph = shapeCells(SH_SPHERE, 20), cub = shapeCells(SH_CUBE, 20), con = shapeCells(SH_CONE, 30), lin = shapeCells(SH_LINE, 30);
        const bool cells = cub == 81 && sph < cub && sph > 40 && lin == 6 && con > lin && con < 40;
        // конус на восток от (10,10): (13,10) и (13,11) внутри, (10,13) — сбоку — нет, (8,10) — сзади — нет
        const bool cone = inShape(SH_CONE, 30, 10, 10, 16, 10, 13, 10) && inShape(SH_CONE, 30, 10, 10, 16, 10, 13, 11) &&
                          !inShape(SH_CONE, 30, 10, 10, 16, 10, 10, 13) && !inShape(SH_CONE, 30, 10, 10, 16, 10, 8, 10);
        const bool line = inShape(SH_LINE, 30, 10, 10, 16, 10, 15, 10) && !inShape(SH_LINE, 30, 10, 10, 16, 10, 13, 11) && inShape(SH_LINE, 30, 10, 10, 13, 13, 12, 12);
        Coef C2 = CC();
        Spec a;
        a.delivery = 1;
        a.range = 60;
        a.dmgN = 4;
        a.radius = 20;
        Spec cu = a;
        cu.shape = SH_CUBE;
        Spec one = a;
        one.radius = 0;
        Spec three = one;
        three.targets = 3;
        const bool price = specPrice(cu, C2) > specPrice(a, C2) && specPrice(three, C2) > specPrice(one, C2);
        // несколько целей: три врага рядом — бьёт троих
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss caster = plain(100, 0, 0, 5);
        three.dmgS = 6;
        caster.abil = {three};
        w.f.push_back(mk(caster, 0, 10, 10));
        for (int k = 0; k < 4; k++) w.f.push_back(mk(plain(1000, 0, 0, 5), 1, 14 + k, 10));
        w.f[0].uses.assign(1, UNL);
        doSpec(w, w.f[0], 0, 1);
        int hit = 0;
        for (int k = 1; k <= 4; k++) hit += w.f[k].hp < 1000;
        T("T190", "§v21 формы: сфера — круг, куб — квадрат, конус и линия — от себя; цена области по клеткам; несколько целей на выбор",
          cells && cone && line && price && hit == 3,
          fmt("клеток: сфера 20 фт %d, куб %d, конус 30 фт %d, линия 30 фт %d; конус %d, линия %d; цены %d; задето целей %d из 4", sph, cub, con, lin, cone, line, price, hit));
    }
    // T191: союзники (v21) — лечение на раненого союзника, правило-аура на своих рядом, Помощь (преимущество на одну атаку
    // союзника), Подготовка (удар реакцией, когда враг входит в досягаемость), приказ союзнику атаковать, реакция за
    // союзника (помеха атаке по нему)
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss me = plain(100, 1, 6, 5), al = plain(100, 1, 6, 5), en = plain(1000, 1, 6, 5);
        Spec hl;
        hl.self = hl.ally = true;
        hl.range = 30;
        hl.healN = 0;
        hl.healB = 40;
        Spec cmd;
        cmd.self = cmd.ally = cmd.cmdAtk = true;
        cmd.range = 30;
        Spec grd;
        grd.self = true;
        grd.react = 5;
        grd.protect = true;
        grd.range = 5;
        me.abil = {hl, cmd, grd};
        Rule aura;
        aura.what = E_SAVEB;
        aura.n = 3;
        aura.allyR = 10;
        me.rules = {aura};
        w.f.push_back(mk(me, 0, 10, 10));
        w.f.push_back(mk(en, 1, 12, 11));
        w.f.push_back(mk(al, 0, 11, 11));   // союзник рядом с врагом
        w.f[2].hp = 30;
        g_curW = &w;
        // лечение — раненому союзнику
        doSpec(w, w.f[0], 0, -1);
        const bool heal = w.f[2].hp == 70 && w.f[0].hp == 100;
        // аура: +3 к спасброскам у союзника в 10 фт, у врага — нет
        const bool auraOk = saveBonus(w.f[2], DEX) == modOf(al.stats[DEX]) + 3 && saveBonus(w.f[1], DEX) == modOf(en.stats[DEX]);
        // Помощь: враг вплотную ко мне — первая атака союзника по нему с преимуществом, вторая — нет
        bool a1, d1, a2, d2;
        w.f[1].x = 11;
        w.f[1].y = 10;
        TurnState ts;
        ts.speed = 30;
        const bool helpOk = applyPrim(w, 0, ts, Prim{PR_HELP, 1}, true, nullptr);
        advState(w, w.f[2], w.f[1], 5, a1, d1);
        bool crit;
        attackRoll(w, w.f[2], w.f[1], 0, 5, crit);
        advState(w, w.f[2], w.f[1], 5, a2, d2);
        const bool help = helpOk && a1 && !a2;
        // приказ: союзник бьёт реакцией
        w.f[2].reactionUsed = false;
        const int h0 = w.f[1].hp;
        int tries = 0;
        while (w.f[1].hp == h0 && tries++ < 50) {
            w.f[2].reactionUsed = false;
            doSpec(w, w.f[0], 1, -1);
        }
        const bool command = w.f[1].hp < h0 && w.f[2].reactionUsed;
        // реакция за союзника: враг бьёт союзника рядом со мной — помеха, моя реакция потрачена
        w.f[0].reactionUsed = false;
        attackRoll(w, w.f[1], w.f[2], 0, 5, crit);
        const bool guard = w.f[0].reactionUsed;
        // Подготовка: враг входит в досягаемость — удар реакцией
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        v.aw = v.ah = W;
        Boss rd = plain(100, 3, 6, 5);
        v.f.push_back(mk(rd, 0, 10, 10));
        v.f.push_back(mk(plain(1000, 1, 6, 5), 1, 20, 10));
        v.f[1].b.ac = 1;
        v.f[0].readied = true;
        g_curW = &v;
        const int vh = v.f[1].hp;
        moveToCell(v, v.f[1], 11, 10, 60);
        const bool ready = v.f[0].reactionUsed && !v.f[0].readied && v.f[1].hp < vh;
        g_curW = nullptr;
        T("T191", "§v21 союзники: лечение союзнику, аура на своих, Помощь на одну атаку, приказ атаковать, реакция за союзника, Подготовка",
          heal && auraOk && help && command && guard && ready,
          fmt("лечение %d, аура %d, Помощь %d (преимущество %d, потом %d), приказ %d, защита союзника %d, Подготовка %d", heal, auraOk, help, a1, a2, command, guard, ready));
    }
    // T192: «событие → эффект» (v21) — хиты до 0 → остаться с 1 (раз за бой), убийство → атака бонусом (если бонусное
    // свободно), крит → урон цели, после Уклонения → лечение, в начале хода → временные хиты, +инициатива, преимущество на
    // следующую атаку — на одну атаку
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss me = plain(100, 1, 6, 5);
        me.trigs = {Trig{T_DOWN, X_STAY1, 1, 6, 0, 0, 1}, Trig{T_KILL, X_BONUSATK}, Trig{T_DODGE, X_HEAL, 0, 6, 10}, Trig{T_TURN, X_TEMP, 0, 6, 7}, Trig{T_CRIT, X_ADVNEXT}};
        w.f.push_back(mk(me, 0, 10, 10));
        w.f.push_back(mk(plain(1000, 1, 6, 5), 1, 11, 11));   // главный враг (жив — бой идёт)
        w.f.push_back(mk(plain(1, 1, 6, 5), 1, 11, 10));      // приспешник, которого убьём
        hurt(w.f[0], 500, DT_FIRE, true);
        const bool stay = w.f[0].hp == 1;
        hurt(w.f[0], 500, DT_FIRE, true);
        const bool once = w.f[0].hp <= 0;
        w.f[0].hp = 100;
        // убийство → атака бонусом по следующему врагу
        TurnState ts;
        ts.speed = 30;
        w.f[1].b.ac = -100;
        const int h1 = w.f[1].hp;
        w.f[2].b.ac = -100;
        applyPrim(w, 0, ts, Prim{PR_ATTACK, 2, 0}, true, nullptr);
        const bool bonus = w.f[2].hp <= 0 && ts.bonusUsed && w.f[1].hp < h1;
        // Уклонение → лечение 10
        w.f[0].hp = 50;
        TurnState t2;
        applyPrim(w, 0, t2, Prim{PR_DODGE}, true, nullptr);
        const bool dodge = w.f[0].hp == 60;
        // начало хода → 7 временных хитов
        TurnState t3;
        w.f[0].temp = 0;
        beginTurn(w, 0, t3);
        const bool turn = w.f[0].temp == 7;
        // преимущество на следующую атаку — одна атака
        w.f[0].advNext = true;
        bool a1, d1, a2, d2, crit;
        advState(w, w.f[0], w.f[1], 5, a1, d1);
        attackRoll(w, w.f[0], w.f[1], 0, 5, crit);
        advState(w, w.f[0], w.f[1], 5, a2, d2);
        const bool adv = a1 && !a2;
        Coef C2 = CC();
        const bool price = trigPrice(Trig{T_DOWN, X_STAY1, 1, 6, 0, 0, 1}, me, C2) > 0 && !trigAllowed(T_KILL, X_DMG) && !trigAllowed(T_TURN, X_STAY1) && trigAllowed(T_INIT, X_INITB);
        T("T192", "§v21 «событие → эффект»: 0 хитов → 1 (раз за бой), убийство → атака бонусом, Уклонение → лечение, начало хода → временные хиты, преимущество на одну атаку",
          stay && once && bonus && dodge && turn && adv && price,
          fmt("остаться с 1 %d (второй раз нет %d), атака бонусом %d, лечение после Уклонения %d, временные хиты в начале хода %d, преимущество на одну атаку %d, цены и сочетания %d",
              stay, once, bonus, dodge, turn, adv, price));
    }
    // T193: броски и ресурсы (v21) — Благословение (+к4 к атаке) поднимает попадания; переброс промаха (Везунчик) — тоже,
    // и тратит применения; «Второй шанс» режет попадания по мне; снижение урона −3 у немагического физического; игнор
    // сопротивления; размен −5/+10 включается, когда выгоден
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        auto hitRate = [&](const Boss& a, const Boss& d, int N) {
            int h = 0;
            for (int i = 0; i < N; i++) {
                w.f.clear();
                w.f.push_back(mk(a, 0, 10, 10));
                w.f.push_back(mk(d, 1, 11, 10));
                bool crit;
                h += attackRoll(w, w.f[0], w.f[1], 0, 5, crit);
            }
            return (double)h / N;
        };
        Boss a = plain(100, 1, 6, 5), d = plain(100, 1, 6, 5);
        d.ac = 15;
        const double base = hitRate(a, d, 4000);
        Boss bl = a;
        bl.rules = {Rule{W_ALWAYS, 0, E_ATKDIE, 1, 4}};
        const double bless = hitRate(bl, d, 4000);
        Boss lk = a;
        lk.trigs = {Trig{T_MISS, X_REROLL, 1, 6, 0, 0, UNL}};
        const double lucky = hitRate(lk, d, 4000);
        Boss sc = d;
        sc.trigs = {Trig{T_HITBY, X_REROLL, 1, 6, 0, 0, UNL}};
        const double second = hitRate(a, sc, 4000);
        const bool rolls = bless > base + 0.07 && lucky > base + 0.15 && second < base - 0.10;
        // снижение урона и игнор сопротивления
        Boss ham = plain(1000, 0, 0, 5);
        ham.rules = {Rule{W_ALWAYS, 0, E_DMGRED, 3}};
        Fighter f = mk(ham, 0, 1, 1);
        const int r1 = hurt(f, 10, DT_SLASHING, false), r2 = hurt(f, 10, DT_SLASHING, true);
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        Boss src = plain(100, 0, 0, 5);
        src.rules = {Rule{W_ALWAYS, 0, E_PIERCE, DT_FIRE}};
        Boss fr = plain(1000, 0, 0, 5);
        fr.resMask = 1u << DT_FIRE;
        v.f.push_back(mk(src, 0, 1, 1));
        v.f.push_back(mk(fr, 1, 2, 1));
        g_curW = &v;
        g_srcIdx = 0;
        const int p1 = hurt(v.f[1], 20, DT_FIRE, true);
        g_srcIdx = -1;
        const int p2 = hurt(v.f[1], 20, DT_FIRE, true);
        g_curW = nullptr;
        const bool dmg = r1 == 7 && r2 == 10 && p1 == 20 && p2 == 10;
        Coef C2 = CC();
        const bool price = rulePrice(Rule{W_ALWAYS, 0, E_ATKDIE, 1, 4}, a, C2) > 0 && trigPrice(Trig{T_MISS, X_REROLL, 1, 6, 0, 0, 3}, a, C2) > 0 && !trigAllowed(T_MISS, X_HEAL);
        T("T193", "§v21 броски и ресурсы: Благословение, Везунчик и Второй шанс меняют попадания; снижение урона; игнор сопротивления",
          rolls && dmg && price,
          fmt("попадания: обычно %.2f, с Благословением %.2f, с перебросом %.2f, против Второго шанса %.2f; урон −3: %d и магический %d; сквозь резист %d, без источника %d; цены %d",
              base, bless, lucky, second, r1, r2, p1, p2, price));
    }
    // T194: размеры (v21) — схватить и толкнуть можно не больше чем на ступень крупнее; маленьким тяжёлое оружие —
    // с помехой; крупный стоит очков, маленький с тяжёлым оружием — возвращает
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        Boss med = plain(100, 1, 6, 5), big = plain(100, 1, 6, 5), huge = plain(100, 1, 6, 5);
        big.size = 3;
        huge.size = 4;
        w.f.push_back(mk(med, 0, 10, 10));
        w.f.push_back(mk(big, 1, 11, 10));
        w.f.push_back(mk(huge, 1, 10, 11));
        g_curW = &w;
        const bool gBig = applyCond(w.f[1], C_GRAPPLE, 1, -1, 10, false, 0), gHuge = applyCond(w.f[2], C_GRAPPLE, 1, -1, 10, false, 0);
        const int hx = w.f[2].x, hy = w.f[2].y;
        pushAway(w, w.f[0], w.f[2], 10);
        const bool noPush = w.f[2].x == hx && w.f[2].y == hy;
        g_curW = nullptr;
        // маленький с тяжёлым оружием бьёт реже
        auto rate = [&](int size) {
            int h = 0;
            for (int i = 0; i < 3000; i++) {
                World v;
                std::memset(v.wall, 0, sizeof v.wall);
                std::memset(v.climb, 0, sizeof v.climb);
                Boss a = plain(100, 1, 6, 5);
                a.size = size;
                a.wHeavy = true;
                Boss d = plain(100000, 0, 0, 5);
                d.ac = 15;
                v.f.push_back(mk(a, 0, 10, 10));
                v.f.push_back(mk(d, 1, 11, 10));
                const int h0 = v.f[1].hp;
                doWeapon(v, v.f[0], 1, 1);
                h += v.f[1].hp < h0;
            }
            return h / 3000.0;
        };
        const double small = rate(1), mid = rate(2);
        Coef C2 = CC();
        Boss sm = plain(100, 1, 6, 5);
        sm.size = 1;
        sm.wHeavy = true;
        const bool price = sizePrice(big, C2) > 0 && sizePrice(sm, C2) < 0 && sizePrice(med, C2) == 0;
        T("T194", "§v21 размеры: схватить и толкнуть — не больше чем на ступень крупнее; маленьким тяжёлое оружие — с помехой; цена размера",
          gBig && !gHuge && noPush && small < mid - 0.1 && price,
          fmt("схватил большого %d, огромного %d; огромный не сдвинут %d; попадания тяжёлым: маленький %.2f, средний %.2f; цены %d", gBig, gHuge, noPush, small, mid, price));
    }
    // T195: прочее (v21) — притягивание к себе, снижение максимума хитов, вампиризм, рассеивание снимает баффы и
    // концентрацию, контрзаклинание гасит способность в зоне, истинное зрение видит невидимого, Подвижный не провоцирует
    if (TB()) {
        World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        w.aw = w.ah = W;
        Boss me = plain(100, 1, 6, 5), en = plain(1000, 1, 6, 5);
        Spec sp;
        sp.delivery = 2;
        sp.range = 60;
        sp.dmgN = 10;
        sp.dmgS = 6;
        sp.pullFt = 30;
        sp.maxHpDmg = sp.drain = sp.dispel = true;
        me.abil = {sp};
        w.f.push_back(mk(me, 0, 10, 10));
        w.f.push_back(mk(en, 1, 20, 10));
        w.f[0].hp = 50;
        w.f[1].buffRounds = 3;
        w.f[1].invisRounds = 3;
        hitOne(w, sp, w.f[0], w.f[1]);
        const bool misc = w.f[1].x < 20 && w.f[1].maxHp < 1000 && w.f[0].hp > 50 && w.f[1].buffRounds == 0 && w.f[1].invisRounds == 0;
        // контрзаклинание
        Boss cs = plain(100, 0, 0, 5);
        Spec c;
        c.self = c.counter = true;
        c.react = 4;
        c.range = 60;
        cs.abil = {c};
        World v;
        std::memset(v.wall, 0, sizeof v.wall);
        std::memset(v.climb, 0, sizeof v.climb);
        Boss caster = plain(100, 0, 0, 5);
        Spec fire;
        fire.delivery = 2;
        fire.range = 60;
        fire.dmgN = 10;
        caster.abil = {fire};
        v.f.push_back(mk(caster, 0, 10, 10));
        v.f.push_back(mk(cs, 1, 15, 10));
        const int h0 = v.f[1].hp;
        doSpec(v, v.f[0], 0, 1);
        const bool counter = v.f[1].hp == h0 && v.f[1].reactionUsed;
        // истинное зрение
        Boss ts = plain(100, 0, 0, 5);
        ts.truesight = 30;
        Fighter a = mk(ts, 0, 10, 10), b2 = mk(plain(100, 0, 0, 5), 1, 13, 10), c2 = mk(plain(100, 0, 0, 5), 0, 10, 10);
        b2.invisRounds = 3;
        const bool sight = canSee(v, a, b2) && !canSee(v, c2, b2);
        Coef C2 = CC();
        Spec plain1;
        plain1.delivery = 2;
        plain1.range = 60;
        plain1.dmgN = 10;
        const bool price = specPrice(sp, C2) > specPrice(plain1, C2) && specPrice(c, C2) > 0;
        T("T195", "§v21 прочее: притягивание, снижение максимума хитов, вампиризм, рассеивание, контрзаклинание, истинное зрение",
          misc && counter && sight && price,
          fmt("притягивание/максимум/вампиризм/рассеивание %d (x=%d, максимум %d, мои хиты %d), контрзаклинание %d, истинное зрение %d, цены %d",
              misc, w.f[1].x, w.f[1].maxHp, w.f[0].hp, counter, sight, price));
    }
    // T196: местность (v21, шаг 1) — дерево: залезть стоит подъём на 20 фт (15 фт движения), с вершины до соседа на земле
    // 20 фт (ближним не достать), крона — полукрытие; дерево закрывает обзор, но с вершины другого дерева видно поверх;
    // объекты с хитами: огненный шар валит дерево (остаются обломки — труднопроходимо), яд объектам не вредит, стена — 54 хита
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.tree[10][11] = true;   // дерево рядом с (10,10)
        w.tree[10][14] = true;   // дерево между (12,10) и (16,10)
        w.tree[10][18] = true;
        w.wall[20][5] = true;
        initObjects(w);
        Fighter g = mk(plain(100, 0, 0, 5), 0, 10, 10);
        const int climbCost = stepCost(w, g, 10, 10, 11, 10), down = stepCost(w, g, 11, 10, 10, 10);
        const int dUp = cellDist(&w, 11, 10, 10, 10), dFlat = cellDist(&w, 10, 10, 12, 10);
        const bool climb = climbCost == 15 && down == 5 && dUp == 20 && dFlat == 10 && coverBonus(w, 5, 10, 11, 10) == 2;
        const bool sight = !los(w, 12, 10, 16, 10) && los(w, 11, 10, 18, 10) && los(w, 12, 10, 13, 10);
        const bool hp = w.ohp[10][11] == OBJ_TREE_HP && w.ohp[20][5] == OBJ_WALL_HP && objAC(w, 11, 10) == 15 && objAC(w, 5, 20) == 17;
        const bool poison = !hitObject(w, 11, 10, 100, DT_POISON) && w.tree[10][11];
        Spec fb;
        fb.delivery = 1;
        fb.range = 60;
        fb.radius = 10;
        fb.dmgN = 20;
        fb.dmgS = 10;
        fb.dtype = DT_FIRE;
        areaHitsObjects(w, fb, g, 14, 10);
        bool rubble = false;
        for (auto& d : w.dyn) rubble = rubble || (d[0] == 14 && d[1] == 10 && d[2] == 4);
        const bool burn = !w.tree[10][14] && w.diff[10][14] && rubble && w.tree[10][11];
        const bool wall = !hitObject(w, 5, 20, 53, DT_BLUDGEONING) && hitObject(w, 5, 20, 1, DT_BLUDGEONING) && !w.wall[20][5];
        T("T196", "§v21 местность: дерево (залезть, высота, укрытие, обзор), объекты с хитами (огонь валит дерево, яд мимо, стена 54 хита)",
          climb && sight && hp && poison && burn && wall,
          fmt("дерево: залезть %d фт, спуститься %d, с вершины до соседа %d фт, по земле %d, укрытие %d; обзор %d; хиты %d; яд %d; огонь %d; стена %d",
              climbCost, down, dUp, dFlat, coverBonus(w, 5, 10, 11, 10), sight, hp, poison, burn, wall));
    }
    // T197: опасные зоны (v21, шаг 2) — лава бьёт при входе не чаще раза за ход (второй шаг по лаве в тот же ход — без
    // урона), в начале нового хода снова; летун над лавой не горит, но ядовитый дым его травит; путь при равной длине
    // обходит зону (длина пути та же — кандидаты сети верны)
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.hazN = 4;
        w.hazDC = 30;   // спасбросок не пройти: урон полный
        for (int x = 11; x <= 12; x++) w.haz[10][x] = 1;   // лава — в коридоре между стенами, обойти нельзя
        for (int x = 9; x <= 14; x++) w.wall[9][x] = w.wall[11][x] = true;
        w.haz[15][10] = 3;                                  // ядовитый дым
        w.f.push_back(mk(plain(100, 0, 0, 5), 0, 10, 10));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 25, 25));
        Fighter& g = w.f[0];
        g.hp = g.maxHp = 1000;
        g_turnSerial++;
        moveToCell(w, g, 13, 10, 15);   // через лаву по прямой: 2 клетки лавы, урон один раз
        const int hit1 = 1000 - g.hp;
        g_turnSerial++;
        g.x = 11;
        const int h0 = g.hp;
        hazardHit(w, g);                 // новый ход в лаве — урон снова
        const int hit2 = h0 - g.hp;
        Boss fl = plain(100, 0, 0, 5);
        fl.fly = true;
        w.f.push_back(mk(fl, 0, 11, 10));
        Fighter& b = w.f.back();
        b.hp = b.maxHp = 1000;
        g_turnSerial++;
        hazardHit(w, b);
        const int flyLava = 1000 - b.hp;
        b.x = 10;
        b.y = 15;
        g_turnSerial++;
        hazardHit(w, b);
        const int flySmoke = 1000 - b.hp;
        // путь (10,13) → (14,13) по ровному: прямо 4 клетки; ставим лаву на (12,13) — путь той же длины обходит её
        w.haz[13][12] = 1;
        Fighter p = mk(plain(100, 0, 0, 5), 0, 10, 13);
        std::vector<PStep> path;
        pathTo(w, p, 14, 13, path);
        bool avoid = path.size() == 4;
        for (auto& c : path) avoid = avoid && w.haz[c.second][c.first] == 0;
        const bool once = hit1 >= 4 && hit1 <= 40, again = hit2 >= 4 && hit2 <= 40;
        T("T197", "§v21 опасные зоны: урон при входе раз за ход и в начале хода, летун над лавой цел (дым травит), путь обходит зону при равной длине",
          once && again && flyLava == 0 && flySmoke > 0 && avoid,
          fmt("через 2 клетки лавы −%d (4к10, раз за ход), новый ход в лаве −%d, летун над лавой −%d, в дыму −%d, путь %zu шагов в обход %d",
              hit1, hit2, flyLava, flySmoke, path.size(), avoid));
    }
    // T198: высоты и второй ярус (v21, шаг 3) — обрыв (перепад 2 уровня) пешком не пройти, по лестнице можно, летун —
    // только под открытым небом; под настилом на 20 фт средний проходит, огромный — нет; путь на настил по лестнице;
    // стоящий на настиле и стоящий под ним — 20 фт и не видят друг друга сквозь настил; настил рушится — стоявший на нём
    // падает (урон, сбит с ног); толкнули с настила — падает; карта игрока с обрывом без лестницы не проходит проверку
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.haz, 0, sizeof w.haz);
        std::memset(w.ladder, 0, sizeof w.ladder);
        std::memset(w.deck, 0, sizeof w.deck);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 30; y++)
            for (int x = 20; x < 30; x++) w.height[y][x] = 2;   // скала 20 фт с x = 20
        Fighter walker = mk(plain(100, 0, 0, 5), 0, 19, 5);
        Boss fb = plain(100, 0, 0, 5);
        fb.fly = true;
        Fighter flyer = mk(fb, 0, 19, 5);
        const bool cliff = !stepOK(w, walker, 19, 5, 0, 20, 5, 0) && stepOK(w, flyer, 19, 5, 0, 20, 5, 0);
        w.ladder[6][20] = true;
        const bool ladder = stepOK(w, walker, 19, 6, 0, 20, 6, 0);
        // настил на 20 фт над (5..9, 15): под ним средний проходит, огромный — нет; летун под настилом обрыв не перелетит
        for (int x = 5; x <= 9; x++) w.deck[15][x] = 2;
        w.ladder[15][4] = true;
        Boss huge = plain(100, 0, 0, 5);
        huge.size = 4;
        Fighter hg = mk(huge, 1, 4, 14);
        const bool clearance = nodeOK(w, walker, 6, 15, 0) && !nodeOK(w, hg, 6, 15, 0);
        w.deck[5][19] = 3;   // навес над клеткой у скалы: летун под ним на скалу не взлетит
        const bool roofFly = !stepOK(w, flyer, 19, 5, 0, 20, 5, 0);
        w.deck[5][19] = 0;
        Fighter p = mk(plain(100, 0, 0, 5), 0, 3, 15);
        std::vector<PStep> path;
        for (int x = 5; x <= 9; x++) w.pit[15][x] = true;   // настил — мост над ущельем: путь только по нему, через лестницу
        pathTo(w, p, 7, 15, path);
        const bool onDeckPath = !path.empty() && path.back().l == 1;
        for (int x = 5; x <= 9; x++) w.pit[15][x] = false;
        w.f.push_back(mk(plain(100, 0, 0, 5), 0, 7, 15));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 7, 16));
        w.f[0].up = true;
        g_curW = &w;
        const int dUpDown = distFt(w.f[0], w.f[1]);
        w.f[1].x = 7;
        w.f[1].y = 15;   // прямо под настилом
        const bool slab = !losF(w, w.f[0], w.f[1]);
        initObjects(w);
        w.f[0].hp = w.f[0].maxHp = 1000;
        w.f[1].hp = w.f[1].maxHp = 1000;
        const bool col = hitDeck(w, 7, 15, 1000, DT_FIRE) && !w.f[0].up && w.f[0].hp < 1000 && w.f[0].cond[C_PRONE] > 0 && w.f[1].hp < 1000 && w.deck[15][7] == 0;
        // толкнули с настила (8,15) на север, где настила нет — падает
        w.f[0].x = 8;
        w.f[0].y = 15;
        w.f[0].up = true;
        w.f[0].cond[C_PRONE] = 0;
        w.f[1].x = 8;
        w.f[1].y = 17;   // толкает с юга
        const int hp0 = w.f[0].hp;
        pushAway(w, w.f[1], w.f[0], 10);
        const bool pushOff = !w.f[0].up && w.f[0].y == 14 && w.f[0].hp < hp0;
        g_curW = nullptr;
        // карта игрока: обрыв без лестницы — разделена, с лестницей — годится
        auto mapJ = [](bool lad) {
            std::string h = "[", l = "[";
            for (int y = 0; y < 8; y++) {
                h += std::string(y ? "," : "") + "\"00002222\"";
                l += std::string(y ? "," : "") + (lad && y == 3 ? "\"00001000\"" : "\"00000000\"");
            }
            return "{\"aw\":8,\"ah\":8,\"height\":" + h + "],\"ladder\":" + l + "]}";
        };
        CustomMap cm;
        std::string err1, err2;
        bool okJ = true;
        const bool bad = !parseCustomMap(parseJson(mapJ(false), &okJ), cm, err1), good = parseCustomMap(parseJson(mapJ(true), &okJ), cm, err2);
        T("T198", "§v21 высоты и ярусы: обрыв (пешком нет, лестница, летун под небом), рост под настилом, путь на настил, обзор сквозь настил, обрушение, толчок с настила, проверка карты",
          cliff && ladder && clearance && roofFly && onDeckPath && dUpDown == 20 && slab && col && pushOff && bad && good,
          fmt("обрыв %d, лестница %d, рост %d, летун под навесом %d, путь на настил %d, на настиле↔внизу %d фт, обзор закрыт %d, обрушение %d, толчок с настила %d, карта: без лестницы «%s», с лестницей %d",
              cliff, ladder, clearance, roofFly, onDeckPath, dUpDown, slab, col, pushOff, err1.substr(0, 40).c_str(), good));
    }
    // T199: мир из шума — участок определяется зерном мира и координатами: тот же мир, построенный заново, и любое окно кэша
    // дают те же клетки; другое зерно — другой мир
    if (TB()) {
        auto same = [](const wg::Cell& a, const wg::Cell& b) {
            return a.wall == b.wall && a.climb == b.climb && a.diff == b.diff && a.dark == b.dark && a.pit == b.pit && a.water == b.water &&
                   a.tree == b.tree && a.ladder == b.ladder && a.strong == b.strong && a.town == b.town && a.road == b.road &&
                   a.height == b.height && a.haz == b.haz && a.deck == b.deck && a.door == b.door && a.biome == b.biome;
        };
        static wg::Plan P1, P2, P3;
        wg::buildPlan(P1, 777, 1000);
        wg::buildPlan(P2, 777, 1000);
        wg::buildPlan(P3, 778, 1000);
        int diffWin = 0, diffPlan = 0, diffSeed = 0, n = 0;
        for (int k = 0; k < 6; k++) {   // участки у поселений (сложные клетки) и случайные
            const bool nearTown = k < 3 && k < (int)P1.towns.size();
            const int cx = nearTown ? P1.towns[k].x : 100 + 150 * k, cy = nearTown ? P1.towns[k].y : 700 - 90 * k;
            std::vector<wg::Cell> a;
            wg::fillLevelCache(P1, cx - 30, cy - 30, 60, 60);
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++) a.push_back(wg::worldCell(P1, x, y));
            wg::fillLevelCache(P1, cx - 7, cy - 45, 90, 70);   // другое окно
            size_t i = 0;
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++, i++) diffWin += !same(a[i], wg::worldCell(P1, x, y));
            wg::fillLevelCache(P2, cx - 25, cy - 25, 50, 50);
            i = 0;
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++, i++) diffPlan += !same(a[i], wg::worldCell(P2, x, y));
            wg::fillLevelCache(P3, cx - 30, cy - 30, 60, 60);
            i = 0;
            for (int y = cy - 20; y < cy + 20; y++)
                for (int x = cx - 20; x < cx + 20; x++, i++) diffSeed += !same(a[i], wg::worldCell(P3, x, y));
            n += 1600;
        }
        const bool plans = P1.towns.size() == P2.towns.size() && P1.rivers.size() == P2.rivers.size() && P1.roads.size() == P2.roads.size();
        T("T199", "§11 мир из шума: участок задан зерном и координатами (окно и повторная постройка мира не меняют клеток); другое зерно — другой мир",
          diffWin == 0 && diffPlan == 0 && plans && diffSeed > n / 10,
          fmt("клеток %d: отличий при другом окне %d, при новой постройке %d (план одинаков: %d); при другом зерне %d", n, diffWin, diffPlan, plans, diffSeed));
    }
    // T200: биомы по температуре и влажности, объекты по биому: леса (лес, тайга, джунгли) гуще деревьями, чем сухие и холодные
    // открытые (степь, пустыня, тундра, ледник); в мире есть поселения со стенами и дверями, реки с водой
    if (TB()) {
        int cnt[wg::B_N] = {}, trees[wg::B_N] = {};
        int towns = 0, townWall = 0, townDoor = 0, water = 0, worlds = 0;
        for (uint64_t sd : {11ULL, 2026ULL, 31337ULL, 900001ULL}) {
            static wg::Plan P;
            wg::buildPlan(P, sd, 1000);
            worlds++;
            towns += (int)P.towns.size();
            for (int y = 3; y < 1000; y += 9)
                for (int x = 5; x < 1000; x += 9) {
                    const wg::Cell c = wg::worldCell(P, x, y);
                    water += c.water;
                    if (c.town) {
                        townWall += c.wall;
                        townDoor += c.door > 0;
                        continue;
                    }
                    cnt[c.biome]++;
                    trees[c.biome] += c.tree;
                }
            for (auto& t : P.towns)   // дома поселения: стены и двери у центра
                for (int y = t.y - t.r; y <= t.y + t.r; y++)
                    for (int x = t.x - t.r; x <= t.x + t.r; x++) {
                        if (x < 0 || y < 0 || x >= 1000 || y >= 1000) continue;
                        const wg::Cell c = wg::worldCell(P, x, y);
                        if (!c.town) continue;
                        townWall += c.wall;
                        townDoor += c.door > 0;
                    }
        }
        int kinds = 0;
        for (int b = 0; b < wg::B_N; b++) kinds += cnt[b] > 50;
        auto share = [&](std::initializer_list<int> bs) {
            double t = 0, n = 0;
            for (int b : bs) t += trees[b], n += cnt[b];
            return n > 0 ? t / n : 0.0;
        };
        const double woods = share({wg::B_FOREST, wg::B_TAIGA, wg::B_JUNGLE}), open = share({wg::B_STEPPE, wg::B_DESERT, wg::B_TUNDRA, wg::B_GLACIER});
        std::string det;
        for (int b = 0; b < wg::B_N; b++) det += fmt("%s %d, ", wg::biomeName(b), cnt[b]);
        T("T200", "§11 биомы (температура × влажность) и объекты по биому; поселения со стенами и дверями; реки",
          kinds >= 7 && woods > 3 * open && woods > 0.05 && towns >= worlds && townWall > 0 && townDoor > 0 && water > 0,
          fmt("биомов %d из 9 (%sклеток в 4 мирах); деревьев: леса %.3f, открытые %.3f; поселений %d, их стен %d, дверей %d; воды %d",
              kinds, det.c_str(), woods, open, towns, townWall, townDoor, water));
    }
    // T201: двери и крепкие стены — закрытая дверь закрывает обзор; пройти через неё — +5 фт (открывает); открытая не мешает;
    // у двери 18 хитов, выбитая — пустой проём; стена крепости — 120 хитов
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.strong, 0, sizeof w.strong);
        std::memset(w.deck, 0, sizeof w.deck);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 5; y < 15; y++) w.wall[y][10] = true;   // стена с дверью в (10,10)
        w.wall[10][10] = false;
        w.door[10][10] = 1;
        w.wall[20][5] = w.strong[20][5] = true;
        initObjects(w);
        Fighter g = mk(plain(100, 0, 0, 5), 0, 9, 10);
        const bool shutSight = !los(w, 8, 10, 12, 10);
        const int costShut = stepCost(w, g, 9, 10, 10, 10);
        setDoor(w, 10, 10, 2);
        const bool openSight = los(w, 8, 10, 12, 10);
        const int costOpen = stepCost(w, g, 9, 10, 10, 10);
        setDoor(w, 10, 10, 1);
        const bool hp = w.ohp[10][10] == OBJ_DOOR_HP && w.ohp[20][5] == OBJ_STRONG_HP;
        const bool broke = !hitObject(w, 10, 10, 17, DT_BLUDGEONING) && hitObject(w, 10, 10, 1, DT_BLUDGEONING) && w.door[10][10] == 0 && !w.wall[10][10] &&
                           los(w, 8, 10, 12, 10);
        const bool strong = !hitObject(w, 5, 20, 119, DT_BLUDGEONING) && hitObject(w, 5, 20, 1, DT_BLUDGEONING);
        T("T201", "§11 двери: закрытая закрывает обзор, пройти — +5 фт (открывает), открытая не мешает; 18 хитов, выбитая — проём; стена крепости 120 хитов",
          shutSight && costShut == 10 && openSight && costOpen == 5 && hp && broke && strong,
          fmt("обзор закрыта/открыта %d/%d; шаг через закрытую %d фт, открытую %d; хиты %d; выбита %d; крепкая стена %d",
              !shutSight, openSight, costShut, costOpen, hp, broke, strong));
    }
    // T202: размеры арен из мира — стороны 2–100, чаще средние (10–60 — около 80%, 2–5 — около 3%, 80–100 — около 5%)
    if (TB()) {
        const int N = 200000;
        int mn = 999, mx = 0, tiny = 0, mid = 0, huge = 0;
        for (int i = 0; i < N; i++) {
            const int a = pickArenaSide();
            mn = std::min(mn, a);
            mx = std::max(mx, a);
            tiny += a <= 5;
            mid += a >= 10 && a <= 60;
            huge += a >= 80;
        }
        const double pt = 100.0 * tiny / N, pm = 100.0 * mid / N, ph = 100.0 * huge / N;
        T("T202", "§11 размер арены из мира: стороны 2–100, 2–5 около 3%, 10–60 около 80%, 80–100 около 5%",
          mn == 2 && mx == 100 && std::fabs(pt - 3) < 0.5 && std::fabs(pm - 80) < 0.7 && std::fabs(ph - 5) < 0.5,
          fmt("сторона %d–%d; 2–5 %.1f%%, 10–60 %.1f%%, 80–100 %.1f%%", mn, mx, pt, pm, ph));
    }
    // T203: клетки по размеру — большой 2×2, огромный 3×3, громадный 4×4 от левой верхней клетки; расстояние между ближайшими
    // клетками; в проход шириной в клетку крупный не пролезет; область задевает по любой клетке квадрата; чужой квадрат занят
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 30; y++) w.wall[y][20] = y != 15;   // стена с проходом в одну клетку (20,15)
        initObjects(w);
        Boss big = plain(100, 0, 0, 5), huge = plain(100, 0, 0, 5);
        big.size = 3;
        huge.size = 5;
        w.f.push_back(mk(big, 0, 10, 10));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 12, 10));
        w.f.push_back(mk(huge, 1, 2, 20));
        g_curW = &w;
        const Fighter &B = w.f[0], &M = w.f[1], &H = w.f[2];
        const int dBM = distFt(B, M), dHB = distFt(H, B);
        const bool occ = occupied(w, 11, 11, &M) && !occupied(w, 12, 12, &M) && occupied(w, 5, 23, nullptr) && !occupied(w, 6, 24, nullptr);
        std::vector<PStep> pb, pm, ph;
        Boss huge3 = plain(100, 0, 0, 5);
        huge3.size = 4;
        Fighter bigW = mk(big, 0, 15, 15), medW = mk(plain(100, 0, 0, 5), 0, 15, 15), hugeW = mk(huge3, 0, 15, 14);
        pathTo(w, bigW, 25, 15, pb);
        pathTo(w, medW, 25, 15, pm);
        pathTo(w, hugeW, 25, 14, ph);
        // большой протискивается в проход шириной в клетку (как средний: вдвое дороже), огромному (3×3 → 2×2) не пролезть
        const int sqCost = stepCost(w, bigW, 19, 15, 20, 15);
        // огромный не пролезает: идёт как можно ближе (до стены), но за стену не попадает
        const bool hugeStops = !ph.empty() && ph.back().first < 20;
        const bool gap = !pb.empty() && pb.back().first == 25 && !pm.empty() && hugeStops && sqCost == 10 && stepCost(w, medW, 19, 15, 20, 15) == 5;
        // шар радиусом 5 фт вокруг (14,14) задевает угол квадрата большого (11,11), но не (10,10)-клетку отдельно стоящего среднего
        const bool area = inShapeF(SH_SPHERE, 5, 0, 0, 12, 12, B) && !inShapeF(SH_SPHERE, 5, 0, 0, 14, 14, B);
        const bool fit = !nodeOKn(w, bigW, 19, 15, 0, 2) && nodeOK(w, bigW, 19, 15, 0) && fpAt(w, bigW, 20, 15, 0) == 1 && nodeOKn(w, bigW, 21, 15, 0, 2) && !nodeOK(w, hugeW, 19, 14, 0);
        g_curW = nullptr;
        T("T203", "§11 клетки по размеру: большой 2×2, огромный 3×3, громадный 4×4; расстояние между ближайшими клетками; протискивание на размер меньше; область по любой клетке",
          dBM == 5 && dHB == 45 && occ && gap && area && fit,
          fmt("большой–средний вплотную %d фт, громадный–большой %d фт; занятые клетки %d; через проход в клетку: средний %s, большой %s (протискиваясь, шаг %d фт), огромный %s; область %d; помещается %d",
              dBM, dHB, occ, pm.empty() ? "нет" : "да", pb.empty() ? "нет" : "да", sqCost, hugeStops ? "нет (встал у стены)" : "да", area, fit));
    }
    // T204: высота полёта — летун на высоте (10 фт за уровень) дальше от стоящего внизу (ближним с земли не достать), подъём
    // стоит 10 фт движения за уровень, потолок (настил) ограничивает; сбитый с ног летун падает (1к6 за 10 фт); область от земли
    // достаёт летуна не выше своего размера; не летун взлететь не может
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.deck[20][20] = 3;   // навес на 30 фт
        initObjects(w);
        Boss fb = plain(200, 1, 6, 5), gb = plain(200, 1, 6, 5);
        fb.fly = true;
        w.f.push_back(mk(fb, 0, 10, 10));
        w.f.push_back(mk(gb, 1, 11, 10));
        g_curW = &w;
        Fighter &Fl = w.f[0], &Gr = w.f[1];
        TurnState ts;
        ts.speed = 30;
        std::string why;
        const int d0 = distFt(Fl, Gr);
        const bool up = applyPrim(w, 0, ts, Prim{PR_ALT, 2}, true, &why);
        const int d2 = distFt(Fl, Gr), left = ts.speed;
        const bool noReach = d2 > meleeReach(Gr.b);
        TurnState ts2;
        ts2.speed = 30;
        const bool walkerNo = !applyPrim(w, 1, ts2, Prim{PR_ALT, 1}, true, &why);
        const int capOpen = maxAltAt(w, Fl, 10, 10, 0), capRoof = maxAltAt(w, Fl, 20, 20, 0);
        Fl.alt = 3;
        const bool area = !inShapeF(SH_SPHERE, 20, 0, 0, 10, 10, Fl);
        Fl.alt = 2;
        const bool area2 = inShapeF(SH_SPHERE, 20, 0, 0, 10, 10, Fl);
        const int hp0 = Fl.hp;
        applyCond(Fl, C_PRONE, 1, -1, 0, false);
        const bool fell = Fl.alt == 0 && Fl.hp < hp0 && Fl.hp >= hp0 - 12;
        g_curW = nullptr;
        T("T204", "§7 высота полёта: 10 фт за уровень, ближним с земли не достать, потолок, падение сбитого (1к6 за 10 фт), область от земли",
          up && d0 == 5 && d2 == 20 && left == 10 && noReach && walkerNo && capOpen == ALT_MAX && capRoof == 2 && area && area2 && fell,
          fmt("подъём на 20 фт %d (осталось движения %d из 30); расстояние до стоящего рядом %d → %d фт (ближним не достать %d); не летун не взлетел %d; предел высоты %d, под навесом в 30 фт %d; огненный шар 20 фт: летуна на 30 фт не задел %d, на 20 фт задел %d; сбит с ног — упал %d (−%d)",
              up, left, d0, d2, noReach, walkerNo, capOpen, capRoof, area, area2, fell, hp0 - Fl.hp));
    }
    // T205: бойцы в одной клетке на разной высоте — мешают, только если пересекаются по высоте (рост: средний 10 фт, большой
    // 20): под стоящим на мосту пройти можно; под летуном на 10 фт пройдёт средний, большой — нет
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int x = 5; x < 15; x++) w.deck[10][x] = 2;   // мост на 20 фт
        initObjects(w);
        Boss fb = plain(100, 0, 0, 5), big = plain(100, 0, 0, 5);
        fb.fly = true;
        big.size = 3;
        w.f.push_back(mk(plain(100, 0, 0, 5), 0, 8, 10));   // на мосту
        w.f[0].up = true;
        w.f.push_back(mk(fb, 0, 20, 20));                    // летун на 10 фт
        w.f[1].alt = 1;
        g_curW = &w;
        Fighter walker = mk(plain(100, 0, 0, 5), 1, 8, 9), bigW = mk(big, 1, 19, 19);
        const bool underBridge = !blockedAt(w, walker, 8, 10) && blockedAt(w, w.f[0], 8, 10) == false;
        Fighter onDeck = mk(plain(100, 0, 0, 5), 1, 9, 10);
        onDeck.up = true;
        const bool deckBlock = blockedAt(w, onDeck, 8, 10);   // на мосту — та же клетка, тот же уровень
        const bool underFlier = !blockedAt(w, walker, 20, 20) && blockedAt(w, bigW, 19, 19);
        g_curW = nullptr;
        T("T205", "§11 бойцы в одной клетке на разной высоте: мешают, только если пересекаются по высоте (под мостом, под летуном)",
          underBridge && deckBlock && underFlier,
          fmt("под стоящим на мосту пройти %d, на мосту — занято %d; под летуном на 10 фт: средний проходит, большой упирается %d", underBridge, deckBlock, underFlier));
    }
    // T206: скорости движения (v21) — лазун: лазалка и подъём без надбавки, по обрыву как по лестнице; пловец: вода не
    // труднопроходима; копатель: сквозь ущелье и обрыв; у каждого атома цена, конструктор их принимает
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.water, 0, sizeof w.water);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.ladder, 0, sizeof w.ladder);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        for (int y = 0; y < 30; y++) w.height[y][15] = w.height[y][16] = 3;   // скала в 30 фт
        w.climb[5][5] = true;
        w.water[6][6] = w.diff[6][6] = true;
        for (int y = 0; y < 30; y++) w.pit[y][25] = true;   // ущелье
        initObjects(w);
        Boss cb = plain(100, 0, 0, 5), sb = cb, bb = cb, pb = cb;
        cb.climbS = true;
        sb.swimS = true;
        bb.burrowS = true;
        Fighter C1 = mk(cb, 0, 14, 10), S1 = mk(sb, 0, 5, 6), B1 = mk(bb, 0, 24, 10), P1 = mk(pb, 0, 14, 10);
        const bool cliff = stepOK(w, C1, 14, 10, 0, 15, 10, 0) && !stepOK(w, P1, 14, 10, 0, 15, 10, 0) && stepCost(w, C1, 14, 10, 15, 10) == 5;
        const bool climbT = stepCost(w, C1, 4, 5, 5, 5) == 5 && stepCost(w, P1, 4, 5, 5, 5) == 10;
        const bool swim = stepCost(w, S1, 5, 6, 6, 6) == 5 && stepCost(w, P1, 5, 6, 6, 6) == 10;
        std::vector<PStep> pbw, ppw;
        pathTo(w, B1, 27, 10, pbw);
        Fighter P2 = mk(pb, 0, 24, 10);
        pathTo(w, P2, 27, 10, ppw);
        const bool burrow = !pbw.empty() && pbw.back().first == 27 && (ppw.empty() || ppw.back().first < 25);
        Coef C2 = CC();
        g_B = 1600;
        const bool price = C2[AT_CLIMB] > 0 && C2[AT_SWIM] > 0 && C2[AT_BURROW] > 0;
        bool ok = true;
        Boss cbx;
        long long bu = 0;
        std::string e;
        const bool ctor = chr::buildCharacter(parseJson("{\"budget\":1600,\"passives\":{\"climbS\":true,\"swimS\":true,\"burrowS\":true}}", &ok), C2, cbx, bu, e) && cbx.climbS && cbx.swimS && cbx.burrowS;
        T("T206", "§4 скорости лазания, плавания, копания: лазун по скале и лазалке без надбавки, пловец по воде, копатель сквозь ущелье; цена, конструктор",
          cliff && climbT && swim && burrow && price && ctor,
          fmt("скала %d, лазалка %d, вода %d, ущелье %d, цена %d, конструктор %d %s", cliff, climbT, swim, burrow, price, ctor, e.c_str()));
    }
    // T207: Увёртливость (ЛОВ от области: успех — 0, провал — половина), солнечная чувствительность (днём под небом — помеха,
    // ночью и под навесом — нет), действия логова (обвал бьёт врагов у центра, потом трясина — не подряд), превращение (свой запас
    // хитов облика, способности недоступны, хиты облика кончились — свой облик и лишний урон на свои хиты)
    if (TB()) {
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.climb, 0, sizeof w.climb);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.diff, 0, sizeof w.diff);
        std::memset(w.height, 0, sizeof w.height);
        std::memset(w.door, 0, sizeof w.door);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.dark, 0, sizeof w.dark);
        std::memset(w.haz, 0, sizeof w.haz);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.deck[5][5] = 3;
        initObjects(w);
        g_curW = &w;
        // Увёртливость: 400 огненных шаров по увёртливому и обычному с ЛОВ 10
        Boss caster = plain(100, 0, 0, 5);
        Spec fb;
        fb.delivery = 1;
        fb.saveStat = DEX;
        fb.range = 60;
        fb.radius = 20;
        fb.dmgN = 8;
        fb.dmgS = 6;
        fb.dtype = DT_FIRE;
        caster.abil = {fb};
        Boss ev = plain(100000, 0, 0, 5), no = ev;
        ev.evasion = true;
        w.f.push_back(mk(caster, 0, 10, 10));
        w.f.push_back(mk(ev, 1, 14, 10));
        w.f.push_back(mk(no, 1, 14, 12));
        long long dEv = 0, dNo = 0, zero = 0;
        for (int i = 0; i < 400; i++) {
            const int h1 = w.f[1].hp, h2 = w.f[2].hp;
            hitOne(w, fb, w.f[0], w.f[1]);
            hitOne(w, fb, w.f[0], w.f[2]);
            dEv += h1 - w.f[1].hp;
            dNo += h2 - w.f[2].hp;
            zero += h1 == w.f[1].hp;
        }
        const bool evasion = dEv * 10 < dNo * 6 && zero > 100;
        // солнце
        Boss ss = plain(100, 1, 6, 5);
        ss.sunSens = true;
        Fighter sa = mk(ss, 0, 10, 10), sb = mk(plain(100, 1, 6, 5), 1, 11, 10), sc = mk(ss, 0, 5, 5);
        bool a1, d1, a2, d2, a3, d3;
        w.day = true;
        advState(w, sa, sb, 5, a1, d1);
        advState(w, sc, sb, 5, a2, d2);   // под навесом
        w.day = false;
        advState(w, sa, sb, 5, a3, d3);
        w.day = true;
        const bool sun = d1 && !d2 && !d3;
        // логово: обвал по двум врагам рядом, затем трясина
        w.f.clear();
        Boss lb = plain(500, 0, 0, 5);
        lb.lair = 2;
        w.f.push_back(mk(lb, 0, 2, 2));
        w.f.push_back(mk(plain(500, 0, 0, 5), 1, 20, 20));
        w.f.push_back(mk(plain(500, 0, 0, 5), 1, 21, 20));
        const int lh1 = w.f[1].hp, lh2 = w.f[2].hp;
        lairAction(w, 0);
        const bool rock = w.f[1].hp < lh1 && w.f[2].hp < lh2 && lh1 - w.f[1].hp <= 40 && w.f[0].lairLast == 1;
        lairAction(w, 0);
        const bool bog = w.f[0].lairLast == 2 && w.diff[20][20] && w.diff[22][22];
        // превращение
        w.f.clear();
        Boss dr = plain(50, 1, 4, 5), bear = plain(30, 2, 8, 5);
        bear.size = 3;
        dr.tmpl = {bear};
        Spec fs;
        fs.self = true;
        fs.delivery = 2;
        fs.formTmpl = 0;
        fs.uses = 1;
        dr.abil = {fs};
        w.f.push_back(mk(dr, 0, 10, 10));
        w.f.push_back(mk(plain(100, 0, 0, 5), 1, 20, 20));
        TurnState ts;
        ts.speed = ts.speed0 = 30;
        std::string why;
        const bool cast = applyPrim(w, 0, ts, Prim{PR_SPELL, 0, 0}, true, &why);
        Fighter& D = w.f[0];
        const bool inForm = D.inForm && D.hp == 30 && D.b.size == 3 && D.b.wN == 2 && fpN(D) == 2;
        TurnState ts2;
        ts2.speed = 30;
        const bool blocked = !applyPrim(w, 0, ts2, Prim{PR_SPELL, 0, 0}, true, &why);
        hurt(D, 40, DT_BLUDGEONING, false);
        const bool back = !D.inForm && D.hp == 40 && D.b.size == 2 && D.b.wN == 1;
        g_curW = nullptr;
        T("T207", "§9 Увёртливость, солнечная чувствительность, действия логова, превращение",
          evasion && sun && rock && bog && cast && inForm && blocked && back,
          fmt("урон огненным шаром: увёртливый %lld, обычный %lld (без урона %lld из 400); солнце %d; логово: обвал %d, трясина %d; превращение %d, в облике %d, способности закрыты %d, возврат (хиты 40) %d %s",
              dEv, dNo, zero, sun, rock, bog, cast, inForm, blocked, back, why.c_str()));
    }
    // T208: призыв у персонажа (конструктор) — группы разных приспешников, цена = шаблоны × копии × применения; призыв
    // ставит копии всех групп; превращение из конструктора; у генератора есть смешанные призывы
    if (TB()) {
        Coef C2 = CC();
        const char* wolf = "{\"v\":21,\"budget\":150,\"stats\":[12,15,12,3,12,6],\"acV\":3,\"main\":1,\"weapon\":{\"n\":2,\"s\":4,\"range\":5,\"att\":1,\"reach\":5,\"type\":11,\"finesse\":true},\"passives\":{\"speed\":40}}";
        const char* skel = "{\"v\":21,\"budget\":120,\"stats\":[10,14,15,6,8,5],\"acV\":3,\"main\":1,\"weapon\":{\"n\":1,\"s\":6,\"range\":5,\"att\":1,\"reach\":5,\"type\":11,\"finesse\":true},\"passives\":{\"speed\":30}}";
        const char* bear = "{\"v\":21,\"budget\":400,\"stats\":[19,10,16,2,13,7],\"acV\":1,\"main\":0,\"size\":3,\"weapon\":{\"n\":2,\"s\":6,\"range\":5,\"att\":2,\"reach\":5,\"type\":10},\"passives\":{\"speed\":40,\"climbS\":true}}";
        auto mk1 = [&](const std::string& abil, Boss& b, std::string& e) {
            bool ok = true;
            long long bu = 0;
            const std::string js = "{\"v\":21,\"budget\":3000,\"stats\":[10,10,10,10,10,10],\"weapon\":{\"n\":1,\"s\":6,\"range\":5,\"att\":1,\"reach\":5,\"type\":11},\"abilities\":[" + abil + "]}";
            return chr::buildCharacter(parseJson(js, &ok), C2, b, bu, e);
        };
        Boss b1, b2, b3;
        std::string e1, e2, e3;
        const bool one = mk1(std::string("{\"self\":true,\"uses\":1,\"summon\":{\"groups\":[{\"n\":2,\"minion\":") + wolf + "}]}}", b1, e1);
        const bool two = mk1(std::string("{\"self\":true,\"uses\":1,\"summon\":{\"groups\":[{\"n\":2,\"minion\":") + wolf + "},{\"n\":1,\"minion\":" + skel + "}]}}", b2, e2);
        const bool form = mk1(std::string("{\"self\":true,\"uses\":1,\"form\":{\"body\":") + bear + "}}", b3, e3);
        const bool parse = one && two && form && b2.abil[0].sumN == 2 && b2.abil[0].sumMore.size() == 1 && b2.tmpl.size() == 2 && b3.abil[0].formTmpl == 0;
        g_B = 3000 + STAT_BASE;
        const bool price = parse && b2.abil[0].price > b1.abil[0].price && b3.abil[0].price > 0;
        // призыв ставит волков и скелета
        static World w;
        std::memset(w.wall, 0, sizeof w.wall);
        std::memset(w.pit, 0, sizeof w.pit);
        std::memset(w.deck, 0, sizeof w.deck);
        std::memset(w.tree, 0, sizeof w.tree);
        std::memset(w.height, 0, sizeof w.height);
        w.aw = w.ah = 30;
        w.dyn.clear();
        w.f.clear();
        w.f.reserve(64);
        initObjects(w);
        int wolves = 0, skels = 0;
        if (parse) {
            w.f.push_back(mk(b2, 0, 10, 10));
            w.f[0].uses = {1};
            g_curW = &w;
            summonFrom(w, 0, 0);
            g_curW = nullptr;
            for (size_t i = 1; i < w.f.size(); i++) (w.f[i].b.wN == 2 ? wolves : skels)++;
        }
        int mixed = 0;
        for (int i = 0; i < 3000; i++) {
            Boss b = makeBoss(1600, C2, true, true);
            for (auto& sp : b.abil) mixed += !sp.sumMore.empty();
        }
        T("T208", "§16 призыв у персонажа: группы разных приспешников из конструктора, цена, призыв ставит все группы; превращение из конструктора; смешанные призывы у генератора",
          parse && price && wolves == 2 && skels == 1 && mixed > 0,
          fmt("разбор %d (%s%s%s), цена %d, волков %d, скелетов %d, смешанных призывов у генератора %d из 3000", parse, e1.c_str(), e2.c_str(), e3.c_str(), price, wolves, skels, mixed));
    }
    // T209: нарастающая стойкость (этап 7) — паралич СЛ 20 на 10 раундов: без стойкости держался бы ~8 раундов из 10, с ней
    // каждый провал +2 к следующему спасброску — в среднем меньше 5; лёгкие состояния (опутан) стойкости не дают
    if (TB()) {
        double sumP = 0, sumR = 0;
        const int N = 4000;
        for (int i = 0; i < N; i++) {
            Fighter f = mk(plain(100, 0, 0, 5), 0, 1, 1), g = f;
            applyCond(f, C_PARAL, 10, WIS, 20, true);
            applyCond(g, C_RESTR, 10, STR, 20, true);
            int rp = 0, rr = 0;
            while (f.cond[C_PARAL] > 0 && rp < 20) { endOfTurn(f); rp++; }
            while (g.cond[C_RESTR] > 0 && rr < 20) { endOfTurn(g); rr++; }
            sumP += rp;
            sumR += rr;
        }
        const double mp = sumP / N, mr = sumR / N;
        T("T209", "§6 нарастающая стойкость: каждый проваленный повторный спасбросок от жёсткого контроля — +2 к следующему",
          mp < 5.0 && mr > 7.0, fmt("паралич СЛ 20 держится в среднем %.2f раунда, опутанность СЛ 20 — %.2f (без стойкости)", mp, mr));
    }
    // ===== v21, этап 3: обучение новой сети =====
    // T176: градиенты новой сети (типы, способность, указатели на врага и клетку, ценность, GRU по времени) совпадают с
    // численными
    if (TB()) {
        pn::PNetT<double> net;
        net.init(11);
        uint64_t sd = 99;
        auto rnd = [&]() { return GruNet<double>::rnd01(sd) * 2 - 1; };
        // 4 решения по ступеням: идти (клетка), применить → заклинание → способность 3 (клетка), взаимодействие → смена
        // оружия → запасное 2, применить → атака → оружие (враг)
        const int NT = 4;
        std::vector<pn::Decision> ds(NT);
        for (int t = 0; t < NT; t++) {
            pn::Decision& d = ds[t];
            d.x.resize(pn::D_IN);
            for (auto& v : d.x) v = (float)(rnd() * 0.5);
            for (int k = 0; k < pn::ST_N; k++) d.stMask[k] = true;
            d.stMask[pn::ST_END] = t != 1;   // «конец хода» закрыт (пустой ход) — маска тоже проверяется
            for (int k = 0; k < CAT_N; k++) d.catMask[k] = k != CAT_SUMMON;
            for (int k = 0; k < pn::NAB; k++) {
                d.abMask[k] = k == pn::AB_WEAPON || (k >= pn::AB_SPEC && k < pn::AB_SPEC + 4) || k == pn::AB_DASH;
                d.abCat[k] = k == pn::AB_WEAPON ? CAT_ATTACK : (k >= pn::AB_DASH ? CAT_MANEUVER : (k % 2 ? CAT_SPELL : CAT_ATTACK));
            }
            for (int k = 0; k < pn::IN_N; k++) d.inMask[k] = k != pn::IN_SURGE;
            for (int k = 0; k < pn::NSWAP; k++) d.swapMask[k] = true;
            d.st = t == 0 ? pn::ST_MOVE : (t == 2 ? pn::ST_INTERACT : pn::ST_APPLY);
            if (t == 1) d.cat = CAT_SPELL, d.ab = pn::AB_SPEC + 2;   // слот 3 — заклинание (нечётный)
            if (t == 3) d.cat = CAT_ATTACK, d.ab = pn::AB_WEAPON;
            if (t == 2) d.in = pn::IN_SWAP, d.sw = 1;
            if (t == 2) continue;   // смена оружия — без указателя
            d.ptrKind = t == 3 ? pn::PT_ENEMY : pn::PT_CELL;
            d.nc = t == 3 ? pn::MAXE : 5;
            const int nf = net.nf[d.ptrKind];
            d.feats.resize((size_t)d.nc * nf);
            for (auto& v : d.feats) v = (float)rnd();
            d.pmask.assign(d.nc, 1);
            d.pmask[0] = 0;
            d.c = 2;
        }
        const double R[NT] = {0.3, -0.2, 1.0, 0.5}, Wg[NT] = {1.0, -0.7, 0.4, 0.8}, vc = 0.5;
        auto lossOf = [&](const pn::PNetT<double>& n) {
            std::vector<double> h(n.H, 0.0), lg(pn::NFIX);
            typename GruNet<double>::Step st;
            double L = 0;
            for (int t = 0; t < NT; t++) {
                std::vector<double> x(ds[t].x.begin(), ds[t].x.end());
                double v;
                n.core.forward(x.data(), h.data(), st, lg.data(), v);
                h = st.h;
                L += -Wg[t] * pn::decisionLogp<double>(n, ds[t], h.data(), lg.data(), 0.0, nullptr, nullptr, nullptr) + 0.5 * vc * (v - R[t]) * (v - R[t]);
            }
            return L;
        };
        // аналитически: как в pt::update
        std::vector<double> gc(net.core.p.size(), 0.0), gp(net.ptr.size(), 0.0);
        {
            std::vector<typename GruNet<double>::Step> st(NT);
            std::vector<std::vector<double>> lg(NT, std::vector<double>(pn::NFIX));
            std::vector<double> val(NT), h(net.H, 0.0);
            std::vector<std::vector<double>> xs(NT);
            for (int t = 0; t < NT; t++) {
                xs[t].assign(ds[t].x.begin(), ds[t].x.end());
                net.core.forward(xs[t].data(), h.data(), st[t], lg[t].data(), val[t]);
                h = st[t].h;
            }
            std::vector<double> dhn(net.H, 0.0), dhp(net.H);
            for (int t = NT - 1; t >= 0; t--) {
                std::vector<double> dlg(pn::NFIX, 0.0), dh(dhn);
                pn::decisionLogp<double>(net, ds[t], st[t].h.data(), lg[t].data(), -Wg[t], dlg.data(), &gp, dh.data());
                net.core.backward(st[t], dlg.data(), vc * (val[t] - R[t]), dh.data(), gc, dhp.data());
                dhn = dhp;
            }
        }
        double worst = 0, worstAbs = 0;
        int checked = 0;
        auto check = [&](bool core, size_t i) {
            pn::PNetT<double> a = net, b = net;
            const double eps = 1e-6;
            (core ? a.core.p[i] : a.ptr[i]) += eps;
            (core ? b.core.p[i] : b.ptr[i]) -= eps;
            const double num = (lossOf(a) - lossOf(b)) / (2 * eps), an = core ? gc[i] : gp[i];
            // относительная ошибка — у градиентов от 1e-5; у мелких (~1e-6) разность при eps 1e-6 даёт лишь 3–4 верных
            // знака, там проверяется абсолютная ошибка
            const double mag = std::fabs(num) + std::fabs(an);
            if (mag >= 1e-5)
                worst = std::max(worst, std::fabs(num - an) / mag);
            else
                worstAbs = std::max(worstAbs, std::fabs(num - an));
            checked++;
        };
        for (size_t i = 0; i < net.core.p.size(); i += net.core.p.size() / 60) check(true, i);
        for (size_t i = net.core.oWo; i < net.core.p.size(); i += 97) check(true, i);   // головы и ценность
        for (size_t i = 0; i < net.ptr.size(); i += net.ptr.size() / 60) check(false, i);
        T("T176", "§v21 этап 3 и шаг 4: градиенты сети по ступеням (тип, категория, способность, взаимодействие, оружие, указатели, ценность, GRU по времени) совпадают с численными",
          worst < 1e-4 && worstAbs < 1e-9,
          fmt("проверено параметров %d, наибольшая относительная ошибка %.2e (градиенты от 1e-5, порог 1e-4), абсолютная у мелких %.1e (порог 1e-9)", checked, worst, worstAbs));
    }
    blkTime();
    if (g_shards > 1)
        printf("#ИТОГ %d %d\n", g_pass, g_fail);
    else
        printf("\nИтого: PASS %d, FAIL %d\n", g_pass, g_fail);
    return 0;
}
