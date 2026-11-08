// ============================================================================
// Экспорт боя для просмотрщика (viewer_template.html): боссы с разбором цены «по покупкам», карта, кадры боя.
// Подключается из boss_arena_v20.cc после describeBoss. Данные зашиваются в один HTML-файл, он открывается двойным кликом без сервера.
// ============================================================================
#pragma once
#include <unistd.h>
#include "mini_json.h"

// Покупка босса: часть (cat) для значка и порядка показа, название, пояснение, цена в очках
struct BuyItem {
    std::string cat, label, detail;
    long long cost = 0;
};

static const char* DT_ADJ[DT_N] = {"Кислотный", "Дробящий", "Ледяной", "Огненный", "Силовой", "Грозовой", "Некротический", "Колющий", "Ядовитый", "Психический", "Лучистый", "Рубящий", "Звуковой"};

// Название способности по её свойствам (способности безымянные, а просмотрщику нужны имена)
static std::string specTitle(const Spec& s) {
    if (!s.name.empty()) return s.name;
    if (s.offhand) return "Атака второй рукой";
    if (s.onHit) return s.pool ? "Приём" : "Удар при попадании";
    if (s.self) {
        if (s.healN) return "Исцеление";
        if (s.tempN) return "Каменная кожа";
        if (s.acB) return "Щит из воздуха";
        if (s.sbAmt) return "Прилив силы";
        if (s.buffN) return "Заточка оружия";
        if (s.darkR) return "Покров тьмы";
        if (s.invisN) return "Невидимость";
        if (s.hide) return "Укрытие";
        if (s.teleFt) return "Прыжок сквозь пространство";
        if (s.terr) return "Колючая поросль";
        return "Особое умение";
    }
    if (s.wallLen > 0) return "Барьер";
    if (s.ground && s.terr && !(s.dmgN || s.cond || s.pushFt || s.dotN)) return "Заросли";
    std::string noun;
    if (s.radius > 0) noun = s.range > 5 ? "взрыв" : "волна";
    else if (s.range > 5) noun = s.delivery == 2 ? "луч" : "залп";
    else noun = "удар";
    std::string t;
    if (s.dmgN > 0) t = std::string(DT_ADJ[s.dtype]) + " " + noun;
    else {
        switch (s.cond) {
            case C_PARAL: t = "Паралич"; break;
            case C_STUN: t = "Оглушающий " + noun; break;
            case C_RESTR: t = "Путы"; break;
            case C_GRAPPLE: t = "Хватка"; break;
            case C_FRIGHT: t = "Ужасающий рёв"; break;
            case C_POISON: t = "Отравляющий " + noun; break;
            case C_BLIND: t = "Слепящая вспышка"; break;
            case C_SLOW: t = "Замедляющий " + noun; break;
            case C_PRONE: t = "Подсечка"; break;
            case C_CHARM: t = "Очарование"; break;
            case C_WEAK: t = "Ослабляющий " + noun; break;
            default: t = s.pushFt ? "Мощный толчок" : "Заклятие"; break;
        }
    }
    if (s.dmgN > 0 && s.cond > 0 && s.cond != C_PRONE) t += " (" + std::string(CN[s.cond]) + ")";
    if (s.pushFt > 0 && s.dmgN > 0) t += " с отбросом";
    if (s.ground && s.dmgN > 0) t += " по клетке";
    return t;
}

