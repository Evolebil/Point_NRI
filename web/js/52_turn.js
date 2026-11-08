// ---- панель хода ----
const REQ = () => G.req;
function myPos(){ const f = FR[curIdx].f[G.req ? G.req.me : G.me]; return f; }   // ходящий: свой герой или свой миньон
function renderTurn(){
  const q = G.req, el = $('turn'); if (!q) return;
  const atkOk = q.weapon.armed && (q.actionFree || q.weapon.atkLeft > 0);
  if (G.mode === 'attack' && !atkOk) G.mode = 'move';
  if (G.mode.startsWith('attack:')){ const a = (q.alts || []).find(x => x.k === +G.mode.slice(7)); if (!a || !(a.left > 0 || q.actionFree)) G.mode = 'move'; }
  if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; if (!a || !a.avail) G.mode = 'move'; }
  const mv = q.speed0 ? Math.min(100, 100 * q.speedLeft / Math.max(q.speed0, q.speedLeft)) : 0;
  const tt = +$('turnTime').value;
  let h = `<div class="tbar"><b>${q.me === G.me ? 'Ваш ход' : 'Ход вашего приспешника'} · раунд ${q.round}</b>${tt ? `<span class="timer" id="timer">⏱ ${tt} с</span>` : ''}<span>Движение: <b class="tnum">${q.speedLeft}</b> фт</span><div class="mv"><i style="width:${mv}%"></i></div>
    <span><span class="dot ${q.actionFree ? 'free' : ''}"></span>действие</span><span><span class="dot ${q.bonusFree ? 'free b' : ''}"></span>бонусное</span>
    <button class="primary endb" data-c="end" title="Клавиша E или Enter">⏭ Завершить ход</button><button class="danger" data-leave="1" title="Выйти из боя">🚪</button></div>`;
  // порядок ходов (инициатива из первого кадра боя); живые, текущий ходящий выделен
  const ini = (FR[0] && (FR[0].ev || []).find(e => e.k === 'init')) || (FR[1] && (FR[1].ev || []).find(e => e.k === 'init'));
  if (ini){ const fl = FR[FR.length - 1].f; h += `<div class="small" style="margin:4px 0">⏱ Порядок ходов: ${ini.order.filter(([k]) => fl[k] && fl[k].hp > 0).map(([k, v]) => k === q.me ? `<b>${nameOf(k, fl)} (${v})</b>` : `${nameOf(k, fl)} (${v})`).join(' → ')}</div>`; }
  // что сделал соперник с вашего прошлого решения
  const rec = [];
  for (let k = Math.max(1, G.sentAt || 1); k < FR.length; k++){ const fr = FR[k]; if (fr.actor >= 0 && fr.f[fr.actor] && fr.f[fr.actor].team !== fr.f[G.me].team){ const L = frameLog(k); rec.push(`<div class="ln ${L.cls}">${L.lines.join('<br>')}${L.evs.join('')}</div>`); } }
  if (rec.length) h += `<div class="recap"><b>Пока вы ждали</b>${rec.slice(-5).join('')}</div>`;
  h += `<div class="acts">`;
  h += `<button data-m="move" class="sp ${G.mode === 'move' ? 'sel' : ''}" title="Клавиша M; клик по клетке — идти, по врагу в досягаемости — удар"><span>🦶 Идти</span><small>клик по полю</small></button>`;
  const wTag = !q.weapon.armed ? 'безоружны' : (q.weapon.atkLeft > 0 ? `ещё атак: ${q.weapon.atkLeft} (можно идти и сменить цель)` : `${q.weapon.attacks > 1 ? q.weapon.attacks + ' атаки по одной' : '1 атака'}, ${q.weapon.range} фт`);
  h += `<button data-m="attack" class="sp ${G.mode === 'attack' ? 'sel' : ''}" ${atkOk ? '' : 'disabled'} title="Клавиша A; затем клик по врагу"><span>⚔ Оружие</span><small>${esc(wTag)}</small></button>`;
  // сколько атак по выбранной цели сразу: из доступных в этом действии (остальные — потом, по другим целям)
  const atkMax = G.mode === 'attack' ? (q.weapon.atkLeft > 0 ? q.weapon.atkLeft : q.weapon.attacks)
    : (G.mode.startsWith('attack:') ? (a => a ? (a.left > 0 ? a.left : a.att) : 1)((q.alts || []).find(x => x.k === +G.mode.slice(7))) : 1);
  if (atkMax > 1){ G.atkN = Math.min(Math.max(1, G.atkN || 1), atkMax); h += `<label class="small" title="Сколько атак по цели одним щелчком">атак по цели <input type="number" id="atkN" min="1" max="${atkMax}" value="${G.atkN}" style="width:56px"> из ${atkMax}</label>`; }
  q.abils.forEach(a => {
    const sp = a.title ? a : BOSS[G.me].abil[a.k];
    const tag = [a.react ? 'реакция, сама' : (a.bonus ? 'бонус' : 'действие'), a.self ? (a.tele ? 'прыжок ' + a.tele + ' фт' : 'на себя') : (a.ground ? 'по клетке, ' : '') + a.range + ' фт' + (a.radius ? ', радиус ' + a.radius : '') + (a.wallLen ? ', барьер ' + a.wallLen + ' кл.' : ''), a.uses >= 0 ? a.uses + ' ост.' : ''].filter(Boolean).join(' · ');
    h += `<button class="sp ${G.mode === 'spell:' + a.k ? 'sel' : ''}" data-spell="${a.k}" ${a.avail ? '' : 'disabled'} title="${esc(a.why || ((S.abilities[a.k] || {})._desc ? S.abilities[a.k]._desc + ' — ' : '') + (sp.desc || ''))} (клавиша ${a.k + 1})"><span>✨ ${esc(sp.title)}</span><small>${esc(tag)}</small></button>`;
  });
  if (q.alts && q.alts.length){
    // оружие мультиатаки: удары в том же действии «Атака», что и основное (укус + когти)
    const started = q.weapon.atkLeft > 0 || q.alts.some(a => a.left > 0);
    h += q.alts.filter(a => a.multi).map(a => { const ok = a.left > 0 || (!started && q.actionFree); return `<button data-m="attack:${a.k}" class="sp ${G.mode === 'attack:' + a.k ? 'sel' : ''}" ${ok ? '' : 'disabled'} title="Удар этим оружием в действии «Атака»; затем клик по врагу"><span>⚔ ${esc(wpnNames()[a.k + 1] || 'Оружие')}</span><small>${a.left > 0 ? 'ещё ударов: ' + a.left : esc(a.t)}</small></button>`; }).join('');
    // запасное оружие: 1-я смена за ход бесплатно, 2-я — бонусным, 3-я — действием
    const can = q.swaps === 0 || (q.swaps === 1 && q.bonusFree) || (q.swaps === 2 && q.actionFree), cost = ['бесплатно', 'бонусное действие', 'действие'][q.swaps] || 'нельзя';
    h += q.alts.filter(a => !a.multi).map(a => `<button class="sp" data-swap="${a.k}" ${can ? '' : 'disabled'} title="Сменить оружие (${cost})"><span>🔁 ${esc(wpnNames()[a.k + 1] || 'Запасное')}</span><small>${esc(a.t)}</small></button>`).join('');
  }
  h += `</div><div class="acts minor">
    <button data-c="dash" ${q.actionFree ? '' : 'disabled'} title="Действие: движения вдвое больше">💨 Рывок</button>
    <button data-c="dodge" ${q.actionFree ? '' : 'disabled'} title="Действие: атаки по вам с помехой до вашего хода">🛡 Уклонение</button>
    <button data-c="disengage" ${q.actionFree ? '' : 'disabled'} title="Действие: уход без провоцированных атак">🏃 Отход</button>
    <button data-c="help" ${q.actionFree && q.enemies.some(en => en.dist <= 5) ? '' : 'disabled'} title="Враг вплотную: первая атака вашего союзника по нему — с преимуществом">🤝 Помощь</button>
    <button data-c="ready" ${q.actionFree && (q.weapon.armed || q.abils.some(a => a.avail && !a.react)) ? '' : 'disabled'} title="Действием подготовить: что сделать реакцией и при каком условии (выбор рядом)">⏳ Подготовка</button>
    <select id="readyK" title="Что подготовить">${q.weapon.armed ? `<option value="-1">удар оружием</option>` : ''}${q.abils.filter(a => !a.react).map(a => `<option value="${a.k}" ${+G.readyK === a.k ? 'selected' : ''}>${esc(a.title)}</option>`).join('')}</select>
    <select id="readyT" title="При каком условии"><option value="1" ${G.readyT != 2 ? 'selected' : ''}>когда враг войдёт в досягаемость</option><option value="2" ${G.readyT == 2 ? 'selected' : ''}>когда враг применит способность</option></select>
    ${q.surgeLeft > 0 ? `<button data-c="surge" ${!q.actionFree ? '' : 'disabled'}>⚡ Всплеск (${q.surgeLeft})</button>` : ''}
    ${q.conc >= 0 ? `<button data-c="dropconc" title="Бесплатно: эффекты концентрации снимаются">🧘 Снять концентрацию</button>` : ''}
    <button data-c="toward" title="Идти к ближайшему врагу">🎯 К врагу</button>
    ${(q.doors || []).map(d => `<button data-door="${d[0]},${d[1]}" title="Взаимодействие: первое за ход бесплатно, дальше — действием">🚪 ${d[2] === 1 ? 'Открыть' : 'Закрыть'} дверь</button>`).join('')}
    ${q.fly ? `<button data-alt="1" ${q.fly.can && q.fly.alt < q.fly.max && q.speedLeft >= 10 ? '' : 'disabled'} title="10 фт движения за уровень; уход вверх из досягаемости ближника провоцирует атаку">🪽 Выше</button><button data-alt="-1" ${q.fly.alt > 0 && q.speedLeft >= 10 ? '' : 'disabled'} title="10 фт движения за уровень">⬇ Ниже</button><span class="tag" title="Высота полёта над землёй">${q.fly.alt ? 'в воздухе ' + q.fly.alt * 10 + ' фт' : 'на земле'}</span>` : ''}</div>
    <div class="msg" id="tmsg"></div><div class="hint">${modeHint()}</div>`;
  el.innerHTML = h;
  if (Math.abs(el.offsetHeight - lastTurnH) > 40 && viewActive()) fitPlay();
}
// ---- таймер хода (реальное время): по истечении ход заканчивается сам ----
let turnTimer = null;
function startTurnTimer(){
  clearInterval(turnTimer);
  const tt = +$('turnTime').value; if (!tt || G.over) return;
  const t0 = performance.now();
  turnTimer = setInterval(() => {
    const left = Math.ceil(tt - (performance.now() - t0) / 1000), el = $('timer');
    if (G.busy || !G.req || G.over){ clearInterval(turnTimer); return; }
    if (el){ el.textContent = '⏱ ' + Math.max(0, left) + ' с'; el.classList.toggle('low', left <= 5); }
    if (left <= 0){ clearInterval(turnTimer); toast('Время хода вышло: ход передан сопернику.'); cmd({c:'end'}); }
  }, 250);
}
function modeHint(){
  const q = G.req;
  if (G.mode === 'move') return 'Режим «идти»: щёлкните клетку на поле (движение тратит скорость; труднопроходимая местность и подъём дороже; уход из досягаемости ближнего врага даёт ему провоцированную атаку, кроме действия «Отход»).';
  if (G.mode.startsWith('attack:')) return 'Щёлкните врага — удар этим оружием в действии «Атака» (мультиатака). Удары основным оружием — кнопкой «Оружие».';
  if (G.mode === 'attack') return 'Щёлкните врага — одна атака (красные кольца — цели в дальности и в прямой видимости). Остальные атаки действия — следующими щелчками; между ними можно идти. Щелчок по стене, дереву или укрытию — удар по объекту (у объектов есть КД и хиты).';
  if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; return a && a.ground ? 'Щёлкните клетку на поле: там встанет барьер или сработает область (превью — по курсору).' : (a && a.self && a.tele ? 'Щёлкните клетку, куда прыгнуть.' : (a && a.self && a.ally ? `Щёлкните себя или союзника (до ${a.range} фт).` : 'Щёлкните врага — цель способности.')); }
  return '';
}
$('turn').addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b || G.busy || !G.req) return;
  if (b.dataset.m){ G.mode = b.dataset.m; renderTurn(); return; }
  if (b.dataset.spell != null){
    const k = +b.dataset.spell, a = G.req.abils[k];
    if (a.ground){ G.mode = 'spell:' + k; renderTurn(); return; }
    if (a.self && !a.tele && !a.ally){ cmd({c:'spell', k}); return; }   // на себя — цели нет; «и на союзника» — щелчок по себе или союзнику
    G.mode = G.mode === 'spell:' + k ? 'move' : 'spell:' + k; renderTurn(); return;   // цель — кликом по полю; повторное нажатие — отмена
  }
  if (b.dataset.door){ const [x, y] = b.dataset.door.split(',').map(Number); cmd({c:'door', x, y}); return; }
  if (b.dataset.alt){ cmd({c:'alt', dir: +b.dataset.alt}); return; }
  if (b.dataset.swap != null){ const k = +b.dataset.swap, nm = wpnNames(); [nm[0], nm[k + 1]] = [nm[k + 1], nm[0]]; cmd({c:'swap', k}); return; }   // имена меняются местами, как оружие в движке
  if (b.dataset.leave){ leaveFight(); return; }
  if (b.dataset.c === 'toward'){
    const me = myPos(), en = G.req.enemies.slice().sort((x, y) => x.dist - y.dist)[0]; if (!en) return;
    const ef = FR[curIdx].f[en.i]; cmd({c:'move', x: ef.x, y: ef.y}); return;
  }
  if (b.dataset.c === 'help'){   // Помощь — против ближайшего врага вплотную
    const en = G.req.enemies.filter(e => e.dist <= 5).sort((x, y) => x.dist - y.dist)[0]; if (!en) return;
    cmd({c:'help', tgt: en.i}); return;
  }
  if (b.dataset.c === 'ready'){ cmd({c:'ready', k: +($('readyK') || {value: -1}).value, trig: +($('readyT') || {value: 1}).value}); return; }   // подготовка с выбором (8 ноября)
  if (b.dataset.c) cmd({c: b.dataset.c});
});
// Оружие по кнопке: если единственная цель в дальности — бьём сразу
$('turn').addEventListener('dblclick', () => {});
$('turn').addEventListener('input', e => { if (e.target.id === 'atkN') G.atkN = Math.max(1, +e.target.value || 1); });   // атак по цели
$('turn').addEventListener('change', e => { if (e.target.id === 'readyK') G.readyK = +e.target.value; if (e.target.id === 'readyT') G.readyT = +e.target.value; });
function weaponClick(){ const q = G.req; const ok = q.enemies.filter(en => !en.charm && en.dist <= q.weapon.range && en.los); if (ok.length === 1) cmd({c:'attack', tgt: ok[0].i, reckless: q.weapon.reckless}); else { G.mode = 'attack'; renderTurn(); } }

