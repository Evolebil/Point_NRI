// ============================================================================
// Две команды нейросетей (Этап 6, итог): у каждой команды своя пара — сеть-Сборщик (builder_net.h) и сеть-боец (fighter_train.h).
// Команды играют друг с другом матчами: каждая собирает своего босса своей сетью-Сборщиком, босса ведёт своя сеть-боец (если ИНТ >= 12;
// иначе обычный бот). Оба боя матча идут со сменой сторон. Обе пары учатся одновременно наградой:
//   сеть-Сборщик — исход матча (единственная награда в конце сборки), сеть-боец — урон врагу/мне на каждом решении и исход боя.
// Соперники в обучении: другая команда сейчас (60%), зал славы другой команды (20%), якорь — обычный генератор + правило-бот (20%).
//
//   ./net_teams train <итераций> <матчей в итерации> <зерно> <префикс> [стартовый Сборщик T0] [стартовый Сборщик T1] [стартовый боец]
//   ./net_teams eval  <префикс> <матчей>           — команда 0 против команды 1, обе против якоря и против пары «лучший прежний Сборщик + общий боец»
// Сборка: g++ -std=c++17 -O2 -pthread -o net_teams net_teams.cc
// ============================================================================
#define main boss_main
#include "boss_arena_v20.cc"
#undef main
#include <cstdio>
#include <fstream>
#include <sstream>
#include "gru.h"
#include "par.h"
#include "fighter_train.h"
#include "builder_net.h"

using bld::BNet;
static const long long BUDGETS[6] = {200, 400, 800, 1600, 3200, 6400};

struct Team {
    bld::Learner builder;              // сеть-Сборщик, Adam, зал славы Сборщика
    Net fighter;                       // сеть-боец
    AdamState fadam;
    std::vector<Net> fhof;             // зал славы бойца (параллелен builder.hof)
    void snapshot() { builder.snapshot(); fhof.push_back(fighter); if (fhof.size() > 8) fhof.erase(fhof.begin()); }
};
struct TeamSide {   // что команда выставляет в матче
    const BNet* builder = nullptr;
    const Net* fighter = nullptr;
};

struct Match {
    bld::Sample bs[2];                 // Сборщик команды 0 и 1: шаги сборки и оценка (доля очков матча)
    std::vector<cmb::Traj> ct[2];      // траектории бойцов команд 0 и 1 (по бою)
    double score0 = 0;
    int failures = 0, guards = 0, int12[2] = {0, 0};
};

// Один бой: боссы A (сторона 0) и B (сторона 1), у каждой стороны своя сеть-боец (или обычный бот). Траектории сети записываются.
static int fightTeams(const Boss& A, const Boss& B, const Net* nA, const Net* nB, bool greedy, uint64_t seed, std::vector<cmb::Traj>* tr, Match* m, int teamOfSide0) {
    NetCtx ctx;
    ctx.greedy = greedy;
    ctx.rng.seed(mix(seed, 4711));
    const Net* nets[2] = {nA, nB};
    const Boss* bs[2] = {&A, &B};
    for (int t = 0; t < 2; t++) {
        const int in = bs[t]->stats[INT];
        if (nets[t] && in >= 12) {
            ctx.side[t].active = true;
            ctx.sideNet[t] = nets[t];
            ctx.fallback[t] = in <= 15 ? 0.25 : 0.0;
            g_rlTeam[t] = true;
            if (m) m->int12[teamOfSide0 ^ t]++;
        } else {
            g_rlTeam[t] = false;
        }
    }
    g_ctx = &ctx;
    const long long c0 = g_capped;
    const int r = fight(A, B);
    if (m) m->guards += (int)(g_capped - c0);
    g_rlTeam[0] = g_rlTeam[1] = false;
    g_ctx = nullptr;
    for (int t = 0; t < 2; t++) {
        if (!ctx.side[t].active) continue;
        cmb::Traj tj;
        tj.outcome = r == 2 ? 2 : (r == t ? 1 : 0);
        auto& sd = ctx.side[t];
        if (!sd.steps.empty()) {
            const double term = tj.outcome == 1 ? 1.0 : (tj.outcome == 0 ? -1.0 : 0.0);
            sd.steps.back().reward = (float)((sd.prevOpp - g_lastHp[1 - t]) - (sd.prevMy - g_lastHp[t]) + term - g_netTimePen);
        }
        tj.steps = std::move(sd.steps);
        if (tr && t < 2) tr[t].push_back(std::move(tj));   // tr[t]: траектории стороны t этого боя
    }
    return r;
}

