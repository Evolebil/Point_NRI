// ====================================================================================================================
// 5. Редактор карт (ТЗ ГМ-РКМ: Ш-01 — оболочка и черновик, Ш-02 — холст, инструменты, палитра, параметры)
// ====================================================================================================================
const LAYERS = ['wall','pit','water','diff','climb','dark','height','tree','haz','ladder','deck','door','strong'];
let MAPS = {}; try { MAPS = JSON.parse(localStorage.getItem('dnd_maps') || '{}'); } catch (e) {}
let ME = null, meCS = 20;
// материалы палитры (табл. 7 ТЗ): код ТЗ, ключ в коде, название, цвет значка, группа. Смысл (проходимость, урон) — у движка
const MATS = [
  ['R01','wall','Стена','#4A4A66','relief'], ['R02','pit','Ущелье','#111114','relief'], ['R03','water','Ручей','#3A7FC4','relief'],
  ['R04','h','Высота земли (уровень)','#5C9A58','relief'], ['R05','ladder','Лестница','#D2A55A','relief'], ['R06','deck','Настил / потолок (уровень)','#9A6A3A','relief'],
  ['R07','diff','Грязь','#7A5A34','relief'],
  ['O01','climb','Укрытие','#8A8A8A','obj'], ['O02','tree','Дерево','#2F6A3A','obj'], ['O03','doorC','Дверь (закрыта)','#7A5230','obj'],
  ['O04','doorO','Дверь (открыта)','#A8794A','obj'], ['O05','strong','Крепкая стена','#1D1D2B','obj'],
  ['H01','z1','Лава','#E05A2A','haz'], ['H02','z2','Кислота','#7FD23A','haz'], ['H03','z3','Ядовитый дым','#8A9A4A','haz'], ['H04','z4','Ледяное поле','#8FD2F0','haz'],
  ['H05','z5','Грозовое поле','#5A5AE0','haz'], ['H06','z6','Некротический туман','#4A3A6A','haz'], ['H07','dark','Тьма','#1A1A3A','haz'],
  ['S01','startA','Старт A','#3A78E0','start'], ['S02','startB','Старт B','#D6452E','start'], ['S03','erase','Ластик','#9A9A9A','start']];
const MAT = Object.fromEntries(MATS.map(m => [m[1], m]));
const MAT_GROUPS = [['Рельеф', 'relief'], ['Объекты', 'obj'], ['Опасное', 'haz'], ['Старты и ластик', 'start']];
const GROUP_NAME = {relief: 'Рельеф', obj: 'Объекты', haz: 'Опасное', start: 'Старты'};
const GROUP_LAYERS = {relief: ['wall','pit','water','height','ladder','deck','diff'], obj: ['climb','tree','door','strong'], haz: ['haz','dark']};
// инструменты (табл. 6): ключ, название, клавиша, код клавиши (работает и в русской раскладке), значок; черта — после пипетки
const ME_TOOLS = [['brush','Кисть','B','KeyB','✎'], ['fill','Заливка','G','KeyG','◩'], ['rect','Прямоугольник','R','KeyR','▭'], ['line','Линия','L','KeyL','╱'],
  ['erase','Ластик','E','KeyE','⌫'], ['pick','Пипетка','I','KeyI','⊙'], ['select','Выделение','S','KeyS','⬚'], ['start','Старты','T','KeyT','⚑']];
const ED = {tool: 'brush', prev: 'brush', mat: 'wall', side: 0, size: 1, shape: 'sq', rect: 'fill', grid: true, sel: null, clip: null, drag: null,
  view: false, locked: false, hover: null, dirty: null, raf: 0, warned: 0, fav: []};
try { ED.grid = localStorage.getItem('dnd_me_grid') !== '0'; ED.fav = JSON.parse(localStorage.getItem('dnd_me_fav') || '[]').filter(k => MAT[k]).slice(0, 12); } catch (e) {}
const START_MAX = 16;   // клеток одного старта (Д-06)

function meNew(aw, ah, theme = 0){ const L = {}; LAYERS.forEach(k => L[k] = new Uint8Array(aw * ah)); return {name: ME ? ME.name : 'Новая арена', aw, ah, theme, L, starts: [[], []], vx: new Map()}; }
// Высота, настил и дерево — в уровнях по 5 фт, символ клетки — цифра или буква (0–9, a–z = 10–35); карта с "hz":5.
// Старые карты (уровни по 10 фт, дерево «1») переводятся при чтении: высота и настил ×2, дерево — 20 фт.
// Старты: движок читает "starts" — по клетке на сторону; редактор хранит до 16 клеток на сторону в "st": {"A": [...], "B": [...]}
// (пишется, только когда у стороны не одна клетка — карты с одной клеткой на сторону не меняются)
function meFromJson(j, name){
  const m = meNew(j.aw, j.ah, j.theme || 0), old = j.hz !== 5; if (name) m.name = name;
  LAYERS.forEach(k => { for (let y = 0; y < j.ah; y++){ const row = (j[k] || [])[y] || ''; for (let x = 0; x < j.aw; x++){
    const v = Math.max(0, parseInt(row[x] || '0', 36) || 0);
    m.L[k][y * j.aw + x] = !old || !HZ_LAYERS.includes(k) ? v : (k === 'tree' ? (v ? 4 : 0) : 2 * v); } } });
  const ok = c => Array.isArray(c) && c[0] >= 0 && c[1] >= 0 && c[0] < j.aw && c[1] < j.ah;
  const side = (list) => { const out = []; (Array.isArray(list) ? list : []).forEach(c => { if (ok(c) && out.length < START_MAX && !out.some(o => o[0] === c[0] && o[1] === c[1])) out.push([c[0], c[1]]); }); return out; };
  if (j.st && typeof j.st === 'object') m.starts = [side(j.st.A), side(j.st.B)];
  else m.starts = [side(j.starts && j.starts[0] ? [j.starts[0]] : []), side(j.starts && j.starts[1] ? [j.starts[1]] : [])];
  // слой кубов (этап 4: пещеры и дома генератора для боя в 3D): [x, y, нижний куб, материалы] — хранится как есть
  if (Array.isArray(j.vx)) for (const c of j.vx) if (Array.isArray(c) && c[0] >= 0 && c[1] >= 0 && c[0] < j.aw && c[1] < j.ah && typeof c[3] === 'string') m.vx.set(c[0] + ',' + c[1], [c[0], c[1], c[2], c[3]]);
  return m;
}
const HZ_LAYERS = ['height', 'deck', 'tree'];
function meToJson(m = ME){
  const o = {aw: m.aw, ah: m.ah, theme: m.theme, hz: 5};
  LAYERS.forEach(k => { o[k] = []; for (let y = 0; y < m.ah; y++){ let r = ''; for (let x = 0; x < m.aw; x++) r += m.L[k][y * m.aw + x].toString(36); o[k].push(r); } });
  const [A, B] = m.starts;
  if (A.length && B.length) o.starts = [A[0].slice(), B[0].slice()];
  if ((A.length || B.length) && (A.length !== 1 || B.length !== 1)) o.st = {A: A.map(c => c.slice()), B: B.map(c => c.slice())};
  const vx = [...(m.vx || new Map()).values()].filter(c => c[0] < m.aw && c[1] < m.ah).sort((a, b) => a[1] - b[1] || a[0] - b[0]);
  if (vx.length) o.vx = vx;
  return o;
}
function meMsg(t, cls = ''){
  $('meMsg').innerHTML = t ? `<div class="${cls || 'hint'}">${esc(t)}</div>` : '';
  if (t && !$('meMsg').offsetParent) toast(t, 4000, cls === 'err');   // вкладка «Карта» скрыта — иначе ответа не видно
}

