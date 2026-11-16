// Проверки 3D-вида (этап 1а): перевод 2D-карты в кубы 5×5×5 фт и луч по кубам. Запуск: node tests/test_render3d.mjs
// Файл web/js/55_render3d.js исполняется в песочнице без браузера (флаг ?3d=1 выключен — библиотека 3D не грузится).
import fs from 'node:fs';
import vm from 'node:vm';

const src = fs.readFileSync(new URL('../web/js/55_render3d.js', import.meta.url), 'utf8');
const ctx = vm.createContext({
  location: {search: ''}, URLSearchParams, $: () => ({}), toast: () => {}, console,
  hash: () => .5, lv36: (L, x, y) => { const c = L && L[y] && L[y][x]; return c ? parseInt(c, 36) || 0 : 0; }, FR: [], curIdx: 0, prevIdx: 0, G: {}, cam: {}, MAP: null, AW: 1, AH: 1
});
vm.runInContext(src + '\nglobalThis.T = {R3, VM, VZ0, r3Column, r3DynMap, r3Level, r3Z, r3Voxel, vIdx, r3Area, r3TopZ, r3VxMap};', ctx);
const {R3, VM, VZ0, r3Column, r3DynMap, r3Level, r3Z, r3Voxel, vIdx, r3Area, r3TopZ, r3VxMap} = ctx.T;

// карта 3×1 из слоёв: каждый слой — одна строка
function build(layers, dyn = [], extra = {}){
  const m = Object.assign({aw: 3, ah: 1, theme: 0}, extra);
  for (const k of ['wall', 'pit', 'water', 'diff', 'climb', 'dark', 'height', 'tree', 'haz', 'ladder', 'deck', 'door', 'strong']) m[k] = [layers[k] || '000'];
  ctx.MAP = m; ctx.AW = 3; ctx.AH = 1;
  R3.H = 32; R3.vox = new Uint8Array(3 * 32); R3.dynCells = r3DynMap(dyn); R3.vxo = r3VxMap();
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

// Предпросмотр области в режиме кубов — та же сцена и те же ответы, что T222 в движке (tests/audit/16_*.inc):
// земля до куба G, стена камня 30 фт по x = 6, применивший — средний в (2, 6, G)
{
  ctx.AW = 20; ctx.AH = 20; R3.H = 32; R3.vox = new Uint8Array(20 * 20 * 32);
  const G = VZ0 + 4, set = (x, y, z, m) => { R3.vox[vIdx(x, y, z)] = m; };
  for (let y = 0; y < 20; y++) for (let x = 0; x < 20; x++) for (let z = 0; z < G; z++) set(x, y, z, z === G - 1 ? VM.GRASS : VM.DIRT);
  for (let y = 0; y < 20; y++) for (let z = G; z < G + 6; z++) set(6, y, z, VM.STONE);
  const me = {x: 2, y: 6, z: G}, sph = r3Area(0, 10, me, 5, 6, G);
  check('область: верх столбца — куб над землёй', r3TopZ(3, 3) === G && r3TopZ(6, 3) === G + 6);
  check('сфера у стены: рядом и сбоку — да, за стеной и дальше радиуса — нет', sph(4, 6, G) && sph(5, 8, G) && !sph(7, 6, G) && !sph(5, 9, G));
  check('сфера: на 10 фт над центром — да, на 15 — нет', sph(5, 6, G + 2) && !sph(5, 6, G + 3));
  for (let y = 10; y <= 14; y++) for (let x = 10; x <= 14; x++) set(x, y, G + 1, VM.STONE);
  const roof = r3Area(0, 10, me, 12, 12, G);
  check('сфера под потолком: на плите — нет, под ней рядом — да', !roof(12, 12, G + 2) && roof(13, 12, G));
  set(16, 3, G, VM.LEAF);
  check('листва область не держит', r3Area(0, 10, me, 15, 3, G)(17, 3, G));
  const cone = r3Area(2, 15, me, 5, 6, G), line = r3Area(3, 30, me, 5, 6, G);
  check('конус и линия упираются в стену', cone(4, 6, G) && !cone(4, 10, G) && line(5, 6, G) && !line(8, 6, G));
}

// Слой кубов карты (этап 4, World::vxo): в бою по кубам — поверх слоёв, как voxColumn в движке; в бою по слоям — не виден
build({height: '600', wall: '010'}, [], {vox: 1, vx: [[0, 0, 2, '00.0'], [1, 0, VZ0, '5a0'], [2, 0, VZ0, 'ii1z']]});
check('слой кубов: пещера в земле (воздух), «.» — как по слоям', at(0, 2) === 0 && at(0, 3) === 0 && at(0, 4) === VM.DIRT && at(0, 5) === 0 && at(0, VZ0 + 5) === VM.GRASS);
check('слой кубов: стена дома — камень, крыша — доски, выше — воздух вместо стены 30 фт', at(1, VZ0) === VM.STONE && at(1, VZ0 + 1) === VM.PLANK && at(1, VZ0 + 2) === 0 && at(1, VZ0 + 5) === VM.STONE);
check('слой кубов: лестница — свой материал, дно мира и чужой символ — не ставятся', at(2, VZ0) === VM.LADDV && at(2, VZ0 + 1) === VM.LADDV && at(2, VZ0 + 2) === 0 && at(2, VZ0 + 3) === 0);
ctx.G.mode = 'move';
let hl = r3Voxel({x: 2.5, y: VZ0 + 1.5, z: 3.5}, {x: 0, y: 0, z: -1}, 1e9);
check('луч проходит сквозь лестницу (в режиме хода)', hl === null || hl.y !== 0 || hl.x !== 2, JSON.stringify(hl));
build({height: '600'}, [], {vx: [[0, 0, 2, '00']]});
check('бой по слоям: слой кубов не виден', at(0, 2) === VM.DIRT && at(0, 3) === VM.DIRT);

console.log(`3D-вид: PASS ${pass}, FAIL ${fail}`);
process.exit(fail ? 1 : 0);
