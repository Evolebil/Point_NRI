// ====================================================================================================================
// 3D-вид боя (настройка «Вид боя», флаг ?3d=1 / ?3d=0 перекрывает её). Карта по слоям переводится в кубы 5×5×5 фт прямо здесь;
// рисуется как в макете /demo3d (этап 5): рельеф — Surface Nets по природным кубам, рукотворное — кубами, деревья, камни,
// трава, небо и свет, тьма без неба; бойцы — модели Quaternius по типу существа (пока грузятся — капсулы; настройка «Жетоны»).
// Камера: тактическая, 3-е и 1-е лицо; клетка выбирается лучом по кубам (не по гладкой поверхности: правила — по кубам).
// Те же кадры FR, команды cmd() и щелчки по полю (52_turn.js); очередь кадров (step, seek, onCaughtUp) — общая,
// R3.draw только рисует. Подписи (ХП, урон, броски) и круги дальности — на прежнем холсте cv поверх 3D.
// ====================================================================================================================
const R3 = {
  want: r3Want(),   // 3D выбран (настройка или флаг): без него модуль ничего не делает, библиотека не грузится
  on: false, ready: false, failed: false, loading: false,
  map: null, vox: null, H: 0, NX: 0, NY: 0, NZ: 0, chunks: [], dynFr: null, dynKey: '', dynCells: new Map(),
  fm: [], capGeo: new Map(), fogKey: '', dragged: false, hit: null, voxEd: new Map(), voxAt: 0,   // voxEd — правки кубов боя по клеткам
  view: {tx: 0, ty: 0, tz: 0, yaw: .65, pitch: .95, dist: 30, fdist: 22, kind: 'tac', pp: .45, pd: 5, fp: .25},   // kind: tac / third / first (решение №2)
  fps: 0, fpsN: 0, fpsT0: 0, models: new Map(), tokens: r3TokWant(), tLast: 0
};
const VZ0 = 2;      // кубов под уровнем «0» нынешней карты: дно ущелья
const VCH = 16;     // чанк 16×16×16
const R3DARK = .1;  // тьма без неба (решение владельца 2026-11-16: «почти чёрная»): доля яркости там, куда не достаёт свет бойцов
// материалы кубов: цвет (sRGB, яркая пастель как в макете) и твёрдость для луча
const VM = {AIR:0, GRASS:1, DIRT:2, STONE:3, STRONG:4, WOOD:5, LEAF:6, WATER:7, MUD:8, PLANK:9, DOOR:10, PITF:11,
  LAVA:12, ACID:13, SMOKE:14, ICE:15, STORM:16, NECRO:17, FORCE:18, COBBLE:19, RUBBLE:20, DARKG:21, COVER:22, LADDER:23, CDIRT:24, LADDV:25};   // LADDER — поверхность клетки с лестницей слоя, LADDV — куб «лестница» движка
const VCOL = [0, 0x7cc55a, 0x9a7550, 0x9a9a96, 0x6c6e78, 0x7a5232, 0x5fb548, 0x48bfe0, 0x7a5a36, 0xa77a4c, 0x7a4e2a, 0x25262c,
  0xff6a2a, 0x8fe03a, 0x8e9a72, 0xbfe8ff, 0x6a78e8, 0x5a4876, 0x8a73d9, 0xa8a49a, 0x8a8070, 0x3a3e52, 0x9a8a70, 0xd9b47a, 0x8a8478, 0xb98a4e];

// Вид боя: ?3d=1 / ?3d=0 в адресе, иначе настройка (localStorage dnd_view: '3d' или '2d'; по умолчанию 2D)
// жетоны вместо моделей (необязательное правило, решение №10): настройка, localStorage dnd_tokens
function r3TokWant(){ try { return localStorage.getItem('dnd_tokens') === '1'; } catch (e) { return false; } }
function r3Want(){
  const q = new URLSearchParams(location.search).get('3d');
  if (q === '1' || q === '0') return q === '1';
  try { return localStorage.getItem('dnd_view') === '3d'; } catch (e) { return false; }
}
// ---- загрузка библиотеки: только когда выбран 3D, отдельным файлом с кэшем ----
R3.load = function(){
  if (R3.ready || R3.loading || R3.failed) return;
  R3.loading = true;
  const s = document.createElement('script');
  s.src = '/vendor/three-r159.min.js';
  const go = () => { R3.loading = false; try { R3.init(); } catch (e) { R3.fail('не удалось включить 3D: ' + e.message); } };
  s.onload = () => {   // загрузчик моделей — вторым файлом; не загрузился — бойцы капсулами
    const g = document.createElement('script'); g.src = '/vendor/three-r159-gltf.js';
    g.onload = go; g.onerror = () => { console.error('не загрузился загрузчик моделей — бойцы капсулами'); go(); };
    document.head.appendChild(g);
  };
  s.onerror = () => { R3.loading = false; R3.fail('не загрузилась библиотека 3D'); };
  document.head.appendChild(s);
};
if (R3.want) R3.load();
R3.fail = function(msg){ R3.failed = true; R3.on = false; if (R3.cv) R3.cv.style.display = 'none'; console.error(msg); toast(msg + ' — показан 2D-вид.'); r3SyncSeg(); R3.syncCamBtn(); };
// Настройка «Вид боя»: 3D — следующий бой в режиме кубов (путь, обзор, области — по кубам), идущий бой — 3D-видом сразу;
// 2D — прежний вид и прежний бой по слоям
R3.setWant = function(v){
  R3.want = v;
  try { localStorage.setItem('dnd_view', v ? '3d' : '2d'); } catch (e) {}
  if (v && !R3.ready) R3.load();
  else if (R3.ready){ R3.on = v; R3.cv.style.display = v ? 'block' : 'none'; cam.snap = true; R3.hit = null; $('view3dBtn').textContent = v ? 'Вид: 3D' : 'Вид: 2D'; }
  r3SyncSeg(); if (R3.ready) R3.syncCamBtn();
};
function r3SyncSeg(){
  const seg = $('viewSeg'); if (!seg || !seg.querySelectorAll) return;
  seg.querySelectorAll('[data-view]').forEach(b => b.classList.toggle('on', (b.dataset.view === '3d') === R3.want));
  const n = $('viewNote'); if (n) n.textContent = R3.failed ? '3D не включился на этом устройстве — показан 2D.' : (R3.want ? 'Бой — в объёмном мире из кубов (5×5×5 фт): высота, тоннели, копание, области не сквозь стены. Тяжелее для слабых устройств.' : 'Плоская карта сверху — лёгкий вид для любого устройства.');
}

R3.init = function(){
  const T = THREE, coarse = matchMedia('(pointer: coarse)').matches;
  R3.low = coarse || new URLSearchParams(location.search).get('low') === '1';   // телефон (или ?low=1): пониженная графика — без сглаживания, плотность пикселей до 1.25, без травы, проще свет
  R3.cv = document.createElement('canvas'); R3.cv.id = 'cv3';
  R3.cv.style.cssText = 'position:absolute;inset:0;width:100%;height:100%;display:block';
  stage.insertBefore(R3.cv, cv);
  cv.style.position = 'relative'; cv.style.zIndex = '1';   // подписи и щелчки — на прежнем холсте поверх
  R3.ren = new T.WebGLRenderer({canvas: R3.cv, antialias: !coarse, powerPreference: 'high-performance'});
  R3.ren.setPixelRatio(Math.min(devicePixelRatio || 1, R3.low ? 1.25 : 2));
  R3.cv.addEventListener('webglcontextlost', e => { e.preventDefault(); R3.fail('3D-вид потерял видеокарту'); });
  R3.scene = new T.Scene();
  R3.camera = new T.PerspectiveCamera(45, 1, .1, 1200);
  // свет как в макете: небо (полусфера) и солнце; ночью — тусклее и синее (R3.setSky)
  R3.hemi = new T.HemisphereLight(0xdff1ff, 0x6c7a48, 1.5); R3.sun = new T.DirectionalLight(0xfff0d8, 2.4);
  R3.sun.position.set(40, 70, 25); R3.scene.add(R3.hemi, R3.sun);
  R3.skyU = {top: {value: new T.Color(0x4f9fe6)}, hor: {value: new T.Color(0xcfe9f6)}};   // небо: градиент на сфере вокруг камеры
  R3.sky = new T.Mesh(new T.SphereGeometry(500, 24, 12), new T.ShaderMaterial({side: T.BackSide, depthWrite: false, fog: false, uniforms: R3.skyU,
    vertexShader: 'varying float h; void main(){ h = normalize(position).y; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
    fragmentShader: 'uniform vec3 top, hor; varying float h; void main(){ gl_FragColor = vec4(mix(hor, top, pow(clamp(h, 0.0, 1.0), .6)), 1.0); }'}));
  R3.sky.renderOrder = -1; R3.sky.frustumCulled = false; R3.scene.add(R3.sky);
  // рельеф: плоские грани под светом; в шейдере — туман войны (маска клеток), сетка 5 фт по верхним граням и тьма без неба:
  // вершина знает, открыто ли над ней небо (aS), свет бойцов (uLt: центр и радиус в кубах) снимает тьму в своём круге
  R3.U = {uFog: {value: null}, uFogOn: {value: 0}, uGrid: {value: 0}, uSize: {value: new T.Vector2(1, 1)}, uDark: {value: R3DARK},
    uLt: {value: Array.from({length: 8}, () => new T.Vector4())}, uLtN: {value: 0}};
  R3.mat = R3.low ? new T.MeshLambertMaterial({vertexColors: true, flatShading: true}) : new T.MeshStandardMaterial({vertexColors: true, flatShading: true, roughness: 1, metalness: 0});
  R3.mat.onBeforeCompile = sh => {
    Object.assign(sh.uniforms, R3.U);
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nattribute vec3 aC;\nattribute float aS;\nvarying vec3 vC;\nvarying vec3 vWp;\nvarying float vS;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvC = aC; vWp = position; vS = aS;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\nuniform sampler2D uFog;\nuniform float uFogOn, uGrid, uDark;\nuniform vec2 uSize;\nuniform vec4 uLt[8];\nuniform int uLtN;\nvarying vec3 vC;\nvarying vec3 vWp;\nvarying float vS;')
      .replace('#include <color_fragment>', `#include <color_fragment>
        if (uFogOn > .5) diffuseColor.rgb *= mix(.33, 1., texture2D(uFog, (vC.xy + .5) / uSize).r);
        if (uGrid > .5 && vC.z > .5){ vec2 g = abs(fract(vWp.xz) - .5); if (max(g.x, g.y) > .47) diffuseColor.rgb *= .62; }
        float lt = 0.;
        for (int i = 0; i < 8; i++){ if (i >= uLtN) break; lt = max(lt, 1. - smoothstep(uLt[i].w * .55, uLt[i].w, distance(vWp, uLt[i].xyz))); }
        diffuseColor.rgb *= mix(uDark, 1., max(vS, lt)) * mix(vec3(1.), vec3(1.12, .96, .78), lt * (1. - vS));`);
  };
  R3.ray = new T.Raycaster();
  R3.shadowGeo = new T.CircleGeometry(.5, 20).rotateX(-Math.PI / 2);
  R3.shadowMat = new T.MeshBasicMaterial({color: 0x000000, transparent: true, opacity: .35, depthWrite: false});
  R3.bindInput();
  R3.ready = true; R3.on = R3.want;
  if (!R3.on) R3.cv.style.display = 'none';
  R3.syncCamBtn();
  const b = $('view3dBtn'); if (b){ b.hidden = false; b.textContent = R3.on ? 'Вид: 3D' : 'Вид: 2D'; }
  r3SyncSeg();
};

