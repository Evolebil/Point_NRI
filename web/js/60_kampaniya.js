// ====================================================================================================================
// Кампания (исследование и социалка, ТЗ docs/TZ_issledovanie_socialka.md): панель мастера и экран игрока.
// Вкладку и её раздел создаёт этот файл (play.html не меняется). Правила — build/kampaniya, журнал — web/kampaniya.py.
// Мастер видит всё и решает по каждому черновику: принять, перебросить, править итог, отменить. Игрок предлагает.
// ====================================================================================================================
const KMP = {id: '', v: null, timer: 0, speaker: ''};   // speaker — герой, который говорит в разговоре
const KMP_TERR = ['Пашни и сёла', 'Лес', 'Холмы и плато', 'Горы', 'Болото', 'Река и переправа', 'Город'];
const KMP_ROLES = ['без занятия', 'проводник', 'дозорный', 'добытчик', 'картограф', 'следопыт', 'помощь'];
const KMP_TRAV = ['пеший', 'всадник', 'гонец', 'армия', 'обоз'];
const KMP_LIFE = ['нищий', 'жалкий', 'бедный', 'скромный', 'комфортный', 'богатый', 'аристократический'];
const kmpMoney = cp => { cp = Math.round(+cp || 0); if (!cp) return '0 мм'; const s = cp < 0 ? '−' : ''; cp = Math.abs(cp);
  return s + [[Math.floor(cp / 100), 'зм'], [Math.floor(cp / 10) % 10, 'см'], [cp % 10, 'мм']].filter(x => x[0]).map(x => x[0] + ' ' + x[1]).join(' '); };
const kmpOpt = (arr, sel) => arr.map(x => `<option${x === sel ? ' selected' : ''}>${esc(x)}</option>`).join('');
const kmpName = () => (typeof playerName === 'function' && playerName(false)) || $('rmName').value || S.name || '';
const kmpChecks = () => (typeof CAT !== 'undefined' && CAT && CAT.skills ? CAT.skills.map(k => k.name) : []).concat(STAT_N);

// вкладка: кнопка и раздел; showTab (00_common.js) знает вкладки 1–6 — седьмую включаем сами
(function kmpMount(){
  const b = document.createElement('button');
  b.className = 'tab'; b.id = 'tab7'; b.setAttribute('role', 'tab'); b.setAttribute('aria-selected', 'false'); b.textContent = 'Кампания';
  $('tab6').after(b);
  const s = document.createElement('section');
  s.className = 'scene'; s.id = 's7';
  s.innerHTML = `<div class="card" style="max-width:1100px;margin:0 auto"><h3>Кампания</h3>
    <div class="hint">Исследование и социалка. Мастер — тот, кто создал кампанию: он ведёт путь, встречи, разговоры и сделки; движок
    бросает кости и считает, мастер принимает, перебрасывает или правит. Игроки добавляют своего героя и предлагают проверки и сделки.</div>
    <div class="row"><span class="small">Имя: <b id="kmpWho"></b> (вкладка «Комната»)</span>
      <select id="kmpSel"></select><button id="kmpOpen">Открыть</button>
      <label>Новая: <input id="kmpTitle" placeholder="название" style="width:170px"></label>
      <label>зерно <input id="kmpSeed" type="number" value="1" style="width:80px"></label><button class="primary" id="kmpNew">Создать</button></div>
    <div id="kmpBox"></div></div>`;
  $('s6').after(s);
  const orig = showTab;
  showTab = function(n){ orig(n <= 6 ? n : 0); $('s7').classList.toggle('on', n === 7); $('tab7').setAttribute('aria-selected', n === 7);
    if (n === 7) kmpList(); };
  b.onclick = () => showTab(7);
  $('kmpOpen').onclick = () => { KMP.id = $('kmpSel').value; kmpCall({cmd: 'kmp_open'}); };
  $('kmpNew').onclick = async () => { const v = await kmpCall({cmd: 'kmp_create', title: $('kmpTitle').value || 'Кампания', seed: +$('kmpSeed').value || 1}); if (v.type === 'kmp') kmpList(); };
})();

