// ---- панель хода ----
const REQ = () => G.req;
function myPos(){ const f = FR[curIdx].f[G.req ? G.req.me : G.me]; return f; }   // ходящий: свой герой или свой миньон
function renderTurn(){
  const q = G.req, el = $('turn'); if (!q) return;
  const meNow = FR[curIdx] && FR[curIdx].f && FR[curIdx].f[q.me];
  const atkOk = q.weapon.armed && (q.actionFree || q.weapon.atkLeft > 0);
  if (G.mode === 'attack' && !atkOk) G.mode = 'move';
  if (G.mode.startsWith('attack:')){ const a = (q.alts || []).find(x => x.k === +G.mode.slice(7)); if (!a || !(a.left > 0 || q.actionFree)) G.mode = 'move'; }
  if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; if (!a || !a.avail) G.mode = 'move'; }
  const mv = q.speed0 ? Math.min(100, 100 * q.speedLeft / Math.max(q.speed0, q.speedLeft)) : 0;
  const meNow2 = meNow;
  let h = `<div class="tbar"><span>Движение <b class="tnum">${ftToText(q.speedLeft)}</b></span>${meNow2 ? `<span class="muted">ХП ${meNow2.hp}/${meNow2.maxHp} · КД ${meNow2.ac}</span>` : ''}
    <span class="act-lbl ${q.actionFree ? 'free' : ''}"><span class="dot ${q.actionFree ? 'free' : ''}"></span>действие</span><span class="act-lbl ${q.bonusFree ? 'free b' : ''}"><span class="dot ${q.bonusFree ? 'free b' : ''}"></span>малое</span><span class="act-lbl"><span class="dot ${q.reactionFree ? 'free' : ''}"></span>реакция</span>${slotsLine(q.slots)}
    <button class="primary endb" data-c="end" title="Клавиша E или Enter">Завершить ход</button><button class="danger" data-leave="1">${G.room ? 'Выйти' : 'Сдаться'}</button></div>`;
  roundBar(q);
  h += `<div class="acts">`;   // что сделал соперник — в журнале боя (список «Пока вы ждали» убран по просьбе владельца 2026-11-16)
  h += `<button data-m="move" class="sp ${G.mode === 'move' ? 'sel' : ''}" title="Клавиша M; клик по клетке — идти, по врагу в досягаемости — удар"><span>Идти</span><small>клик по полю</small></button>`;
  const wTag = !q.weapon.armed ? 'безоружны' : (q.weapon.atkLeft > 0 ? `ещё атак: ${q.weapon.atkLeft} (можно идти и сменить цель)` : `${q.weapon.attacks > 1 ? q.weapon.attacks + ' атаки по одной' : '1 атака'}, ${ftToText(q.weapon.range)}`);
  const attackWhy = !q.weapon.armed ? 'Недоступно: у персонажа нет оружия.' : (!q.actionFree && q.weapon.atkLeft <= 0 ? 'Недоступно: действие уже потрачено.' : (q.enemies.every(en => en.dist > q.weapon.range) ? `Пока нет цели в радиусе ${q.weapon.range} фт. Подойдите ближе.` : 'Клавиша A; затем выберите подсвеченную цель.'));
  h += `<button data-m="attack" class="sp ${G.mode === 'attack' ? 'sel' : ''}" ${atkOk ? '' : 'disabled'} title="${attackWhy}"><span>Оружие${q.weapon.armed ? ' · ' + (q.weapon.atkLeft > 0 ? q.weapon.atkLeft : q.weapon.attacks) + ' ' + pluralRu(q.weapon.atkLeft > 0 ? q.weapon.atkLeft : q.weapon.attacks, 'атака', 'атаки', 'атак') : ''}</span><small>${esc(wTag)}</small></button>`;
  // сколько атак по выбранной цели сразу: из доступных в этом действии (остальные — потом, по другим целям)
  const atkMax = G.mode === 'attack' ? (q.weapon.atkLeft > 0 ? q.weapon.atkLeft : q.weapon.attacks)
    : (G.mode.startsWith('attack:') ? (a => a ? (a.left > 0 ? a.left : a.att) : 1)((q.alts || []).find(x => x.k === +G.mode.slice(7))) : 1);
  // поле «атак по цели» — в конце ряда (ниже): появится при выборе оружия и не сдвинет кнопки под мышью
  const atkNh = atkMax > 1 ? (G.atkN = Math.min(Math.max(1, G.atkN || 1), atkMax), `<label class="small" title="Сколько атак по цели одним щелчком">атак по цели <input type="number" id="atkN" min="1" max="${atkMax}" value="${G.atkN}" style="width:56px"> из ${atkMax}</label>`) : '';
  if (q.abils.some(a => !a.react)) h += `<label class="small">Применений способности <input id="castCount" type="number" min="1" max="20" value="${G.castCount || 1}" style="width:56px"></label>`;
  q.abils.forEach(a => {
    const sp = a.title ? a : BOSS[G.me].abil[a.k];
    const tag = [a.react ? 'реакция, сама' : (a.bonus ? 'бонус' : 'действие'), a.self ? (a.tele ? 'прыжок ' + a.tele + ' фт' : 'на себя') : (a.ground ? 'по клетке, ' : '') + a.range + ' фт' + (a.radius ? ', радиус ' + a.radius : '') + (a.wallLen ? ', барьер ' + a.wallLen + ' кл.' : ''), a.slot ? 'ячейка ' + a.slot + ' ур.' : '', a.uses >= 0 ? 'осталось ' + a.uses : ''].filter(Boolean).join(' · ');
    h += `<button class="sp ${G.mode === 'spell:' + a.k ? 'sel' : ''}" data-spell="${a.k}" ${a.avail ? '' : 'disabled'} title="${esc(a.why || ((S.abilities[a.k] || {})._desc ? S.abilities[a.k]._desc + ' — ' : '') + abilDesc(sp.desc))} (клавиша ${a.k + 1})"><span>✨ ${esc(sp.title)}${a.slot ? ` <i class="slot-tag" title="Тратит ячейку ${a.slot} ур. или выше">${a.slot}</i>` : ''}</span><small>${esc(tag)}</small></button>`;
  });
  if (q.alts && q.alts.length){
    // оружие мультиатаки: удары в том же действии «Атака», что и основное (укус + когти)
    const started = q.weapon.atkLeft > 0 || q.alts.some(a => a.left > 0);
    h += q.alts.filter(a => a.multi).map(a => { const ok = a.left > 0 || (!started && q.actionFree); return `<button data-m="attack:${a.k}" class="sp ${G.mode === 'attack:' + a.k ? 'sel' : ''}" ${ok ? '' : 'disabled'} title="Удар этим оружием в действии «Атака»; затем клик по врагу"><span>⚔ ${esc(wpnNames()[a.k + 1] || 'Оружие')}</span><small>${a.left > 0 ? 'ещё ударов: ' + a.left : esc(a.t)}</small></button>`; }).join('');
    // запасное оружие: 1-я смена за ход бесплатно, 2-я — бонусным, 3-я — действием
    const can = q.swaps === 0 || (q.swaps === 1 && q.bonusFree) || (q.swaps === 2 && q.actionFree), cost = ['бесплатно', 'бонусное действие', 'действие'][q.swaps] || 'нельзя';
    h += q.alts.filter(a => !a.multi).map(a => `<button class="sp" data-swap="${a.k}" ${can ? '' : 'disabled'} title="Сменить оружие (${cost})"><span>🔁 ${esc(wpnNames()[a.k + 1] || 'Запасное')}</span><small>${esc(a.t)}</small></button>`).join('');
  }
  h += `

    ${(q.illusions || []).map(([i,x,y]) => `<button data-study="${i}" ${q.actionFree ? '' : 'disabled'} title="Действие: проверка Анализа против СЛ заклинателя">Изучить образ (${x}, ${y})</button>`).join('')}
    ${q.vt ? '<button data-climb="1">Лезть вверх</button><button data-climb="-1">Спуститься</button>' : ''}
    <button data-m="jump" ${q.speedLeft < 5 ? 'disabled' : ''}>Прыгнуть</button>
    <button data-c="dash" ${q.actionFree ? '' : 'disabled'} title="Действие: движения вдвое больше">Рывок</button>
    <button data-c="dodge" ${q.actionFree ? '' : 'disabled'} title="Действие: атаки по вам с помехой до вашего хода">Уклонение</button>
    <button data-c="disengage" ${q.actionFree ? '' : 'disabled'} title="Действие: уход без провоцированных атак">Отход</button>
    <button data-c="help" ${q.actionFree && q.enemies.some(en => en.dist <= 5) ? '' : 'disabled'} title="${q.actionFree ? 'Недоступно: подойдите к врагу на 5 фт.' : 'Недоступно: действие уже потрачено.'} Помощь даёт союзнику преимущество на первую атаку по врагу рядом.">🤝 Помощь</button>
    <button data-c="ready" ${q.actionFree && (q.weapon.armed || q.abils.some(a => a.avail && !a.react)) ? '' : 'disabled'} title="Выбрать действие и условие во всплывающем окне">Подготовка</button>
    ${q.surgeLeft > 0 ? `<button data-c="surge" ${!q.actionFree ? '' : 'disabled'}>Всплеск (${q.surgeLeft})</button>` : ''}
    ${q.conc >= 0 ? `<button data-c="dropconc" title="Бесплатно: эффекты концентрации снимаются">Снять концентрацию</button>` : ''}
    <button data-c="toward" title="Идти к ближайшему врагу">К врагу</button>
    ${(q.doors || []).map(d => `<button data-door="${d[0]},${d[1]}" title="Взаимодействие: первое за ход бесплатно, дальше — действием">🚪 ${d[2] === 1 ? 'Открыть' : 'Закрыть'} дверь</button>`).join('')}
    ${q.fly ? `<button data-alt="1" ${q.fly.can && q.fly.alt < q.fly.max && q.speedLeft >= 5 ? '' : 'disabled'} title="Выше на 5 фт (5 фт движения); уход вверх из досягаемости ближника провоцирует атаку">🪽 Выше</button><button data-alt="-1" ${q.fly.alt > 0 && q.speedLeft >= 5 ? '' : 'disabled'} title="Ниже на 5 фт (5 фт движения)">⬇ Ниже</button><span class="tag" title="Высота полёта над землёй">${q.fly.alt ? 'в воздухе ' + q.fly.alt * 5 + ' фт' : 'на земле'}</span>` : ''}${q.vfly ? `<span class="tag" title="Летун в 3D: клавиши R и F поднимают и опускают точку хода над клеткой под мышью (по 5 фт)">🪽 R / F — высота хода</span>` : ''}${atkNh}</div>
    </div><div class="msg" id="tmsg"></div><div class="hint">${modeHint()}</div>`;
  el.innerHTML = h;
  const count = $('castCount');
  if (count) count.oninput = () => { G.castCount = Math.min(20, Math.max(1, Math.floor(+count.value || 1))); };
  const actionHelp = {
    move:'Переместиться на клетку в пределах оставшейся скорости', attack:'Атаковать выбранную цель оружием персонажа',
    dash:'Удвоить скорость на этот ход', dodge:'Атаки по вам идут с помехой до начала вашего следующего хода',
    disengage:'Переместиться, не давая врагу атаки при уходе', help:'Помочь союзнику: его следующая атака по цели идёт с преимуществом',
    ready:'Подготовить действие, которое сработает в ответ на событие', toward:'Переместиться к ближайшему врагу',
    end:'Закончить ваш ход (клавиша E или Enter)', leave:'Прервать бой и вернуться к настройкам'
  };
  el.querySelectorAll('button[data-m],button[data-c],button[data-leave]').forEach(b => {
    const key = b.dataset.m || b.dataset.c || 'leave', explanation = actionHelp[key];
    if (!explanation) return;
    if (!b.title) b.title = explanation;
    b.setAttribute('aria-label', `${b.textContent.trim()}: ${b.title}`);
  });
}
// ---- таймер хода (реальное время): по истечении ход заканчивается сам ----
let turnTimer = null;
// время на ход — необязательное правило из настроек; только одиночный бой (в комнате время держит сервер)
function startTurnTimer(){
  clearInterval(turnTimer);
  const tt = +$('turnTime').value; if (!tt || G.over || G.room) return;
  const t0 = performance.now();
  const tick = () => {
    const left = Math.ceil(tt - (performance.now() - t0) / 1000), el = $('timer');
    if (G.busy || !G.req || G.over || !G.active){ clearInterval(turnTimer); return; }
    if (el){ el.textContent = Math.max(0, left) + ' с'; el.classList.toggle('low', left <= 5); }
    if (left <= 0){
      clearInterval(turnTimer); toast('Время хода вышло: ход передан сопернику.');
      const host = $('log'); host.insertAdjacentHTML('beforeend', `<div class="ln mut"><span class="ev bad">⏱ Время хода (${tt} с) вышло — ход завершён автоматически</span></div>`); host.scrollTop = host.scrollHeight;
      cmd({c:'end'});
    }
  };
  tick(); turnTimer = setInterval(tick, 250);
}
// ячейки заклинаний: по уровням — сколько осталось из купленных
function slotsLine(sl){
  if (!sl || !sl.length) return '';
  return `<span class="slots" title="Ячейки заклинаний: осталось из купленных">Ячейки ${sl.map(([L, left, all]) => `<b>${L}</b>&nbsp;${all <= 6 ? '<i class="pip on"></i>'.repeat(left) + '<i class="pip"></i>'.repeat(all - left) : left + '/' + all}`).join(' ')}</span>`;
}
// полоса раунда сверху: порядок ходов по инициативе (живые), ходящий выделен; справа — время хода, если включено
function roundBar(q){
  const ini = (FR[0] && (FR[0].ev || []).find(e => e.k === 'init')) || (FR[1] && (FR[1].ev || []).find(e => e.k === 'init'));
  const fl = FR[FR.length - 1].f;
  const order = ini ? ini.order.filter(([k]) => fl[k] && fl[k].hp > 0).map(([k, v]) => k === q.me ? `<b>${nameOf(k, fl)} (${v})</b>` : `${nameOf(k, fl)} (${v})`).join(' → ') : '';
  const tt = !G.room && +$('turnTime').value;
  $('roundBar').innerHTML = `<b>Раунд ${q.round}</b><span class="order" title="Порядок ходов">${order}</span>${tt ? `<span class="timer" id="timer">${tt} с</span>` : ''}`;
}
window.openReadyDialog = function(q){
  const opts = []; if (q.weapon && q.weapon.armed) opts.push([-1, 'Удар оружием']); (q.abils || []).filter(a => !a.react).forEach(a => opts.push([a.k, a.title]));
  const m = modal('Подготовка', `<label class="field">Что сделать реакцией<select id="readyK">${opts.map(([v, n]) => `<option value="${v}">${esc(n)}</option>`).join('')}</select></label>
    <label class="field">Условие<select id="readyT"><option value="1">Когда враг войдёт в досягаемость</option><option value="2">Когда враг применит способность</option></select></label>`,
    `<button data-close>Отмена</button><button class="primary" id="readyOk">Подготовить</button>`);
  m.q('#readyOk').onclick = () => { const k = +m.q('#readyK').value, trig = +m.q('#readyT').value; m.close(); cmd({c:'ready', k, trig}); };
};
function modeHint(){
  const q = G.req;
  if (G.mode === 'jump') return 'Выберите клетку приземления. Дальность ограничена Силой и оставшимся движением.';
  if (G.mode === 'move') return 'Режим «идти»: щёлкните клетку на поле (движение тратит скорость; труднопроходимая местность и подъём дороже; уход из досягаемости ближнего врага даёт ему провоцированную атаку, кроме действия «Отход»).';
  if (G.mode.startsWith('attack:')) return 'Щёлкните врага — удар этим оружием в действии «Атака» (мультиатака). Удары основным оружием — кнопкой «Оружие».';
  if (G.mode === 'attack') return 'Щёлкните врага — одна атака (красные кольца — цели в дальности и в прямой видимости). Остальные атаки действия — следующими щелчками; между ними можно идти. Щелчок по стене, дереву или укрытию — удар по объекту (у объектов есть КД и хиты).';
  if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; return a && a.ground ? 'Щёлкните клетку на поле: там встанет барьер или сработает область (превью — по курсору).' : (a && a.self && a.tele ? 'Щёлкните клетку, куда прыгнуть.' : (a && a.self && a.ally ? `Щёлкните себя или союзника (до ${a.range} фт).` : 'Щёлкните врага — цель способности.')); }
  return '';
}
$('turn').addEventListener('click', e => {
  const b = e.target.closest('button'); if (b && b.dataset.leave){ leaveFight(); return; }
  if (!b || G.busy || !G.req) return;
  if (b.dataset.study != null){ cmd({c:'study', i:+b.dataset.study}); return; }
  if (b.dataset.climb){
    const f = myPos(), z = f.z + Number(b.dataset.climb);
    if (z < 1 || G.req.speedLeft < 5){ toast('Недостаточно движения или достигнуто дно карты.'); return; }
    cmd({c:'move',x:f.x,y:f.y,z}); return;
  }
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
  if (b.dataset.c === 'ready'){ window.openReadyDialog?.(G.req); return; }
  if (b.dataset.c) cmd({c: b.dataset.c});
});
// Оружие по кнопке: если единственная цель в дальности — бьём сразу
$('turn').addEventListener('dblclick', () => {});
$('turn').addEventListener('input', e => { if (e.target.id === 'atkN') G.atkN = Math.max(1, +e.target.value || 1); });   // атак по цели
$('turn').addEventListener('change', e => { if (e.target.id === 'readyK') G.readyK = +e.target.value; if (e.target.id === 'readyT') G.readyT = +e.target.value; });
function weaponClick(){ const q = G.req; const ok = q.enemies.filter(en => !en.charm && en.dist <= q.weapon.range && en.los); if (ok.length === 1) cmd({c:'attack', tgt: ok[0].i, reckless: q.weapon.reckless}); else { G.mode = 'attack'; renderTurn(); } }

