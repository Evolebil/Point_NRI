// ====================================================================================================================
// 1. Конструктор персонажа
// ====================================================================================================================
const DEFAULT_CHAR = {v:21, name:'Рыцарь', budget:500, ctype:9, stats:[16,10,14,8,10,8], acV:4, main:0, prof:2, size:2, armor:{kind:0, shield:false, stealthDis:false, prof:true},
  weapon:{armed:true, n:1, s:8, range:5, att:2, reach:5, type:11, finesse:false, ammo:false, prof:true},
  abilities:[{_kind:'save', saveStat:1, range:5, radius:10, dmgN:2, dmgS:6, dtype:1, cond:1, condRounds:1, recharge:5}],
  passives:{speed:30, saveProf:[2], rules:[], res:[], imm:[], vuln:[], immCond:[], slots:[0,0,0,0,0,0,0,0,0,0], aura:{n:0,s:6,type:3,rad:0,cond:0,rounds:1}, rider:{n:0,s:6,type:3,cond:0,rounds:1}}};
let S = JSON.parse(JSON.stringify(DEFAULT_CHAR));
// описание до v21: acV — прежние КД-очки (+1 КД за 2), speedBonus — надбавка к бесплатным 30 фт
function migrateChar(c){
  if (c.weapon && c.weapon.prof == null) c.weapon.prof = true;
  // атака второй рукой — галочка у оружия (8 ноября), а не тип способности
  const oh = (c.abilities || []).find(a => a._kind === 'offhand');
  if (oh && c.weapon){ c.weapon.offhand = true; c.weapon.twfMod = !!oh.twfMod; c.abilities = c.abilities.filter(a => a._kind !== 'offhand'); }
  if (!c.armor) c.armor = {kind:0, shield:false, stealthDis:false, prof:true};   // доспехи (v21)
  (c.abilities || []).forEach(a => { if (a._kind === 'zone'){ a.ground = true; const hostile = a.dmgN > 0 || a.cond > 0 || a.pushFt > 0 || a.dotN > 0; a._kind = !hostile ? 'auto' : ['attack', 'save', 'auto'][a._zdel == null ? 1 : +a._zdel]; } });   // «по клетке» — галочка   // владения (v21): прежние персонажи владеют своим оружием
  const p = c.passives || {};   // «когда → что»: прежние склейки — в правила
  if (!p.rules){
    p.rules = [];
    if (p.pack) p.rules.push({when:1, what:0});
    if (p.reckless) p.rules.push({when:7, what:0}, {when:7, what:4});
    if (p.saveAdv) p.rules.push({when:0, what:7});
    if (p.phN > 0) p.rules.push({when:2, what:1, n:p.phN, s:6});
    if (p.cdN > 0) p.rules.push({when:5, warg:p.cdK || 1, what:1, n:p.cdN, s:6, once:true});
    if (p.condDefT > 0) p.rules.push({when:[0, 3, 2, 4][p.condDefT], what:p.condDefS === 2 ? 6 : 5});
    p.rules.forEach(r => { r.warg = r.warg || 1; r.n = r.n || 1; r.s = r.s || 6; r.once = !!r.once; });
    ['pack', 'reckless', 'saveAdv', 'phN', 'cdN', 'cdK', 'condDefT', 'condDefS'].forEach(k => delete p[k]);
  }
  if (c.v >= 21) return c;
  c.acV = Math.floor((c.acV || 0) / 2);
  c.passives = c.passives || {};
  if (c.passives.speed == null) c.passives.speed = 30 + (c.passives.speedBonus || 0);
  delete c.passives.speedBonus;
  c.v = 21;
  return c;
}
try { const t = localStorage.getItem('dnd_char'); if (t) S = Object.assign(JSON.parse(JSON.stringify(DEFAULT_CHAR)), migrateChar(JSON.parse(t))); } catch (e) {}
let CAT = null, lastPrice = null, priceTimer = null, priceSeq = 0;

