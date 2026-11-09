// ====================================================================================================================
// Общее
// ====================================================================================================================
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
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
