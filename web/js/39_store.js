// ====================================================================================================================
// Хранилище редакторов (ТЗ ГМ-РКМ, Ш-01): рабочая копия в IndexedDB (черновики, п. 4.1.5), ключ владельца (Д-05),
// индикатор сохранения в верхней панели (В-04)
// ====================================================================================================================
// IndexedDB «gornilo»: хранилище drafts — черновик каждого редактора по ключу ('map', 'world'). Нет IndexedDB (частный
// режим, запрет) — черновики молчат, остальное работает
const GMDB = {
  p: null,
  open(){
    if (!this.p) this.p = new Promise((res, rej) => {
      if (typeof indexedDB === 'undefined'){ rej(new Error('нет IndexedDB')); return; }
      const rq = indexedDB.open('gornilo', 1);
      rq.onupgradeneeded = () => { const db = rq.result; if (!db.objectStoreNames.contains('drafts')) db.createObjectStore('drafts'); };
      rq.onsuccess = () => res(rq.result);
      rq.onerror = () => rej(rq.error);
      rq.onblocked = () => rej(new Error('IndexedDB занята другой вкладкой'));
    });
    return this.p;
  },
  async run(store, mode, f){
    const db = await this.open();
    return new Promise((res, rej) => {
      const tx = db.transaction(store, mode), rq = f(tx.objectStore(store));
      tx.oncomplete = () => res(rq.result); tx.onerror = tx.onabort = () => rej(tx.error);
    });
  },
  get(store, key){ return this.run(store, 'readonly', s => s.get(key)); },
  put(store, key, val){ return this.run(store, 'readwrite', s => s.put(val, key)); },
  del(store, key){ return this.run(store, 'readwrite', s => s.delete(key)); },
};
// ключ владельца: случайный UUID v4, создаётся при первом запуске и хранится в браузере; уходит только в заголовке
// X-Owner-Key запросов к ассетам и мирам (Ш-04, Ш-10), в журналы и адреса не пишется (п. 4.7.4)
function ownerKey(){
  let k = '';
  try { k = localStorage.getItem('dnd_owner_key') || ''; } catch (e) {}
  if (/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(k)) return k;
  if (crypto.randomUUID) k = crypto.randomUUID();
  else { const b = crypto.getRandomValues(new Uint8Array(16)); b[6] = (b[6] & 15) | 64; b[8] = (b[8] & 63) | 128; const h = [...b].map(x => x.toString(16).padStart(2, '0')).join(''); k = `${h.slice(0, 8)}-${h.slice(8, 12)}-${h.slice(12, 16)}-${h.slice(16, 20)}-${h.slice(20)}`; }
  try { localStorage.setItem('dnd_owner_key', k); } catch (e) {}
  return k;
}
ownerKey();
// индикатор сохранения (В-04): ok — «Сохранено», dirty — «Есть несохранённые изменения», draft — «Черновик сохранён N с назад»
// (время черновика — at), bad — «Не удалось сохранить»
function wsSaved(el, state, at){
  if (!el) return;
  el.dataset.state = state; el.dataset.at = at || '';
  wsSavedText(el);
}
function wsSavedText(el){
  const s = el.dataset.state, sec = Math.max(0, Math.round((Date.now() - +el.dataset.at) / 1000));
  el.querySelector('span').textContent = s === 'dirty' ? 'Есть несохранённые изменения' : s === 'draft' ? `Черновик сохранён ${sec} с назад` : s === 'bad' ? 'Не удалось сохранить' : 'Сохранено';
}
setInterval(() => document.querySelectorAll('.ws-saved[data-state="draft"]').forEach(wsSavedText), 1000);
// заголовок редактора (В-02): «название · размер», длиннее 28 знаков — с «…»
function wsTitle(name, size){ const t = `${name} · ${size}`; return t.length > 28 ? t.slice(0, 27) + '…' : t; }
// диалог несохранённых изменений (п. 4.1.2): «Сохранить» — save() (false — остаться), «Не сохранять» — discard(), затем then()
function wsLeaveDialog(save, discard, then){
  const m = modal('Сохранить изменения?', '<p>Есть несохранённые изменения.</p>', '<button data-close>Отмена</button><button id="wsLeaveNo">Не сохранять</button><button class="primary" id="wsLeaveYes">Сохранить</button>');
  m.q('#wsLeaveNo').onclick = async () => { m.close(); await discard(); then(); };
  m.q('#wsLeaveYes').onclick = async () => { m.close(); if (await save()) then(); };
}
