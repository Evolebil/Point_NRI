// значения из состояния в поля формы
function syncForm(){
  document.querySelectorAll('#form [data-p]').forEach(el => {
    const v = PATHS(S, el.dataset.p), t = el.dataset.t;
    if (t === 'bool') el.checked = !!v;
    else if (t === 'str') { if (el !== document.activeElement) el.value = v == null ? '' : v; }
    else if (el.tagName === 'SELECT') { el.value = String(v == null ? (el.options[0] || {}).value : v); if (v == null && el.dataset.dt && el.options[0]) setPath(S, el.dataset.p, +el.options[0].value); }
    else if (el.dataset.inf) { if (el !== document.activeElement) el.value = +v > 0 ? v : ''; }
    else if (el !== document.activeElement || el.type === 'range') el.value = v == null ? 0 : v;
  });
  document.querySelectorAll('#form [data-chips]').forEach(box => {
    const arr = PATHS(S, box.dataset.chips) || [];
    box.querySelectorAll('.chipb').forEach(c => c.classList.toggle('on', arr.includes(+c.dataset.i)));
  });
  document.querySelectorAll('#form [data-main]').forEach(r => r.checked = (S.main == null ? 0 : S.main) === +r.dataset.main);
  const w = S.weapon || {}; const armed = !!w.armed;
  document.querySelectorAll('#wpn input,#wpn select').forEach(el => { if (el.dataset.p !== 'weapon.armed') el.disabled = !armed; });
  STAT_N.forEach((n, i) => { const v = (S.stats || [])[i] || 10, el = document.querySelector(`[data-statcost="${i}"]`); if (el) { const c = v * (v + 1) / 2 - 55; el.textContent = `мод ${Math.floor((v - 10) / 2) >= 0 ? '+' : ''}${Math.floor((v - 10) / 2)} · ${c > 0 ? '−' + c : (c < 0 ? '+' + (-c) : '0')} оч.`; el.style.color = c < 0 ? 'var(--good)' : ''; } });
  const acv = S.acV || 0; const ae = document.querySelector('[data-accost]'); if (ae) ae.textContent = `${acv * (2 * acv + 1)} оч.`;   // (2a)(2a+1)/2
  const av = document.querySelector('[data-acval]'); if (av) av.textContent = lastPrice && lastPrice.boss ? `итого КД ${lastPrice.boss.ac}` : `КД ${10 + acv} + ЛОВ`;
  $('bud').textContent = S.budget;
}
function onInput(e){
  const el = e.target;
  if (el.dataset.p){
    const t = el.dataset.t; let v;
    if (t === 'bool') v = el.checked; else if (t === 'str') v = el.value; else if (t === 'num') v = num(el.value, 0); else v = parseInt(el.value, 10);
    if (el.dataset.p === 'budget'){
      v = Math.round(+el.value);   // любое целое в допустимых пределах; пока число набирается и вне пределов — ждём
      if (!Number.isFinite(v) || v < (CAT.minBudget || 20) || v > (CAT.maxBudget || 100000) || v === S.budget) return;
      S.budget = v; loadCatalog().then(() => { syncForm(); queuePrice(); }); save(); return;
    }
    setPath(S, el.dataset.p, v);
    if (el.dataset.p.endsWith('_kind') || /^abilities\.\d+\.(ground|shape)$/.test(el.dataset.p)) renderAbilities();
    if (/^passives\.rules\.\d+\.(when|what)$/.test(el.dataset.p)) renderRules();
    if (/^passives\.trigs\.\d+\.(on|what)$/.test(el.dataset.p)) renderTrigs();
    save(); syncForm(); queuePrice();
  } else if (el.dataset.main != null){ S.main = +el.dataset.main; save(); queuePrice(); }
}
$('form').addEventListener('input', e => {
  const q = e.target.dataset && e.target.dataset.chipsearch;
  if (q == null) return;
  const t = e.target.value.trim().toLowerCase(), box = document.querySelector(`[data-chipscope="${q}"]`);
  if (!box) return;
  box.querySelectorAll('.chipb').forEach(c => c.style.display = !t || c.textContent.toLowerCase().includes(t) ? '' : 'none');
  box.querySelectorAll('.chips').forEach(ch => { const vis = [...ch.children].some(c => c.style.display !== 'none'), hd = ch.previousElementSibling;
    ch.style.display = vis ? '' : 'none'; if (hd && hd.classList.contains('chip-g')) hd.style.display = vis ? '' : 'none'; });
});
$('form').addEventListener('input', onInput);
$('form').addEventListener('change', onInput);
$('form').addEventListener('click', e => {
  const chip = e.target.closest('.chipb');
  if (chip){
    const path = chip.parentElement.dataset.chips, arr = PATHS(S, path) || []; const i = +chip.dataset.i, k = arr.indexOf(i);
    if (k >= 0) arr.splice(k, 1); else arr.push(i);
    setPath(S, path, arr); save(); syncForm(); queuePrice(); return;
  }
  const tdel = e.target.closest('[data-tdel]');
  if (tdel){ S.passives.trigs.splice(+tdel.dataset.tdel, 1); save(); renderTrigs(); queuePrice(); return; }
  const tp = e.target.closest('[data-tpreset]');
  if (tp){ addPreset('trig', tp.dataset.tpreset); return; }
  const rdel = e.target.closest('[data-rdel]');
  if (rdel){ S.passives.rules.splice(+rdel.dataset.rdel, 1); save(); renderRules(); queuePrice(); return; }
  const rp = e.target.closest('[data-rpreset]');
  if (rp){ addPreset('rule', rp.dataset.rpreset); return; }
  // описание из механики: деталь способности (оружия) из разбора цены, без цены; запоминаем, по какой механике собрано
  const mk = e.target.closest('[data-mkdesc]'), mkw = e.target.closest('[data-mkwdesc]');
  if (mk || mkw){
    const det = mkw ? mechWeapon() : mechAbil(+mk.dataset.mkdesc); if (!det) return;
    const o = mkw ? S.weapon : S.abilities[+mk.dataset.mkdesc];
    o._desc = det.charAt(0).toUpperCase() + det.slice(1); o._descMech = det;
    save(); syncForm(); renderPrice(); return;
  }
  if (e.target.closest('[data-altadd]')){ (S.altWeapons = S.altWeapons || []).push({n:1, s:6, type:7, att:1, range:80, reach:5, ammo:true}); save(); buildForm(); queuePrice(); return; }
  const adel = e.target.closest('[data-altdel]');
  if (adel){ S.altWeapons.splice(+adel.dataset.altdel, 1); save(); buildForm(); queuePrice(); return; }
  const del = e.target.closest('[data-del]');
  if (del){
    S.abilities.splice(+del.dataset.del, 1);
    [...OPEN].forEach(k => { if (k.startsWith('abilities.')) OPEN.delete(k); });   // номера способностей сдвинулись
    save(); renderAbilities(); queuePrice(); return;
  }
  const pr = e.target.closest('[data-preset]');
  if (pr){ addPreset('abil', pr.dataset.preset); return; }
  const sga = e.target.closest('[data-sgadd]'), sgd = e.target.closest('[data-sgdel]'), ms = e.target.closest('[data-minisave]');
  if (sga){ const a = S.abilities[+sga.dataset.sgadd]; a.groups = a.groups || {}; a.groups[Date.now()] = {m:'Скелет', n:1}; save(); renderAbilities(); queuePrice(); return; }
  if (sgd){ const [ai, g] = sgd.dataset.sgdel.split(':'); delete S.abilities[+ai].groups[g]; save(); renderAbilities(); queuePrice(); return; }
  if (ms){
    const c = toChar(); c.abilities = c.abilities.filter(x => !x.summon && !x.form);
    const nm = prompt('Имя в библиотеке приспешников:', S.name || 'Приспешник'); if (!nm) return;
    MINI_USER[nm] = c; try { localStorage.setItem('dnd_minions', JSON.stringify(MINI_USER)); } catch (er) {}
    renderAbilities(); toast('«' + nm + '» — в библиотеке приспешников'); return;
  }
  const ga = e.target.closest('[data-gadd]'), gd = e.target.closest('[data-gdel]');
  if (ga || gd){   // группа эффектов: добавить (с начальными значениями) или убрать (обнулить)
    const key = (ga || gd).dataset[ga ? 'gadd' : 'gdel'], r = GREG[key]; if (!r) return;
    if (ga){ OPEN.add(key); if (r.g.init && !(r.g.has && r.g.has(r.obj))) r.g.init(r.obj); }
    else { OPEN.delete(key); if (r.g.clear) r.g.clear(r.obj); }
    save(); r.rer(); queuePrice(); return;
  }
  const sb = e.target.closest('[data-step]');
  if (sb){ goStep(+sb.dataset.step); return; }
  if (e.target.closest('[data-tab2]')) showTab(2);
});
$('form').addEventListener('change', e => {   // заготовки из выпадающего списка
  const el = e.target.closest('[data-presetsel]');
  if (el && el.value !== ''){ const v = el.value; el.value = '';
    if (el.dataset.presetsel === 'char'){ if (confirm('Готовый персонаж заменит всё, что вы собрали (бюджет останется вашим). Продолжить?')) applyCharPreset(v); }
    else addPreset(el.dataset.presetsel, v); return; }
  if (e.target.id === 'simpleMode'){ SIMPLE = e.target.checked; try { localStorage.setItem('dnd_simple', SIMPLE ? '1' : '0'); } catch (x) {} buildForm(); goStep(STEP); }
});
// заготовки оружия (как в D&D): меняют только урон, дальность и свойства; число атак, владение и описание остаются
const WEAPON_PRESETS = [
  {n:'Кинжал', d:[1,4,'колющ'], f:{finesse:true, light:true, range:5, long:0}},
  {n:'Короткий меч', d:[1,6,'колющ'], f:{finesse:true, light:true}},
  {n:'Рапира', d:[1,8,'колющ'], f:{finesse:true}},
  {n:'Длинный меч', d:[1,8,'рубящ'], f:{versatile:10}},
  {n:'Боевой топор', d:[1,8,'рубящ'], f:{versatile:10}},
  {n:'Боевой молот', d:[1,8,'дробящ'], f:{versatile:10}},
  {n:'Булава', d:[1,6,'дробящ'], f:{}},
  {n:'Копьё', d:[1,6,'колющ'], f:{versatile:8}},
  {n:'Двуручный меч', d:[2,6,'рубящ'], f:{twoHanded:true, heavy:true}},
  {n:'Секира', d:[1,12,'рубящ'], f:{twoHanded:true, heavy:true}},
  {n:'Алебарда', d:[1,10,'рубящ'], f:{twoHanded:true, heavy:true, reach:10}},
  {n:'Короткий лук', d:[1,6,'колющ'], f:{ammo:true, twoHanded:true, range:80, long:320}},
  {n:'Длинный лук', d:[1,8,'колющ'], f:{ammo:true, twoHanded:true, heavy:true, range:150, long:500}},
  {n:'Лёгкий арбалет', d:[1,8,'колющ'], f:{ammo:true, twoHanded:true, range:80, long:320, att:1}},
];
function addPreset(kind, k){
  S.passives = S.passives || {};
  if (kind === 'wpn'){
    const P = WEAPON_PRESETS[+k], w = S.weapon = S.weapon || {}, ti = CAT.dtypes.findIndex(x => x.startsWith(P.d[2]));
    Object.assign(w, {armed:true, n:P.d[0], s:P.d[1], finesse:false, light:false, twoHanded:false, heavy:false, ammo:false, versatile:0, range:5, long:0, reach:5}, P.f);
    if (ti >= 0) w.type = ti;
    save(); syncForm(); renderSteps(); queuePrice(); return;
  }
  if (kind === 'abil'){
    const a = k === 'blank' ? {_kind:'attack', range:5, dmgN:1, dmgS:6, dtype:3} : JSON.parse(JSON.stringify(ABIL_PRESETS[+k].a));
    (S.abilities = S.abilities || []).push(a); save(); renderAbilities();
  } else if (kind === 'rule'){
    S.passives.rules = S.passives.rules || [];
    const add = k === 'blank' ? [{when:0, what:0, n:1, s:6}] : RULE_PRESETS[+k].r;
    add.forEach(r => S.passives.rules.push(Object.assign({warg:1, n:1, s:6, once:false}, JSON.parse(JSON.stringify(r)))));
    save(); renderRules();
  } else {
    S.passives.trigs = S.passives.trigs || [];
    const add = k === 'blank' ? [{on:1, what:1, n:1, s:6}] : TRIG_PRESETS[+k].t;
    add.forEach(t => S.passives.trigs.push(Object.assign({n:1, s:6, b:0, cond:1, uses:0}, JSON.parse(JSON.stringify(t)))));
    save(); renderTrigs();
  }
  renderSteps(); queuePrice();
}
function save(){
  try { localStorage.setItem('dnd_char', JSON.stringify(S)); } catch (e) {}
  // копия на сервере под именем игрока: адрес туннеля меняется — браузер «забывает», сервер помнит
  clearTimeout(save.t); save.t = setTimeout(() => { const n = playerName(false); if (n) api({cmd:'player_save', name:n, s:S}); }, 1500);
}
function playerName(ask){   // имя — из строки «Вы: …» под меню (05_name.js); не задано и нужно — подсветить её
  let n = ''; try { n = localStorage.getItem('dnd_rmname') || ''; } catch (e) {}
  if (!n && ask && $('nameIn')) $('nameIn').focus();
  return n;
}

