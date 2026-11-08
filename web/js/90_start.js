// ====================================================================================================================
// Запуск
// ====================================================================================================================
(async function init(){
  // игрок по имени: персонаж с сервера (если в этом браузере его нет) и возврат в свою комнату или бой
  let hadLocal = false; try { hadLocal = !!localStorage.getItem('dnd_char'); } catch (e) {}
  const pn = playerName(true);
  let pl = null;
  if (pn){ pl = await api({cmd:'player_load', name:pn}); if (pl && pl.s && !hadLocal) try { S = Object.assign(JSON.parse(JSON.stringify(DEFAULT_CHAR)), migrateChar(pl.s)); localStorage.setItem('dnd_char', JSON.stringify(S)); } catch (e) {} }
  if (pn) $('rmName').value = pn;
  await loadCatalog();
  if (!CAT){ $('form').innerHTML = '<div class="card err">Нет связи с движком. Запустите: python3 play_server.py</div>'; return; }
  buildForm();
  if (pl && pl.room){ ROOM.code = pl.room; ROOM.n = 0; G.room = ''; const v = await roomCall({cmd:'room_join', name:pn}); if (v && v.type === 'room'){ toast('Вы снова в комнате ' + v.code); if (v.status !== 'fight') showTab(6); roomPoll(); } else ROOM.code = ''; }
  doPrice();
  $('seed').value = 1 + Math.floor(Math.random() * 99999);
  startLoop();
})();
