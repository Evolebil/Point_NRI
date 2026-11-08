// ---- комната: бой нескольких команд (опрос сервера раз в секунду) ----
let TEAMS = [];
const ROOM = {code:'', n:0, timer:null, key:'', v:null, lobbyHtml:''};
const TEAM_COL = ['#3d8bff','#ff5a45','#3fbf6a','#e5b83a','#b36bff','#30c5c5','#ff8ad8','#a0a0a0'];
function teamCol(t){ return TEAM_COL[t % TEAM_COL.length]; }
try { $('rmName').value = localStorage.getItem('dnd_rmname') || ''; } catch (e) {}
async function roomCall(o){ const v = await api(Object.assign({code: ROOM.code, since: ROOM.n}, o)); roomFeed(v); return v; }
// долгий опрос: сервер держит запрос, пока в комнате ничего не изменилось (до 20 с) — ответ приходит сразу после хода
// любого игрока. Нет связи (туннель упал) — плашка и повтор с паузой; персонаж и место берегут сервер и имя игрока
function connBanner(on){
  let el = $('connBar');
  if (!el){ el = document.createElement('div'); el.id = 'connBar'; el.style.cssText = 'position:fixed;left:0;right:0;top:0;z-index:99;padding:8px;text-align:center;background:#b33;color:#fff;font-weight:700;display:none'; document.body.appendChild(el); }
  el.textContent = '⚠ Нет связи с сервером — переподключаюсь… (если ссылка сменилась, откройте новую и войдите под тем же именем: вы вернётесь в бой)';
  el.style.display = on ? 'block' : 'none';
}
async function roomPoll(){
  clearTimeout(ROOM.timer); if (!ROOM.code) return;
  const my = ROOM.pollId = (ROOM.pollId || 0) + 1;
  const v = await api({cmd:'room_poll', code: ROOM.code, since: ROOM.n, ver: ROOM.v ? ROOM.v.ver : undefined});
  if (my !== ROOM.pollId) return;   // запущен новый опрос
  const net = v && v.type === 'error' && /нет связи|Failed|NetworkError|JSON/.test(v.err || '');
  connBanner(net);
  if (v && v.type === 'error' && !net && /нет такой комнаты/.test(v.err || '')){ toast('Комната закрыта (сервер перезапускался?)'); ROOM.code = ''; $('rmJoin').style.display = ''; return; }
  if (!net) roomFeed(v);
  ROOM.timer = setTimeout(roomPoll, net ? 3000 : 50);
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
    G.room = v.code; G.wNames = null; G.active = false; G.over = false; G.busy = false; G.mode = 'move'; G.logged = 0; G.shown = null; G.pending = null; G.req = null;
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
async function roomLeave(){
  await api({cmd:'room_leave', code: ROOM.code});
  clearTimeout(ROOM.timer); ROOM.code = ''; ROOM.n = 0; ROOM.v = null; ROOM.lobbyHtml = ''; G.room = ''; G.active = false; clearInterval(turnTimer);
  $('rmBox').innerHTML = ''; $('rmJoin').style.display = ''; $('result').classList.remove('on'); showTab(6);
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
function roomLobby(v){
  const host = v.host, mine = v.members.find(m => m.me);
  const nT = Math.max(2, ...v.members.map(m => m.team + 2));
  const teamSel = (m) => `<select data-rmteam="${m.i}" ${m.me || (host && m.bot != null) ? '' : 'disabled'}>${[...Array(Math.min(64, nT)).keys()].map(t => `<option value="${t}" ${t === m.team ? 'selected' : ''}>команда ${t + 1}</option>`).join('')}</select>`;
  let h = `<div class="row" style="margin:10px 0"><span>Код комнаты: <b style="font-size:24px;letter-spacing:3px">${esc(v.code)}</b></span><span class="small">${v.status === 'fight' ? 'идёт бой' : (v.status === 'end' ? 'бой окончен' : 'ждём игроков')}</span>${v.err ? `<span class="err">${esc(v.err)}</span>` : ''}</div>`;
  h += `<table class="small" style="width:100%"><tr><th style="text-align:left">Кто</th><th>Команда</th><th>Готов</th><th></th></tr>` + v.members.map(m => `<tr><td><span class="chip" style="background:${teamCol(m.team)}">${m.team + 1}</span> ${esc(m.name)}${m.me ? ' (вы)' : ''}${m.bot != null ? ` · бот, бюджет ${m.bot}` : ''}</td><td>${teamSel(m)}</td><td style="text-align:center">${m.ready ? '✔' : '—'}</td><td>${host && !m.me ? `<button data-rmkick="${m.i}" class="danger">✕</button>` : ''}</td></tr>`).join('') + `</table>`;
  h += `<div class="row" style="margin-top:10px"><button id="rmMine" class="primary">Взять моего персонажа («${esc(S.name || '')}»)</button>${mine && mine.ready ? '<span class="small">персонаж выбран ✔</span>' : '<span class="small">персонаж не выбран</span>'}</div>`;
  if (host) h += `<div class="row" style="margin-top:10px"><b>Хозяин:</b> бот — бюджет <input type="number" id="rmBotB" value="${(lastPrice && lastPrice.ok && S.budget) || 500}" style="width:80px"> в команду <input type="number" id="rmBotT" value="2" min="1" max="64" style="width:50px"><button id="rmBot">+ бот</button>
    <label>Предел раундов ${'<select id="rmRounds"><option value="10">10</option><option value="20">20</option><option value="50" selected>50</option><option value="0">без предела (500)</option></select>'}</label>
    <button class="primary" id="rmStart" ${v.status === 'fight' ? 'disabled' : ''}>⚔ Начать бой</button></div>
    <div class="small">Карта — как во вкладке «Соперник» (своя карта или участок мира); по умолчанию — случайный участок мира.</div>`;
  else h += `<div class="small" style="margin-top:10px">Бой начинает хозяин комнаты.</div>`;
  h += `<div class="row" style="margin-top:12px"><button id="rmLeave" class="danger">🚪 Выйти из комнаты</button>${v.status === 'fight' && !G.room ? `<button id="rmBack">⚔ Вернуться в бой</button>` : ''}</div>`;
  if (v.status === 'fight' && !G.room) h += `<div class="small">Бой уже идёт — дождитесь следующего.</div>`;
  if (ROOM.lobbyHtml !== h){   // перерисовать, только если что-то изменилось (иначе стирается ввод)
    const fo = document.activeElement && document.activeElement.id, val = fo && $(fo) ? $(fo).value : null;
    ROOM.lobbyHtml = h; $('rmBox').innerHTML = h;
    if (fo && $(fo) && val != null){ $(fo).value = val; $(fo).focus(); }
  }
  $('rmJoin').style.display = 'none';
}
$('rmCreate').onclick = async () => { try { localStorage.setItem('dnd_rmname', $('rmName').value); } catch (e) {} ROOM.n = 0; G.room = ''; await api({cmd:'room_create', name: $('rmName').value || S.name}).then(v => { roomFeed(v); roomPoll(); }); };
$('rmEnter').onclick = async () => { try { localStorage.setItem('dnd_rmname', $('rmName').value); } catch (e) {} ROOM.code = $('rmCode').value.trim().toUpperCase(); ROOM.n = 0; G.room = ''; const v = await roomCall({cmd:'room_join', name: $('rmName').value || S.name}); if (v.type === 'room') roomPoll(); else ROOM.code = ''; };
$('rmBox').addEventListener('click', async e => {
  const b = e.target.closest('button'); if (!b) return;
  if (b.id === 'rmLeave'){ if (confirm('Выйти из комнаты?' + (v0().status === 'fight' ? ' Ваш герой останется в бою, его ходы будут завершаться сами.' : ''))) await roomLeave(); return; }
  if (b.id === 'rmBack'){ G.room = ''; ROOM.n = 0; await roomCall({cmd:'room_poll'}); return; }
  if (b.id === 'rmMine'){ if (!lastPrice || !lastPrice.ok){ toast('Персонаж не собран: проверьте вкладку «Персонаж»'); return; } await roomCall({cmd:'room_set', char: toChar(), name: $('rmName').value || S.name}); }
  if (b.id === 'rmBot') await roomCall({cmd:'room_bot', budget: +$('rmBotB').value, team: Math.max(0, +$('rmBotT').value - 1)});
  if (b.dataset.rmkick) await roomCall({cmd:'room_kick', i: +b.dataset.rmkick});
  if (b.id === 'rmStart'){
    const o = {cmd:'room_start', rounds: +$('rmRounds').value, seed: 1 + Math.floor(Math.random() * 99999)};
    const mname = $('mapSel').value; if (mname && MAPS[mname]) o.map = MAPS[mname];
    if (mname === '__world') o.world = {seed: $('wSeed').value.trim() || '1', size: +$('wSize').value, x: WPICK ? WPICK[0] : -1, y: WPICK ? WPICK[1] : -1, w: +$('wW').value, h: +$('wH').value};
    ROOM.n = 0; G.room = ''; await roomCall(o);
  }
});
$('rmBox').addEventListener('change', async e => {
  const t = e.target.closest('[data-rmteam]'); if (!t) return;
  const m = ROOM.v.members[+t.dataset.rmteam];
  if (m.me) await roomCall({cmd:'room_set', team: +t.value});
  else toast('Команду бота меняют, убрав его и добавив заново');
});
const toast = (t, ms = 3500) => { const e = $('toast'); e.textContent = t; e.classList.toggle('on', !!t); clearTimeout(toast.t); if (t) toast.t = setTimeout(() => e.classList.remove('on'), ms); };

async function startGame(){
  if (!lastPrice || !lastPrice.ok){ showTab(1); return; }
  $('oppErr').innerHTML = '';
  $('btnFight').disabled = true;
  const msg = {cmd:'start', char: toChar(), seed: +$('seed').value, opp: oppCfg(), side: +$('side').value};
  const mname = $('mapSel').value; if (mname && MAPS[mname]) msg.map = MAPS[mname];
  if (mname === '__world') msg.world = {seed: $('wSeed').value.trim() || '1', size: +$('wSize').value, x: WPICK ? WPICK[0] : -1, y: WPICK ? WPICK[1] : -1, w: +$('wW').value, h: +$('wH').value};
  const r = await api(msg);
  $('btnFight').disabled = false;
  if (r.type === 'error'){ $('oppErr').innerHTML = `<div class="err">${esc(r.err)}</div>`; return; }
  // новая игра
  G.active = false; G.over = false; G.busy = false; G.mode = 'move'; G.logged = 0; G.shown = null; G.pending = null; G.req = null; G.wNames = null; $('log').innerHTML = ''; $('result').classList.remove('on');
  const ini = r.init;
  MAP = ini.map; AW = MAP.aw; AH = MAP.ah; BOSS = ini.bosses; G.me = ini.me; FR = []; terrain = null;
  BOSS.forEach((b, k) => { b.minion = b.minion || null; });
  G.enemyBrain = ini.enemyBrain || '';
  FR = r.frames.slice(0, 1); G.active = true;
  G.seen = {melee:false, ranged:false, abil:{}}; G.sentAt = 1;
  showTab(3); fitPlay(); camReset(); setCamMode(cam.mode); seek(0);
  receive(r, 1);
}
// порция ответа движка: кадры добавляются, потом (когда рисовальщик всё показал) — ход игрока или итог
function receive(r, skip = 0){
  G.busy = true; $('turn').innerHTML = '<div class="hint">…</div>';
  if (r.type === 'error'){ toast(r.err); G.busy = false; if (G.req && !G.over) renderTurn(); return; }   // вернуть кнопки хода, иначе панель так и останется «…»
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
  else { G.req = r.req; G.busy = false; renderTurn(); if (r.req.msg) toast(r.req.msg); startTurnTimer(); }
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
    <div class="btns"><button class="primary" id="rAgain">Ещё раз (другой соперник)</button><button id="rSame">Тот же соперник</button><button id="rEdit">К персонажу</button><button id="rHide">Посмотреть поле</button></div>`;
  $('result').classList.add('on');
  $('rAgain').onclick = () => { $('seed').value = 1 + Math.floor(Math.random() * 99999); prevKey = ''; startGame(); };
  $('rSame').onclick = startGame; $('rEdit').onclick = () => showTab(1); $('rHide').onclick = () => $('result').classList.remove('on');
}

// выйти из боя: в комнате — герой остаётся, его ходы завершаются сами; одиночный бой — бросить
async function leaveFight(){
  if (G.room){ if (confirm('Выйти из боя? Ваш герой останется, его ходы будут завершаться сами.')) await roomLeave(); return; }
  if (!confirm('Бросить этот бой?')) return;
  G.active = false; G.over = true; clearInterval(turnTimer); G.req = null; $('turn').innerHTML = '<div class="hint">Бой брошен.</div>'; showTab(1);
}