// ---- клетки: материал, ластик группы, старты ----
const meGroup = k => (MAT[k] || MAT.wall)[4];
// поставить материал k в клетку (x, y): правила сочетаний — как были в редакторе (стена убирает воду и дерево и т. п.)
function meSet(x, y, k){
  const m = ME, i = y * m.aw + x, L = m.L;
  if (k === 'startA' || k === 'startB'){ meStartAdd(x, y, k === 'startB' ? 1 : 0); return; }
  if (k === 'erase') return;
  if (m.vx) m.vx.delete(x + ',' + y);   // правка клетки снимает её столбец слоя кубов: дальше клетка — по слоям
  if (/^z\d$/.test(k)){ if (!L.wall[i] && !L.pit[i]) L.haz[i] = +k[1]; return; }
  const lv = +$('meLevel').value || 0;
  if (k === 'h'){ if (!L.pit[i] && !L.wall[i]){ L.height[i] = lv; if (L.deck[i] && L.deck[i] <= lv) L.deck[i] = 0; } return; }
  if (k === 'doorC' || k === 'doorO'){ if (!L.wall[i] && !L.pit[i]){ L.door[i] = k === 'doorC' ? 1 : 2; L.tree[i] = L.climb[i] = 0; } return; }
  if (k === 'strong'){ L.wall[i] = 1; L.strong[i] = 1; L.door[i] = 0; L.tree[i] = 0; return; }
  if (k === 'deck'){ if (!L.wall[i] && lv > L.height[i]) L.deck[i] = lv; return; }
  if (k === 'wall'){ L.wall[i] = 1; L.pit[i] = 0; L.water[i] = 0; L.climb[i] = 0; L.height[i] = 0; L.tree[i] = 0; }
  else if (k === 'tree'){ if (!L.wall[i] && !L.pit[i] && !L.water[i]){ L.tree[i] = +$('meTreeH').value || 4; L.climb[i] = 0; } }   // высота дерева — из списка (10–40 фт)
  else if (k === 'pit'){ L.pit[i] = 1; L.wall[i] = 0; L.water[i] = 0; L.height[i] = 0; L.climb[i] = 0; }
  else if (k === 'water'){ L.water[i] = 1; L.wall[i] = 0; L.pit[i] = 0; }
  else if (k === 'climb'){ if (!L.wall[i] && !L.pit[i]){ L.climb[i] = 1; L.tree[i] = 0; } }
  else L[k][i] = 1;   // лестница, грязь, тьма
}
// ластик (И-05): очищает слой группы — рельеф на «землю» уровня 0, объект и опасное убираются, старт снимается
function meClear(x, y, g){
  const m = ME, i = y * m.aw + x, L = m.L;
  if (g === 'start'){ meStartDel(x, y); return; }
  if (m.vx) m.vx.delete(x + ',' + y);
  if (g === 'relief'){ if (!L.strong[i]) L.wall[i] = 0; L.pit[i] = L.water[i] = L.height[i] = L.ladder[i] = L.deck[i] = L.diff[i] = 0; }
  else if (g === 'obj'){ if (L.strong[i]){ L.strong[i] = 0; L.wall[i] = 0; } L.climb[i] = L.tree[i] = L.door[i] = 0; }
  else if (g === 'haz'){ L.haz[i] = 0; L.dark[i] = 0; }
}
const meStartAt = (list, x, y) => list.findIndex(c => c[0] === x && c[1] === y);
function meStartAdd(x, y, side){
  const s = ME.starts; if (meStartAt(s[side], x, y) >= 0) return true;
  if (s[side].length >= START_MAX){ if (Date.now() - ED.warned > 1500){ ED.warned = Date.now(); toast('У старта может быть не больше 16 клеток.', 4000, true); } return false; }   // М-07
  const o = meStartAt(s[1 - side], x, y); if (o >= 0) s[1 - side].splice(o, 1);
  s[side].push([x, y]); const i = y * ME.aw + x; ME.L.wall[i] = 0; ME.L.pit[i] = 0;   // старт не стоит в стене и ущелье
  meTouch(x, y); return true;
}
function meStartDel(x, y){ ME.starts.forEach(l => { const k = meStartAt(l, x, y); if (k >= 0) l.splice(k, 1); }); meTouch(x, y); }
const meIn = (x, y) => x >= 0 && y >= 0 && x < ME.aw && y < ME.ah;

// ---- рисование: перерисовываются только изменённые клетки (в ближайшем кадре); подсветка — отдельным слоем ----
function meTouch(x0, y0, x1 = x0, y1 = y0){
  const d = ED.dirty || (ED.dirty = {x0, y0, x1, y1});
  d.x0 = Math.min(d.x0, x0); d.y0 = Math.min(d.y0, y0); d.x1 = Math.max(d.x1, x1); d.y1 = Math.max(d.y1, y1);
  if (!ED.raf) ED.raf = requestAnimationFrame(meFlush);
}
function meFlush(){
  ED.raf = 0; const d = ED.dirty; ED.dirty = null; if (!d || !ME) return;
  paintMap($('meCv').getContext('2d'), ME, meCS, Math.max(0, d.x0), Math.max(0, d.y0), Math.min(ME.aw, d.x1 + 1), Math.min(ME.ah, d.y1 + 1), ED.grid);
}
function meDraw(){
  const cv2 = $('meCv'), m = ME;
  meCS = Math.max(8, Math.min(40, Math.floor(2400 / Math.max(m.aw, m.ah))));
  if (cv2.width !== m.aw * meCS || cv2.height !== m.ah * meCS){ cv2.width = m.aw * meCS; cv2.height = m.ah * meCS; cv2.style.width = cv2.width + 'px'; cv2.style.height = cv2.height + 'px'; }
  ED.dirty = null; paintMap(cv2.getContext('2d'), m, meCS, 0, 0, m.aw, m.ah, ED.grid);
  $('meTitle').textContent = wsTitle(m.name || 'Без названия', `${m.aw}×${m.ah}`);
  const key = m.aw + 'x' + m.ah + ':' + meCS;
  if (MV.key !== key){ MV.key = key; meFit(); }
  meOverlay();
}
// предпросмотр карты в чужом холсте (комната, фон меню): вписать по центру
function drawMapFit(cv2, m){
  const c = cv2.getContext('2d'), cs = Math.max(1, Math.floor(Math.min(cv2.width / m.aw, cv2.height / m.ah)));
  c.setTransform(1, 0, 0, 1, 0, 0); c.clearRect(0, 0, cv2.width, cv2.height);
  c.translate(Math.floor((cv2.width - m.aw * cs) / 2), Math.floor((cv2.height - m.ah * cs) / 2));
  paintMap(c, m, cs, 0, 0, m.aw, m.ah, true); c.setTransform(1, 0, 0, 1, 0, 0);
}
// клетки [x0, x1) × [y0, y1) карты m по cs пикселей; клетка закрашивается целиком, поэтому часть можно рисовать поверх старого
function paintMap(c, m, cs, x0 = 0, y0 = 0, x1 = m.aw, y1 = m.ah, grid = true){
  const city = m.theme === 1, g = city ? [74,78,90] : [58,92,60];
  for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++){
    const i = y * m.aw + x, h = m.L.height[i], px = x * cs, py = y * cs, n = ((x * 7 + y * 13) % 5) * 2;
    c.fillStyle = `rgb(${g[0] + h * 9 + n},${g[1] + h * 9 + n},${g[2] + h * 9 + n})`; c.fillRect(px, py, cs, cs);
    if (m.L.diff[i]){ c.fillStyle = 'rgba(86,62,30,.5)'; c.fillRect(px, py, cs, cs); }
    if (m.L.water[i]){ c.fillStyle = '#2d6fa3'; c.fillRect(px, py, cs, cs); }
    if (m.L.pit[i]){ c.fillStyle = '#07080c'; c.fillRect(px, py, cs, cs); }
    if (m.L.wall[i]){ c.fillStyle = '#2a2d36'; c.fillRect(px, py, cs, cs); c.fillStyle = '#4a4f5e'; c.fillRect(px, py, cs, Math.max(2, cs * .3)); }
    if (m.L.climb[i]){ c.fillStyle = '#7d7568'; c.beginPath(); c.arc(px + cs / 2, py + cs / 2, cs * .36, 0, 7); c.fill(); }
    if (m.L.tree[i]){ c.fillStyle = '#2f6a33'; c.beginPath(); c.arc(px + cs / 2, py + cs / 2, cs * (.3 + .02 * m.L.tree[i]), 0, 7); c.fill(); c.strokeStyle = 'rgba(0,0,0,.4)'; c.lineWidth = 1; c.stroke();
      if (cs >= 16){ c.fillStyle = 'rgba(255,255,255,.85)'; c.font = `700 ${Math.round(cs * .32)}px system-ui`; c.textAlign = 'center'; c.textBaseline = 'middle'; c.fillText(m.L.tree[i] * 5, px + cs / 2, py + cs / 2); } }   // высота дерева в футах
    if (m.L.haz[i]){ c.fillStyle = ['', '#e2572a', '#7ccf2e', '#7e8f57', '#9fd8ff', '#5868d8', '#4b3a63'][m.L.haz[i]]; c.globalAlpha = .75; c.fillRect(px, py, cs, cs); c.globalAlpha = 1; }
    if (m.L.dark[i]){ c.fillStyle = 'rgba(6,8,28,.5)'; c.fillRect(px, py, cs, cs); }
    if (h){ c.strokeStyle = 'rgba(255,255,255,.35)'; c.lineWidth = 1; c.strokeRect(px + .5, py + .5, cs - 1, cs - 1); if (cs >= 16){ c.fillStyle = 'rgba(255,255,255,.7)'; c.font = `700 ${Math.round(cs * .38)}px system-ui`; c.textAlign = 'right'; c.textBaseline = 'top'; c.fillText(h * 5, px + cs - 2, py + 1); } }
    if (m.L.door[i]){ c.fillStyle = m.L.door[i] === 1 ? '#6b4423' : 'rgba(160,112,64,.55)'; c.fillRect(px + cs * .2, py + cs * .2, cs * .6, cs * .6); }
    if (m.L.strong[i]){ c.fillStyle = '#17191f'; c.fillRect(px, py, cs, cs); c.fillStyle = '#565c6c'; c.fillRect(px, py, cs, Math.max(2, cs * .3)); }
    if (m.L.ladder[i]){ c.strokeStyle = '#c9a46a'; c.lineWidth = 2; c.beginPath(); c.moveTo(px + cs * .3, py + 2); c.lineTo(px + cs * .3, py + cs - 2); c.moveTo(px + cs * .7, py + 2); c.lineTo(px + cs * .7, py + cs - 2); for (let k = 1; k <= 3; k++){ c.moveTo(px + cs * .3, py + cs * k / 4); c.lineTo(px + cs * .7, py + cs * k / 4); } c.stroke(); }
    if (m.L.deck[i]){ c.fillStyle = 'rgba(150,112,60,.5)'; c.fillRect(px, py, cs, cs); c.strokeStyle = 'rgba(240,200,120,.9)'; c.lineWidth = 1; c.strokeRect(px + 1.5, py + 1.5, cs - 3, cs - 3); if (cs >= 16){ c.fillStyle = '#ffe2a0'; c.font = `700 ${Math.round(cs * .34)}px system-ui`; c.textAlign = 'left'; c.textBaseline = 'bottom'; c.fillText('⌂' + m.L.deck[i] * 5, px + 2, py + cs - 1); } }
  }
  for (const [x,y,z,col] of (m.vx || new Map()).values()){
    if (x<x0 || x>=x1 || y<y0 || y>=y1) continue;
    const surface = 1 + m.L.height[y*m.aw+x];
    if (col[surface-z] !== '0') continue;
    c.fillStyle='#211d1a'; c.fillRect(x*cs,y*cs,cs,cs);
    c.fillStyle='#ab9981';
    for (let j=1;j<=3;j++) c.fillRect((x+j*.17)*cs,(y+j*.22)*cs,cs*(1-j*.2),cs*.09);
  }
  if (grid && cs >= 6){ c.strokeStyle = 'rgba(255,255,255,.07)'; c.lineWidth = 1; c.beginPath(); for (let x = x0; x <= x1; x++){ c.moveTo(x * cs + .5, y0 * cs); c.lineTo(x * cs + .5, y1 * cs); } for (let y = y0; y <= y1; y++){ c.moveTo(x0 * cs, y * cs + .5); c.lineTo(x1 * cs, y * cs + .5); } c.stroke(); }
  [['A', '#2f7de1'], ['B', '#e0553f']].forEach(([l, col], k) => (m.starts[k] || []).forEach(st => {
    if (st[0] < x0 || st[0] >= x1 || st[1] < y0 || st[1] >= y1) return;
    c.fillStyle = col; c.beginPath(); c.arc((st[0] + .5) * cs, (st[1] + .5) * cs, Math.max(3, cs * .42), 0, 7); c.fill(); c.strokeStyle = '#fff'; c.lineWidth = 2; c.stroke();
    if (cs >= 10){ c.fillStyle = '#fff'; c.font = `800 ${Math.max(8, Math.round(cs * .5))}px system-ui`; c.textAlign = 'center'; c.textBaseline = 'middle'; c.fillText(l, (st[0] + .5) * cs, (st[1] + .5) * cs + 1); }
  }));
}

