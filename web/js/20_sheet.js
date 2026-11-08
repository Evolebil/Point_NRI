// ---- шпаргалка: markdown от сервера в простую вёрстку ----
let sheetFor = 0;
async function loadSheet(){
  if (sheetFor === S.budget) return; sheetFor = S.budget;
  const t = await (await fetch('/sheet?budget=' + S.budget)).text();
  const inl = x => esc(x).replace(/\*\*(.+?)\*\*/g, '<b>$1</b>');
  let h = '', tbl = null, ul = false;
  const endUl = () => { if (ul){ h += '</ul>'; ul = false; } };
  for (const line of t.split('\n')){
    if (line.startsWith('|')){
      endUl(); const cells = line.split('|').slice(1, -1).map(c => c.trim());
      if (cells.every(c => /^-+$/.test(c))) continue;
      if (!tbl){ tbl = true; h += '<table class="sh"><tr>' + cells.map(c => '<th>' + inl(c) + '</th>').join('') + '</tr>'; }
      else h += '<tr>' + cells.map(c => '<td>' + inl(c) + '</td>').join('') + '</tr>';
      continue;
    }
    if (tbl){ h += '</table>'; tbl = null; }
    if (line.startsWith('# ')) h += '<h2>' + inl(line.slice(2)) + '</h2>';
    else if (line.startsWith('## ')){ endUl(); h += '<h3 style="margin-top:18px">' + inl(line.slice(3)) + '</h3>'; }
    else if (line.startsWith('- ')){ if (!ul){ h += '<ul>'; ul = true; } h += '<li>' + inl(line.slice(2)) + '</li>'; }
    else if (line.trim()){ endUl(); h += '<p>' + inl(line) + '</p>'; }
  }
  if (tbl) h += '</table>'; endUl();
  $('sheet').innerHTML = '<div class="row"><input type="search" id="shQ" placeholder="Поиск по шпаргалке…" style="width:280px"><span class="small" id="shN"></span></div>' + h + '<p class="hint">Цены посчитаны движком для бюджета ' + S.budget + '. Файл: docs/kratko-pravila.md.</p>';
  // поиск: оставляет строки таблиц, пункты и абзацы с найденным текстом; заголовок раздела виден, если в разделе что-то нашлось
  $('shQ').oninput = () => {
    const q = $('shQ').value.trim().toLowerCase(); let n = 0;
    $('sheet').querySelectorAll('tr, li, p:not(.hint)').forEach(el => { if (el.querySelector('th')) return; const ok = !q || el.textContent.toLowerCase().includes(q); el.style.display = ok ? '' : 'none'; if (ok && q) n++; });
    $('sheet').querySelectorAll('table.sh').forEach(t => t.style.display = [...t.querySelectorAll('tr')].some(r => !r.querySelector('th') && r.style.display !== 'none') ? '' : 'none');
    $('sheet').querySelectorAll('ul').forEach(u => u.style.display = [...u.children].some(li => li.style.display !== 'none') ? '' : 'none');
    $('shN').textContent = q ? 'найдено: ' + n : '';
  };
}