// ---- клики по полю ----
function cellAt(e){
  if (R3.on) return R3.cellAt(e);   // 3D-вид: луч по кубам (может не попасть — тогда null)
  const r = cv.getBoundingClientRect(), px = (e.clientX - r.left) * (cvW() / r.width), py = (e.clientY - r.top) * (cvH() / r.height);
  return {x: Math.floor((px - camCX()) / cam.cell + cam.x), y: Math.floor((py - camCY()) / cam.cell + cam.y)};
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
  if (R3.on && (!R3.hit || R3.hit.k == null)) return null;
  const fr = FR[curIdx]; let best = null, bd = 1e9;
  const gap = (a, n, b) => Math.max(0, b - (a + n - 1), a - b);   // клетка — до квадрата крупного (n×n от x, y)
  fr.f.forEach((f, k) => { if (f.hp <= 0 || f.hid || f.team === fr.f[G.me].team || f.fled) return; const n = f.n || 1, d = Math.max(gap(f.x, n, c.x), gap(f.y, n, c.y)); if (d <= 0 && d < bd){ bd = d; best = k; } });
  if (best == null) fr.f.forEach((f, k) => { if (f.hp <= 0 || f.hid || f.team === fr.f[G.me].team || f.fled) return; const n = f.n || 1, d = Math.hypot(f.x + (n - 1) / 2 - c.x, f.y + (n - 1) / 2 - c.y) - (n - 1) / 2; if (d < 1.6 && d < bd){ bd = d; best = k; } });
  return best;
}
let lookingAtCell = false;
$('lookAtCell').onclick = () => {
  lookingAtCell = !lookingAtCell;
  $('lookAtCell').setAttribute('aria-pressed', String(lookingAtCell));
  if (lookingAtCell) toast('Выберите клетку, чтобы повернуть голову.');
};
cv.addEventListener('click', e => {
  if (lookingAtCell && !R3.on){
    lookingAtCell = false;
    $('lookAtCell').setAttribute('aria-pressed', 'false');
    const c = cellAt(e), me = myPos();
    if (c && me && (c.x !== me.x || c.y !== me.y)){
      R3.view.yaw = Math.atan2(me.x-c.x, me.y-c.y);
      R3.view.fp = 0;
    }
    return;
  }
  if (e.shiftKey) return;   // Shift + перетаскивание — сдвиг обзора, не ход
  if (G.busy || !G.req || G.over || (R3.on && R3.dragged)) return;   // в 3D перетаскивание — поворот камеры, не ход
  const c = cellAt(e), q = G.req; if (!c) return;
  if (G.mode === 'jump'){ cmd({c:'jump', x:c.x, y:c.y, z:c.z}); return; }
  if (G.mode === 'move'){ const en = enemyAt(c); if (en != null && (q.actionFree || q.weapon.atkLeft > 0) && q.weapon.armed){ const E = q.enemies.find(x => x.i === en); if (E && E.dist <= q.weapon.range && E.los){ cmd({c:'attack', tgt: en, reckless: q.weapon.reckless}); return; } }
    const n = (FR[curIdx].f[q.me] || {}).n || 1, off = Math.floor((n - 1) / 2);
    const mv = {c:'move', x: Math.max(0, Math.min(AW - n, c.x - off)), y: Math.max(0, Math.min(AH - n, c.y - off))};
    if (c.z != null) mv.z = c.z;   // режим кубов: высота, куда щёлкнули
    cmd(mv); return; }
  if (G.mode.startsWith('attack:')){   // удар оружием мультиатаки
    const en = enemyAt(c); if (en == null){ toast('Щёлкните по врагу.'); return; }
    const a = (q.alts || []).find(x => x.k === +G.mode.slice(7)); if (!targetOk(en, a ? a.range : 5)) return;
    cmd({c:'attack', tgt: en, wk: +G.mode.slice(7), n: G.atkN || 1}); return;
  }
  if (G.mode === 'attack'){
    const en = enemyAt(c);
    if (en == null){ if (c.oz != null) cmd({c:'hitobj', x:c.x, y:c.y, z:c.oz}); else if (isObjCell(c.x, c.y)) cmd({c:'hitobj', x:c.x, y:c.y}); else toast('Щёлкните по врагу или по объекту (стена, дерево, укрытие).'); return; }
    if (!targetOk(en, q.weapon.range)) return;
    cmd({c:'attack', tgt: en, reckless: q.weapon.reckless, n: G.atkN || 1}); return;
  }
  if (G.mode.startsWith('spell:')){
    const k = +G.mode.slice(6), a = q.abils[k];
    if ((a.self && a.tele) || a.ground){ cmd(Object.assign({c:'spell', k, x:c.x, y:c.y}, c.z != null && a.ground ? {z: c.z} : {})); return; }   // режим кубов: центр области — куб, по верху которого щёлкнули
    if (a.self && a.ally){ const al = allyAt(c); if (al == null){ toast('Щёлкните себя или союзника.'); return; } cmd({c:'spell', k, tgt: al}); return; }
    const en = enemyAt(c); if (en == null){ toast('Щёлкните по врагу.'); return; }
    if (!targetOk(en, a.range, a.targetType)) return;
    cmd({c:'spell', k, tgt: en});
  }
});
// цель в дальности и в прямой видимости (как красные кольца на поле); иначе — объяснение, ход не тратится
function targetOk(en, range, type = -1){
  const E = G.req.enemies.find(x => x.i === en); if (!E) return true;   // нет сведений — решает движок
  if (type >= 0 && type !== E.ctype){ toast("Способность действует на другой тип существа."); return false; }
  if (E.charm){ toast('По этой цели нельзя: вы очарованы ею.', 4500, true); return false; }
  if (E.dist > range){ toast(`Цель далеко: ${E.dist} фт, а дальность — ${range} фт. Подойдите ближе (кнопка «К врагу» или щелчок по полю в режиме «Идти»).`, 5000, true); return false; }
  if (!E.los){ toast('Цель не в прямой видимости: её закрывает стена или укрытие. Обойдите препятствие.', 5000, true); return false; }
  return true;
}
// объект в клетке (стена, дерево, укрытие) и не разрушен (обломки — в изменениях арены, вид 4)
function isObjCell(x, y){
  if (x < 0 || y < 0 || x >= AW || y >= AH) return false;
  const fr = FR[curIdx]; if (fr.dyn && fr.dyn.some(d => d[0] === x && d[1] === y && d[2] === 4)) return false;
  return ['wall', 'tree', 'climb'].some(k => ch(MAP[k], x, y));
}
let hover = null;
cv.addEventListener('mousemove', e => { hover = cellAt(e); });
// свободный обзор: колесо — масштаб к курсору, правая кнопка или Shift + перетаскивание — сдвиг (обычный щелчок — ход)
cv.addEventListener('wheel', e => {
  if (R3.on) return;   // 3D-вид приближает сам
  e.preventDefault(); setCamMode('free');
  const r = cv.getBoundingClientRect(), mx = e.clientX - r.left, my = e.clientY - r.top;
  const wx = cam.fx + (mx - camCX()) / cam.fcell, wy = cam.fy + (my - camCY()) / cam.fcell;
  cam.fcell = Math.max(2, Math.min(80, cam.fcell * (e.deltaY < 0 ? 1.15 : 1 / 1.15)));
  cam.fx = wx - (mx - camCX()) / cam.fcell; cam.fy = wy - (my - camCY()) / cam.fcell;
}, {passive: false});
let panFrom = null;
cv.addEventListener('mousedown', e => { if (R3.on) return; if (e.button === 2 || e.shiftKey){ setCamMode('free'); panFrom = {x: e.clientX, y: e.clientY, fx: cam.fx, fy: cam.fy}; e.preventDefault(); } });
addEventListener('mousemove', e => { if (panFrom){ cam.fx = panFrom.fx - (e.clientX - panFrom.x) / cam.fcell; cam.fy = panFrom.fy - (e.clientY - panFrom.y) / cam.fcell; } });
addEventListener('mouseup', () => { if (panFrom) setTimeout(() => { panFrom = null; }, 0); });
cv.addEventListener('contextmenu', e => e.preventDefault());
// во весь экран: настоящий полный экран, а если браузер не даёт (встроенные окна) — блок поля на всё окно
function setBig(on){ $('stage').parentElement.classList.toggle('bigfield', on); fitStage(); $('fullBtn').textContent = on || document.fullscreenElement ? 'Свернуть' : 'Во весь экран'; }
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
  const frame = FR[curIdx];
  const q = G.req && curIdx === FR.length-1 ? G.req
    : frame && frame.vis ? {vis:frame.vis,visW:AW,visH:AH} : G.lastReq;
  if (!q || !q.vis || G.over) return null;   // в бою туман не отключается
  const me = frame && frame.f[G.req ? G.req.me : G.me];
  if (!me || me.hid) return null;
  const key = [q.vis,q.visW,q.visH,me.x,me.y,me.z,me.n].join('|');   // туман не зависит от камеры: открыто всё, что видит боец по правилам (во все стороны)
  if (key !== fogKey){
    fogKey = key;
    const W = q.visW, H = q.visH, bin = atob(q.vis);
    fogC = document.createElement('canvas'); fogC.width = W; fogC.height = H;
    const c = fogC.getContext('2d'), img = c.createImageData(W, H);
    const half = (me.n || 1)/2;
    for (let y=0; y<H; y++) for (let x=0; x<W; x++){
      const k = y*W+x;
      const own = x>=me.x && x<me.x+2*half && y>=me.y && y<me.y+2*half;
      const seen = (bin.charCodeAt(k>>3) >> (k&7)) & 1;
      img.data[4*k+3] = own || seen ? 0 : 255;
    }
    c.putImageData(img, 0, 0);
  }
  return fogC;
}
// путь под мышью: дерево путей игрока из запроса хода (pt — по 3 символа base64 на узел: предок и футы / 5; узлы земли,
// затем настила). По этому же дереву движок ведёт ход игрока — что показано, то и будет пройдено
const B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
let ptKey = '', ptPar = null, ptFt = null;
// режим кубов: дерево путей движка q.vt — [x, y, z, предок, футы] по узлам в порядке поиска; без высоты — первый узел клетки
// (туда же придёт ход движка), с высотой щелчка (z) — именно этот куб
function pathPreviewVox(q, x, y, z){
  const t = q.vt; let k = -1;
  for (let i = 0; i < t.length; i += 5) if (t[i] === x && t[i + 1] === y && (z == null || t[i + 2] === z)){ k = i / 5; break; }
  if (k <= 0) return null;
  const cells = []; for (let c = k; c > 0 && cells.length <= t.length / 5; c = t[5 * c + 3]) cells.push({x: t[5 * c], y: t[5 * c + 1], z: t[5 * c + 2], ft: t[5 * c + 4]});
  return {cells: cells.reverse(), ft: t[5 * k + 4]};
}
function pathPreview(q, x, y, z){
  if (q && q.vt) return pathPreviewVox(q, x, y, z);
  const N = AW * AH;
  if (!q || !q.pt || x < 0 || y < 0 || x >= AW || y >= AH || q.pt.length !== 3 * N * (q.ptL || 1)) return null;
  if (q.pt !== ptKey){
    ptKey = q.pt; const M = N * (q.ptL || 1); ptPar = new Int32Array(2 * N).fill(-1); ptFt = new Int32Array(2 * N).fill(-1);
    for (let k = 0; k < M; k++){
      const c = B64.indexOf(q.pt[3 * k]), ft = 5 * (64 * B64.indexOf(q.pt[3 * k + 1]) + B64.indexOf(q.pt[3 * k + 2]));
      if (c <= 0) continue;
      const i = (c - 1) % 8, d = i < 4 ? i : i + 1, kx = k % N % AW, ky = Math.floor(k % N / AW);
      ptPar[k] = (ky + Math.floor(d / 3) - 1) * AW + kx + d % 3 - 1 + (c > 8 ? N : 0); ptFt[k] = ft;
    }
  }
  const k0 = y * AW + x, k = ptFt[k0 + N] >= 0 && (ptFt[k0] < 0 || ptFt[k0 + N] < ptFt[k0]) ? k0 + N : k0;   // как в движке (pathTo)
  if (ptFt[k] < 0) return null;
  const cells = []; for (let c = k; ptPar[c] >= 0 && cells.length <= 2 * N; c = ptPar[c]) cells.push({x: c % N % AW, y: Math.floor(c % N / AW), up: c >= N ? 1 : 0, ft: ptFt[c]});
  return {cells: cells.reverse(), ft: ptFt[k]};
}
// подпись пути: длина и сколько пройдёт за этот ход
const pathLabel = (p, left) => p.ft <= left ? p.ft + ' фт' : `${p.ft} фт · за ход ${Math.max(0, ...p.cells.filter(c => c.ft <= left).map(c => c.ft))} фт`;
// линия пути по точкам экрана: пройдёт за ход — зелёная, дальше — пунктир
function pathStroke(pts, cells, left){
  let cut = 0; while (cut < cells.length && cells[cut].ft <= left) cut++;
  ctx.save(); ctx.lineCap = ctx.lineJoin = 'round';
  ctx.strokeStyle = 'rgba(0,0,0,.55)'; ctx.lineWidth = 7; ctx.beginPath(); pts.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)); ctx.stroke();
  ctx.strokeStyle = '#7dffb0'; ctx.lineWidth = 3.5; ctx.beginPath(); pts.slice(0, cut + 1).forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)); ctx.stroke();
  if (cut < cells.length){ ctx.strokeStyle = 'rgba(230,230,240,.8)'; ctx.setLineDash([6, 6]); ctx.beginPath(); pts.slice(cut).forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)); ctx.stroke(); ctx.setLineDash([]); }
  const e = pts[cut]; ctx.fillStyle = '#7dffb0'; ctx.strokeStyle = 'rgba(0,0,0,.7)'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(e.x, e.y, 6, 0, 7); ctx.fill(); ctx.stroke();   // где окажется в конце хода
  ctx.restore();
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
      const f = fr.f[en.i]; if (!f || f.hp <= 0) return; const n = f.n || 1, ok = en.dist <= range && en.los && !en.charm && !(ga && ga.targetType >= 0 && ga.targetType !== en.ctype), r = Math.max(f.kind < 2 ? RB() : RM(), n > 1 ? n * cell * .42 : 0) + 8 + Math.sin(now / 160) * 2;
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
  const path = G.mode === 'move' && hover ? pathPreview(q, hover.x, hover.y) : null;
  if (path) pathStroke([{x: cx, y: cy}, ...path.cells.map(c => ({x: PX(c.x + .5), y: PY(c.y + .5)}))], path.cells, q.speedLeft);
  if (hover && hover.x >= 0 && hover.y >= 0 && hover.x < AW && hover.y < AH){
    ctx.save(); ctx.strokeStyle = 'rgba(255,255,255,.9)'; ctx.lineWidth = 2; ctx.strokeRect(PX(hover.x), PY(hover.y), cell, cell);
    const n = me.n || 1, gap = (a, b) => Math.max(0, b - (a + n - 1), a - b), d = path ? pathLabel(path, q.speedLeft) : 5 * Math.max(gap(me.x, hover.x), gap(me.y, hover.y)) + ' фт'; ctx.font = '700 13px system-ui'; ctx.fillStyle = '#fff'; ctx.strokeStyle = 'rgba(0,0,0,.8)'; ctx.lineWidth = 4; ctx.textAlign = 'left';
    ctx.strokeText(d, PX(hover.x) + cell + 4, PY(hover.y) + 14); ctx.fillText(d, PX(hover.x) + cell + 4, PY(hover.y) + 14); ctx.restore();
  }
}
addEventListener('keydown', e => {
  if (e.repeat || !viewActive() || G.busy || !G.req || e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  if (e.key === 'e' || e.key === 'E' || e.key === 'Enter'){ cmd({c:'end'}); }
  else if (e.key === 'm' || e.key === 'M'){ G.mode = 'move'; renderTurn(); }
  else if (e.key === 'a' || e.key === 'A'){ const b = document.querySelector('#turn [data-m=attack]'); if (b && !b.disabled) b.click(); }
  else if (/^[1-9]$/.test(e.key)){ const b = document.querySelector(`#turn [data-spell="${+e.key - 1}"]`); if (b && !b.disabled) b.click(); }
});
addEventListener('resize', () => { if (G.active) fitPlay(); });
// поле боя на весь экран: холст по размеру сцены (панели лежат поверх)
fitStage = function(){
  const W = Math.max(300, stage.clientWidth || innerWidth), H = Math.max(240, stage.clientHeight || innerHeight);
  cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr); cv.style.width = W + 'px'; cv.style.height = H + 'px';
  cam.snap = true;
};
function fitPlay(){ fitStage(); }
// поля холста под панелями боя (раз в 300 мс): сверху — панели верхней половины, снизу — нижние, что заходят в середину
// по ширине (журнал и камера в углах не в счёт); свободной части — не меньше 45 % высоты. Камера держит бойцов в ней
let safeT = 0;
function camSafeHook(){
  const now = performance.now(); if (now - safeT < 300) return; safeT = now;
  let t = 0, b = 0;
  if (viewActive() && !document.body.classList.contains('hud-all-off')){
    const S = stage.getBoundingClientRect(), x0 = S.left + S.width * .25, x1 = S.right - S.width * .25;
    document.querySelectorAll('#stagebox > .fx').forEach(el => {
      if (el.classList.contains('hud-off') || !el.offsetParent) return;
      const r = el.getBoundingClientRect(); if (!r.height) return;
      if ((r.top + r.bottom) / 2 < S.top + S.height / 2) t = Math.max(t, r.bottom - S.top + 8);
      else if (r.right > x0 && r.left < x1) b = Math.max(b, S.bottom - r.top + 8);
    });
    const room = S.height * .55; if (t + b > room){ const k = room / (t + b); t *= k; b *= k; }
  }
  if (Math.abs(t - cam.st) > 2 || Math.abs(b - cam.sb) > 2){ cam.st = Math.round(t); cam.sb = Math.round(b); }
}
$('btnFight').onclick = startGame;
$('tempo').onchange = () => { speed = 1.4 * +$('tempo').value; try { localStorage.setItem('dnd_tempo', $('tempo').value); } catch (e) {} };
$('turnTime').onchange = () => { try { localStorage.setItem('dnd_turntime', $('turnTime').value); } catch (e) {} };
try { $('tempo').value = localStorage.getItem('dnd_tempo') || '1'; $('turnTime').value = localStorage.getItem('dnd_turntime') || '0'; } catch (e) {}
if (!$('tempo').value) $('tempo').value = '1';
if (!$('turnTime').value) $('turnTime').value = '0';
speed = 1.4 * +$('tempo').value;
// журнал боя: на телефоне сворачивается
$('logToggle').onclick = () => { const box = $('logBox'), open = !box.classList.contains('open'); box.classList.toggle('open', open); $('logToggle').setAttribute('aria-expanded', open); };