const ABIL_PRESETS = [
  {n:'Удар заклинанием', a:{_kind:'attack', range:60, dmgN:3, dmgS:8, dtype:3}},
  {n:'Огненный шар', a:{_kind:'save', saveStat:1, range:60, radius:20, dmgN:4, dmgS:6, dtype:3, recharge:5}},
  {n:'Удар с отбросом', a:{_kind:'attack', range:5, dmgN:2, dmgS:6, dtype:1, pushFt:10}},
  {n:'Ледяные путы', a:{_kind:'save', saveStat:0, range:30, cond:2, condRounds:2, dmgN:2, dmgS:6, dtype:2, uses:2}},
  {n:'Страх', a:{_kind:'save', saveStat:4, range:30, radius:15, cond:3, condRounds:2, uses:1}},
  {n:'Лечение', a:{_kind:'self', healN:3, bonus:true, uses:3}},
  {n:'Каменная кожа', a:{_kind:'self', tempN:4, uses:1}},
  {n:'Прыжок', a:{_kind:'self', teleFt:30, bonus:true, recharge:5}},
  {n:'Стена-барьер', a:{_kind:'auto', ground:true, range:30, wallLen:5, wallRounds:3, uses:2}},
  {n:'Заросли (замедляют путь)', a:{_kind:'auto', ground:true, range:30, radius:15, terr:true, recharge:5}},
  {n:'Огненный шар по клетке', a:{_kind:'save', ground:true, saveStat:1, range:60, radius:20, dmgN:4, dmgS:6, dtype:3, recharge:5}},
  // заклинания PHB (v21): числа из книги в механиках движка; для заклинаний с ячейкой купите ячейки (пассивы)
  {n:'PHB · Огненный снаряд (заговор)', a:{_kind:'attack', name:'Огненный снаряд', range:120, dmgN:1, dmgS:10, dtype:3}},
  {n:'PHB · Луч холода (заговор)', a:{_kind:'attack', name:'Луч холода', range:60, dmgN:1, dmgS:8, dtype:2, cond:7, condRounds:1}},
  {n:'PHB · Священное пламя (заговор)', a:{_kind:'save', name:'Священное пламя', saveStat:1, range:60, dmgN:1, dmgS:8, dtype:10}},
  {n:'PHB · Волшебная стрела (1 ур.)', a:{_kind:'auto', name:'Волшебная стрела', range:120, dmgN:3, dmgS:4, dtype:4, slotLvl:1}},
  {n:'PHB · Громовая волна (1 ур.)', a:{_kind:'save', name:'Громовая волна', saveStat:2, range:15, radius:15, shape:1, dmgN:2, dmgS:8, dtype:12, pushFt:10, slotLvl:1}},
  {n:'PHB · Лечащее слово (1 ур.)', a:{_kind:'self', name:'Лечащее слово', healN:1, bonus:true, slotLvl:1}},
  {n:'PHB · Туманный шаг (2 ур.)', a:{_kind:'self', name:'Туманный шаг', teleFt:30, bonus:true, slotLvl:2}},
  {n:'PHB · Удержание личности (2 ур.)', a:{_kind:'save', name:'Удержание личности', saveStat:4, range:60, cond:10, condRounds:10, conc:true, slotLvl:2}},
  {n:'PHB · Паутина (2 ур.)', a:{_kind:'save', name:'Паутина', ground:true, saveStat:1, range:60, radius:20, cond:2, condRounds:10, conc:true, slotLvl:2}},
  {n:'PHB · Огненный шар (3 ур.)', a:{_kind:'save', name:'Огненный шар', ground:true, saveStat:1, range:150, radius:20, dmgN:8, dmgS:6, dtype:3, slotLvl:3}},
  {n:'PHB · Молния (3 ур.)', a:{_kind:'save', name:'Молния', saveStat:1, range:100, radius:100, shape:3, dmgN:8, dmgS:6, dtype:5, slotLvl:3}},
  {n:'PHB · Ужас (3 ур.)', a:{_kind:'save', name:'Ужас', saveStat:4, range:30, radius:30, shape:2, cond:3, condRounds:10, conc:true, slotLvl:3}},
];
const PATHS = (o, p) => p.split('.').reduce((a, k) => (a == null ? a : a[k]), o);
function setPath(o, p, v){ const ks = p.split('.'); let a = o; for (let i = 0; i < ks.length - 1; i++){ if (a[ks[i]] == null) a[ks[i]] = {}; a = a[ks[i]]; } a[ks[ks.length - 1]] = v; }
function num(v, d = 0){ const x = parseFloat(v); return Number.isFinite(x) ? x : d; }