async function kmpList(){
  $('kmpWho').textContent = kmpName() || '— введите имя во вкладке «Комната»';
  const v = await api({cmd: 'kmp_list', name: kmpName()});
  if (v.type !== 'kmp_list') return;
  $('kmpSel').innerHTML = v.list.map(c => `<option value="${esc(c.id)}"${c.id === KMP.id ? ' selected' : ''}>${esc(c.title)}${c.master ? ' (мастер)' : ''}</option>`).join('') || '<option value="">нет кампаний</option>';
}
async function kmpCall(o){
  const v = await api(Object.assign({id: KMP.id, name: kmpName(), hero: KMP.speaker}, o));
  if (v.type === 'error'){ toast(v.err); return v; }
  if (v.type === 'kmp'){ KMP.id = v.id; KMP.v = v; kmpRender(); clearTimeout(KMP.timer); KMP.timer = setTimeout(kmpPoll, 4000); }
  return v;
}
async function kmpPoll(){   // свежий журнал и черновики игроков; пока вкладка открыта и мастер не правит форму
  if (!KMP.id || !$('s7').classList.contains('on') || document.activeElement && $('kmpBox').contains(document.activeElement)){
    KMP.timer = setTimeout(kmpPoll, 4000); return; }
  const v = await api({cmd: 'kmp_open', id: KMP.id, name: kmpName(), hero: KMP.speaker});
  if (v.type === 'kmp' && JSON.stringify(v) !== JSON.stringify(KMP.v)){ KMP.v = v; kmpRender(); }
  KMP.timer = setTimeout(kmpPoll, 4000);
}
const kmpPropose = act => kmpCall({cmd: 'kmp_propose', act});
const kmpDirect = act => kmpCall({cmd: 'kmp_direct', act});

// ---- черновик: коротко, что выпало ----
function kmpCheckLine(c){
  const d = c.d20.length > 1 ? `к20 ${c.d20.join('/')}${c.adv ? ' преим.' : ''}${c.dis ? ' помеха' : ''} → ${c.die}` : `к20 ${c.die}`;
  return `<div class="small">${esc(c.who)}: ${esc(c.what)} — ${d} ${c.mod >= 0 ? '+' : ''}${c.mod} = <b>${c.total}</b>${c.dc ? ` против ${c.dc} ` + (c.ok ? '<b style="color:#7bd88f">успех</b>' : '<b style="color:#ff7b7b">провал</b>') : ''}
    <span class="hint">(${esc(c.parts)})</span></div>`;
}
function kmpEvText(e){
  const r = e.res || {}, a = e.act || {};
  switch (e.type){
    case 'day': { const en = r.enc || {};
      return `День: ${r.km} км, ${esc(r.terrain)}${r.road ? ' по дороге' : ''}, темп ${['медленный','обычный','быстрый'][r.pace]}, ${r.hours} ч.
        ${r.lost ? '<b style="color:#ff7b7b">Сбились с пути.</b>' : ''} Встреча: к20 ${en.d20} против ${en.thr} — ${en.hit ? `<b>${en.ambush ? 'ЗАСАДА' : 'да'}</b> (к12 ${en.d12}): ${esc(en.text || '—')}` : 'нет'}.
        Дозор (пассивная Внимательность): ${r.lookout || '—'}. Добыто: ${r.forage ? r.forage.food : 0} фунтов еды. Еда ${r.food}, вода ${r.water}. Траты на жизнь: ${kmpMoney(r.pay)}${r.payShort ? ' (денег не хватило)' : ''}.
        ${(r.heroes || []).filter(h => h.exh).map(h => esc(h.name) + ': истощение +' + h.exh).join('; ')}`; }
    case 'rest': return `${r.kind} отдых: ` + (r.heroes || []).map(h => `${esc(h.name)} ${h.hp} ХП, костей ${h.hd}, истощение ${h.exh}`).join('; ');
    case 'buy': case 'sell': return `${e.type === 'buy' ? 'Покупка' : 'Продажа'}: ${esc(r.item)} ×${r.qty} — ${kmpMoney(r.total)}${r.pct ? ` (торг ${r.pct > 0 ? '+' : ''}${r.pct}%)` : ''}${r.offend ? ', торговец обижен' : ''}`;
    case 'turnin': return `${esc(r.party)} сдают «${esc(r.fact)}»${r.confirm ? ' (подтверждение)' : ''}: ${kmpMoney(r.pay)}; дальше за новые — ${kmpMoney(r.tier)}`;
    case 'contest': return `Состязание: ${r.win ? 'победил ' + esc(r.win) : 'ничья'} (разница ${r.diff})`;
    case 'group': return `Групповая проверка: ${r.passed} из ${r.of} — ${r.ok ? 'успех' : 'провал'}`;
    case 'check': return `Проверка: ${r.ok ? 'успех' : 'провал'}`;
    case 'hero': return `Герой: ${esc(e.name || '')}`;
    case 'dialog': return (e.npc ? `Разговор с ${esc(e.npc)}` : e.node ? '' : 'Разговор окончен') + (e.said ? ` · ${esc(e.hero || '')}: «${esc(e.said)}»` : '') +
      (e.node ? ` → ${esc(e.node)}` : '') + (e.text ? ` (${esc(e.text)})` : '') + ((e.do || []).length ? ' · ' + e.do.map(d => d.type === 'gm' && d.purse ? kmpMoney(d.purse) : d.fact || d.text || (d.type + (d.delta ? ' ' + d.delta : ''))).map(esc).join(', ') : '');
    default: return esc(e.type) + ' ' + esc(JSON.stringify(Object.fromEntries(Object.entries(e).filter(([k]) => !['n','type','by','attempt'].includes(k)))));
  }
}