// Матч: Сборщик команды X собирает босса (с записью, если recX), соперник — команда Y или якорь (обычный генератор и правило-бот).
static void runMatch(const Coef& C, long long B, const TeamSide& X, const TeamSide* Y, bool recX, bool recY, bool greedy, uint64_t seed, Match& out) {
    rng.seed(seed);
    bld::BrainNet bx(X.builder, recX ? &out.bs[0].steps : nullptr);
    g_bb = &bx;
    Boss bossX = makeBoss(B, C, true, false);
    g_bb = nullptr;
    Boss bossY;
    if (Y) {
        bld::BrainNet by(Y->builder, recY ? &out.bs[1].steps : nullptr);
        g_bb = &by;
        bossY = makeBoss(B, C, true, false);
        g_bb = nullptr;
    } else {
        bossY = makeBoss(B, C, true, false);
    }
    out.failures = bld::checkBoss(bossX, B) + bld::checkBoss(bossY, B);
    const Net* nX = X.fighter;
    const Net* nY = Y ? Y->fighter : nullptr;
    std::vector<cmb::Traj> trA[2], trB[2];
    // бой 1: X — сторона 0
    const int r1 = fightTeams(bossX, bossY, nX, nY, greedy, mix(seed, 1), trA, &out, 0);
    // бой 2: Y — сторона 0
    const int r2 = fightTeams(bossY, bossX, nY, nX, greedy, mix(seed, 2), trB, &out, 1);
    const double sX = ((r1 == 0 ? 1.0 : (r1 == 2 ? 0.5 : 0.0)) + (r2 == 1 ? 1.0 : (r2 == 2 ? 0.5 : 0.0))) / 2;
    out.score0 = sX;
    out.bs[0].score = sX;
    out.bs[1].score = 1 - sX;
    for (auto& t : trA[0]) out.ct[0].push_back(std::move(t));   // X ходила стороной 0 в бою 1
    for (auto& t : trB[1]) out.ct[0].push_back(std::move(t));   // X ходила стороной 1 в бою 2
    for (auto& t : trA[1]) out.ct[1].push_back(std::move(t));
    for (auto& t : trB[0]) out.ct[1].push_back(std::move(t));
    out.bs[0].mine = bossX;
    out.bs[1].mine = bossY;
}

static bool loadFighter(const std::string& fn, Net& n) { return cmb::loadNet(fn, n); }
static void freshFighter(Net& n, const std::string& from) {
    if (!loadFighter(from, n)) { std::fprintf(stderr, "не прочитан боец %s\n", from.c_str()); std::exit(1); }
}

