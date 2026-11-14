// ====================================================================================================================
// Оболочка «Горнило Миров»: персонажи игрока, быстрый бой, экран персонажа, настройки, миры, жалобы, фон меню.
// Экраны лежат в play.html (разделы s0–s10), переключает их go() из 00_common.js; бой, комната, карты и кампания —
// в своих модулях. Здесь — только то, что связывает их в одну игру.
// ====================================================================================================================
const LS = {
  get(k, d = ''){ try { const v = localStorage.getItem(k); return v == null ? d : v; } catch (e) { return d; } },
  set(k, v){ try { localStorage.setItem(k, v); return true; } catch (e) { return false; } },
  del(k){ try { localStorage.removeItem(k); } catch (e) {} },
};
const clone = o => JSON.parse(JSON.stringify(o));
const pkeyJs = n => String(n || '').trim().toLowerCase();
const BUDGETS = [200, 800, 1600, 3200, 6400];

// ---- персонажи игрока: список в браузере + копия на сервере под именем игрока ----
function readProfiles(){ try { const v = JSON.parse(LS.get('dnd_chars', '[]')); return Array.isArray(v) ? v.filter(p => p && p.id && p.character) : []; } catch (e) { return []; } }
const activeId = () => LS.get('dnd_active_char', '');
const activeProfile = () => { const l = readProfiles(); return l.find(p => p.id === activeId()) || null; };
const newId = () => 'char-' + Date.now().toString(36) + Math.random().toString(36).slice(2, 6);
const fullChar = c => Object.assign(clone(DEFAULT_CHAR), migrateChar(clone(c || {})));
let profSeq = 0, profTimer = 0;
function writeProfiles(list){
  if (!LS.set('dnd_chars', JSON.stringify(list))){ toast('Не удалось сохранить: в браузере закончилось место', 6000, true); return; }
  LS.set('dnd_chars_dirty', '1'); profSeq++;
  clearTimeout(profTimer); profTimer = setTimeout(pushProfiles, 1200);   // на сервер — не чаще раза в 1,2 с, а не на каждое нажатие
}
async function pushProfiles(){
  const n = playerName(); if (!n) return;
  const seq = profSeq, r = await api({cmd:'player_chars_save', name: n, chars: readProfiles(), active: activeId()});
  if (r.type === 'ok'){ if (seq === profSeq) LS.set('dnd_chars_dirty', '0'); }
  else if (!/нет связи/.test(r.err || '')) toast(r.err || 'Не удалось сохранить персонажей на сервере', 5000, true);
}
addEventListener('pagehide', () => { if (LS.get('dnd_chars_dirty') === '1' && playerName() && navigator.sendBeacon)   // ушли со страницы до отправки
  navigator.sendBeacon('/api', JSON.stringify({sid: SID, cmd:'player_chars_save', name: playerName(), chars: readProfiles(), active: activeId()})); });
function setActive(p){
  LS.set('dnd_active_char', p.id);
  S = fullChar(p.character); LS.set('dnd_char', JSON.stringify(S));
  // форма конструктора — под нового персонажа сразу: иначе пересчёт цены (syncForm) допишет в него поля от прежнего
  if (CAT) buildForm();
}
// правка в конструкторе сразу попадает в активного персонажа (save() зовётся на каждое изменение)
const baseSave = save;
save = function(){
  baseSave();
  const list = readProfiles(), p = list.find(x => x.id === activeId());
  if (!p || ROUTE !== 'editor') return;
  p.character = clone(S); p.name = (S.name || '').trim() || 'Без имени'; p.budget = +S.budget || 0;
  writeProfiles(list);
};
// вход под именем: персонажи с сервера (или свои несохранённые — на сервер), старый одиночный персонаж — в список
async function loadPlayer(name){
  const key = pkeyJs(name), owner = LS.get('dnd_chars_owner', '');
  const r = await api({cmd:'player_chars_load', name});
  if (r.type !== 'player_chars') return;   // нет связи: работаем с тем, что в браузере
  let local = owner && owner !== key ? [] : readProfiles();
  const server = Array.isArray(r.chars) ? r.chars.filter(p => p && p.id && p.character) : [];
  const dirty = LS.get('dnd_chars_dirty') === '1' && owner === key;
  if (server.length && !dirty){ local = server; LS.set('dnd_chars', JSON.stringify(server)); if (r.active) LS.set('dnd_active_char', r.active); }
  else if (!server.length && !local.length){
    const legacy = await api({cmd:'player_load', name});
    if (legacy && legacy.s && Array.isArray(legacy.s.stats)) local = [{id: newId(), name: legacy.s.name || 'Персонаж', budget: +legacy.s.budget || 0, character: legacy.s}];
  }
  LS.set('dnd_chars_owner', key);
  if (!server.length || dirty){ LS.set('dnd_chars', JSON.stringify(local)); if (local.length) writeProfiles(local); }
  const p = local.find(x => x.id === activeId()) || local[0];
  if (p) setActive(p); else LS.del('dnd_active_char');
}
// вернуться туда, где был: комната (по имени) или незаконченный поиск быстрого боя
async function resumeSession(name){
  const me = await api({cmd:'player_load', name});
  if (me && me.room){
    ROOM.code = me.room; ROOM.n = 0; G.room = '';
    const v = await roomCall({cmd:'room_join', name});
    if (v && v.type === 'room'){ toast('Вы снова в комнате ' + v.code); if (v.status !== 'fight') go('coop'); roomPoll(); return; }
    ROOM.code = '';
  }
  const m = await api({cmd:'match_poll'});
  if (m.type === 'match_wait'){ const p = quickProfile(+m.budget); if (p){ Q.budget = +m.budget; Q.char = p.id; go('quick'); openSearch(+m.budget, m.elapsed || 0); } }
  else if (m.type === 'match') openMatchedRoom(m.code);
}

