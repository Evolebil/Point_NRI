// ====================================================================================================================
// Кампания (исследование и социалка; ТЗ docs/TZ_kampaniya_v2.md §6): экран входа и вкладки внутри кампании.
// Вкладку и её раздел создаёт этот файл (play.html не меняется). Правила — build/kampaniya, журнал — web/kampaniya.py.
// Кампания = комната с кодом (Р25): мастер создаёт и даёт код или ссылку (/?k=КОД), игроки входят. Мастер решает по каждому
// черновику: принять, перебросить, править итог, отменить; игрок предлагает.
// ====================================================================================================================
// sub — вкладка внутри кампании; draft — маршрут, который прокладывают щелчками; mode — 'route' | 'pos'; speaker — кто говорит
const KMP = {id: '', v: null, timer: 0, sub: 'map', speaker: '', world: null, worldOf: '', draft: [], mode: '', justCreated: false, gen: null, pmode: 'быстрее всего', preview: null};
const KMP_TERR = ['Пашни и сёла', 'Лес', 'Холмы и плато', 'Горы', 'Болото', 'Река и переправа', 'Город'];
const KMP_ROLES = ['без занятия', 'проводник', 'дозорный', 'добытчик', 'картограф', 'следопыт', 'помощь'];
const KMP_TRAV = ['пеший', 'всадник', 'гонец', 'армия', 'обоз'];
const KMP_LIFE = ['нищий', 'жалкий', 'бедный', 'скромный', 'комфортный', 'богатый', 'аристократический'];
const kmpMoney = cp => { cp = Math.round(+cp || 0); if (!cp) return '0 мм'; const s = cp < 0 ? '−' : ''; cp = Math.abs(cp);
  return s + [[Math.floor(cp / 100), 'зм'], [Math.floor(cp / 10) % 10, 'см'], [cp % 10, 'мм']].filter(x => x[0]).map(x => x[0] + ' ' + x[1]).join(' '); };
const kmpOpt = (arr, sel) => arr.map(x => `<option${x === sel ? ' selected' : ''}>${esc(x)}</option>`).join('');
const kmpName = () => playerName(false);
const kmpChecks = () => (typeof CAT !== 'undefined' && CAT && CAT.skills ? CAT.skills.map(k => k.name) : []).concat(STAT_N);
// подсказки к терминам (КР-07): значок «?» с одной фразой
const KMP_HELP = {
  'зерно': 'Число, от которого зависят все случайности кампании: то же зерно и те же решения — те же броски.',
  'СЛ': 'Сложность проверки: к20 + модификатор должно быть не меньше СЛ.',
  'пассивная Внимательность': '10 + модификатор Внимательности: замечает ли герой засаду, не бросая кубик.',
  'истощение': '1 — помеха на проверки, 2 — скорость вдвое, 3 — помеха на атаки и спасброски, 4 — максимум ХП вдвое, 5 — скорость 0, 6 — смерть.',
  'кости хитов': 'На коротком отдыхе герой тратит их: каждая лечит к8 + ТЕЛ. Долгий отдых возвращает половину.',
  'темп': 'Медленный ×0,75 (можно идти скрытно), обычный ×1, быстрый ×1,25 (−5 к пассивной Внимательности, нельзя добывать еду).',
  'отношение': 'Дружелюбный, безразличный, враждебный; внутри ступени — сдвиг от −2 до +2 (поправка к проверкам).',
  'роли': 'Проводник ведёт (навигация), дозорный замечает засады, добытчик ищет еду, картограф +2 проводнику, помощь — преимущество.',
  'черновик': 'Движок бросил кости и посчитал, но ещё ничего не изменил: мастер принимает, перебрасывает, правит или отменяет.',
};
const kq = t => `<span title="${esc(KMP_HELP[t] || t)}" style="cursor:help;border:1px solid currentColor;border-radius:50%;padding:0 5px;font-size:11px;opacity:.7;margin-left:3px">?</span>`;

// вкладка: кнопка и раздел; showTab (00_common.js) знает вкладки 1–6 — седьмую включаем сами
(function kmpMount(){
  const b = document.createElement('button');
  b.className = 'tab'; b.id = 'tab7'; b.setAttribute('role', 'tab'); b.setAttribute('aria-selected', 'false'); b.textContent = 'Кампания';
  $('tab6').after(b);
  const s = document.createElement('section');
  s.className = 'scene'; s.id = 's7';
  s.innerHTML = `<div class="card" style="max-width:1100px;margin:0 auto"><div id="kmpHome"></div><div id="kmpBox"></div></div>`;
  $('s6').after(s);
  const orig = showTab;
  showTab = function(n){ orig(n <= 6 ? n : 0); $('s7').classList.toggle('on', n === 7); $('tab7').setAttribute('aria-selected', n === 7);
    if (n === 7) KMP.id ? kmpCall({cmd: 'kmp_open'}) : kmpHome(); };
  b.onclick = () => showTab(7);
  // ссылка с кодом кампании: /?k=КОД — сразу в неё
  const k = new URLSearchParams(location.search).get('k');
  if (k) setTimeout(() => { KMP.id = k.toUpperCase(); showTab(7); }, 300);
})();

// ---- экран входа: мои кампании, новая, по коду ----
async function kmpHome(){
  KMP.id = ''; KMP.v = null; clearTimeout(KMP.timer);
  $('kmpBox').innerHTML = '';
  const who = kmpName();
  let h = `<h3>Кампания</h3><div class="hint">Поход с мастером: путь по карте, встречи, разговоры с NPC, торговля, отдых. Мастер создаёт
    кампанию и даёт игрокам код; движок бросает кости, мастер решает. (Бой на арене против бота или друзей — вкладки «Соперник» и «Комната».)</div>`;
  if (!who){ $('kmpHome').innerHTML = h + `<div class="card" style="margin-top:10px">Сначала введите своё имя в строке над вкладками — под ним вы будете мастером или игроком.</div>`; return; }
  const v = await api({cmd: 'kmp_list', name: who});
  const list = v.type === 'kmp_list' ? v.list : [];
  const ago = t => { const m = Math.round((Date.now() / 1000 - t) / 60); return m < 1 ? 'только что' : m < 60 ? m + ' мин назад' : m < 1440 ? Math.round(m / 60) + ' ч назад' : Math.round(m / 1440) + ' дн. назад'; };
  h += `<div class="sec-h">Мои кампании</div>` + (list.length ? list.map(c => `<div class="card" style="margin:6px 0;display:flex;gap:12px;align-items:center;flex-wrap:wrap">
      <div style="flex:1;min-width:220px"><b>${esc(c.title)}</b> <span class="small">· код <b class="tnum">${esc(c.id)}</b></span><br>
      <span class="small">${c.master ? 'вы мастер' : 'мастер: ' + esc(c.masterName)} · героев ${c.heroes} · игроков ${c.players} · изменена ${ago(c.t)}</span></div>
      <button class="primary" data-open="${esc(c.id)}">Открыть</button></div>`).join('')
    : `<div class="hint">Пока нет. Создайте кампанию ниже (вы станете мастером) или войдите по коду, который дал мастер.</div>`);
  h += `<div style="display:flex;gap:12px;flex-wrap:wrap;margin-top:10px">
    <div class="card" style="flex:1;min-width:260px"><h3>Новая кампания</h3><div class="hint">Вы будете мастером. После создания появится код для игроков.</div>
      <div class="row"><label>Название <input id="kmpTitle" placeholder="например, Акт I — Каркассон" style="width:220px"></label>
      <button class="primary" id="kmpNew">Создать</button></div></div>
    <div class="card" style="flex:1;min-width:260px"><h3>Войти по коду</h3><div class="hint">Код из пяти знаков даёт мастер.</div>
      <div class="row"><label>Код <input id="kmpCode" maxlength="5" style="width:90px;text-transform:uppercase"></label><button class="primary" id="kmpJoin">Войти</button></div></div></div>`;
  $('kmpHome').innerHTML = h;
  $('kmpHome').querySelectorAll('button[data-open]').forEach(b => b.onclick = () => { KMP.id = b.dataset.open; kmpCall({cmd: 'kmp_open'}); });
  $('kmpNew').onclick = async () => { KMP.justCreated = true; const r = await kmpCall({cmd: 'kmp_create', title: $('kmpTitle').value.trim() || 'Кампания'}); if (r.type !== 'kmp') KMP.justCreated = false; };
  const join = () => { const c = $('kmpCode').value.trim().toUpperCase(); if (c.length !== 5){ toast('код — пять знаков'); return; } KMP.id = c; kmpCall({cmd: 'kmp_open'}); };
  $('kmpJoin').onclick = join; $('kmpCode').onkeydown = e => { if (e.key === 'Enter') join(); };
}