// ---- мир из кубов: столбец клетки (x, y) по слоям нынешней карты и изменениям арены ----
const vIdx = (x, y, z) => (y * AW + x) * R3.H + z;
function vGet(x, y, z){ return x < 0 || y < 0 || x >= AW || y >= AH || z >= R3.H ? 0 : (z < 0 ? VM.DIRT : R3.vox[vIdx(x, y, z)]); }
const mapN = (L, x, y) => lv36(MAP[L], x, y);
// уровень (по 5 фт = 1 куб), на котором стоит боец: как zF в движке (земля, вершина дерева, настил) + высота полёта
function r3Level(f){
  const gone = R3.dynCells.get(f.x + ',' + f.y) || {};
  return (f.up ? mapN('deck', f.x, f.y) : mapN('height', f.x, f.y) + (gone[4] ? 0 : mapN('tree', f.x, f.y))) + (f.alt || 0);
}
// куб, где ноги: в режиме кубов (бой по ?3d=1, этап 1б) — из кадра движка, иначе — по слоям карты
const r3Z = f => MAP && MAP.vox && f.z != null ? f.z : VZ0 + r3Level(f);
// материалы движка (src/engine/vox.h VoxMat) → материалы рисунка (VM)
const ENG2VM = [VM.AIR, VM.PITF, VM.DIRT, VM.GRASS, VM.DIRT, VM.STONE, VM.STRONG, VM.WOOD, VM.LEAF, VM.WATER, VM.PLANK, VM.DOOR, VM.COVER, VM.RUBBLE, VM.LAVA, VM.ACID, VM.ICE, VM.FORCE, VM.LADDV];
// слой кубов карты (этап 4, World::vxo в движке): столбцы [x, y, нижний куб, материалы base36 движка, '.' — как по слоям]
function r3VxMap(){
  const m = new Map();
  if (MAP && MAP.vox && Array.isArray(MAP.vx)) for (const c of MAP.vx) if (Array.isArray(c) && typeof c[3] === 'string') m.set(c[0] + ',' + c[1], c);
  return m;
}
function r3Column(x, y){
  const V = R3.vox, i0 = vIdx(x, y, 0), H = R3.H, d = R3.dynCells.get(x + ',' + y) || {}, city = !!MAP.theme;
  for (let z = 0; z < H; z++) V[i0 + z] = 0;
  const top = VZ0 + mapN('height', x, y);   // первый куб воздуха над землёй
  const put = (z0, z1, m) => { for (let z = Math.max(0, z0); z < Math.min(H, z1); z++) V[i0 + z] = m; };
  if (mapN('pit', x, y)) put(0, 1, VM.PITF);
  else {
    const hz = mapN('haz', x, y);
    let surf = city ? VM.COBBLE : VM.GRASS;
    if (mapN('water', x, y)) surf = VM.WATER; else if (hz) surf = VM.LAVA + hz - 1; else if (mapN('diff', x, y) || d[2]) surf = VM.MUD;
    else if (mapN('ladder', x, y)) surf = VM.LADDER; else if (mapN('dark', x, y) || d[3]) surf = VM.DARKG;
    if (d[4]) surf = VM.RUBBLE;
    put(0, top - 1, city ? VM.CDIRT : VM.DIRT); put(top - 1, top, surf);
  }
  const broken = !!d[4];
  if (!broken && mapN('wall', x, y)) put(top, top + 6, mapN('strong', x, y) ? VM.STRONG : VM.STONE);   // стена 30 фт
  const th = mapN('tree', x, y);   // дерево своей высоты (10–40 фт): ствол и крона в 2 куба (у куста — 1 и 1)
  if (!broken && th){ const wd = Math.max(1, th - 2); put(top, top + wd, VM.WOOD); put(top + wd, top + th, VM.LEAF); }
  if (!broken && mapN('climb', x, y)) put(top, top + 1, VM.COVER);   // укрытие: низкий ящик (рисуется в полкуба)
  const d0 = mapN('door', x, y);
  if (d0){   // проём 10 фт, над ним перемычка до высоты стены; закрытая дверь — два куба
    const st = d[6] ? 6 : d[7] ? 7 : d[8] ? 8 : 0, open = st ? st === 6 : d0 === 2;
    put(top + 2, top + 6, VM.STONE); if (!open && st !== 8) put(top, top + 2, VM.DOOR);
  }
  const dk = mapN('deck', x, y);
  if (dk && !d[5] && VZ0 + dk - 1 >= top) put(VZ0 + dk - 1, VZ0 + dk, VM.PLANK);   // настил: стоять на его верху
  const o = R3.vxo && R3.vxo.get(x + ',' + y);   // слой кубов карты (пещеры, дома) — поверх слоёв, как voxColumn в движке
  if (o) for (let k = 0; k < o[3].length; k++){
    const e = parseInt(o[3][k], 36), z = o[2] + k;
    if (e >= 0 && e < ENG2VM.length && e !== 1 && z >= 0 && z < H) V[i0 + z] = ENG2VM[e];
  }
  if (d[1]) put(top, top + 4, VM.FORCE);   // временная стена способности
  const ed = R3.voxEd.get(x + ',' + y);   // режим кубов: вырытое и разрушенное в бою — поверх (как vxResync в движке)
  if (ed) for (const [z, m] of ed) if (z >= 0 && z < H) V[i0 + z] = m;
  r3ColInfo(x, y);
}
// изменения арены кадра: клетка -> {вид: 1}; двери — последнее состояние
function r3DynMap(dyn){
  const m = new Map();
  (dyn || []).forEach(d => { const k = d[0] + ',' + d[1], o = m.get(k) || {}; if (d[2] >= 6 && d[2] <= 8){ delete o[6]; delete o[7]; delete o[8]; } o[d[2]] = 1; m.set(k, o); });
  return m;
}
R3.setMap = function(){
  const T = THREE;
  R3.chunks.forEach(c => { if (c && c.mesh){ R3.scene.remove(c.mesh); c.mesh.geometry.dispose(); } });
  R3.fm.forEach(r3FmFree);
  R3.voxEd = new Map(); R3.voxAt = 0;
  R3.fm = []; R3.map = MAP; R3.dynFr = null; R3.fogKey = ''; R3.dynCells = r3DynMap(FR[curIdx] && FR[curIdx].dyn); R3.dynKey = JSON.stringify((FR[curIdx] && FR[curIdx].dyn) || []);
  let maxTop = 0;
  for (let y = 0; y < AH; y++) for (let x = 0; x < AW; x++) maxTop = Math.max(maxTop, VZ0 + Math.max(mapN('height', x, y) + Math.max(6, mapN('tree', x, y)), mapN('deck', x, y)));
  R3.vxo = r3VxMap(); R3.vxo.forEach(c => { maxTop = Math.max(maxTop, c[2] + c[3].length); });
  R3.H = Math.ceil((maxTop + 2) / VCH) * VCH;
  R3.vox = new Uint8Array(AW * AH * R3.H);
  R3.NX = Math.ceil(AW / VCH); R3.NY = Math.ceil(R3.H / VCH); R3.NZ = Math.ceil(AH / VCH);
  for (let y = 0; y < AH; y++) for (let x = 0; x < AW; x++) r3Column(x, y);
  R3.chunks = new Array(R3.NX * R3.NY * R3.NZ).fill(null);
  for (let i = 0; i < R3.chunks.length; i++) R3.buildChunk(i);
  // маска тумана войны: 1 тексель = 1 клетка
  if (R3.U.uFog.value) R3.U.uFog.value.dispose();
  R3.fogData = new Uint8Array(AW * AH * 4).fill(255);
  R3.U.uFog.value = new T.DataTexture(R3.fogData, AW, AH, T.RGBAFormat); R3.U.uFog.value.needsUpdate = true;
  R3.U.uSize.value.set(AW, AH);
  R3.setSky(MAP.day === 0);
  let sum = 0, mx = 0; for (let k = 0; k < AW * AH; k++){ sum += R3.gtop[k]; mx = Math.max(mx, R3.gtop[k]); }
  R3.gAvg = sum / (AW * AH); R3.gMax = mx;
  const v = R3.view; r3Fit(cvW() / cvH()); v.fdist = Math.min(v.dist, 24);
};
// камера «вся арена»: в середину на средней высоте земли (горы — не в склоне), дальше на узком экране (арена — по ширине)
function r3Fit(aspect){
  const v = R3.view; v.tx = AW / 2; v.tz = AH / 2; v.ty = R3.gAvg || VZ0;
  v.dist = Math.max(AW, AH) * 1.05 / Math.min(1, Math.max(.3, aspect || 1)) + ((R3.gMax || 0) - (R3.gAvg || 0));
}
// небо, свет и дымка: день — как в макете, ночь — тёмно-синее небо и луна
R3.setSky = function(night){
  const top = night ? 0x0b1226 : 0x4f9fe6, hor = night ? 0x1f2a44 : 0xcfe9f6, D = Math.max(AW, AH);
  R3.skyU.top.value.set(top); R3.skyU.hor.value.set(hor);
  R3.hemi.color.set(night ? 0x8a9ccc : 0xdff1ff); R3.hemi.groundColor.set(night ? 0x1c2026 : 0x6c7a48); R3.hemi.intensity = night ? .6 : 1.5;
  R3.sun.color.set(night ? 0x9fb4ff : 0xfff0d8); R3.sun.intensity = night ? .55 : 2.4;
  R3.scene.fog = new THREE.Fog(hor, D * 1.4, D * 3);
};
// ---- сетка чанка (сменная функция «чанк -> сетка», ТЗ 3.4): природное — Surface Nets (гладко, плоские грани, как в макете
// /demo3d), рукотворное (стены, двери, полы этажей, ящики) — кубами с острыми углами; деревья, камни и трава — в той же сетке.
// Правила и луч выбора клетки — по кубам (R3.vox), сглаживание только рисует ----
const FACES = [   // [смещение соседа x, z(высота), y], 4 угла грани (X, Y, Z трёх)
  [[0, 1, 0], [[0,1,0],[0,1,1],[1,1,1],[1,1,0]]],
  [[0, -1, 0], [[0,0,0],[1,0,0],[1,0,1],[0,0,1]]],
  [[1, 0, 0], [[1,0,0],[1,1,0],[1,1,1],[1,0,1]]],
  [[-1, 0, 0], [[0,0,0],[0,0,1],[0,1,1],[0,1,0]]],
  [[0, 0, 1], [[0,0,1],[1,0,1],[1,1,1],[0,1,1]]],
  [[0, 0, -1], [[0,0,0],[0,1,0],[1,1,0],[1,0,0]]]];