// ---- клики по полю ----
function cellAt(e){
  const r = cv.getBoundingClientRect(), px = (e.clientX - r.left) * (cvW() / r.width), py = (e.clientY - r.top) * (cvH() / r.height);
  return {x: Math.floor((px - cvW() / 2) / cam.cell + cam.x), y: Math.floor((py - cvH() / 2) / cam.cell + cam.y)};
}
// имена оружия в бою: [в руках, запасные…]; смена меняет местами (как swapWeapon в движке)
function wpnNames(){
  if (!G.wNames || G.wNamesFor !== G.req){ if (!G.wNames) G.wNames = ['Основное', ...(S.altWeapons || []).map((q, i) => q.name || 'Запасное ' + (i + 1))]; G.wNamesFor = G.req; }
  return G.wNames;
}
// свой боец (или союзник) в клетке — цель способности «и на союзника»
function allyAt(c){
  const fr = FR[curIdx], my = fr.f[G.req ? G.req.me : G.me].team; let best = null;
  fr.f.forEach((f, k) => { if (f.hp <= 0 || f.team !== my || f.fled) return; const n = f.n || 1; if (c.x >= f.x && c.x < f.x + n && c.y >= f.y && c.y < f.y + n) best = k; });
  return best;
}
function enemyAt(c){
  const fr = FR[curIdx]; let best = null, bd = 1e9;
  const gap = (a, n, b) => Math.max(0, b - (a + n - 1), a - b);   // клетка — до квадрата крупного (n×n от x, y)
  fr.f.forEach((f, k) => { if (f.hp <= 0 || f.team === fr.f[G.me].team || f.fled) return; const n = f.n || 1, d = Math.max(gap(f.x, n, c.x), gap(f.y, n, c.y)); if (d <= 0 && d < bd){ bd = d; best = k; } });
  if (best == null) fr.f.forEach((f, k) => { if (f.hp <= 0 || f.team === fr.f[G.me].team || f.fled) return; const n = f.n || 1, d = Math.hypot(f.x + (n - 1) / 2 - c.x, f.y + (n - 1) / 2 - c.y) - (n - 1) / 2; if (d < 1.6 && d < bd){ bd = d; best = k; } });
  return best;
}
cv.addEventListener('click', e => {
  if (e.shiftKey) return;   // Shift + перетаскивание — сдвиг обзора, не ход
  if (G.busy || !G.req || G.over) return;
  const c = cellAt(e), q = G.req;
  if (G.mode === 'move'){ const en = enemyAt(c); if (en != null && (q.actionFree || q.weapon.atkLeft > 0) && q.weapon.armed){ const E = q.enemies.find(x => x.i === en); if (E && E.dist <= q.weapon.range && E.los){ cmd({c:'attack', tgt: en, reckless: q.weapon.reckless}); return; } } cmd({c:'move', x:c.x, y:c.y}); return; }
  if (G.mode.startsWith('attack:')){   // удар оружием мультиатаки
    const en = enemyAt(c); if (en == null){ toast('Щёлкните по врагу.'); return; }
    cmd({c:'attack', tgt: en, wk: +G.mode.slice(7), n: G.atkN || 1}); return;
  }
  if (G.mode === 'attack'){
    const en = enemyAt(c);
    if (en == null){ if (isObjCell(c.x, c.y)) cmd({c:'hitobj', x:c.x, y:c.y}); else toast('Щёлкните по врагу или по объекту (стена, дерево, укрытие).'); return; }
    cmd({c:'attack', tgt: en, reckless: q.weapon.reckless, n: G.atkN || 1}); return;
  }
  if (G.mode.startsWith('spell:')){
    const k = +G.mode.slice(6), a = q.abils[k];
    if ((a.self && a.tele) || a.ground){ cmd({c:'spell', k, x:c.x, y:c.y}); return; }
    if (a.self && a.ally){ const al = allyAt(c); if (al == null){ toast('Щёлкните себя или союзника.'); return; } cmd({c:'spell', k, tgt: al}); return; }
    const en = enemyAt(c); if (en == null){ toast('Щёлкните по врагу.'); return; } cmd({c:'spell', k, tgt: en});
  }
});
// объект в клетке (стена, дерево, укрытие) и не разрушен (обломки — в изменениях арены, вид 4)
function isObjCell(x, y){
  if (x < 0 || y < 0 || x >= AW || y >= AH) return false;
  const fr = FR[curIdx]; if (fr.dyn && fr.dyn.some(d => d[0] === x && d[1] === y && d[2] === 4)) return false;
  return ['wall', 'tree', 'climb'].some(k => MAP[k] && MAP[k][y] && MAP[k][y][x] === '1');
}
let hover = null;
cv.addEventListener('mousemove', e => { hover = cellAt(e); });
// свободный обзор: колесо — масштаб к курсору, правая кнопка или Shift + перетаскивание — сдвиг (обычный щелчок — ход)
cv.addEventListener('wheel', e => {
  e.preventDefault(); setCamMode('free');
  const r = cv.getBoundingClientRect(), mx = e.clientX - r.left, my = e.clientY - r.top;
  const wx = cam.fx + (mx - cvW() / 2) / cam.fcell, wy = cam.fy + (my - cvH() / 2) / cam.fcell;
  cam.fcell = Math.max(2, Math.min(80, cam.fcell * (e.deltaY < 0 ? 1.15 : 1 / 1.15)));
  cam.fx = wx - (mx - cvW() / 2) / cam.fcell; cam.fy = wy - (my - cvH() / 2) / cam.fcell;
}, {passive: false});
let panFrom = null;
cv.addEventListener('mousedown', e => { if (e.button === 2 || e.shiftKey){ setCamMode('free'); panFrom = {x: e.clientX, y: e.clientY, fx: cam.fx, fy: cam.fy}; e.preventDefault(); } });
addEventListener('mousemove', e => { if (panFrom){ cam.fx = panFrom.fx - (e.clientX - panFrom.x) / cam.fcell; cam.fy = panFrom.fy - (e.clientY - panFrom.y) / cam.fcell; } });
addEventListener('mouseup', () => { if (panFrom) setTimeout(() => { panFrom = null; }, 0); });
cv.addEventListener('contextmenu', e => e.preventDefault());
// во весь экран: настоящий полный экран, а если браузер не даёт (встроенные окна) — блок поля на всё окно
function setBig(on){ $('stage').parentElement.classList.toggle('bigfield', on); fitStage(); $('fullBtn').textContent = on || document.fullscreenElement ? '⛶ Свернуть' : '⛶ Во весь экран'; }
$('fullBtn').onclick = () => {
  const box = $('stage').parentElement;
  if (document.fullscreenElement) document.exitFullscreen();
  else if (box.classList.contains('bigfield')) setBig(false);
  else box.requestFullscreen().catch(() => setBig(true));
};
document.addEventListener('fullscreenchange', () => setBig(false));
addEventListener('keydown', e => { if (e.key === 'Escape' && $('stage').parentElement.classList.contains('bigfield')) setBig(false); });
cv.addEventListener('mouseleave', () => { hover = null; });
// подсветка: дальность хода, допустимые цели, клетка под курсором
// туман войны: маска видимых клеток из последнего запроса хода (бит на клетку, base64) -> холст 1 пиксель = 1 клетка
let fogKey = '', fogC = null;
function fogCanvas(){
  const q = G.req; if (!q || !q.vis || !$('fogOn').checked || G.over) return null;
  if (q.vis !== fogKey){
    fogKey = q.vis; const W = q.visW, H = q.visH, bin = atob(q.vis);
    fogC = document.createElement('canvas'); fogC.width = W; fogC.height = H;
    const c = fogC.getContext('2d'), img = c.createImageData(W, H);
    for (let k = 0; k < W * H; k++){ const seen = (bin.charCodeAt(k >> 3) >> (k & 7)) & 1; img.data[4 * k + 3] = seen ? 0 : 255; }
    c.putImageData(img, 0, 0);
  }
  return fogC;
}
function overlayDraw(now){
  if (!G.req || G.busy || G.over || !FR.length) return;
  const q = G.req, fr = FR[curIdx], me = fr.f[q.me], cell = cam.cell;
  const cx = PX(me.x + (me.n || 1) / 2), cy = PY(me.y + (me.n || 1) / 2);
  if (G.mode === 'move' && q.speedLeft >= 5){
    ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, q.speedLeft / 5 * cell, 0, 7); ctx.fillStyle = 'rgba(95,208,138,.10)'; ctx.fill(); ctx.strokeStyle = 'rgba(95,208,138,.65)'; ctx.lineWidth = 2; ctx.setLineDash([8, 6]); ctx.stroke(); ctx.restore();
  }
  let range = 0;
  if (G.mode === 'attack') range = q.weapon.range; else if (G.mode.startsWith('attack:')){ const a = (q.alts || []).find(x => x.k === +G.mode.slice(7)); range = a ? a.range : 5; } else if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; range = a.self ? (a.ally ? a.range : 0) : a.range; }
  if (range){
    ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, (range / 5 + ((me.n || 1) - 1) / 2) * cell, 0, 7); ctx.strokeStyle = 'rgba(255,214,90,.7)'; ctx.lineWidth = 2; ctx.setLineDash([4, 6]); ctx.stroke(); ctx.restore();
    const ga = G.mode.startsWith('spell:') ? q.abils[+G.mode.slice(6)] : null;
    if (!(ga && ga.ground)) q.enemies.forEach(en => {
      const f = fr.f[en.i]; if (!f || f.hp <= 0) return; const n = f.n || 1, ok = en.dist <= range && en.los && !en.charm, r = Math.max(f.kind < 2 ? RB() : RM(), n > 1 ? n * cell * .42 : 0) + 8 + Math.sin(now / 160) * 2;
      ctx.save(); ctx.beginPath(); ctx.arc(PX(f.x + n / 2), PY(f.y + n / 2), r, 0, 7); ctx.strokeStyle = ok ? '#ff5d5d' : 'rgba(180,180,190,.6)'; ctx.lineWidth = ok ? 4 : 2; ctx.stroke(); ctx.restore();
    });
  }
  const ga2 = G.mode.startsWith('spell:') ? q.abils[+G.mode.slice(6)] : null;
  if (ga2 && ga2.ground && hover && hover.x >= 0 && hover.y >= 0 && hover.x < AW && hover.y < AH){   // превью: область и клетки барьера
    const hx = PX(hover.x + .5), hy = PY(hover.y + .5);
    if (ga2.radius){ ctx.save(); ctx.beginPath(); ctx.arc(hx, hy, ga2.radius / 5 * cell, 0, 7); ctx.fillStyle = 'rgba(255,120,80,.16)'; ctx.fill(); ctx.strokeStyle = 'rgba(255,140,90,.8)'; ctx.lineWidth = 2; ctx.stroke(); ctx.restore(); }
    if (ga2.wallLen){
      const dx = hover.x - me.x, dy = hover.y - me.y, ax = Math.abs(dx), ay = Math.abs(dy), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
      let px = 1, py = 0; if (ax === 0 && ay === 0){ px = 1; py = 0; } else if (ax >= 2 * ay){ px = 0; py = 1; } else if (ay >= 2 * ax){ px = 1; py = 0; } else { px = -sy; py = sx; }
      ctx.save(); ctx.fillStyle = 'rgba(190,160,255,.55)'; ctx.strokeStyle = '#d6c4ff'; ctx.lineWidth = 2;
      for (let i = -Math.floor((ga2.wallLen - 1) / 2); i <= Math.floor(ga2.wallLen / 2); i++){ const x = hover.x + px * i, y = hover.y + py * i; if (x < 0 || y < 0 || x >= AW || y >= AH) continue; ctx.fillRect(PX(x), PY(y), cell, cell); ctx.strokeRect(PX(x) + 1, PY(y) + 1, cell - 2, cell - 2); }
      ctx.restore();
    }
  }
  if (hover && hover.x >= 0 && hover.y >= 0 && hover.x < AW && hover.y < AH){
    ctx.save(); ctx.strokeStyle = 'rgba(255,255,255,.9)'; ctx.lineWidth = 2; ctx.strokeRect(PX(hover.x), PY(hover.y), cell, cell);
    const d = 5 * Math.max(Math.abs(hover.x - me.x), Math.abs(hover.y - me.y)); ctx.font = '700 13px system-ui'; ctx.fillStyle = '#fff'; ctx.strokeStyle = 'rgba(0,0,0,.8)'; ctx.lineWidth = 4; ctx.textAlign = 'left';
    ctx.strokeText(d + ' фт', PX(hover.x) + cell + 4, PY(hover.y) + 14); ctx.fillText(d + ' фт', PX(hover.x) + cell + 4, PY(hover.y) + 14); ctx.restore();
  }
}
addEventListener('keydown', e => {
  if (!viewActive() || G.busy || !G.req || e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  if (e.key === 'e' || e.key === 'E' || e.key === 'Enter'){ cmd({c:'end'}); }
  else if (e.key === 'm' || e.key === 'M'){ G.mode = 'move'; renderTurn(); }
  else if (e.key === 'a' || e.key === 'A'){ const b = document.querySelector('#turn [data-m=attack]'); if (b && !b.disabled) b.click(); }
  else if (/^[1-9]$/.test(e.key)){ const b = document.querySelector(`#turn [data-spell="${+e.key - 1}"]`); if (b && !b.disabled) b.click(); }
});
addEventListener('resize', () => { if (G.active) fitPlay(); });
// карта по ширине колонки, но не выше окна минус панель хода (панель хода всегда на виду)
let lastTurnH = 0;
function fitPlay(){
  fitStage();
  lastTurnH = $('turn').offsetHeight;
  const H = Math.max(300, innerHeight - lastTurnH - 120);
  if (cv.height / dpr > H){ cv.height = Math.round(H * dpr); cv.style.height = H + 'px'; }
  cam.snap = true;
}

$('btnFight').onclick = startGame;
$('tempo').onchange = () => { speed = +$('tempo').value; }; speed = +$('tempo').value;
