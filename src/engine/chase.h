// Погоня и осложнения, кадры для просмотрщика, легендарные действия
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// ---------------------------------------------------------------------------
// Погоня (DMG): бегущий (убегающий) и преследователь. Рывки без риска — 3 + мод. ТЕЛ за бой, дальше проверка ТЕЛ СЛ 10 или истощение; в конце каждого
// хода d20 (1–10 — осложнение для следующего участника); в конце раунда преследуемый проверяет ЛОВ (Скрытность) против пассивного МДР (Внимательность)
// преследователя: оторвался — сбежал, а сбежавший проигрывает. Провоцированных атак между участниками нет. Выход за арену — тоже побег.
// ---------------------------------------------------------------------------
thread_local long long g_chases = 0, g_escapes = 0, g_exhaustions = 0, g_complications = 0;
static bool chkBest(const Fighter& f, std::initializer_list<int> stats, int dc) {   // проверка характеристики (лучшая из перечисленных), помеха при истощении 1+
    int best = -99;
    for (int st : stats) best = std::max(best, f.mod(st));
    return d20(false, f.exh >= 1) + best >= dc;
}
static bool creatureHits(const Fighter& f, int bonus) {
    const int r = rollD20();
    return r == 20 || (r != 1 && r + bonus >= f.b.ac);
}
// Осложнение номер roll (1–10) действует на бойца f в начале его хода: город (theme 1) или дикая местность (0)
static const char* COMP_CITY[10] = {"Толпа на пути", "Рыночные лотки", "Скользкая мостовая", "Узкий проулок", "Груда ящиков", "Острый обломок", "Рухнувшие леса", "Встречный поток людей", "Городской страж", "Падающий хлам"};
static const char* COMP_WILD[10] = {"Густой кустарник", "Рытвины и коренья", "Рой насекомых", "Заросший склон", "Едкая пыльца", "Яма под листвой", "Силок", "Колючий бурелом", "Бритвенная лоза", "Местный зверь"};
// название осложнения по теме и броску к10 (бросок вне 1..10 прижимается к краю таблицы)
static const char* compName(int theme, int roll) {
    const int i = std::clamp(roll, 1, 10) - 1;
    return theme == 1 ? COMP_CITY[i] : COMP_WILD[i];
}
static void applyComplicationInner(World& w, Fighter& f, int roll);
// осложнение погони: применяет и пишет событие с итогом (для просмотрщика)
static void applyComplication(World& w, Fighter& f, int roll) {
    const int hp0 = f.hp, pen0 = f.speedPen, who = (int)(&f - &w.f[0]);
    const bool prone0 = f.cond[C_PRONE] > 0, restr0 = f.cond[C_RESTR] > 0, blind0 = f.cond[C_BLIND] > 0;
    applyComplicationInner(w, f, roll);
    std::string fx;
    auto add = [&](const std::string& t) { fx += (fx.empty() ? "" : ", ") + t; };
    if (f.hp < hp0) add("урон " + std::to_string(hp0 - f.hp));
    if (f.speedPen > pen0) add("скорость −" + std::to_string(f.speedPen - pen0) + " фт");
    if (!prone0 && f.cond[C_PRONE]) add("сбит с ног");
    if (!restr0 && f.cond[C_RESTR]) add("опутан");
    if (!blind0 && f.cond[C_BLIND]) add("ослеплён");
    if (fx.empty()) fx = "справился";
    evPush("{\"k\":\"comp\",\"who\":" + std::to_string(who) + ",\"roll\":" + std::to_string(roll) + ",\"t\":\"" + compName(w.theme, roll) + "\",\"fx\":\"" + fx + "\"}");
}
static void applyComplicationInner(World& w, Fighter& f, int roll) {
    g_complications++;
    auto prone = [&]() { if (f.hp > 0) applyCond(f, C_PRONE, 1, -1, 0, false); };
    auto dmg = [&](int n, int dieS, int type) { hurt(f, rollDice(n, dieS), type, false); };
    if (w.theme == 1) {
        switch (roll) {
            case 1: if (!chkBest(f, {DEX}, 15)) f.speedPen += 10; break;
            case 2: if (!chkBest(f, {STR, DEX}, 10)) f.speedPen += 10; break;
            case 3: if (!saveOK(f, STR, 10)) prone(); break;
            case 4: if (!chkBest(f, {DEX, INT}, 10)) f.speedPen += 10; break;
            case 5: if (!saveOK(f, DEX, 10)) prone(); break;
            case 6: if (!chkBest(f, {DEX}, 10)) { dmg(1, 4, DT_PIERCING); f.speedPen += 5; } break;
            case 7: if (!chkBest(f, {STR, DEX, CHA}, 15)) { dmg(2, 4, DT_BLUDGEONING); f.speedPen += 10; } break;
            case 8: if (!chkBest(f, {STR, DEX, CHA}, 10)) f.speedPen += 5; break;
            case 9: if (creatureHits(f, 3)) dmg(1, 6, DT_PIERCING), f.hp -= 1; break;   // страж: копьё +3, 1к6+1 колющего
            default: if (!saveOK(f, DEX, 10)) dmg(1, 4, DT_BLUDGEONING); break;
        }
    } else {
        switch (roll) {
            case 1: if (!chkBest(f, {STR, DEX}, 10)) f.speedPen += 5; break;
            case 2: if (!chkBest(f, {DEX}, 10)) f.speedPen += 10; break;
            case 3: if (creatureHits(f, 3)) dmg(4, 4, DT_PIERCING); break;   // рой насекомых
            case 4: if (!chkBest(f, {STR, DEX}, 10)) f.speedPen += 10; break;
            case 5: if (!saveOK(f, CON, 10)) { applyCond(f, C_BLIND, 1, -1, 0, false); f.speedPen += speedOf(f) / 2; } break;
            case 6:
                if (!saveOK(f, DEX, 10)) {   // яма: падение на 1к4×5 фт, по 1к6 за каждые 10 фт
                    const int fall = (1 + irand(4)) * 5;
                    if (fall >= 10) dmg(fall / 10, 6, DT_BLUDGEONING);
                    prone();
                }
                break;
            case 7: if (!saveOK(f, DEX, 15)) applyCond(f, C_RESTR, 2, STR, 12, true); break;   // силок: опутан
            case 8: if (!saveOK(f, DEX, 10)) { dmg(1, 4, DT_BLUDGEONING); dmg(1, 4, DT_PIERCING); prone(); } break;
            case 9: if (speedOf(f) >= 10) f.speedPen += 10; else if (!saveOK(f, DEX, 15)) dmg(1, 10, DT_SLASHING); break;   // бритвенная лоза: 10 фт движения или урон
            default: if (creatureHits(f, 3)) dmg(1, 8, DT_PIERCING), f.hp -= 1; break;   // местное существо кусает
        }
    }
}
// после хода босса i: начало и конец погони, рывки сверх нормы, осложнение для соперника; dist0 — расстояние между боссами до хода
static void chaseAfterTurn(World& w, int i, int dist0) {
    Fighter& me = w.f[i];
    Fighter& E = w.f[1 - i];
    if (me.hp <= 0 || E.hp <= 0 || me.fled) { me.dashedNow = false; return; }
    const int dist1 = distFt(me, E);
    const auto reachOf = [](const Fighter& f) { return f.b.wN > 0 && f.b.wRange <= 5 ? std::max(5, f.b.reach) : 5; };
    // побег — отход от реальной угрозы: враг мог бы достать за ход (досягаемость + рывок) или стреляет; обычное перемещение по большой арене погоней не считается
    const int threat = std::max(reachOf(E) + 2 * speedOf(E), E.b.wN > 0 && E.b.wRange > 5 ? E.b.wRange : 0);
    // v21, №13: «выглянуть, ударить, спрятаться» — не побег: кто в этот ход атаковал врага или применил по нему способность
    // либо остался там, где враг достанет его за свой ход (дойдёт и ударит или выстрелит), погоню не начинает
    const bool exposed = dist1 <= reachOf(E) + speedOf(E) || (E.b.wN > 0 && E.b.wRange > 5 && dist1 <= E.b.wRange);
    const bool flight = dist1 >= dist0 + 10 && dist1 > reachOf(E) && dist0 <= threat && !me.struckFoe && !exposed;   // отошёл на 10+ фт из-под угрозы и стоит вне досягаемости ближнего боя врага
    if (!w.chaseOn) {
        if (flight) {
            w.chaseOn = true;
            w.chasePrey = i;
            w.chaseSeen = true;
            g_chases++;
            evPush("{\"k\":\"chase\",\"prey\":" + std::to_string(i) + "}");
        }
    } else if (i == w.chasePrey) {
        if (dist1 + 10 <= dist0 || dist1 <= reachOf(E) || me.struckFoe) {   // преследуемый развернулся навстречу или напал: обычный бой
            w.chaseOn = false;
            evPush("{\"k\":\"chaseEnd\",\"why\":\"turned\"}");
        }
    } else if (dist1 <= reachOf(me)) {
        w.chaseOn = false;                                    // преследователь догнал
        evPush("{\"k\":\"chaseEnd\",\"why\":\"caught\"}");
    }
    if (w.chaseOn) {
        if (!losF(w, me, E)) w.chaseSeen = false;
        if (me.dashedNow) {
            me.dashes++;
            if (me.dashes > me.dashLimit && d20(false, me.exh >= 1) + me.mod(CON) < 10) {   // рывок сверх нормы: проверка ТЕЛ СЛ 10 или истощение
                me.exh = std::min(5, me.exh + 1);
                g_exhaustions++;
                evPush("{\"k\":\"exh\",\"who\":" + std::to_string(i) + ",\"lv\":" + std::to_string(me.exh) + "}");
                if (me.exh == 4) { me.maxHp = std::max(1, me.maxHp / 2); me.hp = std::min(me.hp, me.maxHp); }   // истощение 4: максимум ХП вдвое
            }
        }
        const int r = rollD20();
        if (r <= 10) E.comp = r;                              // осложнение действует на следующего участника
    }
    me.dashedNow = false;
}
// конец раунда погони: проверка ЛОВ (Скрытность) преследуемого против пассивного МДР (Внимательность) преследователя
static void chaseEndOfRound(World& w) {
    if (!w.chaseOn) return;
    Fighter& prey = w.f[w.chasePrey];
    Fighter& pu = w.f[1 - w.chasePrey];
    if (prey.hp <= 0 || pu.hp <= 0) return;
    const bool seen = w.chaseSeen;
    w.chaseSeen = true;
    if (pu.exh >= 5 && distFt(prey, pu) > std::max(5, pu.b.reach)) {   // преследователь выбился из сил (скорость 0) и выбыл из погони: преследуемый ушёл
        prey.fled = true;
        prey.fledWhy = 3;
        w.chaseOn = false;
        g_escapes++;
        evPush("{\"k\":\"fled\",\"who\":" + std::to_string(w.chasePrey) + ",\"why\":3}");
        return;
    }
    if (seen) return;   // не терял преследователя из виду: проверка проваливается сама
    int cover = 0;
    for (int dy = -6; dy <= 6; dy++)
        for (int dx = -6; dx <= 6; dx++) {
            const int x = prey.x + dx, y = prey.y + dy;
            if (x < 0 || y < 0 || x >= w.aw || y >= w.ah) continue;
            if (w.wall[y][x] || w.climb[y][x] || w.dark[y][x]) cover++;
        }
    const bool adv = cover >= 12 || w.dark[prey.y][prey.x], dis = cover <= 1 || prey.exh >= 1 || (prey.b.armStealth && prey.b.armor);   // доспех с помехой к Скрытности   // много/мало мест, где спрятаться
    if (d20(adv || skillAdv(prey, SK_STEALTH), dis) + prey.skill(SK_STEALTH) > 10 + pu.skill(SK_PERCEP) + (skillAdv(pu, SK_PERCEP) ? 5 : 0)) {   // преимущество к пассивной — +5   // Скрытность против пассивной Внимательности
        prey.fled = true;
        prey.fledWhy = 2;
        w.chaseOn = false;
        g_escapes++;
        evPush("{\"k\":\"fled\",\"who\":" + std::to_string(w.chasePrey) + ",\"why\":2}");
    }
}