async function kmpCall(o){
  const v = await api(Object.assign({id: KMP.id, name: kmpName(), hero: KMP.speaker}, o));
  if (v.type === 'error'){ toast(v.err); if (!KMP.v) KMP.id = ''; return v; }
  if (v.type === 'kmp' && (!KMP.world || KMP.worldOf !== v.id || ['kmp_world_use'].includes(o.cmd))) await kmLoadWorld(v.id);
  if (v.type === 'kmp'){
    const before = KMP.v ? KMP.v.drafts.length : 0;
    KMP.id = v.id; KMP.v = v;
    if (v.master && v.drafts.length > before && ['kmp_propose', 'kmp_say', 'kmp_reroll'].includes(o.cmd)) KMP.sub = 'dec';   // свой черновик — сразу к решению
    if (v.master && KMP.sub === 'dec' && !v.drafts.length && o.cmd === 'kmp_accept') KMP.sub = KMP.back || 'map';
    kmpRender(); clearTimeout(KMP.timer); KMP.timer = setTimeout(kmpPoll, 4000);
  }
  return v;
}
async function kmpPoll(){   // свежий журнал и черновики игроков; пока вкладка открыта и никто не правит поле
  if (!KMP.id || !$('s7').classList.contains('on') || document.activeElement && $('kmpBox').contains(document.activeElement) && /INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName)){
    KMP.timer = setTimeout(kmpPoll, 4000); return; }
  const v = await api({cmd: 'kmp_open', id: KMP.id, name: kmpName(), hero: KMP.speaker});
  if (v.type === 'kmp' && JSON.stringify(v) !== JSON.stringify(KMP.v)){ KMP.v = v; kmpRender(); }
  KMP.timer = setTimeout(kmpPoll, 4000);
}
const kmpPropose = act => kmpCall({cmd: 'kmp_propose', act});
const kmpDirect = act => kmpCall({cmd: 'kmp_direct', act});

// ---- черновик: коротко, что выпало ----
function kmpCheckLine(c){
  const d = c.d20.length > 1 ? `к20 ${c.d20.join('/')}${c.adv ? ' преим.' : ''}${c.dis ? ' помеха' : ''} → ${c.die}` : `к20 ${c.die}`;
  return `<div class="small">${esc(c.who)}: ${esc(c.what)} — ${d} ${c.mod >= 0 ? '+' : ''}${c.mod} = <b>${c.total}</b>${c.dc ? ` против ${c.dc} ` + (c.ok ? '<b style="color:#7bd88f">успех</b>' : '<b style="color:#ff7b7b">провал</b>') : ''}
    <span class="hint">(${esc(c.parts)})</span></div>`;
}
function kmpEvText(e){
  const r = e.res || {}, a = e.act || {};
  switch (e.type){
    case 'day': { const en = r.enc || {};
      return `День: ${r.km} км, ${esc(r.terrain)}${r.road ? ' по дороге' : ''}, темп ${['медленный','обычный','быстрый'][r.pace]}, ${r.hours} ч.
        ${r.lost ? '<b style="color:#ff7b7b">Сбились с пути.</b>' : ''} Встреча: к20 ${en.d20} против ${en.thr} — ${en.hit ? `<b>${en.ambush ? 'ЗАСАДА' : 'да'}</b> (к12 ${en.d12}): ${esc(en.text || '—')}` : 'нет'}.
        ${r.blocked ? '<b>Путь упёрся в воду.</b> ' : ''}${r.route ? `Впереди точек маршрута: ${r.route.length}. ` : ''}Дозор (пассивная Внимательность): ${r.lookout || '—'}. Добыто: ${r.forage ? r.forage.food : 0} фунтов еды. Еда ${r.food}, вода ${r.water}. Траты на жизнь: ${kmpMoney(r.pay)}${r.payShort ? ' (денег не хватило)' : ''}.
        ${(r.heroes || []).filter(h => h.exh).map(h => esc(h.name) + ': истощение +' + h.exh).join('; ')}`; }
    case 'rest': return `${r.kind} отдых: ` + (r.heroes || []).map(h => `${esc(h.name)} ${h.hp} ХП, костей ${h.hd}, истощение ${h.exh}`).join('; ');
    case 'buy': case 'sell': return `${e.type === 'buy' ? 'Покупка' : 'Продажа'}: ${esc(r.item)} ×${r.qty} — ${kmpMoney(r.total)}${r.pct ? ` (торг ${r.pct > 0 ? '+' : ''}${r.pct}%)` : ''}${r.offend ? ', торговец обижен' : ''}`;
    case 'turnin': return `${esc(r.party)} сдают «${esc(r.fact)}»${r.confirm ? ' (подтверждение)' : ''}: ${kmpMoney(r.pay)}; дальше за новые — ${kmpMoney(r.tier)}`;
    case 'contest': return `Состязание: ${r.win ? 'победил ' + esc(r.win) : 'ничья'} (разница ${r.diff})`;
    case 'group': return `Групповая проверка: ${r.passed} из ${r.of} — ${r.ok ? 'успех' : 'провал'}`;
    case 'check': return `Проверка: ${r.ok ? 'успех' : 'провал'}`;
    case 'hero': return `Герой: ${esc(e.name || '')}`;
    case 'route': return (r.sum ? `Маршрут ${esc(r.mode)}: ${r.sum.km} км, ≈ ${r.sum.days} дн., по дорогам ${r.sum.road}%` : `Маршрут: ${(e.path || []).length} точек`) +
      (e.by !== 'мастер' ? ` (предложил ${esc(e.by)})` : '');
    case 'dialog': return (e.npc ? `Разговор с ${esc(e.npc)}` : e.node ? '' : 'Разговор окончен') + (e.said ? ` · ${esc(e.hero || '')}: «${esc(e.said)}»` : '') +
      (e.node ? ` → ${esc(e.node)}` : '') + (e.text ? ` (${esc(e.text)})` : '') + ((e.do || []).length ? ' · ' + e.do.map(d => d.type === 'gm' && d.purse ? kmpMoney(d.purse) : d.fact || d.text || (d.type + (d.delta ? ' ' + d.delta : ''))).map(esc).join(', ') : '');
    default: return esc(e.type) + ' ' + esc(JSON.stringify(Object.fromEntries(Object.entries(e).filter(([k]) => !['n','type','by','attempt'].includes(k)))));
  }
}