function kmpRender(){
  const v = KMP.v, s = v.state, M = v.master;
  const heroes = s.heroes.map(h => h.name);
  let h = `<div class="sec-h">${esc(v.title)} — ${M ? 'вы мастер' : 'вы игрок'}</div>
    <div style="margin:4px 0"><b>День ${s.day}</b>, ${s.hour}:00 ·${esc(s.place || 'место не задано')} · пройдено ${s.km} км · ${esc(s.traveler)}, темп ${esc(s.pace)}
    ${s.returnTrip ? '· <b>обратный путь</b>' : ''} · кошелёк отряда ${kmpMoney(s.purse)} · еда ${s.food} фунтов, вода ${s.water} галлонов · жизнь: ${esc(s.lifestyle)}</div>`;
  if (v.errors && v.errors.length) h += `<div class="err small">Obsidian: ${v.errors.map(esc).join('<br>')}</div>`;
  h += `<table class="small" style="width:100%;border-collapse:collapse"><tr><th align="left">Герой</th><th>Ур.</th><th>ХП</th><th>Кости хитов</th><th>Истощение</th><th>Голод, дней</th><th>Пасс. Внимат.</th><th>Кошелёк</th><th align="left">Вещи</th></tr>` +
    s.heroes.map(x => `<tr${x.dead ? ' style="opacity:.5"' : ''}><td>${esc(x.name)}${x.dead ? ' ☠' : ''}</td><td align="center">${x.level}</td><td align="center">${x.hp}/${x.hpMax}</td><td align="center">${x.hd}/${x.hdMax}</td>
      <td align="center">${x.exh}</td><td align="center">${x.daysNoFood}</td><td align="center">${x.passivePerception}</td><td align="center">${kmpMoney(x.purse)}</td>
      <td>${Object.entries(x.inv).map(([k, n]) => esc(k) + ' ×' + n).join(', ')}</td></tr>`).join('') + '</table>';
  h += `<div class="small" style="margin:6px 0">Груз отряда: ${Object.entries(s.inv).map(([k, n]) => esc(k) + ' ×' + n).join(', ') || '—'} · Сведения: ${s.facts.map(esc).join('; ') || '—'}
    · NPC: ${Object.entries(s.npcs).map(([k, n]) => esc(k) + ' — ' + n.attitude + (n.shift ? ` (${n.shift > 0 ? '+' : ''}${n.shift})` : '')).join('; ') || '—'}
    · Задания: ${Object.entries(s.quests).map(([k, q]) => esc(k) + ': за новые ' + kmpMoney(q.tier)).join('; ') || '—'}</div>`;

  // разговор (Д3): реплики NPC, варианты (недоступные — серым), проверка у варианта, лавка
  if (!KMP.speaker || !heroes.includes(KMP.speaker)) KMP.speaker = heroes[0] || '';
  const T = v.talk;
  if (T){
    h += `<div class="sec-h">Разговор: ${esc(T.npc)} — ${esc(T.attitude)}${T.langs.length ? ' · говорит: ' + T.langs.map(esc).join(', ') : ''}</div>
      <div class="card" style="margin:6px 0">${T.lines.map(l => `<div>${esc(l)}</div>`).join('') || '<div class="hint">(узел без реплик)</div>'}
      <div class="row"><label>Говорит <select id="kTalkHero">${kmpOpt(heroes, KMP.speaker)}</select></label>${M ? '<button id="kmpTalkEnd">Закончить разговор</button>' : ''}</div>
      ${T.opts.map(o => `<div style="margin:3px 0"><button data-say="${o.i}"${o.ok ? '' : ' disabled title="' + esc(o.cond.join('; ')) + '"'}>${esc(o.text)}</button>
        ${o.check ? `<span class="small"> 🎲 ${esc(o.check)}</span>` : ''}${o.ok ? '' : `<span class="small" style="opacity:.6"> нужно: ${esc(o.cond.join('; '))}</span>`}</div>`).join('')}
      ${T.opts.length ? '' : '<div class="hint">Вариантов нет — мастер ведёт сам или заканчивает разговор.</div>'}
      ${T.shop && T.shop.length ? '<div class="sec-h">Лавка</div>' + T.shop.map((x, i) => `<div class="row small">${esc(x['вещь'])} — ${esc(x['цена'] || '')}
        <button data-shop="${i}">Купить</button></div>`).join('') : ''}</div>`;
  }

  // черновики
  if (v.drafts.length){
    h += `<div class="sec-h">${M ? 'Ждут решения' : 'Ваши запросы ждут мастера'}</div>`;
    for (const d of v.drafts){
      const e = d.ev;
      h += `<div class="card" style="margin:6px 0"><div><b>${esc(e.type)}</b> · от ${esc(d.who)}${e.attempt ? ` · переброс ${e.attempt}` : ''}${e.hidden ? ' · скрыто' : ''}</div>
        ${(e.checks || []).map(kmpCheckLine).join('')}<div>${kmpEvText(e)}</div>` +
        (M ? `<div class="row"><button class="primary" data-k="acc" data-d="${d.id}">Принять</button><button data-k="rr" data-d="${d.id}">Перебросить</button>
          <button data-k="ed" data-d="${d.id}">Править итог</button><button data-k="cn" data-d="${d.id}">Отменить</button></div>
          <textarea id="kmpEd${d.id}" style="display:none;width:100%;height:110px;font-family:monospace">${esc(JSON.stringify(e.res || {}, null, 1))}</textarea>` : '') + '</div>';
    }
  }

  if (M && v.npcCards){
    h += `<div class="sec-h">NPC из Obsidian</div><div class="row"><select id="kNpcSel">${v.npcCards.map(c => `<option value="${esc(c.name)}"${c.dialog ? '' : ' disabled'}>${esc(c.name)} — ${esc(c.attitude)}${c.dialog ? '' : ' (нет дерева)'}</option>`).join('') || '<option value="">нет карточек с «тип: npc»</option>'}</select>
      <button id="kmpTalk">💬 Начать разговор</button><span class="small">${v.npcCards.length} карточек</span></div>`;
  }

  // игрок: свой герой и запросы
  h += `<div class="sec-h">Запросы</div><div class="row">
    <button id="kmpMine">➕ Мой герой (вкладка «Персонаж»)</button>
    <label>Герой <select id="kChHero">${kmpOpt(heroes)}</select></label><label>проверка <select id="kChWhat">${kmpOpt(kmpChecks())}</select></label>
    ${M ? `<label>СЛ <input id="kChDC" type="number" value="15" style="width:60px"></label><label><input type="checkbox" id="kChAdv"> преим.</label><label><input type="checkbox" id="kChDis"> помеха</label>
      <label><input type="checkbox" id="kChHid"> скрыто</label>` : ''}<button id="kmpCheck">🎲 Проверка</button>
    ${M ? '<button id="kmpGroup">Групповая</button>' : ''}</div>
    <div class="row"><label>Сделка: <select id="kTrK"><option value="buy">купить</option><option value="sell">продать</option></select></label>
    <label>кто <select id="kTrWho">${kmpOpt(['отряд'].concat(heroes))}</select></label><label>вещь <input id="kTrItem" style="width:130px"></label>
    <label>шт. <input id="kTrQty" type="number" value="1" style="width:55px"></label><label>цена за шт. <input id="kTrPrice" placeholder="5 см" style="width:80px"></label>
    <label>торговец <input id="kTrNpc" style="width:110px"></label><label><input type="checkbox" id="kTrH"> торг:</label>
    <select id="kTrHero">${kmpOpt(heroes)}</select><select id="kTrSk">${kmpOpt(['Убеждение', 'Обман', 'Запугивание'])}</select>
    <label>Проницательность торговца <input id="kTrVs" type="number" value="0" style="width:55px"></label><button id="kmpTrade">Сделка</button></div>`;

  if (M){
    h += `<div class="sec-h">Путь</div><div class="row">
      <label>Местность дня <select id="kDayT">${kmpOpt(KMP_TERR)}</select></label><label><input type="checkbox" id="kDayRoad"> по дороге</label>
      <label>часов <input id="kDayH" type="number" value="8" min="0" max="24" style="width:55px"></label>
      <label>темп <select id="kDayP">${kmpOpt(['медленный', 'обычный', 'быстрый'], s.pace)}</select></label><label><input type="checkbox" id="kDayHot"> жара</label></div>
      <div class="row">${s.heroes.filter(x => !x.dead).map((x, i) => `<label>${esc(x.name)} <select data-role="${esc(x.name)}">${kmpOpt(KMP_ROLES)}</select>
        <select data-help="${esc(x.name)}" title="кому помогает">${kmpOpt([''].concat(heroes.filter(n => n !== x.name)))}</select></label>`).join('')}
      <button class="primary" id="kmpDay">☀ Следующий день</button></div>
      <div class="row"><button id="kmpShort">Короткий отдых</button>${s.heroes.map(x => `<label>${esc(x.name)}: костей <input data-hd="${esc(x.name)}" type="number" min="0" max="${x.hd}" value="0" style="width:45px"></label>`).join('')}
      <button id="kmpLong">Долгий отдых</button></div>
      <div class="sec-h">Сведения и задания</div><div class="row">
      <label>Факт <input id="kFact" style="width:200px"></label><button id="kmpLearn">Отряд узнал</button>
      <label>Задание <input id="kQ" value="${esc(Object.keys(s.quests)[0] || '')}" style="width:120px"></label>
      <label>кто сдаёт <input id="kQParty" value="мы" style="width:120px"></label><button id="kmpTurn">Сдать факт</button>
      <label>новое: за первые <input id="kQBase" value="1000 зм" style="width:80px"></label><label>шаг <input id="kQStep" value="0.9" style="width:50px"></label>
      <label>подтверждение <input id="kQConf" value="0.5" style="width:50px"></label><button id="kmpQuest">Создать задание</button></div>
      <div class="sec-h">Мастер напрямую</div><div class="row">
      <label>кому <select id="kGmWho">${kmpOpt(['отряд'].concat(heroes))}</select></label><label>деньги ± <input id="kGmPurse" placeholder="10 зм" style="width:80px"></label>
      <label>вещь <input id="kGmItem" style="width:110px"></label><label>шт. ± <input id="kGmQty" type="number" value="1" style="width:55px"></label>
      <label>ХП = <input id="kGmHp" type="number" style="width:60px"></label><label>истощение = <input id="kGmExh" type="number" min="0" max="6" style="width:50px"></label>
      <label>часы + <input id="kGmHours" type="number" style="width:55px"></label><label>еда ± <input id="kGmFood" type="number" style="width:55px"></label>
      <label>вода ± <input id="kGmWater" type="number" style="width:55px"></label><label>место <input id="kGmPlace" style="width:120px"></label>
      <label>заметка <input id="kGmText" style="width:200px"></label><button class="primary" id="kmpGm">Сделать</button></div>
      <div class="row"><label>NPC <input id="kNpc" style="width:110px"></label><select id="kNpcT">${kmpOpt(['', 'дружелюбный', 'безразличный', 'враждебный'])}</select>
      <label>сдвиг <input id="kNpcD" type="number" value="0" style="width:50px"></label><button id="kmpAtt">Отношение</button>
      <label>Фракция <input id="kRep" style="width:110px"></label><label>± <input id="kRepD" type="number" value="1" style="width:50px"></label><button id="kmpRep">Известность</button></div>
      <div class="row"><label>Путник <select id="kSetT">${kmpOpt(KMP_TRAV, s.traveler)}</select></label><label>жизнь <select id="kSetL">${kmpOpt(KMP_LIFE, s.lifestyle)}</select></label>
      <label><input type="checkbox" id="kSetR"${s.returnTrip ? ' checked' : ''}> обратный путь</label><button id="kmpSet">Применить</button>
      <button id="kmpUndo">↶ Отменить последнее событие</button><button id="kmpMd">Журнал в markdown</button></div>`;
  }
  h += `<div class="sec-h">Журнал</div><div class="small" style="max-height:340px;overflow:auto">` +
    v.journal.slice().reverse().map(e => e.hidden && !e.checks ? `<div>${e.n}. ${esc(e.text)}</div>` :
      `<div style="margin:3px 0"><b>${e.n}.</b> ${kmpEvText(e)}${e.edit ? ' <i>(правка мастера)</i>' : ''}${(e.checks || []).map(kmpCheckLine).join('')}</div>`).join('') + '</div>';
  $('kmpBox').innerHTML = h;
  kmpBind();
}

