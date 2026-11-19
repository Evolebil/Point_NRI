// Проверки оболочки редакторов (ТЗ ГМ-РКМ, Ш-01): отмена и повтор, индикатор сохранения, черновик в IndexedDB, ключ
// владельца, заголовок. Запуск: node tests/test_editor_shell.mjs
// web/js/39_store.js и 40_maps.js исполняются в песочнице без браузера: элементы страницы — заглушки, IndexedDB — в памяти.
import fs from 'node:fs';
import vm from 'node:vm';

// заглушка элемента: свойства, которые трогает редактор; элемент по id один и тот же (значения сохраняются)
const c2d = new Proxy({}, {get: () => () => {}});
function el(){
  const span = {textContent: ''};
  return {value: '', style: {}, dataset: {}, innerHTML: '', textContent: '', disabled: false, width: 0, height: 0, clientWidth: 0, offsetParent: null,
    classList: {toggle(){}, add(){}, remove(){}, contains(){ return false; }}, addEventListener(){}, querySelectorAll(){ return []; },
    querySelector(){ return span; }, getContext(){ return c2d; }, focus(){}, click(){}, setPointerCapture(){},
    getBoundingClientRect(){ return {left: 0, top: 0, width: 100, height: 100, right: 100, bottom: 100}; }};
}
const EL = new Map(), $ = id => { if (!EL.has(id)) EL.set(id, el()); return EL.get(id); };
// IndexedDB в памяти: open, одно хранилище на имя, get/put/delete, завершение транзакции следующим тиком
const IDB = new Map();
const later = f => setTimeout(f, 0);
const indexedDB = {open(){
  const rq = {};
  later(() => {
    const db = {objectStoreNames: {contains: n => IDB.has(n)}, createObjectStore: n => IDB.set(n, new Map()),
      transaction(n){ const tx = {}, st = IDB.get(n), op = f => { const r = {}; r.result = f(); later(() => tx.oncomplete && tx.oncomplete()); return r; };
        tx.objectStore = () => ({get: k => op(() => st.get(k)), put: (v, k) => op(() => { st.set(k, structuredClone(v)); return k; }), delete: k => op(() => { st.delete(k); })}); return tx; }};
    rq.result = db; if (rq.onupgradeneeded) rq.onupgradeneeded(); rq.onsuccess();
  });
  return rq;
}};
const LS = new Map(), modals = [];
const ctx = vm.createContext({
  console, setTimeout, structuredClone, crypto: globalThis.crypto, indexedDB, Uint8Array, JSON, Math, Date, Map, Set, Object, Array, Promise,
  localStorage: {getItem: k => LS.has(k) ? LS.get(k) : null, setItem: (k, v) => LS.set(k, String(v)), removeItem: k => LS.delete(k)},
  setInterval: () => 0, requestAnimationFrame: () => 0, addEventListener(){}, getComputedStyle: () => ({display: 'block'}), innerWidth: 1400,
  document: {querySelectorAll: () => [], querySelector: () => null, addEventListener(){}, visibilityState: 'visible'},
  $, esc: s => String(s), toast(){}, go(){}, goBack(){}, api: async () => ({type: 'mapcheck', ok: true}), confirmBox: async () => true,
  modal(t, body){ const m = {t, body, close(){}, q: () => ({})}; modals.push(m); return m; },
  ROUTE: 'workshop', ROUTE_ENTER: {}, ROUTE_BACK: {},
});
for (const f of ['39_store.js', '40_maps.js']) vm.runInContext(fs.readFileSync(new URL('../web/js/' + f, import.meta.url), 'utf8'), ctx, {filename: f});
const run = code => vm.runInContext(code, ctx);
let pass = 0, fail = 0;
function check(name, ok, info = ''){ if (ok) pass++; else { fail++; console.log('FAIL', name, info); } }
const tick = () => new Promise(r => setTimeout(r, 5));

// 1. ключ владельца — UUID v4, один на браузер
const k1 = run('ownerKey()'), k2 = run('ownerKey()');
check('ключ владельца: UUID v4, не меняется', /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(k1) && k1 === k2 && LS.get('dnd_owner_key') === k1);
// 2. заголовок (В-02)
check('заголовок: длиннее 28 знаков — с «…»', run("wsTitle('Очень длинное название арены для боя', '100×100')").length === 28 && run("wsTitle('Таверна', '20×14')") === 'Таверна · 20×14');

