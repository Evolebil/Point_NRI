// Исполняемая проверка требований Арены Боссов (правила: docs/pravila-v21.md) против движка src/engine/.
// Сборка и запуск: make test (или ./tests/run_tests.sh)
#include "engine.h"
#include "char_spec.h"
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
    g_B = 800;            // бюджет и контекст цены — с нуля: блок не зависит от того, что этот процесс считал перед ним
    g_pc = PriceCtx{};
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
#include "audit/01_T01-T38_osnova.inc"
#include "audit/02_T39-T69.inc"
#include "audit/03_T74-T96.inc"
#include "audit/04_T106-TR4.inc"
#include "audit/05_T116-T125_etap1.inc"
#include "audit/06_T126-T132_um_boica.inc"
#include "audit/07_T137-T155_seti.inc"
#include "audit/08_T156-T164_v21.inc"
#include "audit/09_T165-T169_v21_sposobnosti.inc"
#include "audit/10_T171-T191_v21_boi.inc"
#include "audit/11_T192-T209_v21_boi.inc"
#include "audit/12_T176_obuchenie.inc"
    blkTime();
    if (g_shards > 1)
        printf("#ИТОГ %d %d\n", g_pass, g_fail);
    else
        printf("\nИтого: PASS %d, FAIL %d\n", g_pass, g_fail);
    return 0;
}