// ---- вид: холст двигает и масштабирует преобразование MV; 100 % — клетка 44×44 px, пределы 10–800 % (п. 4.3.9.2) ----
const MV = {z: 1, x: 0, y: 0, key: ''};
const mePct = () => MV.z * meCS / 44 * 100;
// свободная часть экрана между панелями — туда карта вписывается при открытии и по кнопке «Вписать» (отступ 24 px)
function meFreeRect(){
  const st = $('meStage').getBoundingClientRect(), pad = 24, r = {l: st.left + pad, t: st.top + pad, r: st.right - pad, b: st.bottom - pad};
  const vis = el => el && getComputedStyle(el).display !== 'none' && el.getBoundingClientRect();
  const top = vis(document.querySelector('#s5 .ws-top')), bot = vis(document.querySelector('#s5 .ws-bottom')), lf = vis(document.querySelector('#s5 .ws-left')), rt = vis(document.querySelector('#s5 .ws-right'));
  if (top) r.t = Math.max(r.t, top.bottom + pad);
  if (bot) r.b = Math.min(r.b, bot.top - pad);
  if (lf) r.l = Math.max(r.l, lf.right + pad);
  if (rt) r.r = Math.min(r.r, rt.left - pad);
  if (r.r - r.l < 120){ r.l = st.left + pad; r.r = st.right - pad; }
  if (r.b - r.t < 120){ r.t = st.top + pad; r.b = st.bottom - pad; }
  return {x: r.l - st.left, y: r.t - st.top, w: r.r - r.l, h: r.b - r.t};
}
function meFit(){
  if (!$('meStage').clientWidth || !$('meCv').width) return;   // мастерская скрыта — впишем при входе
  const cv2 = $('meCv'), f = meFreeRect();
  MV.z = Math.min(f.w / cv2.width, f.h / cv2.height);
  MV.z = Math.max(10, Math.min(800, mePct())) * 44 / 100 / meCS;
  MV.x = f.x + (f.w - cv2.width * MV.z) / 2; MV.y = f.y + (f.h - cv2.height * MV.z) / 2;
  meView();
}
function meView(){
  if (!$('meStage').clientWidth) return;
  $('meCv').style.transform = `translate(${MV.x}px,${MV.y}px) scale(${MV.z})`;
  $('meZoomPct').textContent = Math.round(mePct()) + '%';
  meOverlay();
}
// масштаб pct (%) с опорой в точке (px, py) сцены: точка карты под курсором остаётся на месте
function meZoomTo(pct, px, py){
  const z = Math.max(10, Math.min(800, pct)) * 44 / 100 / meCS;
  MV.x = px - (px - MV.x) * z / MV.z; MV.y = py - (py - MV.y) * z / MV.z; MV.z = z; meView();
}
const meZoomAt = (k, px, py) => meZoomTo(mePct() * k, px, py);

// ---- подсветка поверх холста: след кисти под курсором, предпросмотр прямоугольника и линии, выделение ----
function meOverlay(){
  const ov = $('meOv'), st = $('meStage'); if (!ov || !st.clientWidth) return;
  const d = devicePixelRatio || 1, W = st.clientWidth, H = st.clientHeight;
  if (ov.width !== Math.round(W * d) || ov.height !== Math.round(H * d)){ ov.width = Math.round(W * d); ov.height = Math.round(H * d); }
  const c = ov.getContext('2d'); c.setTransform(d, 0, 0, d, 0, 0); c.clearRect(0, 0, W, H);
  if (!ME || ED.view) return;
  const S = MV.z * meCS, X = x => MV.x + x * S, Y = y => MV.y + y * S;
  const cells = (list, stroke, fill) => { c.fillStyle = fill; c.strokeStyle = stroke; c.lineWidth = 1; list.forEach(([x, y]) => { if (!meIn(x, y)) return; c.fillRect(X(x), Y(y), S, S); c.strokeRect(X(x) + .5, Y(y) + .5, S - 1, S - 1); }); };
  const box = (x0, y0, x1, y1, col, dash) => { c.save(); c.setLineDash(dash || []); c.strokeStyle = col; c.lineWidth = 2; c.strokeRect(X(x0), Y(y0), (x1 - x0 + 1) * S, (y1 - y0 + 1) * S); c.restore(); };
  const dr = ED.drag;
  if (dr && dr.kind === 'rect') cells(meRectCells(dr.x0, dr.y0, dr.x, dr.y, dr.shift), 'rgba(255,255,255,.9)', 'rgba(255,255,255,.18)');
  else if (dr && dr.kind === 'line') cells(meLineStamp(dr.x0, dr.y0, ...meLineEnd(dr.x0, dr.y0, dr.x, dr.y, dr.shift)), 'rgba(255,255,255,.9)', 'rgba(255,255,255,.18)');
  else if (dr && dr.kind === 'selnew'){ const r = meNorm(dr.x0, dr.y0, dr.x, dr.y); box(r.x0, r.y0, r.x1, r.y1, '#9fdcff', [6, 4]); }
  else if (ED.hover && ['brush', 'erase', 'line', 'rect'].includes(ED.tool)) cells(ED.tool === 'rect' ? [ED.hover] : meStamp(ED.hover[0], ED.hover[1]), 'rgba(255,255,255,.8)', ED.tool === 'erase' ? 'rgba(255,90,90,.18)' : 'rgba(255,255,255,.12)');
  else if (ED.hover && ['fill', 'pick', 'start'].includes(ED.tool)) cells([ED.hover], 'rgba(255,255,255,.9)', 'rgba(255,255,255,.1)');
  if (ED.sel){
    const s = ED.sel, dx = dr && dr.kind === 'move' ? dr.x - dr.x0 : 0, dy = dr && dr.kind === 'move' ? dr.y - dr.y0 : 0;
    box(s.x0 + dx, s.y0 + dy, s.x1 + dx, s.y1 + dy, '#9fdcff', [6, 4]);
  }
}