// ---- внутри кампании: шапка, вкладки Карта / Отряд / Разговор / Журнал / Решения ----
function kmpRender(){
  const v = KMP.v, s = v.state, M = v.master;
  const heroes = s.heroes.map(h => h.name);
  if (!KMP.speaker || !heroes.includes(KMP.speaker)) KMP.speaker = heroes[0] || '';
  $('kmpHome').innerHTML = '';
  const link = location.origin + location.pathname + '?k=' + v.id;
  let h = `<div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap"><button id="kmpBack" title="К списку кампаний">←</button>
    <h3 style="margin:0">${esc(v.title)}</h3><span class="small">${M ? 'вы мастер' : 'мастер: ' + esc(v.masterName)}${v.players.length ? ' · игроки: ' + v.players.map(esc).join(', ') : ''}</span>
    <span style="margin-left:auto" class="small">код <b class="tnum" style="font-size:20px;letter-spacing:2px">${esc(v.id)}</b></span>
    <button id="kmpCopy" title="${esc(link)}">Скопировать ссылку для игроков</button></div>`;
  if (KMP.justCreated){ KMP.justCreated = false;
    h += `<div class="card" style="margin:8px 0;background:rgba(123,216,143,.12)">Кампания создана. Дайте игрокам код <b>${esc(v.id)}</b> или ссылку — они войдут
      и добавят своих героев (вкладка «Отряд»). Дальше: поставьте отряд на карту и ведите путь (вкладка «Карта»).</div>`; }
  h += `<div style="margin:6px 0" class="small"><b>День ${s.day}</b>, ${s.hour}:00 · ${esc(s.place || 'место не задано')} · пройдено ${s.km} км · еда ${s.food} фунтов, вода ${s.water} галлонов · кошелёк отряда ${kmpMoney(s.purse)}${s.returnTrip ? ' · <b>обратный путь</b>' : ''}</div>`;
  const tabs = [['map', 'Карта'], ['party', 'Отряд'], ['talk', 'Разговор' + (v.talk ? ' ●' : '')], ['log', 'Журнал']].concat(M ? [['dec', 'Решения' + (v.drafts.length ? ` (${v.drafts.length})` : '')]] : []);
  h += `<div class="tabs" style="justify-content:flex-start;margin:8px 0">${tabs.map(([k, n]) => `<button class="tab" data-sub="${k}" aria-selected="${KMP.sub === k}">${n}</button>`).join('')}</div>`;
  if (M && v.drafts.length && KMP.sub !== 'dec')
    h += `<div class="card small" style="margin:6px 0;background:rgba(232,180,60,.15)">Ждут решения: ${v.drafts.length} ${kq('черновик')} <button data-sub="dec">Открыть</button></div>`;
  if (!M && v.drafts.length)
    h += `<div class="card small" style="margin:6px 0">Ваши запросы ждут мастера: ${v.drafts.map(d => kmpEvTitle(d.ev)).map(esc).join(', ')}</div>`;
  if (M && v.errors && v.errors.length) h += `<div class="err small">Obsidian: ${v.errors.map(esc).join('<br>')}</div>`;
  h += {map: kmpTabMap, party: kmpTabParty, talk: kmpTabTalk, log: kmpTabLog, dec: kmpTabDec}[KMP.sub](v, s, M, heroes);
  $('kmpBox').innerHTML = h;
  kmpBind();
}
const kmpEvTitle = e => ({day: 'день пути', rest: 'отдых', buy: 'покупка', sell: 'продажа', check: 'проверка', contest: 'состязание', group: 'групповая проверка',
  hero: 'герой ' + (e.name || ''), route: 'маршрут', dialog: 'реплика', turnin: 'сдача сведений', note: 'заметка'}[e.type] || e.type);

