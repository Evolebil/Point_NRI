// Проверки 3D-вида (этап 1а и дальше): перевод 2D-карты в кубы 5×5×5 фт, луч по кубам, этап 5 — сетка чанка (Surface Nets,
// деревья, тьма), выбор модели и анимации. Запуск: node tests/test_render3d.mjs
// Файл web/js/55_render3d.js исполняется в песочнице без браузера (флаг ?3d=1 выключен — библиотека 3D не грузится);
// для сетки чанка в песочницу подгружается та же сборка Three.js (web/vendor), без WebGL.
import fs from 'node:fs';
import vm from 'node:vm';

const src = fs.readFileSync(new URL('../web/js/55_render3d.js', import.meta.url), 'utf8');
const ctx = vm.createContext({
  location: {search: ''}, URLSearchParams, $: () => ({}), toast: () => {}, console,
  hash: () => .5, lv36: (L, x, y) => { const c = L && L[y] && L[y][x]; return c ? parseInt(c, 36) || 0 : 0; }, FR: [], curIdx: 0, prevIdx: 0, G: {}, cam: {}, MAP: null, AW: 1, AH: 1
});
vm.runInContext(fs.readFileSync(new URL('../web/vendor/three-r159.min.js', import.meta.url), 'utf8'), ctx);
vm.runInContext(src + '\nglobalThis.T = {R3, VM, VZ0, r3Column, r3DynMap, r3Level, r3Z, r3Voxel, vIdx, r3Area, r3TopZ, r3VxMap, r3Nets, r3Buf, r3Tree, r3Open, r3SoftAt, r3ModelKey, r3Role, R3MOD, R3CT};', ctx);
const {R3, VM, VZ0, r3Column, r3DynMap, r3Level, r3Z, r3Voxel, vIdx, r3Area, r3TopZ, r3VxMap, r3Nets, r3Buf, r3Tree, r3Open, r3SoftAt, r3ModelKey, r3Role, R3MOD, R3CT} = ctx.T;

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

