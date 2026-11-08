// ---- группы эффектов: показаны только выбранные, остальные — кнопками «+ …» (OPEN — добавленные, но ещё пустые) ----
const OPEN = new Set(), GREG = {};
function groupsHTML(base, obj, groups, rer){
  let h = '', add = '';
  for (const g of groups){
    const key = base + '|' + g.id;
    GREG[key] = {g, obj, rer};
    if (g.always || (g.has && g.has(obj)) || OPEN.has(key))
      h += `<div class="eff"><div class="eff-h">${esc(g.n)}${g.always ? '' : `<button class="x" data-gdel="${key}" title="Убрать">✕</button>`}</div>${g.html}</div>`;
    else add += `<button class="add" data-gadd="${key}">+ ${esc(g.n)}</button>`;
  }
  return h + (add ? `<div class="adds">${add}</div>` : '');
}
const any = (a, ks) => ks.some(k => +a[k] > 0 || a[k] === true);
const zero = (a, ks) => ks.forEach(k => { if (typeof a[k] === 'boolean') a[k] = false; else if (a[k] != null) a[k] = 0; });
const G2 = s => `<div class="grid2">${s}</div>`;
function abilGroups(a, k, P, dt, cd){
  const SAVE = `<label>Спасбросок цели ${selIn(P('saveStat'), STAT_N.map((n, j) => [j, n]))}</label>`;
  const COND = {id:'cond', n:'Состояние', has:a => +a.cond > 0, init:a => { a.cond = 1; a.condRounds = 1; }, clear:a => zero(a, ['cond']),
    html:G2(`<label>Состояние ${selIn(P('cond'), cd)}</label><label>раундов ${numIn(P('condRounds'), 1, LIM.rounds)}</label>${+a.cond === CAT.conds.indexOf('схвачен') ? `${chkIn(P('grabRestr'), 'Схваченный опутан')}${chkIn(P('grabDot'), 'Урон каждый ход, пока держит')}${a.grabDot ? `<label>кубиков ${numIn(P('dotN'), 0, LIM.dice)}</label><label>грань ${selIn(P('dotS'), DIE_OPTS())}</label>` : ''}` : ''}`)};
  const TIMING = {id:'timing', n:'Как применяется', always:true,
    html:G2(`${chkIn(P('bonus'), 'Бонусное действие')}${chkIn(P('inAttack'), 'Вместо одной атаки действия «Атака» (дракон: страх, затем когти)')}${k === 'self' ? chkIn(P('preCast'), 'Действует с начала боя (наложено заранее)') : ''}<label>Реакция (срабатывает сама) ${selIn(P('react'), [[0,'нет — обычное действие'],[1,'когда меня атакуют'],[2,'когда мне нанесли урон'],[3,'когда враг вошёл в зону'],[4,'когда враг применил способность в зоне']])}</label>`)};
  const COST = {id:'cost', n:'Ограничения (дешевле)', has:a => any(a, ['uses', 'recharge', 'slotLvl', 'conc', 'noRepeat', 'immSave']), init:a => { a.uses = 1; }, clear:a => zero(a, ['uses', 'recharge', 'slotLvl', 'conc', 'noRepeat', 'immSave']),
    html:G2(`<label>Применений за бой (пусто — ∞, без ограничения) ${usesIn(P('uses'))}</label><label>Перезарядка ${selIn(P('recharge'), [[0,'нет'],[6,'6'],[5,'5–6'],[4,'4–6']])}</label>
      <label>Ячейка заклинания, ур. (0 — нет) ${numIn(P('slotLvl'), 0, 9)}</label>${chkIn(P('conc'), 'Концентрация')}${chkIn(P('noRepeat'), 'Не два раунда подряд')}${chkIn(P('rechAfter'), 'Кончились применения — перезарядка возвращает все')}${k === 'save' ? chkIn(P('immSave'), 'После успешного спасброска цель к ней невосприимчива') : ''}`)};
  if (k === 'self') return [
    {id:'ally', n:'И на союзника', has:a => !!a.ally, init:a => { a.ally = true; a.range = a.range || 30; }, clear:a => { a.ally = false; },
      html:G2(`${chkIn(P('ally'), 'И на союзника (лечение, хиты, баффы, невидимость)')}<label>дальность до союзника, фт ${numIn(P('range'), 5, LIM.ft, 5)}</label>`)},
    {id:'heal', n:'Лечение', has:a => any(a, ['healN', 'healB']), init:a => { a.healN = 1; a.healS = 8; }, clear:a => zero(a, ['healN', 'healB']),
      html:G2(`<label>кубиков ${numIn(P('healN'), 0, LIM.dice)}</label><label>грань ${selIn(P('healS'), DIE_OPTS())}</label><label>+ к лечению ${numIn(P('healB'), 0, 1000)}</label>`)},
    {id:'temp', n:'Временные хиты', has:a => +a.tempN > 0, init:a => { a.tempN = 1; }, clear:a => zero(a, ['tempN']), html:G2(`<label>к6 ${numIn(P('tempN'), 0, LIM.dice)}</label>`)},
    {id:'ac', n:'Бонус КД', has:a => +a.acB > 0, init:a => { a.acB = 2; a.acRounds = 10; }, clear:a => zero(a, ['acB', 'acRounds']),
      html:G2(`<label>+КД ${numIn(P('acB'), 0, LIM.count)}</label><label>раундов ${numIn(P('acRounds'), 0, LIM.rounds)}</label>`)},
    {id:'buff', n:'Бафф оружия', has:a => +a.buffN > 0, init:a => { a.buffN = 1; a.buffS = 6; a.buffRounds = 10; }, clear:a => zero(a, ['buffN', 'buffRounds']),
      html:G2(`<label>кубиков ${numIn(P('buffN'), 0, LIM.dice)}</label><label>грань ${selIn(P('buffS'), DIE_OPTS())}</label><label>раундов ${numIn(P('buffRounds'), 0, LIM.rounds)}</label>`)},
    {id:'sb', n:'Бафф характеристики', has:a => +a.sbAmt > 0, init:a => { a.sbAmt = 2; a.sbRounds = 10; }, clear:a => zero(a, ['sbAmt', 'sbRounds']),
      html:G2(`<label>характеристика ${selIn(P('sbStat'), STAT_N.map((n, j) => [j, n]))}</label><label>+ к модификатору ${numIn(P('sbAmt'), 0, LIM.count)}</label><label>раундов ${numIn(P('sbRounds'), 0, LIM.rounds)}</label>`)},
    {id:'tele', n:'Телепорт или прыжок', has:a => +a.teleFt > 0, init:a => { a.teleFt = 30; }, clear:a => zero(a, ['teleFt', 'jump']), html:G2(`<label>фт ${numIn(P('teleFt'), 0, LIM.ft, 10)}</label>${chkIn(P('jump'), 'Это прыжок: без действия, тратит движение, через ущелья и воду, но не сквозь стены')}`)},
    {id:'images', n:'Отражения (копии)', has:a => +a.images > 0, init:a => { a.images = 3; }, clear:a => zero(a, ['images']), html:G2(`<label>копий ${numIn(P('images'), 0, LIM.count)}</label><span class="small">атака может уйти в копию: 3 копии — на 6+ к20, 2 — 8+, 1 — 11+; попадание по копии её разрушает</span>`)},
    {id:'globe', n:'Сфера неуязвимости', has:a => +a.globeLvl > 0, init:a => { a.globeLvl = 5; a.globeRounds = 10; }, clear:a => zero(a, ['globeLvl', 'globeRounds']), html:G2(`<label>до уровня ячейки ${numIn(P('globeLvl'), 0, 9)}</label><label>раундов ${numIn(P('globeRounds'), 0, LIM.rounds)}</label><span class="small">10 фт вокруг: способности снаружи этого уровня и ниже не проходят (без ячейки — как 1-й)</span>`)},
    {id:'flyr', n:'Полёт на время', has:a => +a.flyRounds > 0, init:a => { a.flyRounds = 10; }, clear:a => zero(a, ['flyRounds']), html:G2(`<label>раундов ${numIn(P('flyRounds'), 0, LIM.rounds)}</label>`)},
    {id:'xturn', n:'Остановка времени (ходы подряд)', has:a => +a.extraTurns > 0, init:a => { a.extraTurns = 2; }, clear:a => zero(a, ['extraTurns']), html:G2(`<label>ещё ходов подряд ${numIn(P('extraTurns'), 0, 10)}</label>`)},
    {id:'grow', n:'Увеличение', has:a => +a.growRounds > 0, init:a => { a.growRounds = 10; }, clear:a => zero(a, ['growRounds']), html:G2(`<label>раундов ${numIn(P('growRounds'), 0, LIM.rounds)}</label><span class="small">размер +1 (если хватает места) и +1к4 к урону оружия</span>`)},
    {id:'light', n:'Свет', has:a => +a.lightR > 0, init:a => { a.lightR = 20; a.buffRounds = a.buffRounds || 10; }, clear:a => zero(a, ['lightR']), html:G2(`<label>радиус, фт ${numIn(P('lightR'), 0, LIM.ft, 10)}</label><span class="small">ночью без тёмного зрения неосвещённое видно лишь на 30 фт</span>`)},
    {id:'dark', n:'Тьма вокруг', has:a => +a.darkR > 0, init:a => { a.darkR = 10; }, clear:a => zero(a, ['darkR']), html:G2(`<label>радиус, фт ${numIn(P('darkR'), 0, LIM.ft, 10)}</label>`)},
    {id:'invis', n:'Невидимость', has:a => +a.invisN > 0, init:a => { a.invisN = 1; }, clear:a => zero(a, ['invisN']), html:G2(`<label>раундов ${numIn(P('invisN'), 0, LIM.rounds)}</label>`)},
    {id:'hide', n:'Укрытие (скрытность)', has:a => !!a.hide, init:a => { a.hide = true; }, clear:a => { a.hide = false; }, html:G2(chkIn(P('hide'), 'Укрытие (скрытность)'))},
    TIMING, COST];
  if (k === 'onhit') return [
    {id:'base', n:'Приём', always:true, html:`<div class="hint" style="margin:4px 0">Вы атакуете оружием (действие Атака), и на первое попадание ложатся эффекты ниже. Не попали — приём не потрачен. Запас костей задаётся в «Оружии».</div>`},
    {id:'dmg', n:'Доп. урон', has:a => +a.dmgN > 0, init:a => { a.dmgN = 1; a.dmgS = 6; }, clear:a => zero(a, ['dmgN']),
      html:G2(`<label>кубиков ${numIn(P('dmgN'), 0, LIM.dice)}</label><label>грань ${selIn(P('dmgS'), DIE_OPTS())}</label><label>тип ${dtSel(P('dtype'))}</label>`)},
    {id:'pool', n:'Кость запаса к урону', has:a => !!a.pool, init:a => { a.pool = true; }, clear:a => { a.pool = false; }, html:G2(chkIn(P('pool'), 'Тратит кость запаса (к урону)'))},
    COND,
    {id:'save', n:'Цель спасается', has:a => !!a._ohsave, init:a => { a._ohsave = true; }, clear:a => { a._ohsave = false; },
      html:G2(`${chkIn(P('_ohsave'), 'Цель спасается от эффекта')}<label>спасбросок ${selIn(P('saveStat'), STAT_N.map((n, j) => [j, n]))}</label>`)},
    {id:'disarm', n:'Обезоруживание', has:a => !!a.disarm, init:a => { a.disarm = true; }, clear:a => { a.disarm = false; }, html:G2(chkIn(P('disarm'), 'Обезоруживает'))},
    {id:'push', n:'Толчок', has:a => +a.pushFt > 0, init:a => { a.pushFt = 10; }, clear:a => zero(a, ['pushFt']), html:G2(`<label>фт ${numIn(P('pushFt'), 0, LIM.ft, 5)}</label>`)},
    {id:'selfd', n:'Урон себе', has:a => +a.selfN > 0, init:a => { a.selfN = 1; a.selfS = 6; }, clear:a => zero(a, ['selfN']),
      html:G2(`<label>кубиков ${numIn(P('selfN'), 0, LIM.dice)}</label><label>грань ${selIn(P('selfS'), DIE_OPTS())}</label>`)},
    {id:'kill', n:'Убил — лечение себе', has:a => any(a, ['killN', 'killB']), init:a => { a.killN = 1; a.killS = 8; }, clear:a => zero(a, ['killN', 'killB']),
      html:G2(`<label>кубиков ${numIn(P('killN'), 0, LIM.dice)}</label><label>грань ${selIn(P('killS'), DIE_OPTS())}</label><label>+ ${numIn(P('killB'), 0, 1000)}</label>`)},
    TIMING, COST];
  // атака, спасбросок, без броска
  return [
    {id:'base', n:'Основное', always:true, html:G2(`<label>Дальность, фт ${numIn(P('range'), 5, LIM.ft, 5)}</label>${k === 'save' ? SAVE : ''}
      <label>Урон: кубиков ${numIn(P('dmgN'), 0, LIM.dice)}</label><label>грань ${selIn(P('dmgS'), DIE_OPTS())}</label><label>тип урона ${dtSel(P('dtype'))}</label>`)},
    {id:'area', n:'Область', has:a => +a.radius > 0, init:a => { a.radius = 10; }, clear:a => { zero(a, ['radius', 'shape', 'selective']); },
      html:G2(`<label>Форма ${selIn(P('shape'), [[0,'сфера (круг)'],[1,'куб (квадрат)'],[2,'конус от себя'],[3,'линия от себя']])}</label>
        <label>${+a.shape >= 2 ? 'Длина' : (+a.shape === 1 ? 'Полусторона' : 'Радиус')}, фт ${numIn(P('radius'), 0, LIM.ft, 5)}</label>
        ${chkIn(P('selective'), 'Избирательная: бьёт только врагов (иначе задевает и своих, и вас)')}`)},
    {id:'targets', n:'Несколько целей', has:a => +a.targets > 1, init:a => { a.targets = 2; }, clear:a => { a.targets = 1; },
      html:G2(`<label>Целей на выбор (без области) ${numIn(P('targets'), 1, 10, 1, 56)}</label>`)},
    COND,
    {id:'move', n:'Толчок и притягивание', has:a => any(a, ['pushFt', 'pullFt', 'collN']), init:a => { a.pushFt = 10; }, clear:a => zero(a, ['pushFt', 'pullFt', 'collN']),
      html:G2(`<label>Толчок, фт ${numIn(P('pushFt'), 0, LIM.ft, 5)}</label><label>Урон при столкновении, к6 ${numIn(P('collN'), 0, LIM.dice)}</label><label>Притягивание, фт ${numIn(P('pullFt'), 0, LIM.ft, 5)}</label>`)},
    {id:'dot', n:'Урон по времени', has:a => +a.dotN > 0, init:a => { a.dotN = 1; a.dotRounds = 2; }, clear:a => zero(a, ['dotN', 'dotRounds']),
      html:G2(`<label>кубиков ${numIn(P('dotN'), 0, LIM.dice)}</label><label>раундов ${numIn(P('dotRounds'), 0, LIM.rounds)}</label>`)},
    {id:'more', n:'Доп. урон другого типа', has:a => Object.values(a.more || {}).some(m => m && +m.n > 0), init:a => { a.more = {0:{n:1, s:6, type:1}}; }, clear:a => { a.more = {}; }, html:moreHTML(P, dt.filter(x => x[0] !== (a.dtype == null || a.dtype === '' ? dt[0][0] : +a.dtype)))},
    {id:'banish', n:'Изгнание', has:a => +a.banishN > 0, init:a => { a.banishN = 3; a.saveStat = 5; }, clear:a => zero(a, ['banishN']), html:G2(`<label>раундов вне боя ${numIn(P('banishN'), 0, LIM.rounds)}</label><span class="small">цель исчезает с поля: не ходит, её не видно и не достать; изгнавший пал — возвращается</span>`)},
    {id:'healally', n:'Лечит союзников', has:a => !!a.healAlly, init:a => { a.healAlly = true; }, clear:a => { a.healAlly = false; }, html:G2(`${chkIn(P('healAlly'), 'Союзников в области (или союзника-цель) урон этой способности лечит')}`)},
    {id:'special', n:'Особые эффекты', has:a => any(a, ['maxHpDmg', 'drain', 'dispel']), clear:a => zero(a, ['maxHpDmg', 'drain', 'dispel']),
      html:G2(`${chkIn(P('maxHpDmg'), 'Урон снижает и максимум хитов')}${chkIn(P('drain'), 'Вампиризм: половина урона — себе в хиты')}${chkIn(P('dispel'), 'Рассеивание: снимает баффы и концентрацию')}`)},
    {id:'ground', n:'Клетка, барьер, местность', has:a => any(a, ['ground', 'terr', 'wallLen']), init:a => { a.ground = true; }, clear:a => zero(a, ['ground', 'terr', 'wallLen', 'wallRounds']),
      html:G2(`${chkIn(P('ground'), 'Центр в клетке: область или барьер на любой клетке, а не на враге')}${chkIn(P('terr'), 'Труднопроходимая местность')}
        ${a.ground ? `<label>Барьер: клеток в линии (0 — нет) ${numIn(P('wallLen'), 0, 100)}</label><label>раундов барьера ${numIn(P('wallRounds'), 0, LIM.rounds)}</label>` : ''}`)},
    TIMING, COST];
}
const KIND_OPTS = [['attack','Атака (бросок попадания)'],['save','Спасбросок цели'],['auto','Без броска (всегда попадает)'],['self','На себя'],['onhit','При попадании оружием (приём)'],['cmd','Приказ союзнику атаковать'],['guard','Реакция за союзника'],['counter','Контрзаклинание'],['summon','Призыв приспешников'],['form','Превращение (облик)']];
function abilityHTML(a, i){
  const k = a._kind || 'attack';
  const dt = DT_OPTS(), cd = CAT.conds.map((n, j) => [j, n + (CAT.condPrice && CAT.condPrice[j] != null ? ` · ${Math.round(CAT.condPrice[j])} оч. (1 раунд)` : '')]).filter(x => x[0] > 0);
  const P = f => `abilities.${i}.${f}`;
  let h = `<div class="abil" data-ab="${i}"><div class="top"><b>${i + 1}.</b>
    <input type="text" data-p="${P('name')}" data-t="str" placeholder="Название" style="width:170px">
    ${selIn(P('_kind'), KIND_OPTS, 'str')}
    <span class="price" data-abprice="${i}"></span><button data-del="${i}" class="danger" title="Удалить">✕</button></div>
    <div class="small abparts" data-abparts="${i}"></div>
    <details class="descbox"${a._desc ? ' open' : ''}><summary>Описание для игроков (механика — выше; описание на бой не влияет)</summary>
      <textarea data-p="${P('_desc')}" data-t="str" rows="2" placeholder="Например: змея обвивает противника кольцами и сдавливает"></textarea>
      <div class="row"><button data-mkdesc="${i}" title="Черновик из деталей способности — дальше перепишите своими словами">✓ Собрать из механики</button><span class="small" data-descwarn="${i}"></span></div></details>`;
  if (k === 'cmd'){
    h += `<div class="hint" style="margin:6px 0">Союзник (приспешник) в пределах дальности бьёт оружием своей реакцией — по ближайшему врагу в досягаемости.</div>
      <div class="grid2"><label>Дальность до союзника, фт ${numIn(P('range'), 5, LIM.ft, 5)}</label>${chkIn(P('bonus'), 'Бонусным действием')}
      <label>Применений за бой (пусто — ∞, без ограничения) ${usesIn(P('uses'))}</label></div></div>`;
    return h;
  }
  if (k === 'summon'){
    const gs = a.groups || (a.groups = {0:{m:'Волк', n:2}}), keys = Object.keys(gs).filter(g => gs[g]);
    h += `<div class="hint" style="margin:6px 0">Приспешники появляются рядом и ходят сразу после вас — вы управляете ими сами. Цена: шаблон × копии × применения. Свой шаблон — соберите персонажа и нажмите «В библиотеку».</div>`;
    keys.forEach((g, n) => { h += `<div class="grid2"><label>Группа ${n + 1}: кто ${selIn(P('groups.' + g + '.m'), MINI_OPTS(), 'str')}</label><label>Копий ${numIn(P('groups.' + g + '.n'), 1, 20)}</label>${keys.length > 1 ? `<button data-sgdel="${i}:${g}" class="danger">✕ группу</button>` : ''}</div>`; });
    h += `<div class="grid2">${keys.length < 4 ? `<button data-sgadd="${i}">+ ещё группа (другие приспешники)</button>` : ''}<button data-minisave="1" title="Текущий персонаж (без его призывов) — в библиотеку приспешников и обликов">📥 Текущего персонажа — в библиотеку</button></div>
      <div class="grid2"><label>Применений за бой (пусто — ∞, без ограничения) ${usesIn(P('uses'))}</label>${chkIn(P('bonus'), 'Бонусным действием')}${chkIn(P('conc'), 'Концентрация (дешевле; исчезают с ней)')}</div></div>`;
    return h;
  }
  if (k === 'form'){
    h += `<div class="hint" style="margin:6px 0">Тело облика: размер, СИЛ/ЛОВ/ТЕЛ, КД, оружие, скорость и свой запас хитов; ваши ИНТ/МДР/ХАР остаются. Хиты облика кончились — вы возвращаетесь, лишний урон идёт на ваши хиты. В облике способности недоступны.</div>
      <div class="grid2"><label>Облик ${selIn(P('m'), MINI_OPTS(), 'str')}</label><label>Применений за бой ${numIn(P('uses'), 1, 10)}</label>${chkIn(P('bonus'), 'Бонусным действием')}<button data-minisave="1">📥 Текущего персонажа — в библиотеку</button></div></div>`;
    return h;
  }
  if (k === 'counter'){
    h += `<div class="hint" style="margin:6px 0">Срабатывает сама, реакцией: когда враг применяет способность в пределах дальности, она гаснет (плата за неё всё равно списана).</div>
      <div class="grid2"><label>Дальность, фт ${numIn(P('range'), 5, LIM.ft, 5)}</label><label>Применений за бой (пусто — ∞, без ограничения) ${usesIn(P('uses'))}</label>
      <label>Ячейка заклинания, ур. (0 — нет) ${numIn(P('slotLvl'), 0, 9)}</label></div></div>`;
    return h;
  }
  if (k === 'guard'){
    h += `<div class="hint" style="margin:6px 0">Срабатывает сама, реакцией: когда враг атакует союзника рядом с вами. Помеха — как стиль «Защита», удар — как черта «Страж».</div>
      <div class="grid2">${chkIn(P('protect'), 'Помеха этой атаке')}${chkIn(P('strike'), 'Удар оружием по атакующему')}
      <label>Союзник не дальше, фт ${numIn(P('range'), 5, LIM.ft, 5)}</label>
      <label>Применений за бой (пусто — ∞, без ограничения) ${usesIn(P('uses'))}</label></div></div>`;
    return h;
  }
  if (k === 'offhand'){
    h += `<div class="hint" style="margin:6px 0">Бой двумя оружиями: после атаки оружием в свой ход — бонусным действием ещё одна атака тем же оружием. Нужно лёгкое ближнее оружие (галочка в «Оружии»). Без стиля модификатор к урону второй рукой не добавляется (кроме отрицательного).</div>
      <div class="grid2">${chkIn(P('twfMod'), 'Стиль «сражение двумя оружиями»: модификатор к урону второй рукой')}</div></div>`;
    return h;
  }
  return h + groupsHTML('abilities.' + i, a, abilGroups(a, k, P, dt, cd), renderAbilities) + '</div>';
}
// доп. урон другого типа (модификация при сборке, v21 №10): до двух строк
function moreHTML(P, dt){
  // сколько угодно доп. типов урона (8 ноября): заполненные + одна пустая строка
  const filled = Object.keys(PATHS(S, P('more').replace(/^S\./, '')) || {}).filter(k => { const m = PATHS(S, P('more.' + k)); return m && +m.n > 0; }).length;
  const rows = Math.min(Math.max(2, filled + 1), dt.length);
  return `<div class="grid2" style="margin-top:6px">${Array.from({length: rows}, (_, j) => j).map(j => `<label>Доп. урон ${j + 1}: кубиков ${numIn(P('more.' + j + '.n'), 0, LIM.dice)}</label>
    <label>грань ${selIn(P('more.' + j + '.s'), DIE_OPTS())}</label><label>тип (другой) ${selIn(P('more.' + j + '.type'), dt).replace('<select ', '<select data-dt="1" ')}</label>`).join('')}</div>`;
}