function kmpTabMap(v, s, M, heroes){
  let h = '';
  if (M){
    h += `<div class="card" style="margin:6px 0"><div class="row"><button class="primary" id="kmpDay" style="font-size:16px">☀ Следующий день</button>
      <span class="small">${s.pos && s.route.length ? 'отряд идёт по маршруту' : s.pos ? 'маршрута нет — проложите его на карте или задайте местность дня в настройках' : 'отряд не на карте — «Отряд — в город» или «Поставить отряд щелчком»'}</span>
      <button id="kmpShort">Короткий отдых</button><button id="kmpLong">Долгий отдых</button></div>
      <details><summary class="small">Настройки дня: местность, часы, темп ${kq('темп')}, роли в пути ${kq('роли')}</summary>
      <div class="row"><label>Местность дня <select id="kDayT">${s.pos && s.route.length && KMP.world ? '<option value="">по карте и маршруту</option>' : ''}${kmpOpt(KMP_TERR)}</select></label><label><input type="checkbox" id="kDayRoad"> по дороге</label>
      <label>часов <input id="kDayH" type="number" value="8" min="0" max="24" style="width:55px"></label>
      <label>темп <select id="kDayP">${kmpOpt(['медленный', 'обычный', 'быстрый'], s.pace)}</select></label><label><input type="checkbox" id="kDayHot"> жара</label></div>
      <div class="row">${s.heroes.filter(x => !x.dead).map(x => `<label>${esc(x.name)} <select data-role="${esc(x.name)}">${kmpOpt(KMP_ROLES)}</select>
        <select data-help="${esc(x.name)}" title="кому помогает">${kmpOpt([''].concat(heroes.filter(n => n !== x.name)))}</select></label>`).join('') || '<span class="hint">героев нет</span>'}</div>
      <div class="row small">Короткий отдых — кости хитов ${kq('кости хитов')}: ${s.heroes.map(x => `<label>${esc(x.name)} <input data-hd="${esc(x.name)}" type="number" min="0" max="${x.hd}" value="0" style="width:45px"></label>`).join('')}</div></details></div>`;
    h += kmWorldPanel();
  }
  if (!KMP.world) return h + `<div class="hint">Мира у кампании нет. ${M ? 'Создайте его: «Мир» → «Новый мир».' : 'Мир выбирает мастер.'}</div>`;
  const W = KMP.world;
  h += `<div class="row small"><button id="kmRoute"${KMP.mode === 'route' ? ' class="primary"' : ''}>✏ Прокладывать путь щелчками</button>
      <label>или поселение <input id="kmTown" list="kmTowns" style="width:150px"></label><datalist id="kmTowns">${W.towns.map(t => `<option value="${esc(t[0])}">`).join('')}</datalist>
      <button id="kmTownAdd">+ в маршрут</button><label>путь <select id="kmMode">${kmpOpt(['быстрее всего', 'по дорогам', 'напрямик'], KMP.pmode)}</select></label>
      <button id="kmClear"${KMP.draft.length ? '' : ' disabled'}>Очистить</button>
      <button id="kmSend" class="primary"${KMP.preview && KMP.preview.path ? '' : ' disabled'}>${M ? 'Задать маршрут' : 'Предложить маршрут мастеру'}</button>
      ${M ? `<button id="kmTownPos">Отряд — в поселение</button><button id="kmPos"${KMP.mode === 'pos' ? ' class="primary"' : ''}>📍 Поставить отряд щелчком</button>` : ''}</div>
    <div class="small" id="kmSum" style="min-height:1.3em">${kmSumText()}</div>
    <div style="position:relative;max-width:1000px">
      <canvas id="kMap" style="width:100%;height:560px;display:block;border-radius:8px;cursor:grab;background:#1b2a3a;touch-action:none"></canvas>
      <div style="position:absolute;right:8px;top:8px;display:flex;flex-direction:column;gap:4px">
        <button id="kmIn" title="Ближе (колесо мыши)">＋</button><button id="kmOut" title="Дальше">－</button>
        <button id="kmHome" title="Весь мир">⤢</button>${s.pos ? '<button id="kmParty" title="К отряду">◎</button>' : ''}</div>
      <div id="kmScale" style="position:absolute;left:8px;bottom:8px;background:rgba(0,0,0,.55);color:#fff;padding:3px 8px;border-radius:6px;font-size:12px"></div></div>
    <div class="small" id="kmHover" style="min-height:1.3em">Тяните карту мышью, колесо — ближе и дальше. Наведите — местность и путь от отряда.</div>
    <div class="small">${KM_LEG.map(i => `<span style="display:inline-block;width:10px;height:10px;background:${KM_COL[i]};margin:0 3px 0 8px"></span>${esc(W.terrains[i])}`).join('')}
    <span style="margin-left:8px">коричневое — дорога (×1), синее — река; красная точка — отряд, жёлтое — маршрут</span></div>`;
  return h;
}
function kmpTabParty(v, s, M, heroes){
  let h = '';
  if (!s.heroes.length) h += `<div class="card" style="margin:6px 0">Героев пока нет. Соберите героя во вкладке «Персонаж» и нажмите «➕ Мой герой» — ${M ? 'черновик придёт вам в «Решения»' : 'мастер примет его в отряд'}.</div>`;
  else h += `<table class="small" style="width:100%;border-collapse:collapse"><tr><th align="left">Герой</th><th>Ур.</th><th>ХП</th><th>Кости хитов ${kq('кости хитов')}</th><th>Истощение ${kq('истощение')}</th><th>Голод, дней</th><th>Пасс. Внимат. ${kq('пассивная Внимательность')}</th><th>Кошелёк</th><th align="left">Вещи</th></tr>` +
    s.heroes.map(x => `<tr${x.dead ? ' style="opacity:.5"' : ''}><td>${esc(x.name)}${x.dead ? ' ☠' : ''}</td><td align="center">${x.level}</td><td align="center">${x.hp}/${x.hpMax}</td><td align="center">${x.hd}/${x.hdMax}</td>
      <td align="center">${x.exh}</td><td align="center">${x.daysNoFood}</td><td align="center">${x.passivePerception}</td><td align="center">${kmpMoney(x.purse)}</td>
      <td>${Object.entries(x.inv).map(([k, n]) => esc(k) + ' ×' + n).join(', ') || '—'}</td></tr>`).join('') + '</table>';
  h += `<div class="small" style="margin:6px 0">Путник: ${esc(s.traveler)} · жизнь: ${esc(s.lifestyle)} · груз отряда: ${Object.entries(s.inv).map(([k, n]) => esc(k) + ' ×' + n).join(', ') || '—'}<br>
    Сведения: ${s.facts.map(esc).join('; ') || '—'}<br>NPC ${kq('отношение')}: ${Object.entries(s.npcs).map(([k, n]) => esc(k) + ' — ' + n.attitude + (n.shift ? ` (${n.shift > 0 ? '+' : ''}${n.shift})` : '')).join('; ') || '—'}
    · Задания: ${Object.entries(s.quests).map(([k, q]) => esc(k) + ': за новые сведения ' + kmpMoney(q.tier)).join('; ') || '—'}</div>`;
  h += `<div class="sec-h">Действия</div><div class="row"><button id="kmpMine">➕ Мой герой (из вкладки «Персонаж»)</button></div>`;
  if (heroes.length) h += `<div class="row"><label>Проверка: герой <select id="kChHero">${kmpOpt(heroes)}</select></label><label>навык <select id="kChWhat">${kmpOpt(kmpChecks())}</select></label>
    ${M ? `<label>СЛ ${kq('СЛ')} <input id="kChDC" type="number" value="15" style="width:60px"></label><label><input type="checkbox" id="kChAdv"> преимущество</label><label><input type="checkbox" id="kChDis"> помеха</label>
      <label><input type="checkbox" id="kChHid"> скрыто от игроков</label>` : ''}<button id="kmpCheck">🎲 Проверка</button>${M ? '<button id="kmpGroup">Групповая (весь отряд)</button>' : ''}</div>
    <div class="row"><select id="kTrK"><option value="buy">Купить</option><option value="sell">Продать</option></select>
    <label>кто <select id="kTrWho">${kmpOpt(['отряд'].concat(heroes))}</select></label><label>вещь <input id="kTrItem" style="width:130px"></label>
    <label>шт. <input id="kTrQty" type="number" value="1" style="width:55px"></label><label>цена за шт. <input id="kTrPrice" placeholder="5 см" style="width:80px"></label>
    <label>у кого <input id="kTrNpc" placeholder="торговец" style="width:110px"></label>
    <details style="display:inline-block"><summary class="small">торговаться</summary><label><input type="checkbox" id="kTrH"> торг:</label>
    <select id="kTrHero">${kmpOpt(heroes)}</select><select id="kTrSk">${kmpOpt(['Убеждение', 'Обман', 'Запугивание'])}</select>
    <label>Проницательность торговца <input id="kTrVs" type="number" value="0" style="width:55px"></label></details><button id="kmpTrade">Сделка</button></div>`;
  if (M) h += `<details style="margin-top:8px"><summary>Сведения и задания</summary><div class="row">
      <label>Факт <input id="kFact" style="width:200px"></label><button id="kmpLearn">Отряд узнал</button>
      <label>Задание <input id="kQ" value="${esc(Object.keys(s.quests)[0] || '')}" style="width:120px"></label>
      <label>кто сдаёт <input id="kQParty" value="мы" style="width:120px"></label><button id="kmpTurn">Сдать факт</button></div>
      <div class="row"><label>Новое задание: за первые сведения <input id="kQBase" value="1000 зм" style="width:80px"></label><label>каждые следующие × <input id="kQStep" value="0.9" style="width:50px"></label>
      <label>подтверждение × <input id="kQConf" value="0.5" style="width:50px"></label><button id="kmpQuest">Создать задание</button></div></details>
    <details><summary>Мастер напрямую: деньги, вещи, ХП, время, место</summary><div class="row">
      <label>кому <select id="kGmWho">${kmpOpt(['отряд'].concat(heroes))}</select></label><label>деньги ± <input id="kGmPurse" placeholder="10 зм или -5 см" style="width:110px"></label>
      <label>вещь <input id="kGmItem" style="width:110px"></label><label>шт. ± <input id="kGmQty" type="number" value="1" style="width:55px"></label>
      <label>ХП = <input id="kGmHp" type="number" style="width:60px"></label><label>истощение = <input id="kGmExh" type="number" min="0" max="6" style="width:50px"></label>
      <label>часы + <input id="kGmHours" type="number" style="width:55px"></label><label>еда ± <input id="kGmFood" type="number" style="width:55px"></label>
      <label>вода ± <input id="kGmWater" type="number" style="width:55px"></label><label>место <input id="kGmPlace" style="width:120px"></label>
      <label>заметка в журнал <input id="kGmText" style="width:200px"></label><button class="primary" id="kmpGm">Сделать</button></div></details>
    <details><summary>NPC, фракции, поход</summary><div class="row"><label>NPC <input id="kNpc" style="width:110px"></label><select id="kNpcT">${kmpOpt(['', 'дружелюбный', 'безразличный', 'враждебный'])}</select>
      <label>сдвиг <input id="kNpcD" type="number" value="0" style="width:50px"></label><button id="kmpAtt">Отношение</button>
      <label>Фракция <input id="kRep" style="width:110px"></label><label>± <input id="kRepD" type="number" value="1" style="width:50px"></label><button id="kmpRep">Известность</button></div>
      <div class="row"><label>Путник <select id="kSetT">${kmpOpt(KMP_TRAV, s.traveler)}</select></label><label>уровень жизни <select id="kSetL">${kmpOpt(KMP_LIFE, s.lifestyle)}</select></label>
      <label><input type="checkbox" id="kSetR"${s.returnTrip ? ' checked' : ''}> обратный путь (порог встреч −4, засады)</label><button id="kmpSet">Применить</button></div></details>`;
  return h;
}

function kmpTabTalk(v, s, M, heroes){
  let h = '';
  if (M && v.npcCards) h += `<div class="row"><label>NPC из Obsidian <select id="kNpcSel">${v.npcCards.map(c => `<option value="${esc(c.name)}"${c.dialog ? '' : ' disabled'}>${esc(c.name)} — ${esc(c.attitude)}${c.dialog ? '' : ' (нет дерева)'}</option>`).join('') || '<option value="">нет карточек</option>'}</select></label>
      <button id="kmpTalk">💬 Начать разговор</button>${v.npcCards.length ? '' : '<span class="hint">В хранилище нет заметок с «тип: npc». Примеры — docs/primery-kartochek.</span>'}</div>`;
  const T = v.talk;
  if (!T) return h + `<div class="card" style="margin:6px 0">Сейчас никто не разговаривает. ${M ? 'Выберите NPC и начните разговор.' : 'Разговор начинает мастер.'}</div>`;
  if (!heroes.length) return h + `<div class="hint">В отряде нет героев — говорить некому.</div>`;
  h += `<div class="card" style="margin:6px 0"><div class="small">${esc(T.npc)} — ${esc(T.attitude)} ${kq('отношение')}${T.langs.length ? ' · говорит: ' + T.langs.map(esc).join(', ') : ''}</div>
    ${T.lines.map(l => `<div style="margin:4px 0">${esc(l)}</div>`).join('') || '<div class="hint">(реплики нет — мастер говорит своими словами)</div>'}
    <div class="row"><label>Говорит <select id="kTalkHero">${kmpOpt(heroes, KMP.speaker)}</select></label>${M ? '<button id="kmpTalkEnd">Закончить разговор</button>' : ''}</div>
    ${T.opts.map(o => `<div style="margin:3px 0"><button data-say="${o.i}"${o.ok ? '' : ' disabled'}>${esc(o.text)}</button>
      ${o.check ? `<span class="small"> 🎲 ${esc(o.check)}</span>` : ''}${o.ok ? '' : `<span class="small" style="opacity:.6"> нужно: ${esc(o.cond.join('; '))}</span>`}</div>`).join('')}
    ${T.opts.length ? '' : '<div class="hint">Вариантов нет — мастер ведёт сам или заканчивает разговор.</div>'}
    ${T.shop && T.shop.length ? '<div class="sec-h">Лавка</div>' + T.shop.map((x, i) => `<div class="row small">${esc(x['вещь'])} — ${esc(x['цена'] || '')}
      <button data-shop="${i}">Купить</button></div>`).join('') : ''}</div>`;
  return h;
}

