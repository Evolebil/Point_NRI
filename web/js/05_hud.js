// ---- скрытие панелей поверх поля (бой, мастерская карт, миры, карта кампании): у каждой панели кнопка «свернуть» —
// остаётся ярлык с названием; клавиша H прячет или возвращает все панели экрана. Свёрнутые помнятся в браузере ----
const HUD_SEL = '.scene-full .glass, [data-hud]';
const HUD_NAMES = {'fx-me': 'Ваша команда', 'fx-round': 'Раунд', 'fx-enemy': 'Противники', 'fx-log': 'Журнал', 'fx-turn': 'Ход',
  'fx-cam': 'Камера', 'ws-top': 'Меню', 'ws-left': 'Панель', 'ws-right': 'Параметры', 'ws-bottom': 'Карты'};
let HUD_OFF = {};
try { HUD_OFF = JSON.parse(localStorage.getItem('dnd_hud_off') || '{}') || {}; } catch (e) { HUD_OFF = {}; }
const hudSave = () => { try { localStorage.setItem('dnd_hud_off', JSON.stringify(HUD_OFF)); } catch (e) {} };
function hudKey(el){
  if (el.dataset.hudKey) return el.dataset.hudKey;
  const sc = el.closest('.scene'), cls = Object.keys(HUD_NAMES).find(c => el.classList.contains(c));
  return el.dataset.hudKey = (sc ? sc.id : '') + ':' + (el.dataset.hud || cls || el.id || 'panel');
}
// кнопка в каждой панели: панели перерисовываются (innerHTML), поэтому кнопка возвращается после каждой перерисовки
function hudScan(){
  hudScan.q = 0;
  document.querySelectorAll(HUD_SEL).forEach(el => {
    const k = hudKey(el);
    if (!el.dataset.hudName) el.dataset.hudName = el.getAttribute('aria-label') || HUD_NAMES[Object.keys(HUD_NAMES).find(c => el.classList.contains(c))] || el.dataset.hud || 'Панель';
    el.classList.add('hud-panel'); el.classList.toggle('hud-off', !!HUD_OFF[k]);
    let b = el.querySelector(':scope > .hud-min');
    if (!b){ b = document.createElement('button'); b.type = 'button'; b.className = 'hud-min'; el.prepend(b); }
    const t = HUD_OFF[k] ? `Показать: ${el.dataset.hudName}` : `Свернуть: ${el.dataset.hudName} (H — все панели)`;
    if (b.title !== t){ b.title = t; b.setAttribute('aria-label', t); b.textContent = HUD_OFF[k] ? '▸' : '–'; }
  });
}
new MutationObserver(() => { if (!hudScan.q) hudScan.q = requestAnimationFrame(hudScan); }).observe(document.body, {childList: true, subtree: true});
document.addEventListener('click', e => {
  const b = e.target.closest('.hud-min'); if (!b) return;
  e.preventDefault(); e.stopPropagation();
  const el = b.parentElement, k = hudKey(el);
  if (HUD_OFF[k]) delete HUD_OFF[k]; else HUD_OFF[k] = 1;
  hudSave(); hudScan();
}, true);
// H — все панели экрана; вернуть — той же клавишей или кнопкой в углу
function hudAll(off){
  document.body.classList.toggle('hud-all-off', off);
  let r = $('hudBack');
  if (!r){ r = document.createElement('button'); r.id = 'hudBack'; r.type = 'button'; r.className = 'btn btn-sm hud-back'; r.textContent = 'Показать панели (H)'; r.onclick = () => hudAll(false); document.body.append(r); }
  r.hidden = !off;
}
addEventListener('keydown', e => {
  if (e.code !== 'KeyH' || e.repeat || e.ctrlKey || e.metaKey || e.altKey || /INPUT|TEXTAREA|SELECT/.test(e.target.tagName) || document.querySelector('.modal-bg')) return;
  if (!FULL_ROUTES.includes(ROUTE) && ROUTE !== 'campaign') return;
  hudAll(!document.body.classList.contains('hud-all-off'));
});
hudScan();
