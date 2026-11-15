// Проверки 3D-вида (этап 1а): перевод 2D-карты в кубы 5×5×5 фт и луч по кубам. Запуск: node tests/test_render3d.mjs
// Файл web/js/55_render3d.js исполняется в песочнице без браузера (флаг ?3d=1 выключен — библиотека 3D не грузится).
import fs from 'node:fs';
import vm from 'node:vm';

const src = fs.readFileSync(new URL('../web/js/55_render3d.js', import.meta.url), 'utf8');
const ctx = vm.createContext({
  location: {search: ''}, URLSearchParams, $: () => ({}), toast: () => {}, console,
  hash: () => .5, lv36: (L, x, y) => { const c = L && L[y] && L[y][x]; return c ? parseInt(c, 36) || 0 : 0; }, FR: [], curIdx: 0, prevIdx: 0, G: {}, cam: {}, MAP: null, AW: 1, AH: 1
});
vm.runInContext(src + '\nglobalThis.T = {R3, VM, VZ0, r3Column, r3DynMap, r3Level, r3Z, r3Voxel, vIdx};', ctx);
const {R3, VM, VZ0, r3Column, r3DynMap, r3Level, r3Z, r3Voxel, vIdx} = ctx.T;

// карта 3×1 из слоёв: каждый слой — одна строка
function build(layers, dyn = []){
  const m = {aw: 3, ah: 1, theme: 0};
  for (const k of ['wall', 'pit', 'water', 'diff', 'climb', 'dark', 'height', 'tree', 'haz', 'ladder', 'deck', 'door', 'strong']) m[k] = [layers[k] || '000'];
  ctx.MAP = m; ctx.AW = 3; ctx.AH = 1;
  R3.H = 32; R3.vox = new Uint8Array(3 * 32); R3.dynCells = r3DynMap(dyn);
  for (let x = 0; x < 3; x++) r3Column(x, 0);
}
const at = (x, z) => R3.vox[vIdx(x, 0, z)];
let pass = 0, fail = 0;
function check(name, ok, info = ''){ if (ok) pass++; else { fail++; console.log('FAIL', name, info); } }

build({});
check('ровная земля: верхний куб — трава, над ним воздух', at(0, VZ0 - 1) === VM.GRASS && at(0, VZ0) === 0);
check('боец на ровной земле стоит в кубе VZ0', r3Z({x: 0, y: 0}) === VZ0);

build({height: '400'});
check('высота 4 (20 фт) = 4 куба над нулём', at(0, VZ0 + 3) === VM.GRASS && at(0, VZ0 + 4) === 0 && r3Z({x: 0, y: 0}) === VZ0 + 4);
check('летун: высота полёта 2 (10 фт) = +2 куба', r3Z({x: 0, y: 0, alt: 2}) === VZ0 + 6);
build({height: 'b00'});
check('уровень буквой: b = 11 (55 фт)', at(0, VZ0 + 10) === VM.GRASS && at(0, VZ0 + 11) === 0);

build({wall: '100', strong: '010'});
check('стена — 6 кубов камня (30 фт)', at(0, VZ0) === VM.STONE && at(0, VZ0 + 5) === VM.STONE && at(0, VZ0 + 6) === 0);

build({tree: '428'});
check('дерево 20 фт: 2 куба ствола + 2 кроны, стоят на вершине', at(0, VZ0) === VM.WOOD && at(0, VZ0 + 2) === VM.LEAF && at(0, VZ0 + 4) === 0 && r3Level({x: 0, y: 0}) === 4);
check('куст 10 фт и великан 40 фт', at(1, VZ0) === VM.WOOD && at(1, VZ0 + 1) === VM.LEAF && at(1, VZ0 + 2) === 0 && at(2, VZ0 + 5) === VM.WOOD && at(2, VZ0 + 7) === VM.LEAF && at(2, VZ0 + 8) === 0 && r3Level({x: 2, y: 0}) === 8);

build({pit: '100'});
check('ущелье: только дно', at(0, 0) === VM.PITF && at(0, 1) === 0 && at(0, VZ0 - 1) === 0);

build({deck: '600'});
check('настил на 30 фт: доска в кубе VZ0+5, на нём стоят в VZ0+6', at(0, VZ0 + 5) === VM.PLANK && at(0, VZ0) === 0 && r3Z({x: 0, y: 0, up: 1}) === VZ0 + 6);

build({door: '100'});
check('закрытая дверь: 2 куба двери и перемычка до 30 фт', at(0, VZ0) === VM.DOOR && at(0, VZ0 + 1) === VM.DOOR && at(0, VZ0 + 2) === VM.STONE && at(0, VZ0 + 6) === 0);
build({door: '100'}, [[0, 0, 6, 0]]);
check('открытая дверь (изменение арены 6): проём свободен', at(0, VZ0) === 0 && at(0, VZ0 + 1) === 0 && at(0, VZ0 + 2) === VM.STONE);

build({wall: '100', tree: '040'}, [[0, 0, 4, 0], [1, 0, 4, 0]]);
check('разрушенные стена и дерево (вид 4): обломки, над ними воздух', at(0, VZ0 - 1) === VM.RUBBLE && at(0, VZ0) === 0 && at(1, VZ0) === 0 && r3Level({x: 1, y: 0}) === 0);

build({water: '100', haz: '010'});
check('вода и лава — материал верхнего куба', at(0, VZ0 - 1) === VM.WATER && at(1, VZ0 - 1) === VM.LAVA);

build({wall: '001'});
let h = r3Voxel({x: .5, y: 20.5, z: .5}, {x: 0, y: -1, z: 0}, 1e9);
check('луч сверху попадает в верх земли, грань +z', h && h.x === 0 && h.z === VZ0 - 1 && h.nz === 1 && h.nx === 0, JSON.stringify(h));
h = r3Voxel({x: .5, y: VZ0 + 2.5, z: .5}, {x: 1, y: 0, z: 0}, 1e9);
check('луч вбок упирается в стену, грань к стреляющему', h && h.x === 2 && h.z === VZ0 + 2 && h.nx === -1, JSON.stringify(h));
h = r3Voxel({x: .5, y: VZ0 + 7.5, z: .5}, {x: 1, y: 0, z: 0}, 1e9);
check('луч над стеной (выше 30 фт) проходит мимо', h === null, JSON.stringify(h));

console.log(`3D-вид: PASS ${pass}, FAIL ${fail}`);
process.exit(fail ? 1 : 0);