function kmpTabLog(v, s, M){
  let h = M ? `<div class="row"><button id="kmpUndo">↶ Отменить последнее событие</button><button id="kmpMd">Скачать журнал (markdown)</button></div>` : '';
  if (!v.journal.length) return h + '<div class="hint">Журнал пуст.</div>';
  return h + `<div class="small" style="max-height:520px;overflow:auto">` + v.journal.slice().reverse().map(e => e.hidden && !e.checks ? `<div>${e.n}. ${esc(e.text)}</div>` :
    `<div style="margin:4px 0"><b>${e.n}.</b> ${kmpEvText(e)}${e.edit ? ' <i>(правка мастера)</i>' : ''}${(e.checks || []).map(kmpCheckLine).join('')}</div>`).join('') + '</div>';
}

function kmpTabDec(v){
  if (!v.drafts.length) return '<div class="hint">Решать нечего. Здесь появляются запросы игроков и ваши броски — до того, как попасть в журнал.</div>';
  return v.drafts.map(d => { const e = d.ev;
    return `<div class="card" style="margin:6px 0"><div><b>${esc(kmpEvTitle(e))}</b> · от ${esc(d.who)}${e.attempt ? ` · переброс ${e.attempt}` : ''}${e.hidden ? ' · скрыто от игроков' : ''}</div>
      ${(e.checks || []).map(kmpCheckLine).join('')}<div>${kmpEvText(e)}</div>
      <div class="row"><button class="primary" data-k="acc" data-d="${d.id}">Принять</button><button data-k="rr" data-d="${d.id}">Перебросить</button>
      <button data-k="ed" data-d="${d.id}">Править итог</button><button data-k="cn" data-d="${d.id}">Отменить</button></div>
      <textarea id="kmpEd${d.id}" style="display:none;width:100%;height:110px;font-family:monospace">${esc(JSON.stringify(e.res || {}, null, 1))}</textarea></div>`; }).join('');
}

function kmpBind(){
  const val = id => $(id) ? $(id).value : '', on = id => $(id) && $(id).checked, click = (id, f) => { if ($(id)) $(id).onclick = f; };
  $('kmpBox').querySelectorAll('[data-sub]').forEach(b => b.onclick = () => { if (b.dataset.sub === 'dec' && KMP.sub !== 'dec') KMP.back = KMP.sub; KMP.sub = b.dataset.sub; kmpRender(); });
  click('kmpBack', kmpHome);
  click('kmpCopy', () => { const t = $('kmpCopy').title; (navigator.clipboard ? navigator.clipboard.writeText(t) : Promise.reject()).then(() => toast('Ссылка скопирована: ' + t), () => prompt('Ссылка для игроков:', t)); });
  if (KMP.sub === 'map') kmBind();
  $('kmpBox').querySelectorAll('button[data-d]').forEach(b => b.onclick = () => {
    const d = +b.dataset.d, k = b.dataset.k;
    if (k === 'ed'){ const t = $('kmpEd' + d); t.style.display = t.style.display === 'none' ? '' : 'none'; return; }
    if (k === 'acc'){ const t = $('kmpEd' + d); let edit = null;
      if (t && t.style.display !== 'none') try { edit = JSON.parse(t.value); } catch (e) { toast('итог — не JSON: ' + e.message); return; }
      kmpCall({cmd: 'kmp_accept', draft: d, edit}); }
    if (k === 'rr') kmpCall({cmd: 'kmp_reroll', draft: d});
    if (k === 'cn') kmpCall({cmd: 'kmp_cancel', draft: d});
  });
  click('kmpMine', () => { if (!lastPrice || !lastPrice.ok){ toast('Персонаж не собран: проверьте вкладку «Персонаж»'); return; }
    kmpPropose({type: 'hero', name: S.name || kmpName(), char: toChar()}); });
  click('kmpCheck', () => kmpPropose({type: 'check', hero: val('kChHero'), what: val('kChWhat'), dc: +val('kChDC') || 10, adv: on('kChAdv'), dis: on('kChDis'), hidden: on('kChHid')}));
  click('kmpTrade', () => { const a = {type: val('kTrK'), who: val('kTrWho'), item: val('kTrItem').trim(), qty: +val('kTrQty') || 1, price: val('kTrPrice'), npc: val('kTrNpc')};
    if (!a.item){ toast('Какая вещь?'); return; }
    if (!a.price){ toast('Цена за штуку: например «5 см» или «2 зм»'); return; }
    if (on('kTrH')) a.haggle = {hero: val('kTrHero'), what: val('kTrSk'), vs: a.npc || 'торговец', vsMod: +val('kTrVs') || 0};
    kmpPropose(a); });
  if ($('kTalkHero')) $('kTalkHero').onchange = () => { KMP.speaker = val('kTalkHero'); kmpCall({cmd: 'kmp_open'}); };
  $('kmpBox').querySelectorAll('button[data-say]').forEach(b => b.onclick = () => kmpCall({cmd: 'kmp_say', i: +b.dataset.say, hero: KMP.speaker}));
  $('kmpBox').querySelectorAll('button[data-shop]').forEach(b => b.onclick = () => { const x = KMP.v.talk.shop[+b.dataset.shop];
    kmpPropose({type: 'buy', who: 'отряд', item: x['вещь'], qty: 1, price: x['цена'] || '0 мм', npc: KMP.v.talk.npc}); });
  if (!KMP.v.master) return;
  click('kmpTalk', () => val('kNpcSel') && kmpCall({cmd: 'kmp_talk', npc: val('kNpcSel')}));
  click('kmpTalkEnd', () => kmpCall({cmd: 'kmp_talk_end'}));
  click('kmpGroup', () => kmpPropose({type: 'group', what: val('kChWhat'), dc: +val('kChDC') || 10}));
  click('kmpDay', () => { const roles = {};
    $('kmpBox').querySelectorAll('select[data-role]').forEach(sel => { const n = sel.dataset.role, hp = $('kmpBox').querySelector(`select[data-help="${CSS.escape(n)}"]`);
      if (sel.value === 'помощь' && hp && hp.value) roles[n] = 'помощь:' + hp.value; else if (sel.value !== 'без занятия') roles[n] = sel.value; });
    kmpPropose({type: 'day', ...(val('kDayT') ? {terrain: val('kDayT')} : {}), road: on('kDayRoad'), hours: +val('kDayH') || 8, pace: val('kDayP'), hot: on('kDayHot'), roles}); });
  click('kmpShort', () => { const spend = {}; $('kmpBox').querySelectorAll('input[data-hd]').forEach(i => { if (+i.value) spend[i.dataset.hd] = +i.value; });
    kmpPropose({type: 'rest', kind: 'короткий', spend}); });
  click('kmpLong', () => kmpPropose({type: 'rest', kind: 'долгий'}));
  click('kmpLearn', () => val('kFact') && kmpDirect({type: 'learn', fact: val('kFact')}));
  click('kmpTurn', () => val('kFact') && kmpPropose({type: 'turnin', quest: val('kQ'), fact: val('kFact'), party: val('kQParty') || 'мы'}));
  click('kmpQuest', () => val('kQ') && kmpDirect({type: 'quest', id: val('kQ'), base: val('kQBase'), step: +val('kQStep') || 0.9, confirm: +val('kQConf') || 0.5}));
  click('kmpGm', () => { const a = {type: 'gm'}, who = val('kGmWho');
    if (who !== 'отряд') a.hero = who;
    if (val('kGmPurse')) a.purse = val('kGmPurse').trim().startsWith('-') ? -kmpParse(val('kGmPurse')) : val('kGmPurse');
    if (val('kGmItem')) a.item = val('kGmItem'), a.qty = +val('kGmQty') || 1;
    if (val('kGmHp') !== '') a.hp = +val('kGmHp');
    if (val('kGmExh') !== '') a.exh = +val('kGmExh');
    if (+val('kGmHours')) a.hours = +val('kGmHours');
    if (+val('kGmFood')) a.food = +val('kGmFood');
    if (+val('kGmWater')) a.water = +val('kGmWater');
    if (val('kGmPlace')) a.place = val('kGmPlace');
    if (val('kGmText')) a.text = val('kGmText');
    if (Object.keys(a).length < 2 || (Object.keys(a).length === 2 && a.hero)){ toast('Нечего делать: заполните хотя бы одно поле'); return; }
    kmpDirect(a); });
  click('kmpAtt', () => val('kNpc') && kmpDirect(Object.assign({type: 'attitude', npc: val('kNpc'), delta: +val('kNpcD') || 0}, val('kNpcT') ? {tier: val('kNpcT')} : {})));
  click('kmpRep', () => val('kRep') && kmpDirect({type: 'rep', faction: val('kRep'), delta: +val('kRepD') || 0}));
  click('kmpSet', () => kmpDirect({type: 'setup', traveler: val('kSetT'), lifestyle: val('kSetL'), returnTrip: on('kSetR')}));
  click('kmpUndo', () => confirm('Отменить последнее событие журнала?') && kmpCall({cmd: 'kmp_undo'}));
  click('kmpMd', async () => { const v = await api({cmd: 'kmp_export', id: KMP.id, name: kmpName()});
    if (v.type !== 'kmp_export'){ toast(v.err || 'ошибка'); return; }
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([v.md], {type: 'text/markdown'})); a.download = v.title + ' — журнал.md'; a.click(); });
}
// «10 зм 5 см» → медные (для вычитания денег мастером)
function kmpParse(t){ let tot = 0; t.replace(/(\d+)\s*(зм|см|мм)?/g, (m, n, u) => { tot += +n * (u === 'зм' ? 100 : u === 'см' ? 10 : 1); }); return tot; }