// Имя босса по приметам (детерминированно): прилагательное + существительное
static std::string bossTitle(const Boss& b, unsigned salt) {
    static const char* NOUN_STR[3] = {"Громила", "Крушитель", "Страж"};
    static const char* NOUN_DEX[3] = {"Клинок", "Охотник", "Налётчик"};
    static const char* NOUN_INT[3] = {"Чародей", "Колдун", "Маг"};
    static const char* NOUN_WIS[3] = {"Жрец", "Провидец", "Хранитель"};
    static const char* NOUN_CHA[3] = {"Властелин", "Тиран", "Владыка"};
    const char* n;
    const unsigned h = salt * 2654435761u + (unsigned)b.hp * 40503u + (unsigned)b.stats[b.mainStat];
    switch (b.mainStat) {
        case STR: n = NOUN_STR[h % 3]; break;
        case DEX: n = NOUN_DEX[h % 3]; break;
        case INT: n = NOUN_INT[h % 3]; break;
        case WIS: n = NOUN_WIS[h % 3]; break;
        default: n = NOUN_CHA[h % 3]; break;
    }
    std::string adj;
    if (hasSummonSpec(b)) adj = "Повелевающий";
    else if (b.fly) adj = "Крылатый";
    else if (b.regenN) adj = "Неумирающий";
    else if (b.resNM || b.immNM) adj = "Каменный";
    else if (b.auraRad > 0) adj = "Пылающий";
    else if (b.wRange > 5) adj = "Меткий";
    else if (b.parry || b.uncanny) adj = "Неуловимый";
    else if (b.hp > 400) adj = "Исполинский";
    else if (b.abil.size() >= 4) adj = "Многоликий";
    else adj = "Свирепый";
    // согласование рода: женский/средний не встречаются, но -ий/-ый/-щий мужского рода подходят всем существительным выше
    return adj + " " + n;
}