static int trainMain(int argc, char** argv) {
    const int iters = argc > 2 ? std::atoi(argv[2]) : 100, M = argc > 3 ? std::atoi(argv[3]) : 512;
    const uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1;
    const std::string prefix = argc > 5 ? argv[5] : "net_runs/team";
    const std::string b0 = argc > 6 ? argv[6] : "net_runs/b3_A.bin", b1 = argc > 7 ? argv[7] : "net_runs/b3_B.bin", fgt = argc > 8 ? argv[8] : "net_fighter.bin";
    if (const char* e = std::getenv("DND_TIMEPEN")) g_netTimePen = std::atof(e); else g_netTimePen = 0.06;
    Coef C = bld::coefNow();
    Team T[2];
    for (int t = 0; t < 2; t++) {
        if (!bld::loadNet(t == 0 ? b0 : b1, T[t].builder.net)) { std::fprintf(stderr, "не прочитан Сборщик\n"); return 1; }
        T[t].builder.adam.init(T[t].builder.net.size());
        freshFighter(T[t].fighter, fgt);
        T[t].fadam.init(T[t].fighter.size());
    }
    // якорь для контроля: лучший прежний Сборщик (b3_A) и общий боец (net_fighter.bin)
    BNet baseB;
    Net baseF;
    const bool haveBase = bld::loadNet("net_runs/b3_A.bin", baseB) && loadFighter("net_fighter.bin", baseF);
    std::vector<Match> mt(M);
    std::vector<char> kind(M);
    for (int it = 1; it <= iters; it++) {
        parallelFor(M, [&](int k) {
            const uint64_t sd = mix(mix(seed, it), k);
            const int r = (int)(mix(sd, 77) % 100);
            int kd = r < 60 ? 0 : (r < 80 ? 1 : 2);            // 0 — две текущие команды, 1 — против зала славы, 2 — против якоря
            const int learner = (int)(mix(sd, 78) & 1);        // для типов 1 и 2: какая команда учится
            Match& m = mt[k];
            m = Match();
            const long long B = BUDGETS[mix(sd, 5) % 6];
            const TeamSide s0{&T[0].builder.net, &T[0].fighter}, s1{&T[1].builder.net, &T[1].fighter};
            if (kd == 1 && T[1 - learner].builder.hof.empty()) kd = 0;
            if (kd == 0) {
                runMatch(C, B, s0, &s1, true, true, false, sd, m);
            } else if (kd == 1) {
                const Team& o = T[1 - learner];
                const size_t hi = mix(sd, 79) % o.builder.hof.size();
                const TeamSide hs{&o.builder.hof[hi], &o.fhof[hi]};
                const TeamSide& me = learner == 0 ? s0 : s1;
                runMatch(C, B, me, &hs, true, false, false, sd, m);
            } else {
                const TeamSide& me = learner == 0 ? s0 : s1;
                runMatch(C, B, me, nullptr, true, false, false, sd, m);
            }
            kind[k] = (char)(kd * 2 + learner);   // для статистики
            if (kd != 0) {   // учится одна команда: данные соперника (зал славы, якорь) не нужны
                m.bs[1].steps.clear();
                m.ct[1].clear();
                if (learner == 1) { std::swap(m.bs[0], m.bs[1]); std::swap(m.ct[0], m.ct[1]); }   // данные учащейся команды 1 — на индекс 1
            }
        });
        // статистика
        double s01 = 0, n01 = 0, sAnc[2] = {0, 0}, nAnc[2] = {0, 0}, fails = 0, guards = 0, steps = 0;
        for (int k = 0; k < M; k++) {
            const int kd = kind[k] / 2, lr = kind[k] % 2;
            fails += mt[k].failures; guards += mt[k].guards;
            if (kd == 0) { s01 += mt[k].score0; n01++; }
            if (kd == 2) { sAnc[lr] += (lr == 0 ? mt[k].bs[0].score : mt[k].bs[1].score); nAnc[lr]++; }
            steps += mt[k].bs[0].steps.size() + mt[k].bs[1].steps.size();
        }
        // обновления: Сборщики и бойцы обеих команд
        double ent[2], vl[2];
        cmb::UpdStats cu[2];
        for (int t = 0; t < 2; t++) {
            std::vector<bld::Sample> sm;
            sm.reserve(M);
            std::vector<cmb::Traj*> trajs;
            for (int k = 0; k < M; k++) {
                if (!mt[k].bs[t].steps.empty()) sm.push_back(std::move(mt[k].bs[t]));
                for (auto& tj : mt[k].ct[t]) if (!tj.steps.empty()) trajs.push_back(&tj);
            }
            if (!sm.empty()) bld::updateLearner(T[t].builder, sm, it, ent[t], vl[t]); else ent[t] = vl[t] = 0;
            if (!trajs.empty()) cu[t] = cmb::applyUpdate(T[t].fighter, T[t].fadam, trajs);
        }
        if (it % 25 == 0) { T[0].snapshot(); T[1].snapshot(); }
        if (it % 5 == 0 || it == 1) {
            std::printf("итерация %4d: команда 0 против команды 1 %.1f%% (%.0f матчей); против якоря: команда 0 %.1f%%, команда 1 %.1f%%; решений Сборщика на матч %.0f; награда бойца %.2f/%.2f; сбоев %.0f; до предохранителя %.0f\n", it,
                        n01 ? 100 * s01 / n01 : 0.0, n01, nAnc[0] ? 100 * sAnc[0] / nAnc[0] : 0.0, nAnc[1] ? 100 * sAnc[1] / nAnc[1] : 0.0, steps / M, cu[0].meanRet, cu[1].meanRet, fails, guards);
            std::fflush(stdout);
        }
        if (it % 50 == 0 || it == iters) {
            for (int t = 0; t < 2; t++) {
                bld::saveNet(prefix + "_T" + std::to_string(t) + "_builder.bin", T[t].builder.net);
                cmb::saveNet(prefix + "_T" + std::to_string(t) + "_fighter.bin", T[t].fighter);
            }
            // контроль: жадные бойцы; против команды 1, против якоря и против прежней пары
            const int N = 1500;
            std::vector<Match> ev(N), eb(N), ea(N);
            parallelFor(N, [&](int k) {
                const uint64_t sd = mix(990000 + it, k);
                const long long B = BUDGETS[mix(sd, 5) % 6];
                const TeamSide s0{&T[0].builder.net, &T[0].fighter}, s1{&T[1].builder.net, &T[1].fighter}, sb{&baseB, &baseF};
                runMatch(C, B, s0, &s1, false, false, true, sd, ev[k]);
                if (haveBase) runMatch(C, B, s0, &sb, false, false, true, mix(sd, 3), eb[k]);
                runMatch(C, B, s0, nullptr, false, false, true, mix(sd, 4), ea[k]);
            });
            double a = 0, b = 0, c = 0, f = 0, h0 = 0, h1 = 0;
            for (int k = 0; k < N; k++) { a += ev[k].score0; b += eb[k].score0; c += ea[k].score0; f += ev[k].failures + eb[k].failures + ea[k].failures; h0 += ev[k].int12[0]; h1 += ev[k].int12[1]; }
            std::printf("  контроль (%d матчей, бойцы жадно): команда 0 против команды 1 %.1f%%; команда 0 против прежней пары (Сборщик b3_A + общий боец) %.1f%%; команда 0 против якоря (случайная сборка + правило-бот) %.1f%%; системных сбоев %.0f; боёв команды 0 / 1 под управлением сети-бойца: %.0f / %.0f\n",
                        N, 100 * a / N, 100 * b / N, 100 * c / N, f, h0, h1);
            std::fflush(stdout);
        }
    }
    return 0;
}