async function loadCatalog(){
  const r = await api({cmd:'catalog', budget:S.budget});
  if (r.type === 'catalog') CAT = r.data;
  return CAT;
}
function queuePrice(){ clearTimeout(priceTimer); priceTimer = setTimeout(doPrice, 140); }
async function doPrice(){
  const seq = ++priceSeq;
  const r = await api({cmd:'price', char: toChar()});
  if (seq !== priceSeq) return;
  if (r.type !== 'price'){ $('verdict').innerHTML = `<div class="err">${esc(r.err || 'ошибка')}</div>`; return; }
  lastPrice = r.data; renderPrice(); syncForm(); renderSteps();
}
// механика способности i и оружия словами движка (деталь из разбора цены, без «-- N очк.»)
function mechAbil(i){
  const b = lastPrice && lastPrice.boss; if (!b) return '';
  const it = b.items.filter(x => x.cat === 'ability' && x.label !== 'Наценка за число способностей')[i];
  return it ? String(it.detail || '').replace(/\s*--\s*\d+\s*очк\.?\s*$/, '').trim() : '';
}
function mechWeapon(){
  const b = lastPrice && lastPrice.boss; if (!b) return '';
  const it = b.items.find(x => x.cat === 'weapon' && /оружие/i.test(x.label) && !/Запасное|мультиатаки|Дальняя|Двуруч|Универс|Без владения/.test(x.label));
  return it ? String(it.detail || '') : '';
}
// бюджет по частям: покупки, возврат за недостатки (характеристики ниже 10, срезанная ЛОВ, отрицательные особенности) и остаток в здоровье
function budgetLine(b){
  if (!b) return '';
  let pos = 0, neg = 0, hp = 0;
  b.items.forEach(it => { if (it.cat === 'hp') hp += it.cost; else if (it.cost > 0) pos += it.cost; else neg -= it.cost; });
  const left = S.budget - pos + neg - hp;
  return `<div class="small tnum">Бюджет ${S.budget} = покупки ${pos}${neg ? ` − возврат за недостатки ${neg}` : ''} + здоровье ${hp} (${b.hp} ХП)${left ? ` + остаток ${left} (меньше 1 ХП)` : ''}</div>`;
}
function renderPrice(){
  const d = lastPrice, b = d.boss;
  $('spent').textContent = d.spent != null ? d.spent : '—';
  $('leftLbl').textContent = d.left != null ? (d.left >= 0 ? 'осталось ' + d.left : 'перерасход ' + (-d.left)) : '';
  const seg = {};
  if (b) b.items.forEach(it => seg[it.cat] = (seg[it.cat] || 0) + Math.max(0, it.cost));
  $('meter').innerHTML = Object.keys(CATS).filter(k => seg[k] > 0).map(k => `<i title="${CATS[k].n}: ${seg[k]}" style="width:${100 * seg[k] / S.budget}%;background:${CATS[k].c}"></i>`).join('');
  // всё непотраченное уходит в хиты: показать, на сколько хитов изменилась последняя покупка
  if (renderPrice.last !== d){   // новый разбор цены (renderPrice зовут и повторно с тем же разбором)
    renderPrice.dHp = d.ok && renderPrice.hp != null && renderPrice.who === S.name ? b.hp - renderPrice.hp : 0;
    if (d.ok){ renderPrice.hp = b.hp; renderPrice.who = S.name; }
    renderPrice.last = d;
  }
  const dHp = renderPrice.dHp || 0;
  const big = (v, n, t) => `<div style="text-align:center;min-width:70px" title="${t}"><div class="big tnum" style="font-size:24px">${v}</div><div class="small">${n}</div></div>`;
  $('verdict').innerHTML = d.ok ? `<div class="small" style="margin-top:6px">Персонаж собран — можно в бой или в кампанию:</div>
    <div style="display:flex;gap:6px;margin:4px 0">${big(b.hp, 'ХП', 'Хиты: здоровье')}${big(b.ac, 'КД', 'Класс доспеха')}${big(b.speed + ' фт', 'скорость', 'Шагов по 5 фт за ход')}</div>
    ${dHp ? `<div class="small"><b style="color:${dHp > 0 ? 'var(--good)' : 'var(--bad)'}">${dHp > 0 ? '+' : ''}${dHp} ХП: ${dHp > 0 ? 'освободилось' : 'потрачено'} ${Math.abs(2 * dHp)} оч.</b></div>` : ''}
    <div class="small">Всё, что не потрачено, уходит в хиты: 2 очка = 1 ХП.</div>${budgetLine(b)}` : `<div class="err">${esc(d.err)}</div>`;
  $('toOpp').disabled = !d.ok;
  weaponSummary();
  $('items').innerHTML = b ? b.items.map(it => `<div class="item${it.cost < 0 ? ' neg' : ''}" style="--c:${(CATS[it.cat] || CATS.passive).c}"><span>${(CATS[it.cat] || CATS.passive).ic}</span><div><div>${esc(it.label)}</div>${it.detail ? `<div class="dt">${esc(it.detail)}</div>` : ''}</div><span class="co tnum">${it.cost > 0 ? '−' : '+'}${Math.abs(it.cost)}</span></div>`).join('') : '';
  // цена каждой способности рядом с её редактором
  const ab = b ? b.items.filter(it => it.cat === 'ability' && it.label !== 'Наценка за число способностей') : [];
  document.querySelectorAll('[data-abprice]').forEach(el => { const it = ab[+el.dataset.abprice]; el.textContent = it ? it.cost + ' оч.' : ''; });
  { const oh = b ? b.items.find(it => it.cat === 'ability' && /атака второй рукой/.test(it.detail || '')) : null, el = document.querySelector('[data-ohprice]'); if (el) el.textContent = oh ? oh.cost + ' оч.' : ''; }
  { const alts = b ? b.items.filter(it => it.label === 'Запасное оружие' || it.label === 'Оружие мультиатаки') : []; document.querySelectorAll('[data-altprice]').forEach(el => { const it = alts[+el.dataset.altprice]; el.textContent = it ? it.cost + ' оч.' : ''; }); }
  // описание собрано по другой механике — подсказать, что его стоит проверить (текст игрока не трогаем)
  document.querySelectorAll('[data-descwarn]').forEach(el => { const a = S.abilities[+el.dataset.descwarn], m = mechAbil(+el.dataset.descwarn); el.textContent = a && a._desc && a._descMech && m && a._descMech !== m ? '⚠ механика изменилась — проверьте описание' : ''; });
  { const el = document.querySelector('[data-wdescwarn]'), m = mechWeapon(); if (el) el.textContent = S.weapon._desc && S.weapon._descMech && m && S.weapon._descMech !== m ? '⚠ механика изменилась — проверьте описание' : ''; }
  // из чего цена способности: части (+) и скидки (−) — разница цены с этой частью и без неё
  document.querySelectorAll('[data-abparts]').forEach(el => {
    const pp = (d.abParts || [])[+el.dataset.abparts];
    el.innerHTML = pp && pp.length ? 'Из чего цена: ' + pp.map(([n, v]) => `<span style="color:${v < 0 ? 'var(--good)' : 'inherit'}">${esc(n)} ${v > 0 ? '+' : '−'}${Math.abs(v)}</span>`).join(' · ') : '';
  });
  const tl = b ? b.items.filter(it => it.label === 'Событие') : [];   // «событие → эффект»: цена каждого
  document.querySelectorAll('[data-tprice]').forEach(el => { const it = tl[+el.dataset.tprice]; el.textContent = it ? it.cost + ' оч.' : ''; });
  const rl = b ? b.items.filter(it => it.label === 'Правило') : [];   // «когда → что»: цена каждого правила
  document.querySelectorAll('[data-rprice]').forEach(el => { const it = rl[+el.dataset.rprice]; el.textContent = it ? (it.cost < 0 ? '+' + (-it.cost) + ' оч. назад' : it.cost + ' оч.') : ''; });
}
// оружие словами: что выбрано и сколько это в среднем за ход (П-2); необычное для D&D — предупреждение, а не запрет
function weaponSummary(){
  const el = document.querySelector('[data-wpnsum]'), wr = document.querySelector('[data-wpnwarn]'); if (!el) return;
  const w = S.weapon || {}, st = S.stats || [], md = i => Math.floor(((st[i] || 10) - 10) / 2);
  if (!w.armed){ el.innerHTML = `<b>Без оружия</b>: удар кулаком к${w.fistS || 2}, атак за действие ${w.att || 1}.`; wr.textContent = ''; return; }
  const n = +w.n || 1, s = +w.s || 6, att = +w.att || 1, mod = w.ammo ? md(1) : w.finesse ? Math.max(md(0), md(1)) : md(0);
  const avg = att * Math.max(1, n * (s + 1) / 2 + mod), dt = CAT && CAT.dtypes ? CAT.dtypes[w.type] || '' : '';
  el.innerHTML = `<b>${n}к${s}${mod ? (mod > 0 ? ' + ' : ' − ') + Math.abs(mod) : ''}</b> ${esc(dt)}, атак за действие ${att}, ${+w.range > 5 ? 'дальность ' + w.range + ' фт' : 'ближний бой'} —
    <b>≈ ${Math.round(avg)} урона за ход</b>, если все атаки попали <span class="small">(${w.ammo ? 'ЛОВ' : w.finesse ? 'большее из СИЛ и ЛОВ' : 'СИЛ'})</span>`;
  const odd = [];
  if (s >= 20) odd.push(`грань к${s} — у оружия D&D самая большая к12`);
  if (n >= 4) odd.push(`${n} кубиков — обычно 1–2`);
  if (att >= 4) odd.push(`${att} атак за действие — у героев D&D до 4 на 20-м уровне`);
  wr.textContent = odd.length ? '⚠ Необычно: ' + odd.join('; ') + '. Можно, но это сильно — цена учтёт.' : '';
}
$('btnExport').onclick = () => {
  const blob = new Blob([JSON.stringify(S, null, 2)], {type:'application/json'}), a = document.createElement('a');
  a.href = URL.createObjectURL(blob); a.download = (S.name || 'character') + '.json'; a.click();
};
$('btnImport').onclick = () => $('fileImport').click();
async function importChar(j){ S = Object.assign(JSON.parse(JSON.stringify(DEFAULT_CHAR)), migrateChar(j)); save(); await loadCatalog(); buildForm(); queuePrice(); }
$('fileImport').onchange = async e => {
  const f = e.target.files[0]; if (!f) return;
  try { await importChar(JSON.parse(await f.text())); }
  catch (err) { alert('Не удалось прочитать файл: ' + err.message); }
};
// файл, брошенный в окно: персонаж (есть характеристики) или карта; без этого браузер открывал файл вместо страницы
addEventListener('dragover', e => e.preventDefault());
addEventListener('drop', async e => {
  e.preventDefault();
  const f = e.dataTransfer && e.dataTransfer.files[0]; if (!f) return;
  try {
    const j = JSON.parse(await f.text());
    if (Array.isArray(j.stats)){ await importChar(j); showTab(1); }
    else { ME = meFromJson(j, j.name || f.name.replace(/\.json$/, '')); showTab(5); meShow(); meMsg('Карта прочитана: проверьте и сохраните.'); }
  } catch (err) { alert('Не удалось прочитать файл: ' + err.message); }
});
$('toOpp').onclick = () => showTab(2);