// Разбор цены босса «по покупкам» (независимый пересчёт, как priceOf в тестах: сумма обязана совпасть с ценой босса)
static std::vector<BuyItem> bossItems(const Boss& b, const Coef& C, double budget) {
    const double prevB = g_B;
    g_B = budget + STAT_BASE;   // budget — видимый (характеристики от 10), внутри — абсолютный
    PcGuard pcg(b);             // цены оружия и длящихся эффектов — по этому телу
    std::vector<BuyItem> v;
    auto P = [&](int a, double m = 1.0) { return C[a] * m * scl(a); };
    auto add = [&](const char* cat, const std::string& label, const std::string& detail, long long cost) { v.push_back({cat, label, detail, cost}); };
    auto cp = [&](const char* cat, const std::string& label, const std::string& detail, double x) { add(cat, label, detail, (long long)std::ceil(x)); };
    char buf[200];
    for (int i = 0; i < STAT_N; i++) {
        std::snprintf(buf, sizeof buf, "модификатор %+d; %s", modOf(b.stats[i]), b.stats[i] > STAT_ZERO ? "выше десяти: платим" : (b.stats[i] < STAT_ZERO ? "ниже десяти: очки возвращаются" : "десять бесплатно"));
        add("stat", std::string(SN[i]) + " " + std::to_string(b.stats[i]), buf, statCost(b.stats[i]));
    }
    std::snprintf(buf, sizeof buf, "%d единиц КД (по +1 КД)", b.acV);
    add("ac", "КД " + std::to_string(b.ac), buf, acCost(b.acV));
    // доспехи и щит (v21): ЛОВ к КД по доспеху; цена доспеха — возврат за срезанную ЛОВ, щит, помеха к Скрытности, без владения
    if (b.size != 2) add("ac", std::string("Размер: ") + SZN[std::clamp(b.size, 0, 5)], b.size > 2 ? "схватить и толкнуть могут лишь существа на ступень меньше" : "тяжёлое оружие — с помехой", sizePrice(b, C));
    if (b.armor || b.shield || unarmoredBonus(b) > 0) {
        std::string d = std::string(ARN[std::clamp(b.armor, 0, AR_N - 1)]) + ", ЛОВ к КД " + std::to_string(dexAC(b)) + (b.shield ? ", щит +2" : "");
        if (unarmoredBonus(b) > 0) d += std::string(", без доспеха + ") + SN[b.unarmoredStat] + " " + std::to_string(unarmoredBonus(b));
        if (b.armStealth && b.armor) d += ", помеха к Скрытности";
        if (b.noArmProf) d += ", без владения";
        add("ac", "Доспех и щит", d, armorPrice(b, C));
    }
    auto wpName = [&](int n, int sd, int range, int att, int reach, bool finesse, int type) {
        std::string d = std::to_string(n) + "к" + std::to_string(sd) + " " + DTN[type] + ", ";
        d += range > 5 ? "дальнее " + std::to_string(range) + " фт" : (reach > 5 ? "ближнее, досягаемость " + std::to_string(reach) + " фт" : "ближнее");
        if (att > 1) d += ", атак за действие: " + std::to_string(att);
        if (finesse) d += ", фехтовальное";
        return d;
    };
    if (b.wN > 0) add("weapon", b.wNatural ? "Естественное оружие" : "Оружие", wpName(b.wN, b.wS, b.wRange, b.wAtt, b.reach, b.wFinesse, b.wType), wpCostOf(C, b.wN, b.wS, b.wRange, b.reach, b.wAtt, b.wFinesse, b.wAmmo));
    if (b.wN > 0 && b.wLong > b.wRange && b.wRange > 5) add("weapon", "Дальняя дистанция", "до " + std::to_string(b.wLong) + " фт, дальше " + std::to_string(b.wRange) + " фт — с помехой", weaponLongPrice(C, b.wN, b.wS, b.wRange, b.wLong, b.reach, b.wAtt));
    if (b.wN > 0 && (b.wTwoH || b.wVers > b.wS)) add("weapon", b.wTwoH ? "Двуручное оружие" : "Универсальное оружие", b.wTwoH ? "обе руки заняты: без щита и второй руки, очки возвращаются" : "двумя руками (без щита и второй руки) к" + std::to_string(b.wVers), weaponHandsPrice(b, C));
    if (b.poolN > 0) add("weapon", "Запас костей", std::to_string(b.poolN) + "к" + std::to_string(b.poolS) + " на приёмы (цена — в приёмах)", 0);
    if (b.rN > 0 || b.rCond > 0) {
        double q = riderGrabPrice(b, C);   // захват: опутан, урон пока держит (пакет атомов 2)
        std::string d;
        if (b.rN > 0) {
            q += C[AT_RDMG] * b.rN * (b.rS + 1) / 2.0;
            d += "+" + std::to_string(b.rN) + "к" + std::to_string(b.rS) + " " + DTN[b.rType];
        }
        if (b.rCond > 0) {
            double dm = (b.rCond == C_PRONE) ? 1.0 : 1 + 0.25 * (b.rRounds - 1);
            q += C[AT_PRONE + b.rCond - 1] * dm * scl(AT_PRONE + b.rCond - 1) * C[AT_RCOND];
            d += std::string(d.empty() ? "" : " и ") + CN[b.rCond] + " на " + std::to_string(b.rRounds) + " р.";
        }
        cp("weapon", "Довесок к удару", d + ", раз в ход", q);
    }
    for (auto& q : b.alt) add("weapon", q.multi ? "Оружие мультиатаки" : "Запасное оружие", wpName(q.n, q.s, q.range, q.att, q.reach, q.finesse, q.type), wpPrice(q, C));
    for (auto& s : b.abil) add("ability", specTitle(s), describe(s), specPrice(s, C));
    if (b.abil.size() > 3) cp("ability", "Наценка за число способностей", "способностей больше трёх: " + std::to_string(b.abil.size()), P(AT_NABIL, tierN(TR_NABIL, (double)(b.abil.size() - 3))));
    if (b.regenN) cp("passive", "Регенерация", std::to_string(b.regenN) + "к6 в начале хода", P(AT_REGEN, b.regenN) * suppMul(b, C, 1));
    for (int d = 0; d < DT_N; d++) {
        if ((b.resMask >> d) & 1) cp("passive", std::string("Резист: ") + DTN[d], "урон вдвое меньше", P(AT_RESIST, 5.0 * g_share[d]) * suppMul(b, C, 4));
        if ((b.immMask >> d) & 1) cp("passive", std::string("Иммунитет: ") + DTN[d], "урон не проходит", P(AT_IMMTYPE, 5.0 * g_share[d]) * suppMul(b, C, 4));
        if ((b.vulnMask >> d) & 1) add("passive", std::string("Уязвимость: ") + DTN[d], "урон вдвое больше, очки возвращаются", -(long long)std::ceil(P(AT_VULN, 5.0 * g_share[d])));
    }
    if (b.resNM) cp("passive", "Резист к немагическому физическому", "обычное оружие бьёт вдвое слабее", P(AT_RESNM, 5.0 * g_nmShare) * suppMul(b, C, 4));
    if (b.immNM) cp("passive", "Иммунитет к немагическому физическому", "обычное оружие не берёт", P(AT_IMMNM, 5.0 * g_nmShare) * suppMul(b, C, 4));
    if (b.wMagic) cp("passive", "Магическое оружие", "пробивает защиту от немагического", P(AT_WMAGIC));
    if (b.wSilver && !b.wMagic) cp("passive", "Посеребрённое оружие", "пробивает сопротивление немагическому", P(AT_WMAGIC) * C[AT_WSILVER]);
    if (b.lightR > 0) cp("passive", "Свет вокруг себя", std::to_string(b.lightR) + " фт: ночью освещённое видно издалека", C[AT_LIGHT] * b.lightR / 10.0);
    if (b.legRes) cp("passive", "Легендарное сопротивление", std::to_string(b.legRes) + " раз за бой", P(AT_LEGRES, b.legRes));
    if (b.legAct) cp("passive", "Легендарные действия", std::to_string(b.legAct) + " очк. между ходами врага", P(AT_LEGACT, b.legAct));
    if (b.parry) cp("passive", "Парирование", "+" + std::to_string(b.parry) + " к КД реакцией", P(AT_PARRY, b.parry));
    if (b.uncanny) cp("passive", "Невероятное уклонение", "половина урона реакцией", P(AT_UNCANNY));
    if (b.magicRes) cp("passive", "Сопротивление магии", "преимущество на спасброски от способностей", P(AT_MAGICRES));
    if (b.spellRes) cp("passive", "Сопротивление урону от способностей", "урон способностей вдвое меньше (с резистом к типу не складывается)", P(AT_SPELLRES));
    if (b.critImm) cp("passive", "Иммунитет к критам", "критическое попадание по нему — обычное", P(AT_CRITIMM));
    if (b.immPush) cp("passive", "Неподвижность", "иммунитет к отталкиванию", P(AT_IMMPUSH));
    if (b.disengB) cp("passive", "Отход бонусным действием", "", P(AT_DISENGB));
    if (b.redirect) cp("passive", "Перенаправление", "реакцией подставляет приспешника", P(AT_REDIRECT));
    for (auto& r : b.rules) add("passive", "Правило", describeRule(r), rulePrice(r, b, C));   // «когда → что»
    for (auto& t : b.trigs) add("passive", "Событие", describeTrig(t), trigPrice(t, b, C));   // «событие → эффект» (v21)
    for (int k = 1; k < C_N; k++)
        if (b.immCond & (1u << k)) cp("passive", std::string("Иммунитет: ") + CN[k], "", P(AT_IMMCOND));
    for (int st = 0; st < STAT_N; st++)
        if (b.saveProf & (1u << st)) cp("passive", std::string("Владение спасброском ") + SN[st], "+" + std::to_string(b.prof) + " к спасброску", P(AT_SAVEPROF, b.prof / 2.0));
    // владения (v21): бонус мастерства, навыки, инструменты, без владения своим оружием
    if (b.prof > 2) cp("passive", "Бонус мастерства +" + std::to_string(b.prof), "к атакам, СЛ и всему, чем владеешь (+2 даром)", P(AT_PROF, b.prof - 2));
    for (int k = 0; k < SK_N; k++) {
        if (!((b.skillProf >> k) & 1u)) continue;
        const int at = skCombat(k) ? AT_SKILLC : AT_SKILLNC;
        const double m = skCombat(k) ? b.prof / 2.0 : 1.0;
        cp("passive", std::string("Навык: ") + SKN[k], std::string("+") + std::to_string(b.prof) + (skCombat(k) ? " (в бою)" : " (вне боя)"), P(at, m));
        if ((b.skillExp >> k) & 1u) cp("passive", std::string("Экспертиза: ") + SKN[k], "мастерство ещё раз", P(at, m));
    }
    if (b.toolN > 0) cp("passive", "Владение инструментами", "×" + std::to_string(b.toolN) + " (вне боя)", P(AT_SKILLNC, b.toolN));
    if (b.wNoProf && b.wN > 0) add("weapon", "Без владения своим оружием", "мастерство не идёт к атакам оружием, очки возвращаются", -(long long)std::ceil(P(AT_NOWPROF, b.prof / 2.0)));
    if (b.fly) cp("passive", "Полёт", "", P(AT_FLY));
    if (b.speed > 0) cp("passive", "Скорость", std::to_string(b.speed) + " фт (покупается вся)", P(AT_SPEED, speedUnits(b.speed)));
    if (b.riposte) cp("passive", "Контратака", "ответный удар реакцией", P(AT_RIPOSTE));
    if (b.surge) cp("passive", "Всплеск действий", "×" + std::to_string(b.surge), P(AT_SURGE, b.surge));
    for (int L = 1; L < 10; L++)
        if (b.slots[L]) cp("passive", "Ячейки заклинаний " + std::to_string(L) + " ур.", "×" + std::to_string(b.slots[L]), C[AT_SLOT] * L * b.slots[L]);
    if (b.dv) cp("passive", "Тёмное зрение", std::to_string(b.dv) + " фт", P(AT_DARKV, b.dv == 30 ? 1 : (b.dv == 60 ? 2 : 3)));
    if (b.truesight) cp("passive", "Истинное зрение", std::to_string(b.truesight) + " фт: видит невидимых и сквозь тьму", P(AT_TRUESIGHT, b.truesight <= 10 ? 1 : (b.truesight <= 30 ? 2 : (b.truesight <= 60 ? 3 : 4))));
    if (b.blindsight) cp("passive", "Слепое зрение", std::to_string(b.blindsight) + " фт", P(AT_BLINDS, b.blindsight == 10 ? 1 : (b.blindsight == 30 ? 2 : 3)));
    if (b.ignDiff) cp("passive", "Лёгкая поступь", "игнорирует труднопроходимую местность", P(AT_IGNTERR));
    if (b.climbS) cp("passive", "Скорость лазания", "лазалка и подъём без надбавки, обрывы — как по лестнице", P(AT_CLIMB));
    if (b.swimS) cp("passive", "Скорость плавания", "вода не труднопроходима", P(AT_SWIM));
    if (b.burrowS) cp("passive", "Скорость копания", "сквозь ущелья и обрывы под землёй, местность не мешает", P(AT_BURROW));
    if (b.lair) cp("passive", "Действия логова", "уровень " + std::to_string(b.lair) + ": в начале раунда обвал (" + std::to_string(2 * b.lair) + "к10) или трясина", P(AT_LAIR, b.lair));
    if (b.evasion) cp("passive", "Увёртливость", "спасбросок ЛОВ от области: успех — без урона, провал — половина", P(AT_EVASION));
    if (b.sunSens) add("passive", "Солнечная чувствительность", "на солнце атаки с помехой, очки возвращаются", -(long long)std::ceil(P(AT_SUNSENS)));
    if (b.auraRad > 0) {
        double q = 0;
        std::string d = "радиус " + std::to_string(b.auraRad) + " фт, ";
        if (b.auraN > 0) {
            q += C[AT_AURA] * tierN(TR_AURADICE, b.auraN * (b.auraS + 1) / 7.0) * 3.5;
            d += std::to_string(b.auraN) + "к" + std::to_string(b.auraS) + " " + DTN[b.auraType];
        }
        if (b.auraCond > 0) {
            double dm = (b.auraCond == C_PRONE) ? 1.0 : 1 + 0.25 * (b.auraRounds - 1);
            q += C[AT_PRONE + b.auraCond - 1] * dm * scl(AT_PRONE + b.auraCond - 1) * C[AT_AURACOND];
            d += std::string(b.auraN > 0 ? " и " : "") + CN[b.auraCond];
        }
        q *= 1 + C[AT_AURARAD] * tierN(TR_AURARAD, b.auraRad / 5.0 - 1);
        cp("aura", "Аура", d, q * suppMul(b, C, 2));
    }
    add("hp", "Хиты " + std::to_string(b.hp), "2 очка за каждый ХП: всё, что осталось, идёт в здоровье", 2LL * b.hp);
    g_B = prevB;
    return v;
}

