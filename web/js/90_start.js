// ====================================================================================================================
// Запуск (зовётся в конце 95_gornilo.js, когда все модули загружены)
// ====================================================================================================================
let campaignScript;
async function loadCampaign(){
  if (!campaignScript){
    campaignScript = new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = '/campaign-module.js';
      script.onload = resolve;
      script.onerror = reject;
      document.head.appendChild(script);
    });
  }
  try {
    await campaignScript;
    if (ROUTE === 'campaign' && ROUTE_ENTER.campaign !== loadCampaign) ROUTE_ENTER.campaign();
  } catch {
    campaignScript = null;
    toast('Не удалось загрузить кампанию. Откройте раздел ещё раз.', 5000, true);
  }
}
if (!ROUTE_ENTER.campaign) ROUTE_ENTER.campaign = loadCampaign;
async function startApp(){
  const pn = playerName();
  await loadCatalog();
  if (!CAT){ $('s0').insertAdjacentHTML('beforeend', '<div class="panel" style="margin:24px 34px;max-width:560px"><b>Нет связи с движком игры.</b><p class="muted">Сервер не отвечает. Обновите страницу через минуту.</p></div>'); go('home'); return; }
  if (pn) await loadPlayer(pn);
  buildForm(); doPrice();
  $('seed').value = 1 + Math.floor(Math.random() * 99999);
  startLoop();
  const invite = new URLSearchParams(location.search).get('k');
  const route = invite && /^[a-z0-9]{5}$/i.test(invite) ? 'campaign' : routeFromPath();
  go(route === 'battle' ? 'home' : route);
  if (!pn){ askName(true); return; }
  if (route === 'battle' || route === 'home') resumeSession(pn);
}