// ---- имя игрока ----
function askName(required){
  const cur = playerName();
  const m = modal(required ? 'Как вас зовут?' : 'Сменить имя', `<label class="field">Имя игрока<input id="nmIn" maxlength="24" value="${esc(cur)}" autocomplete="nickname"></label>
    <div class="small">От 1 до 24 символов. Под этим именем сохраняются персонажи, место в комнате и кампании — войдите с ним на любом устройстве.</div><div class="form-err" id="nmErr"></div>`,
    `${required ? '' : '<button data-close>Отмена</button>'}<button class="primary" id="nmOk">Сохранить</button>`, {required});
  const input = m.q('#nmIn');
  const ok = async () => {
    const v = input.value.trim(); if (!v || v.length > 24){ m.q('#nmErr').textContent = 'Введите имя от 1 до 24 символов.'; input.focus(); return; }
    m.q('#nmOk').disabled = true;
    const done = await setPlayerName(v); m.q('#nmOk').disabled = false;
    if (done){ m.close(); if (required) resumeSession(v); }
  };
  m.q('#nmOk').onclick = ok;
  input.addEventListener('keydown', e => { if (e.key === 'Enter'){ e.preventDefault(); ok(); } });
}
async function setPlayerName(name){
  name = String(name || '').trim();
  if (!name || name.length > 24){ toast('Имя: от 1 до 24 символов', 4000, true); return false; }
  if (name === playerName()) return true;
  if (ROOM.code || G.active){ toast('Сначала выйдите из комнаты или боя', 4000, true); return false; }
  const before = LS.get('dnd_chars_owner', '');
  LS.set('dnd_rmname', name);
  if (before && before !== pkeyJs(name)){   // смена имени: если под новым именем пусто — персонажи переезжают с игроком
    const r = await api({cmd:'player_chars_load', name});
    if (r.type === 'player_chars' && !(r.chars || []).length) LS.set('dnd_chars_owner', pkeyJs(name));
  }
  await loadPlayer(name);
  if ($('rmName')) $('rmName').value = name;
  buildForm(); queuePrice();
  if (ROUTE === 'settings') renderSettings();
  return true;
}

// ---- жалобы и разработчики (кнопки в углу главного меню) ----
$('reportBtn').onclick = () => {
  const m = modal('Сообщить о проблеме', `<label class="field">Что это<select id="rpCat"><option>Ошибка</option><option>Идея</option><option>Другое</option></select></label>
    <label class="field">Что случилось<textarea id="rpText" rows="6" maxlength="2000" placeholder="Что вы делали и что пошло не так"></textarea></label>
    <div class="small"><span id="rpCount">0</span> / 2000 · к письму приложим экран, тему и браузер</div>
    <label class="field">Как с вами связаться (необязательно)<input id="rpContact" maxlength="200" placeholder="почта или ник"></label><div class="form-err" id="rpErr"></div>`,
    `<button id="rpCancel">Отмена</button><button class="primary" id="rpSend" disabled>Отправить</button>`);
  const tx = m.q('#rpText'), btn = m.q('#rpSend');
  tx.oninput = () => { m.q('#rpCount').textContent = tx.value.length; btn.disabled = tx.value.trim().length < 10; };
  m.q('#rpCancel').onclick = async () => { if (tx.value.trim() && !(await confirmBox('Закрыть и удалить введённый текст?', 'Закрыть'))) return; m.close(); };
  btn.onclick = async () => {
    btn.disabled = true; m.q('#rpErr').textContent = '';
    const r = await api({cmd:'report', category: m.q('#rpCat').value, text: tx.value.trim(), contact: m.q('#rpContact').value.trim(),
      metadata: {version: SITE.version, screen: ROUTE, theme: document.documentElement.dataset.theme, name: playerName(), browser: navigator.userAgent}});
    if (r.type === 'ok'){ m.close(); toast('Спасибо, отправлено'); }
    else { m.q('#rpErr').textContent = r.err || 'Не удалось отправить, повторите'; btn.disabled = tx.value.trim().length < 10; }
  };
};
const SITE = {version: '0.1.0', developers: [], templates: []};
const siteReady = api({cmd:'site_config'}).then(r => { if (r.type === 'site_config') Object.assign(SITE, r); });
$('devBtn').onclick = async () => {
  await siteReady;
  const cards = SITE.developers.length ? SITE.developers.map(p => `<div class="dev-card"><b>${esc(p.name || '')}</b><span class="muted">${esc(p.role || '')}</span>${(p.links || []).filter(l => /^https?:\/\//.test(l.url || '')).map(l => `<a href="${esc(l.url)}" target="_blank" rel="noopener">${esc(l.title || l.url)}</a>`).join('')}</div>`).join('') : '<p class="muted">Сведения о разработчиках пока не заполнены.</p>';
  modal('Разработчики', cards + `<p class="small">Версия ${esc(SITE.version)}</p>`, '<button class="primary" data-close>Закрыть</button>');
};
function templates(){ return SITE.templates.length ? SITE.templates.map((t, i) => ({name: t.name, k: Number.isInteger(+t.preset_index) ? +t.preset_index : i})) : CHAR_PRESETS.map((p, i) => ({name: p.n, k: i})); }

// ---- цена и итог персонажа без открытия конструктора (кэш по содержимому) ----
const PRICE_CACHE = new Map();
async function profilePrice(p){
  const old = S; S = fullChar(p.character); const spec = toChar(); S = old;
  const key = JSON.stringify(spec);
  if (PRICE_CACHE.has(key)) return PRICE_CACHE.get(key);
  const r = await api({cmd:'price', char: spec});
  if (r.type !== 'price') return null;   // связь: не кэшировать
  if (PRICE_CACHE.size > 60) PRICE_CACHE.delete(PRICE_CACHE.keys().next().value);
  PRICE_CACHE.set(key, r.data); return r.data;
}

