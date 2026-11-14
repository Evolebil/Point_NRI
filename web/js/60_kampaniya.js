// ====================================================================================================================
// Кампания (исследование и социалка; ТЗ docs/TZ_kampaniya_v2.md §6): экран входа и вкладки внутри кампании.
// Вкладку и её раздел создаёт этот файл (play.html не меняется). Правила — build/kampaniya, журнал — web/kampaniya.py.
// Кампания = комната с кодом (Р25): мастер создаёт и даёт код или ссылку (/?k=КОД), игроки входят. Мастер решает по каждому
// черновику: принять, перебросить, править итог, отменить; игрок предлагает.
// ====================================================================================================================
// sub — вкладка внутри кампании; draft — маршрут, который прокладывают щелчками; mode — 'route' | 'pos'; speaker — кто говорит
const KMP = {id: '', v: null, timer: 0, sub: 'map', speaker: '', world: null, worldOf: '', draft: [], mode: '', justCreated: false, gen: null, pmode: 'быстрее всего', preview: null, mapStyle: 'physical', layers: {water: true, relief: true, cover: true, landforms: true, biome: false, rivers: true, roads: true, realm: false, people: false, settlements: true, labels: true, grid: false, compass: true, game: true}};
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
// экран «Кампания» (раздел s7 в play.html): вход — список кампаний или открытая кампания
ROUTE_ENTER.campaign = () => { KMP.id ? kmpCall({cmd: 'kmp_open'}) : kmpHome(); };
ROUTE_BACK.campaign = () => { if (KMP.id){ KMP.id = ''; clearTimeout(KMP.timer); kmpHome(); } else go('home'); };
{ // ссылка с кодом кампании: /?k=КОД — сразу в неё
  const k = new URLSearchParams(location.search).get('k');
  if (k && /^[A-Za-z0-9]{5}$/.test(k)) setTimeout(() => { KMP.id = k.toUpperCase(); go('campaign'); }, 300);
}

// ---- экран входа: мои кампании, новая, по коду ----
async function kmpHome(){
  KMP.id = ''; KMP.v = null; clearTimeout(KMP.timer);
  $('kmpBox').innerHTML = '';
  const who = kmpName();
  const intro = `<h1 class="page-title">Кампания</h1>`;
  if (!who){ $('kmpHome').innerHTML = intro + `<div class="panel">Сначала задайте имя игрока в настройках.</div>`; return; }
  const v = await api({cmd: 'kmp_list', name: who});
  const list = v.type === 'kmp_list' ? v.list : [];
  const ago = t => { const m = Math.round((Date.now() / 1000 - t) / 60); return m < 1 ? 'только что' : m < 60 ? m + ' мин назад' : m < 1440 ? Math.round(m / 60) + ' ч назад' : Math.round(m / 1440) + ' дн. назад'; };
  const rows = list.length ? list.map(c => `<div class="list-row simple"><span class="name"><b>${esc(c.title)}</b><span class="meta">код ${esc(c.id)} · ${c.master ? 'вы мастер' : 'мастер: ' + esc(c.masterName)} · героев ${c.heroes} · игроков ${c.players} · ${ago(c.t)}</span></span>
      <span class="acts-r"><button data-open="${esc(c.id)}">Открыть</button>${c.master ? `<button class="danger" data-delete="${esc(c.id)}" data-title="${esc(c.title)}">Удалить</button>` : ''}</span></div>`).join('')
    : '<div class="empty">Пока нет кампаний. Создайте свою (вы станете мастером) или войдите по коду мастера.</div>';
  $('kmpHome').innerHTML = `${intro}<div class="cols cols-side"><div class="panel"><h2>Мои кампании</h2><div class="list">${rows}</div></div>
    <aside style="display:flex;flex-direction:column;gap:24px"><div class="panel"><h2>Новая кампания</h2><p class="muted small">Вы станете мастером. После создания появится код для игроков.</p>
      <label class="field">Название<input id="kmpTitle" placeholder="например, Акт I" maxlength="60"></label><button class="primary wide" id="kmpNew">Создать</button></div>
    <div class="panel"><h2>Войти по коду</h2><label class="field">Код из пяти знаков<input id="kmpCode" class="code-input" maxlength="5" placeholder="XXXXX" autocomplete="off" spellcheck="false"></label><button class="primary wide" id="kmpJoin">Войти</button></div></aside></div>`;
  $('kmpHome').querySelectorAll('button[data-open]').forEach(b => b.onclick = () => { KMP.id = b.dataset.open; kmpCall({cmd: 'kmp_open'}); });
  $('kmpHome').querySelectorAll('button[data-delete]').forEach(b => b.onclick = () => kmpDeleteDialog(b));
  $('kmpNew').onclick = async () => { const title=$('kmpTitle').value.trim();if(!title||title.length>60){toast('Введите название кампании (1–60 символов)');$('kmpTitle').focus();return;}KMP.justCreated = true; const r = await kmpCall({cmd: 'kmp_create', title}); if (r.type !== 'kmp') KMP.justCreated = false; };
  const join = () => { const c = $('kmpCode').value.trim().toUpperCase(); if (c.length !== 5){ toast('код — пять знаков'); return; } KMP.id = c; kmpCall({cmd: 'kmp_open'}); };
  $('kmpJoin').onclick = join; $('kmpCode').oninput = () => { $('kmpCode').value=$('kmpCode').value.toUpperCase().replace(/[^A-Z0-9]/g,'').slice(0,5); }; $('kmpCode').onkeydown = e => { if (e.key === 'Enter') join(); };
  $('kmpTitle').onkeydown = e => { if (e.key === 'Enter') $('kmpNew').click(); };
}

