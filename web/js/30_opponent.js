// ====================================================================================================================
// 2. Соперник
// ====================================================================================================================
function oppCfg(){ return {mode: $('oppMode').value, mul: +$('oppMul').value, brain: $('oppBrain').value}; }
let prevKey = '';
async function loadPreview(){
  const key = JSON.stringify([S.budget, $('seed').value, oppCfg()]);
  if (key === prevKey) return; prevKey = key;
  $('oppCard').innerHTML = '<h3>Соперник</h3><div class="hint">Собираем…</div>';
  const r = await api({cmd:'preview', budget:S.budget, seed:+$('seed').value, opp:oppCfg()});
  if (r.type !== 'preview'){ $('oppCard').innerHTML = `<h3>Соперник</h3><div class="err">${esc(r.err || 'ошибка')}</div>`; return; }
  const b = r.boss;
  $('oppCard').innerHTML = `<h3><span class="chip B">враг</span>${esc(b.title)}</h3><div class="hint">Собран: ${esc(r.label)} · ${b.budget} очков · ИНТ ${b.int} · ${b.hp} ХП · КД ${b.ac} · скорость ${b.speed} фт</div>
    <div class="items" style="max-height:520px">${b.items.map(it => `<div class="item${it.cost < 0 ? ' neg' : ''}" style="--c:${(CATS[it.cat] || CATS.passive).c}"><span>${(CATS[it.cat] || CATS.passive).ic}</span><div><div>${esc(it.label)}</div>${it.detail ? `<div class="dt">${esc(it.detail)}</div>` : ''}</div><span class="co tnum">${it.cost > 0 ? '−' : '+'}${Math.abs(it.cost)}</span></div>`).join('')}</div>`;
}
['oppMode','oppMul','seed'].forEach(id => $(id).addEventListener('change', loadPreview));
// ---- мир на выбор: зерно, размер, место боя на карте мира ----
const BIOME_COL = {a:'#b9c3c9', b:'#3f6b4f', c:'#e8f0f6', d:'#b5a76a', e:'#5d9a4e', f:'#4a6b4a', g:'#d9c27a', h:'#a8b25a', i:'#2f7d3a', '~':'#2d6fa3', V:'#07080c', '#':'#555a66', '+':'#a08a6a', '=':'#c8b08a'};
const BIOME_NAMES = [['a','тундра'],['b','тайга'],['c','ледник'],['d','степь'],['e','лес и поля'],['f','болото'],['g','пустыня'],['h','саванна'],['i','джунгли'],['~','вода'],['#','стены'],['+','поселение'],['=','дорога']];
let WMAP = null, WPICK = null;
$('wSize').innerHTML = [1000,2000,3000,5000,7500,10000].map(v => `<option value="${v}">${v}×${v} (~${(v * 5 * 0.3048 / 1000).toFixed(1)} км)</option>`).join('');
$('mapSel').addEventListener('change', () => { const on = $('mapSel').value === '__world'; $('worldBox').style.display = on ? '' : 'none'; $('worldCard').style.display = on ? '' : 'none'; if (on && !WMAP) showWorld(); });
$('wDice').onclick = () => { $('wSeed').value = String(1 + Math.floor(Math.random() * 1e9)); WPICK = null; showWorld(); };
$('wShow').onclick = showWorld;
['wSize'].forEach(id => $(id).addEventListener('change', () => { WPICK = null; showWorld(); }));
['wW','wH'].forEach(id => $(id).addEventListener('change', drawWorld));
async function showWorld(){
  $('wPick').textContent = 'строим мир…';
  const r = await api({cmd:'worldmap', seed: $('wSeed').value.trim() || '1', size: +$('wSize').value});
  if (r.type !== 'worldmap'){ $('wPick').textContent = r.err || 'ошибка'; return; }
  WMAP = r; drawWorld();
  $('wLegend').innerHTML = BIOME_NAMES.map(([k, n]) => `<span style="--c:${BIOME_COL[k]}">${n}</span>`).join('');
}
function drawWorld(){
  if (!WMAP) return;
  const cv2 = $('wCv'), n = WMAP.n, px = Math.max(2, Math.floor(Math.min(900, cv2.parentElement.clientWidth - 30) / n));
  cv2.width = n * px; cv2.height = n * px;
  const c = cv2.getContext('2d');
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++){
    const b = WMAP.biome[j][i], h = +WMAP.height[j][i];
    c.fillStyle = BIOME_COL[b] || '#888'; c.fillRect(i * px, j * px, px, px);
    if (h && !'~#+V='.includes(b)){ c.fillStyle = `rgba(255,255,255,${h * .035})`; c.fillRect(i * px, j * px, px, px); }
  }
  const k = px / WMAP.step;
  WMAP.towns.forEach(t => { c.strokeStyle = t[3] === 2 ? '#ff6b6b' : '#ffe2a0'; c.lineWidth = 1.5; c.beginPath(); c.arc(t[0] * k, t[1] * k, Math.max(3, t[2] * k), 0, 7); c.stroke(); });
  if (WPICK){ c.strokeStyle = '#fff'; c.lineWidth = 2; c.strokeRect(WPICK[0] * k, WPICK[1] * k, Math.max(3, +$('wW').value * k), Math.max(3, +$('wH').value * k)); }
  $('wPick').textContent = WPICK ? `место: клетка ${WPICK[0]}, ${WPICK[1]} (из ${WMAP.size})` : 'место: случайное — щёлкните по карте мира';
}
$('wCv').addEventListener('click', e => {
  if (!WMAP) return;
  const r = $('wCv').getBoundingClientRect(), k = ($('wCv').width / r.width) / ($('wCv').width / WMAP.n) * WMAP.step;
  const aw = +$('wW').value, ah = +$('wH').value;
  const x = Math.round((e.clientX - r.left) * k - aw / 2), y = Math.round((e.clientY - r.top) * k - ah / 2);
  WPICK = [Math.max(0, Math.min(WMAP.size - aw, x)), Math.max(0, Math.min(WMAP.size - ah, y))]; drawWorld();
});
$('btnDice').onclick = () => { $('seed').value = 1 + Math.floor(Math.random() * 99999); loadPreview(); };