// ---- геометрия инструментов: след кисти (квадрат или круг), линия Брезенхэма, прямоугольник ----
function meStamp(cx, cy, b = ED.size, shape = ED.shape){
  const out = [], o = Math.floor((b - 1) / 2), c = (b - 1) / 2, r = b / 2 - .25;
  for (let ky = 0; ky < b; ky++) for (let kx = 0; kx < b; kx++){
    if (shape === 'ci' && b > 2 && (kx - c) * (kx - c) + (ky - c) * (ky - c) > r * r) continue;
    out.push([cx - o + kx, cy - o + ky]);
  }
  return out;
}
function meLineCells(x0, y0, x1, y1){
  const out = [], dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  let err = dx + dy;
  for (;;){ out.push([x0, y0]); if (x0 === x1 && y0 === y1) break; const e2 = 2 * err; if (e2 >= dy){ err += dy; x0 += sx; } if (e2 <= dx){ err += dx; y0 += sy; } }
  return out;
}
// конец линии: с Shift — угол кратен 45°
function meLineEnd(x0, y0, x1, y1, shift){
  if (!shift) return [x1, y1];
  const dx = x1 - x0, dy = y1 - y0, ax = Math.abs(dx), ay = Math.abs(dy);
  if (ax > 2 * ay) return [x1, y0];
  if (ay > 2 * ax) return [x0, y1];
  const d = Math.max(ax, ay); return [x0 + Math.sign(dx) * d, y0 + Math.sign(dy) * d];
}
// линия толщиной в размер кисти: след кисти в каждой точке, без повторов
function meLineStamp(x0, y0, x1, y1){
  const seen = new Set(), out = [];
  meLineCells(x0, y0, x1, y1).forEach(([x, y]) => meStamp(x, y).forEach(c => { const k = c[0] + ',' + c[1]; if (!seen.has(k)){ seen.add(k); out.push(c); } }));
  return out;
}
const meNorm = (ax, ay, bx, by) => ({x0: Math.min(ax, bx), y0: Math.min(ay, by), x1: Math.max(ax, bx), y1: Math.max(ay, by)});
// прямоугольник от угла к углу: залитый или контур; с Shift — квадрат (сторона — по большей стороне)
function meRectCells(ax, ay, bx, by, shift){
  if (shift){ const d = Math.max(Math.abs(bx - ax), Math.abs(by - ay)); bx = ax + (bx >= ax ? d : -d); by = ay + (by >= ay ? d : -d); }
  const r = meNorm(ax, ay, bx, by), out = [];
  for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++)
    if (ED.rect === 'fill' || x === r.x0 || x === r.x1 || y === r.y0 || y === r.y1) out.push([x, y]);
  return out;
}
// применить к клеткам: материал (кисть, линия, прямоугольник) или ластик группы; клетки вне карты пропускаются
function meApply(list, erase){
  const g = meGroup(ED.mat);
  list.forEach(([x, y]) => { if (!meIn(x, y)) return; if (erase) meClear(x, y, g); else meSet(x, y, ED.mat); meTouch(x, y); });
}
// заливка (И-02): связная по 4 сторонам область, где слой группы материала такой же, как в кликнутой клетке
function meFill(x, y){
  const g = meGroup(ED.mat); if (g === 'start' || !meIn(x, y)) return;
  const m = ME, Ls = GROUP_LAYERS[g].map(k => m.L[k]), W = m.aw, H = m.ah, i0 = y * W + x;
  const same = i => Ls.every(L => L[i] === L[i0]), seen = new Uint8Array(W * H), q = [i0], out = []; seen[i0] = 1;
  while (q.length){
    const i = q.pop(), cx = i % W, cy = (i - cx) / W; out.push([cx, cy]);
    if (cx > 0 && !seen[i - 1] && same(i - 1)){ seen[i - 1] = 1; q.push(i - 1); }
    if (cx < W - 1 && !seen[i + 1] && same(i + 1)){ seen[i + 1] = 1; q.push(i + 1); }
    if (cy > 0 && !seen[i - W] && same(i - W)){ seen[i - W] = 1; q.push(i - W); }
    if (cy < H - 1 && !seen[i + W] && same(i + W)){ seen[i + W] = 1; q.push(i + W); }
  }
  out.forEach(([cx, cy]) => meSet(cx, cy, ED.mat));
  meTouch(0, 0, W - 1, H - 1);
}
// пипетка (И-06): верхний непустой материал клетки — опасное, объект, рельеф; затем прежний инструмент
function mePick(x, y){
  const L = ME.L, i = y * ME.aw + x; let k = null;
  const lv = v => { const o = $('meLevel'); if ([...o.options].some(p => +p.value === v)) o.value = v; };
  if (L.haz[i]) k = 'z' + L.haz[i]; else if (L.dark[i]) k = 'dark';
  else if (L.strong[i]) k = 'strong'; else if (L.door[i]) k = L.door[i] === 1 ? 'doorC' : 'doorO';
  else if (L.tree[i]){ k = 'tree'; const o = $('meTreeH'); if ([...o.options].some(p => +p.value === L.tree[i])) o.value = L.tree[i]; }
  else if (L.climb[i]) k = 'climb';
  else if (L.wall[i]) k = 'wall'; else if (L.pit[i]) k = 'pit'; else if (L.water[i]) k = 'water'; else if (L.ladder[i]) k = 'ladder';
  else if (L.deck[i]){ k = 'deck'; lv(L.deck[i]); } else if (L.diff[i]) k = 'diff'; else if (L.height[i]){ k = 'h'; lv(L.height[i]); }
  if (k) ED.mat = k;
  meSetTool(ED.prev);
}