// ---- карта мира (ТЗ v2 §2, §3): куски 256 × 256 клеток по уровням обзора, координаты — км ----
const KM_COL = ['#d8d09e', '#3f8f3f', '#b5a46a', '#8a6a4a', '#2e8b8b', '#4a7fd6', '#b22222', '#5f86b5', '#e8d28c', '#eef2f7'];
const KM_LEG = [0, 1, 2, 3, 4, 8, 9, 6, 7];
const KM_RGB = KM_COL.map(x => [1, 3, 5].map(i => parseInt(x.substr(i, 2), 16)));
const KMV = {cx: 0, cy: 0, kpp: 1, tiles: {}, raw: {}, wait: {}, drag: null};   // центр вида (км), км на пиксель, куски: ключ → картинка и данные
async function kmLoadWorld(id){
  const r = await api({cmd: 'kmp_worldinfo', id, name: kmpName()});
  KMP.worldOf = id; KMV.tiles = {}; KMV.raw = {}; KMV.wait = {};
  KMP.world = r.type === 'kmp_worldinfo' ? Object.assign(r.info, {wid: r.wid}) : null;
  if (KMP.world) kmFit();
}
function kmFit(){ const W = KMP.world; KMV.cx = W.w / 2; KMV.cy = W.h / 2; KMV.kpp = Math.max(W.w, W.h) / 900; }
const kmLevel = () => Math.max(0, Math.min(KMP.world.zMax, Math.floor(Math.log2(Math.max(1e-9, KMV.kpp / KMP.world.cellKm)))));
function kmTileCanvas(t){   // кусок → картинка: цвет местности, выше — темнее, дорога, река
  const c = document.createElement('canvas'); c.width = c.height = 256;
  const g = c.getContext('2d'), im = g.createImageData(256, 256);
  for (let k = 0; k < 65536; k++){
    const ter = t.terr.charCodeAt(k) - 48, lv = t.level.charCodeAt(k) - 48, f = t.flags.charCodeAt(k) - 48;
    let p = KM_RGB[ter];
    if (ter !== 7) { const sh = 1 - Math.max(0, lv - 2) * 0.045; p = p.map(v => v * sh); }
    if (f & 1) p = [110, 84, 60];
    if (f & 2) p = [42, 111, 176];
    im.data[k * 4] = p[0]; im.data[k * 4 + 1] = p[1]; im.data[k * 4 + 2] = p[2]; im.data[k * 4 + 3] = 255;
  }
  g.putImageData(im, 0, 0);
  return c;
}
function kmGetTile(wid, z, tx, ty, redraw){
  const key = wid + '|' + z + '|' + tx + '|' + ty;
  if (KMV.tiles[key]) return KMV.tiles[key];
  if (!KMV.wait[key]){
    KMV.wait[key] = 1;
    api({cmd: 'kmp_tile', id: KMP.id, name: kmpName(), wid, z, tx, ty}).then(r => { if (r.type === 'kmp_tile'){ KMV.tiles[key] = kmTileCanvas(r.tile); KMV.raw[key] = r.tile; redraw(); } });
  }
  return null;
}
const kmToPx = (cv, x, y) => [cv.width / 2 + (x - KMV.cx) / KMV.kpp, cv.height / 2 + (y - KMV.cy) / KMV.kpp];
const kmToKm = (cv, px, py) => [KMV.cx + (px - cv.width / 2) * KMV.kpp, KMV.cy + (py - cv.height / 2) * KMV.kpp];
function kmNiceKm(v){ const p = Math.pow(10, Math.floor(Math.log10(v))); return [1, 2, 5, 10].map(m => m * p).find(m => m >= v) || v; }
function kmDraw(){
  const cv = $('kMap'); if (!cv || !KMP.world) return;
  const W = KMP.world, s = KMP.v.state, b = cv.getBoundingClientRect();
  if (cv.width !== Math.round(b.width)) cv.width = Math.round(b.width);
  cv.height = 560;
  const g = cv.getContext('2d'); g.imageSmoothingEnabled = false;
  g.fillStyle = '#1b2a3a'; g.fillRect(0, 0, cv.width, cv.height);
  const z = kmLevel(), ck = W.cellKm * (1 << z), span = ck * 256;
  const [x0, y0] = kmToKm(cv, 0, 0), [x1, y1] = kmToKm(cv, cv.width, cv.height);
  for (let ty = Math.max(0, Math.floor(y0 / span)); ty <= Math.min(Math.ceil(W.h / span) - 1, Math.floor(y1 / span)); ty++)
    for (let tx = Math.max(0, Math.floor(x0 / span)); tx <= Math.min(Math.ceil(W.w / span) - 1, Math.floor(x1 / span)); tx++){
      const [px, py] = kmToPx(cv, tx * span, ty * span), sz = span / KMV.kpp;
      let im = kmGetTile(W.wid, z, tx, ty, kmDraw);
      if (!im && z < W.zMax){ const pz = z + 1, ps = span * 2, ptx = Math.floor(tx / 2), pty = Math.floor(ty / 2), pim = KMV.tiles[W.wid + '|' + pz + '|' + ptx + '|' + pty];   // пока грузится — крупнее
        if (pim){ g.drawImage(pim, (tx % 2) * 128, (ty % 2) * 128, 128, 128, px, py, sz, sz); continue; } }
      if (im) g.drawImage(im, px, py, sz + 0.5, sz + 0.5);
    }
  // поселения: крупные — всегда, мелкие — когда близко
  g.textBaseline = 'middle';
  for (const t of W.towns){
    const [px, py] = kmToPx(cv, t[1], t[2]); if (px < -50 || py < -20 || px > cv.width + 50 || py > cv.height + 20) continue;
    const big = t[3] >= 20000, mid = t[3] >= 5000;
    if (!big && !(mid && KMV.kpp < 2) && KMV.kpp > 0.5) continue;
    g.fillStyle = big ? '#b22222' : '#222'; g.beginPath(); g.arc(px, py, big ? 4 : 3, 0, 7); g.fill();
    g.font = big ? 'bold 12px sans-serif' : '11px sans-serif'; g.strokeStyle = 'rgba(255,255,255,.7)'; g.lineWidth = 3; g.strokeText(t[0], px + 6, py - 7); g.fillStyle = '#111'; g.fillText(t[0], px + 6, py - 7);
  }
  const line = (pts, col, dash) => { if (pts.length < 2) return; g.strokeStyle = col; g.lineWidth = 3; g.setLineDash(dash || []); g.beginPath();
    pts.forEach((p, i) => { const [x, y] = kmToPx(cv, p[0], p[1]); i ? g.lineTo(x, y) : g.moveTo(x, y); }); g.stroke(); g.setLineDash([]); };
  const from = s.pos ? [s.pos] : [];
  line(from.concat(s.route), '#ffcc00');
  if (KMP.preview && KMP.preview.path) line(from.concat(KMP.preview.path), '#ffffff', [6, 5]);
  else line(from.concat(KMP.draft), 'rgba(255,255,255,.6)', [2, 6]);
  for (const p of KMP.draft){ const [x, y] = kmToPx(cv, p[0], p[1]); g.fillStyle = '#fff'; g.strokeStyle = '#000'; g.lineWidth = 1; g.fillRect(x - 4, y - 4, 8, 8); g.strokeRect(x - 4, y - 4, 8, 8); }
  if (s.route.length){ const e = s.route[s.route.length - 1], [x, y] = kmToPx(cv, e[0], e[1]); g.fillStyle = '#ffcc00'; g.beginPath(); g.arc(x, y, 5, 0, 7); g.fill(); }
  if (s.pos){ const [x, y] = kmToPx(cv, s.pos[0], s.pos[1]); g.fillStyle = '#e8112d'; g.strokeStyle = '#fff'; g.lineWidth = 2; g.beginPath(); g.arc(x, y, 7, 0, 7); g.fill(); g.stroke(); }
  // масштаб: линейка и размер клетки уровня
  const km = kmNiceKm(KMV.kpp * 110), px = km / KMV.kpp, ckTxt = ck < 1 ? Math.round(ck * 1000) + ' м' : ck.toFixed(ck < 10 ? 1 : 0) + ' км';
  $('kmScale').innerHTML = `<span style="display:inline-block;width:${Math.round(px)}px;border-bottom:3px solid #fff;margin-right:6px;vertical-align:middle"></span>${km} км · клетка ${ckTxt} · мир ${W.w}×${W.h} км`;
}
function kmHoverInfo(e){
  const cv = $('kMap'), b = cv.getBoundingClientRect(), [x, y] = kmToKm(cv, (e.clientX - b.left) * cv.width / b.width, (e.clientY - b.top) * cv.height / b.height);
  const W = KMP.world, s = KMP.v.state;
  if (x < 0 || y < 0 || x > W.w || y > W.h){ $('kmHover').textContent = 'за краем мира'; return; }
  const z = kmLevel(), ck = W.cellKm * (1 << z), span = ck * 256, tx = Math.floor(x / span), ty = Math.floor(y / span), im = KMV.tiles[W.wid + '|' + z + '|' + tx + '|' + ty];
  let txt = '';
  const raw = KMV.raw && KMV.raw[W.wid + '|' + z + '|' + tx + '|' + ty];
  if (raw){ const k = Math.floor((y - ty * span) / ck) * 256 + Math.floor((x - tx * span) / ck), ter = raw.terr.charCodeAt(k) - 48, f = raw.flags.charCodeAt(k) - 48;
    txt = `${W.terrains[ter]}, высота ${raw.level[k]}${f & 1 ? ', дорога' : ''}${f & 2 ? ', река' : ''}`; }
  else if (im) txt = '…';
  if (s.pos){ const d = Math.hypot(x - s.pos[0], y - s.pos[1]), kmDay = {'пеший': 35, 'всадник': 47, 'гонец': 85, 'армия': 25, 'обоз': 15}[s.traveler] || 35;
    txt += ` · от отряда ${d < 10 ? d.toFixed(1) : Math.round(d)} км по прямой — ${(d / kmDay).toFixed(1)} дн. по дороге, ${(d / (kmDay * 0.5)).toFixed(1)} дн. по лесу`; }
  $('kmHover').textContent = txt + ` · ${x.toFixed(1)}, ${y.toFixed(1)} км`;
}
function kmBind(){
  if (!KMP.world){ kmBindWorld(); return; }
  if (!KMV.cx && !KMV.cy) kmFit();
  kmDraw();   // сразу: requestAnimationFrame в фоновой вкладке не срабатывает
  const cv = $('kMap'), W = KMP.world;
  const zoom = (f, px, py) => { const [x, y] = px == null ? [KMV.cx, KMV.cy] : kmToKm(cv, px, py);
    KMV.kpp = Math.min(Math.max(W.w, W.h) / 200, Math.max(W.cellKm / 6, KMV.kpp * f)); const [nx, ny] = px == null ? [KMV.cx, KMV.cy] : kmToKm(cv, px, py);
    KMV.cx += x - nx; KMV.cy += y - ny; kmDraw(); };
  const pxOf = e => { const b = cv.getBoundingClientRect(); return [(e.clientX - b.left) * cv.width / b.width, (e.clientY - b.top) * cv.height / b.height]; };
  cv.onwheel = e => { e.preventDefault(); const [px, py] = pxOf(e); zoom(e.deltaY > 0 ? 1.25 : 0.8, px, py); };
  cv.onpointerdown = e => { KMV.drag = {x: e.clientX, y: e.clientY, cx: KMV.cx, cy: KMV.cy, moved: false}; cv.setPointerCapture(e.pointerId); cv.style.cursor = 'grabbing'; };
  cv.onpointermove = e => { kmHoverInfo(e); const d = KMV.drag; if (!d) return; const b = cv.getBoundingClientRect(), k = cv.width / b.width;
    if (Math.abs(e.clientX - d.x) + Math.abs(e.clientY - d.y) > 4) d.moved = true;
    KMV.cx = d.cx - (e.clientX - d.x) * k * KMV.kpp; KMV.cy = d.cy - (e.clientY - d.y) * k * KMV.kpp; kmDraw(); };
  cv.onpointerup = e => { const d = KMV.drag; KMV.drag = null; cv.style.cursor = 'grab'; if (!d || d.moved) return;
    const [x, y] = kmToKm(cv, ...pxOf(e)); const p = [+x.toFixed(3), +y.toFixed(3)];
    if (KMP.mode === 'pos'){ KMP.mode = ''; kmpDirect({type: 'gm', pos: p, text: 'отряд перенесён'}); return; }
    if (KMP.mode !== 'route') { toast('Чтобы проложить путь, нажмите «✏ Прокладывать путь щелчками»'); return; }
    KMP.draft.push(p); kmPreview(); };
  $('kmIn').onclick = () => zoom(0.5); $('kmOut').onclick = () => zoom(2); $('kmHome').onclick = () => { kmFit(); kmDraw(); };
  if ($('kmParty')) $('kmParty').onclick = () => { const s = KMP.v.state; KMV.cx = s.pos[0]; KMV.cy = s.pos[1]; KMV.kpp = Math.min(KMV.kpp, 0.5); kmDraw(); };
  $('kmRoute').onclick = () => { KMP.mode = KMP.mode === 'route' ? '' : 'route'; kmpRender(); };
  $('kmClear').onclick = () => { KMP.draft = []; KMP.preview = null; kmpRender(); };
  $('kmMode').onchange = () => { KMP.pmode = $('kmMode').value; kmPreview(); };
  $('kmSend').onclick = async () => { const v = await kmpPropose({type: 'route', way: KMP.draft, mode: KMP.pmode});
    if (v.type === 'kmp'){ KMP.draft = []; KMP.preview = null; KMP.mode = ''; kmpRender(); } };
  const town = () => W.towns.find(t => t[0] === $('kmTown').value.trim());
  $('kmTownAdd').onclick = () => { const t = town(); if (!t){ toast('нет такого поселения'); return; } KMP.draft.push([t[1], t[2]]); kmPreview(); };
  if ($('kmTownPos')) $('kmTownPos').onclick = () => { const t = town(); if (!t){ toast('нет такого поселения'); return; } kmpDirect({type: 'gm', pos: [t[1], t[2]], place: t[0]}); };
  if ($('kmPos')) $('kmPos').onclick = () => { KMP.mode = KMP.mode === 'pos' ? '' : 'pos'; kmpRender(); };
  kmBindWorld();
}