function kmpBind(){
  const val = id => $(id) ? $(id).value : '', on = id => $(id) && $(id).checked;
  $('kmpBox').querySelectorAll('button[data-d]').forEach(b => b.onclick = () => {
    const d = +b.dataset.d, k = b.dataset.k;
    if (k === 'ed'){ const t = $('kmpEd' + d); t.style.display = t.style.display === 'none' ? '' : 'none'; return; }
    if (k === 'acc'){ const t = $('kmpEd' + d); let edit = null;
      if (t && t.style.display !== 'none') try { edit = JSON.parse(t.value); } catch (e) { toast('итог — не JSON: ' + e.message); return; }
      kmpCall({cmd: 'kmp_accept', draft: d, edit}); }
    if (k === 'rr') kmpCall({cmd: 'kmp_reroll', draft: d});
    if (k === 'cn') kmpCall({cmd: 'kmp_cancel', draft: d});
  });
  $('kmpMine').onclick = () => { if (!lastPrice || !lastPrice.ok){ toast('Персонаж не собран: проверьте вкладку «Персонаж»'); return; }
    kmpPropose({type: 'hero', name: S.name || kmpName(), char: toChar()}); };
  $('kmpCheck').onclick = () => kmpPropose({type: 'check', hero: val('kChHero'), what: val('kChWhat'), dc: +val('kChDC') || 10, adv: on('kChAdv'), dis: on('kChDis'), hidden: on('kChHid')});
  $('kmpTrade').onclick = () => { const a = {type: val('kTrK'), who: val('kTrWho'), item: val('kTrItem'), qty: +val('kTrQty') || 1, price: val('kTrPrice'), npc: val('kTrNpc')};
    if (!a.item){ toast('какая вещь?'); return; }
    if (on('kTrH')) a.haggle = {hero: val('kTrHero'), what: val('kTrSk'), vs: a.npc || 'торговец', vsMod: +val('kTrVs') || 0};
    kmpPropose(a); };
  if ($('kTalkHero')) $('kTalkHero').onchange = () => { KMP.speaker = val('kTalkHero'); kmpCall({cmd: 'kmp_open'}); };
  $('kmpBox').querySelectorAll('button[data-say]').forEach(b => b.onclick = () => kmpCall({cmd: 'kmp_say', i: +b.dataset.say, hero: KMP.speaker}));
  $('kmpBox').querySelectorAll('button[data-shop]').forEach(b => b.onclick = () => { const x = KMP.v.talk.shop[+b.dataset.shop];
    kmpPropose({type: 'buy', who: 'отряд', item: x['вещь'], qty: 1, price: x['цена'] || '0 мм', npc: KMP.v.talk.npc}); });
  if (!KMP.v.master) return;
  if ($('kmpTalk')) $('kmpTalk').onclick = () => val('kNpcSel') && kmpCall({cmd: 'kmp_talk', npc: val('kNpcSel')});
  if ($('kmpTalkEnd')) $('kmpTalkEnd').onclick = () => kmpCall({cmd: 'kmp_talk_end'});
  $('kmpGroup').onclick = () => kmpPropose({type: 'group', what: val('kChWhat'), dc: +val('kChDC') || 10});
  $('kmpDay').onclick = () => { const roles = {};
    $('kmpBox').querySelectorAll('select[data-role]').forEach(sel => { const n = sel.dataset.role, hp = $('kmpBox').querySelector(`select[data-help="${CSS.escape(n)}"]`);
      if (sel.value === 'помощь' && hp && hp.value) roles[n] = 'помощь:' + hp.value; else if (sel.value !== 'без занятия') roles[n] = sel.value; });
    kmpPropose({type: 'day', terrain: val('kDayT'), road: on('kDayRoad'), hours: +val('kDayH') || 8, pace: val('kDayP'), hot: on('kDayHot'), roles}); };
  $('kmpShort').onclick = () => { const spend = {}; $('kmpBox').querySelectorAll('input[data-hd]').forEach(i => { if (+i.value) spend[i.dataset.hd] = +i.value; });
    kmpPropose({type: 'rest', kind: 'короткий', spend}); };
  $('kmpLong').onclick = () => kmpPropose({type: 'rest', kind: 'долгий'});
  $('kmpLearn').onclick = () => val('kFact') && kmpDirect({type: 'learn', fact: val('kFact')});
  $('kmpTurn').onclick = () => val('kFact') && kmpPropose({type: 'turnin', quest: val('kQ'), fact: val('kFact'), party: val('kQParty') || 'мы'});
  $('kmpQuest').onclick = () => val('kQ') && kmpDirect({type: 'quest', id: val('kQ'), base: val('kQBase'), step: +val('kQStep') || 0.9, confirm: +val('kQConf') || 0.5});
  $('kmpGm').onclick = () => { const a = {type: 'gm'}, who = val('kGmWho');
    if (who !== 'отряд') a.hero = who;
    if (val('kGmPurse')) a.purse = val('kGmPurse').trim().startsWith('-') ? -kmpParse(val('kGmPurse')) : val('kGmPurse');
    if (val('kGmItem')) a.item = val('kGmItem'), a.qty = +val('kGmQty') || 1;
    if (val('kGmHp') !== '') a.hp = +val('kGmHp');
    if (val('kGmExh') !== '') a.exh = +val('kGmExh');
    if (+val('kGmHours')) a.hours = +val('kGmHours');
    if (+val('kGmFood')) a.food = +val('kGmFood');
    if (+val('kGmWater')) a.water = +val('kGmWater');
    if (val('kGmPlace')) a.place = val('kGmPlace');
    if (val('kGmText')) a.text = val('kGmText');
    if (Object.keys(a).length < 2 || (Object.keys(a).length === 2 && a.hero)){ toast('нечего делать: заполните поле'); return; }
    kmpDirect(a); };
  $('kmpAtt').onclick = () => val('kNpc') && kmpDirect(Object.assign({type: 'attitude', npc: val('kNpc'), delta: +val('kNpcD') || 0}, val('kNpcT') ? {tier: val('kNpcT')} : {}));
  $('kmpRep').onclick = () => val('kRep') && kmpDirect({type: 'rep', faction: val('kRep'), delta: +val('kRepD') || 0});
  $('kmpSet').onclick = () => kmpDirect({type: 'setup', traveler: val('kSetT'), lifestyle: val('kSetL'), returnTrip: on('kSetR')});
  $('kmpUndo').onclick = () => confirm('Отменить последнее событие журнала?') && kmpCall({cmd: 'kmp_undo'});
  $('kmpMd').onclick = async () => { const v = await api({cmd: 'kmp_export', id: KMP.id, name: kmpName()});
    if (v.type !== 'kmp_export'){ toast(v.err || 'ошибка'); return; }
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([v.md], {type: 'text/markdown'})); a.download = v.title + ' — журнал.md'; a.click(); };
}
// «10 зм 5 см» → медные (для вычитания денег мастером)
function kmpParse(t){ let tot = 0; t.replace(/(\d+)\s*(зм|см|мм)?/g, (m, n, u) => { tot += +n * (u === 'зм' ? 100 : u === 'см' ? 10 : 1); }); return tot; }