static thread_local long long g_rounds = 0, g_capped = 0;
// 0 = победил A, 1 = победил B, 2 = ничья
std::string condListJson(const Fighter& f) {
    std::string s = "[";
    bool first = true;
    for (int k = 1; k < C_N; k++)
        if (f.cond[k] > 0) {
            if (!first) s += ",";
            s += "\"";
            s += CN[k];
            s += "\"";
            first = false;
        }
    s += "]";
    return s;
}
// чей туман войны показывать в реплее (−1 — ничей): реплей боя сети помечает врагов, которых её сторона не знает
thread_local int g_fogTeam = -1;
// hid — боец скрыт туманом войны от стороны игрока (или g_fogTeam): игроку не рисуется, в реплее — полупрозрачный
std::string fighterJson(const Fighter& f, int kind, bool hid = false) {
    char buf[500];
    std::snprintf(buf, sizeof buf,
                  "{\"x\":%d,\"y\":%d,\"hp\":%d,\"maxHp\":%d,\"ac\":%d,\"team\":%d,\"kind\":%d,\"temp\":%d,\"exh\":%d,"
                  "\"dash\":%d,\"dashLim\":%d,\"spd\":%d,\"fled\":%d,\"cond\":%s%s%s}",
                  f.x, f.y, f.hp, f.maxHp, f.b.ac + (f.acBuffRounds > 0 ? f.acBuff : 0), f.team, kind, f.temp, f.exh, f.dashes, f.dashLimit,
                  speedOf(f), f.fled ? f.fledWhy : 0, condListJson(f).c_str(), hid ? ",\"hid\":1" : "", f.up ? ",\"up\":1" : "");
    if (fpN(f) > 1 || f.alt > 0) {   // крупный: квадрат n×n от (x, y); летун: высота (уровни по 10 фт)
        std::string r = buf;
        if (fpN(f) > 1) r.insert(r.size() - 1, ",\"n\":" + std::to_string(fpN(f)));
        if (f.alt > 0) r.insert(r.size() - 1, ",\"alt\":" + std::to_string(f.alt));
        return r;
    }
    return buf;
}
// Легендарное действие: в конце хода ДРУГОГО существа, один вариант, пока хватает очков; очки возвращаются в начале
// своего хода. Варианты: 1 очко — одна атака оружием; 2 очка — полное действие Атака (все атаки); 1 очко — Движение на
// половину скорости без провокации.
bool legendaryTurn(World& w, int j) {
    Fighter& me = w.f[j];
    if (me.hp <= 0 || me.b.legAct <= 0 || me.legLeft <= 0 || over(w)) return false;
    // недееспособный не может
    if (me.cond[C_STUN] || me.cond[C_PARAL] || me.cond[C_INCAP]) return false;
    int focus = -1;
    double bd = 1e9;
    for (size_t e = 0; e < w.f.size(); e++) {
        const Fighter& g = w.f[e];
        if (g.hp <= 0 || g.team == me.team || !isKnown(w, me.team, (int)e)) continue;   // туман войны
        if (me.cond[C_CHARM] > 0 && me.charmSrc == (int)e) continue;
        double d = distFt(me, g) - ((int)e < w.nLead ? me.pol->v[PO_FOCUSFT] : 0);
        if (d < bd) {
            bd = d;
            focus = (int)e;
        }
    }
    if (focus < 0) return false;
    int tIdx = me.b.wN > 0 ? nearestEnemyInRange(w, me, atkDist(me.b)) : -1;   // безоружный: только движение
    g_act = ActInfo{};
    if (tIdx >= 0) {
        me.riderUsed = false;
        me.onceUsed = 0;
        // полное действие Атака стоит 2, одна атака — 1
        bool full = me.legLeft >= 2 && me.b.wAtt >= 2;
        doWeapon(w, me, tIdx, full ? -1 : 1);
        me.legLeft -= full ? 2 : 1;
        g_legUsed++;
        return true;
    }
    // Движение: подойти на половину скорости, без провоцированных атак
    if (!me.cond[C_FRIGHT] && !me.cond[C_RESTR]) {
        Fighter& F = w.f[focus];
        bool d0 = me.disengaged;
        me.disengaged = true;
        int used = moveToward(w, me, F, atkDist(me.b), speedOf(me) / 2, true);
        me.disengaged = d0;
        if (used > 0) {
            g_act.type = "legendary-move";
            g_act.target = focus;
            me.legLeft -= 1;
            g_legUsed++;
            return true;
        }
    }
    return false;
}