// ---- путь (ТЗ v2 §4): движок ищет путь через пункты по режиму; до утверждения — предпросмотр со сводкой ----
function kmSumText(){
  const P = KMP.preview;
  if (!KMP.draft.length) return KMP.v && KMP.v.state.route.length ? 'Новый маршрут заменит текущий. Пункты — щелчками или «+ в маршрут».' : 'Пункты маршрута — щелчками («✏ Прокладывать путь») или «+ в маршрут»; путь движок найдёт сам.';
  if (!P) return 'ищу путь…';
  if (P.err) return '⚠ ' + P.err;
  const S = P.sum, terr = Object.entries(S.terr).sort((a, b) => b[1] - a[1]).map(([t, k]) => `${t} ${k} км`).join(', ');
  return `<b>${P.mode}: ${S.km} км, ≈ ${S.days} дн.</b> (${esc(KMP.v.state.traveler)}, обычный темп) · по дорогам ${S.road}% · ${esc(terr)}${S.blocked ? ' · ⚠ путь упирается в воду' : ''}`;
}
async function kmPreview(){
  KMP.preview = null; kmpRender();
  if (!KMP.draft.length) return;
  const seq = (KMP.pseq = (KMP.pseq || 0) + 1);
  const r = await api({cmd: 'kmp_path', id: KMP.id, name: kmpName(), way: KMP.draft, mode: KMP.pmode});
  if (seq !== KMP.pseq) return;   // пока считали, пункты сменились
  KMP.preview = r.type === 'kmp_path' ? r.res : {err: r.err || 'путь не найден'};
  if ($('kmSum')) $('kmSum').innerHTML = kmSumText();
  if ($('kmSend')) $('kmSend').disabled = !(KMP.preview && KMP.preview.path);
  kmDraw();
}

