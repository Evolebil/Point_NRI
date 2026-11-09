// ====================================================================================================================
// Имя игрока — строка под меню, одна на всю игру (под ним сохраняются персонаж, место в комнате и кампании).
// Без prompt(): страница работает и там, где всплывающие окна запрещены. Смена имени — перезагрузка: всё подхватит новое.
// ====================================================================================================================
(function nameBar(){
  const bar = document.createElement('div');
  bar.id = 'nameBar';
  bar.style.cssText = 'max-width:1100px;margin:6px auto 0;padding:6px 12px;border-radius:8px;font-size:14px;display:flex;gap:8px;align-items:center;flex-wrap:wrap';
  document.querySelector('.tabs').after(bar);
  let cur = ''; try { cur = localStorage.getItem('dnd_rmname') || ''; } catch (e) {}
  const set = n => { n = n.trim().slice(0, 40); if (!n) return; try { localStorage.setItem('dnd_rmname', n); } catch (e) {} location.reload(); };
  if (cur){
    bar.innerHTML = `<span>Вы: <b>${esc(cur)}</b></span><button id="nameChange" title="Под другим именем — свой персонаж и свои кампании">Сменить имя</button>`;
    $('nameChange').onclick = () => { bar.innerHTML = `<label>Новое имя <input id="nameIn" value="${esc(cur)}" style="width:180px"></label><button class="primary" id="nameOk">Сохранить</button>`;
      $('nameIn').focus(); $('nameOk').onclick = () => set($('nameIn').value); $('nameIn').onkeydown = e => { if (e.key === 'Enter') set($('nameIn').value); }; };
  } else {
    bar.style.background = 'rgba(232,180,60,.18)';
    bar.innerHTML = `<label><b>Как вас зовут?</b> <input id="nameIn" placeholder="имя игрока" style="width:180px"></label><button class="primary" id="nameOk">Войти</button>
      <span class="hint">Под этим именем сохраняются персонаж, место в комнате и кампании. С другого устройства — войдите с тем же именем.</span>`;
    $('nameOk').onclick = () => set($('nameIn').value);
    $('nameIn').onkeydown = e => { if (e.key === 'Enter') set($('nameIn').value); };
  }
})();