// природные материалы: ниже земли столбца (по слоям) сглаживаются — земля, камень скал и сводов пещер, вода, лава, дно ущелья
const SOFTM = new Uint8Array(32);
[VM.GRASS, VM.DIRT, VM.STONE, VM.STRONG, VM.MUD, VM.PITF, VM.LAVA, VM.ACID, VM.SMOKE, VM.ICE, VM.STORM, VM.NECRO, VM.COBBLE, VM.RUBBLE, VM.DARKG, VM.LADDER, VM.CDIRT, VM.WATER].forEach(m => { SOFTM[m] = 1; });
// закрывает небо (как vxNoSky в движке: твёрдое и непрозрачное; листва, лёд, барьер, ствол и лестница — нет)
const r3Roofy = m => !!m && m !== VM.WOOD && m !== VM.LEAF && m !== VM.LADDV && m !== VM.WATER && m !== VM.LAVA && m !== VM.ACID && m !== VM.ICE && m !== VM.FORCE;
// столбец: gtop — первый куб воздуха над землёй по слоям, skyz — выше этого куба открыто небо; soft — куб сглаживается (1)
// или пустота под землёй (2: ход копателя, пещера, ступени выхода — вырезается всегда, даже шириной в один куб)
function r3ColInfo(x, y){
  const n = AW * AH, H = R3.H;
  if (!R3.gtop || R3.gtop.length !== n){ R3.gtop = new Int16Array(n); R3.skyz = new Int16Array(n); }
  if (!R3.soft || R3.soft.length !== R3.vox.length) R3.soft = new Uint8Array(R3.vox.length);
  const c = y * AW + x, i0 = vIdx(x, y, 0), V = R3.vox, g = R3.gtop[c] = VZ0 + mapN('height', x, y);
  let sk = 0; for (let z = H - 1; z >= 0; z--) if (r3Roofy(V[i0 + z])){ sk = z + 1; break; }
  R3.skyz[c] = sk;
  for (let z = 0; z < H; z++){ const m = V[i0 + z]; R3.soft[i0 + z] = !m ? (z < g ? 2 : 0) : z < g ? SOFTM[m] : (m === VM.PLANK && z === sk - 1 ? 1 : 0); }   // доска под небом — крыша дома, настил
}
const r3SoftQ = (x, y, z) => x < 0 || y < 0 || z < 0 || x >= AW || y >= AH || z >= R3.H ? 0 : R3.soft[vIdx(x, y, z)];   // за краем и под дном — пусто: рельеф — замкнутый остров
const r3SoftAt = (x, y, z) => r3SoftQ(x, y, z) === 1 ? 1 : 0;
// над кубом воздуха (x, y, z) открыто небо: 1 — да (за краем карты — да)
const r3Open = (x, y, z) => x < 0 || y < 0 || x >= AW || y >= AH || z >= R3.skyz[y * AW + x] ? 1 : 0;
// пестрота цвета: крупные пятна, как в макете
function r3Tint(c, x, y, k){ const n = hash(Math.floor(x / 3) * 7 + 11, Math.floor(y / 3) * 13 + 5); c.multiplyScalar(.9 + n * .14 + k * .05); }
// Surface Nets по мягким кубам чанка: вершина в каждой ячейке со сменой знака, четырёхугольник на каждое ребро со сменой знака
const SN_E = [[0,1],[2,3],[4,5],[6,7],[0,2],[1,3],[4,6],[5,7],[0,4],[1,5],[2,6],[3,7]];
function r3Nets(x0, x1, y0, y1, z0, z1, B){
  const T = THREE, sx = x1 - x0 + 3, sy = y1 - y0 + 3, sz = z1 - z0 + 3;   // углы x0-1..x1+1
  const S = new Float32Array(sx * sy * sz), si = (x, y, z) => (x - x0 + 1) + sx * ((y - y0 + 1) + sy * (z - z0 + 1));
  let any = 0, all = 1;
  for (let z = z0 - 1; z <= z1 + 1; z++) for (let y = y0 - 1; y <= y1 + 1; y++) for (let x = x0 - 1; x <= x1 + 1; x++){
    let n = 0, hole = 0;
    for (let k = 0; k < 8; k++){ const q = r3SoftQ(x - 1 + (k & 1), y - 1 + ((k >> 1) & 1), z - 1 + (k >> 2)); if (q === 1) n++; else if (q === 2) hole = 1; }
    const v = S[si(x, y, z)] = hole ? Math.min(n / 8, .499) : n / 8; if (v >= .5) any = 1; else all = 0;   // угол у пустоты под землёй — пустой: стенка хода — по грани куба
  }
  if (!any || all) return;
  const cx = x1 - x0 + 2, cy = y1 - y0 + 2, CV = new Int32Array(cx * cy * (z1 - z0 + 2)).fill(-1), P = [];
  const ci = (x, y, z) => (x - x0 + 1) + cx * ((y - y0 + 1) + cy * (z - z0 + 1));   // ячейки x0-1..x1
  const cv = (x, y, z) => x < x0 - 1 || y < y0 - 1 || z < z0 - 1 || x > x1 || y > y1 || z > z1 ? -1 : CV[ci(x, y, z)];
  for (let z = z0 - 1; z <= z1; z++) for (let y = y0 - 1; y <= y1; y++) for (let x = x0 - 1; x <= x1; x++){
    const c = []; for (let k = 0; k < 8; k++) c.push(S[si(x + (k & 1), y + ((k >> 1) & 1), z + (k >> 2))]);
    let px = 0, py = 0, pz = 0, n = 0;
    for (const [a, b] of SN_E){ if ((c[a] >= .5) === (c[b] >= .5)) continue;
      const t = (.5 - c[a]) / (c[b] - c[a]);
      px += (a & 1) + t * ((b & 1) - (a & 1)); py += ((a >> 1) & 1) + t * (((b >> 1) & 1) - ((a >> 1) & 1)); pz += (a >> 2) + t * ((b >> 2) - (a >> 2)); n++; }
    if (!n) continue;
    CV[ci(x, y, z)] = P.length / 3; P.push(x + px / n, y + py / n, z + pz / n);
  }
  const N = new T.Vector3(), E = new T.Vector3(), col = new T.Color();
  // цвет и флаги — на весь четырёхугольник (по его средней нормали): иначе на уступах соседние треугольники одного склона
  // красятся по-разному («пила» трава/земля)
  const Q = [new T.Vector3(), new T.Vector3(), new T.Vector3(), new T.Vector3()];
  const tri = (a, b, c, ox, oy, oz) => {   // вершины в осях карты (x, y, z-высота); треугольник — наружу (ox, oy, oz)
    N.subVectors(b, a).cross(E.subVectors(c, a));
    const pts = N.x * ox + N.y * oy + N.z * oz < 0 ? [a, b, c] : [a, c, b];   // оси карты -> оси Three (обмен y и z — зеркало): обход в обратном порядке
    for (const v of pts) B.v(v.x, v.z, v.y, col, qs.gx, qs.gy, qs.top, qs.lit);
  };
  const qs = {gx: 0, gy: 0, top: 0, lit: 1};
  const quad = (a, b, c, d, ox, oy, oz) => {
    if (a < 0 || b < 0 || c < 0 || d < 0) return;
    Q[0].fromArray(P, 3 * a); Q[1].fromArray(P, 3 * b); Q[2].fromArray(P, 3 * c); Q[3].fromArray(P, 3 * d);
    N.subVectors(Q[2], Q[0]).cross(E.subVectors(Q[3], Q[1])); if (N.x * ox + N.y * oy + N.z * oz < 0) N.negate();
    N.normalize();
    const mx = (Q[0].x + Q[1].x + Q[2].x + Q[3].x) / 4, my = (Q[0].y + Q[1].y + Q[2].y + Q[3].y) / 4, mz = (Q[0].z + Q[1].z + Q[2].z + Q[3].z) / 4;
    let m = 0;   // материал под гранью: куб под серединой против нормали, иначе соседи
    for (const dd of [.5, .9, .2]){ const q = vGet(Math.floor(mx - N.x * dd), Math.floor(my - N.y * dd), Math.floor(mz - N.z * dd)); if (q && SOFTM[q] || q === VM.PLANK){ m = q; break; } }
    if (!m) m = VM.DIRT;
    const open = r3Open(Math.floor(mx + N.x * .6), Math.floor(my + N.y * .6), Math.floor(mz + N.z * .6)) || r3Open(Math.floor(mx), Math.floor(my), Math.floor(mz) + 1);   // небо — перед гранью или прямо над ней
    if (m === VM.GRASS && (N.z < .3 || !open)) m = VM.DIRT;   // трава — сверху и на склонах под небом; обрывы и своды пещер — земля
    col.set(VCOL[m]); r3Tint(col, mx, my, hash(Math.floor(mx * 7), Math.floor(my * 7) + Math.floor(mz * 3)));
    if (N.z < .2 && (m === VM.GRASS || m === VM.DIRT)) col.multiplyScalar(.92);
    qs.lit = m === VM.LAVA || m === VM.ACID ? 1 : open; qs.top = N.z > .55 ? 1 : 0; qs.gx = Math.floor(mx); qs.gy = Math.floor(my);
    tri(Q[0], Q[1], Q[2], ox, oy, oz); tri(Q[0], Q[2], Q[3], ox, oy, oz);
  };
  // рёбра от угла (x, y, z) — этого чанка; у края карты — и рёбра за краем (иначе по краю рельефа дыра)
  const xs = x0 === 0 ? -1 : x0, xe = x1 === AW ? AW + 1 : x1, ys = y0 === 0 ? -1 : y0, ye = y1 === AH ? AH + 1 : y1, zs = z0 === 0 ? -1 : z0;
  for (let z = zs; z < z1; z++) for (let y = ys; y < ye; y++) for (let x = xs; x < xe; x++){
    const s0 = S[si(x, y, z)] >= .5, o = s0 ? 1 : -1;
    if (x + 1 <= x1 + 1 && s0 !== (S[si(x + 1, y, z)] >= .5)) quad(cv(x, y - 1, z - 1), cv(x, y, z - 1), cv(x, y, z), cv(x, y - 1, z), o, 0, 0);
    if (y + 1 <= y1 + 1 && s0 !== (S[si(x, y + 1, z)] >= .5)) quad(cv(x - 1, y, z - 1), cv(x, y, z - 1), cv(x, y, z), cv(x - 1, y, z), 0, o, 0);
    if (s0 !== (S[si(x, y, z + 1)] >= .5)) quad(cv(x - 1, y - 1, z), cv(x, y - 1, z), cv(x, y, z), cv(x - 1, y, z), 0, 0, o);
  }
}
// ствол дерева — в углу клетки; угол у каждого дерева свой (по месту, как движок не различает — правила те же)
const TRUNK_W = .34;
function r3TrunkOff(x, y){ const k = Math.floor(hash(x * 31 + 5, y * 17 + 3) * 4); return [(k & 1) ? 1 - TRUNK_W - .06 : .06, (k & 2) ? 1 - TRUNK_W - .06 : .06]; }
// заготовки (без индексов): ствол, крона-шар, крона-конус, камень, пучок травы
let R3G = null;
function r3Geos(){
  if (R3G) return R3G;
  const T = THREE, ni = g => (g.index ? g.toNonIndexed() : g).attributes.position.array;
  return (R3G = {cyl: ni(new T.CylinderGeometry(.7, 1, 1, 6).translate(0, .5, 0)), ball: ni(new T.IcosahedronGeometry(1, 0)), cone: ni(new T.ConeGeometry(1, 1, 7).translate(0, .5, 0)),
    rock: ni(new T.DodecahedronGeometry(1, 0)), tuft: ni(new T.ConeGeometry(.05, .38, 3).translate(0, .19, 0)), m: new T.Matrix4(), q: new T.Quaternion(), e: new T.Euler(), v: new T.Vector3(), s: new T.Vector3()});
}
// заготовка в мир: позиция (X, Y-высота, Z), масштаб, поворот вокруг вертикали
function r3Prop(B, arr, X, Y, Z, sx, sy, sz, rot, col, gx, gy, lit, tilt = 0){
  const G = r3Geos(); G.m.compose(G.v.set(X, Y, Z), G.q.setFromEuler(G.e.set(tilt, rot, tilt * .7)), G.s.set(sx, sy, sz));
  const e = G.m.elements;
  for (let i = 0; i < arr.length; i += 3){ const a = arr[i], b = arr[i + 1], c = arr[i + 2];
    B.v(e[0] * a + e[4] * b + e[8] * c + e[12], e[1] * a + e[5] * b + e[9] * c + e[13], e[2] * a + e[6] * b + e[10] * c + e[14], col, gx, gy, 0, lit); }
}
const LEAFC = [0x5fb548, 0x4ea83f, 0x79c957, 0x3f9a45];
// дерево из кубов движка: ствол (n кубов) и крона (m кубов листвы над ним) — тонкий ствол в углу и низкополигональная крона
function r3Tree(B, x, y, z, nw, nl){
  const G = r3Geos(), k = hash(x * 13 + 7, y * 29 + 1), cells = nw + nl, [ox, oy] = r3TrunkOff(x, y), lit = r3Open(x, y, z + cells);
  const c = new THREE.Color(VCOL[VM.WOOD]), tx = x + ox + TRUNK_W / 2, ty = y + oy + TRUNK_W / 2;
  const trunkH = nl ? nw + nl * .5 : nw, r = .07 + Math.min(cells, 8) * .012;
  r3Prop(B, G.cyl, tx, z, ty, r, trunkH, r, k * 6, c, x, y, lit);
  if (!nl) return;
  const leaf = new THREE.Color(LEAFC[Math.floor(k * 4) % 4]), pine = cells >= 5 ? k < .55 : k < .2, cxp = (tx + x + .5) / 2, cyp = (ty + y + .5) / 2;
  if (pine){ const n = cells >= 6 ? 3 : 2, crown = Math.max(1, nl + nw * .4), base = z + cells - crown;
    for (let i = 0; i < n; i++){ const rr = (.55 + cells * .08) * (1 - i / (n + .6)), h = crown / n * 1.45; r3Prop(B, G.cone, cxp, base + i * crown / n * .8, cyp, rr, h, rr, k * 9 + i, leaf, x, y, lit); } }
  else { const rr = Math.min(1.3, .5 + nl * .22 + cells * .05); r3Prop(B, G.ball, cxp, z + cells - rr * .85, cyp, rr, rr * .9, rr, k * 7, leaf, x, y, lit, k);
    if (cells >= 4){ const r2 = rr * .68; r3Prop(B, G.ball, cxp + (k - .5) * rr, z + cells - rr * 1.25, cyp + (.5 - k) * rr * .6, r2, r2 * .9, r2, k * 11, leaf, x, y, lit, k * .5); } }
}
// лестница (куб «лестница» движка) — тонкая доска у твёрдого соседа (стены дома), иначе посреди клетки
const LADDER_W = .14;
function r3Ladder(B, x, y, z, c){
  let ox = .5 - LADDER_W / 2, oy = 0, sx = LADDER_W, sy = 1;
  for (const [ax, ay] of [[0, 1], [0, -1], [1, 0], [-1, 0]]) if (vOpaque(vGet(x + ax, y + ay, z)) || vOpaque(vGet(x + ax, y + ay, z + 1))){
    if (ax) { ox = ax > 0 ? 1 - LADDER_W : 0; oy = 0; sx = LADDER_W; sy = 1; } else { ox = 0; oy = ay > 0 ? 1 - LADDER_W : 0; sx = 1; sy = LADDER_W; }
    break;
  }
  const up = vGet(x, y, z + 1), lit = r3Open(x, y, z);
  for (const [[dx, dz, dy], quad] of FACES){
    if (dz === 1 && up === VM.LADDV) continue;
    c.set(VCOL[VM.LADDV]).multiplyScalar((z & 1) ? .9 : 1.05);   // ступени — полосами
    B.quad(quad.map(([qx, qy, qz]) => [x + ox + qx * sx, z + qy, y + oy + qz * sy]), c, x, y, 0, lit);
  }
}
const vOpaque = m => m !== 0 && m !== VM.COVER && m !== VM.WOOD && m !== VM.LADDV && m !== VM.LEAF;   // ствол, лестница, листва — грани соседей за ними видны
// построитель: массивы вершин (без индексов), цвет, клетка (x, y, верхняя грань — для сетки 5 фт), открыто ли небо
function r3Buf(){
  const pos = [], col = [], cel = [], sky = [];
  return {pos, col, cel, sky,
    v(X, Y, Z, c, gx, gy, top, lit){ pos.push(X, Y, Z); col.push(c.r, c.g, c.b); cel.push(gx, gy, top); sky.push(lit); },
    quad(q, c, gx, gy, top, lit){ for (const i of [0, 1, 2, 0, 2, 3]) this.v(q[i][0], q[i][1], q[i][2], c, gx, gy, top, lit); }};
}
R3.buildChunk = function(ci){
  const T = THREE, old = R3.chunks[ci];
  if (old && old.mesh){ R3.scene.remove(old.mesh); old.mesh.geometry.dispose(); }
  const cx = ci % R3.NX, cy = Math.floor(ci / R3.NX) % R3.NY, cz = Math.floor(ci / (R3.NX * R3.NY));
  const x0 = cx * VCH, x1 = Math.min(AW, x0 + VCH), y0 = cz * VCH, y1 = Math.min(AH, y0 + VCH), z0 = cy * VCH, z1 = Math.min(R3.H, z0 + VCH);
  const B = r3Buf(), c = new T.Color(), G = r3Geos();
  r3Nets(x0, x1, y0, y1, z0, z1, B);
  for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++){
    const g = R3.gtop[y * AW + x];
    for (let z = z0; z < z1; z++){
      const i = vIdx(x, y, z), m = R3.vox[i]; if (!m || R3.soft[i]) continue;
      if (m === VM.WOOD){ if (vGet(x, y, z - 1) !== VM.WOOD){ let nw = 1, nl = 0; while (vGet(x, y, z + nw) === VM.WOOD) nw++; while (vGet(x, y, z + nw + nl) === VM.LEAF) nl++; r3Tree(B, x, y, z, nw, nl); } continue; }
      if (m === VM.LEAF){   // листва без ствола под ней (ствол срублен) — свой шарик
        let d = z - 1; while (vGet(x, y, d) === VM.LEAF) d--; if (vGet(x, y, d) === VM.WOOD) continue;
        c.set(LEAFC[(x + y + z) & 3]); r3Prop(B, G.ball, x + .5, z + .45, y + .5, .55, .5, .55, hash(x, y + z) * 6, c, x, y, r3Open(x, y, z)); continue;
      }
      if (m === VM.LADDV){ r3Ladder(B, x, y, z, c); continue; }
      const hgt = m === VM.COVER ? .5 : 1, jit = .94 + hash(x * 7 + z, y * 13 + z) * .12;   // ящик — в полкуба; лёгкая пестрота кубов
      for (const [[dx, dz, dy], quad] of FACES){
        const nb = vGet(x + dx, y + dy, z + dz);
        if (hgt === 1 && vOpaque(nb) && !r3SoftAt(x + dx, y + dy, z + dz)) continue;   // грань закрыта рукотворным соседом
        if (dz === -1 && (z === 0 || r3SoftAt(x, y, z - 1))) continue;   // стоит на земле: низ не виден
        c.set(VCOL[m]).multiplyScalar(jit); const top = dz === 1 && m !== VM.COVER ? 1 : 0;
        B.quad(quad.map(([qx, qy, qz]) => [x + qx, z + qy * hgt, y + qz]), c, x, y, top, r3Open(x + dx, y + dy, z + dz));
      }
    }
    // камни и трава: на ровной траве под небом, по месту (детерминированно; правила их не видят)
    if (g - 1 < z0 || g - 1 >= z1 || vGet(x, y, g - 1) !== VM.GRASS || vGet(x, y, g) || !r3Open(x, y, g)) continue;
    if ([[1, 0], [-1, 0], [0, 1], [0, -1]].some(([a, b]) => x + a >= 0 && y + b >= 0 && x + a < AW && y + b < AH && R3.gtop[(y + b) * AW + x + a] !== g)) continue;
    const k = hash(x * 17 + 3, y * 23 + 9);
    if (k < .035){ c.set(VCOL[VM.STONE]).multiplyScalar(.95 + k * 2); const r = .14 + k * 6; r3Prop(B, G.rock, x + .3 + k * 9 % .4, g - .05, y + .35 + k * 13 % .3, r, r * .65, r * .85, k * 90, c, x, y, 1, k * 20); }
    if (R3.low) continue;
    for (let t = 0, nt = 2 + Math.floor(k * 3); t < nt; t++){
      const a = hash(x * 5 + t * 31, y * 7 + t * 17), b = hash(x * 11 + t * 13, y * 3 + t * 41);
      c.set(VCOL[VM.GRASS]).multiplyScalar(.85 + a * .3); r3Prop(B, G.tuft, x + .1 + a * .8, g, y + .1 + b * .8, .5 + b * .4, .35 + a * .45, .5 + b * .4, a * 6, c, x, y, 1, (b - .5) * .4);   // пучок — по щиколотку среднему
    }
  }
  if (!B.pos.length){ R3.chunks[ci] = {mesh: null}; return; }
  const geo = new T.BufferGeometry();
  geo.setAttribute('position', new T.Float32BufferAttribute(B.pos, 3));
  geo.setAttribute('color', new T.Float32BufferAttribute(B.col, 3));
  geo.setAttribute('aC', new T.Float32BufferAttribute(B.cel, 3));
  geo.setAttribute('aS', new T.Float32BufferAttribute(B.sky, 1));
  geo.computeVertexNormals(); geo.computeBoundingSphere();
  const mesh = new T.Mesh(geo, R3.mat); mesh.matrixAutoUpdate = false;
  R3.scene.add(mesh); R3.chunks[ci] = {mesh};
};
// изменения арены (временная стена, обломки, двери, рухнувший настил): перестроить только затронутые столбцы и чанки
R3.syncDyn = function(fr){
  if (R3.dynFr === fr) return; R3.dynFr = fr;
  const key = JSON.stringify(fr.dyn || []); if (key === R3.dynKey) return;
  const was = R3.dynCells; R3.dynCells = r3DynMap(fr.dyn); R3.dynKey = key;
  const cells = new Set([...was.keys(), ...R3.dynCells.keys()]), dirty = new Set();
  cells.forEach(k => {
    const [x, y] = k.split(',').map(Number); if (x < 0 || y < 0 || x >= AW || y >= AH) return;
    r3Column(x, y);
    for (let ay = -1; ay <= 1; ay++) for (let ax = -1; ax <= 1; ax++){   // соседний чанк: его грани и гладкая поверхность у границы тоже меняются
      const nx = x + ax, ny = y + ay; if (nx < 0 || ny < 0 || nx >= AW || ny >= AH) continue;
      for (let cy = 0; cy < R3.NY; cy++) dirty.add(Math.floor(nx / VCH) + R3.NX * (cy + R3.NY * Math.floor(ny / VCH)));
    }
  });
  dirty.forEach(ci => R3.buildChunk(ci));
};
// режим кубов: события «vox» из кадров (разрушен блок, вырыт ход) — по порядку кадров; назад по кадрам — мир заново
R3.syncVox = function(){
  if (!MAP.vox) return;
  if (curIdx < R3.voxAt) R3.setMap();
  if (curIdx <= R3.voxAt) return;
  const dirty = new Set(), cols = new Set();
  for (let k = R3.voxAt + 1; k <= curIdx; k++) for (const e of (FR[k] && FR[k].ev) || []){
    if (e.k !== 'vox' || e.x < 0 || e.y < 0 || e.x >= AW || e.y >= AH || e.z < 0 || e.z >= R3.H) continue;
    const m = ENG2VM[e.m] != null ? ENG2VM[e.m] : VM.AIR, key = e.x + ',' + e.y, ed = R3.voxEd.get(key) || [];
    const old = ed.findIndex(q => q[0] === e.z); if (old >= 0) ed.splice(old, 1); ed.push([e.z, m]); R3.voxEd.set(key, ed);
    R3.vox[vIdx(e.x, e.y, e.z)] = m; cols.add(e.x + e.y * AW);
    for (let ay = -1; ay <= 1; ay++) for (let ax = -1; ax <= 1; ax++){   // соседний чанк: грани, гладкая поверхность и дерево (весь столбец) тоже меняются
      const nx = e.x + ax, ny = e.y + ay; if (nx < 0 || ny < 0 || nx >= AW || ny >= AH) continue;
      for (let cy = 0; cy < R3.NY; cy++) dirty.add(Math.floor(nx / VCH) + R3.NX * (cy + R3.NY * Math.floor(ny / VCH)));
    }
  }
  R3.voxAt = curIdx;
  cols.forEach(c => r3ColInfo(c % AW, Math.floor(c / AW)));   // небо и сглаживание столбца — по новым кубам
  dirty.forEach(ci => R3.buildChunk(ci));
};
// туман войны: та же маска, что у 2D (бит на клетку из запроса хода)
R3.syncFog = function(){
  const q = G.req || G.lastReq, on = !!(q && q.vis && !G.over && q.visW === AW && q.visH === AH);
  R3.U.uFogOn.value = on ? 1 : 0; if (!on || q.vis === R3.fogKey) return;
  R3.fogKey = q.vis; const bin = atob(q.vis);
  for (let k = 0; k < AW * AH; k++) R3.fogData[4 * k] = ((bin.charCodeAt(k >> 3) >> (k & 7)) & 1) ? 255 : 0;
  R3.U.uFog.value.needsUpdate = true;
};

