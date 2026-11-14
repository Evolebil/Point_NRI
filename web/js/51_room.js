// ---- комната: бой нескольких команд (опрос сервера раз в секунду) ----
let TEAMS = [];
const ROOM = {code:'', n:0, timer:null, key:'', v:null, lobbyHtml:'', mapChoice:''};
const TEAM_COL = ['#3d8bff','#ff5a45','#3fbf6a','#e5b83a','#b36bff','#30c5c5','#ff8ad8','#a0a0a0'];
function teamCol(t){ return TEAM_COL[t % TEAM_COL.length]; }
try { $('rmName').value = localStorage.getItem('dnd_rmname') || ''; } catch (e) {}
function roomJoinValidity(){const name=$('rmName').value.trim(),code=$('rmCode').value; $('rmCreate').disabled=!name||name.length>24;$('rmEnter').disabled=!name||name.length>24||code.length!==5;}
$('rmName').addEventListener('input',roomJoinValidity);
$('rmCode').addEventListener('input',()=>{$('rmCode').value=$('rmCode').value.toUpperCase().replace(/[^A-Z0-9]/g,'').slice(0,5);roomJoinValidity();});
roomJoinValidity();
async function roomCall(o){ const v = await api(Object.assign({code: ROOM.code, since: ROOM.n}, o)); roomFeed(v); return v; }
// долгий опрос: сервер держит запрос, пока в комнате ничего не изменилось (до 20 с) — ответ приходит сразу после хода
// любого игрока. Нет связи (туннель упал) — плашка и повтор с паузой; персонаж и место берегут сервер и имя игрока
function connBanner(on){
  let el = $('connBar');
  if (!el){ el = document.createElement('div'); el.id = 'connBar'; el.className = 'connbar'; el.hidden = true; document.body.appendChild(el); }
  el.textContent = '⚠ Нет связи с сервером — переподключаюсь… (если ссылка сменилась, откройте новую и войдите под тем же именем: вы вернётесь в бой)';
  el.hidden = !on;
}
async function roomPoll(){
  clearTimeout(ROOM.timer); if (!ROOM.code) return;
  const my = ROOM.pollId = (ROOM.pollId || 0) + 1;
  const v = await api({cmd:'room_poll', code: ROOM.code, since: ROOM.n, ver: ROOM.v ? ROOM.v.ver : undefined});
  if (my !== ROOM.pollId) return;   // запущен новый опрос
  const net = v && v.type === 'error' && /нет связи|Failed|NetworkError|JSON/.test(v.err || '');
  connBanner(net);
  if (v && v.type === 'error' && !net && /нет такой комнаты|комната не найдена/i.test(v.err || '')){ toast('Комната закрыта (сервер перезапускался?)', 5000, true); roomReset(); if (ROUTE === 'coop') go('coop'); return; }
  if (!net) roomFeed(v);
  // ошибка без связи — пауза растёт; другая ошибка — тоже пауза (иначе опрос крутится без остановки)
  const bad = net || !v || v.type === 'error';
  ROOM.retry = bad ? Math.min((ROOM.retry || 0) + 1, 5) : 0;
  ROOM.timer = setTimeout(roomPoll, bad ? [1000, 2000, 4000, 8000, 10000][ROOM.retry - 1] : 50);
}
function roomFeed(v){
  if (!v) return;
  if (v.type === 'error'){   // ход не дошёл (связь, «не ваш ход») — панель снова доступна, без перезагрузки
    toast(v.err);
    if (G.room && G.req && !G.over){ G.busy = false; renderTurn(); }
    return;
  }
  if (v.type !== 'room') return;
  ROOM.v = v; ROOM.code = v.code;
  if (v.status === 'lobby' || (!G.room && !v.init)) roomLobby(v);
  if (v.init && G.room !== v.code){   // бой начался: экран боя
    G.room = v.code; G.wNames = null; G.active = false; G.over = false; G.busy = false; G.mode = 'move'; G.logged = 0; G.shown = null; G.pending = null; G.req = G.lastReq = null;
    $('log').innerHTML = ''; $('result').classList.remove('on');
    const ini = v.init; MAP = ini.map; AW = MAP.aw; AH = MAP.ah; BOSS = ini.bosses; TEAMS = ini.team; G.me = Math.max(0, ini.seat.indexOf(v.seat)); FR = []; terrain = null;
    BOSS.forEach(b => { b.minion = b.minion || null; }); G.enemyBrain = 'правило-бот';
    const late = v.frames.length > 3;   // вернулся посреди боя: история сразу, без проигрывания
    FR = late ? v.frames.slice() : v.frames.slice(0, 1); ROOM.n = v.frames.length; G.active = true; G.seen = {melee:false, ranged:false, abil:{}}; G.sentAt = FR.length;
    showTab(3); fitPlay(); camReset(); setCamMode(cam.mode); seek(late ? FR.length - 1 : 0);
    if (late) logUpTo(FR.length - 1);
    ROOM.key = ''; roomTurn(v, late ? [] : v.frames.slice(1), true); return;
  }
  if (G.room === v.code && G.active){
    roomTurn(v, v.frames);
    if (v.status === 'lobby'){ G.room = ''; }   // хозяин вернул комнату в лобби (ошибка движка)
  }
}
const v0 = () => ROOM.v || {};
function roomReset(){
  clearTimeout(ROOM.timer); ROOM.pollId = (ROOM.pollId || 0) + 1;
  ROOM.code = ''; ROOM.n = 0; ROOM.v = null; ROOM.lobbyHtml = ''; ROOM.autoStart = false; G.room = ''; G.active = false; clearInterval(turnTimer);
  $('rmBox').innerHTML = ''; $('rmJoin').hidden = false; $('result').classList.remove('on'); connBanner(false);
}
async function roomLeave(){
  await api({cmd:'room_leave', code: ROOM.code});
  roomReset(); go('home');
}
function roomTurn(v, frames, first){
  if (!first){   // ответы могут прийти вперемешку (опрос и свой ход): берём только кадры, которых ещё нет
    const start = v.nframes - frames.length;
    if (v.nframes < ROOM.n) return;   // устаревший ответ
    frames = frames.slice(Math.max(0, ROOM.n - start));
  }
  ROOM.n = Math.max(ROOM.n, v.nframes);
  const key = (v.req ? JSON.stringify(v.req) : '') + '|' + (v.end ? 'e' : '') + '|' + v.waiting;
  if (!frames.length && key === ROOM.key) return;
  ROOM.key = key;
  const r = v.req ? {type:'turn', req:v.req} : (v.end ? Object.assign({type:'end'}, v.end) : {type:'wait', waiting:v.waiting});
  r.frames = frames;
  receive(r);
}
// лобби комнаты: участники слева, карта справа (макет «Комната»)
function roomLobby(v){
  const host = v.host, mine = v.members.find(m => m.me);
  const nT = Math.max(2, ...v.members.map(m => m.team + 2));
  const teamSel = m => `<select class="team" data-rmteam="${m.i}" ${m.me || host ? '' : 'disabled'} aria-label="Команда">${[...Array(Math.min(64, nT)).keys()].map(t => `<option value="${t}" ${t === m.team ? 'selected' : ''}>команда ${t + 1}</option>`).join('')}</select>`;
  const brainSel = m => m.bot == null || m.npc ? `<span class="brain muted">${m.npc ? 'противник-бот' : esc(m.charName || '')}</span>` :
    `<select class="brain" data-rmbrain="${m.i}" ${host && v.status === 'lobby' ? '' : 'disabled'} aria-label="Кто ведёт бота"><option value="net" ${m.brain === 'net' ? 'selected' : ''}>Нейросеть</option><option value="rules" ${m.brain !== 'net' ? 'selected' : ''}>Алгоритм</option></select>`;
  const who = m => m.bot != null && !m.npc ? `${esc(m.name)} <span class="small">· ${m.bot} оч.</span>` : `${esc(m.name)}${m.me ? ' <span class="small">(вы)</span>' : ''}${m.host ? ' <span class="small">· хозяин</span>' : ''}`;
  const rows = v.members.map((m, k) => `<div class="member${m.me ? ' me' : ''}">
      <span class="num" style="background:${teamCol(m.team)}">${k + 1}</span><span class="nm" title="${esc(m.name)}">${who(m)}</span>${brainSel(m)}${teamSel(m)}
      <span class="ready${m.ready ? '' : ' no'}">${m.ready ? 'готов' : (m.selected ? 'не готов' : 'нет героя')}</span>
      ${host && !m.me && v.status === 'lobby' ? `<button class="danger kick" data-rmkick="${m.i}" title="Убрать" aria-label="Убрать ${esc(m.name)}">✕</button>` : '<span></span>'}</div>`).join('');
  const state = v.status === 'fight' ? 'идёт бой' : v.status === 'end' ? 'бой окончен' : 'ждём игроков';
  let h = `<div class="room-head"><h1>Комната</h1><span class="room-code">${esc(v.code)}</span><button id="rmCopy" type="button">Скопировать код</button>
    <span class="room-state">${state} · ${host ? 'вы — хозяин' : 'хозяин начинает бой'}</span></div>${v.err ? `<div class="err">${esc(v.err)}</div>` : ''}
    <div class="room-cols"><div>
      <div class="panel"><h2>Участники</h2>${rows}${v.status === 'lobby' ? '<div class="member-free">Свободное место — ждём друзей</div>' : ''}</div>
      <div class="panel room-tools">
        <button id="rmMine" type="button">${mine && mine.selected ? 'Сменить персонажа' : 'Взять моего персонажа'}</button>
        ${mine && mine.selected ? `<button id="rmReady" type="button" class="${mine.ready ? '' : 'primary'}">${mine.ready ? 'Не готов' : 'Готов'}</button>` : ''}
        ${host && v.status === 'lobby' ? `<span class="sep"></span><span class="muted">Бот:</span><div class="seg" id="rmBotSeg"><button type="button" data-brain="rules">Алгоритм</button><button type="button" data-brain="net" class="on">Нейросеть</button></div>
        <span class="muted">бюджет</span><input type="number" id="rmBotB" value="${(lastPrice && lastPrice.ok && S.budget) || 1600}" min="20" max="100000" aria-label="Бюджет бота"><button id="rmBot" type="button">+ Добавить бота</button>` : ''}
      </div></div>
      <div class="panel room-map"><h2>Карта</h2>
        <div class="seg"><button type="button" data-rmmap="gen" class="${ROOM.mapKind !== 'own' ? 'on' : ''}">Сгенерированная</button><button type="button" data-rmmap="own" class="${ROOM.mapKind === 'own' ? 'on' : ''}">Своя</button></div>
        <canvas id="rmMapCv" width="640" height="372" aria-label="Предпросмотр карты"></canvas>
        <div class="room-line"${ROOM.mapKind === 'own' ? ' hidden' : ''}><span>Код карты (зерно)</span><input type="number" id="rmSeed" min="1" max="99999" value="${ROOM.seed}" ${host ? '' : 'disabled'} aria-label="Зерно карты"><button id="rmSeedDice" type="button" ${host ? '' : 'disabled'}>Случайная</button></div>
        <div class="room-line"${ROOM.mapKind === 'own' ? '' : ' hidden'}><span>Своя карта</span><select id="rmOwn" ${host ? '' : 'disabled'} aria-label="Своя карта">${roomOwnOptions()}</select></div>
        <hr><div class="room-line"><span>Предел раундов</span><select id="rmRounds" ${host ? '' : 'disabled'} aria-label="Предел раундов">${[10, 20, 50, 0].map(n => `<option value="${n}" ${n === ROOM.rounds ? 'selected' : ''}>${n || 'без предела'}</option>`).join('')}</select></div>
      </div></div>
    <div class="room-foot"><span class="small">${host ? 'Нужны две команды и готовые игроки.' : 'Начать бой может только хозяин комнаты.'}</span>
      ${v.status === 'fight' && !G.room ? '<button id="rmBack" type="button" class="primary">Вернуться в бой</button>' : ''}
      ${host ? `<button class="btn-accent btn-xl" id="rmStart" type="button" ${v.status !== 'lobby' || v.members.some(m => m.bot == null && !m.ready) || new Set(v.members.map(m => m.team)).size < 2 ? 'disabled' : ''}>Начать бой</button>` : ''}</div>`;
  if (ROOM.lobbyHtml !== h){   // перерисовать, только если что-то изменилось (иначе стирается ввод)
    const fo = document.activeElement && document.activeElement.id, val = fo && $(fo) ? $(fo).value : null;
    ROOM.lobbyHtml = h; $('rmBox').innerHTML = h;
    if (ROOM.botBrain) $('rmBox').querySelectorAll('#rmBotSeg [data-brain]').forEach(b => b.classList.toggle('on', b.dataset.brain === ROOM.botBrain));
    if (fo && $(fo) && val != null){ $(fo).value = val; $(fo).focus(); }
    roomMapPreview();
  }
  $('rmJoin').hidden = true;
  if (ROUTE === 'coop') go('coop');   // надпись «Выйти из комнаты» на кнопке назад
}
// выбор карты комнаты: своя (из мастерской) или сгенерированная по зерну; предпросмотр — та же карта, что пойдёт в бой
Object.assign(ROOM, {mapKind: 'gen', seed: 1 + Math.floor(Math.random() * 99999), own: '', rounds: 50, botBrain: 'net', genMap: null, genKey: ''});
function roomOwnOptions(){
  const names = Object.keys(MAPS);
  if (!names.length) return '<option value="">нет сохранённых карт — нарисуйте в Мастерской</option>';
  if (!names.includes(ROOM.own)) ROOM.own = names[0];
  return names.map(n => `<option value="${esc(n)}" ${n === ROOM.own ? 'selected' : ''}>${esc(n)} · ${MAPS[n].aw}×${MAPS[n].ah}</option>`).join('');
}
async function roomMapPreview(){
  const cv2 = $('rmMapCv'); if (!cv2) return;
  let m = null;
  if (ROOM.mapKind === 'own') m = MAPS[ROOM.own] || null;
  else {
    const key = String(ROOM.seed);
    if (ROOM.genKey !== key){ ROOM.genKey = key; ROOM.genMap = null; const r = await api({cmd:'genmap', seed: ROOM.seed, w: 32, h: 20, theme: 0}); if (ROOM.genKey === key && r.type === 'map') ROOM.genMap = r.map; }
    m = ROOM.genMap;
  }
  const c = $('rmMapCv'); if (!c) return;
  if (!m){ const x = c.getContext('2d'); x.clearRect(0, 0, c.width, c.height); return; }
  drawMapFit(c, meFromJson(m));
}
function roomMapPayload(){
  if (ROOM.mapKind === 'own') return MAPS[ROOM.own] ? {map: MAPS[ROOM.own]} : {};
  return ROOM.genMap && ROOM.genKey === String(ROOM.seed) ? {map: ROOM.genMap} : {};
}
$('rmCreate').onclick = async () => {
  const name = $('rmName').value.trim(); if (!name || name.length > 24){ toast('Введите имя игрока (1–24 символа)', 4000, true); return; }
  if (!(await setPlayerName(name))) return;
  $('rmCreate').disabled = true; ROOM.n = 0; G.room = '';
  const v = await api({cmd:'room_create', name}); $('rmCreate').disabled = false;
  if (v.type === 'room'){ roomFeed(v); roomPoll(); } else toast(v.err || 'Не удалось создать комнату', 5000, true);
};
$('rmEnter').onclick = async () => {
  const name = $('rmName').value.trim(), code = $('rmCode').value.trim().toUpperCase();
  if (!name || code.length !== 5){ toast('Введите имя и код из пяти знаков', 4000, true); return; }
  if (!(await setPlayerName(name))) return;
  $('rmEnter').disabled = true; ROOM.code = code; ROOM.n = 0; G.room = '';
  const v = await roomCall({cmd:'room_join', name}); roomJoinValidity();
  if (v.type === 'room') roomPoll(); else ROOM.code = '';
};
$('rmCode').addEventListener('keydown', e => { if (e.key === 'Enter' && !$('rmEnter').disabled) $('rmEnter').click(); });
$('rmBox').addEventListener('click', async e => {
  const b = e.target.closest('button'); if (!b || b.disabled) return;
  if (b.id === 'rmCopy'){ (navigator.clipboard ? navigator.clipboard.writeText(ROOM.code) : Promise.reject()).then(() => toast('Код скопирован: ' + ROOM.code), () => toast('Код комнаты: ' + ROOM.code)); return; }
  if (b.id === 'rmBack'){ G.room = ''; ROOM.n = 0; await roomCall({cmd:'room_poll'}); return; }
  if (b.id === 'rmMine'){ const char = await pickRoomCharacter(); if (char) await roomCall({cmd:'room_set', char, name: playerName() || S.name}); return; }
  if (b.id === 'rmReady'){ const me = ROOM.v.members.find(m => m.me); await roomCall({cmd:'room_set', ready: !(me && me.ready)}); return; }
  if (b.dataset.brain){ ROOM.botBrain = b.dataset.brain; b.parentElement.querySelectorAll('button').forEach(x => x.classList.toggle('on', x === b)); return; }
  if (b.id === 'rmBot'){
    const counts = Array(64).fill(0); ROOM.v.members.forEach(m => { counts[m.team]++; });
    const mine = ROOM.v.members.find(m => m.me), team = counts.findIndex((c, t) => t !== (mine ? mine.team : -1) && t < Math.max(2, ...ROOM.v.members.map(m => m.team + 1)));
    await roomCall({cmd:'room_bot', budget: Math.max(20, Math.min(100000, +$('rmBotB').value || 1600)), team: team < 0 ? 1 : team, brain: ROOM.botBrain}); return;
  }
  if (b.dataset.rmkick){ const member = ROOM.v.members[+b.dataset.rmkick]; if (!(await confirmBox(`Убрать из комнаты «${member ? member.name : 'игрока'}»?`, 'Убрать', true))) return; await roomCall({cmd:'room_kick', i: +b.dataset.rmkick}); return; }
  if (b.dataset.rmmap){ ROOM.mapKind = b.dataset.rmmap; ROOM.lobbyHtml = ''; roomLobby(ROOM.v); return; }
  if (b.id === 'rmSeedDice'){ ROOM.seed = 1 + Math.floor(Math.random() * 99999); $('rmSeed').value = ROOM.seed; roomMapPreview(); return; }
  if (b.id === 'rmStart'){
    if (ROOM.mapKind === 'own' && !MAPS[ROOM.own]){ toast('Выберите свою карту или сгенерированную', 4000, true); return; }
    if (ROOM.mapKind === 'gen' && !(ROOM.genMap && ROOM.genKey === String(ROOM.seed))) await roomMapPreview();
    b.disabled = true;
    ROOM.n = 0; G.room = ''; await roomCall(Object.assign({cmd:'room_start', rounds: ROOM.rounds, seed: ROOM.seed}, roomMapPayload()));
  }
});
$('rmBox').addEventListener('change', async e => {
  const t = e.target;
  if (t.id === 'rmSeed'){ ROOM.seed = Math.max(1, Math.min(99999, Math.trunc(+t.value) || 1)); t.value = ROOM.seed; roomMapPreview(); return; }
  if (t.id === 'rmOwn'){ ROOM.own = t.value; roomMapPreview(); return; }
  if (t.id === 'rmRounds'){ ROOM.rounds = +t.value; return; }
  if (t.dataset.rmbrain){ await roomCall({cmd:'room_set', i: +t.dataset.rmbrain, brain: t.value}); return; }
  if (!t.dataset.rmteam) return;
  const m = ROOM.v.members[+t.dataset.rmteam];
  if (m.me) await roomCall({cmd:'room_set', team: +t.value});
  else if (ROOM.v.host) await roomCall({cmd:'room_set', i: m.i, team: +t.value});
});