// ---- быстрый бой ----
const Q = {budget: +LS.get('dnd_match_budget', '1600') || 1600, char: '', searching: false, pollT: 0, gen: 0};
if (!BUDGETS.includes(Q.budget)) Q.budget = 1600;
const quickProfile = budget => { const l = readProfiles().filter(p => +p.budget === budget); return l.find(p => p.id === Q.char) || l.find(p => p.id === activeId()) || l[0] || null; };
function renderQuick(){
  const list = readProfiles();
  $('qBudgets').innerHTML = BUDGETS.map(n => `<button type="button" class="budget${n === Q.budget ? ' on' : ''}" data-budget="${n}"><b>${n}</b><small>персонажей: ${list.filter(p => +p.budget === n).length}</small></button>`).join('');
  $('qCharHead').textContent = `2. Персонаж на ${Q.budget} очков`;
  $('qCreate').textContent = `+ Создать персонажа на ${Q.budget}`;
  const mine = list.filter(p => +p.budget === Q.budget), pick = quickProfile(Q.budget);
  Q.char = pick ? pick.id : '';
  $('qChars').innerHTML = mine.length ? mine.map(p => `<button type="button" class="list-row pick${p.id === Q.char ? ' on' : ''}" data-qchar="${esc(p.id)}"><span class="name"><b>${esc(p.name)}</b></span><span class="meta" data-hp>…</span><span class="meta" data-ac></span><span></span></button>`).join('')
    : '<div class="empty">Нет персонажа на эту сумму — создайте его.</div>';
  $('qFind').disabled = true;
  const gen = ++Q.gen;
  mine.forEach(async p => {
    const d = await profilePrice(p); if (gen !== Q.gen) return;
    const row = $('qChars').querySelector(`[data-qchar="${CSS.escape(p.id)}"]`); if (!row) return;
    row.dataset.ok = d && d.ok ? '1' : '0';
    row.querySelector('[data-hp]').textContent = d && d.ok ? d.boss.hp + ' ХП' : (d ? 'не собран' : 'нет связи');
    row.querySelector('[data-ac]').textContent = d && d.ok ? 'КД ' + d.boss.ac : '';
    if (p.id === Q.char) $('qFind').disabled = !(d && d.ok);
  });
}
$('qBudgets').addEventListener('click', e => { const b = e.target.closest('[data-budget]'); if (!b) return; Q.budget = +b.dataset.budget; LS.set('dnd_match_budget', Q.budget); renderQuick(); });
$('qChars').addEventListener('click', e => {
  const b = e.target.closest('[data-qchar]'); if (!b) return;
  Q.char = b.dataset.qchar; $('qChars').querySelectorAll('[data-qchar]').forEach(x => x.classList.toggle('on', x === b));
  $('qFind').disabled = b.dataset.ok !== '1';
  if (b.dataset.ok === '0') toast('Персонаж не собран: откройте его в «Мои персонажи» и исправьте', 5000, true);
});
$('qCreate').onclick = () => createCharacter(Q.budget, 'quick');
$('qFind').onclick = async () => {
  const p = readProfiles().find(x => x.id === Q.char); if (!p) return;
  $('qFind').disabled = true;
  setActive(p); await doPrice();
  if (!lastPrice || !lastPrice.ok){ $('qFind').disabled = false; toast('Персонаж не собран: ' + ((lastPrice && lastPrice.err) || 'нет связи'), 5000, true); return; }
  const ch = toChar(); if (p.portrait) ch.portrait = p.portrait;
  const r = await api({cmd:'match_find', name: playerName() || S.name, char: ch, budget: Q.budget});
  if (r.type === 'error'){ $('qFind').disabled = false; toast(r.err || 'Не удалось начать поиск', 5000, true); return; }
  if (r.type === 'match_bot') return quickBot();
  if (r.type === 'match') return openMatchedRoom(r.code);
  openSearch(Q.budget, r.elapsed || 0);
};
let searchModal = null;
function openSearch(budget, elapsed){
  stopSearch();
  const p = quickProfile(budget), price = lastPrice && lastPrice.ok ? lastPrice.boss : null;
  searchModal = modal('', `<div class="spinner" aria-hidden="true"></div><h2>Ищем соперника на ${budget} очков</h2><div class="search-clock tnum"><span id="srClock">0:00</span> <small>/ 1:00</small></div>
    <div class="progress"><i id="srBar"></i></div><p class="muted">Если никто не найдётся, подберём бота-нейросеть на те же ${budget} очков.</p>
    <div class="chip-row"><span>${esc(p ? p.name : S.name)}</span><span class="muted">${price ? price.hp + ' ХП · КД ' + price.ac : ''}</span></div>
    <button class="danger" id="srCancel" type="button">Отмена</button>`, '', {required: true, cls: 'search'});
  Q.searching = true; showSearch(elapsed);
  searchModal.q('#srCancel').onclick = async () => { stopSearch(); await api({cmd:'match_cancel'}); renderQuick(); };
  Q.pollT = setTimeout(pollSearch, 900);
}
function showSearch(sec){ sec = Math.max(0, Math.min(60, +sec || 0)); const c = $('srClock'), b = $('srBar'); if (c) c.textContent = `0:${String(Math.min(59, sec)).padStart(2, '0')}`; if (b) b.style.width = (sec / 60 * 100) + '%'; }
function stopSearch(){ Q.searching = false; clearTimeout(Q.pollT); if (searchModal){ searchModal.close(); searchModal = null; } }
async function pollSearch(){
  if (!Q.searching) return;
  const r = await api({cmd:'match_poll'});
  if (!Q.searching) return;
  if (r.type === 'match_wait'){ showSearch(r.elapsed); Q.pollT = setTimeout(pollSearch, 1500); return; }
  stopSearch();
  if (r.type === 'match') return openMatchedRoom(r.code);
  if (r.type === 'match_bot') return quickBot();
  if (/нет связи/.test(r.err || '')){ toast('Связь пропала — поиск продолжается', 4000, true); Q.searching = true; openSearch(Q.budget, 0); return; }
  toast(r.err || 'Поиск прерван', 5000, true); renderQuick();
}
async function quickBot(){
  const p = readProfiles().find(x => x.id === Q.char) || quickProfile(Q.budget);
  if (!p){ toast('Персонаж для боя не найден', 5000, true); go('quick'); return; }
  setActive(p); await doPrice();
  $('oppMode').value = 'net'; $('oppBrain').value = 'net'; $('side').value = '-1'; $('mapSel').value = ''; $('seed').value = 1 + Math.floor(Math.random() * 99999);
  if (!(await startGame())){ go('quick'); toast($('oppErr').textContent || 'Не удалось начать бой', 6000, true); }
}
async function openMatchedRoom(code){
  ROOM.code = code; ROOM.n = 0; G.room = '';
  const v = await roomCall({cmd:'room_join', name: playerName() || S.name});
  if (v.type !== 'room'){ ROOM.code = ''; toast(v.err || 'Не удалось войти в бой', 5000, true); go('quick'); return; }
  go('coop'); roomPoll();
  if (v.host && v.status === 'lobby') roomCall({cmd:'room_start', rounds: 50, seed: 1 + Math.floor(Math.random() * 99999)});   // оба игрока уже готовы
}
ROUTE_ENTER.quick = renderQuick;