// ---- бойцы: модели Quaternius (CC0, web/assets/LICENSES.md) по типу существа и размеру; пока модель грузится или
// загрузчика нет — капсула цвета стороны (она же всегда — невидимая цель луча и силуэт сквозь стены); жетоны — по настройке ----
function r3Cap(n, sz){
  const key = n === 1 && sz != null && sz < 2 ? 's' + sz : n;
  let g = R3.capGeo.get(key); if (g) return g;
  const r = key === 's0' ? .1 : key === 's1' ? .17 : n > 1 ? .3 * n : .28, len = key === 's0' ? .04 : key === 's1' ? .15 : n > 1 ? .95 * n - 2 * r : .4;
  g = new THREE.CapsuleGeometry(r, len, 4, 12); g.userData.h = len + 2 * r; R3.capGeo.set(key, g); return g;
}
// модель и её анимации (окончание имени клипа): стоять, идти, бежать, атака, удар, смерть; fly — висит в воздухе (летает всегда)
const R3MOD = {
  adventurer: {idle: 'Idle', walk: 'Walk', run: 'Run', atk: 'Sword_Slash', hit: 'HitRecieve', die: 'Death'},
  orc: {idle: 'Idle', walk: 'Walk', run: 'Run', atk: 'Weapon', hit: 'HitReact', die: 'Death'},
  wolf: {idle: 'Idle', walk: 'Walk', run: 'Gallop', atk: 'Attack', hit: 'Idle_HitReact_Left', die: 'Death'},
  bat: {idle: 'Bat_Flying', walk: 'Bat_Flying', run: 'Bat_Flying', atk: 'Bat_Attack', hit: 'Bat_Hit', die: 'Bat_Death', fly: 1},
  skeleton: {idle: 'Skeleton_Idle', walk: 'Skeleton_Running', run: 'Skeleton_Running', atk: 'Skeleton_Attack', die: 'Skeleton_Death'},
  ghost: {idle: 'Flying_Idle', walk: 'Fast_Flying', run: 'Fast_Flying', atk: 'Punch', hit: 'HitReact', die: 'Death', fly: 1},
  yeti: {idle: 'Idle', walk: 'Walk', run: 'Run', atk: 'Punch', hit: 'HitReact', die: 'Death'},
  demon: {idle: 'Idle', walk: 'Walk', run: 'Run', atk: 'Weapon', hit: 'HitReact', die: 'Death'},
  alien: {idle: 'Idle', walk: 'Walk', run: 'Walk', atk: 'Bite_Front', hit: 'HitRecieve', die: 'Death'},
  slime: {idle: 'Slime_Idle', walk: 'Slime_Walk', run: 'Slime_Walk', atk: 'Slime_Attack', die: 'Slime_Death'},
  robot: {idle: 'Robot_Idle', walk: 'Robot_Walking', run: 'Robot_Running', atk: 'Robot_Punch', die: 'Robot_Death'},
  dragon: {idle: 'Dragon_Flying', walk: 'Dragon_Flying', run: 'Dragon_Flying', atk: 'Dragon_Attack', hit: 'Dragon_Hit', die: 'Dragon_Death', fly: 1},
  mushroom: {idle: 'Idle', walk: 'Walk', run: 'Run', atk: 'Punch', hit: 'HitReact', die: 'Death'},
  hywirl: {idle: 'Flying_Idle', walk: 'Fast_Flying', run: 'Fast_Flying', atk: 'Punch', hit: 'HitReact', die: 'Death', fly: 1}};