async function startGame(){
  if (!lastPrice || !lastPrice.ok){ showTab(1); return false; }
  $('oppErr').innerHTML = '';
  $('btnFight').disabled = true;
  const character=toChar();try{const active=localStorage.getItem('dnd_active_char'),profile=JSON.parse(localStorage.getItem('dnd_chars')||'[]').find(p=>p.id===active);if(profile&&profile.portrait)character.portrait=profile.portrait;}catch(e){}
  const msg = {cmd:'start', char: character, seed: +$('seed').value, opp: oppCfg(), side: +$('side').value};
  const mname = $('mapSel').value; if (mname && MAPS[mname]) msg.map = MAPS[mname];
  if (mname === '__world') msg.world = {seed: $('wSeed').value.trim() || '1', size: +$('wSize').value, x: WPICK ? WPICK[0] : -1, y: WPICK ? WPICK[1] : -1, w: +$('wW').value, h: +$('wH').value};
  const r = await api(msg);
  $('btnFight').disabled = false;
  if (r.type === 'error'){ $('oppErr').innerHTML = `<div class="err">${esc(r.err)}</div>`; return false; }
  // новая игра
  G.active = false; G.over = false; G.busy = false; G.mode = 'move'; G.logged = 0; G.shown = null; G.pending = null; G.req = G.lastReq = null; G.wNames = null; $('log').innerHTML = ''; $('result').classList.remove('on');
  const ini = r.init;
  MAP = ini.map; AW = MAP.aw; AH = MAP.ah; BOSS = ini.bosses; G.me = ini.me; FR = []; terrain = null;
  BOSS.forEach((b, k) => { b.minion = b.minion || null; });
  G.enemyBrain = ini.enemyBrain || '';
  FR = r.frames.slice(0, 1); G.active = true;
  G.seen = {melee:false, ranged:false, abil:{}}; G.sentAt = 1;
  showTab(3); fitPlay(); camReset(); setCamMode(cam.mode); seek(0);
  receive(r, 1);
  return true;
}
// порция ответа движка: кадры добавляются, потом (когда рисовальщик всё показал) — ход игрока или итог
function receive(r, skip = 0){
  G.busy = true; $('turn').innerHTML = '<div class="hint">…</div>';
  if (r.type === 'error'){
    if (/игра не идёт|бой прерван/i.test(r.err || '')){ fightBroken(r.err); return; }
    toast(r.err, 5000, true); G.busy = false; if (G.req && !G.over) renderTurn(); return;
  }   // вернуть кнопки хода, иначе панель так и останется «…»
  const frames = r.frames.slice(skip);
  FR.push(...frames);
  G.pending = r;
  if (curIdx < FR.length - 1){ playing = true; holdUntil = performance.now() + 60; }
  else onCaughtUp();
}
function onCaughtUp(){
  const r = G.pending; if (!r || G.shown === r) return;
  if (curIdx < FR.length - 1) return;
  G.shown = r;
  if (r.type === 'wait') { G.req = null; G.busy = true; $('turn').innerHTML = `<div class="hint">⏳ Ходит: <b>${esc(r.waiting || 'бот')}</b>… <button class="danger" data-leave="1">🚪 Выйти из боя</button></div>`; return; }
  if (r.type === 'end') { clearInterval(turnTimer); showEnd(r); }
  else { G.req = G.lastReq = r.req; G.busy = false; renderTurn(); if (r.req.msg) toast(r.req.msg + altHint(r.req)); startTurnTimer(); }
}
async function cmd(c){
  if (G.busy || G.over) return;
  G.busy = true; clearInterval(turnTimer); toast('');
  G.sentAt = FR.length;
  if (G.room){ const v = await api(Object.assign({cmd:'room_step', code:G.room, since:ROOM.n}, c)); roomFeed(v); return; }
  const r = await api(Object.assign({cmd:'step'}, c));
  receive(r);
}
function showEnd(r){
  G.over = true; G.busy = true;
  if (G.room){   // бой команд: победила команда (или ничья по очкам)
    const myT = TEAMS[G.me], win = r.result === myT, draw = r.result < 0;
    // очки нужны только при пределе раундов: доля оставшихся хитов команды (0…1) + 1 за каждого убитого врага
    const sc = r.reason !== 5 ? '' : `<p class="small">Очки = доля оставшихся хитов команды (от 0 до 1) + 1 за каждого убитого врага</p>` +
      (r.scores || []).map((v, t) => `<div>Команда ${t + 1}${t === myT ? ' (ваша)' : ''}: <b>${(+v).toFixed(2)}</b></div>`).join('');
    $('turn').innerHTML = `<div class="hint">Бой окончен.</div>`;
    const box = $('result').querySelector('.box');
    box.innerHTML = `<h2 style="color:${draw ? '#ddd' : (win ? '#5fd08a' : '#ff7b7b')}">${draw ? 'Ничья' : (win ? 'Победа команды!' : 'Победила команда ' + (r.result + 1))}</h2>
      <p>${r.reason === 5 ? 'Предел раундов — победа по очкам (доля хитов команды + убитые враги)' : 'Осталась одна команда'}</p><p class="small">${r.rounds} раундов</p>${sc}
      <div class="btns"><button class="primary" id="rLobby">В комнату</button><button id="rHide">Посмотреть поле</button></div>`;
    $('result').classList.add('on');
    $('rLobby').onclick = () => { $('result').classList.remove('on'); showTab(6); }; $('rHide').onclick = () => $('result').classList.remove('on');
    return;
  }
  const win = r.result === G.me, draw = r.result === 2;
  const why = ['Оба живы после предохранителя (500 раундов)', win ? 'Вы победили: соперник повержен' : 'Вы повержены', win ? 'Соперник вышел за край арены: побег — это поражение' : 'Вы вышли за край арены: побег — это поражение',
    win ? 'Соперник оторвался от погони: побег — это поражение' : 'Вы оторвались от погони: побег — это поражение', win ? 'Соперник выбился из сил: он «ушёл» и проиграл' : 'Вы ушли от выбившегося из сил соперника — и проиграли'][r.reason] || '';
  $('turn').innerHTML = `<div class="hint">Бой окончен.</div>`;
  const box = $('result').querySelector('.box');
  box.innerHTML = `<h2 style="color:${draw ? '#ddd' : (win ? '#5fd08a' : '#ff7b7b')}">${draw ? 'Ничья' : (win ? 'Победа!' : 'Поражение')}</h2><p>${why}</p><p class="small">${r.rounds} раундов</p>
    <div class="btns"><button class="primary" id="rAgain">Ещё бой</button><button id="rSame">Тот же соперник</button><button id="rHide">Посмотреть поле</button><button id="rEdit">В главное меню</button></div>`;
  $('result').classList.add('on');
  $('rAgain').onclick = () => { $('seed').value = 1 + Math.floor(Math.random() * 99999); prevKey = ''; startGame(); };
  $('rSame').onclick = startGame; $('rEdit').onclick = () => { G.active = false; go('home'); }; $('rHide').onclick = () => $('result').classList.remove('on');
}