// шаги конструктора: показан один; в подписи — цена шага (по категориям покупок) или сколько выбрано
const STEPS = [
  {n:'Основа'}, {n:'Характеристики и КД', cats:['stat', 'ac']}, {n:'Владения'}, {n:'Оружие', cats:['weapon']},
  {n:'Способности', cats:['ability', 'minions']}, {n:'Защита', adv:true}, {n:'Движение и чувства', adv:true}, {n:'Особенности', adv:true}];
let STEP = 0, SIMPLE = false;
try { SIMPLE = localStorage.getItem('dnd_simple') === '1'; } catch (e) {}
// простой режим прячет шаги для опытных (adv); купленное на них остаётся и считается в цене
const stepVis = i => !SIMPLE || !STEPS[i].adv;
const stepPrev = i => { for (let j = i - 1; j >= 0; j--) if (stepVis(j)) return j; return -1; };
const stepNext = i => { for (let j = i + 1; j < STEPS.length; j++) if (stepVis(j)) return j; return -1; };
// готовые персонажи для новичков: берутся поверх стандартного, бюджет остаётся вашим; типы урона — по названию
const CHAR_PRESETS = [
  {n:'Рыцарь — меч и доспех, ближний бой', c:{}},
  {n:'Лучник — длинный лук, держит дистанцию', c:{name:'Лучник', stats:[10,16,12,10,14,8], main:1, acV:2, armor:{kind:1, shield:false, stealthDis:false, prof:true},
    weapon:{armed:true, n:1, s:8, _dt:'колющ', range:150, long:500, att:2, reach:5, ammo:true, twoHanded:true, heavy:true, prof:true}, abilities:[], passives:{speed:30, saveProf:[1]}}},
  {n:'Маг — огненный снаряд и огненный шар', c:{name:'Маг', stats:[8,14,14,16,12,10], main:3, acV:1,
    weapon:{armed:true, n:1, s:4, _dt:'колющ', range:5, att:1, reach:5, finesse:true, light:true, prof:true},
    abilities:[{_kind:'attack', name:'Огненный снаряд', range:120, dmgN:1, dmgS:10, dtype:3}, {_kind:'save', name:'Огненный шар', ground:true, saveStat:1, range:150, radius:20, dmgN:4, dmgS:6, dtype:3, recharge:5}],
    passives:{speed:30, saveProf:[3, 4]}}},
  {n:'Монах — без доспеха и оружия, быстрый', c:{name:'Монах', stats:[10,16,14,10,16,8], main:1, acV:0,
    weapon:{armed:false, fistS:8, att:2}, abilities:[], passives:{speed:40, saveProf:[0, 1], evasion:true, unarmoredStat:4, disengB:true}}},
  {n:'Варвар — двуручный топор, много здоровья', c:{name:'Варвар', stats:[16,14,16,8,10,8], main:0, acV:0,
    weapon:{armed:true, n:1, s:12, _dt:'рубящ', range:5, att:2, reach:5, twoHanded:true, heavy:true, prof:true}, abilities:[], passives:{speed:40, saveProf:[0, 2], unarmoredStat:2}}},
];
function applyCharPreset(k){
  const P = CHAR_PRESETS[+k], c = JSON.parse(JSON.stringify(DEFAULT_CHAR)), x = JSON.parse(JSON.stringify(P.c));
  if (x.weapon){ const ti = CAT.dtypes.findIndex(t => t.startsWith(x.weapon._dt || '~')); delete x.weapon._dt; x.weapon = Object.assign({}, c.weapon, x.weapon); if (ti >= 0) x.weapon.type = ti; }
  if (x.passives) x.passives = Object.assign({}, c.passives, x.passives);
  S = Object.assign(c, x, {budget: S.budget});
  save(); buildForm(); queuePrice();
}
try { STEP = Math.min(STEPS.length - 1, Math.max(0, +localStorage.getItem('dnd_step') || 0)); } catch (e) {}
function stepNote(i){
  const p = S.passives || {}, L = x => (x || []).length;
  const st = STEPS[i], b = lastPrice && lastPrice.boss;
  if (st.cats && b){ const c = b.items.filter(it => st.cats.includes(it.cat)).reduce((s, it) => s + it.cost, 0); return c ? c + ' оч.' : '—'; }
  if (i === 0) return S.budget + ' оч.';
  const n = [0, 0,
    L(p.skills) + L(p.expert) + L(p.saveProf) + (+p.tools > 0) + ((+S.prof || 2) > 2), 0, 0,
    L(p.res) + L(p.imm) + L(p.vuln) + L(p.immCond) + !!p.resNM + !!p.immNM + (+p.parry > 0) + !!p.uncanny + !!p.riposte + !!p.magicRes + !!p.spellRes + !!p.critImm,
    !!p.fly + !!p.immPush + !!p.disengB + !!p.ignDiff + !!p.climbS + !!p.swimS + !!p.burrowS + !!p.evasion + !!p.sunSens + (+p.lair > 0) + (+p.dv > 0) + (+p.blindsight > 0) + (+p.truesight > 0),
    L(p.rules) + L(p.trigs) + (+p.regenN > 0) + (+p.surge > 0) + (+p.legRes > 0) + (+p.legAct > 0) + !!p.wMagic + ((p.slots || []).some(x => +x > 0)) + (+(p.aura || {}).rad > 0) + (+(p.rider || {}).n > 0 || +(p.rider || {}).cond > 0)][i];
  return n ? 'выбрано ' + n : 'ничего';
}
function renderSteps(){
  const bar = $('stepBar'); if (!bar) return;
  if (!stepVis(STEP)) STEP = Math.max(0, stepPrev(STEP));
  bar.innerHTML = STEPS.map((s, i) => !stepVis(i) ? '' : `<button class="step-b${i === STEP ? ' on' : ''}" data-step="${i}"><span>${i + 1} · ${esc(s.n)}</span><small>${esc(stepNote(i))}</small></button>`).join('');
  document.querySelectorAll('#form .stepbox').forEach(el => el.classList.toggle('on', +el.dataset.box === STEP));
}
function goStep(i){
  STEP = Math.min(STEPS.length - 1, Math.max(0, i));
  if (!stepVis(STEP)) STEP = Math.max(0, stepPrev(STEP));
  try { localStorage.setItem('dnd_step', STEP); } catch (e) {}
  renderSteps();
  $('form').scrollIntoView({block:'start', behavior:'smooth'});
}
function stepNav(i){
  const a = stepPrev(i), b = stepNext(i);
  return `<div class="stepnav">${a >= 0 ? `<button data-step="${a}">← ${esc(STEPS[a].n)}</button>` : '<span></span>'}${b >= 0 ? `<button class="primary" data-step="${b}">${esc(STEPS[b].n)} →</button>` : '<button class="primary" data-tab2>К сопернику →</button>'}</div>`;
}
function presetSel(kind, list){
  return `<select data-presetsel="${kind}"><option value="">+ из заготовки…</option>${list.map((p, i) => `<option value="${i}">${esc(p.n)}</option>`).join('')}</select>`;
}