// тип существа (ct в кадре режима кубов; 0–13, как в конструкторе) и полёт (fl) -> модель
const R3CT = ['alien', 'wolf', 'adventurer', 'robot', 'dragon', 'hywirl', 'adventurer', 'demon', 'orc', 'adventurer', 'yeti', 'slime', 'mushroom', 'skeleton'];
function r3ModelKey(f){
  const ct = f.ct != null && f.ct >= 0 && f.ct < 14 ? f.ct : 9;
  if (f.fl && ct === 1) return 'bat';
  if (f.fl && ct === 13) return 'ghost';
  return R3CT[ct];
}
// модель: загрузка один раз (по требованию), размеры исходника для подгонки
function r3Model(key){
  let e = R3.models.get(key); if (e) return e;
  e = {state: 'load'}; R3.models.set(key, e);
  if (!THREE.GLTFLoader){ e.state = 'fail'; return e; }
  new THREE.GLTFLoader().load('/assets/models/' + key + '.glb', g => {
    const sc = g.scene; sc.updateMatrixWorld(true);
    const box = new THREE.Box3().setFromObject(sc), sz = box.getSize(new THREE.Vector3());
    Object.assign(e, {state: 'ok', g, minY: box.min.y, cx: (box.min.x + box.max.x) / 2, cz: (box.min.z + box.max.z) / 2, h: Math.max(.01, sz.y), w: Math.max(.01, sz.x, sz.z)});
  }, undefined, err => { e.state = 'fail'; console.error('модель ' + key + ' не загрузилась', err); });
  return e;
}
// экземпляр модели для бойца: копия со своим скелетом, материалами (для тьмы) и проигрывателем анимаций
function r3Inst(e, key, h, n){
  const T = THREE, obj = THREE.SkeletonUtils.clone(e.g.scene), inner = new T.Group(), outer = new T.Group(), mats = [];
  const k = Math.min(h / e.h, .95 * n / e.w);   // рост по размеру, но не шире своей клетки (n×n)
  obj.traverse(o => { if (o.isMesh){ o.frustumCulled = false; o.material = [].concat(o.material).map(m => { const c = r3Stamp(m.clone()); mats.push([c, c.color.clone()]); return c; }); if (o.material.length === 1) o.material = o.material[0]; } });
  inner.add(obj); inner.scale.setScalar(k); inner.position.set(-e.cx * k, -e.minY * k, -e.cz * k); outer.add(inner);
  const mixer = new T.AnimationMixer(obj), acts = {}, M = R3MOD[key];
  for (const role of ['idle', 'walk', 'run', 'atk', 'hit', 'die']){
    const want = M[role]; if (!want) continue;
    const clip = e.g.animations.find(c => c.name === want || c.name.endsWith('|' + want)); if (!clip) continue;
    const a = mixer.clipAction(clip); if (role === 'atk' || role === 'hit' || role === 'die'){ a.setLoop(T.LoopOnce, 1); a.clampWhenFinished = true; }
    acts[role] = a;
  }
  return {outer, mixer, acts, mats, role: '', dim: 1, fly: !!M.fly, hgt: e.h * k};
}
// силуэт сквозь стены: боец метит трафарет (1), силуэт рисуется только там, где боец закрыт миром (глубина больше), и не поверх бойцов
function r3Stamp(m){ return Object.assign(m, {stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc, stencilZPass: THREE.ReplaceStencilOp}); }
function r3GhostMat(col){
  return new THREE.MeshBasicMaterial({color: col, transparent: true, opacity: .32, depthTest: true, depthFunc: THREE.GreaterDepth, depthWrite: false,
    stencilWrite: true, stencilRef: 1, stencilFunc: THREE.NotEqualStencilFunc, stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp, stencilZPass: THREE.KeepStencilOp});
}
// убрать модель и жетон бойца (свои материалы и текстура портрета освобождаются; геометрия и клипы — общие у модели)
function r3DropView(m){
  if (m.mi){ R3.scene.remove(m.mi.outer); m.mi.mixer.stopAllAction(); m.mi.mixer.uncacheRoot(m.mi.mixer.getRoot()); m.mi.mats.forEach(([c]) => c.dispose()); m.mi = null; }
  if (m.tok){ R3.scene.remove(m.tok); m.tok.children.forEach(o => { if (o.material.map) o.material.map.dispose(); o.material.dispose(); }); m.tok = null; }
  m.key = '';
}
function r3FmFree(m){
  if (!m) return;
  R3.scene.remove(m.body, m.shadow, m.ghost); m.mat.dispose(); m.gmat.dispose();
  r3DropView(m);
}
// жетон: подставка цвета стороны и портрет (или буква) — всегда лицом к камере
function r3Token(f, n, col){
  const T = THREE, g = new T.Group(), r = .42 * n;
  const base = new T.Mesh(R3.tokGeo || (R3.tokGeo = new T.CylinderGeometry(1, 1.05, .08, 24).translate(0, .04, 0)), r3Stamp(new T.MeshLambertMaterial({color: col})));
  base.scale.set(r, 1, r); g.add(base);
  const cv2 = document.createElement('canvas'); cv2.width = cv2.height = 128; const tex = new T.CanvasTexture(cv2); tex.colorSpace = T.SRGBColorSpace;
  const sp = new T.Sprite(r3Stamp(new T.SpriteMaterial({map: tex, transparent: true}))); sp.scale.set(r * 1.9, r * 1.9, 1); sp.position.y = .1 + r * .95; g.add(sp);
  g.userData = {cv: cv2, tex, drawn: ''};
  return g;
}
function r3TokenDraw(g, f, col){
  const img = tokenPortrait(f), key = (img ? img.src : '') + col; if (g.userData.drawn === key) return;
  const c = g.userData.cv.getContext('2d'); c.clearRect(0, 0, 128, 128);
  c.beginPath(); c.arc(64, 64, 60, 0, 7); c.fillStyle = col; c.fill();
  if (img){ c.save(); c.beginPath(); c.arc(64, 64, 54, 0, 7); c.clip(); c.drawImage(img, 10, 10, 108, 108); c.restore(); }
  c.lineWidth = 6; c.strokeStyle = '#fff'; c.beginPath(); c.arc(64, 64, 57, 0, 7); c.stroke();
  g.userData.tex.needsUpdate = true; g.userData.drawn = key;
}
// крошечные в одной клетке (до 4, режим кубов): каждый — в свою четверть клетки
function r3TinySlots(fr){
  const by = new Map(), slot = [];
  fr.f.forEach((f, k) => { if (f.sz !== 0 || f.hp <= 0) return; const c = f.x + ',' + f.y + ',' + f.z; if (!by.has(c)) by.set(c, []); by.get(c).push(k); });
  by.forEach(l => { if (l.length > 1) l.forEach((k, i) => { slot[k] = [(i & 1) ? .25 : -.25, (i & 2) ? .25 : -.25]; }); });
  return slot;
}
function r3Colors(f){
  const side = f.kind % 2, tc = f.lead != null && typeof teamCol === 'function' ? teamCol(f.team) : null;
  return f.hp <= 0 ? '#8a8a8a' : (tc || (side ? '#e0644f' : '#4f8fe6'));
}
// роль анимации бойца k в кадре: смерть, движение, атака (свой ход: удар, выстрел, способность), получил урон, стоит
function r3Role(fr, k, f, t, moved){
  if (f.hp <= 0) return 'die';
  const live = stepT0 > -1e8 && t < 1;
  if (live && moved) return Math.hypot(moved[0], moved[1]) > 3 ? 'run' : 'walk';
  if (live && fr.actor === k && /^(attack|ability|alt|legendary)/.test(fr.act || '')) return 'atk';
  const a = FR[prevIdx] && FR[prevIdx].f[k]; if (live && a && f.hp < a.hp) return 'hit';
  return 'idle';
}
// свет бойцов (lt — радиус в футах, режим кубов) и тьма: боец под крышей без света рядом — темнее, как рельеф
function r3Lights(list){
  const L = R3.U.uLt.value; let n = 0;
  for (const o of list){ if (n >= 8) break; if (o.f.lt > 0 && o.f.hp > 0) L[n++].set(o.X, o.Y + o.h * .7, o.Z, o.f.lt / 5 + .5); }
  R3.U.uLtN.value = n;
  for (const o of list){
    const covered = MAP.vox && o.f.z != null && !r3Open(o.f.x, o.f.y, o.f.z + Math.max(1, o.n));
    let lit = covered ? 0 : 1;
    for (let i = 0; i < n && lit < 1; i++) lit = Math.max(lit, 1 - Math.min(1, Math.max(0, (Math.hypot(L[i].x - o.X, L[i].y - o.Y - o.h * .5, L[i].z - o.Z) - L[i].w * .55) / (L[i].w * .45))));
    o.dim = R3DARK + (1 - R3DARK) * lit;
  }
}
R3.syncFighters = function(fr, t, dt){
  const out = [], tiny = r3TinySlots(fr), T = THREE;
  R3.selfObj = null;
  fr.f.forEach((f, k) => {
    let m = R3.fm[k];
    if (!m){
      const mat = r3Stamp(new T.MeshLambertMaterial({color: r3Colors(f)})), gmat = r3GhostMat(r3Colors(f));
      m = R3.fm[k] = {mat, gmat, body: new T.Mesh(r3Cap(f.n || 1, f.sz), mat), shadow: new T.Mesh(R3.shadowGeo, R3.shadowMat), n: f.n || 1, sz: f.sz, col: '', key: '', yaw: 0};
      m.ghost = new T.Mesh(m.body.geometry, gmat); m.ghost.renderOrder = 10;   // силуэт сквозь стены и деревья: бойца не теряем за кубами
      m.body.userData.k = k; R3.scene.add(m.body, m.shadow, m.ghost);
    }
    const n = f.n || 1, boss = f.ld != null ? !!f.ld : f.kind < 2;
    if (m.n !== n || m.sz !== f.sz){ m.body.geometry = m.ghost.geometry = r3Cap(n, f.sz); m.n = n; m.sz = f.sz; r3DropView(m); }   // новый размер — модель заново
    const hide = (f.hid && typeof G !== 'undefined') || (f.hp <= 0 && !boss) || (R3.view.kind === 'first' && k === (G.req ? G.req.me : G.me));   // 1-е лицо: себя не видно
    const h = m.body.geometry.userData.h;
    // вид: жетон (настройка), модель (когда загрузилась) или капсула
    const key = R3.tokens ? 'tok' : r3ModelKey(f);
    if (key !== m.key){
      if (m.mi || m.tok) r3DropView(m);
      if (key === 'tok'){ m.tok = r3Token(f, n, r3Colors(f)); R3.scene.add(m.tok); m.key = key; }
      else { const e = r3Model(key); if (e.state === 'ok'){ m.mi = r3Inst(e, key, h, n); R3.scene.add(m.mi.outer); } if (e.state !== 'load') m.key = key; }   // загружается — капсула, попробуем в следующем кадре
    }
    const shown = m.mi || m.tok;
    m.body.visible = m.shadow.visible = m.ghost.visible = !hide; m.mat.visible = !shown;   // капсула прозрачна для глаза, но ловит луч
    if (m.mi) m.mi.outer.visible = !hide; if (m.tok) m.tok.visible = !hide;
    if (hide){   // свой боец не виден (1-е лицо, скрыт, пал) — но камера 1-го и 3-го лица стоит при нём, а не у бойца 0
      if (k === (G.req ? G.req.me : G.me)){ const p = posAt(k, t), a = FR[prevIdx].f[k]; R3.selfObj = {k, f, X: p.x + n / 2, Y: a ? lerp(r3Z(a), r3Z(f), t) : r3Z(f), Z: p.y + n / 2, h, n}; }
      return;
    }
    const c = r3Colors(f); if (c !== m.col){ m.mat.color.set(c); m.gmat.color.set(c); m.col = c; if (m.tok) m.tok.userData.dim = -1; }
    const p = posAt(k, t), a = FR[prevIdx].f[k], tele = FR[curIdx].act === 'teleport' && FR[curIdx].actor === k;
    const z = a && !tele ? lerp(r3Z(a), r3Z(f), t) : r3Z(f);
    const ts = tiny[k], X = p.x + n / 2 + (ts ? ts[0] : 0), Z = p.y + n / 2 + (ts ? ts[1] : 0);
    const A = fr.actor, act = k === A && stepT0 > -1e8 && t < 1;
    m.mat.emissive.set(act ? 0x6a5414 : 0x000000);
    if (f.hp <= 0){ m.body.rotation.set(0, 0, Math.PI / 2); m.body.position.set(X, z + (f.sz === 0 ? .1 : f.sz === 1 ? .17 : .3 * n), Z); }   // павший лежит
    else { m.body.rotation.set(0, 0, 0); m.body.position.set(X, z + h / 2, Z); }
    m.shadow.scale.setScalar(f.sz === 0 ? .35 : f.sz === 1 ? .6 : .9 * n); m.shadow.position.set(X, (MAP.vox ? r3Z(f) : VZ0 + r3Level(f) - (f.alt || 0)) + .02, Z);
    m.ghost.position.copy(m.body.position); m.ghost.rotation.copy(m.body.rotation);
    // поворот: по ходу движения, иначе — к цели своего действия, иначе к ближайшему живому врагу
    const moved = a && (a.x !== f.x || a.y !== f.y) && !tele ? [f.x - a.x, f.y - a.y] : null;
    let face = null;
    if (moved) face = moved;
    else { const tg = fr.actor === k && fr.target >= 0 ? fr.f[fr.target] : null;
      if (tg && tg !== f) face = [tg.x - f.x, tg.y - f.y];
      else { let best = 1e9; fr.f.forEach(g => { if (g.team !== f.team && g.hp > 0){ const d = Math.hypot(g.x - f.x, g.y - f.y); if (d < best){ best = d; face = [g.x - f.x, g.y - f.y]; } } }); } }
    if (face && (face[0] || face[1])){ const want = Math.atan2(face[0], face[1]); let d = want - m.yaw; d = Math.atan2(Math.sin(d), Math.cos(d)); m.yaw += d * Math.min(1, dt * 8); }
    if (m.mi){
      const mi = m.mi, role = r3Role(fr, k, f, t, moved);
      if (role !== mi.role){   // смена анимации — плавно (нет своей — ближайшая: бег -> шаг, остальное -> стоять)
        const next = mi.acts[role] || mi.acts[role === 'run' ? 'walk' : 'idle'] || mi.acts.idle;
        if (next && next !== mi.cur){ next.reset().play(); if (mi.cur) next.crossFadeFrom(mi.cur, .18, false); mi.cur = next; }
        mi.role = role;
      }
      mi.mixer.update(dt);
      mi.outer.position.set(X, z, Z); mi.outer.rotation.y = m.yaw;
    }
    if (m.tok){ m.tok.position.set(X, z, Z); r3TokenDraw(m.tok, f, c); }
    out.push({k, f, X, Y: z, Z, h: m.mi && f.hp > 0 ? Math.max(.3, m.mi.hgt) : h, n, boss, m});   // подписи и портрет — над головой модели
  });
  for (let k = fr.f.length; k < R3.fm.length; k++) if (R3.fm[k]){ const m = R3.fm[k]; m.body.visible = m.shadow.visible = m.ghost.visible = false; if (m.mi) m.mi.outer.visible = false; if (m.tok) m.tok.visible = false; }
  r3Lights(out);
  out.forEach(o => {   // тьма: модель и жетон темнее там, где темно (материалы — свои у каждого бойца)
    const mi = o.m.mi, tk = o.m.tok;
    if (mi && Math.abs(mi.dim - o.dim) >= .01){ mi.dim = o.dim; mi.mats.forEach(([c, base]) => c.color.copy(base).multiplyScalar(o.dim)); }
    if (tk && tk.userData.dim !== o.dim){ tk.userData.dim = o.dim; tk.children[0].material.color.set(o.m.col).multiplyScalar(o.dim); tk.children[1].material.color.setScalar(o.dim); }
  });
  return out;
};
// настройка «Жетоны вместо моделей»: на ходу — бойцы пересоздаются в следующем кадре
R3.setTokens = function(v){
  R3.tokens = v; try { localStorage.setItem('dnd_tokens', v ? '1' : '0'); } catch (e) {}
  const seg = $('tokSeg'); if (seg && seg.querySelectorAll) seg.querySelectorAll('[data-tok]').forEach(b => b.classList.toggle('on', (b.dataset.tok === '1') === v));
};
if ($('tokSeg') && $('tokSeg').addEventListener){
  $('tokSeg').addEventListener('click', e => { const b = e.target.closest('[data-tok]'); if (b) R3.setTokens(b.dataset.tok === '1'); });
  R3.setTokens(R3.tokens);
}

