// ====================================================================================================================
// Общее
// ====================================================================================================================
const $ = id => document.getElementById(id);
const ftToText = ft => { const m = (+ft * 0.3048).toFixed(1).replace('.', ',').replace(/,0$/, ''); return `${ft} фт (≈${m} м)`; };
function pluralRu(n, one, few, many){ const x = Math.abs(+n) % 100, d = x % 10; return x >= 11 && x <= 14 ? many : d === 1 ? one : d >= 2 && d <= 4 ? few : many; }
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const UI_ICON_PATH = {
  character:'<circle cx="12" cy="8" r="3.5"/><path d="M5 20c.5-3.3 2.8-5 7-5s6.5 1.7 7 5"/>',
  stats:'<path d="M4 19V5m0 14h16"/><path d="m7 15 3-4 3 2 5-7"/>',
  skill:'<path d="m12 3 2.7 5.5 6.1.9-4.4 4.3 1 6.1-5.4-2.9-5.4 2.9 1-6.1-4.4-4.3 6.1-.9z"/>',
  weapon:'<path d="m14 4 6 6-9.5 9.5-6-6zM4 20l4-4M14 4l2-2 6 6-2 2"/>',
  magic:'<path d="m12 3 1.8 5.2L19 10l-5.2 1.8L12 17l-1.8-5.2L5 10l5.2-1.8zM19 16l1 2 2 1-2 1-1 2-1-2-2-1 2-1z"/>',
  shield:'<path d="M12 3 20 6v5c0 5-3.3 8.3-8 10-4.7-1.7-8-5-8-10V6z"/><path d="m8 12 2.5 2.5L16 9"/>',
  move:'<path d="M12 3v18M3 12h18M12 3l-3 3m3-3 3 3m6 6-3-3m3 3-3 3m-6 6 3-3m-3 3-3-3m-6-6 3 3m-3-3 3-3"/>',
  feature:'<path d="M4 5h16v14H4zM8 9h8M8 12h8M8 15h5"/>',
  opponent:'<path d="M4 5h6v6H4zM14 13h6v6h-6zM10 8h4v8"/>',
  battle:'<path d="m5 4 15 16M19 4 4 19M8 3l2 2M3 8l2 2m9 9 2 2m3-5 2 2"/>',
  room:'<path d="M4 21V5l8-3 8 3v16M9 21v-6h6v6M8 8h1m6 0h1m-8 4h1m6 0h1"/>',
  map:'<path d="m3 6 6-3 6 3 6-3v15l-6 3-6-3-6 3zM9 3v15m6-12v15"/>',
  price:'<path d="M3 12V4h8l10 10-7 7z"/><circle cx="7.5" cy="8" r="1"/>',
  damage:'<path d="M13 2 5 13h6l-1 9 9-12h-6z"/>',
  plus:'<path d="M12 5v14M5 12h14"/>'
};
function uiIcon(name){ return `<svg class="ui-icon" aria-hidden="true" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${UI_ICON_PATH[name] || UI_ICON_PATH.feature}</svg>`; }
const SIDE = ['A','B'];
const STAT_N = ['СИЛ','ЛОВ','ТЕЛ','ИНТ','МДР','ХАР'];
const STAT_FULL = ['Сила: удары, сила рук, Атлетика', 'Ловкость: точность, уклонение, КД без тяжёлого доспеха', 'Телосложение: здоровье, выносливость', 'Интеллект: знания, Анализ', 'Мудрость: внимательность, чутьё', 'Харизма: убеждение, обаяние'];
const STAT_TABLE = [6, 8, 10, 12, 14, 16, 18, 20, 24, 30];   // значения для таблицы цен на шаге «Характеристики»
const DT_COL = ['#9be15d','#cdbba0','#8fd8ff','#ff8a3d','#c58bff','#fff06a','#9a74d6','#e4e4e4','#7bd88f','#ff7bd5','#fff2a8','#f0f0f0','#9fb4ff'];
const CATS = {
  stat:{ic:'💪',c:'#8e7cc3',n:'Характеристики'}, ac:{ic:'🛡️',c:'#5b9bd5',n:'КД'}, weapon:{ic:'⚔️',c:'#d9822b',n:'Оружие'},
  ability:{ic:'✨',c:'#d95fa8',n:'Способности'}, passive:{ic:'🧿',c:'#3fb0a0',n:'Пассивы'}, aura:{ic:'🔥',c:'#e5b83a',n:'Аура'},
  minions:{ic:'👥',c:'#8aa04a',n:'Приспешники'}, hp:{ic:'❤️',c:'#d23b3b',n:'Здоровье'}
};
// свой id браузера: у каждого игрока свой бой и своё место в комнате
let SID = ''; try { SID = localStorage.getItem('dnd_sid') || ''; if (!SID){ SID = Math.random().toString(36).slice(2) + Date.now().toString(36); localStorage.setItem('dnd_sid', SID); } } catch (e) { SID = Math.random().toString(36).slice(2); }
async function api(obj){
  try {
    const ac = new AbortController(), to = setTimeout(() => ac.abort(), 35000);   // туннель завис — не ждать вечно
    const r = await fetch('/api', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(Object.assign({sid: SID}, obj)), signal: ac.signal});
    clearTimeout(to);
    return await r.json();
  } catch (e) { return {type:'error', err:'нет связи с сервером: ' + e.message}; }
}
// ---- экраны: маршрут -> раздел страницы; модули добавляют свои действия при входе (ROUTE_ENTER) и по кнопке «назад» (ROUTE_BACK) ----
const SCENES = {home:'s0', editor:'s1', quick:'s2', battle:'s3', sheet:'s4', workshop:'s5', coop:'s6', campaign:'s7', characters:'s8', settings:'s9', worlds:'s10'};
const TAB_ROUTE = {1:'editor', 2:'quick', 3:'battle', 4:'sheet', 5:'workshop', 6:'coop', 7:'campaign'};
const FULL_ROUTES = ['battle', 'workshop', 'worlds'];   // поле на весь экран: без шапки и фона
const ROUTE_ENTER = {}, ROUTE_BACK = {};
let ROUTE = '';
function go(route){
  if (!SCENES[route]) route = 'home';
  const prev = ROUTE; ROUTE = route;
  Object.entries(SCENES).forEach(([r, id]) => { const el = $(id); if (el) el.classList.toggle('on', r === route); });
  document.body.className = document.body.className.replace(/\broute-\S+/g, '').trim() + ' route-' + route;
  document.body.classList.toggle('full', FULL_ROUTES.includes(route));
  const back = $('backBtn');
  back.hidden = route === 'home';
  back.textContent = route === 'coop' && ROOM.code ? 'Выйти из комнаты' : '← В главное меню';
  back.classList.toggle('danger', route === 'coop' && !!ROOM.code);
  if (prev !== route) scrollTo(0, 0);
  if (ROUTE_ENTER[route]) ROUTE_ENTER[route](prev);
}
// место под содержимое внутри блока (без отступов) — поле мастерской между панелями
function innerSize(el){ const cs = getComputedStyle(el); return [el.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight), el.clientHeight - parseFloat(cs.paddingTop) - parseFloat(cs.paddingBottom)]; }
function goBack(){ if (ROUTE_BACK[ROUTE]) ROUTE_BACK[ROUTE](); else go('home'); }
function showTab(n){ go(TAB_ROUTE[n] || 'home'); }
$('backBtn').onclick = goBack;
document.addEventListener('click', e => { const b = e.target.closest('[data-go]'); if (b) { e.preventDefault(); go(b.dataset.go); } });
// уведомление внизу экрана (одно на всю игру)
function toast(t, ms = 3500, isErr = false){
  const e = $('toast'); if (!e) return;
  e.textContent = t || ''; e.classList.toggle('on', !!t); e.classList.toggle('err', !!isErr);
  clearTimeout(toast.t); if (t) toast.t = setTimeout(() => e.classList.remove('on'), isErr ? Math.max(ms, 6000) : ms);
}
// окно поверх экрана: закрывается крестиком действия, Esc и щелчком мимо (кроме обязательных)
function modal(title, body, buttons, opts = {}){
  const root = document.createElement('div'); root.className = 'modal-bg'; root.dataset.modal = '1';
  root.innerHTML = `<section class="modal ${opts.cls || ''}" role="dialog" aria-modal="true" aria-label="${esc(title || '')}">${title ? `<h2>${esc(title)}</h2>` : ''}<div class="modal-body">${body}</div>${buttons ? `<div class="modal-acts">${buttons}</div>` : ''}</section>`;
  // окна складываются стопкой (подтверждение поверх окна); Esc закрывает только верхнее
  const onKey = e => { if (e.key !== 'Escape' || [...document.querySelectorAll('.modal-bg[data-modal]')].pop() !== root) return; e.preventDefault(); e.stopImmediatePropagation(); if (!opts.required) close(); };
  const close = (force) => { if (opts.required && force !== true) return; if (!root.isConnected) return; root.remove(); document.removeEventListener('keydown', onKey, true); if (opts.onClose) opts.onClose(); };
  root._close = close;
  document.addEventListener('keydown', onKey, true);
  root.addEventListener('click', e => { if (e.target === root && !opts.required) close(); });
  document.body.append(root);
  root.querySelectorAll('[data-close]').forEach(b => b.onclick = () => close(true));
  const f = root.querySelector('[autofocus],input,select,textarea,button.primary'); if (f) setTimeout(() => f.focus(), 30);
  return {root, close: () => close(true), q: sel => root.querySelector(sel)};
}
function confirmBox(text, okText = 'Да', danger = false){
  return new Promise(res => {
    const m = modal('Подтвердите', `<p>${esc(text)}</p>`, `<button data-no>Отмена</button><button class="${danger ? 'danger' : 'primary'}" data-yes>${esc(okText)}</button>`, {onClose: () => res(false)});
    m.q('[data-no]').onclick = () => m.close();
    m.q('[data-yes]').onclick = () => { res(true); m.close(); };
  });
}