// ---- выделение (И-07): буфер — все слои и столбцы кубов прямоугольника; старты не переносятся ----
function meCopy(s){
  const w = s.x1 - s.x0 + 1, h = s.y1 - s.y0 + 1, b = {w, h, L: {}, vx: []};
  LAYERS.forEach(k => { const a = b.L[k] = new Uint8Array(w * h); for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) a[y * w + x] = ME.L[k][(s.y0 + y) * ME.aw + s.x0 + x]; });
  (ME.vx || new Map()).forEach(c => { if (c[0] >= s.x0 && c[0] <= s.x1 && c[1] >= s.y0 && c[1] <= s.y1) b.vx.push([c[0] - s.x0, c[1] - s.y0, c[2], c[3]]); });
  return b;
}
function meWipe(s){
  for (let y = s.y0; y <= s.y1; y++) for (let x = s.x0; x <= s.x1; x++){ LAYERS.forEach(k => ME.L[k][y * ME.aw + x] = 0); ME.vx.delete(x + ',' + y); }
  meTouch(s.x0, s.y0, s.x1, s.y1);
}
function mePaste(b, px, py){
  for (let y = 0; y < b.h; y++) for (let x = 0; x < b.w; x++){
    const tx = px + x, ty = py + y; if (!meIn(tx, ty)) continue;
    LAYERS.forEach(k => ME.L[k][ty * ME.aw + tx] = b.L[k][y * b.w + x]); ME.vx.delete(tx + ',' + ty);
  }
  b.vx.forEach(c => { const tx = px + c[0], ty = py + c[1]; if (meIn(tx, ty)) ME.vx.set(tx + ',' + ty, [tx, ty, c[2], c[3]]); });
  meTouch(px, py, px + b.w - 1, py + b.h - 1);
}
// выделение в пределах карты (после вставки и поворота часть может уйти за край)
const meClip = s => { const r = {x0: Math.max(0, s.x0), y0: Math.max(0, s.y0), x1: Math.min(ME.aw - 1, s.x1), y1: Math.min(ME.ah - 1, s.y1)}; return r.x0 > r.x1 || r.y0 > r.y1 ? null : r; };
function meTurn(b, how){   // поворот на 90° по часовой стрелке, отражения по горизонтали (h) и вертикали (v)
  const w = how === 'r' ? b.h : b.w, h = how === 'r' ? b.w : b.h, o = {w, h, L: {}, vx: []};
  const src = (x, y) => how === 'r' ? [y, b.h - 1 - x] : how === 'h' ? [b.w - 1 - x, y] : [x, b.h - 1 - y];
  LAYERS.forEach(k => { const a = o.L[k] = new Uint8Array(w * h); for (let y = 0; y < h; y++) for (let x = 0; x < w; x++){ const [sx, sy] = src(x, y); a[y * w + x] = b.L[k][sy * b.w + sx]; } });
  b.vx.forEach(c => { for (let y = 0; y < h; y++) for (let x = 0; x < w; x++){ const [sx, sy] = src(x, y); if (sx === c[0] && sy === c[1]) o.vx.push([x, y, c[2], c[3]]); } });
  return o;
}
function meSelOp(op){
  const s = ED.sel; if (!s && op !== 'paste') return;
  const before = meShot();
  if (op === 'copy'){ ED.clip = meCopy(s); toast('Скопировано'); return; }
  if (op === 'cut'){ ED.clip = meCopy(s); meWipe(s); }
  else if (op === 'del') meWipe(s);
  else if (op === 'paste'){
    if (!ED.clip) return;
    const [px, py] = ED.hover || (s ? [s.x0, s.y0] : [0, 0]);
    mePaste(ED.clip, px, py); ED.sel = meClip({x0: px, y0: py, x1: px + ED.clip.w - 1, y1: py + ED.clip.h - 1});
    if (ED.tool !== 'select') meSetTool('select');
  }
  else { const b = meTurn(meCopy(s), op); meWipe(s); mePaste(b, s.x0, s.y0); ED.sel = meClip({x0: s.x0, y0: s.y0, x1: s.x0 + b.w - 1, y1: s.y0 + b.h - 1}); }
  meStep(before); meOverlay();
}

// ---- указатели на сцене: мышь (ЛКМ — инструмент, ПКМ — ластик группы, средняя или пробел + ЛКМ — сдвиг), касания (один
// палец — инструмент, два — сдвиг и масштаб щипком); в режиме просмотра один палец и мышь двигают карту ----
const meCvEl = $('meCv'), meStageEl = $('meStage');
const MP = new Map();   // id указателя -> {x, y}
let mePan = null, meSpace = false, mePinch = null;
function meCellAt(e){
  const r = meStageEl.getBoundingClientRect(), S = MV.z * meCS;
  return [Math.floor((e.clientX - r.left - MV.x) / S), Math.floor((e.clientY - r.top - MV.y) / S)];
}
function meHoverInfo(){
  const h = ED.hover;
  $('meCoord').textContent = h && ME && meIn(h[0], h[1]) ? `Клетка ${h[0] + 1}, ${h[1] + 1} · высота ${ME.L.height[h[1] * ME.aw + h[0]] * 5} фт` : '';
}
meStageEl.addEventListener('contextmenu', e => e.preventDefault());
meStageEl.addEventListener('pointerdown', e => {
  const r = meStageEl.getBoundingClientRect(); MP.set(e.pointerId, {x: e.clientX - r.left, y: e.clientY - r.top});
  try { meStageEl.setPointerCapture(e.pointerId); } catch (er) {}   // захват не обязателен: без него просто теряем указатель за краем
  if (e.pointerType === 'touch' && MP.size === 2){ meCancelDrag(); mePan = null; const [a, b] = [...MP.values()]; mePinch = {d: Math.hypot(a.x - b.x, a.y - b.y), cx: (a.x + b.x) / 2, cy: (a.y + b.y) / 2}; return; }
  if (MP.size > 1) return;
  if (e.button === 1 || (e.button === 0 && (meSpace || ED.view))){ e.preventDefault(); mePan = {x: e.clientX, y: e.clientY, ox: MV.x, oy: MV.y}; meStageEl.classList.add('panning'); return; }
  if (ED.view || (e.button !== 0 && e.button !== 2) || !ME) return;
  const [x, y] = meCellAt(e), t = e.button === 2 ? 'erase' : ED.tool;
  ED.hover = [x, y];
  if (t === 'brush' || t === 'erase'){ ED.drag = {kind: 'stroke', erase: t === 'erase', pend: meShot(), lx: x, ly: y}; meApply(meStamp(x, y), t === 'erase'); }
  else if (t === 'fill'){ if (meIn(x, y)){ const b = meShot(); meFill(x, y); meStep(b); } }
  else if (t === 'pick'){ if (meIn(x, y)) mePick(x, y); }
  else if (t === 'start'){
    if (!meIn(x, y)) return;
    const b = meShot(); if (e.altKey) meStartDel(x, y); else meStartAdd(x, y, ED.side); meStep(b);
  }
  else if (t === 'rect' || t === 'line') ED.drag = {kind: t, x0: x, y0: y, x, y, shift: e.shiftKey};
  else if (t === 'select'){
    const s = ED.sel;
    if (s && x >= s.x0 && x <= s.x1 && y >= s.y0 && y <= s.y1) ED.drag = {kind: 'move', x0: x, y0: y, x, y};
    else { ED.sel = null; ED.drag = {kind: 'selnew', x0: x, y0: y, x, y}; }
  }
  meOverlay();
});
meStageEl.addEventListener('pointermove', e => {
  const r = meStageEl.getBoundingClientRect(); if (MP.has(e.pointerId)) MP.set(e.pointerId, {x: e.clientX - r.left, y: e.clientY - r.top});
  if (mePinch && MP.size >= 2){
    const [a, b] = [...MP.values()], d = Math.hypot(a.x - b.x, a.y - b.y), cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2;
    MV.x += cx - mePinch.cx; MV.y += cy - mePinch.cy; meZoomAt(d / Math.max(1, mePinch.d), cx, cy); mePinch = {d, cx, cy}; return;
  }
  if (mePan){ MV.x = mePan.ox + e.clientX - mePan.x; MV.y = mePan.oy + e.clientY - mePan.y; meView(); return; }
  if (!ME) return;
  const [x, y] = meCellAt(e), moved = !ED.hover || ED.hover[0] !== x || ED.hover[1] !== y;
  ED.hover = [x, y]; if (moved) meHoverInfo();
  const dr = ED.drag;
  if (dr && dr.kind === 'stroke'){   // протяжка между событиями мыши — линией без пропусков
    if (x !== dr.lx || y !== dr.ly){ meApply(meLineStamp(dr.lx, dr.ly, x, y), dr.erase); dr.lx = x; dr.ly = y; }
  } else if (dr){ dr.x = x; dr.y = y; dr.shift = e.shiftKey; }
  if (moved || dr) meOverlay();
});
function meEndDrag(e){
  const dr = ED.drag; ED.drag = null; if (!dr || !ME) return;
  if (dr.kind === 'stroke') meStep(dr.pend);
  else if (dr.kind === 'rect' || dr.kind === 'line'){
    const b = meShot();
    meApply(dr.kind === 'rect' ? meRectCells(dr.x0, dr.y0, dr.x, dr.y, dr.shift) : meLineStamp(dr.x0, dr.y0, ...meLineEnd(dr.x0, dr.y0, dr.x, dr.y, dr.shift)), false);
    meStep(b);
  } else if (dr.kind === 'selnew') ED.sel = meClip(meNorm(dr.x0, dr.y0, dr.x, dr.y));
  else if (dr.kind === 'move' && (dr.x !== dr.x0 || dr.y !== dr.y0)){
    const s = ED.sel, b = meShot(), buf = meCopy(s), dx = dr.x - dr.x0, dy = dr.y - dr.y0;
    meWipe(s); mePaste(buf, s.x0 + dx, s.y0 + dy); ED.sel = meClip({x0: s.x0 + dx, y0: s.y0 + dy, x1: s.x1 + dx, y1: s.y1 + dy}); meStep(b);
  }
  meOverlay();
}
// второй палец: начатое рисование отменяется целиком (штрих не оставляет следа)
function meCancelDrag(){ const dr = ED.drag; ED.drag = null; if (dr && dr.kind === 'stroke'){ ME = meShot(dr.pend); meDraw(); } }
const meUp = e => {
  MP.delete(e.pointerId); if (MP.size < 2) mePinch = null;
  if (!MP.size){ mePan = null; meStageEl.classList.remove('panning'); meEndDrag(e); }
};
meStageEl.addEventListener('pointerup', meUp);
meStageEl.addEventListener('pointercancel', meUp);
meStageEl.addEventListener('pointerleave', () => { if (!ED.drag){ ED.hover = null; meHoverInfo(); meOverlay(); } });
// колесо — масштаб шагом 10 % (п. 4.3.9.2) к курсору; щипок на сенсорной панели (Ctrl + колесо) — плавно
meStageEl.addEventListener('wheel', e => {
  e.preventDefault(); const r = meStageEl.getBoundingClientRect(), px = e.clientX - r.left, py = e.clientY - r.top;
  if (e.ctrlKey){ meZoomAt(Math.exp(-e.deltaY * 0.01), px, py); return; }
  const p = mePct(); meZoomTo(e.deltaY < 0 ? Math.floor(p / 10 + 1e-6) * 10 + 10 : Math.ceil(p / 10 - 1e-6) * 10 - 10, px, py);
}, {passive: false});
addEventListener('keydown', e => { if (e.code === 'Space' && ROUTE === 'workshop' && !/INPUT|TEXTAREA|SELECT/.test(e.target.tagName)){ e.preventDefault(); if (!meSpace){ meSpace = true; meStageEl.classList.add('grab'); } } });
addEventListener('keyup', e => { if (e.code === 'Space'){ meSpace = false; meStageEl.classList.remove('grab'); } });
const meZoomBtn = k => { const f = meStageEl.getBoundingClientRect(); meZoomAt(k, f.width / 2, f.height / 2); };
$('meZoomIn').onclick = () => meZoomBtn(1.25); $('meZoomOut').onclick = () => meZoomBtn(0.8); $('meZoomFit').onclick = meFit;
$('meGridBtn').onclick = () => { ED.grid = !ED.grid; try { localStorage.setItem('dnd_me_grid', ED.grid ? '1' : '0'); } catch (e) {} $('meGridBtn').setAttribute('aria-pressed', String(ED.grid)); meDraw(); };