// ---- этап 5: сетка чанка ----
// карта W×H из слоёв (строки по H), кубы и сведения столбцов
function buildMap(W, Hh, layers, extra = {}){
  const m = Object.assign({aw: W, ah: Hh, theme: 0}, extra);
  for (const k of ['wall', 'pit', 'water', 'diff', 'climb', 'dark', 'height', 'tree', 'haz', 'ladder', 'deck', 'door', 'strong']) m[k] = layers[k] || Array.from({length: Hh}, () => '0'.repeat(W));
  ctx.MAP = m; ctx.AW = W; ctx.AH = Hh;
  R3.H = 32; R3.vox = new Uint8Array(W * Hh * 32); R3.dynCells = r3DynMap([]); R3.vxo = r3VxMap(); R3.voxEd = new Map();
  for (let y = 0; y < Hh; y++) for (let x = 0; x < W; x++) r3Column(x, y);
}
// Surface Nets по всем чанкам: треугольники (позиции Three: X, Y-высота, Z) и флаг неба
function nets(W, Hh){
  const B = r3Buf();
  for (let y0 = 0; y0 < Hh; y0 += 16) for (let x0 = 0; x0 < W; x0 += 16) for (let z0 = 0; z0 < R3.H; z0 += 16) r3Nets(x0, Math.min(W, x0 + 16), y0, Math.min(Hh, y0 + 16), z0, Math.min(R3.H, z0 + 16), B);
  return B;
}
// замкнутость: каждое ребро треугольника — ровно у двух треугольников
function watertight(B){
  const key = i => B.pos.slice(3 * i, 3 * i + 3).map(v => Math.round(v * 1e4)).join(','), E = new Map();
  for (let t = 0; t < B.pos.length / 9; t++) for (let e = 0; e < 3; e++){
    const a = key(3 * t + e), b = key(3 * t + (e + 1) % 3), k = a < b ? a + '|' + b : b + '|' + a;
    E.set(k, (E.get(k) || 0) + 1);
  }
  let bad = 0; E.forEach(n => { if (n !== 2) bad++; }); return bad;
}
{
  const rows = (W, Hh, f) => Array.from({length: Hh}, (_, y) => Array.from({length: W}, (_, x) => f(x, y)).join(''));
  buildMap(6, 6, {height: rows(6, 6, () => '2')});
  let B = nets(6, 6), tops = [];
  for (let i = 0; i < B.pos.length; i += 3) if (B.pos[i] > 1.01 && B.pos[i] < 4.99 && B.pos[i + 2] > 1.01 && B.pos[i + 2] < 4.99 && B.pos[i + 1] > 1) tops.push(B.pos[i + 1]);
  check('гладко: ровная земля — поверхность ровно на верху кубов (боец стоит там же, где по правилам)', tops.length > 0 && tops.every(v => Math.abs(v - (VZ0 + 2)) < 1e-6), tops.slice(0, 5).join());
  {   // лицевая сторона наружу (в осях Three): у верха земли нормаль треугольника смотрит вверх, иначе его отсечёт видеокарта
    let up = 0, n = 0;
    for (let t = 0; t < B.pos.length; t += 9){ const p = B.pos; if (Math.min(p[t + 1], p[t + 4], p[t + 7]) < VZ0 + 2 - 1e-6 || p[t] < 1.01 || p[t] > 4.99 || p[t + 2] < 1.01 || p[t + 2] > 4.99) continue;
      const ux = p[t + 3] - p[t], uy = p[t + 4] - p[t + 1], uz = p[t + 5] - p[t + 2], vx = p[t + 6] - p[t], vy = p[t + 7] - p[t + 1], vz = p[t + 8] - p[t + 2];
      n++; if (uz * vx - ux * vz > 0) up++; }
    check('гладко: верх земли смотрит вверх (виден сверху)', n > 0 && up === n, `${up}/${n}`);
  }
  check('гладко: остров замкнут, по краю карты дыр нет', B.pos.length > 0 && watertight(B) === 0, watertight(B));
  // карта шире чанка (20 клеток) с холмом и ямой — шов чанков без дыр
  buildMap(20, 18, {height: rows(20, 18, (x, y) => (Math.hypot(x - 15, y - 9) < 4 ? '6' : (x === 3 && y === 3 ? '0' : '3')))});
  B = nets(20, 18);
  check('гладко: холм на шве двух чанков — поверхность замкнута', watertight(B) === 0, watertight(B));
  // стена по слоям — кубы (не сглаживается), земля под ней — мягкая
  buildMap(3, 1, {wall: ['010'], height: ['222']});
  check('стена — рукотворное (кубы), земля — природное (гладко)', !r3SoftAt(1, 0, VZ0 + 2) && r3SoftAt(1, 0, VZ0 + 1) && r3SoftAt(0, 0, VZ0 + 1));
  // дом слоем кубов: крыша (доска под небом) — гладко, пол второго этажа под крышей — кубы, под крышей темно
  buildMap(3, 1, {height: ['000']}, {vox: 1, vx: [[1, 0, VZ0, '5a005a']]});
  check('крыша дома (доска под небом) сглаживается, пол этажа под ней — нет', r3SoftAt(1, 0, VZ0 + 5) && !r3SoftAt(1, 0, VZ0 + 1) && !r3SoftAt(1, 0, VZ0));
  check('тьма без неба: над полом под крышей неба нет, на крыше и рядом с домом — есть', !r3Open(1, 0, VZ0 + 2) && r3Open(1, 0, VZ0 + 6) && r3Open(0, 0, VZ0));
  // треугольники: под открытым небом — светло (aS = 1), в пещере — темно (aS = 0)
  buildMap(5, 5, {height: rows(5, 5, () => '8')}, {vox: 1, vx: [[2, 2, 3, '00']]});
  B = nets(5, 5); let inCave = 0, caveDark = 0, topLit = 0, top = 0;
  for (let t = 0; t < B.sky.length; t += 3){ const y = (B.pos[3 * t + 1] + B.pos[3 * t + 4] + B.pos[3 * t + 7]) / 3, X = B.pos[3 * t], Z = B.pos[3 * t + 2]; if (y < 5.5 && y > 2.5 && X > 1 && X < 4 && Z > 1 && Z < 4){ inCave++; if (!B.sky[t]) caveDark++; } else if (y > VZ0 + 7.5){ top++; if (B.sky[t]) topLit++; } }
  check('пещера: стены полости темнее (без неба), верх земли — под небом; полость в один куб не пропадает', inCave > 0 && caveDark === inCave && top > 0 && topLit === top, `${caveDark}/${inCave} ${topLit}/${top}`);
  // дерево: тонкий ствол в углу клетки, крона над ним; ствол срублен — листва отдельным шаром
  buildMap(3, 1, {tree: ['040']});
  let T3 = r3Buf(); r3Tree(T3, 1, 0, VZ0, 2, 2);
  let xs = [], ys = []; for (let i = 0; i < T3.pos.length; i += 3){ xs.push(T3.pos[i]); ys.push(T3.pos[i + 1]); }
  const tx = xs.slice(0, 72), maxY = Math.max(...ys);
  check('дерево 20 фт: ствол тоньше клетки, крона до верха 4 кубов', Math.max(...tx) - Math.min(...tx) < .4 && Math.abs(maxY - (VZ0 + 4)) < .35 && Math.min(...ys) === VZ0, `${Math.max(...tx) - Math.min(...tx)} ${maxY}`);
}
// модель по типу существа (ct из кадра режима кубов) и анимация
check('модель по типу: гуманоид, зверь, нежить, дракон, слизь; летун — свой вариант; без типа — гуманоид',
  r3ModelKey({ct: 9}) === 'adventurer' && r3ModelKey({ct: 1}) === 'wolf' && r3ModelKey({ct: 1, fl: 1}) === 'bat' && r3ModelKey({ct: 13}) === 'skeleton' &&
  r3ModelKey({ct: 13, fl: 1}) === 'ghost' && r3ModelKey({ct: 4}) === 'dragon' && r3ModelKey({ct: 11}) === 'slime' && r3ModelKey({}) === 'adventurer' && R3CT.length === 14);
{
  // у каждой модели — файл и все названные анимации в нём (опечатка в имени клипа — модель стоит столбом)
  let miss = [];
  for (const [k, M] of Object.entries(R3MOD)){
    let b; try { b = fs.readFileSync(new URL(`../web/assets/models/${k}.glb`, import.meta.url)); } catch (e) { miss.push(k + ': нет файла'); continue; }
    const j = JSON.parse(b.subarray(20, 20 + b.readUInt32LE(12)).toString('utf8')), names = (j.animations || []).map(a => a.name);
    for (const role of ['idle', 'walk', 'run', 'atk', 'hit', 'die']) if (M[role] && !names.some(n => n === M[role] || n.endsWith('|' + M[role]))) miss.push(`${k}.${role}=${M[role]}`);
  }
  check('модели: файлы на месте, клипы анимаций есть', miss.length === 0, miss.join(', '));
}
{
  ctx.stepT0 = 0; ctx.prevIdx = 0; ctx.FR = [{f: [{hp: 10}, {hp: 10}]}];
  const fr = {actor: 0, act: 'attack-melee', f: [{hp: 10}, {hp: 7}]};
  check('анимация: шаг и бег, атака своего хода, удар по нему, смерть, стоит',
    r3Role(fr, 0, fr.f[0], .5, [1, 0]) === 'walk' && r3Role(fr, 0, fr.f[0], .5, [4, 2]) === 'run' && r3Role(fr, 0, fr.f[0], .5, null) === 'atk' &&
    r3Role(fr, 1, fr.f[1], .5, null) === 'hit' && r3Role(fr, 1, {hp: 0}, .5, null) === 'die' && r3Role(fr, 0, fr.f[0], 1, null) === 'idle');
}

console.log(`3D-вид: PASS ${pass}, FAIL ${fail}`);
process.exit(fail ? 1 : 0);
