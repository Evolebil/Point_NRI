// Описание босса текстом (describeBoss)
// Часть движка (порядок частей — engine.h): отдельно не подключается.
#pragma once

// кости хитов (Бестиарий): кость по размеру (к4 крошечный … к20 громадный), хиты ≈ N × (средняя кость + мод. ТЕЛ)
std::string hitDiceText(const Boss& b) {
    static const int HD[6] = {4, 6, 8, 10, 12, 20};
    const int d = HD[std::clamp(b.size, 0, 5)], con = modOf(b.stats[CON]);
    const double per = std::max(1.0, (d + 1) / 2.0 + con);
    const int n = std::max(1, (int)std::lround(b.hp / per)), rest = b.hp - (int)std::lround(n * (d + 1) / 2.0);
    return std::to_string(n) + "к" + std::to_string(d) + (rest > 0 ? " + " + std::to_string(rest) : (rest < 0 ? " − " + std::to_string(-rest) : ""));
}
std::string describeBoss(const Boss& b) {
    char buf[400];
    std::string r;
    r += std::string("  ") + CTN[std::clamp(b.ctype, 0, 13)] + ", кости хитов " + hitDiceText(b) + "\n";
    std::snprintf(buf, sizeof buf, "  Статы:");
    r += buf;
    for (int i = 0; i < STAT_N; i++) {
        std::snprintf(buf, sizeof buf, " %s %d", SN[i], b.stats[i]);
        r += buf;
    }
    std::snprintf(buf, sizeof buf, " | %s | КД %d (%s%s) | ХП %d | главная: %s | стоимость %lld\n", SZN[std::clamp(b.size, 0, 5)], b.ac, ARN[std::clamp(b.armor, 0, AR_N - 1)], b.shield ? ", щит" : "", b.hp, SN[b.mainStat],
                  b.cost);
    r += buf;
    if (b.wN > 0) {
        std::snprintf(buf, sizeof buf, "  Оружие%s: %dк%d, дальн. %d фт, атак за действие: %d%s (%lld очк.)\n",
                      b.wNatural ? " (естественное)" : "", b.wN, b.wS, b.wRange, b.wAtt,
                      b.wRange > 5 ? (b.wAmmo ? ", боеприпасное (ЛОВ)"
                                              : (b.wFinesse ? ", метательное фехтовальное" : ", метательное (СИЛ)"))
                                   : (b.wFinesse ? ", фехтовальное" : ""),
                      b.wcost);
        r += buf;
        if (b.truesight) r += "  Истинное зрение " + std::to_string(b.truesight) + " фт\n";
        if (b.wLight || b.wLong || b.poolN || b.wTwoH || b.wVers || b.wHeavy) {   // оружие и Зуро (v21)
            r += "  Свойства:";
            if (b.wTwoH) r += " двуручное;";
            if (b.wHeavy) r += " тяжёлое;";
            if (b.wVers) r += " универсальное (двумя руками к" + std::to_string(b.wVers) + ");";
            if (b.wLight) r += " лёгкое;";
            if (b.wLong) r += " дальняя дистанция до " + std::to_string(b.wLong) + " фт (с помехой);";
            if (b.poolN) r += " запас костей " + std::to_string(b.poolN) + "к" + std::to_string(b.poolS) + ";";
            r += "\n";
        }
    } else
        r += "  Оружие: безоружный\n";
    if (b.wN > 0 && b.wRange <= 5 && b.reach > 5) {
        std::snprintf(buf, sizeof buf, "  Досягаемость: %d фт\n", b.reach);
        r += buf;
    }
    if (b.rN > 0 || b.rCond > 0) {
        r += "  Довесок к удару (раз в ход):";
        if (b.rN > 0) {
            std::snprintf(buf, sizeof buf, " +%dк%d (%s)", b.rN, b.rS, DTN[b.rType]);
            r += buf;
        }
        if (b.rCond > 0) {
            std::snprintf(buf, sizeof buf, " + %s на %d р. (спасбросок %s)", CN[b.rCond], b.rRounds,
                          b.rCond == C_GRAPPLE ? "—" : SN[condSaveStat(b.rCond)]);
            r += buf;
        }
        std::snprintf(buf, sizeof buf, " (%lld очк.)\n", b.rcost);
        r += buf;
    }
    if (b.auraRad > 0) {
        r += "  Аура (всегда включена):";
        if (b.auraN > 0) {
            std::snprintf(buf, sizeof buf, " %dк%d (%s)", b.auraN, b.auraS, DTN[b.auraType]);
            r += buf;
        }
        if (b.auraCond > 0) {
            std::snprintf(buf, sizeof buf, " + %s на %d р. (спасбросок %s)", CN[b.auraCond], b.auraRounds,
                          b.auraCond == C_GRAPPLE ? "—" : SN[condSaveStat(b.auraCond)]);
            r += buf;
        }
        std::snprintf(buf, sizeof buf, ", радиус %d фт\n", b.auraRad);
        r += buf;
    }
    {
        bool any = false;
        for (int L = 1; L < 10; L++)
            if (b.slots[L]) any = true;
        if (any) {
            r += "  Ячейки заклинаний:";
            for (int L = 1; L < 10; L++)
                if (b.slots[L]) {
                    std::snprintf(buf, sizeof buf, " %d ур. ×%d;", L, b.slots[L]);
                    r += buf;
                }
            r += "\n";
        }
    }
    for (auto& rl : b.rules) r += "  Правило: " + describeRule(rl) + "\n";
    for (auto& tg : b.trigs) r += "  Событие: " + describeTrig(tg) + "\n";
    if (b.riposte || b.surge) {
        r += "  Реакции:";
        if (b.riposte) r += " контратака (реакция);";
        if (b.surge) {
            std::snprintf(buf, sizeof buf, " всплеск действий ×%d;", b.surge);
            r += buf;
        }
        r += "\n";
    }
    if (b.dv || b.blindsight || b.ignDiff || b.climbS || b.swimS || b.burrowS || b.evasion || b.sunSens || b.lair) {
        r += "  Чувства/движение:";
        if (b.dv) {
            std::snprintf(buf, sizeof buf, " тёмное зрение %d фт;", b.dv);
            r += buf;
        }
        if (b.blindsight) {
            std::snprintf(buf, sizeof buf, " слепое зрение %d фт;", b.blindsight);
            r += buf;
        }
        if (b.ignDiff) r += " игнорирует труднопроходимую местность;";
        if (b.climbS) r += " лазает;";
        if (b.swimS) r += " плавает;";
        if (b.burrowS) r += " копает;";
        if (b.evasion) r += " увёртливость;";
        if (b.lair) r += " логово " + std::to_string(b.lair) + ";";
        if (b.sunSens) r += " боится солнца;";
        r += "\n";
    }
    for (auto& q : b.alt) {
        std::snprintf(buf, sizeof buf, "  Запасное оружие: %dк%d (%s), дальн. %d фт%s, атак: %d%s (%lld очк.)\n", q.n,
                      q.s, DTN[q.type], q.range, (q.range <= 5 && q.reach > 5) ? " (досягаемость больше)" : "", q.att,
                      q.range > 5 ? (q.ammo ? ", боеприпасное" : ", метательное") : (q.finesse ? ", фехтовальное" : ""),
                      q.cost);
        r += buf;
    }
    if (b.suppMask) {
        auto mk2 = [&](unsigned m) {
            std::string o;
            for (int t = 0; t < DT_N; t++)
                if ((m >> t) & 1) o += std::string(o.empty() ? "" : ", ") + DTN[t];
            return o;
        };
        std::string tg;
        if (b.suppTgt & 1) tg += "регенерация ";
        if (b.suppTgt & 2) tg += "аура ";
        if (b.suppTgt & 4) tg += "резисты/иммунитеты ";
        r += "  Подавление: урон типа (" + mk2(b.suppMask) + ") отключает на ход: " + tg + "\n";
    }
    r += std::string("  Тип урона оружия: ") + DTN[b.wType] + (b.wMagic ? " (магическое)" : " (немагическое)") + "\n";
    if (b.regenN || b.resMask || b.immMask || b.vulnMask || b.resNM || b.immNM || b.fly ||
        b.speed != 30 || b.immCond || b.saveProf || b.legRes || b.legAct || b.parry || b.uncanny || b.immPush || b.magicRes || b.spellRes || b.critImm || b.wSilver || b.lightR || b.unarmoredStat >= 0 ||
        b.disengB || b.redirect || b.prof != 2 || b.wNoProf || b.skillProf || b.toolN) {
        r += "  Пассивы:";
        if (b.prof != 2) {
            std::snprintf(buf, sizeof buf, " мастерство +%d;", b.prof);
            r += buf;
        }
        if (b.wNoProf) r += " не владеет своим оружием;";
        for (int k = 0; k < SK_N; k++)
            if ((b.skillProf >> k) & 1u) r += std::string(" ") + SKN[k] + (((b.skillExp >> k) & 1u) ? " (экспертиза)" : "") + ";";
        if (b.toolN) {
            std::snprintf(buf, sizeof buf, " инструментов %d;", b.toolN);
            r += buf;
        }
        if (b.regenN) {
            std::snprintf(buf, sizeof buf, " регенерация %dк6;", b.regenN);
            r += buf;
        }
        auto mk = [&](unsigned m) {
            std::string o;
            for (int t = 0; t < DT_N; t++)
                if ((m >> t) & 1) o += std::string(o.empty() ? "" : ", ") + DTN[t];
            return o;
        };
        if (b.resMask) r += " резист: " + mk(b.resMask) + ";";
        if (b.immMask) r += " иммунитет: " + mk(b.immMask) + ";";
        if (b.vulnMask) r += " УЯЗВИМОСТЬ: " + mk(b.vulnMask) + ";";
        if (b.resNM) r += " резист к немагическому физическому;";
        if (b.immNM) r += " иммунитет к немагическому физическому;";
        if (b.parry) {
            std::snprintf(buf, sizeof buf, " парирование +%d;", b.parry);
            r += buf;
        }
        if (b.uncanny) r += " невероятное уклонение;";
        if (b.magicRes) r += " сопротивление магии;";
        if (b.spellRes) r += " сопротивление урону от способностей;";
        if (b.critImm) r += " иммунитет к критам;";
        if (b.wSilver) r += " посеребрённое оружие;";
        if (b.lightR) r += " свет " + std::to_string(b.lightR) + " фт;";
        if (b.unarmoredStat >= 0) r += std::string(" без доспеха + ") + SN[b.unarmoredStat] + ";";
        if (b.immPush) r += " иммунитет к отталкиванию;";
        if (b.redirect) r += " Перенаправление (реакция);";
        if (b.disengB) r += " Отход бонусным действием;";
        if (b.legAct) {
            std::snprintf(buf, sizeof buf, " легендарные действия %d оч.;", b.legAct);
            r += buf;
        }
        if (b.legRes) {
            std::snprintf(buf, sizeof buf, " легендарное сопротивление %d/бой;", b.legRes);
            r += buf;
        }
        if (b.fly) r += " полёт;";
        if (b.speed != 30) {
            std::snprintf(buf, sizeof buf, " скорость %d фт;", b.speed);
            r += buf;
        }
        for (int k = 0; k < STAT_N; k++)
            if ((b.saveProf >> k) & 1u) {
                std::snprintf(buf, sizeof buf, " владение спасброском %s;", SN[k]);
                r += buf;
            }
        for (int k = 1; k < C_N; k++)
            if (b.immCond & (1u << k)) {
                std::snprintf(buf, sizeof buf, " иммунитет: %s;", CN[k]);
                r += buf;
            }
        r += "\n";
    }
    for (auto& s : b.abil) r += "  Способность: " + describe(s) + "\n";
    for (size_t t = 0; t < b.tmpl.size(); t++) {
        std::snprintf(buf, sizeof buf, "  Шаблон приспешника %zu (стоимость %lld):\n", t + 1, b.tmpl[t].cost);
        r += buf;
        r += describeBoss(b.tmpl[t]);
    }
    return r;
}