function toSpec(a){   // из состояния формы в описание способности для движка
  const k = a._kind || 'attack', o = {};
  for (const key of Object.keys(a)) if (key[0] !== '_') o[key] = a[key];
  o.self = k === 'self';
  o.delivery = k === 'save' ? 1 : (k === 'auto' ? 2 : 0);
  if (k === 'zone'){ o.ground = true; o.delivery = (o.dmgN > 0 || o.cond > 0 || o.pushFt > 0 || o.dotN > 0) ? (a._zdel == null ? 1 : +a._zdel) : 0; }   // старые описания
  if (k === 'onhit'){ o.onHit = true; o.delivery = a._ohsave ? 1 : 0; }   // приём при попадании оружием
  if (k === 'offhand'){ return {offhand:true, twfMod:!!a.twfMod, name:a.name || ''}; }   // атака второй рукой
  if (k === 'cmd'){ return {cmdAtk:true, self:true, range:+a.range || 30, bonus:!!a.bonus, uses:+a.uses || 0, name:a.name || ''}; }   // приказ союзнику
  if (k === 'counter'){ return {counter:true, self:true, react:4, range:+a.range || 60, uses:+a.uses || 0, slotLvl:+a.slotLvl || 0, name:a.name || ''}; }   // контрзаклинание
  if (k === 'summon'){   // призыв: до 4 групп приспешников из библиотеки
    const M = MINI(), gs = Object.values(a.groups || {}).filter(g => g && M[g.m]).slice(0, 4).map(g => ({n: Math.max(1, +g.n || 1), minion: M[g.m]}));
    return {self:true, summon:{groups: gs}, bonus:!!a.bonus, conc:!!a.conc, uses:+a.uses || 0, name:a.name || ''};
  }
  if (k === 'form'){ const M = MINI(); return {self:true, form:{body: M[a.m] || M['Медведь']}, bonus:!!a.bonus, uses:+a.uses || 1, name:a.name || ''}; }
  if (k === 'guard'){ return {react:5, self:true, protect:!!a.protect, strike:!!a.strike, range:+a.range || 5, uses:+a.uses || 0, name:a.name || ''}; }   // реакция за союзника
  // доп. урон другого типа (v21): в форме — more.0, more.1; движку — список непустых
  o.more = Object.values(a.more || {}).filter(m => m && +m.n > 0).map(m => ({n:+m.n, s:+(m.s || DIE_OPTS()[0][0]), type:m.type == null || m.type === '' ? DT_OPTS().map(x => x[0]).find(t => t !== +o.dtype) : +m.type}));   // пустое поле = первый вариант списка, как показывает форма
  if (k === 'self') o.more = [];
  return o;
}
// Библиотека приспешников и обликов (v21): готовые шаблоны + сохранённые игроком персонажи (описание для движка)
const MINI_PRESETS = {
  'Волк': {v:21, name:'Волк', budget:150, ctype:1, stats:[12,15,12,3,12,6], acV:3, main:1, size:2, weapon:{n:2, s:4, range:5, att:1, reach:5, type:7, finesse:true}, passives:{speed:40}},
  'Скелет': {v:21, name:'Скелет', budget:120, ctype:13, stats:[10,14,15,6,8,5], acV:3, main:1, size:2, weapon:{n:1, s:6, range:5, att:1, reach:5, type:7, finesse:true}, passives:{speed:30, vuln:[1], imm:[8]}},
  'Медведь': {v:21, name:'Медведь', budget:400, ctype:1, stats:[19,10,16,2,13,7], acV:1, main:0, size:3, weapon:{n:2, s:6, range:5, att:2, reach:5, type:11}, passives:{speed:40, climbS:true}},
  'Орёл': {v:21, name:'Орёл', budget:120, ctype:1, stats:[6,15,10,2,14,7], acV:2, main:1, size:1, weapon:{n:1, s:6, range:5, att:1, reach:5, type:11, finesse:true}, passives:{speed:10, fly:true}}};