function kmpDeleteDialog(button){
  const id = button.dataset.delete, title = button.dataset.title;
  document.getElementById('kmpDeleteDialog')?.remove();
  document.body.insertAdjacentHTML('beforeend', `<div id="kmpDeleteDialog" class="kmp-delete-backdrop"><div class="kmp-delete-dialog" role="dialog" aria-modal="true" aria-label="Подтверждение удаления кампании"><h2 style="margin-bottom:12px">Подтвердите</h2><p>Удалить кампанию «${esc(title)}»? Это действие нельзя отменить.</p><div class="err" data-delete-error style="display:none"></div><div class="row"><button type="button" class="danger" data-delete-confirm>Удалить</button><button type="button" data-delete-cancel>Отмена</button></div></div></div>`);
  const modal = $('kmpDeleteDialog'), cancel = modal.querySelector('[data-delete-cancel]'), remove = modal.querySelector('[data-delete-confirm]'), error = modal.querySelector('[data-delete-error]');
  const close = () => { modal.remove(); document.removeEventListener('keydown', onKey); button.focus(); };
  const onKey = e => { if (e.key === 'Escape' && modal.isConnected){ e.preventDefault(); close(); } };
  cancel.onclick = close;
  modal.onclick = e => { if (e.target === modal) close(); };
  document.addEventListener('keydown', onKey);
  cancel.focus();
  remove.onclick = async () => {
    remove.disabled = true; remove.textContent = 'Удаляю…'; error.style.display = 'none';
    let r;
    try { r = await api({cmd:'kmp_delete', id, name:kmpName()}); }
    catch (e) { r = {type:'error', err:e.message || String(e)}; }
    if (r.type !== 'kmp_deleted'){
      remove.disabled = false; remove.textContent = 'Удалить';
      error.textContent = `Не удалось удалить кампанию: ${r.err || 'неизвестная ошибка'}. Кампания осталась в списке.`; error.style.display = '';
      return;
    }
    close(); toast(`Кампания «${title}» удалена.`, 4000); kmpHome();
  };
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
    h += kmWorldPanel() + kmBrushPanel(s);
  }
  if (!KMP.world) return h + `<div class="hint">Мира у кампании нет. ${M ? 'Создайте его: «Мир» → «Новый мир».' : 'Мир выбирает мастер.'}</div>`;
  const W = KMP.world;
  h += `<div class="row small"><button id="kmRoute"${KMP.mode === 'route' ? ' class="primary"' : ''}>✏ Прокладывать путь щелчками</button>
      <label>или поселение <input id="kmTown" list="kmTowns" style="width:150px"></label><datalist id="kmTowns">${W.towns.map(t => `<option value="${esc(t[0])}">`).join('')}</datalist>
      <button id="kmTownAdd">+ в маршрут</button><label>путь <select id="kmMode">${kmpOpt(['быстрее всего', 'по дорогам', 'напрямик'], KMP.pmode)}</select></label>
      <div class="row small" aria-label="Профили слоёв"><span>Профиль:</span>${[['physical','Физическая'],['political','Политическая'],['people','Расы'],['master','Мастер (всё)'],['player','Для игроков']].map(([id,n]) => `<button data-kmpreset="${id}"${KMP.mapStyle===id?' class="primary"':''}>${n}</button>`).join('')}</div>
      <details style="display:inline-block"><summary>Слои карты</summary><div class="row small" style="max-width:700px">
        <label><input data-kmlayer="water" type="checkbox"${KMP.layers.water ? ' checked' : ''}> вода</label><label><input data-kmlayer="relief" type="checkbox"${KMP.layers.relief ? ' checked' : ''}> затенение высот</label>
        <label><input data-kmlayer="cover" type="checkbox"${KMP.layers.cover ? ' checked' : ''}> покров</label><label><input data-kmlayer="landforms" type="checkbox"${KMP.layers.landforms ? ' checked' : ''}> горы, холмы, деревья</label>
        <label><input data-kmlayer="biome" type="checkbox"${KMP.layers.biome ? ' checked' : ''}> биомы</label><label><input data-kmlayer="rivers" type="checkbox"${KMP.layers.rivers ? ' checked' : ''}> реки и озёра</label>
        <label><input data-kmlayer="roads" type="checkbox"${KMP.layers.roads ? ' checked' : ''}> дороги</label><label><input data-kmlayer="realm" type="checkbox"${KMP.layers.realm ? ' checked' : ''}${W.realmLayer ? '' : ' disabled'}> границы и государства</label>
        <label><input data-kmlayer="people" type="checkbox"${KMP.layers.people ? ' checked' : ''}${W.peopleLayer ? '' : ' disabled'}> расы</label><label><input data-kmlayer="settlements" type="checkbox"${KMP.layers.settlements ? ' checked' : ''}> поселения</label>
        <label><input data-kmlayer="labels" type="checkbox"${KMP.layers.labels ? ' checked' : ''}> подписи</label><label><input data-kmlayer="grid" type="checkbox"${KMP.layers.grid ? ' checked' : ''}> сетка</label>
        <label><input data-kmlayer="compass" type="checkbox"${KMP.layers.compass ? ' checked' : ''}> компас</label>
        <label><input data-kmlayer="game" type="checkbox"${KMP.layers.game ? ' checked' : ''}> отряд и маршрут</label></div></details>
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
    <div class="small" style="min-height:1.3em"><strong>Под курсором:</strong> <span id="kmHover">наведите на карту — покажу местность и расстояние</span></div>
    <div class="small" id="kmLegend">${kmLegendHtml(KMP.mapStyle, W)}</div>`;
  return h;
}
function kmpTabParty(v, s, M, heroes){
  let h = '';
  if (!s.heroes.length) h += `<div class="card" style="margin:6px 0">Героев пока нет. Соберите героя в «Мои персонажи» (берётся выбранный) и нажмите «➕ Мой герой» — ${M ? 'черновик придёт вам в «Решения»' : 'мастер примет его в отряд'}.</div>`;
  else h += `<table class="small" style="width:100%;border-collapse:collapse"><tr><th align="left">Герой</th><th>Ур.</th><th>ХП</th><th>Кости хитов ${kq('кости хитов')}</th><th>Истощение ${kq('истощение')}</th><th>Голод, дней</th><th>Пасс. Внимат. ${kq('пассивная Внимательность')}</th><th>Кошелёк</th><th align="left">Вещи</th></tr>` +
    s.heroes.map(x => `<tr${x.dead ? ' style="opacity:.5"' : ''}><td>${esc(x.name)}${x.dead ? ' ☠' : ''}</td><td align="center">${x.level}</td><td align="center">${x.hp}/${x.hpMax}</td><td align="center">${x.hd}/${x.hdMax}</td>
      <td align="center">${x.exh}</td><td align="center">${x.daysNoFood}</td><td align="center">${x.passivePerception}</td><td align="center">${kmpMoney(x.purse)}</td>
      <td>${Object.entries(x.inv).map(([k, n]) => esc(k) + ' ×' + n).join(', ') || '—'}</td></tr>`).join('') + '</table>';
  h += `<div class="small" style="margin:6px 0">Путник: ${esc(s.traveler)} · жизнь: ${esc(s.lifestyle)} · груз отряда: ${Object.entries(s.inv).map(([k, n]) => esc(k) + ' ×' + n).join(', ') || '—'}<br>
    Сведения: ${s.facts.map(esc).join('; ') || '—'}<br>NPC ${kq('отношение')}: ${Object.entries(s.npcs).map(([k, n]) => esc(k) + ' — ' + n.attitude + (n.shift ? ` (${n.shift > 0 ? '+' : ''}${n.shift})` : '')).join('; ') || '—'}
    · Задания: ${Object.entries(s.quests).map(([k, q]) => esc(k) + ': за новые сведения ' + kmpMoney(q.tier)).join('; ') || '—'}</div>`;
  h += `<div class="sec-h">Действия</div><div class="row"><button id="kmpMine">➕ Мой герой (выбранный в «Мои персонажи»)</button></div>`;
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
  click('kmpMine', () => { if (!lastPrice || !lastPrice.ok){ toast('Персонаж не собран: откройте его в «Мои персонажи»'); return; }
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
const KM_COL = ['#d8d09e', '#3f8f3f', '#b5a46a', '#8a6a4a', '#2e8b8b', '#4a7fd6', '#b22222', '#5f86b5', '#e8d28c', '#eef2f7', '#11161d'];   // 10 — скрыто туманом
const KM_LEG = [0, 1, 2, 3, 4, 8, 9, 6, 7];
const KM_RGB = KM_COL.map(x => [1, 3, 5].map(i => parseInt(x.substr(i, 2), 16)));
const KM_BIOME_NAME = ['Тундра', 'Тайга', 'Умеренный пояс', 'Средиземноморье', 'Степь', 'Пустыня', 'Горы', 'Болота', 'Древний лес', 'Пустоши'];
const KM_BIOME_COL = ['#dfe9ef', '#5f8a6a', '#cfe0a8', '#e8d48a', '#e0cf8f', '#ead6a6', '#a08060', '#7fbcae', '#3f7f3f', '#8f8f7a'];
const KM_BIOME_RGB = KM_BIOME_COL.map(x => [1, 3, 5].map(i => parseInt(x.substr(i, 2), 16)));
const KM_COVER_NAME = ['вода', 'тундра', 'тайга', 'лес', 'степь', 'поля и луга', 'редколесье', 'пустыня', 'скалы', 'болото'];
const KM_COVER_COL = ['#4e7fa5', '#e1e8e4', '#6c8871', '#397247', '#c5b878', '#d8d19d', '#a1ad77', '#d5bd7b', '#8b8173', '#708f83'];
const KM_COVER_RGB = KM_COVER_COL.map(x => x.match(/[\da-f]{2}/gi).map(v => parseInt(v,16)));
const KM_RELIEF_NAME = ['равнина', 'холмы', 'плато', 'горы', 'высокие горы'];
function kmLegendHtml(_style, W){
  let h = '';
  const item = (color, name) => { h += `<span style="display:inline-block;width:10px;height:10px;background:${color};margin:0 3px 0 8px"></span>${esc(name)}`; };
  if (KMP.layers.water) item(KM_COVER_COL[0], 'море и озёра');
  if (KMP.layers.cover) KM_COVER_NAME.slice(1).forEach((name, i) => item(KM_COVER_COL[i+1], name));
  if (KMP.layers.biome) KM_BIOME_NAME.forEach((name, i) => item(KM_BIOME_COL[i], name));
  if (KMP.layers.relief) KM_RELIEF_NAME.slice(1).forEach(name => item('#6a5947', name));
  if (KMP.layers.landforms) item('#6a5947','значки рельефа и деревьев');
  if (KMP.layers.settlements) [['#b22222','город'],['#333333','деревня']].forEach(([c,n]) => item(c,n));
  if (KMP.layers.realm) { item('#9b87bd','государства'); }
  if (KMP.layers.people) {
    item('#687b9c','ареалы рас');
    const names = [...new Set((W.labels || []).filter(l => l[3] === 'people').map(l => l[0]))];
    if (names.length) h += `<details style="display:inline-block;margin-left:8px"><summary>расы (${names.length})</summary>${names.map(n => `<span style="margin-right:8px">${esc(n)}</span>`).join('')}</details>`;
  }
  if (KMP.layers.roads) h += '<span style="display:inline-block;width:18px;border-top:3px solid #bd3027;margin:0 3px 0 8px"></span>дороги';
  if (KMP.layers.rivers) h += '<span style="display:inline-block;width:18px;border-top:3px solid #2878bd;margin:0 3px 0 8px"></span>реки';
  if (KMP.layers.grid) h += '<span style="display:inline-block;width:18px;border-top:1px solid #65717b;margin:0 3px 0 8px"></span>сетка';
  if (KMP.layers.compass) h += ' ↑ север';
  if (kmFogged()) h += '<span style="display:inline-block;width:10px;height:10px;background:repeating-linear-gradient(135deg,#202630 0 3px,#68717e 3px 5px);margin:0 3px 0 8px"></span>не разведано';
  return h;
}
function kmSetPreset(name){
  KMP.mapStyle = name;
  const common = {water:true,relief:true,cover:true,landforms:true,biome:false,rivers:true,roads:true,realm:false,people:false,settlements:true,labels:true,grid:false,compass:true,game:true};
  KMP.layers = Object.assign({}, common);
  if (name === 'political') KMP.layers.realm = true, KMP.layers.landforms = false;
  if (name === 'people') KMP.layers.people = true;
  if (name === 'master') KMP.layers.realm = true, KMP.layers.people = true, KMP.layers.grid = true;
  if (name === 'player') KMP.layers.people = false, KMP.layers.realm = false;
}
const KMV = {cx: 0, cy: 0, kpp: 1, tiles: {}, raw: {}, wait: {}, drag: null};   // центр вида (км), км на пиксель, куски: ключ → картинка и данные
async function kmLoadWorld(id){
  const r = await api({cmd: 'kmp_worldinfo', id, name: kmpName(), asPlayer: KMP.asPlayer});
  const same = KMP.world && KMP.worldOf === id && KMP.world.wid === r.wid;
  KMP.worldOf = id; KMV.wait = {};
  if (!same){ KMV.tiles = {}; KMV.raw = {}; }
  KMP.world = r.type === 'kmp_worldinfo' ? Object.assign(r.info, {wid: r.wid, fogN: r.fogN}) : null;
  if (KMP.world && !same) kmFit();
}
// ключ куска: мир, уровень, место и туман (у игрока — число операций тумана: после открытия куски перезапрашиваются)
const kmFogged = () => KMP.v && (!KMP.v.master || KMP.asPlayer);
const kmKey = (wid, z, tx, ty) => wid + '|' + z + '|' + tx + '|' + ty + '|' + (kmFogged() ? 'f' + KMP.v.state.fogN : 'm');
async function kmSyncFog(){   // туман сменился — у игрока обновить список поселений
  if (KMP.world && kmFogged() && KMP.world.fogN !== KMP.v.state.fogN){ await kmLoadWorld(KMP.id); kmDraw(); }
}
function kmFit(){ const W = KMP.world; KMV.cx = W.w / 2; KMV.cy = W.h / 2; KMV.kpp = Math.max(W.w, W.h) / 900; }
const kmLevel = () => Math.max(0, Math.min(KMP.world.zMax, Math.floor(Math.log2(Math.max(1e-9, KMV.kpp / KMP.world.cellKm)))));
function kmTileCanvas(t){
  const c = document.createElement('canvas'); c.width = c.height = 256;
  const g = c.getContext('2d'), im = g.createImageData(256, 256);
  for (let k = 0; k < 65536; k++){
    const ter = t.terr.charCodeAt(k) - 48, lv = t.level.charCodeAt(k) - 48;
    const bi = t.biome ? t.biome.charCodeAt(k) - 48 : 4;
    const cover = t.cover ? t.cover.charCodeAt(k) - 48 : (ter === 1 ? 3 : ter === 3 ? 8 : ter === 4 ? 9 : ter === 8 ? 7 : 5);
    let p = KMP.layers.cover ? (KM_COVER_RGB[cover] || KM_RGB[ter]) : KM_RGB[ter];
    if (ter === 7) p = KMP.layers.water ? [66,116,153] : [212,205,179];
    const rc = t.realm ? t.realm.charCodeAt(k) : 48, pc = t.people ? t.people.charCodeAt(k) : 48;
    const blend = (a,b,alpha) => a.map((v,i) => Math.round(v*(1-alpha)+b[i]*alpha));
    if (KMP.layers.biome && ter !== 10 && ter !== 7) p = blend(p, KM_BIOME_RGB[bi] || p, .46);
    if (KMP.layers.realm && ter !== 10 && rc !== 48) {
      const col = [100+(rc*47)%100,85+(rc*31)%100,95+(rc*67)%100];
      if ((k % 11) < 3) p = blend(p,col,.34);
    }
    if (KMP.layers.people && ter !== 10 && pc !== 48) {
      const col = [80+(pc*47)%130,85+(pc*31)%120,105+(pc*67)%120];
      if ((k % 13) < 3) p = blend(p,col,.35);
    }
    if (ter === 10) p = (k % 256 + Math.floor(k / 256)) % 6 < 2 ? [91, 100, 112] : [29, 36, 47];
    else if (KMP.layers.relief && ter !== 7) { const sh = 1 - Math.max(0, lv - 2) * 0.035; p = p.map(v => Math.round(v * sh)); }
    im.data[k * 4] = p[0]; im.data[k * 4 + 1] = p[1]; im.data[k * 4 + 2] = p[2]; im.data[k * 4 + 3] = 255;
  }
  g.putImageData(im, 0, 0);
  if (KMP.layers.landforms) {
    g.fillStyle = '#60462f';
    for (let y = 2; y < 256; y += 6) for (let x = 2; x < 256; x += 6) {
      const k = y * 256 + x, rel = t.relief ? t.relief.charCodeAt(k)-48 : (t.terr.charCodeAt(k)-48 === 3 ? 3 : t.terr.charCodeAt(k)-48 === 2 ? 1 : 0);
      if (rel >= 3) { g.beginPath(); g.moveTo(x,y-2); g.lineTo(x-2,y+2); g.lineTo(x+2,y+2); g.closePath(); g.fill(); }
      else if (rel === 1 || rel === 2) { g.beginPath(); g.arc(x,y,1.2,0,Math.PI*2); g.fill(); }
      if (KMP.layers.cover && t.cover && t.cover.charCodeAt(k)-48 === 3) { g.fillStyle='#234e35'; g.beginPath(); g.moveTo(x,y-2); g.lineTo(x-2,y+2); g.lineTo(x+2,y+2); g.closePath(); g.fill(); g.fillStyle='#60462f'; }
    }
  }
  const strokes = (bit, color, outline, step=1) => { for (let y=0;y<256;y+=step) for(let x=0;x<256;x+=step){ const k=y*256+x, f=t.flags.charCodeAt(k)-48; if(f&bit){ g.fillStyle=outline; g.fillRect(x,y,step,step); g.fillStyle=color; g.fillRect(x,y,Math.max(1,step*.52),Math.max(1,step*.52)); } } };
  if(KMP.layers.roads) strokes(1,'#cf392e','#f1dfbd');
  if(KMP.layers.rivers) strokes(2,'#2878bd','#d4e4e8');
  return c;
}
function kmGetTile(wid, z, tx, ty, redraw){
  const key = kmKey(wid, z, tx, ty);
  if (KMV.tiles[key]) return KMV.tiles[key];
  if (!KMV.wait[key]){
    KMV.wait[key] = 1;
    api({cmd: 'kmp_tile', id: KMP.id, name: kmpName(), wid, z, tx, ty, asPlayer: KMP.asPlayer}).then(r => { if (r.type === 'kmp_tile'){ KMV.tiles[key] = kmTileCanvas(r.tile); KMV.raw[key] = r.tile; redraw(); } });
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
      if (!im && z < W.zMax){ const pz = z + 1, ps = span * 2, ptx = Math.floor(tx / 2), pty = Math.floor(ty / 2), pim = KMV.tiles[kmKey(W.wid, pz, ptx, pty)];   // пока грузится — крупнее
        if (pim){ g.drawImage(pim, (tx % 2) * 128, (ty % 2) * 128, 128, 128, px, py, sz, sz); continue; } }
      if (im) g.drawImage(im, px, py, sz + 0.5, sz + 0.5);
    }
  // поселения: крупные — всегда, мелкие — когда близко
  g.textBaseline = 'middle';
  const occupied = [];
  if (KMP.layers.settlements) for (const t of W.towns){
    const [px, py] = kmToPx(cv, t[1], t[2]); if (px < -50 || py < -20 || px > cv.width + 50 || py > cv.height + 20) continue;
    const big = t[3] >= 20000, mid = t[3] >= 5000;
    if (!big && !(mid && KMV.kpp < 2) && KMV.kpp > 0.5) continue;
    if (KMV.kpp > 2 && !big) continue;
    const box = [px+3,py-13,px+Math.min(130,t[0].length*7+8),py+2];
    if (occupied.some(a => box[0]<a[2] && box[2]>a[0] && box[1]<a[3] && box[3]>a[1])) continue;
    occupied.push(box);
    g.fillStyle = big ? '#b22222' : '#222'; g.beginPath(); g.arc(px, py, big ? 4 : 3, 0, 7); g.fill();
    g.font = big ? 'bold 12px sans-serif' : '11px sans-serif'; g.strokeStyle = 'rgba(255,255,255,.7)'; g.lineWidth = 3; g.strokeText(t[0], px + 6, py - 7); g.fillStyle = '#111'; g.fillText(t[0], px + 6, py - 7);
  }
  g.font = 'italic 12px serif'; g.fillStyle = 'rgba(30,20,10,.85)';
  if (KMP.layers.labels) for (const l of W.labels || []){
    const kind = l[3] || 'place';
    if (kind === 'people' && !KMP.layers.people) continue;
    if (kind === 'realm' && !KMP.layers.realm) continue;
    const [px, py] = kmToPx(cv, l[1], l[2]); if (px > -200 && py > -20 && px < cv.width + 50 && py < cv.height + 20){ g.strokeStyle = 'rgba(255,255,255,.75)'; g.lineWidth = 3; g.strokeText(l[0], px, py); g.fillText(l[0], px, py); }
  }
  const line = (pts, col, dash) => { if (pts.length < 2) return; g.strokeStyle = col; g.lineWidth = 3; g.setLineDash(dash || []); g.beginPath();
    pts.forEach((p, i) => { const [x, y] = kmToPx(cv, p[0], p[1]); i ? g.lineTo(x, y) : g.moveTo(x, y); }); g.stroke(); g.setLineDash([]); };
  if (KMP.layers.game) {
    const from = s.pos ? [s.pos] : [];
    line(from.concat(s.route), '#ffcc00');
    if (KMP.preview && KMP.preview.path) line(from.concat(KMP.preview.path), '#ffffff', [6, 5]);
    else line(from.concat(KMP.draft), 'rgba(255,255,255,.6)', [2, 6]);
    for (const p of KMP.draft){ const [x, y] = kmToPx(cv, p[0], p[1]); g.fillStyle = '#fff'; g.strokeStyle = '#000'; g.lineWidth = 1; g.fillRect(x - 4, y - 4, 8, 8); g.strokeRect(x - 4, y - 4, 8, 8); }
    if (s.route.length){ const e = s.route[s.route.length - 1], [x, y] = kmToPx(cv, e[0], e[1]); g.fillStyle = '#ffcc00'; g.beginPath(); g.arc(x, y, 5, 0, 7); g.fill(); }
  }
  if (KMV.stroke){ g.fillStyle = KMP.tool === 'fogHide' ? 'rgba(0,0,0,.45)' : KMP.tool === 'fogOpen' ? 'rgba(255,255,255,.35)' : 'rgba(255,220,0,.35)';
    for (const p of KMV.stroke){ const [x, y] = kmToPx(cv, p[0], p[1]); g.beginPath(); g.arc(x, y, Math.max(2, KMP.brushR / KMV.kpp), 0, 7); g.fill(); } }
  if (KMP.line && KMP.line.length) line(KMP.line, KMP.tool === 'river' ? '#2a6fb0' : '#6e543c', [4, 3]);
  if (KMP.layers.game && s.pos){ const [x, y] = kmToPx(cv, s.pos[0], s.pos[1]); g.fillStyle = '#e8112d'; g.strokeStyle = '#fff'; g.lineWidth = 2; g.beginPath(); g.arc(x, y, 7, 0, 7); g.fill(); g.stroke(); }
  if (KMP.layers.grid) {
    const step = kmNiceKm(KMV.kpp * 80), left = Math.floor(x0 / step) * step, top = Math.floor(y0 / step) * step;
    g.save(); g.strokeStyle = 'rgba(65,78,88,.25)'; g.lineWidth = 1; g.beginPath();
    for(let x=left;x<=x1;x+=step){ const px=kmToPx(cv,x,0)[0]; g.moveTo(px,0); g.lineTo(px,cv.height); }
    for(let y=top;y<=y1;y+=step){ const py=kmToPx(cv,0,y)[1]; g.moveTo(0,py); g.lineTo(cv.width,py); }
    g.stroke(); g.restore();
  }
  if (KMP.layers.compass) {
    g.save(); g.translate(28,28); g.strokeStyle='rgba(20,28,36,.8)'; g.fillStyle='rgba(255,255,255,.9)'; g.lineWidth=1.5;
    g.beginPath(); g.moveTo(0,-13); g.lineTo(-5,2); g.lineTo(0,-1); g.lineTo(5,2); g.closePath(); g.fill(); g.stroke();
    g.font='bold 11px sans-serif'; g.textAlign='center'; g.fillText('С',0,-17); g.restore();
  }
  // масштаб: линейка и размер клетки уровня
  const km = kmNiceKm(KMV.kpp * 110), px = km / KMV.kpp, ckTxt = ck < 1 ? Math.round(ck * 1000) + ' м' : ck.toFixed(ck < 10 ? 1 : 0) + ' км';
  $('kmScale').innerHTML = `<span style="display:inline-block;width:${Math.round(px)}px;border-bottom:3px solid #fff;margin-right:6px;vertical-align:middle"></span>${km} км · клетка ${ckTxt} · мир ${W.w}×${W.h} км`;
}
async function kmOpenCity(town){
  const r=await api({cmd:'kmp_city',id:KMP.id,name:kmpName(),asPlayer:KMP.asPlayer,town:town[0],x:town[1],y:town[2]});
  if(r.type!=='city'||!r.plan){toast(r.err||'Не удалось построить план поселения');return;}
  const p=r.plan, old=document.getElementById('kmCityDialog'); if(old)old.remove();
  const dlg=document.createElement('div');dlg.id='kmCityDialog';dlg.style.cssText='position:fixed;inset:0;z-index:10000;background:rgba(8,12,20,.82);display:grid;place-items:center;padding:16px';
  dlg.innerHTML=`<section class="card" style="width:min(1100px,96vw);max-height:94vh;overflow:auto;background:var(--panel,#171b25);padding:16px">
    <div class="row" style="justify-content:space-between"><div><h2 style="margin:0">${esc(town[0])}</h2><span class="small">${Number(town[3]||0).toLocaleString('ru-RU')} жителей · зерно ${p.seed} · 1 клетка = ${p.cellFt} футов</span></div>
    <div class="row"><button id="kmCityPng">Скачать PNG</button><button id="kmCityJson">Скачать JSON</button><button id="kmCityClose">Закрыть</button></div></div>
    <canvas id="kmCityCanvas" style="display:block;width:100%;height:min(67vh,650px);margin-top:10px;background:#e8dfc0;border:1px solid #75694f"></canvas><div id="kmCityKey" class="small" style="display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:4px;margin-top:8px"></div></section>`;
  document.body.appendChild(dlg);
  const cv=dlg.querySelector('#kmCityCanvas'),dpr=window.devicePixelRatio||1,rect=cv.getBoundingClientRect();cv.width=Math.max(1,Math.floor(rect.width*dpr));cv.height=Math.max(1,Math.floor(rect.height*dpr));
  const g=cv.getContext('2d');g.scale(dpr,dpr);const w=rect.width,h=rect.height,m=28,scale=Math.min((w-2*m)/p.widthFt,(h-2*m)/(p.heightFt+270)),ox=(w-p.widthFt*scale)/2,oy=(h-(p.heightFt+270)*scale)/2,X=x=>ox+x*scale,Y=y=>oy+y*scale;
  g.fillStyle='#e8dfc0';g.fillRect(0,0,w,h);const key=[];
  for(const f of p.fields){g.fillStyle=['#d7c58a','#cbb578','#e1d094'][f.key%3];g.fillRect(X(f.x),Y(f.y),f.w*scale,f.h*scale);g.strokeStyle='#a28e5e';g.strokeRect(X(f.x),Y(f.y),f.w*scale,f.h*scale);}
  for(const a of p.roads){g.fillStyle=a.kind==='avenue'?'#c3a36d':'#d4c39a';g.fillRect(X(a.x),Y(a.y),a.w*scale,a.h*scale);}
  for(const a of p.walls){g.fillStyle=p.wall===2?'#625b52':'#735d3e';g.fillRect(X(a.x),Y(a.y),Math.max(2,a.w*scale),Math.max(2,a.h*scale));}
  for(const b of p.buildings){if(b.kind==='gate'){g.fillStyle='#bd8d44';g.fillRect(X(b.x),Y(b.y),b.w*scale,b.h*scale);key.push([b.key,b.name]);continue;}if(b.kind==='wall')continue;
    g.fillStyle=b.kind==='citadel'?'#796248':b.kind==='temple'?'#d5d1b8':b.kind==='market'?'#d29d4f':b.kind==='warehouse'?'#987a50':b.kind==='bridge'?'#856141':b.kind==='hall'?'#9a744c':b.district==='noble'?'#879ab3':b.district==='craft'?'#aa6551':b.district==='trade'?'#c49555':'#a9664b';
    g.strokeStyle='rgba(48,38,28,.65)';g.lineWidth=Math.max(.7,scale*2);g.fillRect(X(b.x),Y(b.y),Math.max(1,b.w*scale),Math.max(1,b.h*scale));g.strokeRect(X(b.x),Y(b.y),Math.max(1,b.w*scale),Math.max(1,b.h*scale));if(b.key<20)key.push([b.key,b.name]);}
  g.fillStyle='#2b2119';g.font='12px sans-serif';g.fillText('СЕВЕР ↑',ox+8,oy-8);g.fillText('Масштаб: 100 футов',ox+8,Math.min(h-8,Y(p.heightFt+250)));
  dlg.querySelector('#kmCityKey').innerHTML=key.map(k=>`<span><b>${k[0]}.</b> ${esc(k[1])}</span>`).join('')+`<span>Дома: ${p.buildings.filter(b=>b.kind==='house').length}</span><span>Ворота: ${p.buildings.filter(b=>b.kind==='gate').length}</span>`;
  dlg.querySelector('#kmCityClose').onclick=()=>dlg.remove();dlg.querySelector('#kmCityPng').onclick=()=>{const a=document.createElement('a');a.download=`${town[0]}_plan.png`;a.href=cv.toDataURL('image/png');a.click();};
  dlg.querySelector('#kmCityJson').onclick=()=>{const a=document.createElement('a'),u=URL.createObjectURL(new Blob([JSON.stringify({town:town[0],plan:p},null,2)],{type:'application/json'}));a.href=u;a.download=`${town[0]}_plan.json`;a.click();URL.revokeObjectURL(u);};
  dlg.addEventListener('click',e=>{if(e.target===dlg)dlg.remove();});
}
function kmHoverInfo(e){
  const cv = $('kMap'), b = cv.getBoundingClientRect(), [x, y] = kmToKm(cv, (e.clientX - b.left) * cv.width / b.width, (e.clientY - b.top) * cv.height / b.height);
  const W = KMP.world, s = KMP.v.state;
  if (x < 0 || y < 0 || x > W.w || y > W.h){ $('kmHover').textContent = 'за краем мира'; return; }
  const z = kmLevel(), ck = W.cellKm * (1 << z), span = ck * 256, tx = Math.floor(x / span), ty = Math.floor(y / span), im = KMV.tiles[kmKey(W.wid, z, tx, ty)];
  let txt = '';
  const raw = KMV.raw && KMV.raw[kmKey(W.wid, z, tx, ty)];
  if (raw){ const k = Math.floor((y - ty * span) / ck) * 256 + Math.floor((x - tx * span) / ck), ter = raw.terr.charCodeAt(k) - 48, f = raw.flags.charCodeAt(k) - 48;
    const cov = raw.cover ? raw.cover.charCodeAt(k)-48 : -1, bi = raw.biome ? raw.biome.charCodeAt(k)-48 : -1, rel = raw.relief ? raw.relief.charCodeAt(k)-48 : -1;
    txt = ter === 10 ? 'не разведано' : `${W.terrains[ter]}${cov >= 0 ? ` · ${KM_COVER_NAME[cov] || 'покров'}` : ''}${bi >= 0 ? ` · биом: ${KM_BIOME_NAME[bi]}` : ''}${rel > 0 ? ` · ${KM_RELIEF_NAME[rel] || 'рельеф'}` : ''}, высота ${raw.level[k]}${f & 1 ? ', дорога' : ''}${f & 2 ? ', река' : ''}`; }
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
  const redrawLayers = () => { for (const [key, raw] of Object.entries(KMV.raw)) KMV.tiles[key] = kmTileCanvas(raw); $('kmLegend').innerHTML = kmLegendHtml(KMP.mapStyle, W); kmDraw(); };
  $('kmpBox').querySelectorAll('[data-kmpreset]').forEach(b => b.onclick = async () => {
    const oldFog = KMP.asPlayer;
    kmSetPreset(b.dataset.kmpreset);
    KMP.asPlayer = KMP.v.master && b.dataset.kmpreset === 'player';
    if (!W.realmLayer) KMP.layers.realm = false;
    if (!W.peopleLayer) KMP.layers.people = false;
    $('kmpBox').querySelectorAll('[data-kmlayer]').forEach(i => { i.checked = !!KMP.layers[i.dataset.kmlayer]; });
    $('kmpBox').querySelectorAll('[data-kmpreset]').forEach(i => i.classList.toggle('primary', i.dataset.kmpreset === KMP.mapStyle));
    if (oldFog !== KMP.asPlayer) await kmLoadWorld(KMP.id);
    redrawLayers();
  });
  $('kmpBox').querySelectorAll('[data-kmlayer]').forEach(i => i.onchange = () => { KMP.layers[i.dataset.kmlayer] = i.checked; redrawLayers(); });
  const zoom = (f, px, py) => { const [x, y] = px == null ? [KMV.cx, KMV.cy] : kmToKm(cv, px, py);
    KMV.kpp = Math.min(Math.max(W.w, W.h) / 200, Math.max(W.cellKm / 6, KMV.kpp * f)); const [nx, ny] = px == null ? [KMV.cx, KMV.cy] : kmToKm(cv, px, py);
    KMV.cx += x - nx; KMV.cy += y - ny; kmDraw(); };
  const pxOf = e => { const b = cv.getBoundingClientRect(); return [(e.clientX - b.left) * cv.width / b.width, (e.clientY - b.top) * cv.height / b.height]; };
  cv.onwheel = e => { e.preventDefault(); const [px, py] = pxOf(e); zoom(e.deltaY > 0 ? 1.25 : 0.8, px, py); };
  cv.onpointerdown = e => { cv.setPointerCapture(e.pointerId);
    if (KM_PAINT.includes(KMP.tool)){ KMV.stroke = [kmToKm(cv, ...pxOf(e))]; kmDraw(); return; }   // мазок кисти
    KMV.drag = {x: e.clientX, y: e.clientY, cx: KMV.cx, cy: KMV.cy, moved: false}; cv.style.cursor = 'grabbing'; };
  cv.onpointermove = e => { kmHoverInfo(e);
    if (KMV.stroke){ const p = kmToKm(cv, ...pxOf(e)), l = KMV.stroke[KMV.stroke.length - 1];
      if (Math.hypot(p[0] - l[0], p[1] - l[1]) >= KMP.brushR * 0.5) { KMV.stroke.push(p); kmDraw(); } return; }
    const d = KMV.drag; if (!d) return; const b = cv.getBoundingClientRect(), k = cv.width / b.width;
    if (Math.abs(e.clientX - d.x) + Math.abs(e.clientY - d.y) > 4) d.moved = true;
    KMV.cx = d.cx - (e.clientX - d.x) * k * KMV.kpp; KMV.cy = d.cy - (e.clientY - d.y) * k * KMV.kpp; kmDraw(); };
  cv.onpointerup = e => { if (KMV.stroke){ const st = KMV.stroke; KMV.stroke = null; kmStrokeDone(st); return; }
    const d = KMV.drag; KMV.drag = null; cv.style.cursor = 'grab'; if (!d || d.moved) return;
    const [x, y] = kmToKm(cv, ...pxOf(e)); const p = [+x.toFixed(3), +y.toFixed(3)];
    if (KMP.tool === 'road' || KMP.tool === 'river'){ (KMP.line = KMP.line || []).push(p); kmDraw(); return; }
    if (KMP.tool === 'town'){ const n = ($('kbName') || {}).value; if (!n){ toast('Впишите имя поселения'); return; }
      kmEdit([{op: 'town', name: n, kind: $('kbKind').value, x: p[0], y: p[1]}]); return; }
    if (KMP.tool === 'label'){ const t = ($('kbText') || {}).value; if (!t){ toast('Впишите надпись'); return; } kmEdit([{op: 'label', text: t, x: p[0], y: p[1]}]); return; }
    if (KMP.mode === 'pos'){ KMP.mode = ''; kmpDirect({type: 'gm', pos: p, text: 'отряд перенесён'}); return; }
    if (KMP.mode === '' && KMP.layers.settlements){
      const q=pxOf(e), hit=W.towns.find(t=>{const z=kmToPx(cv,t[1],t[2]);return Math.hypot(z[0]-q[0],z[1]-q[1])<12;});
      if(hit){kmOpenCity(hit);return;}
    }
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
  kmBindBrush();
  kmSyncFog();
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

// ---- кисть и туман (ТЗ v2 §3.1, §5): правки мира — у мира (видны всем его кампаниям), туман — в журнале кампании ----
const KM_PAINT = ['terr', 'erase', 'fogOpen', 'fogHide'];
const KM_TOOLS = [['', '✋ Рука (двигать карту)'], ['terr', '🖌 Местность'], ['road', 'Дорога'], ['river', 'Река'], ['town', 'Поселение'], ['label', 'Надпись'],
  ['erase', '🧽 Ластик'], ['fogOpen', '☀ Открыть игрокам'], ['fogHide', '🌫 Скрыть от игроков']];
const KM_TOWN_KINDS = ['хутор', 'село', 'городок', 'город', 'крупный город', 'великий город'];
KMP.tool = ''; KMP.brushR = 5; KMP.brushT = 1; KMP.asPlayer = false;
function kmBrushPanel(s){
  const T = KMP.tool;
  return `<details class="card" style="margin:6px 0"${T || KMP.asPlayer ? ' open' : ''}><summary><b>Кисть и туман</b> <span class="small">(рисовать на мире: местность, дороги, реки, поселения; что видят игроки)</span></summary>
    <div class="row">${KM_TOOLS.map(([k, n]) => `<button data-tool="${k}"${T === k ? ' class="primary"' : ''}>${n}</button>`).join('')}</div>
    <div class="row">${KM_PAINT.includes(T) ? `<label>радиус, км <input id="kbR" type="number" min="0.2" max="200" step="0.5" value="${KMP.brushR}" style="width:70px"></label>` : ''}
      ${T === 'terr' ? `<label>местность <select id="kbT">${[0, 1, 2, 3, 4, 8, 9, 7].map(i => `<option value="${i}"${+KMP.brushT === i ? ' selected' : ''}>${esc(KMP.world.terrains[i])}</option>`).join('')}</select></label>` : ''}
      ${T === 'road' || T === 'river' ? `<span class="small">щёлкайте точки линии (${(KMP.line || []).length})</span><button id="kbLineOk" class="primary">Закончить линию</button><button id="kbLineNo">Сбросить</button>` : ''}
      ${T === 'town' ? `<label>имя <input id="kbName" style="width:140px"></label><label>вид <select id="kbKind">${kmpOpt(KM_TOWN_KINDS, 'село')}</select></label><span class="small">затем щелчок по карте</span>` : ''}
      ${T === 'label' ? `<label>надпись <input id="kbText" style="width:200px" placeholder="Солонь: булливаги"></label><span class="small">затем щелчок по карте</span>` : ''}
      ${KM_PAINT.includes(T) ? '<span class="small">ведите по карте с зажатой кнопкой</span>' : ''}</div>
    <div class="row"><button id="kbUndo">↶ Отменить последнюю правку мира</button><button id="kbFogAll">Открыть игрокам всю карту</button><button id="kbFogNone">Скрыть всё от игроков</button>
      <label><input type="checkbox" id="kbAsPlayer"${KMP.asPlayer ? ' checked' : ''}> как видят игроки</label></div>
    <div class="hint">Правки мира видят все кампании на этом мире. Отряд в пути сам открывает полосу ~8 км вокруг пройденного.</div></details>`;
}
async function kmEdit(ops){
  const v = await kmpCall({cmd: 'kmp_edit', ops});
  if (v.type === 'kmp'){ KMV.tiles = {}; KMV.raw = {}; await kmLoadWorld(KMP.id); kmDraw(); }
}
function kmStrokeDone(st){
  const r = KMP.brushR, pts = st.map(p => [+p[0].toFixed(3), +p[1].toFixed(3)]);
  if (KMP.tool === 'terr') kmEdit(pts.map(p => ({op: 'terr', t: +KMP.brushT, x: p[0], y: p[1], r})));
  else if (KMP.tool === 'erase') kmEdit(pts.map(p => ({op: 'erase', x: p[0], y: p[1], r})));
  else kmpDirect({type: 'fog', op: KMP.tool === 'fogHide' ? 'скрыть' : 'открыть', r, pts});
}
function kmBindBrush(){
  const click = (id, f) => { if ($(id)) $(id).onclick = f; };
  document.querySelectorAll('[data-tool]').forEach(b => b.onclick = () => { KMP.tool = b.dataset.tool; KMP.line = []; KMP.mode = ''; kmpRender(); });
  if ($('kbR')) $('kbR').oninput = () => { KMP.brushR = Math.max(0.2, +$('kbR').value || 5); };
  if ($('kbT')) $('kbT').onchange = () => { KMP.brushT = +$('kbT').value; };
  click('kbLineOk', () => { if ((KMP.line || []).length < 2){ toast('Нужно хотя бы две точки'); return; } kmEdit([{op: KMP.tool, pts: KMP.line}]); KMP.line = []; });
  click('kbLineNo', () => { KMP.line = []; kmDraw(); });
  click('kbUndo', () => kmpCall({cmd: 'kmp_edit_undo'}).then(async v => { if (v.type === 'kmp'){ KMV.tiles = {}; KMV.raw = {}; await kmLoadWorld(KMP.id); kmDraw(); } }));
  click('kbFogAll', () => kmpDirect({type: 'fog', op: 'всё'}));
  click('kbFogNone', () => confirm('Скрыть от игроков всю карту? Откроется снова, где отряд пройдёт.') && kmpDirect({type: 'fog', op: 'ничего'}));
  if ($('kbAsPlayer')) $('kbAsPlayer').onchange = async () => { KMP.asPlayer = $('kbAsPlayer').checked; await kmLoadWorld(KMP.id); kmDraw(); };
  if ($('kMap')) $('kMap').style.cursor = KM_PAINT.includes(KMP.tool) ? 'crosshair' : KMP.tool ? 'copy' : 'grab';
}

// ---- мир кампании: мастер выбирает — генератор по параметрам (с предпросмотром) или Акт I ----
const KM_GEN = [['size', 'Размер мира, км', 50, 5000, 50, 800], ['sea', 'Доля моря', 0, 0.9, 0.05, 0.3], ['mount', 'Гористость', 0, 1, 0.05, 0.5],
  ['rough', 'Изрезанность (холмы, берег)', 0, 1, 0.05, 0.5], ['climate', 'Климат: холодно ← → жарко', -1, 1, 0.1, 0], ['humid', 'Влажность: сухо ← → сыро', -1, 1, 0.1, 0],
  ['towns', 'Поселений на 100 × 100 км', 0.1, 6, 0.1, 1], ['rivers', 'Густота рек', 0.1, 3, 0.1, 1]];
function kmWorldPanel(){
  const W = KMP.world, G = KMP.gen;
  let h = `<details class="card" style="margin:6px 0"${G ? ' open' : ''}><summary><b>Мир:</b> ${W ? esc(W.name) + ` — ${W.w}×${W.h} км, поселений ${W.towns.length}` : 'нет'} <span class="small">(сменить мир, создать новый)</span></summary>`;
  if (!G) return h + `<div class="row"><button id="kmGenOpen" class="primary">Новый мир…</button><button id="kmAct1">Европа и Средиземноморье 1208</button>
    <span class="hint">Смена мира не двигает отряд: поставьте его заново.</span></div></details>`;
  h += `<div class="hint">Подберите параметры и нажмите «Показать» — мир кампании не изменится, пока вы не нажмёте «Взять этот мир».</div>
    <div class="row"><label>Название <input id="kgName" value="${esc(G.name)}" style="width:180px"></label><label>Зерно ${kq('зерно')} <input id="kgSeed" type="number" value="${G.params.seed}" style="width:110px"></label><button id="kgDice" title="Другое зерно">🎲</button></div>
    <div class="card" style="margin:6px 0"><b>Карта высот</b> <span class="small">(необязательно): чёрно-белый PNG, светлее — выше. Страница уменьшит её до ${KM_HMAX} точек по большей стороне.</span>
      <div class="row"><input type="file" id="kgFile" accept="image/png,image/jpeg,image/webp">${G.hm ? `<button id="kgHmOff">Без карты высот</button>` : ''}</div>
      ${G.hm ? `<div class="row"><label>км на точку <input id="kgKmpp" type="number" min="0.05" step="0.05" value="${G.hm.kmpp}" style="width:80px"></label>
        <span class="small" id="kgHmSize">${kmHmSize()}</span>
        <label>уровень моря <input id="kgSea" type="range" min="0" max="1" step="0.01" value="${G.hm.sea}"> <span class="tnum" id="kgSeaV">${G.hm.sea}</span></label></div>
        <canvas id="kgHmCv" style="max-width:100%;max-height:300px;image-rendering:pixelated;border-radius:6px"></canvas>
        <div class="small">Синее — будет морем. Климат, биомы, реки, поселения и дороги генератор достроит сам; размер мира и гористость берутся из картинки.</div>` : ''}</div>
    <div class="grid2">${KM_GEN.filter(x => !G.hm || !['size', 'sea', 'mount'].includes(x[0])).map(([k, n, lo, hi, st]) => `<label>${n} <input type="range" data-kg="${k}" min="${lo}" max="${hi}" step="${st}" value="${G.params[k]}"> <span class="tnum" data-kgv="${k}">${G.params[k]}</span></label>`).join('')}</div>
    <div class="row"><button id="kgShow" class="primary">Показать</button><button id="kgUse"${G.info ? '' : ' disabled'}>Взять этот мир</button><button id="kgCancel">Отмена</button>
    <span class="small" id="kgMsg">${G.info ? `${G.info.w}×${G.info.h} км, поселений ${G.info.towns.length}, клеток ${Math.ceil(G.info.w / G.info.cellKm)}² по 305 м` : ''}</span></div>
    <canvas id="kgCv" width="512" height="512" style="width:100%;max-width:512px;image-rendering:pixelated;border-radius:8px;display:${G.info ? 'block' : 'none'}"></canvas></details>`;
  return h;
}
function kmBindWorld(){
  const click = (id, f) => { if ($(id)) $(id).onclick = f; };
  click('kmGenOpen', () => { KMP.gen = {name: 'Новый мир', params: Object.fromEntries(KM_GEN.map(x => [x[0], x[5]]).concat([['seed', 1 + Math.floor(Math.random() * 99999)]])), info: null}; kmpRender(); });
  click('kmAct1', async () => {
    const b = $('kmAct1'), label = b.textContent;
    b.disabled = true; b.textContent = 'Загружаю Европу…';
    try {
      const v = await kmpCall({cmd: 'kmp_world_use', act1: true});
      if (v.type !== 'kmp') { b.disabled = false; b.textContent = label; toast(v.err || 'Не удалось загрузить карту Европы'); }
    } catch (e) {
      b.disabled = false; b.textContent = label;
      toast('Не удалось загрузить карту Европы: ' + (e && e.message || e));
    }
  });
  if (!KMP.gen) return;
  const G = KMP.gen;
  document.querySelectorAll('[data-kg]').forEach(r => r.oninput = () => { G.params[r.dataset.kg] = +r.value; document.querySelector(`[data-kgv="${r.dataset.kg}"]`).textContent = r.value; });
  $('kgName').oninput = () => { G.name = $('kgName').value; };
  $('kgSeed').oninput = () => { G.params.seed = +$('kgSeed').value || 1; };
  click('kgDice', () => { G.params.seed = 1 + Math.floor(Math.random() * 999999); $('kgSeed').value = G.params.seed; });
  click('kgCancel', () => { KMP.gen = null; kmpRender(); });
  if ($('kgFile')) $('kgFile').onchange = e => { const f = e.target.files[0]; if (f) kmHmLoad(f); };
  click('kgHmOff', () => { G.hm = null; G.info = null; kmpRender(); });
  if (G.hm){
    $('kgKmpp').oninput = () => { G.hm.kmpp = Math.max(0.05, +$('kgKmpp').value || 1); $('kgHmSize').textContent = kmHmSize(); };
    $('kgSea').oninput = () => { G.hm.sea = +$('kgSea').value; $('kgSeaV').textContent = G.hm.sea; kmHmDraw(); };
    kmHmDraw();
  }
  click('kgShow', async () => { $('kgMsg').textContent = G.hm && !G.hm.token ? 'загружаю карту высот…' : 'строю мир…';
    if (G.hm && !G.hm.token){   // карта высот — на сервер один раз (серое, base64)
      let bin = ''; for (let i = 0; i < G.hm.px.length; i += 0x8000) bin += String.fromCharCode.apply(null, G.hm.px.subarray(i, i + 0x8000));
      const u = await api({cmd: 'kmp_hmap', id: KMP.id, name: kmpName(), data: btoa(bin), w: G.hm.w, h: G.hm.h});
      if (u.type !== 'kmp_hmap'){ $('kgMsg').textContent = u.err || 'не загрузилось'; return; }
      G.hm.token = u.token; $('kgMsg').textContent = 'строю мир…';
    }
    if (G.hm) Object.assign(G.params, {hmap: G.hm.token, hw: G.hm.w, hh: G.hm.h, kmpp: G.hm.kmpp, hmSea: G.hm.sea}); else ['hmap', 'hw', 'hh', 'kmpp', 'hmSea'].forEach(k => delete G.params[k]);
    const r = await api({cmd: 'kmp_world_preview', id: KMP.id, name: kmpName(), params: G.params});
    if (r.type !== 'kmp_preview'){ $('kgMsg').textContent = r.err || 'ошибка'; return; }
    G.info = r.info; G.tiles = []; kmpRender(); kmPreviewDraw(); });
  click('kgUse', () => (!G.hm || G.hm.token || (toast('Сначала «Показать»: карта высот ещё не загружена'), false)) && confirm(`Сделать «${G.name}» миром кампании? Отряд останется на тех же числах км — поставьте его заново.`) &&
    kmpCall({cmd: 'kmp_world_use', title: G.name, params: G.hm ? Object.assign(G.params, {hmap: G.hm.token, hw: G.hm.w, hh: G.hm.h, kmpp: G.hm.kmpp, hmSea: G.hm.sea}) : G.params}).then(v => { if (v.type === 'kmp'){ KMP.gen = null; kmFit(); kmpRender(); } }));
  if (G.info) kmPreviewDraw();
}
// ---- карта высот: картинка → серое ≤ KM_HMAX по большей стороне (усреднением) → предпросмотр с морем ----
const KM_HMAX = 1024;
function kmHmSize(){ const H = KMP.gen.hm; const w = H.w * H.kmpp, h = H.h * H.kmpp, big = Math.max(w, h);
  return `мир ${Math.round(w)} × ${Math.round(h)} км${big > 5000 ? ' — больше 5000 км: уменьшите км на точку' : ''}`; }
function kmHmLoad(file){
  const img = new Image(), url = URL.createObjectURL(file);
  img.onload = () => {
    const k = Math.min(1, KM_HMAX / Math.max(img.width, img.height)), w = Math.max(2, Math.round(img.width * k)), h = Math.max(2, Math.round(img.height * k));
    const c = document.createElement('canvas'); c.width = w; c.height = h;
    const g = c.getContext('2d'); g.imageSmoothingEnabled = true; g.imageSmoothingQuality = 'high'; g.drawImage(img, 0, 0, w, h);   // уменьшение с усреднением
    const d = g.getImageData(0, 0, w, h).data, px = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) px[i] = Math.round(0.299 * d[i * 4] + 0.587 * d[i * 4 + 1] + 0.114 * d[i * 4 + 2]);
    URL.revokeObjectURL(url);
    KMP.gen.hm = {w, h, px, kmpp: +(Math.min(1000 / Math.max(w, h), 5000 / Math.max(w, h))).toFixed(2), sea: 0.3, token: null, src: file.name};
    KMP.gen.info = null; kmpRender();
  };
  img.onerror = () => toast('Не удалось прочитать картинку');
  img.src = url;
}
function kmHmDraw(){
  const H = KMP.gen && KMP.gen.hm, cv = $('kgHmCv'); if (!H || !cv) return;
  cv.width = H.w; cv.height = H.h;
  const g = cv.getContext('2d'), im = g.createImageData(H.w, H.h), sea = H.sea * 255;
  for (let i = 0; i < H.px.length; i++){ const v = H.px[i], wet = v < sea;
    im.data[i * 4] = wet ? 60 : v; im.data[i * 4 + 1] = wet ? 110 : v; im.data[i * 4 + 2] = wet ? 190 : v; im.data[i * 4 + 3] = 255; }
  g.putImageData(im, 0, 0);
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