// ---- камера: тактическая (вращение, наклон, приближение); режимы кнопки «Камера» — те же, что у 2D ----
// своя фигура для камер от первого и третьего лица: свой боец (в реплее — первый)
function r3MeObj(list){ const me = G.req ? G.req.me : G.me; return list.find(o => o.k === me) || (R3.selfObj && R3.selfObj.k === me ? R3.selfObj : null) || list.find(o => o.k === 0); }
R3.camUpdate = function(list){
  const v = R3.view, W = cvW(), H = cvH(), C = R3.camera, o = v.kind !== 'tac' ? r3MeObj(list) : null;
  if (o){   // 3-е лицо — за спиной героя на расстоянии pd, 1-е — из глаз; мышь — поворот (yaw) и наклон (pp / fp)
    const fx = Math.sin(v.yaw), fz = Math.cos(v.yaw);
    if (v.kind === 'third'){
      const ty = o.Y + o.h * .8, c = Math.cos(v.pp), s = Math.sin(v.pp);
      C.position.set(o.X + v.pd * c * fx, ty + v.pd * s + .3, o.Z + v.pd * c * fz); C.lookAt(o.X, ty, o.Z);
    } else {
      const ey = o.Y + r3Eye(o.f), c = Math.cos(v.fp);
      C.position.set(o.X, ey, o.Z); C.lookAt(o.X - fx * c, ey - Math.sin(v.fp), o.Z - fz * c);
    }
    if (C.aspect !== W / H){ C.aspect = W / H; C.updateProjectionMatrix(); }
    return;
  }
  if (cam.mode === 'fit') r3Fit(W / H);
  else if (cam.mode === 'follow'){
    const fr = FR[curIdx], A = fr.actor, me = G.req ? G.req.me : G.me;
    const who = list.find(o => o.k === (A >= 0 && stepT0 > -1e8 && playing ? A : me)) || list.find(o => o.k === me);
    if (who){ const s = cam.snap ? 1 : .12; v.tx += (who.X - v.tx) * s; v.tz += (who.Z - v.tz) * s; v.ty += (who.Y - v.ty) * s; }
    v.dist = v.fdist;
  }
  cam.snap = false;
  const cp = Math.cos(v.pitch), sp = Math.sin(v.pitch);
  C.position.set(v.tx + v.dist * cp * Math.sin(v.yaw), v.ty + v.dist * sp, v.tz + v.dist * cp * Math.cos(v.yaw));
  C.lookAt(v.tx, v.ty, v.tz);
  if (C.aspect !== W / H){ C.aspect = W / H; C.updateProjectionMatrix(); }
};
R3.bindInput = function(){
  const P = new Map(); let last = null, pinch = 0;
  const v = R3.view;
  const up = e => { P.delete(e.pointerId); if (P.size < 2) pinch = 0; if (!P.size) setTimeout(() => { R3.dragged = false; }, 0); };
  cv.addEventListener('pointerdown', e => {
    if (!R3.on) return;
    try { cv.setPointerCapture(e.pointerId); } catch (er) {}   // отпускание вне поля тоже дойдёт
    P.set(e.pointerId, {x: e.clientX, y: e.clientY}); R3.dragged = false; last = {x: e.clientX, y: e.clientY, sx: e.clientX, sy: e.clientY, b: e.button, sh: e.shiftKey};
    if (P.size === 2){ const [a, b] = [...P.values()]; pinch = Math.hypot(a.x - b.x, a.y - b.y); }
  });
  cv.addEventListener('pointermove', e => {
    if (!R3.on || !P.has(e.pointerId) || !last) return;
    if (e.pointerType === 'mouse' && !e.buttons){ up(e); return; }   // отпускание потерялось: мышь без кнопок не крутит камеру
    const prev = P.get(e.pointerId); P.set(e.pointerId, {x: e.clientX, y: e.clientY});
    if (Math.hypot(e.clientX - last.sx, e.clientY - last.sy) > 6) R3.dragged = true;
    const dx = e.clientX - prev.x, dy = e.clientY - prev.y;
    if (P.size === 2){   // два пальца: щипок — приближение, сдвиг середины — сдвиг обзора
      const [a, b] = [...P.values()], d = Math.hypot(a.x - b.x, a.y - b.y);
      if (pinch > 0) R3.zoom(pinch / Math.max(1, d)); pinch = d; R3.pan(dx / 2, dy / 2); return;
    }
    if (!R3.dragged) return;
    if (v.kind === 'third'){ v.yaw -= dx * .008; v.pp = Math.max(.05, Math.min(1.3, v.pp + dy * .006)); return; }
    if (v.kind === 'first'){ v.yaw -= dx * .006; v.fp = Math.max(-1.2, Math.min(1.2, v.fp + dy * .005)); return; }
    if (last.b === 2 || last.b === 1 || last.sh) R3.pan(dx, dy);
    else { v.yaw -= dx * .008; v.pitch = Math.max(.22, Math.min(1.5, v.pitch + dy * .006)); }
  });
  cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
  cv.addEventListener('wheel', e => { if (!R3.on) return; e.preventDefault(); R3.zoom(e.deltaY < 0 ? 1 / 1.12 : 1.12); }, {passive: false});
};
R3.zoom = function(f){
  const v = R3.view;
  if (v.kind === 'third'){ v.pd = Math.max(2, Math.min(14, v.pd * f)); return; }
  if (v.kind === 'first') return;
  if (cam.mode === 'fit') setCamMode('free');
  v.dist = Math.max(4, Math.min(Math.max(AW, AH) * 2.4, v.dist * f)); if (cam.mode === 'follow') v.fdist = v.dist;
};
R3.pan = function(dx, dy){
  if (R3.view.kind !== 'tac') return;   // камера привязана к герою
  const v = R3.view, k = v.dist * .0016, c = Math.cos(v.yaw), s = Math.sin(v.yaw);
  if (cam.mode !== 'free') setCamMode('free');
  v.tx = Math.max(0, Math.min(AW, v.tx - (dx * c + dy * s) * k)); v.tz = Math.max(0, Math.min(AH, v.tz - (-dx * s + dy * c) * k));
};
// луч из камеры через точку экрана: сначала капсулы бойцов, затем кубы (обход по сетке Amanatides–Woo)
R3.cellAt = function(e){
  const r = cv.getBoundingClientRect(), T = THREE;
  R3.ray.setFromCamera(new T.Vector2((e.clientX - r.left) / r.width * 2 - 1, -(e.clientY - r.top) / r.height * 2 + 1), R3.camera);
  // враг на луче — цель даже за кубами (его силуэт виден) и за своей капсулой; свой или союзник — только если не закрыт
  const fr = FR[curIdx], myT = fr.f[G.req ? G.req.me : G.me].team;
  const hits = R3.ray.intersectObjects(R3.fm.filter(m => m && m.body.visible).map(m => m.body), false);
  const he = hits.find(h => { const f = fr.f[h.object.userData.k]; return f && f.hp > 0 && f.team !== myT; });
  const o = R3.ray.ray.origin, d = R3.ray.ray.direction, vh = r3Voxel(o, d, 1e9), hb = he || hits[0];
  if (hb && (hb === he || !vh || hb.distance <= vh.t)){ const f = fr.f[hb.object.userData.k]; R3.hit = {x: f.x, y: f.y, top: r3Z(f), k: hb.object.userData.k}; return {x: f.x, y: f.y}; }
  if (!vh){ R3.hit = null; return null; }
  // удар по объекту — сама клетка стены (в режиме кубов — любой разрушимый блок: копать действием), иначе — куб перед гранью
  const vm = R3.vox[vIdx(vh.x, vh.y, vh.z)], inObj = G.mode === 'attack' && (MAP.vox ? vm !== VM.PITF && vm !== VM.WATER && vm !== VM.LAVA && vm !== VM.ACID : isObjCell(vh.x, vh.y));
  const x = inObj ? vh.x : vh.x + vh.nx, y = inObj ? vh.y : vh.y + vh.ny;
  R3.hit = {x, y, top: inObj ? vh.z + 1 : vh.z + vh.nz, z: MAP.vox && !inObj && vh.nz === 1 ? vh.z + 1 : null};
  const c = {x, y};
  if (!inObj && r3FlyAim()){ const z = r3TopZ(x, y) + R3.flyOff; R3.hit.z = R3.hit.top = z; c.z = z; return c; }   // летун: в воздух над клеткой
  if (MAP.vox && inObj) c.oz = vh.z;               // какой блок бить
  else if (MAP.vox && vh.nz === 1) c.z = vh.z + 1;  // щёлкнули по верху блока: идти именно на эту высоту (тоннель или крыша)
  return c;
};
function r3Voxel(o, d, maxT){
  // клетка луча в координатах карты: X -> x, Z -> y, Y -> z
  let x = Math.floor(o.x), y = Math.floor(o.z), z = Math.floor(o.y);
  const sx = Math.sign(d.x), sy = Math.sign(d.z), sz = Math.sign(d.y);
  const tdx = sx ? Math.abs(1 / d.x) : 1e9, tdy = sy ? Math.abs(1 / d.z) : 1e9, tdz = sz ? Math.abs(1 / d.y) : 1e9;
  let tx = sx ? ((sx > 0 ? x + 1 - o.x : o.x - x) * tdx) : 1e9, ty = sy ? ((sy > 0 ? y + 1 - o.z : o.z - y) * tdy) : 1e9, tz = sz ? ((sz > 0 ? z + 1 - o.y : o.y - z) * tdz) : 1e9;
  let nx = 0, ny = 0, nz = 0, t = 0;
  for (let i = 0; i < 2000 && t <= Math.min(maxT, 1500); i++){
    const m = x >= 0 && y >= 0 && x < AW && y < AH && z >= 0 && z < R3.H ? R3.vox[vIdx(x, y, z)] : 0;
    if (m && ((m !== VM.WOOD && m !== VM.LADDV) || G.mode === 'attack')) return {x, y, z, nx, ny, nz, t};   // тонкий ствол и лестница — сквозь (рубить — в режиме атаки)
    if (z < 0 && sz <= 0) return null;
    if (tx < ty && tx < tz){ x += sx; t = tx; tx += tdx; nx = -sx; ny = 0; nz = 0; }
    else if (ty < tz){ y += sy; t = ty; ty += tdy; nx = 0; ny = -sy; nz = 0; }
    else { z += sz; t = tz; tz += tdz; nx = 0; ny = 0; nz = -sz; }
  }
  return null;
}