let MINI_USER = {}; try { MINI_USER = JSON.parse(localStorage.getItem('dnd_minions') || '{}'); } catch (e) {}
const MINI = () => Object.assign({}, MINI_PRESETS, MINI_USER);
const MINI_OPTS = () => Object.keys(MINI()).map(n => [n, n + (MINI_PRESETS[n] ? '' : ' (свой)') + ' · бюджет ' + MINI()[n].budget]);
function toChar(){
  const c = JSON.parse(JSON.stringify(S));
  // безоружный (8 ноября): удар кулаком своей гранью (по умолчанию к2 ≈ «1 + СИЛ»), дробящий, ближний
  const w = c.weapon; if (!w.armed) c.weapon = {n:1, s:+w.fistS || 2, range:5, att:Math.max(1, +w.att || 1), reach:5, type:1}; else { delete w.armed; delete w._desc; delete w._descMech; delete w.fistS; }
  c.abilities = (S.abilities || []).map(toSpec);
  if (S.weapon && S.weapon.armed && S.weapon.offhand) c.abilities.push({offhand:true, twfMod:!!S.weapon.twfMod, name:'Вторая рука'});   // галочка у оружия
  if (c.weapon){ delete c.weapon.offhand; delete c.weapon.twfMod; }
  c.altWeapons = (S.altWeapons || []).map(q => ({n:+q.n || 1, s:+q.s || 6, type:+(q.type || 0), att:+q.att || 1, range:+q.range || 5, reach:+q.reach || 5, finesse:!!q.finesse, ammo:!!q.ammo, multi:!!q.multi}));
  return c;
}

// технические пределы и кубики приходят из движка (каталог); запасные значения — на случай старого каталога
const LIM = {get ft(){ return (CAT && CAT.maxFt) || 500; }, get rounds(){ return (CAT && CAT.maxRounds) || 500; }, get dice(){ return (CAT && CAT.maxDice) || 1000; },
  get count(){ return (CAT && CAT.maxCount) || 1000; }, get stat(){ return (CAT && CAT.statMax) || 40; }, get ac(){ return (CAT && CAT.acMax) || 30; }};