// высота считается в расстояние (10 фт за уровень): объяснить, почему цель «далеко», когда летун высоко
function altHint(q){
  const me = FR[curIdx] && FR[curIdx].f[q.me];
  return me && me.alt && /дальше дальности/.test(q.msg || '') ? ` Вы на высоте ${me.alt * 10} фт — высота тоже входит в расстояние до цели, спуститесь ниже.` : '';
}
function fightBroken(err){
  G.over = true; G.busy = true; G.req = null; clearInterval(turnTimer);
  $('turn').innerHTML = '<div class="hint">Бой прерван.</div>';
  const box = $('result').querySelector('.box');
  box.innerHTML = `<h2>Бой прерван</h2><p>${esc(err || 'Сервер перезапускался.')}</p><div class="btns"><button class="primary" id="rAgain">Новый бой</button><button id="rEdit">В главное меню</button></div>`;
  $('result').classList.add('on');
  $('rAgain').onclick = () => { $('seed').value = 1 + Math.floor(Math.random() * 99999); startGame(); };
  $('rEdit').onclick = () => { G.active = false; go('home'); };
}
// выйти из боя: в комнате — герой остаётся, его ходы завершаются сами; одиночный бой — бросить
async function leaveFight(){
  if (G.over){ G.active = false; go(G.room ? 'coop' : 'home'); return; }
  if (G.room){ if (await confirmBox('Выйти из боя? Ваш герой останется, его ходы будут завершаться сами.', 'Выйти', true)) await roomLeave(); return; }
  if (!(await confirmBox('Сдаться и выйти из боя? Бой будет засчитан как поражение.', 'Сдаться', true))) return;
  G.active = false; G.over = true; clearInterval(turnTimer); G.req = null; $('turn').innerHTML = '<div class="hint">Бой брошен.</div>'; go('home');
}
ROUTE_BACK.battle = leaveFight;
ROUTE_BACK.coop = async () => { if (!ROOM.code) return go('home'); if (await confirmBox('Выйти из комнаты?' + (v0().status === 'fight' ? ' Ваш герой останется в бою, его ходы будут завершаться сами.' : ''), 'Выйти', true)) await roomLeave(); };
ROUTE_ENTER.coop = () => { if (!ROOM.code){ $('rmJoin').hidden = false; $('rmBox').innerHTML = ''; ROOM.lobbyHtml = ''; } const n = playerName(); if (n && !$('rmName').value) $('rmName').value = n; roomJoinValidity(); };