function buildForm(){
  const dt = DT_OPTS(), cd = CAT.conds.map((n, j) => [j, n + (CAT.condPrice && CAT.condPrice[j] != null ? ` · ${Math.round(CAT.condPrice[j])} оч. (1 раунд)` : '')]).filter(x => x[0] > 0);
  const pass = Object.fromEntries(CAT.passives.map(p => [p.key, p]));
  const pc = k => pass[k] ? ` · ${pass[k].price} оч.` : '';
  const U = CAT.unit;
  S.passives = S.passives || {};
  const p = S.passives, PP = f => 'passives.' + f;
  const box = (i, inner) => `<div class="stepbox" data-box="${i}">${inner}${stepNav(i)}</div>`;
  let h = `<div class="steps" id="stepBar"></div>`;
  // 0 · основа
  h += box(0, `<div class="card"><h3>Основа</h3>
    <div class="row"><label><input type="checkbox" id="simpleMode"${SIMPLE ? ' checked' : ''}> Простой режим</label><span class="small">только основные шаги; защита, движение и особенности — в полном режиме (купленное там остаётся)</span></div>
    <div class="row">${presetSel('char', CHAR_PRESETS).replace('+ из заготовки…', 'Готовый персонаж…')}<span class="small">заменит всего персонажа, бюджет останется вашим</span></div>
    <div class="row"><label>Имя <input type="text" data-p="name" data-t="str" style="width:200px"></label>
    <label>Бюджет (очки) ${numIn('budget', CAT.minBudget || 20, CAT.maxBudget || 100000, 1, 90)}</label>
    <label>Размер ${selIn('size', [[0,'крошечный'],[1,'маленький'],[2,'средний'],[3,'большой'],[4,'огромный'],[5,'громадный']])}</label>
    <label>Тип существа ${selIn('ctype', ['аберрация', 'зверь', 'небожитель', 'конструкт', 'дракон', 'элементаль', 'фея', 'исчадие', 'великан', 'гуманоид', 'монстр', 'слизь', 'растение', 'нежить'].map((t, j) => [j, t]))}</label></div>
    <div class="hint">Бюджет — сила персонажа, любое целое число от ${CAT.minBudget || 20} до ${CAT.maxBudget || 100000}. Все характеристики по 10 бесплатны, поэтому очки уходят на то, что выше десяти, и возвращаются, если что-то опустить ниже. Здоровье тратит остаток: 2 очка за ХП. У соперника по умолчанию столько же очков.</div>
    <div class="hint">Дальше по шагам: характеристики, владения, оружие, способности, защита, движение, особенности. Шаги можно проходить в любом порядке; справа — сколько потрачено и все покупки.</div></div>`);
  // 1 · характеристики и КД
  let st = `<div class="card"><h3>Характеристики и КД</h3>
    <div class="hint">Цена значения V (1–${LIM.stat}): V·(V+1)/2 − 55. Десять бесплатно, 16 стоит 81, 20 стоит 155; опустите до 8 — получите 19 очков. </div>
    <div class="hint"><b>Главная характеристика</b> — любая из шести, отметьте кружок «главная» справа. От неё бросок атаки способностей и СЛ спасбросков против них (8 + мастерство + её модификатор). Оружие бьёт своей: СИЛ, фехтовальное и боеприпасы — ЛОВ.</div>`;
  STAT_N.forEach((n, i) => {
    st += `<div class="statrow"><span class="nm">${n}</span><input type="range" min="1" max="${LIM.stat}" data-p="stats.${i}" data-t="int">${numIn('stats.' + i, 1, LIM.stat, 1, 56)}<span class="tnum small" data-statcost="${i}"></span>
      <label class="small"><input type="radio" name="main" data-main="${i}"> главная (способности)</label></div>`;
  });
  st += `<div class="statrow"><span class="nm">КД</span><input type="range" min="0" max="${LIM.ac}" data-p="acV" data-t="int">${numIn('acV', 0, LIM.ac, 1, 56)}<span class="tnum small" data-accost></span><span class="small" data-acval></span></div>
    <div class="hint">КД = 10 + купленные единицы + ЛОВ (по доспеху) + щит. Средний доспех пропускает ЛОВ не больше +2, тяжёлый — не пропускает вовсе; за срезанную ЛОВ очки возвращаются.</div>
    <div class="row"><label>Защита без доспеха: + модификатор ${selIn('passives.unarmoredStat', [[-1, 'нет'], ...STAT_N.map((n, j) => [j, n])])}</label><span class="small">(как у монаха и варвара: только без доспеха; цена — такой же КД по лестнице)</span></div>
    <div class="row"><label>Доспех ${selIn('armor.kind', [[0,'без доспеха'],[1,'лёгкий'],[2,'средний (ЛОВ до +2)'],[3,'тяжёлый (без ЛОВ, СИЛ 13+)']])}</label>
      ${chkIn('armor.shield', 'Щит +2 КД (занимает руку)', U.shield + ' оч.')}${chkIn('armor.stealthDis', 'Помеха к Скрытности')}${chkIn('armor.prof', 'Владею доспехом и щитом')}</div></div>`;
  h += box(1, st);
  // 2 · владения
  const SKL = (CAT.skills || []).map(k => k.name + ' (' + STAT_N[k.stat] + ')' + (k.combat ? ' ⚔' : ''));
  h += box(2, `<div class="card"><h3>Владения</h3>
    <div class="hint">Как в D&D: бонус мастерства — один на всё, чем вы владеете. +2 даром, каждая единица сверх — ${U.prof1} оч. Цена владения растёт с мастерством (× мастерство/2). Экспертиза удваивает мастерство в навыке. ⚔ — навык работает в бою (${U.skillC} оч.), остальные нужны вне боя (${U.skillNC} оч.).</div>
    <div class="row"><label>Бонус мастерства +${numIn('prof', 2, 6, 1, 56)}</label><label>Инструментов ${numIn('passives.tools', 0, 40, 1, 56)}</label></div>
    <div class="sec-h">Спасброски (${U.saveProf} оч. × мастерство/2)</div>${chipsFor('passives.saveProf', STAT_N)}
    <div data-chipscope="skl"><div class="row">${chipSearch('skl')}<span class="small">ищет по названию навыка и характеристике</span></div>
    <div class="sec-h">Навыки — по характеристикам</div>${chipGroups('passives.skills', SKL, i => CAT.skills[i].stat, STAT_N)}
    <div class="sec-h">Экспертиза (только в навыке с владением)</div>${chipGroups('passives.expert', SKL, i => CAT.skills[i].stat, STAT_N)}</div>
    <div class="hint">Оружием и доспехами владеют на своих шагах (галочки «Владею этим оружием», «Владею доспехом и щитом»); инструменты — числом выше.</div></div>`);
  // 3 · оружие
  h += box(3, `<div class="card"><h3>Оружие</h3>
    <div class="row">${chkIn('weapon.armed', 'Есть оружие (иначе безоружный)')}<label>Безоружный удар (когда нет оружия): грань ${selIn('weapon.fistS', DIE_OPTS())}</label><span class="small">к2 ≈ обычный удар кулаком (1 + СИЛ), к6–к10 — монах</span></div>
    <div class="row">${presetSel('wpn', WEAPON_PRESETS)}<span class="small">заготовка заполнит урон, дальность и свойства — дальше правьте как угодно</span></div>
    <div id="wpn">
      <details class="descbox"${S.weapon && S.weapon._desc ? ' open' : ''}><summary>Описание оружия для игроков</summary>
        <textarea data-p="weapon._desc" data-t="str" rows="2" placeholder="Например: ржавая алебарда с зазубринами"></textarea>
        <div class="row"><button data-mkwdesc="1">✓ Собрать из механики</button><span class="small" data-wdescwarn></span></div></details>
      <div class="grid2"><label>Кубиков урона ${numIn('weapon.n', 1, LIM.dice)}</label><label>грань ${selIn('weapon.s', DIE_OPTS())}</label>
      <label>Тип урона ${dtSel('weapon.type')}</label><label>Атак за действие ${numIn('weapon.att', 1, LIM.count)}</label>
      <label>Дальность, фт (5 — ближнее) ${numIn('weapon.range', 5, LIM.ft, 5)}</label><label>Досягаемость ближнего, фт ${numIn('weapon.reach', 5, LIM.ft, 5)}</label>
      ${chkIn('weapon.prof', 'Владею этим оружием', 'без владения: −' + U.noWProf + ' оч. × мастерство/2')}</div>
      <div class="sec-h">Свойства</div>
      <div class="grid2">${chkIn('weapon.finesse', 'Фехтовальное (СИЛ или ЛОВ)', '+3 оч.')}${chkIn('weapon.silver', 'Посеребрённое: пробивает сопротивление немагическому')}${chkIn('weapon.ammo', 'Боеприпасное (по ЛОВ)')}
      ${chkIn('weapon.light', 'Лёгкое (можно бить второй рукой)')}${chkIn('weapon.offhand', 'Атака второй рукой: бонусным действием, после атаки оружием (своим оружием)')}${chkIn('weapon.twfMod', 'Стиль двумя оружиями: модификатор и к урону второй руки')}<span class="price" data-ohprice></span>${chkIn('weapon.twoHanded', 'Двуручное (без щита и второй руки)')}${chkIn('weapon.heavy', 'Тяжёлое (маленьким — помеха)')}
      <label>Универсальное: грань двумя руками ${selIn('weapon.versatile', [[0,'нет'], ...DIE_OPTS()])}</label>
      <label>Дальняя дистанция, фт (0 — нет) ${numIn('weapon.long', 0, LIM.ft, 5)}</label></div>
      <div class="sec-h">Запас костей (кости превосходства, ки)</div>
      <div class="grid2"><label>костей ${numIn('weapon.poolN', 0, LIM.count, 1, 56)}</label><label>грань ${selIn('weapon.poolS', DIE_OPTS())}</label></div>
      <div class="hint">Перезарядка (арбалеты) = 1 атака за действие. Приёмы, которые тратят кости запаса, — способности «При попадании оружием».</div>
    </div>
    <div class="sec-h">Запасное оружие</div>
    <div class="hint">Можно держать несколько видов оружия и менять их в бою: первая смена за ход бесплатна, вторая — бонусным действием, третья — действием. Каждое стоит как отдельное оружие.</div>
    ${(S.altWeapons || []).map((q, i) => { const W = f => `altWeapons.${i}.${f}`; return `<div class="abil"><div class="top"><b>${i + 2}.</b>
      <input type="text" data-p="${W('name')}" data-t="str" placeholder="Название (лук, кинжал…)" style="width:170px"><span class="price" data-altprice="${i}"></span><button data-altdel="${i}" class="danger" title="Удалить">✕</button></div>
      <div class="grid2"><label>Кубиков ${numIn(W('n'), 1, LIM.dice)}</label><label>грань ${selIn(W('s'), DIE_OPTS())}</label><label>Тип урона ${dtSel(W('type'))}</label>
      <label>Атак за действие ${numIn(W('att'), 1, LIM.count)}</label><label>Дальность, фт (5 — ближнее) ${numIn(W('range'), 5, LIM.ft, 5)}</label><label>Досягаемость ближнего, фт ${numIn(W('reach'), 5, LIM.ft, 5)}</label>
      ${chkIn(W('finesse'), 'Фехтовальное')}${chkIn(W('ammo'), 'Боеприпасное (по ЛОВ)')}${chkIn(W('multi'), 'Бьёт в том же действии «Атака» (мультиатака: укус + когти), а не вместо основного')}</div></div>`; }).join('')}
    <button data-altadd="1">+ Запасное оружие</button></div>`);
  // 4 · способности
  h += box(4, `<div class="card"><h3>Способности</h3><div class="hint">Заклинания и приёмы. Цена = эффекты × (100% + надбавки − скидки), но не меньше 20% от эффектов. Надбавки — дальность, область, бонусное действие, «без броска»; скидки — ограничения, перезарядка, концентрация, ячейка. В каждой способности видны только выбранные эффекты, остальные — кнопками «+».</div>
    <div class="row"><button data-preset="blank" class="primary">+ Новая способность</button>${presetSel('abil', ABIL_PRESETS)}</div>
    <div id="abilList"></div></div>`);
  // 5 · защита
  const immCondChips = chipsFor('passives.immCond', CAT.conds.slice(1).map(String)).replace(/data-i="(\d+)"/g, (m, g) => `data-i="${+g + 1}"`);
  h += box(5, `<div class="card"><h3>Защита</h3><div class="hint">Цена резиста и иммунитета зависит от того, как часто встречается этот тип урона.</div>
    <div id="defGroups">${groupsHTML('def', p, [
      {id:'res', n:'Резист (урон вдвое меньше)', has:o => (o.res || []).length > 0, clear:o => { o.res = []; }, html:chipsFor('passives.res', CAT.dtypes, '', dtOrder(), dtPrices('resist5'))},
      {id:'imm', n:'Иммунитет к урону', has:o => (o.imm || []).length > 0, clear:o => { o.imm = []; }, html:chipsFor('passives.imm', CAT.dtypes, '', dtOrder(), dtPrices('imm5'))},
      {id:'vuln', n:'Уязвимость (очки возвращаются)', has:o => (o.vuln || []).length > 0, clear:o => { o.vuln = []; }, html:chipsFor('passives.vuln', CAT.dtypes, '', dtOrder(), dtPrices('vuln5'))},
      {id:'immc', n:'Иммунитет к состояниям', has:o => (o.immCond || []).length > 0, clear:o => { o.immCond = []; }, html:`<div class="small">${U.immCond} оч. за каждое</div>` + immCondChips},
      {id:'nm', n:'Немагическое физическое', has:o => !!o.resNM || !!o.immNM, clear:o => { o.resNM = o.immNM = false; },
        html:G2(`${chkIn(PP('resNM'), 'Резист', pc('resNM'))}${chkIn(PP('immNM'), 'Иммунитет', pc('immNM'))}`)},
      {id:'magic', n:'Против магии и критов', has:o => !!o.magicRes || !!o.spellRes || !!o.critImm, clear:o => { o.magicRes = o.spellRes = o.critImm = false; },
        html:G2(`${chkIn(PP('magicRes'), 'Сопротивление магии (преимущество на спасброски от способностей)', pc('magicRes'))}${chkIn(PP('spellRes'), 'Сопротивление урону от способностей (вдвое меньше)', pc('spellRes'))}${chkIn(PP('critImm'), 'Иммунитет к критам', pc('critImm'))}`)},
      {id:'react', n:'Защитные реакции', has:o => +o.parry > 0 || !!o.uncanny || !!o.riposte, clear:o => { o.parry = 0; o.uncanny = o.riposte = false; },
        html:G2(`<label>Парирование: +КД против ближней атаки ${numIn(PP('parry'), 0, LIM.count)}</label>${chkIn(PP('uncanny'), 'Невероятное уклонение', pc('uncanny'))}${chkIn(PP('riposte'), 'Контратака', pc('riposte'))}`)},
    ], buildForm)}</div></div>`);
  // 6 · движение и чувства
  h += box(6, `<div class="card"><h3>Движение и чувства</h3>
    <div class="grid2"><label>Скорость, фт (покупается вся, шаг 5) ${numIn(PP('speed'), 0, LIM.ft, 5)}</label></div>
    <div class="sec-h">Движение</div>
    <div class="grid2">${chkIn(PP('fly'), 'Полёт', pc('fly'))}${chkIn(PP('immPush'), 'Неподвижность (иммунитет к толчку)', pc('immPush'))}${chkIn(PP('disengB'), 'Отход бонусным действием', pc('disengB'))}${chkIn(PP('ignDiff'), 'Лёгкая поступь', pc('ignDiff'))}${chkIn(PP('climbS'), 'Скорость лазания', pc('climbS'))}${chkIn(PP('swimS'), 'Скорость плавания', pc('swimS'))}${chkIn(PP('burrowS'), 'Скорость копания', pc('burrowS'))}${chkIn(PP('evasion'), 'Увёртливость', pc('evasion'))}${chkIn(PP('sunSens'), 'Солнечная чувствительность (возврат)', pc('sunSens'))}</div>
    <div class="row"><label>Свет вокруг себя, фт (0 — нет) ${numIn(PP('lightR'), 0, LIM.ft, 10)}</label><span class="small">ночью без тёмного зрения неосвещённое видно лишь на 30 фт</span></div>
    <div class="sec-h">Чувства</div>
    <div class="grid2"><label>Тёмное зрение ${selIn(PP('dv'), [[0,'нет'],[30,'30 фт'],[60,'60 фт'],[120,'120 фт']])}</label><label>Слепое зрение ${selIn(PP('blindsight'), [[0,'нет'],[10,'10 фт'],[30,'30 фт'],[60,'60 фт']])}</label><label>Истинное зрение ${selIn(PP('truesight'), [[0,'нет'],[10,'10 фт'],[30,'30 фт'],[60,'60 фт'],[120,'120 фт']])}</label><label title="В начале раунда — обвал (уровень×2к10, ЛОВ — половина) или трясина вокруг врага, по очереди">Действия логова ${selIn(PP('lair'), [[0,'нет'],[1,'1: 2к10 · ' + U.lair + ' оч.'],[2,'2: 4к10 · ' + 2 * U.lair + ' оч.'],[3,'3: 6к10 · ' + 3 * U.lair + ' оч.']])}</label></div></div>`);
  // 7 · особенности
  h += box(7, `<div class="card"><h3>Особенности</h3>
    <div class="sec-h">«Когда → что»</div>
    <div class="hint">Условие и эффект: цена правила = цена эффекта × доля времени, когда условие выполняется. Противник — цель вашей атаки или тот, кто атакует вас.</div>
    <div class="row"><button data-rpreset="blank" class="primary">+ Своё правило</button>${presetSel('rule', RULE_PRESETS)}</div>
    <div id="ruleList"></div>
    <div class="sec-h">«Событие → эффект»</div>
    <div class="hint">Срабатывают при событии: крит, убийство, хиты упали до 0, после Рывка или Уклонения, в начале хода или боя, враг провалил ваш спасбросок.</div>
    <div class="row"><button data-tpreset="blank" class="primary">+ Своё событие</button>${presetSel('trig', TRIG_PRESETS)}</div>
    <div id="trigList"></div>
    <div class="sec-h">Другое</div>
    ${groupsHTML('feat', p, [
      {id:'boost', n:'Усиления', has:o => any(o, ['regenN', 'surge', 'legRes', 'legAct', 'wMagic']), clear:o => zero(o, ['regenN', 'surge', 'legRes', 'legAct', 'wMagic']),
        html:G2(`<label>Регенерация, к6 в ход ${numIn(PP('regenN'), 0, LIM.dice)}</label><label>Всплеск действий (раз за бой) ${numIn(PP('surge'), 0, LIM.count)}</label>
          <label>Легендарное сопротивление ${numIn(PP('legRes'), 0, LIM.count)}</label><label>Легендарные действия (очки) ${numIn(PP('legAct'), 0, LIM.count)}</label>${chkIn(PP('wMagic'), 'Магическое оружие', pc('wMagic'))}`)},
      {id:'slots', n:'Ячейки заклинаний', has:o => (o.slots || []).some(x => +x > 0), clear:o => { o.slots = [0,0,0,0,0,0,0,0,0,0]; },
        html:`<div class="small">Нужны способностям с «ячейкой».</div>` + G2([1,2,3,4,5,6,7,8,9].map(L => `<label>${L} ур. × ${numIn(PP('slots.' + L), 0, LIM.count, 1, 56)}</label>`).join(''))},
      {id:'aura', n:'Аура (на врагов рядом)', has:o => +(o.aura || {}).rad > 0, init:o => { o.aura = Object.assign({s:6, type:3, cond:0, rounds:1}, o.aura, {rad:10}); if (!(+o.aura.n > 0) && !(+o.aura.cond > 0)) o.aura.n = 1; }, clear:o => { (o.aura = o.aura || {}).rad = 0; },
        html:G2(`<label>Радиус, фт ${numIn(PP('aura.rad'), 0, LIM.ft, 5)}</label><label>кубиков ${numIn(PP('aura.n'), 0, LIM.dice)}</label><label>грань ${selIn(PP('aura.s'), DIE_OPTS())}</label>
          <label>тип ${dtSel(PP('aura.type'))}</label><label>состояние ${selIn(PP('aura.cond'), [[0,'нет'], ...cd])}</label><label>раундов ${numIn(PP('aura.rounds'), 1, LIM.rounds)}</label>`)},
      {id:'rider', n:'Довесок к удару оружием (раз в ход)', has:o => +(o.rider || {}).n > 0 || +(o.rider || {}).cond > 0, init:o => { o.rider = Object.assign({s:6, type:3, cond:0, rounds:1}, o.rider, {n:1}); }, clear:o => { o.rider = Object.assign(o.rider || {}, {n:0, cond:0}); },
        html:G2(`<label>кубиков ${numIn(PP('rider.n'), 0, LIM.dice)}</label><label>грань ${selIn(PP('rider.s'), DIE_OPTS())}</label><label>тип ${dtSel(PP('rider.type'))}</label>
          <label>состояние ${selIn(PP('rider.cond'), [[0,'нет'], ...cd])}</label><label>раундов ${numIn(PP('rider.rounds'), 1, LIM.rounds)}</label>
          ${+(p.rider || {}).cond === CAT.conds.indexOf('схвачен') ? `${chkIn(PP('rider.grabRestr'), 'Схваченный опутан')}<label>урон каждый ход, пока держит: кубиков ${numIn(PP('rider.grabDotN'), 0, LIM.dice)}</label><label>грань ${selIn(PP('rider.grabDotS'), DIE_OPTS())}</label>` : ''}`)},
    ], buildForm)}</div>`);
  $('form').innerHTML = h;
  renderSteps();
  renderAbilities();
  renderRules();
  renderTrigs();
  syncForm();
}
// «когда → что»: заготовки — прежние склейки, разложенные на условие и эффект
const RULE_PRESETS = [
  {n:'Тактика стаи', r:[{when:1, what:0}]},
  {n:'Ярость', r:[{when:2, what:1, n:2, s:6}]},
  {n:'Добивание', r:[{when:5, warg:1, what:1, n:1, s:6, once:true}]},
  {n:'Безрассудство', r:[{when:7, what:0}, {when:7, what:4}]},
  {n:'Условная защита', r:[{when:3, what:6}]},
  {n:'Преимущество на спасы', r:[{when:0, what:7}]},
  {n:'Аура защиты', r:[{when:0, what:8, n:3, allyR:10}]},
  {n:'Благословение', r:[{when:0, what:9, n:1, s:4}]},
  {n:'Мастер тяжёлых доспехов', r:[{when:0, what:11, n:3}]},
  {n:'Мастер большого оружия (−5/+10)', r:[{when:0, what:14, n:5}]},
  {n:'Меткий стрелок (укрытие)', r:[{when:0, what:13}]},
  {n:'Подвижный (без провоцированных)', r:[{when:0, what:15}]},
];
function ruleHTML(r, i){
  const P = f => `passives.rules.${i}.${f}`;
  const ws = (CAT.whens || []).map((w, k) => [k, w.name + (([5,6,7,8,10,11,12,15].includes(+r.what)) && !w.self ? ' (нельзя: нужен противник)' : '')]);
  const es = (CAT.whats || []).map((e, k) => [k, e]);
  const cd = CAT.conds.map((n, j) => [j, n + (CAT.condPrice && CAT.condPrice[j] != null ? ` · ${Math.round(CAT.condPrice[j])} оч. (1 раунд)` : '')]).filter(x => x[0] > 0);
  let h = `<div class="row rule" data-rule="${i}"><b>Когда</b> ${selIn(P('when'), ws)}`;
  if (+r.when === 5) h += ` состояние ${selIn(P('warg'), cd)}`;
  h += ` <b>→</b> ${selIn(P('what'), es)}`;
  if (+r.what === 1) h += ` кубиков ${numIn(P('n'), 1, LIM.dice, 1, 56)} грань ${selIn(P('s'), DIE_OPTS())} ${chkIn(P('once'), 'раз в ход')}`;
  if (+r.what === 2) h += ` +${numIn(P('n'), 1, 20, 1, 56)} КД`;
  if (+r.what === 8) h += ` +${numIn(P('n'), 1, 20, 1, 56)}`;
  if (+r.what === 9 || +r.what === 10) h += ` кубиков ${numIn(P('n'), 1, LIM.dice, 1, 56)} грань ${selIn(P('s'), DIE_OPTS())}`;
  if (+r.what === 11) h += ` на ${numIn(P('n'), 1, 1000, 1, 56)}`;
  if (+r.what === 12) h += ` тип ${dtSel(P('n'))}`;
  if (+r.what === 14) h += ` −${numIn(P('n'), 1, 20, 1, 56)} / +2×`;
  if (+r.what === 16) h += ` навык ${selIn(P('n'), (CAT.skills || []).map((x, j) => [j, x.name]))}`;   // преимущество на навык
  if (+r.what === 18) h += ` −${numIn(P('n'), 1, 20, 1, 56)} КД`;   // недостаток: минус к КД
  h += ` <label class="small">аура для своих, фт ${numIn(P('allyR'), 0, LIM.ft, 5, 56)}</label>`;
  return h + ` <span class="price" data-rprice="${i}"></span><button data-rdel="${i}" class="danger" title="Удалить">✕</button></div>`;
}
const TRIG_PRESETS = [
  {n:'Мастер большого оружия (крит/убийство → атака)', t:[{on:0, what:0}, {on:1, what:0}]},
  {n:'Неумолимая ярость (0 хитов → 1)', t:[{on:2, what:5, uses:1}]},
  {n:'Налётчик (Рывок → атака)', t:[{on:3, what:0}]},
  {n:'Бдительный (+5 к инициативе)', t:[{on:6, what:7, n:5}]},
  {n:'Везунчик (3 переброса)', t:[{on:8, what:9, uses:3}, {on:9, what:9, uses:3}]},
  {n:'Второй шанс', t:[{on:10, what:9, uses:1}]},
  {n:'Огненный щит (урон атакующему вблизи)', t:[{on:11, what:3, n:2, s:8}]},
];
function trigHTML(t, i){
  const P = f => `passives.trigs.${i}.${f}`;
  const on = +t.on || 0, ok = (CAT.tok || [])[on] || [];
  const cd = CAT.conds.map((n, j) => [j, n + (CAT.condPrice && CAT.condPrice[j] != null ? ` · ${Math.round(CAT.condPrice[j])} оч. (1 раунд)` : '')]).filter(x => x[0] > 0);
  let h = `<div class="row rule"><b>Событие</b> ${selIn(P('on'), (CAT.tons || []).map((n, k) => [k, n]))} <b>→</b> ${selIn(P('what'), (CAT.tdos || []).map((n, k) => [k, n + (ok[k] ? '' : ' (не сочетается)')]))}`;
  const w = +t.what || 0;
  if (w === 1 || w === 2 || w === 3) h += ` кубиков ${numIn(P('n'), 0, LIM.dice, 1, 56)} грань ${selIn(P('s'), DIE_OPTS())} + ${numIn(P('b'), 0, 1000, 1, 56)}`;
  if (on === 12) h = h.replace('<b>→</b>', `тип ${dtSel(P('arg'))} <b>→</b>`);   // «я получил урон выбранного типа»
  if (w === 4 || w === 10) h += ` ${selIn(P('cond'), cd)} на ${numIn(P('n'), 1, LIM.rounds, 1, 56)} р.${w === 10 ? ' (на себя — очки возвращаются)' : ''}`;
  if (w === 7) h += ` +${numIn(P('n'), 1, 20, 1, 56)}`;
  if (w === 8) h += ` +5 фт × ${numIn(P('n'), 1, 20, 1, 56)}`;
  return h + ` раз за бой (пусто — ∞, без ограничения) ${usesIn(P('uses'), 56)} <span class="price" data-tprice="${i}"></span><button data-tdel="${i}" class="danger" title="Удалить">✕</button></div>`;
}
function renderTrigs(){
  const box = $('trigList'); if (!box) return;
  const ts = (S.passives && S.passives.trigs) || [];
  box.innerHTML = ts.map(trigHTML).join('') || '<div class="hint">Событий нет.</div>';
  syncForm();
}
function renderRules(){
  const box = $('ruleList'); if (!box) return;
  const rs = (S.passives && S.passives.rules) || [];
  box.innerHTML = rs.map(ruleHTML).join('') || '<div class="hint">Особенностей нет.</div>';
  syncForm();
}
function renderAbilities(){
  $('abilList').innerHTML = (S.abilities || []).map(abilityHTML).join('') || '<div class="hint">Способностей пока нет: выберите заготовку выше.</div>';
  syncForm();
}