// карта 20×14 и «штрих»: стена в клетку i — одна операция
run("ME = meNew(20, 14); ME.name = 'Проверка'; meSnap(); meHistReset();");
const stroke = i => run(`{ const b = meShot(); ME.L.wall[${i}] = 1; meStep(b); }`);
const walls = () => run('ME.L.wall.reduce((a, b) => a + b, 0)');
// 3. пустая операция шага не даёт
stroke(0); run('{ const b = meShot(); meStep(b); }');
check('операция без изменений — без шага', run('UNDO.past.length') === 1);
// 4. ИСЦ-02: 120 штрихов, 100 отмен — остаются первые 20, 100 повторов — все 120
run('meHistReset(); ME = meNew(20, 14); ME.name = "Проверка";');
for (let i = 0; i < 120; i++) stroke(i);
for (let i = 0; i < 100; i++) run('meUndo()');
const left20 = walls() === 20 && run('ME.L.wall.slice(0, 20).every(v => v) && ME.L.wall.slice(20).every(v => !v)');
for (let i = 0; i < 100; i++) run('meRedo()');
check('ИСЦ-02: 100 отмен оставляют первые 20 штрихов, 100 повторов возвращают 120', left20 && walls() === 120);
// 5. новое действие очищает повтор
run('meUndo()'); stroke(200);
check('новое действие очищает стек повтора', run('UNDO.fut.length') === 0 && $('meRedo').disabled === true && $('meUndo').disabled === false);
// 6. глубина: не меньше 100 шагов (хранится до 200)
run('meHistReset()'); for (let i = 0; i < 250; i++) stroke(i % 280);
check('глубина отмены не меньше 100 шагов', run('UNDO.past.length') >= 100 && run('UNDO.past.length') <= 200);
// 7. отмена размера карты (операция К-04) — один шаг, возвращает прежний размер
run('meHistReset(); { const b = meShot(); const n = meNew(30, 20); n.name = ME.name; ME = n; meStep(b); } meUndo();');
check('отмена смены размера возвращает 20×14', run('ME.aw') === 20 && run('ME.ah') === 14);

// 8. индикатор и черновик: изменения → «есть изменения», черновик → «черновик сохранён», сохранённая карта → «сохранено»
run("ME = meNew(20, 14); ME.name = 'Проверка'; meHistReset(); meSnap(); meChanged();");
const s0 = $('meSavedInd').dataset.state;
stroke(5);
const s1 = $('meSavedInd').dataset.state;
await run('meDraftSave()'); await tick();
const d = IDB.get('drafts').get('map');
check('индикатор: сохранено → изменения → черновик; черновик в IndexedDB', s0 === 'ok' && s1 === 'dirty' && $('meSavedInd').dataset.state === 'draft' && d && d.name === 'Проверка' && d.map.aw === 20);
await run('meDraftSave()'); await tick();
check('без новых изменений черновик не переписывается', IDB.get('drafts').get('map').at === d.at);

// 9. М-13: черновик, совпадающий с сохранённой картой, удаляется молча; отличающийся — предложение восстановить
run("MAPS['Проверка'] = meToJson(); DRAFT.checked = false;");
IDB.get('drafts').set('map', {name: 'Проверка', map: JSON.parse(run("JSON.stringify(MAPS['Проверка'])")), at: Date.now()});
await run('meDraftCheck()'); await tick();
const silent = !IDB.get('drafts').has('map') && modals.length === 0;
IDB.get('drafts').set('map', {name: 'Другая', map: JSON.parse(run('JSON.stringify(meToJson(meNew(10, 10)))')), at: Date.now()});
run('DRAFT.checked = false;'); await run('meDraftCheck()'); await tick();
check('М-13: свой черновик — молча удалён, чужой — окно «Восстановить?»', silent && modals.length === 1 && /черновик «Другая»/.test(modals[0].body));

console.log(`Оболочка редакторов: PASS ${pass}, FAIL ${fail}`);
process.exit(fail ? 1 : 0);
