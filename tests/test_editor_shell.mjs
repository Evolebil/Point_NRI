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
    querySelector(){ return span; }, getContext(){ return c2d; }, focus(){}, click(){}, setPointerCapture(){}, setAttribute(){},
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

// ---- Ш-02: инструменты боевого редактора ----
const J = s => JSON.parse(run(`JSON.stringify(${s})`));
// 11. кисть: квадрат b×b, круг без углов (размер 3 — 5 клеток, 10 — меньше 100), размер 1 — одна клетка
check('кисть: квадрат 3 — 9, круг 3 — 5, круг 10 — меньше квадрата, 1 — 1', J('meStamp(5, 5, 3, "sq")').length === 9 && J('meStamp(5, 5, 3, "ci")').length === 5
  && J('meStamp(5, 5, 10, "ci")').length < 100 && J('meStamp(5, 5, 10, "sq")').length === 100 && J('meStamp(5, 5, 1, "ci")').length === 1);
// 12. линия Брезенхэма (1,1)–(10,5): 10 клеток, по клетке в каждом столбце; Shift — 0°, 45°, 90°
const line = J('meLineCells(0, 0, 9, 4)');
check('линия: без разрывов, Shift — кратно 45°', line.length === 10 && new Set(line.map(c => c[0])).size === 10
  && J('meLineEnd(0, 0, 9, 4, true)').join() === '9,0' && J('meLineEnd(0, 0, 5, 4, true)').join() === '5,5' && J('meLineEnd(0, 0, 1, 7, true)').join() === '0,7');
// 13. прямоугольник 6×4: залитый 24, контур 16, Shift — квадрат 6×6
run("ME = meNew(20, 14); ED.rect = 'fill';"); const rf = J('meRectCells(1, 1, 6, 4, false)').length, rs = J('meRectCells(1, 1, 6, 4, true)').length;
run("ED.rect = 'line';"); const rl = J('meRectCells(1, 1, 6, 4, false)').length; run("ED.rect = 'fill';");
check('прямоугольник: залитый 24, контур 16, с Shift 36', rf === 24 && rl === 16 && rs === 36);
// 14. заливка 4-связная: кольцо стен, внутри — только внутренность, диагональная щель не пропускает
run(`ME = meNew(20, 14); for (let x = 2; x <= 8; x++){ meSet(x, 2, 'wall'); meSet(x, 8, 'wall'); } for (let y = 2; y <= 8; y++){ meSet(2, y, 'wall'); meSet(8, y, 'wall'); }
  meSet(8, 8, 'h'); ED.mat = 'water'; meFill(5, 5);`);
check('заливка: только связная область одного значения (4 стороны)', run('ME.L.water.reduce((a, b) => a + b, 0)') === 25 && run('ME.L.water[1 * 20 + 1]') === 0);
// 15. ластик чистит только группу выбранного материала
run("$('meLevel').value = 2; ME = meNew(20, 14); meSet(3, 3, 'h'); meSet(3, 3, 'tree'); meSet(3, 3, 'z1'); meClear(3, 3, 'obj');");
const i33 = 3 * 20 + 3;
check('ластик «Объекты»: дерево убрано, высота и лава на месте', run(`ME.L.tree[${i33}]`) === 0 && run(`ME.L.haz[${i33}]`) === 1 && run(`ME.L.height[${i33}]`) > 0);
// 16. пипетка: верхний материал (опасное раньше объекта и рельефа), потом прежний инструмент
run("meSet(3, 3, 'tree'); ED.tool = 'line'; meSetTool('pick'); mePick(3, 3);");
check('пипетка: «Лава» поверх дерева и высоты, инструмент — снова «Линия»', run('ED.mat') === 'z1' && run('ED.tool') === 'line');
// 17. крепкая стена: ластик рельефа её не трогает, ластик объектов убирает вместе со стеной
run("ME = meNew(20, 14); meSet(4, 4, 'strong'); meClear(4, 4, 'relief');"); const kept = run('ME.L.wall[84] + ME.L.strong[84]');
run("meClear(4, 4, 'obj');");
check('крепкая стена: рельефный ластик не трогает, объектный убирает', kept === 2 && run('ME.L.wall[84] + ME.L.strong[84]') === 0);
// 18. выделение: поворот на 90° по часовой стрелке и отражение
run("ME = meNew(20, 14); meSet(2, 2, 'water'); meSet(3, 2, 'wall'); meSet(4, 2, 'wall'); meSet(2, 3, 'wall');");
const rot = J('(() => { const b = meTurn(meCopy({x0: 2, y0: 2, x1: 4, y1: 3}), "r"); return [b.w, b.h, b.L.water.indexOf(1)]; })()');
const flip = J('(() => { const b = meTurn(meCopy({x0: 2, y0: 2, x1: 4, y1: 3}), "h"); return b.L.water.indexOf(1); })()');
check('поворот 3×2 → 2×3, метка из угла слева сверху — справа сверху; отражение — справа', rot.join() === '2,3,1' && flip === 2);
// 19. старты: не больше 16 на сторону, клетка не бывает сразу A и B, в JSON — «starts» (первые клетки) и «st»
run('ME = meNew(20, 14); for (let k = 0; k < 17; k++) meStartAdd(k, 0, 0); meStartAdd(0, 0, 1); meStartAdd(5, 5, 1);');
const sj = J('meToJson()');
check('старты: 16 клеток A, общая клетка ушла к B, starts и st в карте', sj.st.A.length === 15 && sj.st.B.length === 2 && sj.starts.join('|') === '1,0|0,0' && !sj.st.A.some(c => c[0] === 0 && c[1] === 0));
// 20. совместимость: карта с одной клеткой на сторону пишется без «st» и читается обратно так же
run('ME = meNew(20, 14); meStartAdd(1, 1, 0); meStartAdd(9, 9, 1);');
const one = J('meToJson()'), back = J('meToJson(meFromJson(meToJson(), "x"))'), multi = J('meToJson(meFromJson(' + JSON.stringify(sj) + ', "y"))');
check('совместимость: одна клетка на сторону — без st; чтение-запись не меняет карту', !one.st && JSON.stringify(one) === JSON.stringify(back) && JSON.stringify(multi) === JSON.stringify(sj));

console.log(`Оболочка редакторов: PASS ${pass}, FAIL ${fail}`);
process.exit(fail ? 1 : 0);
