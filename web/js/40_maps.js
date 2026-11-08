// ====================================================================================================================
// 5. Редактор карт
// ====================================================================================================================
const LAYERS = ['wall','pit','water','diff','climb','dark','height','tree','haz','ladder','deck','door','strong'];
let MAPS = {}; try { MAPS = JSON.parse(localStorage.getItem('dnd_maps') || '{}'); } catch (e) {}
let ME = null, meTool = 'wall', meCS = 20, mePainting = 0;
const TOOLS = [['wall','Стена','#3b404d'],['pit','Ущелье','#07080c'],['water','Ручей','#2d6fa3'],['h','Высота земли (уровень)','#6f9a6f'],['ladder','Лестница','#c9a46a'],['deck','Настил / потолок (уровень)','#96703c'],['diff','Грязь','#56401e'],['climb','Укрытие','#7d7568'],['tree','Дерево','#2f6a33'],['doorC','Дверь (закрыта)','#6b4423'],['doorO','Дверь (открыта)','#a07040'],['strong','Крепкая стена','#17191f'],['z1','Лава','#e2572a'],['z2','Кислота','#7ccf2e'],['z3','Ядовитый дым','#7e8f57'],['z4','Ледяное поле','#9fd8ff'],['z5','Грозовое поле','#5868d8'],['z6','Некротический туман','#4b3a63'],['dark','Тьма','#1a1d3a'],['startA','Старт A','#2f7de1'],['startB','Старт B','#e0553f'],['erase','Ластик','#999']];
function meNew(aw, ah, theme = 0){ const L = {}; LAYERS.forEach(k => L[k] = new Uint8Array(aw * ah)); return {name: ME ? ME.name : 'Моя арена', aw, ah, theme, L, starts: [null, null]}; }
function meFromJson(j, name){
  const m = meNew(j.aw, j.ah, j.theme || 0); if (name) m.name = name;
  LAYERS.forEach(k => { for (let y = 0; y < j.ah; y++){ const row = (j[k] || [])[y] || ''; for (let x = 0; x < j.aw; x++) m.L[k][y * j.aw + x] = Math.max(0, +(row[x] || 0)) || 0; } });
  m.starts = [j.starts && j.starts[0] ? j.starts[0].slice() : null, j.starts && j.starts[1] ? j.starts[1].slice() : null];
  if (!(m.starts[0] && m.starts[1])) m.starts = [null, null];
  return m;
}
function meToJson(m = ME){
  const o = {aw: m.aw, ah: m.ah, theme: m.theme};
  LAYERS.forEach(k => { o[k] = []; for (let y = 0; y < m.ah; y++){ let r = ''; for (let x = 0; x < m.aw; x++) r += m.L[k][y * m.aw + x]; o[k].push(r); } });
  if (m.starts[0] && m.starts[1]) o.starts = m.starts;
  return o;
}
function meMsg(t, cls = ''){ $('meMsg').innerHTML = t ? `<div class="${cls || 'hint'}">${esc(t)}</div>` : ''; }
function meShow(){
  if (!ME){ const names = Object.keys(MAPS); ME = names.length ? meFromJson(MAPS[names[0]], names[0]) : meNew(20, 14); }
  $('meName').value = ME.name; $('meW').value = ME.aw; $('meH').value = ME.ah; $('meTheme').value = ME.theme;
  $('meTools').innerHTML = TOOLS.map(t => `<span class="chipb ${t[0] === meTool ? 'on' : ''}" data-tool="${t[0]}" style="border-left:8px solid ${t[2]}">${t[1]}</span>`).join('');
  $('meLegend').innerHTML = '<span style="--c:#2f7de1">старт A</span><span style="--c:#e0553f">старт B</span><span style="--c:#f2c14e">обе стартовые клетки нужны вместе, иначе места случайные</span>';
  meList(); meDraw();
}
function meList(){
  const names = Object.keys(MAPS);
  $('meList').innerHTML = names.length ? names.map(n => `<div class="row"><b>${esc(n)}</b><span class="small">${MAPS[n].aw}×${MAPS[n].ah}</span><button data-open="${esc(n)}">Открыть</button><button data-play="${esc(n)}" class="primary">Играть здесь</button><button data-del="${esc(n)}" class="danger">✕</button></div>`).join('') : '<div class="hint">Сохранённых карт нет.</div>';
  const sel = $('mapSel'), cur = sel.value;
  sel.innerHTML = '<option value="">участок мира по зерну боя</option><option value="__world">мир на выбор (место — на карте мира)</option>' + names.map(n => `<option value="${esc(n)}">моя: ${esc(n)} (${MAPS[n].aw}×${MAPS[n].ah})</option>`).join('');
  sel.value = names.includes(cur) || cur === '__world' ? cur : '';
}
function meDraw(){
  const cv2 = $('meCv'), c = cv2.getContext('2d'), m = ME;
  meCS = Math.max(6, Math.min(36, Math.floor(900 / Math.max(m.aw, m.ah))));
  cv2.width = m.aw * meCS; cv2.height = m.ah * meCS;
  const city = m.theme === 1, g = city ? [74,78,90] : [58,92,60];
  for (let y = 0; y < m.ah; y++) for (let x = 0; x < m.aw; x++){
    const i = y * m.aw + x, h = m.L.height[i], px = x * meCS, py = y * meCS, n = ((x * 7 + y * 13) % 5) * 2;
    c.fillStyle = `rgb(${g[0] + h * 18 + n},${g[1] + h * 18 + n},${g[2] + h * 18 + n})`; c.fillRect(px, py, meCS, meCS);
    if (m.L.diff[i]){ c.fillStyle = 'rgba(86,62,30,.5)'; c.fillRect(px, py, meCS, meCS); }
    if (m.L.water[i]){ c.fillStyle = '#2d6fa3'; c.fillRect(px, py, meCS, meCS); }
    if (m.L.pit[i]){ c.fillStyle = '#07080c'; c.fillRect(px, py, meCS, meCS); }
    if (m.L.wall[i]){ c.fillStyle = '#2a2d36'; c.fillRect(px, py, meCS, meCS); c.fillStyle = '#4a4f5e'; c.fillRect(px, py, meCS, Math.max(2, meCS * .3)); }
    if (m.L.climb[i]){ c.fillStyle = '#7d7568'; c.beginPath(); c.arc(px + meCS / 2, py + meCS / 2, meCS * .36, 0, 7); c.fill(); }
    if (m.L.tree[i]){ c.fillStyle = '#2f6a33'; c.beginPath(); c.arc(px + meCS / 2, py + meCS / 2, meCS * .46, 0, 7); c.fill(); c.strokeStyle = 'rgba(0,0,0,.4)'; c.lineWidth = 1; c.stroke(); }
    if (m.L.haz[i]){ c.fillStyle = ['', '#e2572a', '#7ccf2e', '#7e8f57', '#9fd8ff', '#5868d8', '#4b3a63'][m.L.haz[i]]; c.globalAlpha = .75; c.fillRect(px, py, meCS, meCS); c.globalAlpha = 1; }
    if (m.L.dark[i]){ c.fillStyle = 'rgba(6,8,28,.5)'; c.fillRect(px, py, meCS, meCS); }
    if (h){ c.strokeStyle = 'rgba(255,255,255,.35)'; c.lineWidth = 1; c.strokeRect(px + .5, py + .5, meCS - 1, meCS - 1); if (meCS >= 16){ c.fillStyle = 'rgba(255,255,255,.7)'; c.font = `700 ${Math.round(meCS * .38)}px system-ui`; c.textAlign = 'right'; c.textBaseline = 'top'; c.fillText(h, px + meCS - 2, py + 1); } }
    if (m.L.door[i]){ c.fillStyle = m.L.door[i] === 1 ? '#6b4423' : 'rgba(160,112,64,.55)'; c.fillRect(px + meCS * .2, py + meCS * .2, meCS * .6, meCS * .6); }
    if (m.L.strong[i]){ c.fillStyle = '#17191f'; c.fillRect(px, py, meCS, meCS); c.fillStyle = '#565c6c'; c.fillRect(px, py, meCS, Math.max(2, meCS * .3)); }
    if (m.L.ladder[i]){ c.strokeStyle = '#c9a46a'; c.lineWidth = 2; c.beginPath(); c.moveTo(px + meCS * .3, py + 2); c.lineTo(px + meCS * .3, py + meCS - 2); c.moveTo(px + meCS * .7, py + 2); c.lineTo(px + meCS * .7, py + meCS - 2); for (let k = 1; k <= 3; k++){ c.moveTo(px + meCS * .3, py + meCS * k / 4); c.lineTo(px + meCS * .7, py + meCS * k / 4); } c.stroke(); }
    if (m.L.deck[i]){ c.fillStyle = 'rgba(150,112,60,.5)'; c.fillRect(px, py, meCS, meCS); c.strokeStyle = 'rgba(240,200,120,.9)'; c.lineWidth = 1; c.strokeRect(px + 1.5, py + 1.5, meCS - 3, meCS - 3); if (meCS >= 16){ c.fillStyle = '#ffe2a0'; c.font = `700 ${Math.round(meCS * .34)}px system-ui`; c.textAlign = 'left'; c.textBaseline = 'bottom'; c.fillText('▲' + m.L.deck[i], px + 2, py + meCS - 1); } }
  }
  if (meCS >= 10){ c.strokeStyle = 'rgba(255,255,255,.07)'; c.beginPath(); for (let x = 0; x <= m.aw; x++){ c.moveTo(x * meCS + .5, 0); c.lineTo(x * meCS + .5, m.ah * meCS); } for (let y = 0; y <= m.ah; y++){ c.moveTo(0, y * meCS + .5); c.lineTo(m.aw * meCS, y * meCS + .5); } c.stroke(); }
  [['A', '#2f7de1'], ['B', '#e0553f']].forEach(([l, col], k) => { const st = m.starts[k]; if (!st) return; c.fillStyle = col; c.beginPath(); c.arc((st[0] + .5) * meCS, (st[1] + .5) * meCS, Math.max(5, meCS * .42), 0, 7); c.fill(); c.strokeStyle = '#fff'; c.lineWidth = 2; c.stroke(); c.fillStyle = '#fff'; c.font = `800 ${Math.max(9, meCS * .5)}px system-ui`; c.textAlign = 'center'; c.textBaseline = 'middle'; c.fillText(l, (st[0] + .5) * meCS, (st[1] + .5) * meCS + 1); });
}
function mePaint(e, erase){
  const r = $('meCv').getBoundingClientRect(), m = ME;
  const cx = Math.floor((e.clientX - r.left) * ($('meCv').width / r.width) / meCS), cy = Math.floor((e.clientY - r.top) * ($('meCv').height / r.height) / meCS);
  const b = +$('meBrush').value, off = Math.floor((b - 1) / 2);
  for (let dy = 0; dy < b; dy++) for (let dx = 0; dx < b; dx++){
    const x = cx - off + dx, y = cy - off + dy; if (x < 0 || y < 0 || x >= m.aw || y >= m.ah) continue;
    const i = y * m.aw + x, t = meTool, L = m.L;
    if (t === 'startA' || t === 'startB'){ if (!erase) { m.starts[t === 'startA' ? 0 : 1] = [x, y]; L.wall[i] = 0; L.pit[i] = 0; } else if (m.starts[t === 'startA' ? 0 : 1]) m.starts[t === 'startA' ? 0 : 1] = null; continue; }
    if (t === 'erase'){ LAYERS.forEach(k => L[k][i] = 0); m.starts = m.starts.map(s => s && s[0] === x && s[1] === y ? null : s); continue; }
    if (/^z\d$/.test(t)){ if (erase) L.haz[i] = 0; else if (!L.wall[i] && !L.pit[i]) L.haz[i] = +t[1]; continue; }
    const lv = +$('meLevel').value || 0;
    if (t === 'h'){ if (erase) L.height[i] = 0; else if (!L.pit[i] && !L.wall[i]){ L.height[i] = lv; if (L.deck[i] && L.deck[i] <= lv) L.deck[i] = 0; } continue; }
    if (t === 'doorC' || t === 'doorO'){ if (erase) L.door[i] = 0; else if (!L.wall[i] && !L.pit[i]){ L.door[i] = t === 'doorC' ? 1 : 2; L.tree[i] = L.climb[i] = 0; } continue; }
    if (t === 'strong'){ if (erase) L.strong[i] = 0; else { L.wall[i] = 1; L.strong[i] = 1; L.door[i] = 0; L.tree[i] = 0; } continue; }
    if (t === 'deck'){ if (erase) L.deck[i] = 0; else if (!L.wall[i] && lv > L.height[i]) L.deck[i] = lv; continue; }
    if (erase){ L[t][i] = 0; continue; }
    if (t === 'wall'){ L.wall[i] = 1; L.pit[i] = 0; L.water[i] = 0; L.climb[i] = 0; L.height[i] = 0; L.tree[i] = 0; }
    else if (t === 'tree'){ if (!L.wall[i] && !L.pit[i] && !L.water[i]){ L.tree[i] = 1; L.climb[i] = 0; } }
    else if (t === 'pit'){ L.pit[i] = 1; L.wall[i] = 0; L.water[i] = 0; L.height[i] = 0; L.climb[i] = 0; }
    else if (t === 'water'){ L.water[i] = 1; L.wall[i] = 0; L.pit[i] = 0; }
    else if (t === 'climb'){ if (!L.wall[i] && !L.pit[i]){ L.climb[i] = 1; L.tree[i] = 0; } }
    else L[t][i] = 1;
  }
  meDraw();
}
$('meTools').addEventListener('click', e => { const c = e.target.closest('[data-tool]'); if (c){ meTool = c.dataset.tool; meShow(); } });
const meCvEl = $('meCv');
meCvEl.addEventListener('contextmenu', e => e.preventDefault());
meCvEl.addEventListener('pointerdown', e => { mePainting = e.button === 2 ? 2 : 1; meCvEl.setPointerCapture(e.pointerId); mePaint(e, mePainting === 2); });
meCvEl.addEventListener('pointermove', e => { if (mePainting) mePaint(e, mePainting === 2); });
meCvEl.addEventListener('pointerup', () => { mePainting = 0; });
$('meName').oninput = () => { ME.name = $('meName').value; };
$('meTheme').onchange = () => { ME.theme = +$('meTheme').value; meDraw(); };
$('meResize').onclick = () => {
  const aw = Math.max(5, Math.min(200, +$('meW').value || 20)), ah = Math.max(5, Math.min(200, +$('meH').value || 14)), n = meNew(aw, ah, ME.theme); n.name = ME.name;
  for (let y = 0; y < Math.min(ah, ME.ah); y++) for (let x = 0; x < Math.min(aw, ME.aw); x++) LAYERS.forEach(k => n.L[k][y * aw + x] = ME.L[k][y * ME.aw + x]);
  n.starts = ME.starts.map(s => s && s[0] < aw && s[1] < ah ? s : null); if (!(n.starts[0] && n.starts[1])) n.starts = [null, null];
  ME = n; meDraw(); meMsg('Размер изменён.');
};
$('meClear').onclick = () => { const n = meNew(ME.aw, ME.ah, ME.theme); n.name = ME.name; ME = n; meDraw(); };
$('meBorder').onclick = () => { for (let x = 0; x < ME.aw; x++){ ME.L.wall[x] = 1; ME.L.wall[(ME.ah - 1) * ME.aw + x] = 1; } for (let y = 0; y < ME.ah; y++){ ME.L.wall[y * ME.aw] = 1; ME.L.wall[y * ME.aw + ME.aw - 1] = 1; } meDraw(); };
$('meRandom').onclick = async () => {
  const r = await api({cmd:'genmap', seed: 1 + Math.floor(Math.random() * 99999), w: +$('meW').value || 0, h: +$('meH').value || 0, theme: +$('meTheme').value});
  if (r.type === 'map'){ ME = meFromJson(r.map, ME.name); meShow(); meMsg('Случайная заготовка: правьте как нужно.'); } else meMsg(r.err || 'ошибка', 'err');
};
async function meValidate(){
  const r = await api({cmd:'checkmap', map: meToJson()});
  if (r.type === 'mapcheck'){ meMsg(r.ok ? 'Карта подходит для боя.' : r.err, r.ok ? 'ok' : 'err'); return r.ok; }
  meMsg(r.err || 'ошибка', 'err'); return false;
}
$('meCheck').onclick = meValidate;
$('meSave').onclick = async () => {
  ME.name = ($('meName').value || 'Моя арена').trim() || 'Моя арена';
  if (!(await meValidate())) return;
  MAPS[ME.name] = meToJson(); try { localStorage.setItem('dnd_maps', JSON.stringify(MAPS)); } catch (e) {}
  meList(); meMsg('Сохранено: ' + ME.name, 'ok');
};
$('meList').addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return;
  if (b.dataset.open){ ME = meFromJson(MAPS[b.dataset.open], b.dataset.open); meShow(); }
  else if (b.dataset.play){ $('mapSel').value = b.dataset.play; showTab(2); }
  else if (b.dataset.del && confirm('Удалить карту «' + b.dataset.del + '»?')){ delete MAPS[b.dataset.del]; try { localStorage.setItem('dnd_maps', JSON.stringify(MAPS)); } catch (er) {} meList(); }
});
$('meExport').onclick = () => { const blob = new Blob([JSON.stringify(Object.assign({name: ME.name}, meToJson()), null, 1)], {type:'application/json'}), a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = (ME.name || 'map') + '.json'; a.click(); };
$('meImport').onclick = () => $('meFile').click();
$('meFile').onchange = async e => { const f = e.target.files[0]; if (!f) return; try { const j = JSON.parse(await f.text()); ME = meFromJson(j, j.name || f.name.replace(/\.json$/, '')); meShow(); meMsg('Карта прочитана: проверьте и сохраните.'); } catch (err) { meMsg('Не удалось прочитать: ' + err.message, 'err'); } };
meList();