// ---- мои персонажи ----
async function renderCharacters(){
  await siteReady;
  const list = readProfiles(), busy = !!ROOM.code || Q.searching;
  const own = list.map(p => `<div class="list-row${p.id === activeId() ? ' on' : ''}" data-id="${esc(p.id)}">
      <span class="name"><span class="avatar">${p.portrait ? `<img src="${esc(p.portrait)}" alt="" loading="lazy">` : esc((p.name || '?').slice(0, 1).toUpperCase())}</span><b>${esc(p.name || 'Без имени')}</b> <span class="small">· ${+p.budget || 0} оч.</span></span>
      <span class="meta" data-hp>…</span><span class="meta" data-ac></span>
      <span class="acts-r"><button data-edit="${esc(p.id)}">Править</button><button data-dl="${esc(p.id)}" title="Скачать файл персонажа">Скачать</button><button class="danger" data-del="${esc(p.id)}" ${busy ? 'disabled title="Нельзя во время поиска или в комнате"' : ''}>Удалить</button></span></div>`).join('');
  const tpl = templates().map(t => `<div class="list-row" data-tplrow="${t.k}"><span class="name"><b>${esc(t.name.split(' — ')[0])}</b> <span class="small">· шаблон · ${Q.budget} оч.</span></span><span class="meta" data-hp>…</span><span class="meta" data-ac></span><span class="acts-r"><button data-tpl="${t.k}" data-tname="${esc(t.name.split(' — ')[0])}">Копия</button></span></div>`).join('');
  $('charList').innerHTML = (own || '<div class="empty">Пока нет своих персонажей — создайте или возьмите копию шаблона.</div>') + tpl;
  templates().forEach(async t => {
    const d = await profilePrice({character: presetChar(t.k, Q.budget)}), row = $('charList').querySelector(`[data-tplrow="${t.k}"]`); if (!row) return;
    row.querySelector('[data-hp]').textContent = d && d.ok ? d.boss.hp + ' ХП' : '—'; row.querySelector('[data-ac]').textContent = d && d.ok ? 'КД ' + d.boss.ac : '';
  });
  list.forEach(async p => {
    const d = await profilePrice(p), row = $('charList').querySelector(`[data-id="${CSS.escape(p.id)}"]`); if (!row) return;
    row.querySelector('[data-hp]').textContent = d && d.ok ? d.boss.hp + ' ХП' : (d ? 'не собран' : '—');
    row.querySelector('[data-ac]').textContent = d && d.ok ? 'КД ' + d.boss.ac : '';
  });
}
ROUTE_ENTER.characters = renderCharacters;
function budgetField(id, def){ return `<label class="field">Сумма очков<select id="${id}">${BUDGETS.map(n => `<option value="${n}" ${n === def ? 'selected' : ''}>${n} очков</option>`).join('')}</select></label>`; }
function createCharacter(budget, from, presetK){
  const c = clone(DEFAULT_CHAR); c.budget = budget; c.name = 'Новый персонаж';
  const list = readProfiles(), p = {id: newId(), name: c.name, budget, character: c}; list.push(p); writeProfiles(list);
  setActive(p); openEditorFor(p.id, from);
  if (presetK != null){ applyCharPreset(presetK); }
}
$('charCreate').onclick = () => {
  const m = modal('Создать персонажа', budgetField('ncBudget', Q.budget), '<button data-close>Отмена</button><button class="primary" id="ncOk">Создать</button>');
  m.q('#ncOk').onclick = () => { const b = +m.q('#ncBudget').value; m.close(); createCharacter(b, 'characters'); };
};
$('charTemplate').onclick = async () => {
  await siteReady;
  const m = modal('Выбрать готового', `<label class="field">Шаблон<select id="tpK">${templates().map(t => `<option value="${t.k}">${esc(t.name)}</option>`).join('')}</select></label>${budgetField('tpB', Q.budget)}`,
    '<button data-close>Отмена</button><button class="primary" id="tpOk">Создать</button>');
  m.q('#tpOk').onclick = () => { const k = +m.q('#tpK').value, b = +m.q('#tpB').value; m.close(); createCharacter(b, 'characters', k); };
};
$('charUpload').onclick = () => $('charFile').click();
$('charFile').onchange = e => { const f = e.target.files[0]; e.target.value = ''; if (f) importCharacterFile(f); };
async function importCharacterFile(f){
  try {
    if (f.size > 1024 * 1024) throw new Error('файл больше 1 МБ');
    let j = JSON.parse(await f.text());
    if (j && j.format === 'gornilo-character') j = j.character;
    if (!j || typeof j !== 'object' || !Array.isArray(j.stats)) throw new Error('это не файл персонажа');
    const c = fullChar(j), list = readProfiles(), names = new Set(list.map(p => p.name));
    let name = (c.name || f.name.replace(/\.json$/i, '')).trim().slice(0, 40) || 'Персонаж', base = name, k = 2;
    while (names.has(name)) name = `${base} (${k++})`;
    c.name = name; list.push({id: newId(), name, budget: +c.budget || 0, character: c}); writeProfiles(list);
    toast(`Персонаж «${name}» загружен`); if (ROUTE === 'characters') renderCharacters();
  } catch (err) { toast('Не удалось прочитать файл: ' + err.message, 6000, true); }
}
$('charList').addEventListener('click', async e => {
  const b = e.target.closest('button'); if (!b || b.disabled) return;
  if (b.dataset.tpl != null){ createCharacter(Q.budget, 'characters', +b.dataset.tpl); return; }
  const list = readProfiles(), id = b.dataset.edit || b.dataset.dl || b.dataset.del, p = list.find(x => x.id === id); if (!p) return;
  if (b.dataset.edit){ setActive(p); openEditorFor(p.id, 'characters'); }
  else if (b.dataset.dl){
    const ch = clone(p.character); delete ch.portrait;
    const url = URL.createObjectURL(new Blob([JSON.stringify({format:'gornilo-character', version:1, character: ch}, null, 1)], {type:'application/json'}));
    const a = document.createElement('a'); a.href = url; a.download = (p.name || 'персонаж') + '.json'; a.click(); setTimeout(() => URL.revokeObjectURL(url), 2000);
    toast(`Файл «${p.name}.json» скачан`);
  } else if (b.dataset.del){
    if (!(await confirmBox(`Удалить «${p.name}»? Это нельзя отменить.`, 'Удалить', true))) return;
    if (p.portrait) api({cmd:'portrait_delete', portrait: p.portrait.split('/').pop()});
    const rest = list.filter(x => x.id !== p.id); writeProfiles(rest);
    if (activeId() === p.id){ if (rest[0]) setActive(rest[0]); else { LS.del('dnd_active_char'); S = clone(DEFAULT_CHAR); } }
    renderCharacters();
  }
});