static std::string bossViewJson(const Boss& b, const Coef& C, double budget, const std::string& title, bool withMinions = true) {
    std::string o = "{\"title\":" + jq(title) + ",\"budget\":" + std::to_string((long long)budget) + ",\"stats\":[";
    for (int i = 0; i < STAT_N; i++) o += (i ? "," : "") + std::to_string(b.stats[i]);
    long long total = 0;
    std::string items = "[";
    auto its = bossItems(b, C, budget);
    for (size_t i = 0; i < its.size(); i++) {
        total += its[i].cost;
        items += std::string(i ? "," : "") + "{\"cat\":" + jq(its[i].cat) + ",\"label\":" + jq(its[i].label) + ",\"detail\":" + jq(its[i].detail) + ",\"cost\":" + std::to_string(its[i].cost) + "}";
    }
    items += "]";
    o += "],\"ac\":" + std::to_string(b.ac) + ",\"hp\":" + std::to_string(b.hp) + ",\"int\":" + std::to_string(b.stats[INT]) + ",\"main\":" + jq(SN[b.mainStat]) +
         ",\"speed\":" + std::to_string(b.speed) + ",\"fly\":" + (b.fly ? "true" : "false") + ",\"total\":" + std::to_string(total) + ",\"items\":" + items +
         ",\"wType\":" + std::to_string(b.wType) + ",\"wRange\":" + std::to_string(b.wRange) + ",\"reach\":" + std::to_string(b.reach) + ",\"auraRad\":" + std::to_string(b.auraRad) + ",\"abil\":[";
    for (size_t i = 0; i < b.abil.size(); i++)
        o += std::string(i ? "," : "") + "{\"title\":" + jq(specTitle(b.abil[i])) + ",\"desc\":" + jq(describe(b.abil[i])) + ",\"range\":" + std::to_string(b.abil[i].range) +
             ",\"radius\":" + std::to_string(b.abil[i].radius) + ",\"self\":" + (b.abil[i].self ? "true" : "false") + ",\"ground\":" + (b.abil[i].ground ? "true" : "false") + ",\"wallLen\":" + std::to_string(b.abil[i].wallLen) + ",\"react\":" + std::to_string(b.abil[i].react) + ",\"dtype\":" + std::to_string(b.abil[i].dtype) + "}";
    o += "],\"text\":" + jq(describeBoss(b));
    // призыв — способность (v21): шаблон первой призывающей способности, копий за одно применение и число таких способностей
    if (withMinions && hasSummonSpec(b)) {
        int perCast = 0, nSum = 0, t0 = -1;
        for (auto& s : b.abil)
            if (s.sumN > 0) perCast = perCast ? perCast : s.sumN, t0 = t0 >= 0 ? t0 : s.sumTmpl, nSum++;
        o += ",\"minion\":" + bossViewJson(b.tmpl[std::max(0, t0)], C, budget, "Приспешник", false) + ",\"minionCount\":" + std::to_string(perCast) + ",\"summonCap\":" + std::to_string(perCast) +
             ",\"summonAbil\":" + std::to_string(nSum);
    }
    for (auto& s : b.abil)   // превращение (v21): облик
        if (withMinions && s.formTmpl >= 0 && s.formTmpl < (int)b.tmpl.size()) {
            o += ",\"form\":" + bossViewJson(b.tmpl[s.formTmpl], C, budget, "Облик", false);
            break;
        }
    return o + "}";
}