function DIE_OPTS(){ return ((CAT && CAT.dice) || [2,4,6,8,10,12,20,100]).map(x => [x, 'к' + x]); }
function numIn(path, min, max, step = 1, w = 74){
  return `<input type="number" data-p="${path}" data-t="num" min="${min}" max="${max}" step="${step}" style="width:${w}px">`;
}
// число применений: 0 = без ограничения, в поле — пусто и «∞»
function usesIn(path, w = 74){
  return `<input type="number" data-p="${path}" data-t="num" data-inf="1" min="0" max="${LIM.count}" step="1" placeholder="∞" style="width:${w}px">`;
}
function selIn(path, opts, t = 'int'){
  return `<select data-p="${path}" data-t="${t}">${opts.map(([v, l]) => `<option value="${v}">${esc(l)}</option>`).join('')}</select>`;
}
// что делает галочка в бою — короткой строкой под ней (у галочек без записи строки нет)
const CHK_HELP = {
  'passives.resNM': 'вдвое меньше дробящего, колющего и рубящего урона от немагического оружия',
  'passives.immNM': 'немагическое оружие (без «посеребрённого») не наносит физического урона',
  'passives.magicRes': 'преимущество на спасброски от способностей и их эффектов',
  'passives.spellRes': 'урон от способностей вдвое меньше; оружие бьёт как обычно',
  'passives.critImm': 'критическое попадание по вам считается обычным',
  'passives.uncanny': 'реакцией: урон от попавшей атаки вдвое меньше',
  'passives.riposte': 'реакцией: ответный удар по тому, кто попал по вам ближней атакой',
  'passives.fly': 'летает: перелетает препятствия, воду и пропасти',
  'passives.immPush': 'толчки и притягивания не сдвигают с места',
  'passives.disengB': 'отход без провоцированных атак — бонусным действием, а не действием',
  'passives.ignDiff': 'труднопроходимая местность не тратит лишнюю скорость',
  'passives.climbS': 'лазает с полной скоростью',
  'passives.swimS': 'плавает с полной скоростью',
  'passives.burrowS': 'копает землю с полной скоростью',
  'passives.evasion': 'спасбросок ЛОВ от области: успех — урона нет, провал — половина',
  'passives.sunSens': 'на солнце помеха к атакам — очки возвращаются',
};
function chkIn(path, label, hint = ''){
  const help = CHK_HELP[path];
  return `<label><span><input type="checkbox" data-p="${path}" data-t="bool"> ${esc(label)}${hint ? `<span class="price-tag">${esc(hint)}</span>` : ''}</span>${help ? `<span class="small chk-h">${esc(help)}</span>` : ''}</label>`;
}
function chipsFor(path, names, extra = '', order = null, prices = null){   // prices[i] — цена варианта (видна прямо на кнопке)
  return `<div class="chips" data-chips="${path}">${(order || names.map((n, i) => i)).map(i => `<span class="chipb" data-i="${i}">${esc(names[i])}${prices && prices[i] != null ? ` <small>${prices[i] < 0 ? '+' + (-prices[i]) : prices[i]} оч.</small>` : ''}</span>`).join('')}</div>`;
}
// те же кнопки, разбитые на группы (каждая группа — свой ряд с подзаголовком; путь общий, поэтому отметки и клики не меняются)
function chipGroups(path, names, groupOf, groupNames){
  return groupNames.map((g, gi) => { const ord = names.map((n, i) => i).filter(i => groupOf(i) === gi); return ord.length ? `<div class="small chip-g">${esc(g)}</div>${chipsFor(path, names, '', ord)}` : ''; }).join('');
}
// поиск по кнопкам внутри блока: прячет кнопки и подзаголовки групп, в которых ничего не нашлось
function chipSearch(scope){ return `<input type="search" data-chipsearch="${scope}" placeholder="Поиск…" style="width:220px">`; }
// цена защиты от типа урона: единица × доля этого типа в бою (у уязвимости очки возвращаются — показано с «+»)
const dtPrices = key => CAT.dtypes.map((n, i) => Math.max(1, Math.round((CAT.unit[key] || 0) * ((CAT.dshare || [])[i] || 0))) * (key === 'vuln5' ? -1 : 1));
// типы урона в списках: физические, стихии, магические (номера типов в движке не меняются, только порядок показа)
const DT_SHOW = ['дробящий', 'колющий', 'рубящий', 'огонь', 'холод', 'электричество', 'кислота', 'звук', 'яд', 'силовое поле', 'излучение', 'некротическая энергия', 'психическая энергия'];
function dtOrder(){
  const names = CAT.dtypes, rank = n => { const k = DT_SHOW.findIndex(x => n.startsWith(x.slice(0, 5))); return k < 0 ? 99 : k; };
  return names.map((n, j) => j).sort((a, b) => rank(names[a]) - rank(names[b]) || a - b);
}
const DT_OPTS = () => dtOrder().map(j => [j, CAT.dtypes[j]]);
// список типов урона; пустое значение = первый показанный тип (syncForm записывает его, чтобы видимое совпало с тем, что уйдёт движку)
const dtSel = path => selIn(path, DT_OPTS()).replace('<select ', '<select data-dt="1" ');