// ---- экран персонажа (конструктор) ----
let edReturn = 'characters';
function openEditorFor(id, from){ edReturn = from || 'characters'; buildForm(); goStep(0); queuePrice(); go('editor'); }
ROUTE_ENTER.editor = () => { if (!activeProfile()){ go('characters'); return; } renderSteps(); edHead(); };
ROUTE_BACK.editor = () => $('edSave').click();
$('edSave').onclick = () => {
  save();
  if (lastPrice && !lastPrice.ok) toast('Сохранено, но персонаж пока не готов к бою: ' + (lastPrice.err || ''), 6000, true);
  else toast('Персонаж сохранён');
  go(edReturn);
};
$('stepBar').addEventListener('click', e => { const b = e.target.closest('[data-step]'); if (b) goStep(+b.dataset.step); });
const STAT_ABBR = ['СИЛ', 'ЛОВ', 'ТЕЛ', 'ИНТ', 'МДР', 'ХАР'], STAT_NAME = ['Сила', 'Ловкость', 'Телосложение', 'Интеллект', 'Мудрость', 'Харизма'];
// шапка экрана персонажа: портрет, очки без ХП, ХП, КД и характеристики (зовётся после каждого пересчёта цены)
function edHead(){
  if (!$('edStats')) return;
  const d = lastPrice, b = d && d.boss, st = S.stats || [];
  $('edStats').innerHTML = [0, 4, 1, 3, 2, 5].map(k => { const v = +st[k] || 10, mod = Math.floor((v - 10) / 2);
    return `<div class="stat"><i>${STAT_ABBR[k]}</i><span>${STAT_NAME[k]}</span><b class="tnum">${v}</b><em>${mod >= 0 ? '+' : '−'}${Math.abs(mod)}</em></div>`; }).join('');
  $('edAc').textContent = b ? b.ac : '—';
  const bud = +S.budget || 0;
  if (b){
    const hpCost = b.items.filter(it => it.cat === 'hp').reduce((s, it) => s + it.cost, 0), pts = Math.max(0, (d.spent != null ? d.spent : bud) - hpCost);
    $('edPts').textContent = `Очки без учёта ХП: ${pts} / ${bud}`; $('edPtsBar').style.width = Math.min(100, bud ? 100 * pts / bud : 0) + '%';
    $('edHp').textContent = `ХП ${b.hp} / ${b.hp}`; $('edHpBar').style.width = '100%';
  } else {
    $('edPts').textContent = d && d.err ? 'Персонаж не готов: ' + d.err : 'Считаем…'; $('edPtsBar').style.width = '0';
    $('edHp').textContent = 'ХП —'; $('edHpBar').style.width = '0';
  }
  const p = activeProfile(), tok = $('edToken');
  tok.classList.toggle('has', !!(p && p.portrait));
  tok.innerHTML = p && p.portrait ? `<img src="${esc(p.portrait)}" alt="Портрет">` : '<span>Токен создаётся автоматически</span>';
  $('edPortraitBtn').textContent = p && p.portrait ? 'Сменить изображение' : 'Добавить изображение';
}
// портрет: квадрат 512×512 WebP, хранится на сервере (виден в бою и кампании); старый удаляется
$('edPortraitBtn').onclick = () => $('edPortraitFile').click();
$('edPortraitFile').onchange = async e => {
  const f = e.target.files[0]; e.target.value = ''; const p = activeProfile(); if (!f || !p) return;
  if (!['image/png', 'image/jpeg', 'image/webp'].includes(f.type) || f.size > 8 * 1024 * 1024){ toast('Нужна картинка PNG, JPG или WebP до 8 МБ', 5000, true); return; }
  $('edPortraitBtn').disabled = true;
  try {
    const bmp = await createImageBitmap(f), side = Math.min(bmp.width, bmp.height), c = document.createElement('canvas'); c.width = c.height = 512;
    c.getContext('2d').drawImage(bmp, (bmp.width - side) / 2, 0, side, side, 0, 0, 512, 512);   // квадрат сверху: в портрете лицо обычно вверху if (bmp.close) bmp.close();
    let blob = await new Promise(r => c.toBlob(r, 'image/webp', .85));
    if (!blob || blob.type !== 'image/webp') throw new Error('браузер не умеет сохранять WebP');
    if (blob.size > 1.5 * 1024 * 1024) blob = await new Promise(r => c.toBlob(r, 'image/webp', .6));
    const data = await new Promise((res, rej) => { const rd = new FileReader(); rd.onload = () => res(rd.result); rd.onerror = rej; rd.readAsDataURL(blob); });
    const pid = (crypto.randomUUID ? crypto.randomUUID() : 'p' + Date.now().toString(36) + Math.random().toString(36).slice(2)).replace(/[^A-Za-z0-9_-]/g, '');
    const r = await api({cmd:'portrait_put', portraitId: pid, data});
    if (r.type !== 'portrait') throw new Error(r.err || 'сервер не принял файл');
    const list = readProfiles(), q = list.find(x => x.id === p.id); if (!q) return;
    const old = q.portrait; q.portrait = r.url; writeProfiles(list);
    if (old) api({cmd:'portrait_delete', portrait: old.split('/').pop()});
    edHead(); toast('Изображение сохранено');
  } catch (err) { toast('Не удалось загрузить изображение: ' + err.message, 6000, true); }
  finally { $('edPortraitBtn').disabled = false; }
};
// персонаж в комнату: один — сразу, несколько — выбор
function pickRoomCharacter(){
  const list = readProfiles();
  const pack = p => { const old = S; S = fullChar(p.character); const c = toChar(); S = old; if (p.portrait) c.portrait = p.portrait; return c; };
  if (!list.length){ toast('Сначала создайте персонажа в «Мои персонажи»', 5000, true); return Promise.resolve(null); }
  if (list.length === 1) return Promise.resolve(pack(list[0]));
  return new Promise(res => {
    const m = modal('Персонаж для комнаты', `<label class="field">Персонаж<select id="rcSel">${list.map(p => `<option value="${esc(p.id)}" ${p.id === activeId() ? 'selected' : ''}>${esc(p.name)} · ${+p.budget || 0} оч.</option>`).join('')}</select></label>`,
      '<button data-close>Отмена</button><button class="primary" id="rcOk">Выбрать</button>', {onClose: () => res(null)});
    m.q('#rcOk').onclick = () => { const p = list.find(x => x.id === m.q('#rcSel').value); res(p ? pack(p) : null); m.close(); };
  });
}