// ---- инструменты, палитра, подсказка ----
function meSetTool(t){
  if (t === 'pick' && ED.tool !== 'pick') ED.prev = ED.tool;
  if (t !== 'select'){ ED.sel = null; }
  ED.tool = t; ED.drag = null;
  meToolsUi(); meParams(); meOverlay();
}
function meToolsUi(){
  $('meToolCol').innerHTML = ME_TOOLS.map(([k, n, key, , ic], i) => `${i === 6 ? '<div class="me-tool-sep" role="separator"></div>' : ''}<button type="button" class="me-tool${ED.tool === k ? ' on' : ''}" data-metool="${k}" aria-label="${n} (${key})" title="${n} (${key})" aria-pressed="${ED.tool === k}">${ic}<kbd>${key}</kbd></button>`).join('');
  mePalette();
}
function mePalette(){
  const q = $('meSearch').value.trim().toLowerCase(), hit = m => !q || m[2].toLowerCase().includes(q);
  const on = k => (k === 'erase' ? ED.tool === 'erase' : k === 'startA' || k === 'startB' ? ED.tool === 'start' && ED.side === (k === 'startB' ? 1 : 0) : ED.mat === k && ED.tool !== 'start');
  const btn = m => `<button type="button" class="tool${on(m[1]) ? ' on' : ''}" data-mat="${m[1]}" title="${esc(m[2])} (${m[0]})"><i style="background:${m[3]}"></i>${esc(m[2])}</button>`;
  let any = false;
  $('meTools').innerHTML = MAT_GROUPS.map(([title, g]) => { const list = MATS.filter(m => m[4] === g && hit(m)); if (!list.length) return ''; any = true; return `<section class="tool-group"><div class="caps">${title}</div><div>${list.map(btn).join('')}</div></section>`; }).join('');
  $('meNone').hidden = any;
  const fav = ED.fav.map(k => MAT[k]).filter(hit);
  $('meFav').hidden = !fav.length; $('meFavList').innerHTML = fav.map(btn).join('');
}
function meParams(){
  const t = ME_TOOLS.find(x => x[0] === ED.tool), m = MAT[ED.mat];
  $('meToolName').textContent = t ? t[1] : '';
  document.querySelectorAll('#meMapTab .me-param').forEach(el => { el.hidden = !el.dataset.for.split(' ').includes(ED.tool); });
  $('meBrush').value = ED.size; $('meBrushV').textContent = ED.size;
  document.querySelectorAll('#meShape [data-shape]').forEach(b => b.classList.toggle('on', b.dataset.shape === ED.shape));
  document.querySelectorAll('#meRectMode [data-rm]').forEach(b => b.classList.toggle('on', b.dataset.rm === ED.rect));
  document.querySelectorAll('#meSide [data-side]').forEach(b => b.classList.toggle('on', +b.dataset.side === ED.side));
  const sz = `размер ${ED.size} · ${ED.shape === 'ci' ? 'круг' : 'квадрат'}`;
  $('meHint').textContent = ED.tool === 'brush' ? `Кисть · ${m[2]} · ${sz}` : ED.tool === 'erase' ? `Ластик · ${GROUP_NAME[m[4]]} · ${sz}`
    : ED.tool === 'fill' ? `Заливка · ${m[2]}` : ED.tool === 'rect' ? `Прямоугольник · ${m[2]} · ${ED.rect === 'fill' ? 'залитый' : 'контур'} · Shift — квадрат`
    : ED.tool === 'line' ? `Линия · ${m[2]} · толщина ${ED.size} · Shift — 45°` : ED.tool === 'pick' ? 'Пипетка · щёлкните клетку'
    : ED.tool === 'select' ? 'Выделение · Ctrl+C/X/V · Shift+R поворот · Shift+H/V отражение · Del · Esc'
    : `Старты · ${ED.side ? 'B' : 'A'} · ${ME ? ME.starts[ED.side].length : 0}/${START_MAX} · Alt+щелчок — убрать`;
}
$('meToolCol').addEventListener('click', e => { const b = e.target.closest('[data-metool]'); if (b) meSetTool(b.dataset.metool); });
// материал: щелчок — активный (старты и ластик — свой инструмент); правый щелчок — в избранное (в избранном — убрать)
function meMatClick(e){
  const b = e.target.closest('[data-mat]'); if (!b) return; const k = b.dataset.mat;
  if (k === 'erase') meSetTool('erase');
  else if (k === 'startA' || k === 'startB'){ ED.side = k === 'startB' ? 1 : 0; ED.mat = k; meSetTool('start'); }
  else { ED.mat = k; if (['pick', 'select', 'start'].includes(ED.tool)) meSetTool('brush'); else { mePalette(); meParams(); } }
}
function meFavToggle(e){
  const b = e.target.closest('[data-mat]'); if (!b) return; e.preventDefault(); const k = b.dataset.mat, i = ED.fav.indexOf(k);
  if (i >= 0){ if (e.currentTarget.id === 'meFavList') ED.fav.splice(i, 1); }
  else if (ED.fav.length < 12) ED.fav.push(k); else toast('В избранном не больше 12 материалов', 3000, true);
  try { localStorage.setItem('dnd_me_fav', JSON.stringify(ED.fav)); } catch (er) {}
  mePalette();
}
['meTools', 'meFavList'].forEach(id => { $(id).addEventListener('click', meMatClick); $(id).addEventListener('contextmenu', meFavToggle); });
$('meSearch').addEventListener('input', mePalette);
document.querySelectorAll('[data-metab]').forEach(b => b.onclick = () => {
  document.querySelectorAll('[data-metab]').forEach(x => { x.classList.toggle('on', x === b); x.setAttribute('aria-selected', String(x === b)); });
  $('mePal').hidden = b.dataset.metab !== 'pal'; $('meMapTab').hidden = b.dataset.metab !== 'map';
});
$('meBrush').addEventListener('input', () => { ED.size = +$('meBrush').value; meParams(); meOverlay(); });
$('meShape').addEventListener('click', e => { const b = e.target.closest('[data-shape]'); if (b){ ED.shape = b.dataset.shape; meParams(); meOverlay(); } });
$('meRectMode').addEventListener('click', e => { const b = e.target.closest('[data-rm]'); if (b){ ED.rect = b.dataset.rm; meParams(); } });
$('meSide').addEventListener('click', e => { const b = e.target.closest('[data-side]'); if (b){ ED.side = +b.dataset.side; ED.mat = ED.side ? 'startB' : 'startA'; meParams(); mePalette(); } });

