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
const themeSelect = document.getElementById('themeSelect');
if (themeSelect){
  const theme = document.documentElement.dataset.theme || 'system';
  themeSelect.value = theme;
  themeSelect.addEventListener('change', () => {
    const value = themeSelect.value;
    document.documentElement.dataset.theme = value;
    try { localStorage.setItem('dnd_theme', value); } catch (e) {}
  });
}
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
function showTab(n){
  [1,2,3,4,5,6].forEach(i => { $('s' + i).classList.toggle('on', i === n); $('tab' + i).setAttribute('aria-selected', i === n); });
  if (n === 4) loadSheet();
  if (n === 5) meShow();
  if (n === 2) loadPreview();
  if (n === 3 && G.active) { fitPlay(); }
}
$('tab6').onclick = () => showTab(6); $('tab5').onclick = () => showTab(5); $('tab4').onclick = () => showTab(4); $('tab1').onclick = () => showTab(1); $('tab2').onclick = () => showTab(2); $('tab3').onclick = () => showTab(3);