static int evalMain(int argc, char** argv) {
    const std::string prefix = argc > 2 ? argv[2] : "net_runs/team";
    const int N = argc > 3 ? std::atoi(argv[3]) : 3000;
    Coef C = bld::coefNow();
    BNet b[2];
    Net f[2];
    for (int t = 0; t < 2; t++) {
        if (!bld::loadNet(prefix + "_T" + std::to_string(t) + "_builder.bin", b[t]) || !loadFighter(prefix + "_T" + std::to_string(t) + "_fighter.bin", f[t])) { std::fprintf(stderr, "не прочитаны веса команды %d\n", t); return 1; }
    }
    BNet baseB;
    Net baseF;
    const bool haveBase = bld::loadNet("net_runs/b3_A.bin", baseB) && loadFighter("net_fighter.bin", baseF);
    std::printf("| Бюджет | команда 0 против команды 1 | команда 0 против прежней пары | команда 1 против прежней пары | команда 0 против якоря | команда 1 против якоря | ИНТ>=12 (команда 0 / 1) |\n|---|---|---|---|---|---|---|\n");
    for (int k = 0; k < 6; k++) {
        const long long B = BUDGETS[k];
        std::vector<Match> ev(N), e0(N), e1(N), a0(N), a1(N);
        const TeamSide s0{&b[0], &f[0]}, s1{&b[1], &f[1]}, sb{&baseB, &baseF};
        parallelFor(N, [&](int i) {
            const uint64_t sd = mix(31337 + k, i);
            runMatch(C, B, s0, &s1, false, false, true, sd, ev[i]);
            if (haveBase) { runMatch(C, B, s0, &sb, false, false, true, mix(sd, 3), e0[i]); runMatch(C, B, s1, &sb, false, false, true, mix(sd, 4), e1[i]); }
            runMatch(C, B, s0, nullptr, false, false, true, mix(sd, 5), a0[i]);
            runMatch(C, B, s1, nullptr, false, false, true, mix(sd, 6), a1[i]);
        });
        double w[5] = {0, 0, 0, 0, 0}, hi0 = 0, hi1 = 0, bossN = 0;
        for (int i = 0; i < N; i++) {
            w[0] += ev[i].score0; w[1] += e0[i].score0; w[2] += e1[i].score0; w[3] += a0[i].score0; w[4] += a1[i].score0;
            hi0 += ev[i].bs[0].mine.stats[INT] >= 12; hi1 += ev[i].bs[1].mine.stats[INT] >= 12; bossN++;
        }
        std::printf("| %lld | %.1f%% | %.1f%% | %.1f%% | %.1f%% | %.1f%% | %.0f%% / %.0f%% |\n", B, 100 * w[0] / N, 100 * w[1] / N, 100 * w[2] / N, 100 * w[3] / N, 100 * w[4] / N, 100 * hi0 / bossN, 100 * hi1 / bossN);
    }
    return 0;
}

int main(int argc, char** argv) {
    g_rlTurn = netTurn;   // хук хода для бойцов под управлением сети (флаг rl)
    if (argc > 1 && std::string(argv[1]) == "train") return trainMain(argc, argv);
    if (argc > 1 && std::string(argv[1]) == "eval") return evalMain(argc, argv);
    std::fprintf(stderr, "использование: net_teams train|eval (см. шапку файла)\n");
    return 1;
}
