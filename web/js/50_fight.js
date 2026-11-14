// ====================================================================================================================
// 3. Бой (общий код рисования подставляется сервером)
// ====================================================================================================================
let MAP = null, AW = 1, AH = 1, BOSS = [], FR = [];
const cv = $('cv'), ctx = cv.getContext('2d'), stage = $('stage');
let dpr = Math.max(1, devicePixelRatio || 1), terrain = null;
let curIdx = 0, prevIdx = 0, stepT0 = -1e9, playing = false, speed = 1.4, fxs = [], holdUntil = 0;
const ANIM_MS = 520, HOLD_MS = 160;
const ease = t => 1 - Math.pow(1 - t, 3);
const G = {active:false, me:0, pending:null, req:null, mode:'move', busy:false, over:false, logged:0};
const viewActive = () => G.active && $('s3').classList.contains('on');
/*RENDER_JS*/

// ---- воспроизведение кадров: кадры приходят порциями, рисовальщик сам идёт по ним ----
function step(delta){
  const n = curIdx + delta; if (n < 0 || n >= FR.length) return;
  prevIdx = curIdx; curIdx = n; stepT0 = performance.now();
  onEnter(n, true);
}
function pause(){ playing = false; onCaughtUp(); }
function seek(i){ prevIdx = curIdx = i; stepT0 = -1e9; fxs = []; cam.snap = true; onEnter(i, false); }
function onEnter(i, animate){
  const fr = FR[i];
  $('banner').classList.toggle('on', !!fr.chase);
  if (fr.chase) $('banner').textContent = `🏃 ПОГОНЯ · ${BOSS[fr.prey].title} убегает`;
  drawCards(i);
  if (animate && i > 0) spawnFx(i);
  logUpTo(i);
}
function logUpTo(i){
  const host = $('log');
  while (G.logged <= i){
    const k = G.logged++, L = frameLog(k);
    noteSeen(k);
    if (k > 0 && (k === 1 || L.round !== frameLog(k - 1).round)) host.insertAdjacentHTML('beforeend', `<div class="rnd">Раунд ${L.round}</div>`);
    host.insertAdjacentHTML('beforeend', `<div class="ln ${L.cls}">${L.lines.join('<br>')}${L.evs.join('')}</div>`);
  }
  host.scrollTop = host.scrollHeight;
}
// что игрок видел у врага в этом бою: удары оружием и применённые способности
function noteSeen(k){
  const fr = FR[k]; if (!G.seen || !fr || k === 0) return;
  const A = fr.actor, f = A >= 0 ? fr.f[A] : null; if (!f || f.team === FR[k].f[G.me].team) return;
  if (fr.act === 'attack-melee') G.seen.melee = true;
  if (fr.act === 'attack-ranged') G.seen.ranged = true;
  const sp = specOf(A, fr); if (sp && fr.abil >= 0) G.seen.abil[(f.kind < 2 ? 'b' : 'm') + fr.abil] = sp;
}
function seenHTML(){
  const z = G.seen; if (!z) return '';
  const r = [];
  if (z.melee || z.ranged) r.push('⚔ бьёт оружием' + (z.melee && z.ranged ? ' вблизи и издали' : (z.melee ? ' вблизи' : ' издали')));
  Object.values(z.abil).forEach(sp => r.push(`✨ «${esc(sp.title)}»${sp.desc ? ` <span class="small">— ${esc(sp.desc)}</span>` : ''}`));
  return `<div class="seen"><div class="small">Замечено в бою:</div>${r.length ? r.map(x => `<div>${x}</div>`).join('') : '<div class="small">пока ничего</div>'}</div>`;
}
// карточки бойцов поверх поля: слева — вы, справа — враг (в бою команд — своя команда и противники)
function drawCards(i){
  const fr = FR[i], me = G.me; if (!fr) return;
  const face = b => b.portrait ? `<img class="portrait" src="${esc(b.portrait)}" alt="">` : '';
  const hp = (f, cls) => `<div class="hpbar ${cls}"><i style="width:${Math.max(0, 100 * f.hp / f.maxHp)}%"></i></div>`;
  if (G.room){
    const n = BOSS.length, myT = fr.f[me].team;
    const row = k => { const f = fr.f[k], b = BOSS[k];
      return `<div class="team-row"><h4>${face(b)}<span class="chip" style="background:${teamCol(f.team)}">${f.team + 1}</span><span class="ttl">${esc(b.title)}${k === me ? ' (вы)' : ''}${f.hp <= 0 ? ' ☠' : ''}</span></h4>${hp(f, f.team === myT ? 'A' : 'B')}<div class="small stats-line">${Math.max(0, f.hp)} / ${f.maxHp} ХП · КД ${f.ac}${f.cond.length ? ' · ' + f.cond.map(esc).join(', ') : ''}</div></div>`; };
    $('cardMe').className = 'fx fx-me glass pcard'; $('cardMe').innerHTML = `<div class="caps">Ваша команда</div>` + [...Array(n).keys()].filter(k => fr.f[k].team === myT).map(row).join('');
    $('cardEnemy').className = 'fx fx-enemy glass pcard'; $('cardEnemy').innerHTML = `<div class="caps">Противники</div>` + [...Array(n).keys()].filter(k => fr.f[k].team !== myT).map(row).join('') + seenHTML();
    return;
  }
  [[me, 'cardMe', 'fx-me'], [1 - me, 'cardEnemy', 'fx-enemy']].forEach(([k, id, pos]) => {
    const f = fr.f[k], b = BOSS[k], el = $(id), mine = k === me;
    el.className = `fx ${pos} glass pcard ${mine ? 'A' : 'B'}`;
    const mins = fr.f.filter((m, idx) => idx >= 2 && m.kind - 2 === k && m.hp > 0).length;
    const conds = f.cond.map(c => `<span class="pill">${esc(c)}</span>`).join('') + (f.exh ? `<span class="pill">истощение ${f.exh}</span>` : '');
    const sub = [!mine && G.enemyBrain ? 'играет ' + esc(G.enemyBrain) : '', conds ? '' : 'без состояний', b.minion ? 'приспешников ' + mins : ''].filter(Boolean).join(' · ');
    el.innerHTML = `<h4>${face(b)}<span class="chip ${mine ? 'A' : 'B'}">${mine ? 'вы' : 'враг'}</span><span class="ttl">${esc(b.title)}${f.hp <= 0 ? ' ☠' : ''}</span></h4>${hp(f, mine ? 'A' : 'B')}
      <div class="stats-line tnum">${Math.max(0, f.hp)} / ${f.maxHp} ХП${f.temp ? ` (+${f.temp})` : ''} · КД ${f.ac} · ${f.spd} фт</div>
      <div class="small sub">${sub}</div>${conds ? `<div style="margin-top:6px">${conds}</div>` : ''}${!mine ? seenHTML() : ''}`;
  });
}