// Шаблон ищется рядом с программой, в текущей папке или по переменной DND_VIEWER
static std::string findViewerTemplate() {
    std::vector<std::string> cand;
    if (const char* e = std::getenv("DND_VIEWER")) cand.push_back(e);
    cand.push_back(dndPath("viewer_template.html"));
    char exe[1024];
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) {
        exe[n] = 0;
        std::string d = exe;
        d = d.substr(0, d.find_last_of('/') + 1);
        cand.push_back(d + "viewer_template.html");
    }
    for (auto& c : cand) {
        std::ifstream f(c, std::ios::binary);
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    return "";
}

// Запись просмотрщика: боссы (с покупками), карта, кадры боя, сведения о бое (g_fightInfo)
static void writeViewerHTML(const std::string& path, const Boss& A, const Boss& B, const Coef& C, double budget, const std::string& builderA, const std::string& builderB,
                            unsigned seed, const std::string& mapJson, const std::vector<std::string>& frames) {
    std::string tpl = findViewerTemplate();
    if (tpl.empty()) {
        std::printf("Не найден viewer_template.html (положите его рядом с программой или задайте DND_VIEWER)\n");
        return;
    }
    std::string d = "const REPLAY = {\"seed\":" + std::to_string(seed) + ",\"budget\":" + std::to_string((long long)budget) + ",\"bosses\":[" +
                    bossViewJson(A, C, budget, bossTitle(A, seed)) + "," + bossViewJson(B, C, budget, bossTitle(B, seed + 7)) + "],\"builders\":[" + jq(builderA) + "," + jq(builderB) +
                    "],\"ctrl\":[" + (g_fightInfo.ctrl[0] ? "true" : "false") + "," + (g_fightInfo.ctrl[1] ? "true" : "false") + "],\"result\":" + std::to_string(g_fightInfo.result) +
                    ",\"reason\":" + std::to_string(g_fightInfo.reason) + ",\"rounds\":" + std::to_string(g_fightInfo.rounds) + ",\"map\":" + mapJson + ",\"frames\":[\n";
    for (size_t i = 0; i < frames.size(); i++) d += frames[i] + (i + 1 < frames.size() ? ",\n" : "\n");
    d += "]};\n";
    // живой просмотр обучения (live_replay.sh): DND_LIVE=секунд — после конца боя страница ждёт столько и перезагружается
    // (скрипт к тому времени кладёт в тот же файл новый бой текущих весов)
    if (const char* lv = std::getenv("DND_LIVE")) {
        char tb[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(tb, sizeof tb, "%H:%M:%S", std::localtime(&now));
        d += "const LIVE = " + std::to_string(std::max(1, std::atoi(lv))) + ", LIVE_AT = \"" + tb + "\";\n";
    }
    const std::string mark = "/*REPLAY_DATA*/";
    const size_t at = tpl.find(mark);
    if (at == std::string::npos) {
        std::printf("В шаблоне нет метки %s\n", mark.c_str());
        return;
    }
    tpl.replace(at, mark.size(), d);
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        std::printf("Не удалось создать файл %s\n", path.c_str());
        return;
    }
    std::fwrite(tpl.data(), 1, tpl.size(), f);
    std::fclose(f);
    std::printf("Просмотрщик записан: %s (%zu кадров) — открой в браузере.\n", path.c_str(), frames.size());
}