// ---- настройки ----
const THEMES = [['system', 'Как в системе'], ['dark', 'Тёмная'], ['light', 'Светлая'], ['red-black', 'Красно-чёрная'], ['red-navy', 'Красно-тёмно-синяя'], ['forest', 'Лесная']];
$('themeGrid').innerHTML = THEMES.map(([v, n]) => `<button type="button" class="theme-tile" data-theme="${v}"><i class="sw tt-${v}"></i><span>${n}</span></button>`).join('');
function renderSettings(){
  $('setName').textContent = playerName() || 'не задано';
  $('setNameBtn').disabled = !!ROOM.code || G.active; $('setNameBtn').title = $('setNameBtn').disabled ? 'Сначала выйдите из комнаты или боя' : '';
  const t = document.documentElement.dataset.theme, bgm = document.documentElement.dataset.bg;
  $('themeGrid').querySelectorAll('[data-theme]').forEach(b => { b.classList.toggle('on', b.dataset.theme === t); b.setAttribute('aria-pressed', b.dataset.theme === t); });
  $('bgSeg').querySelectorAll('[data-bg]').forEach(b => b.classList.toggle('on', b.dataset.bg === bgm));
}
ROUTE_ENTER.settings = renderSettings;
$('setNameBtn').onclick = () => askName(false);
$('themeGrid').addEventListener('click', e => { const b = e.target.closest('[data-theme]'); if (!b) return; document.documentElement.dataset.theme = b.dataset.theme; LS.set('dnd_theme', b.dataset.theme); renderSettings(); });
$('bgSeg').addEventListener('click', e => { const b = e.target.closest('[data-bg]'); if (!b) return; document.documentElement.dataset.bg = b.dataset.bg; LS.set('dnd_bg', b.dataset.bg); renderSettings(); });

// ---- фон меню: настоящая карта арены (по одному зерну), размытая ----
(async function drawBackground(){
  const r = await api({cmd:'genmap', seed: 47361, w: 48, h: 30, theme: 0});
  if (r.type !== 'map') return;
  const c = $('bgCv'); c.width = 960; c.height = 600; drawMapFit(c, meFromJson(r.map));
})();