// ---- подписи и круги поверх 3D: прежний холст cv, точки мира проецируются на экран ----
const r3V = () => new THREE.Vector3();
function r3P(X, Y, Z){
  const p = (R3._p || (R3._p = r3V())).set(X, Y, Z).project(R3.camera);
  return {x: (p.x + 1) / 2 * cvW(), y: (1 - p.y) / 2 * cvH(), ok: p.z < 1 && p.z > -1};
}
function r3Circle(X, Y, Z, r, stroke, fill, dash, w = 2){
  ctx.save(); ctx.beginPath(); let started = false;
  for (let i = 0; i <= 64; i++){ const a = i / 64 * Math.PI * 2, p = r3P(X + Math.cos(a) * r, Y, Z + Math.sin(a) * r); if (!p.ok){ started = false; continue; } if (started) ctx.lineTo(p.x, p.y); else { ctx.moveTo(p.x, p.y); started = true; } }
  if (fill){ ctx.fillStyle = fill; ctx.fill(); } if (dash) ctx.setLineDash(dash); ctx.strokeStyle = stroke; ctx.lineWidth = w; ctx.stroke(); ctx.restore();
}
function r3Square(x, y, Y, n, stroke, fill){
  const c = [[x, y], [x + n, y], [x + n, y + n], [x, y + n]].map(([a, b]) => r3P(a, Y + .01, b)); if (c.some(p => !p.ok)) return;
  ctx.save(); ctx.beginPath(); c.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)); ctx.closePath();
  if (fill){ ctx.fillStyle = fill; ctx.fill(); } ctx.strokeStyle = stroke; ctx.lineWidth = 2; ctx.stroke(); ctx.restore();
}
function r3Text(txt, x, y, font, fill, w = 4){ ctx.font = font; ctx.textAlign = 'center'; ctx.textBaseline = 'alphabetic'; ctx.lineJoin = 'round'; ctx.lineWidth = w; ctx.strokeStyle = 'rgba(0,0,0,.85)'; ctx.strokeText(txt, x, y); ctx.fillStyle = fill; ctx.fillText(txt, x, y); }
function r3Labels(list, fr){
  list.forEach(o => {
    const {f, boss} = o; if (f.hp <= 0 && !boss) return;
    const hd = r3P(o.X, o.Y + o.h + .15, o.Z); if (!hd.ok) return;
    const px = hd.x, py = hd.y;
    const img = tokenPortrait(f);
    if (img && f.hp > 0){ const r = 15; ctx.save(); ctx.beginPath(); ctx.arc(px, py - 34, r, 0, 7); ctx.clip(); ctx.drawImage(img, px - r, py - 34 - r, 2 * r, 2 * r); ctx.restore(); ctx.beginPath(); ctx.arc(px, py - 34, 15, 0, 7); ctx.strokeStyle = '#fff'; ctx.lineWidth = 2; ctx.stroke(); }
    if (f.fled && f.hp > 0) r3Text('сбежал', px, py - 22, '800 14px system-ui', '#fff');
    if (f.hp <= 0) { r3Text('✖', px, py, '800 16px system-ui', '#ddd'); return; }
    const bw = boss ? 56 : 32, bh = boss ? 7 : 5, bx = px - bw / 2, by = py - bh;
    ctx.fillStyle = 'rgba(0,0,0,.7)'; ctx.fillRect(bx - 2, by - 2, bw + 4, bh + 4);
    const q = Math.max(0, f.hp / f.maxHp); ctx.fillStyle = q > .5 ? '#5fd08a' : (q > .25 ? '#f2c14e' : '#ff5d5d'); ctx.fillRect(bx, by, bw * q, bh);
    if (f.temp){ ctx.fillStyle = '#c5cde2'; ctx.fillRect(bx + bw * q, by, Math.min(bw * (1 - q), bw * f.temp / f.maxHp), bh); }
    if (boss) r3Text(`${Math.max(0, f.hp)}/${f.maxHp}`, px, by - 5, '700 12px system-ui', '#fff', 3);
    if (f.alt) r3Text('🪽 ' + f.alt * 5 + ' фт', px, by - 19, '800 12px system-ui', '#bfe3ff', 3);
    if (boss && f.cond.length){ const g = r3P(o.X, o.Y, o.Z); if (g.ok) r3Text(f.cond.slice(0, 2).join(' · '), g.x, g.y + 18, '700 13px system-ui', '#fff'); }
  });
}
function r3Fx(list, now){
  const at = k => list.find(o => o.k === k);
  const mid = o => r3P(o.X, o.Y + o.h * .6, o.Z);
  fxs = fxs.filter(e => now < e.t0 + e.dur);
  fxs.forEach(e => {
    const u = (now - e.t0) / e.dur; if (u < 0) return;
    const o = e.k !== undefined ? at(e.k) : null;
    ctx.save();
    if (e.kind === 'num' && o){ const p = r3P(o.X, o.Y + o.h + .2, o.Z); if (p.ok){ ctx.globalAlpha = 1 - Math.pow(u, 3); r3Text((e.v < 0 ? '−' : '+') + Math.abs(e.v), p.x, p.y - 30 - u * 36, '900 28px system-ui', e.v < 0 ? '#ff6b6b' : '#5fd08a', 6); } }
    else if (e.kind === 'cond' && o){ const p = r3P(o.X, o.Y, o.Z); if (p.ok){ ctx.globalAlpha = 1 - Math.pow(u, 2.5); r3Text(e.text, p.x, p.y + 34 + u * 26, '800 16px system-ui', '#ffd54d', 5); } }
    else if ((e.kind === 'slash' || e.kind === 'bolt') && at(e.a) && at(e.t)){
      const a = mid(at(e.a)), b = mid(at(e.t)); if (a.ok && b.ok){
        ctx.strokeStyle = e.col; ctx.fillStyle = e.col; ctx.lineCap = 'round';
        if (e.kind === 'slash'){ ctx.globalAlpha = 1 - u; ctx.lineWidth = 7 + (1 - u) * 5; ctx.beginPath(); ctx.moveTo(lerp(a.x, b.x, .4), lerp(a.y, b.y, .4)); ctx.lineTo(lerp(a.x, b.x, .5 + u * .4), lerp(a.y, b.y, .5 + u * .4)); ctx.stroke(); ctx.beginPath(); ctx.arc(b.x, b.y, 14 + u * 20, 0, 7); ctx.stroke(); }
        else { ctx.globalAlpha = .55; ctx.lineWidth = 4; ctx.beginPath(); ctx.moveTo(lerp(a.x, b.x, Math.max(0, u - .3)), lerp(a.y, b.y, Math.max(0, u - .3))); ctx.lineTo(lerp(a.x, b.x, u), lerp(a.y, b.y, u)); ctx.stroke(); ctx.globalAlpha = 1; ctx.shadowColor = e.col; ctx.shadowBlur = 18; ctx.beginPath(); ctx.arc(lerp(a.x, b.x, u), lerp(a.y, b.y, u), 8, 0, 7); ctx.fill(); }
      } }
    else if (e.kind === 'ring' && o){ ctx.globalAlpha = (1 - u) * .85; r3Circle(o.X, o.Y + .05, o.Z, e.r * (.3 + .7 * Math.min(1, u * 2.2)), e.col, e.col + '33', null, 4); }
    else if (e.kind === 'cellbolt' && at(e.a)){ const a = mid(at(e.a)), Y = VZ0 + mapN('height', e.cx, e.cy), b = r3P(e.cx + .5, Y + .5, e.cy + .5); if (a.ok && b.ok){ ctx.fillStyle = e.col; ctx.shadowColor = e.col; ctx.shadowBlur = 18; ctx.beginPath(); ctx.arc(lerp(a.x, b.x, u), lerp(a.y, b.y, u), 8, 0, 7); ctx.fill(); } }
    else if (e.kind === 'cellring'){ ctx.globalAlpha = (1 - u) * .85; r3Circle(e.cx + .5, VZ0 + mapN('height', e.cx, e.cy) + .05, e.cy + .5, e.r * (.3 + .7 * Math.min(1, u * 2.2)), e.col, e.col + '33', null, 4); }
    else if ((e.kind === 'aura' || e.kind === 'spawn' || e.kind === 'flash') && (o || e.from)){
      const X = o ? o.X : e.from.x + .5, Z = o ? o.Z : e.from.y + .5, Y = o ? o.Y : r3Z(e.from);
      ctx.globalAlpha = (1 - u) * .8; r3Circle(X, Y + .05, Z, (o ? o.n : 1) * (e.kind === 'spawn' ? 1.4 - u * .7 : .7 + u * .6), e.kind === 'spawn' ? '#d6a8ff' : (e.col || '#fff'), null, null, 4);
    }
    ctx.restore();
  });
}
// ---- область в объёме (режим кубов) — та же геометрия, что в движке (vox.h: vxRay, vxInShape3, vxAreaCube, vxTopZ) ----
const r3Solid = m => !!m && m !== VM.WATER && m !== VM.LAVA && m !== VM.ACID && m !== VM.WOOD && m !== VM.LADDV;   // ствол и лестница — не твёрдые (как в движке)
const r3BlocksEffect = m => r3Solid(m) && m !== VM.LEAF && m !== VM.COVER;   // полное укрытие: твёрдое, кроме листвы и ящика
function r3TopZ(x, y){ for (let z = R3.H - 1; z >= 1; z--) if (r3Solid(vGet(x, y, z - 1)) && !r3Solid(vGet(x, y, z))) return z; return 1; }
// луч от a к b (кубы концов не считаются); f(материал) ≥ 100 — преграда
function r3Ray(ax, ay, az, bx, by, bz, f){
  const ddx = bx - ax, ddy = by - ay, ddz = bz - az;
  let x = Math.floor(ax), y = Math.floor(ay), z = Math.floor(az), best = 0;
  const ex = Math.floor(bx), ey = Math.floor(by), ez = Math.floor(bz), sx = Math.sign(ddx), sy = Math.sign(ddy), sz = Math.sign(ddz);
  const tdx = sx ? Math.abs(1 / ddx) : 1e9, tdy = sy ? Math.abs(1 / ddy) : 1e9, tdz = sz ? Math.abs(1 / ddz) : 1e9;
  let tx = sx ? (sx > 0 ? x + 1 - ax : ax - x) * tdx : 1e9, ty = sy ? (sy > 0 ? y + 1 - ay : ay - y) * tdy : 1e9, tz = sz ? (sz > 0 ? z + 1 - az : az - z) * tdz : 1e9;
  for (let i = 0; i < 4 * (AW + AH + R3.H); i++){
    if (tx < ty && tx < tz){ x += sx; if (tx > 1) break; tx += tdx; }
    else if (ty < tz){ y += sy; if (ty > 1) break; ty += tdy; }
    else { z += sz; if (tz > 1) break; tz += tdz; }
    if (x === ex && y === ey && z === ez) break;
    best = Math.max(best, f(vGet(x, y, z))); if (best >= 100) break;
  }
  return best;
}
function r3InShape3(shape, size, o, c, p){
  if (shape === 1) return 5 * Math.max(Math.abs(p[0] - c[0]), Math.abs(p[1] - c[1]), Math.abs(p[2] - c[2])) <= size + 1e-9;
  if (shape === 0){ const dx = 5 * (p[0] - c[0]), dy = 5 * (p[1] - c[1]), dz = 5 * (p[2] - c[2]), r = size + 2.5; return dx * dx + dy * dy + dz * dz <= r * r; }
  const ux = c[0] - o[0], uy = c[1] - o[1], uz = c[2] - o[2], len = Math.hypot(ux, uy, uz); if (len <= 0) return false;
  const vx = 5 * (p[0] - o[0]), vy = 5 * (p[1] - o[1]), vz = 5 * (p[2] - o[2]), t = (vx * ux + vy * uy + vz * uz) / len;
  const perp = Math.hypot(vy * uz - vz * uy, vz * ux - vx * uz, vx * uy - vy * ux) / len;
  if (t <= 0 || t > size + 2.5) return false;
  return shape === 3 ? perp <= 3 : perp <= t / 2 + 2.5;
}
const r3Eye = f => f.sz === 0 ? .2 : f.sz === 1 ? .4 : .8 * (f.n || 1);
// область: shape, size; me — боец кадра (x, y, z, n, sz); центр — куб (cx, cy, cz). Возвращает проверку куба p
function r3Area(shape, size, me, cx, cy, cz){
  const n = me.n || 1, c = [cx + .5, cy + .5, cz + .5], o = shape === 2 || shape === 3 ? [me.x + n / 2, me.y + n / 2, me.z + r3Eye(me)] : c;
  return (px, py, pz) => {
    const p = [px + .5, py + .5, pz + .5];
    if (!r3InShape3(shape, size, o, c, p) || r3BlocksEffect(vGet(px, py, pz))) return false;
    return r3Ray(o[0], o[1], o[2], p[0], p[1], p[2], m => r3BlocksEffect(m) ? 100 : 0) < 100;
  };
}
// предпросмотр: клетки земли (верх столбца) и бойцы, которых заденет
function r3AreaPreview(list, ga, me, cx, cy, cz){
  const hit = r3Area(ga.shape, ga.radius, me.f, cx, cy, cz), R = Math.ceil(ga.radius / 5) + 1;
  const far = ga.shape === 2 || ga.shape === 3, bx = far ? me.f.x : cx, by = far ? me.f.y : cy;
  for (let y = Math.max(0, by - R); y <= Math.min(AH - 1, by + R); y++)
    for (let x = Math.max(0, bx - R); x <= Math.min(AW - 1, bx + R); x++){ const tz = r3TopZ(x, y); if (hit(x, y, tz)) r3Square(x, y, tz + .03, 1, 'rgba(255,110,50,.95)', 'rgba(255,100,40,.3)'); }
  list.forEach(o => {
    const f = o.f; if (f.hp <= 0 || (ga.sel && f.team === me.f.team)) return;
    const n = f.n || 1; let h = false;
    for (let k = 0; k < n && !h; k++) for (let dy = 0; dy < n && !h; dy++) for (let dx = 0; dx < n && !h; dx++) h = hit(f.x + dx, f.y + dy, f.z + k);
    if (h) r3Circle(o.X, o.Y + .05, o.Z, .45 * o.n + .15, f.team === me.f.team ? '#ffd35a' : '#ff5d5d', 'rgba(255,90,60,.18)', null, 4);
  });
}
// клетки, куда дойдёт за этот ход: первый узел каждой клетки в дереве путей с ценой не больше остатка движения
function r3Reach(q){
  const t = q.vt, seen = new Set();
  for (let i = 5; i < t.length; i += 5){
    if (t[i + 4] > q.speedLeft) continue;
    const k = t[i] + ',' + t[i + 1]; if (seen.has(k)) continue; seen.add(k);
    r3Square(t[i], t[i + 1], t[i + 2] + .02, 1, 'rgba(95,208,138,.35)', 'rgba(95,208,138,.16)');
  }
}
// подсветка своего хода: дальность движения и оружия, цели, клетка под курсором с расстоянием
function r3Turn(list, now){
  if (!G.req || G.busy || G.over) return;
  const q = G.req, me = list.find(o => o.k === q.me); if (!me) return;
  if (G.mode === 'move' && q.speedLeft >= 5){
    if (q.vt) r3Reach(q);   // режим кубов: досягаемость по дереву путей движка (подъёмы, обходы, тоннели)
    else r3Circle(me.X, me.Y + .04, me.Z, q.speedLeft / 5, 'rgba(95,208,138,.75)', 'rgba(95,208,138,.10)', [8, 6]);
  }
  let range = 0;
  if (G.mode === 'attack') range = q.weapon.range; else if (G.mode.startsWith('attack:')){ const a = (q.alts || []).find(x => x.k === +G.mode.slice(7)); range = a ? a.range : 5; } else if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; range = a.self ? (a.ally ? a.range : 0) : a.range; }
  const ga = G.mode.startsWith('spell:') ? q.abils[+G.mode.slice(6)] : null;
  if (range){
    r3Circle(me.X, me.Y + .04, me.Z, range / 5 + (me.n - 1) / 2, 'rgba(255,214,90,.8)', null, [4, 6]);
    if (!(ga && ga.ground)) q.enemies.forEach(en => { const o = list.find(x => x.k === en.i); if (!o || o.f.hp <= 0) return; const ok = en.dist <= range && en.los && !en.charm; r3Circle(o.X, o.Y + .04, o.Z, .45 * o.n + .12 + Math.sin(now / 160) * .04, ok ? '#ff5d5d' : 'rgba(180,180,190,.7)', null, null, ok ? 4 : 2); });
  }
  const h = R3.hit; if (!h || h.x < 0 || h.y < 0 || h.x >= AW || h.y >= AH) return;
  if (MAP.vox && ga && ga.radius > 0 && ga.shape != null){   // режим кубов: область по кубам (не сквозь стены)
    const t = h.k != null ? FR[curIdx].f[h.k] : null;
    if (ga.ground) r3AreaPreview(list, ga, me, h.x, h.y, h.z != null && !r3Solid(vGet(h.x, h.y, h.z)) ? h.z : r3TopZ(h.x, h.y));   // как движок
    else if (t && t.hp > 0) r3AreaPreview(list, ga, me, t.x, t.y, t.z);
  }
  if (ga && ga.ground){
    if (ga.radius && !(MAP.vox && ga.shape != null)) r3Circle(h.x + .5, h.top + .04, h.y + .5, ga.radius / 5, 'rgba(255,140,90,.85)', 'rgba(255,120,80,.16)');
    if (ga.wallLen){   // клетки барьера — тот же расчёт направления, что у 2D
      const dx = h.x - me.f.x, dy = h.y - me.f.y, ax = Math.abs(dx), ay = Math.abs(dy), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
      let px = 1, py = 0; if (ax === 0 && ay === 0){ px = 1; py = 0; } else if (ax >= 2 * ay){ px = 0; py = 1; } else if (ay >= 2 * ax){ px = 1; py = 0; } else { px = -sy; py = sx; }
      for (let i = -Math.floor((ga.wallLen - 1) / 2); i <= Math.floor(ga.wallLen / 2); i++){ const x = h.x + px * i, y = h.y + py * i; if (x < 0 || y < 0 || x >= AW || y >= AH) continue; r3Square(x, y, h.top, 1, '#d6c4ff', 'rgba(190,160,255,.45)'); }
    }
  }
  const path = G.mode === 'move' ? pathPreview(q, h.x, h.y, h.z) : null;   // путь под мышью (как в 2D; в кубах — с высотой щелчка)
  if (path){ const pts = [r3P(me.X, me.Y + .08, me.Z), ...path.cells.map(c => r3P(c.x + .5, r3Z(c) + .08, c.y + .5))]; if (pts.every(p => p.ok)) pathStroke(pts, path.cells, q.speedLeft); }
  if (r3FlyAim() && h.z != null){   // летун: столбик от земли до точки в воздухе и высота
    const g = r3TopZ(h.x, h.y), a = r3P(h.x + .5, g, h.y + .5), b = r3P(h.x + .5, h.z, h.y + .5);
    if (a.ok && b.ok){ ctx.save(); ctx.setLineDash([4, 4]); ctx.strokeStyle = 'rgba(160,220,255,.9)'; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke(); ctx.restore(); r3Text('↑ ' + 5 * R3.flyOff + ' фт (R/F)', b.x + 10, b.y - 14, '700 12px system-ui', '#bfe6ff'); }
  }
  r3Square(h.x, h.y, h.top, 1, 'rgba(255,255,255,.95)', 'rgba(255,255,255,.12)');
  const n = me.n, gap = (a, b) => Math.max(0, b - (a + n - 1), a - b), d = path ? pathLabel(path, q.speedLeft) : 5 * Math.max(gap(me.f.x, h.x), gap(me.f.y, h.y)) + ' фт', p = r3P(h.x + 1, h.top, h.y);
  if (p.ok){ ctx.save(); ctx.textAlign = 'left'; r3Text(d, p.x + 22, p.y, '700 13px system-ui', '#fff'); ctx.restore(); }
}
// кадр 3D: из render() (viewer_template.html) вместо 2D-рисования
R3.draw = function(now, t, tt){
  if (R3.map !== MAP) R3.setMap();
  const fr = FR[curIdx]; if (!fr) return;
  const W = cvW(), H = cvH(), dp = R3.ren.getPixelRatio();
  if (R3.cv.width !== Math.round(W * dp) || R3.cv.height !== Math.round(H * dp)) R3.ren.setSize(W, H, false);
  R3.syncDyn(fr); R3.syncVox(); R3.syncFog();
  R3.U.uGrid.value = G.req && !G.busy && !G.over ? 1 : 0;   // сетка 5 фт — в свой ход
  const dt = R3.tLast ? Math.min(.1, Math.max(0, (now - R3.tLast) / 1000)) : 0; R3.tLast = now;   // для анимаций моделей
  const list = R3.syncFighters(fr, t, dt);
  R3.camUpdate(list);
  R3.sky.position.copy(R3.camera.position);   // небо — вокруг камеры
  R3.ren.render(R3.scene, R3.camera);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, W, H);
  const A = fr.actor;
  if (A >= 0 && FR[prevIdx].f[A] && stepT0 > -1e8){   // след движения ходящего
    const p0 = FR[prevIdx].f[A], p1 = fr.f[A], n = p1.n || 1;
    if (p0.x !== p1.x || p0.y !== p1.y){ const a = r3P(p0.x + n / 2, r3Z(p0) + .05, p0.y + n / 2), b = r3P(p1.x + n / 2, r3Z(p1) + .05, p1.y + n / 2); if (a.ok && b.ok){ ctx.save(); ctx.strokeStyle = 'rgba(255,255,255,.55)'; ctx.setLineDash([5, 6]); ctx.lineWidth = 3; ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke(); ctx.restore(); } }
  }
  [0, 1].forEach(k => { const o = list.find(x => x.k === k); if (o && o.f.hp > 0 && BOSS[k] && BOSS[k].auraRad > 0) r3Circle(o.X, o.Y + .04, o.Z, BOSS[k].auraRad / 5, k ? 'rgba(255,120,90,.6)' : 'rgba(100,170,255,.6)', k ? 'rgba(255,100,70,.10)' : 'rgba(90,160,255,.10)'); });
  r3Turn(list, now);
  r3Labels(list, fr);
  r3Fx(list, now);
  if (typeof ptrDraw === 'function') ptrDraw(now);   // указки игроков комнаты
  R3.fpsN++; if (now - R3.fpsT0 >= 1000){ R3.fps = Math.round(R3.fpsN * 1000 / (now - R3.fpsT0)); R3.fpsN = 0; R3.fpsT0 = now; }
  ctx.save(); ctx.font = '700 11px system-ui'; ctx.textAlign = 'left'; ctx.textBaseline = 'bottom'; ctx.fillStyle = 'rgba(255,255,255,.7)'; ctx.fillText(`3D · ${R3.fps} к/с${R3.low ? ' · телефон' : ''}`, 8, H - 6); ctx.restore();
};
// переключатель вида в бою (виден после загрузки библиотеки): только на этот бой, настройку не меняет; 2D остаётся запасным
$('view3dBtn').onclick = () => {
  if (!R3.ready || R3.failed) return;
  R3.on = !R3.on; R3.cv.style.display = R3.on ? 'block' : 'none'; cam.snap = true; R3.hit = null; R3.syncCamBtn();
  $('view3dBtn').textContent = R3.on ? 'Вид: 3D' : 'Вид: 2D';
};
if ($('viewSeg') && $('viewSeg').addEventListener){
  $('viewSeg').addEventListener('click', e => { const b = e.target.closest('[data-view]'); if (b) R3.setWant(b.dataset.view === '3d'); });
  r3SyncSeg();
}
// камера (решение №2): тактика → 3-е лицо → 1-е лицо; кнопка в бою и клавиша V (только в 3D)
const CAMK = {tac: '🎯 Тактика', third: '🧍 3-е лицо', first: '👁 1-е лицо'};
R3.setCamKind = function(k){
  R3.view.kind = k; R3.hit = null;
  if (k !== 'tac'){ R3.view.pp = .45; R3.view.fp = .25; }   // наклон — заново, поворот (yaw) — тот же, что у тактики
  const b = $('camKindBtn'); if (b && b.textContent != null) b.textContent = CAMK[k];
  try { localStorage.setItem('dnd_cam3d', k); } catch (e) {}
};
R3.syncCamBtn = function(){ const b = $('camKindBtn'); if (b && 'hidden' in b) b.hidden = !R3.on; };
if ($('camKindBtn') && $('camKindBtn').addEventListener){
  $('camKindBtn').addEventListener('click', () => R3.setCamKind(R3.view.kind === 'tac' ? 'third' : (R3.view.kind === 'third' ? 'first' : 'tac')));
  document.addEventListener('keydown', e => {
    if (!R3.on || e.ctrlKey || e.metaKey || e.altKey || /INPUT|TEXTAREA|SELECT/.test((e.target && e.target.tagName) || '')) return;
    if (e.key === 'v' || e.key === 'V' || e.key === 'м' || e.key === 'М') $('camKindBtn').click();
  });
  try { const k = localStorage.getItem('dnd_cam3d'); if (CAMK[k]) R3.setCamKind(k); } catch (e) {}
}
// Полёт в 3D (режим кубов): высота цели хода летуна — клавиши R (выше) и F (ниже) по 5 фт над верхом клетки под мышью
// (кнопок высоты нет: путь движка ведёт в выбранный куб воздуха, предпросмотр пути показывает подъём)
R3.flyOff = 0;
function r3FlyAim(){ return MAP && MAP.vox && G.req && G.req.vfly && G.mode === 'move' && R3.flyOff > 0; }
if (typeof document !== 'undefined' && document.addEventListener)
  document.addEventListener('keydown', e => {
    if (!R3.on || !MAP || !MAP.vox || !G.req || !G.req.vfly || e.ctrlKey || e.metaKey || e.altKey || /INPUT|TEXTAREA|SELECT/.test((e.target && e.target.tagName) || '')) return;
    const up = /^[rRкК]$/.test(e.key), dn = /^[fFаА]$/.test(e.key); if (!up && !dn) return;
    R3.flyOff = Math.max(0, Math.min(12, R3.flyOff + (up ? 1 : -1)));
    const h = R3.hit; if (h && G.mode === 'move'){ const t = r3TopZ(h.x, h.y); h.z = R3.flyOff > 0 ? t + R3.flyOff : null; h.top = R3.flyOff > 0 ? h.z : t; }
  });