// ---- параметры карты (К-01…К-12) ----
function meShow(){
  if (!ME){ const names = Object.keys(MAPS); ME = names.length ? meFromJson(MAPS[names[0]], names[0]) : meNew(20, 14); }
  $('meName').value = ME.name; $('meW').value = ME.aw; $('meH').value = ME.ah; $('meTheme').value = ME.theme;
  $('meGridBtn').setAttribute('aria-pressed', String(ED.grid));
  meToolsUi(); meParams(); meList(); meDraw();
}
function meList(){
  const names = Object.keys(MAPS), sel = $('meList');
  sel.innerHTML = names.length ? `<option value="">— открыть карту —</option>` + names.map(n => `<option value="${esc(n)}" ${ME && n === ME.name ? 'selected' : ''}>${esc(n)} · ${MAPS[n].aw}×${MAPS[n].ah}</option>`).join('') : '<option value="">нет сохранённых карт</option>';
  $('meDel').disabled = !(ME && MAPS[ME.name]);
}
const meEmpty = () => LAYERS.every(k => ME.L[k].every(v => !v)) && !ME.starts[0].length && !ME.starts[1].length && !ME.vx.size;
$('meName').oninput = () => { ME.name = $('meName').value; $('meTitle').textContent = wsTitle(ME.name || 'Без названия', `${ME.aw}×${ME.ah}`); meChanged(); };
$('meTheme').onchange = () => { const b = meShot(); ME.theme = +$('meTheme').value; meDraw(); meStep(b); };
['meW', 'meH'].forEach(id => $(id).addEventListener('change', () => { $(id).value = Math.max(5, Math.min(100, Math.round(+$(id).value) || 5)); }));   // 5–100
$('meResize').onclick = async () => {
  const aw = Math.max(5, Math.min(100, Math.round(+$('meW').value) || 20)), ah = Math.max(5, Math.min(100, Math.round(+$('meH').value) || 14));
  $('meW').value = aw; $('meH').value = ah;
  if (aw === ME.aw && ah === ME.ah) return;
  if ((aw < ME.aw || ah < ME.ah) && !(await confirmBox(`Обрезать карту до ${aw}×${ah}? Всё, что за границей, будет удалено.`, 'Обрезать', true))){ $('meW').value = ME.aw; $('meH').value = ME.ah; return; }   // М-08
  const b = meShot(), n = meNew(aw, ah, ME.theme); n.name = ME.name;
  for (let y = 0; y < Math.min(ah, ME.ah); y++) for (let x = 0; x < Math.min(aw, ME.aw); x++) LAYERS.forEach(k => n.L[k][y * aw + x] = ME.L[k][y * ME.aw + x]);
  n.starts = ME.starts.map(l => l.filter(s => s[0] < aw && s[1] < ah).map(s => s.slice()));
  (ME.vx || new Map()).forEach((c, k) => { if (c[0] < aw && c[1] < ah) n.vx.set(k, c); });
  ME = n; ED.sel = null; meDraw(); meStep(b); meMsg('Размер изменён.');
};
$('meClear').onclick = async () => { if (!(await confirmBox('Очистить карту?', 'Очистить', true))) return; const b = meShot(), n = meNew(ME.aw, ME.ah, ME.theme); n.name = ME.name; ME = n; ED.sel = null; meDraw(); meStep(b); };
// рамка из стен (К-10): «Крепкая стена» по периметру, 1 клетка (Д-07)
$('meBorder').onclick = () => {
  const b = meShot();
  for (let x = 0; x < ME.aw; x++){ meSet(x, 0, 'strong'); meSet(x, ME.ah - 1, 'strong'); }
  for (let y = 0; y < ME.ah; y++){ meSet(0, y, 'strong'); meSet(ME.aw - 1, y, 'strong'); }
  meDraw(); meStep(b);
};
$('meRandom').onclick = async () => {
  if (!meEmpty() && !(await confirmBox('Заменить содержимое?', 'Заменить'))) return;
  const b = meShot(), r = await api({cmd:'genmap', seed: 1 + Math.floor(Math.random() * 99999), w: ME.aw, h: ME.ah, theme: ME.theme});
  if (r.type === 'map'){ ME = meFromJson(r.map, ME.name); ED.sel = null; meShow(); meStep(b); meMsg('Случайная заготовка: правьте как нужно.'); } else meMsg(r.err || 'ошибка', 'err');
};
async function meValidate(){
  const r = await api({cmd:'checkmap', map: meToJson()});
  if (r.type === 'mapcheck'){ meMsg(r.ok ? 'Карта подходит для боя.' : r.err, r.ok ? 'ok' : 'err'); return r.ok; }
  meMsg(r.err || 'ошибка', 'err'); return false;
}
$('meCheck').onclick = async () => { const ok = await meValidate(); toast($('meMsg').textContent || (ok ? 'Карта подходит для боя.' : 'Карта не подходит'), 4500, !ok); };   // итог проверки — всегда на виду
async function meSaveNow(){
  ME.name = ($('meName').value || '').trim().slice(0, 60) || 'Без названия'; $('meName').value = ME.name;
  if (!(await meValidate())) return false;
  MAPS[ME.name] = meToJson();
  try { localStorage.setItem('dnd_maps', JSON.stringify(MAPS)); } catch (e) { wsSaved($('meSavedInd'), 'bad'); meMsg('Не удалось сохранить: в браузере закончилось место.', 'err'); return false; }
  meList(); meSnap(); meDraftDrop(); meChanged(); meMsg('Сохранено: ' + ME.name, 'ok'); toast('Карта сохранена'); return true;
}
$('meSave').onclick = meSaveNow;
$('meList').addEventListener('change', async e => {
  const n = e.target.value; if (!n || !MAPS[n]) return;
  if (meDirty() && !(await confirmBox('В текущей карте есть несохранённые изменения. Открыть другую?', 'Открыть'))){ meList(); return; }
  ME = meFromJson(MAPS[n], n); ED.sel = null; meSnap(); meHistReset(); meShow(); meChanged(); meClaim();
});
$('meDel').onclick = async () => {
  if (!ME || !MAPS[ME.name] || !(await confirmBox(`Удалить карту «${ME.name}»? Это нельзя отменить.`, 'Удалить', true))) return;
  delete MAPS[ME.name]; try { localStorage.setItem('dnd_maps', JSON.stringify(MAPS)); } catch (er) {} meList(); meMsg('Карта удалена.');
};
// несохранённые изменения: снимок карты при открытии и сохранении
let meSaved = '';
const meSnap = () => { meSaved = ME ? JSON.stringify(Object.assign({n: ME.name}, meToJson())) : ''; };
const meDirty = () => !!ME && JSON.stringify(Object.assign({n: ME.name}, meToJson())) !== meSaved;
// уход из редактора карт (В-01, В-03): при несохранённых изменениях — «Сохранить / Не сохранять / Отмена»
function meLeave(to){
  if (meDirty()) wsLeaveDialog(meSaveNow, meDiscard, () => go(to)); else go(to);
}
// «Не сохранять»: карта — как сохранена (новая — пустая), черновик удаляется
async function meDiscard(){
  meDraftDrop();
  const n = ME.name; ME = MAPS[n] ? meFromJson(MAPS[n], n) : meNew(20, 14); ME.name = n; ED.sel = null;
  meSnap(); meHistReset(); meShow(); meChanged();
}
ROUTE_ENTER.workshop = prev => { meShow(); if (prev !== 'workshop' && !meSaved) meSnap(); meChanged(); meMode(); requestAnimationFrame(meFit); meDraftCheck(); meClaim(); };
ROUTE_BACK.workshop = () => meLeave('home');
document.querySelectorAll('[data-ws-exit]').forEach(b => b.onclick = () => goBack());
$('meExport').onclick = () => { const blob = new Blob([JSON.stringify(Object.assign({name: ME.name}, meToJson()), null, 1)], {type:'application/json'}), a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = (ME.name || 'map') + '.json'; a.click(); };
$('meImport').onclick = () => $('meFile').click();
$('meFile').onchange = async e => {
  const f = e.target.files[0]; e.target.value = ''; if (!f) return;
  try {
    const j = JSON.parse(await f.text());
    if (!Number.isInteger(j.aw) || !Number.isInteger(j.ah) || j.aw < 5 || j.ah < 5 || j.aw > 100 || j.ah > 100) throw new Error('размер');
    const b = meShot(); ME = meFromJson(j, String(j.name || f.name.replace(/\.json$/, '')).slice(0, 60)); ED.sel = null; meShow(); meStep(b); meMsg('Карта прочитана: проверьте и сохраните.');
  } catch (err) { toast('Не удалось открыть файл: он повреждён или имеет неизвестный формат.', 6000, true); }   // М-05
};
meList();
addEventListener('resize', () => { if (ME && ROUTE === 'workshop'){ meMode(); meFit(); } });