// ---- мир кампании: мастер выбирает — генератор по параметрам (с предпросмотром) или Акт I ----
const KM_GEN = [['size', 'Размер мира, км', 50, 5000, 50, 800], ['sea', 'Доля моря', 0, 0.9, 0.05, 0.3], ['mount', 'Гористость', 0, 1, 0.05, 0.5],
  ['rough', 'Изрезанность (холмы, берег)', 0, 1, 0.05, 0.5], ['climate', 'Климат: холодно ← → жарко', -1, 1, 0.1, 0], ['humid', 'Влажность: сухо ← → сыро', -1, 1, 0.1, 0],
  ['towns', 'Поселений на 100 × 100 км', 0.1, 6, 0.1, 1], ['rivers', 'Густота рек', 0.1, 3, 0.1, 1]];
function kmWorldPanel(){
  const W = KMP.world, G = KMP.gen;
  let h = `<details class="card" style="margin:6px 0"${G ? ' open' : ''}><summary><b>Мир:</b> ${W ? esc(W.name) + ` — ${W.w}×${W.h} км, поселений ${W.towns.length}` : 'нет'} <span class="small">(сменить мир, создать новый)</span></summary>`;
  if (!G) return h + `<div class="row"><button id="kmGenOpen" class="primary">Новый мир…</button><button id="kmAct1">Европа 1208 (Акт I)</button>
    <span class="hint">Смена мира не двигает отряд: поставьте его заново.</span></div></details>`;
  h += `<div class="hint">Подберите параметры и нажмите «Показать» — мир кампании не изменится, пока вы не нажмёте «Взять этот мир».</div>
    <div class="row"><label>Название <input id="kgName" value="${esc(G.name)}" style="width:180px"></label><label>Зерно ${kq('зерно')} <input id="kgSeed" type="number" value="${G.params.seed}" style="width:110px"></label><button id="kgDice" title="Другое зерно">🎲</button></div>
    <div class="grid2">${KM_GEN.map(([k, n, lo, hi, st]) => `<label>${n} <input type="range" data-kg="${k}" min="${lo}" max="${hi}" step="${st}" value="${G.params[k]}"> <span class="tnum" data-kgv="${k}">${G.params[k]}</span></label>`).join('')}</div>
    <div class="row"><button id="kgShow" class="primary">Показать</button><button id="kgUse"${G.info ? '' : ' disabled'}>Взять этот мир</button><button id="kgCancel">Отмена</button>
    <span class="small" id="kgMsg">${G.info ? `${G.info.w}×${G.info.h} км, поселений ${G.info.towns.length}, клеток ${Math.ceil(G.info.w / G.info.cellKm)}² по 305 м` : ''}</span></div>
    <canvas id="kgCv" width="512" height="512" style="width:100%;max-width:512px;image-rendering:pixelated;border-radius:8px;display:${G.info ? 'block' : 'none'}"></canvas></details>`;
  return h;
}
function kmBindWorld(){
  const click = (id, f) => { if ($(id)) $(id).onclick = f; };
  click('kmGenOpen', () => { KMP.gen = {name: 'Новый мир', params: Object.fromEntries(KM_GEN.map(x => [x[0], x[5]]).concat([['seed', 1 + Math.floor(Math.random() * 99999)]])), info: null}; kmpRender(); });
  click('kmAct1', () => confirm('Сделать миром кампании Европу 1208 (карту Акта I)?') && kmpCall({cmd: 'kmp_world_use', act1: true}));
  if (!KMP.gen) return;
  const G = KMP.gen;
  document.querySelectorAll('[data-kg]').forEach(r => r.oninput = () => { G.params[r.dataset.kg] = +r.value; document.querySelector(`[data-kgv="${r.dataset.kg}"]`).textContent = r.value; });
  $('kgName').oninput = () => { G.name = $('kgName').value; };
  $('kgSeed').oninput = () => { G.params.seed = +$('kgSeed').value || 1; };
  click('kgDice', () => { G.params.seed = 1 + Math.floor(Math.random() * 999999); $('kgSeed').value = G.params.seed; });
  click('kgCancel', () => { KMP.gen = null; kmpRender(); });
  click('kgShow', async () => { $('kgMsg').textContent = 'строю мир…';
    const r = await api({cmd: 'kmp_world_preview', id: KMP.id, name: kmpName(), params: G.params});
    if (r.type !== 'kmp_preview'){ $('kgMsg').textContent = r.err || 'ошибка'; return; }
    G.info = r.info; G.tiles = []; kmpRender(); kmPreviewDraw(); });
  click('kgUse', () => confirm(`Сделать «${G.name}» миром кампании? Отряд останется на тех же числах км — поставьте его заново.`) &&
    kmpCall({cmd: 'kmp_world_use', title: G.name, params: G.params}).then(v => { if (v.type === 'kmp'){ KMP.gen = null; kmFit(); kmpRender(); } }));
  if (G.info) kmPreviewDraw();
}
async function kmPreviewDraw(){   // обзор всего мира предпросмотра: куски верхнего уровня
  const G = KMP.gen, cv = $('kgCv'); if (!G || !G.info || !cv) return;
  const z = G.info.zMax, n = Math.ceil(Math.max(G.info.w, G.info.h) / (G.info.cellKm * (1 << z) * 256));
  const g = cv.getContext('2d'); g.fillStyle = '#1b2a3a'; g.fillRect(0, 0, 512, 512); g.imageSmoothingEnabled = false;
  const sz = 512 / n;
  for (let ty = 0; ty < n; ty++) for (let tx = 0; tx < n; tx++){
    const r = await api({cmd: 'kmp_tile', id: KMP.id, name: kmpName(), wid: 'предпросмотр', z, tx, ty});
    if (r.type === 'kmp_tile' && $('kgCv') === cv) g.drawImage(kmTileCanvas(r.tile), tx * sz, ty * sz, sz, sz);
  }
  const k = 512 / (n * G.info.cellKm * (1 << z) * 256);
  g.fillStyle = '#b22222';
  for (const t of G.info.towns) if (t[3] >= 20000){ g.beginPath(); g.arc(t[1] * k, t[2] * k, 3, 0, 7); g.fill(); }
}
