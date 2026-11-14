// ====================================================================================================================
// Запуск (зовётся в конце 95_gornilo.js, когда все модули загружены)
// ====================================================================================================================
async function startApp(){
  const pn = playerName();
  await loadCatalog();
  if (!CAT){ $('s0').insertAdjacentHTML('beforeend', '<div class="panel" style="margin:24px 34px;max-width:560px"><b>Нет связи с движком игры.</b><p class="muted">Сервер не отвечает. Обновите страницу через минуту.</p></div>'); go('home'); return; }
  if (pn) await loadPlayer(pn);
  buildForm(); doPrice();
  $('seed').value = 1 + Math.floor(Math.random() * 99999);
  startLoop();
  go('home');
  if (!pn){ askName(true); return; }
  resumeSession(pn);
}