// ---- режим просмотра (Ф-07): окно уже 1024 px или карта открыта в другой вкладке (М-12) — только масштаб и сдвиг ----
function meMode(){
  const v = innerWidth < 1024 || ED.locked;
  if (v !== ED.view){ ED.view = v; ED.drag = null; ED.sel = null; }
  document.body.classList.toggle('me-view', ED.view);
  meOverlay();
}
// одна вкладка на карту: открывая карту, вкладка спрашивает остальные; та, что открыла раньше, отвечает — эта только смотрит
const meBC = typeof BroadcastChannel !== 'undefined' ? new BroadcastChannel('gornilo-editor') : null;
const ME_TAB = {id: Math.random().toString(36).slice(2), t: 0};
if (meBC) meBC.onmessage = e => {
  const d = e.data || {};
  if (d.q === 'who' && ROUTE === 'workshop' && !ED.locked && ME && ME.name === d.name && (ME_TAB.t < d.t || (ME_TAB.t === d.t && ME_TAB.id < d.from)))
    meBC.postMessage({a: 'mine', name: d.name, to: d.from});
  else if (d.a === 'mine' && d.to === ME_TAB.id && ME && ME.name === d.name && !ED.locked){
    ED.locked = true; meMode(); toast('Этот объект уже открыт в другой вкладке. Он открыт только для просмотра.', 6000, true);
  }
};
function meClaim(){ ED.locked = false; ME_TAB.t = Date.now(); meMode(); if (meBC && ME) meBC.postMessage({q: 'who', name: ME.name, from: ME_TAB.id, t: ME_TAB.t}); }

// ---- отмена и повтор (Ф-02, В-05, В-06): снимок карты до операции; один штрих или одна операция — один шаг; до 200 шагов
// (ТЗ — не меньше 100); история — в памяти, сбрасывается при открытии другой карты ----
const UNDO = {past: [], fut: [], max: 200};
function meShot(m = ME){ return {name: m.name, aw: m.aw, ah: m.ah, theme: m.theme, L: Object.fromEntries(LAYERS.map(k => [k, m.L[k].slice()])), starts: m.starts.map(l => l.map(c => c.slice())), vx: new Map(m.vx || [])}; }
function meSame(a, b){
  if (a.aw !== b.aw || a.ah !== b.ah || a.theme !== b.theme || a.vx.size !== b.vx.size || JSON.stringify(a.starts) !== JSON.stringify(b.starts)) return false;
  return LAYERS.every(k => { const x = a.L[k], y = b.L[k]; for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) return false; return true; });
}
// операция закончена: before — снимок до неё; ничего не изменилось (щелчок мимо) — шага нет
function meStep(before){
  if (meSame(before, meShot())) return;
  UNDO.past.push(before); if (UNDO.past.length > UNDO.max) UNDO.past.shift();
  UNDO.fut = []; meChanged();
  if (ED.tool === 'start') meParams();
}
function meRestore(t){ const n = ME.name; ME = meShot(t); ME.name = n; ED.sel = ED.sel && meClip(ED.sel); meShow(); meChanged(); }
function meUndo(){ if (!UNDO.past.length || ED.drag) return; UNDO.fut.push(meShot()); meRestore(UNDO.past.pop()); }
function meRedo(){ if (!UNDO.fut.length || ED.drag) return; UNDO.past.push(meShot()); meRestore(UNDO.fut.pop()); }
function meHistReset(){ UNDO.past = []; UNDO.fut = []; meChanged(); }
$('meUndo').onclick = meUndo; $('meRedo').onclick = meRedo;
// горячие клавиши (табл. 6, п. 4.3.3.3): не в текстовом поле и не при открытом окне; по коду клавиши — и в русской раскладке
addEventListener('keydown', e => {
  if (ROUTE !== 'workshop' || /INPUT|TEXTAREA|SELECT/.test(e.target.tagName) || document.querySelector('.modal-bg')) return;
  if (ED.view) return;
  const ctrl = e.ctrlKey || e.metaKey;
  if (ctrl && !e.altKey){
    const k = e.code;
    if (k === 'KeyZ' && !e.shiftKey) meUndo();
    else if (k === 'KeyY' || (k === 'KeyZ' && e.shiftKey)) meRedo();
    else if (k === 'KeyS') meSaveNow();
    else if (k === 'KeyC' && ED.sel) meSelOp('copy');
    else if (k === 'KeyX' && ED.sel) meSelOp('cut');
    else if (k === 'KeyV' && ED.clip) meSelOp('paste');
    else return;
    e.preventDefault(); return;
  }
  if (e.altKey) return;
  if (e.shiftKey && ED.sel && (e.code === 'KeyR' || e.code === 'KeyH' || e.code === 'KeyV')){ e.preventDefault(); meSelOp(e.code === 'KeyR' ? 'r' : e.code === 'KeyH' ? 'h' : 'v'); return; }
  if (e.code === 'Delete' && ED.sel){ e.preventDefault(); meSelOp('del'); return; }
  if (e.code === 'Escape' && ED.sel){ e.preventDefault(); e.stopImmediatePropagation(); ED.sel = null; meOverlay(); return; }   // Esc снимает выделение, а не уводит из редактора
  if (e.code === 'BracketLeft' || e.code === 'BracketRight'){ ED.size = Math.max(1, Math.min(10, ED.size + (e.code === 'BracketLeft' ? -1 : 1))); meParams(); meOverlay(); return; }
  if (e.shiftKey) return;
  const t = ME_TOOLS.find(x => x[3] === e.code); if (t){ e.preventDefault(); meSetTool(t[0]); }
});
// ---- изменения и черновик (Ф-03): индикатор В-04; черновик — в IndexedDB каждые 30 с и при уходе со вкладки, только
// при изменениях после прошлого черновика; при следующем открытии редактора — предложение восстановить (М-13) ----
const DRAFT = {dirty: false, checked: false};
function meChanged(){
  $('meUndo').disabled = !UNDO.past.length; $('meRedo').disabled = !UNDO.fut.length;
  const ind = $('meSavedInd');
  if (!meDirty()){ wsSaved(ind, 'ok'); DRAFT.dirty = false; }
  else { wsSaved(ind, 'dirty'); DRAFT.dirty = true; }
}
async function meDraftSave(){
  if (!DRAFT.dirty || !ME || !meDirty()) return;
  DRAFT.dirty = false;
  const d = {name: ME.name, map: meToJson(), at: Date.now()};
  try { await GMDB.put('drafts', 'map', d); if (!DRAFT.dirty) wsSaved($('meSavedInd'), 'draft', d.at); }
  catch (e) { DRAFT.dirty = true; console.error('черновик карты не записан:', e.message); }
}
function meDraftDrop(){ DRAFT.dirty = false; GMDB.del('drafts', 'map').catch(() => {}); }
setInterval(meDraftSave, 30000);
document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'hidden') meDraftSave(); });
addEventListener('pagehide', meDraftSave);
async function meDraftCheck(){
  if (DRAFT.checked) return; DRAFT.checked = true;
  let d = null; try { d = await GMDB.get('drafts', 'map'); } catch (e) { return; }
  if (!d || !d.map || !d.map.aw) return;
  const saved = MAPS[d.name] ? JSON.stringify(MAPS[d.name]) : '';
  if (saved && saved === JSON.stringify(d.map)){ meDraftDrop(); return; }   // черновик совпадает с сохранённой картой
  const when = new Date(d.at).toLocaleString('ru-RU', {day: 'numeric', month: 'long', hour: '2-digit', minute: '2-digit'});
  const m = modal('Черновик', `<p>Найден несохранённый черновик «${esc(d.name || 'Без названия')}» от ${esc(when)}. Восстановить?</p>`, '<button id="meDraftNo">Удалить черновик</button><button class="primary" id="meDraftYes">Восстановить</button>', {required: true});
  m.q('#meDraftNo').onclick = () => { m.close(); meDraftDrop(); };
  m.q('#meDraftYes').onclick = () => {
    m.close();
    ME = meFromJson(d.map, d.name); ED.sel = null;
    // отметка сохранённого: карта из списка, а новая карта не сохранялась вовсе ('-' не совпадёт ни с какой картой; '' вход
    // в редактор принял бы за «ещё не снимали» и счёл черновик сохранённым)
    meSaved = MAPS[d.name] ? JSON.stringify(Object.assign({n: d.name}, meToJson(meFromJson(MAPS[d.name], d.name)))) : '-';
    meHistReset(); meShow(); meChanged(); requestAnimationFrame(meFit); meClaim();
  };
}