// ---- мастерская: миры (участок мира -> карта для редактора) ----
const WD = {list: [], cur: null, map: null, frame: {x: 0, y: 0, w: 32, h: 20}, snap: '', drag: null, gen: 0};
const readWorlds = () => { try { const v = JSON.parse(LS.get('dnd_worlds', '[]')); return Array.isArray(v) ? v : []; } catch (e) { return []; } };
const writeWorlds = l => { if (!LS.set('dnd_worlds', JSON.stringify(l))) toast('Не удалось сохранить мир: закончилось место', 5000, true); };
const wdState = () => JSON.stringify([WD.cur && WD.cur.name, WD.frame]);
const wdDirty = () => !!WD.cur && WD.snap !== wdState();
function wdCenter(cells){ const w = Math.min(32, cells), h = Math.min(20, cells); return {x: Math.floor((cells - w) / 2), y: Math.floor((cells - h) / 2), w, h}; }
async function wdOpen(w){
  WD.cur = w; WD.frame = Object.assign(wdCenter(w.cells), w.frame || {}); WD.map = null; wdDraw(); wdList();
  $('wdTitle').textContent = `${w.name} · ${w.cells}×${w.cells}`; $('wdOpen').disabled = true;
  const gen = ++WD.gen, r = await api({cmd:'worldmap', seed: String(w.seed), size: Math.max(1000, w.cells * 10)});
  if (gen !== WD.gen) return;
  if (r.type !== 'worldmap'){ toast(r.err || 'Не удалось построить мир', 5000, true); return; }
  WD.map = r; WD.snap = wdState(); $('wdOpen').disabled = false; wdDraw();
}
function wdList(){
  const l = readWorlds();
  $('wdList').innerHTML = l.length ? l.map(w => `<div class="list-row${WD.cur && WD.cur.id === w.id ? ' on' : ''}"><span class="name"><b>${esc(w.name)}</b></span><span class="acts-r"><button class="btn-sm" data-wopen="${esc(w.id)}">Открыть</button><button class="btn-sm danger" data-wdel="${esc(w.id)}" aria-label="Удалить ${esc(w.name)}">✕</button></span></div>`).join('') : '<div class="small">Сохранённых миров пока нет.</div>';
}
function wdDraw(){
  const c = $('wdCv'), [bw, bh] = innerSize(c.parentElement), side = Math.max(160, Math.min(bw - 8, bh - 8));
  c.width = c.height = 800; c.style.width = c.style.height = side + 'px';
  const x = c.getContext('2d'); x.fillStyle = '#203928'; x.fillRect(0, 0, 800, 800);
  if (!WD.map || !WD.cur) return;
  const n = WD.map.n, cell = 800 / n;
  for (let yy = 0; yy < n; yy++) for (let xx = 0; xx < n; xx++){
    const bi = WD.map.biome[yy][xx], lv = +WD.map.height[yy][xx];
    x.fillStyle = BIOME_COL[bi] || '#666'; x.fillRect(xx * cell, yy * cell, cell + 1, cell + 1);
    if (lv && !'~#+V='.includes(bi)){ x.fillStyle = `rgba(255,255,255,${lv * .025})`; x.fillRect(xx * cell, yy * cell, cell + 1, cell + 1); }
  }
  const k = 800 / WD.cur.cells, f = WD.frame;
  x.save(); x.setLineDash([12, 8]); x.lineWidth = 4; x.strokeStyle = '#f4bd19'; x.strokeRect(f.x * k, f.y * k, f.w * k, f.h * k); x.setLineDash([]); x.fillStyle = '#f4bd19';
  [[f.x, f.y], [f.x + f.w, f.y], [f.x, f.y + f.h], [f.x + f.w, f.y + f.h]].forEach(([a, b]) => { x.beginPath(); x.arc(a * k, b * k, 7, 0, 7); x.fill(); }); x.restore();
  $('wdSel').textContent = `${f.w}×${f.h} клеток`;
}
function wdFromInputs(){
  const name = $('wdName').value.trim().slice(0, 40), seed = Math.max(1, Math.trunc(+$('wdSeed').value) || 0), cells = +$('wdSize').value;
  if (!name){ toast('Назовите мир', 4000, true); return null; }
  if (!seed){ toast('Зерно — целое число больше нуля', 4000, true); return null; }
  return {id: 'world-' + Date.now().toString(36), name, seed: String(seed), cells, theme: +$('wdTheme').value || 0};
}
ROUTE_ENTER.worlds = () => { wdList(); if (!WD.cur){ const l = readWorlds(); if (l[0]) wdOpen(l[0]); else { const w = wdFromInputs(); if (w){ w.id = ''; wdOpen(w); } } } else requestAnimationFrame(wdDraw); };
ROUTE_BACK.worlds = async () => {
  if (wdDirty()){
    const m = modal('Сохранить мир?', '<p>Есть несохранённые изменения участка.</p>', '<button data-close>Отмена</button><button id="wdNo">Не сохранять</button><button class="primary" id="wdYes">Сохранить</button>');
    m.q('#wdNo').onclick = () => { m.close(); WD.snap = wdState(); go('home'); };
    m.q('#wdYes').onclick = () => { m.close(); wdSave(); go('home'); };
    return;
  }
  go('home');
};
function wdSave(){
  if (!WD.cur) return;
  const l = readWorlds(); if (!WD.cur.id) WD.cur.id = 'world-' + Date.now().toString(36);
  WD.cur.frame = Object.assign({}, WD.frame);
  const i = l.findIndex(w => w.id === WD.cur.id); if (i >= 0) l[i] = WD.cur; else l.push(WD.cur);
  writeWorlds(l); WD.snap = wdState(); wdList(); toast('Мир сохранён');
}
$('wdSave').onclick = wdSave;
$('wdCreate').onclick = () => { const w = wdFromInputs(); if (!w) return; const l = readWorlds(); l.push(w); writeWorlds(l); wdOpen(w); toast('Мир создан'); };
$('wdRandom').onclick = () => { $('wdSeed').value = 1 + Math.floor(Math.random() * 99999); };
$('wdList').addEventListener('click', async e => {
  const b = e.target.closest('button'); if (!b) return;
  const l = readWorlds(), w = l.find(x => x.id === (b.dataset.wopen || b.dataset.wdel)); if (!w) return;
  if (b.dataset.wdel){
    if (!(await confirmBox(`Удалить мир «${w.name}»?`, 'Удалить', true))) return;
    writeWorlds(l.filter(x => x.id !== w.id)); if (WD.cur && WD.cur.id === w.id){ WD.cur = null; WD.map = null; WD.snap = ''; wdDraw(); $('wdOpen').disabled = true; $('wdTitle').textContent = 'Мир'; }
    wdList(); return;
  }
  if (wdDirty() && !(await confirmBox('Есть несохранённые изменения. Открыть другой мир?', 'Открыть'))) return;
  wdOpen(w);
});
$('wdExport').onclick = () => {
  if (!WD.cur){ toast('Сначала откройте мир', 4000, true); return; }
  const w = Object.assign({}, WD.cur, {frame: WD.frame}), url = URL.createObjectURL(new Blob([JSON.stringify({type:'gornilo-worlds', version:1, worlds:[w]}, null, 1)], {type:'application/json'}));
  const a = document.createElement('a'); a.href = url; a.download = (w.name || 'мир') + '.json'; a.click(); setTimeout(() => URL.revokeObjectURL(url), 2000);
};
const wdCell = e => { const r = $('wdCv').getBoundingClientRect(), n = WD.cur.cells; return [Math.max(0, Math.min(n, Math.round((e.clientX - r.left) / r.width * n))), Math.max(0, Math.min(n, Math.round((e.clientY - r.top) / r.height * n)))]; };
$('wdCv').addEventListener('pointerdown', e => {
  if (!WD.map || !WD.cur) return;
  const [x, y] = wdCell(e), f = WD.frame, near = 1.5 * WD.cur.cells / 100 + 1;
  const corner = [['nw', f.x, f.y], ['ne', f.x + f.w, f.y], ['sw', f.x, f.y + f.h], ['se', f.x + f.w, f.y + f.h]].find(([, a, b]) => Math.abs(a - x) <= near && Math.abs(b - y) <= near);
  const inside = x >= f.x && x <= f.x + f.w && y >= f.y && y <= f.y + f.h;
  WD.drag = {kind: corner ? 'resize' : inside ? 'move' : 'draw', h: corner ? corner[0] : '', x, y, f: Object.assign({}, f)};
  $('wdCv').setPointerCapture(e.pointerId);
});
$('wdCv').addEventListener('pointermove', e => {
  const d = WD.drag; if (!d) return;
  const [x, y] = wdCell(e), n = WD.cur.cells, o = d.f;
  const fit = (x0, y0, w0, h0) => { const w = Math.max(5, Math.min(100, n, w0)), h = Math.max(5, Math.min(100, n, h0)); return {x: Math.max(0, Math.min(n - w, x0)), y: Math.max(0, Math.min(n - h, y0)), w, h}; };
  if (d.kind === 'move') WD.frame = fit(o.x + x - d.x, o.y + y - d.y, o.w, o.h);
  else if (d.kind === 'draw') WD.frame = fit(Math.min(d.x, x), Math.min(d.y, y), Math.abs(x - d.x), Math.abs(y - d.y));
  else { let l = o.x, t = o.y, r = o.x + o.w, b = o.y + o.h;
    if (d.h.includes('w')) l = Math.min(x, r - 5); if (d.h.includes('e')) r = Math.max(x, l + 5); if (d.h.includes('n')) t = Math.min(y, b - 5); if (d.h.includes('s')) b = Math.max(y, t + 5);
    WD.frame = fit(l, t, r - l, b - t); }
  wdDraw();
});
['pointerup', 'pointercancel'].forEach(ev => $('wdCv').addEventListener(ev, () => { WD.drag = null; }));
$('wdOpen').onclick = async () => {
  if (!WD.map || !WD.cur) return;
  const f = WD.frame, n = WD.map.n, k = n / WD.cur.cells, cx = Math.max(0, Math.min(n - 1, Math.floor((f.x + f.w / 2) * k))), cy = Math.max(0, Math.min(n - 1, Math.floor((f.y + f.h / 2) * k)));
  const bi = WD.map.biome[cy][cx], level = +WD.map.height[cy][cx];
  const terrain = {a:'тундра', b:'тайга', c:'ледник', d:'степь', e:'лес', f:'болото', g:'пустыня', h:'саванна', i:'джунгли'}[bi] || 'лес';
  $('wdOpen').disabled = true;
  const r = await api({cmd:'regionmap', cell: {terrain, level, river: bi === '~', road: bi === '=' ? 'NESW' : '', settle: bi === '#' ? 'город' : bi === '+' ? 'деревня' : '', seed: `${WD.cur.seed}:${cx}:${cy}`}, w: f.w, h: f.h});
  $('wdOpen').disabled = false;
  if (r.type !== 'map'){ toast(r.err || 'Не удалось открыть участок', 5000, true); return; }
  if (typeof meDirty === 'function' && meDirty() && !(await confirmBox('В редакторе карт есть несохранённая карта. Заменить её участком мира?', 'Заменить'))) return;
  ME = meFromJson(r.map, `${WD.cur.name} — участок`); meSaved = ''; go('workshop'); meMsg('Участок мира открыт: правьте и сохраните.');
};
addEventListener('resize', () => { if (ROUTE === 'worlds') wdDraw(); });

// ---- Esc — назад (если не открыто окно и поле боя не развёрнуто) ----
addEventListener('keydown', e => {
  if (e.key !== 'Escape' || ROUTE === 'home' || document.querySelector('.modal-bg,.kmp-delete-backdrop') || document.fullscreenElement || $('stagebox').classList.contains('bigfield')) return;
  if (/INPUT|TEXTAREA|SELECT/.test((document.activeElement || {}).tagName || '')){ document.activeElement.blur(); return; }
  e.preventDefault(); goBack();
});
$('meProps').onclick = () => document.body.classList.toggle('ws-props');

startApp();
