// ====================================================================================================================
// 3D-вид боя (этап 1а, флаг ?3d=1). Движок не меняется: нынешняя 2D-карта переводится в кубы 5×5×5 фт прямо здесь,
// бойцы — цветные капсулы, камера тактическая (вращение, наклон, приближение), клетка выбирается лучом по кубам.
// Те же кадры FR, команды cmd() и щелчки по полю (52_turn.js); очередь кадров (step, seek, onCaughtUp) — общая,
// R3.draw только рисует. Подписи (ХП, урон, броски) и круги дальности — на прежнем холсте cv поверх 3D.
// ====================================================================================================================
const R3 = {
  want: new URLSearchParams(location.search).get('3d') === '1',   // флаг: без него модуль ничего не делает
  on: false, ready: false, failed: false,
  map: null, vox: null, H: 0, NX: 0, NY: 0, NZ: 0, chunks: [], dynFr: null, dynKey: '', dynCells: new Map(),
  fm: [], capGeo: new Map(), fogKey: '', dragged: false, hit: null,
  view: {tx: 0, ty: 0, tz: 0, yaw: .65, pitch: .95, dist: 30, fdist: 22},
  fps: 0, fpsN: 0, fpsT0: 0
};
const VZ0 = 2;      // кубов под уровнем «0» нынешней карты: дно ущелья
const VCH = 16;     // чанк 16×16×16
// материалы кубов: цвет (sRGB) и твёрдость для луча
const VM = {AIR:0, GRASS:1, DIRT:2, STONE:3, STRONG:4, WOOD:5, LEAF:6, WATER:7, MUD:8, PLANK:9, DOOR:10, PITF:11,
  LAVA:12, ACID:13, SMOKE:14, ICE:15, STORM:16, NECRO:17, FORCE:18, COBBLE:19, RUBBLE:20, DARKG:21, COVER:22, LADDER:23, CDIRT:24};
const VCOL = [0, 0x5b8a4f, 0x6e5236, 0x80848f, 0x4a4e58, 0x6b4a2b, 0x3f7a3a, 0x3a7fc0, 0x5e4524, 0x9a7442, 0x6b4423, 0x15161c,
  0xe2572a, 0x7ccf2e, 0x7e8f57, 0x9fd8ff, 0x5868d8, 0x4b3a63, 0x7a63c9, 0x5c606c, 0x6a6152, 0x2a2e40, 0x7d7568, 0xc9a46a, 0x4e525c];

// ---- загрузка библиотеки: только по флагу, отдельным файлом с кэшем ----
if (R3.want){
  const s = document.createElement('script');
  s.src = '/vendor/three-r159.min.js';
  s.onload = () => { try { R3.init(); } catch (e) { R3.fail('не удалось включить 3D: ' + e.message); } };
  s.onerror = () => R3.fail('не загрузилась библиотека 3D');
  document.head.appendChild(s);
}
R3.fail = function(msg){ R3.failed = true; R3.on = false; if (R3.cv) R3.cv.style.display = 'none'; console.error(msg); toast(msg + ' — показан 2D-вид.'); };

R3.init = function(){
  const T = THREE, coarse = matchMedia('(pointer: coarse)').matches;
  R3.low = coarse;   // телефон: пониженная графика (без сглаживания, плотность пикселей до 1.5)
  R3.cv = document.createElement('canvas'); R3.cv.id = 'cv3';
  R3.cv.style.cssText = 'position:absolute;inset:0;width:100%;height:100%;display:block';
  stage.insertBefore(R3.cv, cv);
  cv.style.position = 'relative'; cv.style.zIndex = '1';   // подписи и щелчки — на прежнем холсте поверх
  R3.ren = new T.WebGLRenderer({canvas: R3.cv, antialias: !coarse, powerPreference: 'high-performance'});
  R3.ren.setPixelRatio(Math.min(devicePixelRatio || 1, coarse ? 1.5 : 2));
  R3.cv.addEventListener('webglcontextlost', e => { e.preventDefault(); R3.fail('3D-вид потерял видеокарту'); });
  R3.scene = new T.Scene();
  R3.camera = new T.PerspectiveCamera(45, 1, .1, 1200);
  R3.scene.add(new T.HemisphereLight(0xdde8ff, 0x3a3428, 2.2));
  const sun = new T.DirectionalLight(0xffffff, 1.6); sun.position.set(.45, 1, .3); R3.scene.add(sun);
  // туман войны и сетка 5 фт — в шейдере рельефа: маска клеток текстурой, сетка по верхним граням
  R3.U = {uFog: {value: null}, uFogOn: {value: 0}, uGrid: {value: 0}, uSize: {value: new T.Vector2(1, 1)}};
  R3.mat = new T.MeshBasicMaterial({vertexColors: true});
  R3.mat.onBeforeCompile = sh => {
    Object.assign(sh.uniforms, R3.U);
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nattribute vec3 aC;\nvarying vec3 vC;\nvarying vec2 vXZ;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvC = aC; vXZ = position.xz;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\nuniform sampler2D uFog;\nuniform float uFogOn, uGrid;\nuniform vec2 uSize;\nvarying vec3 vC;\nvarying vec2 vXZ;')
      .replace('#include <color_fragment>', `#include <color_fragment>
        if (uFogOn > .5) diffuseColor.rgb *= mix(.33, 1., texture2D(uFog, (vC.xy + .5) / uSize).r);
        if (uGrid > .5 && vC.z > .5){ vec2 g = abs(fract(vXZ) - .5); if (max(g.x, g.y) > .47) diffuseColor.rgb *= .62; }`);
  };
  R3.ray = new T.Raycaster();
  R3.shadowGeo = new T.CircleGeometry(.5, 20).rotateX(-Math.PI / 2);
  R3.shadowMat = new T.MeshBasicMaterial({color: 0x000000, transparent: true, opacity: .35, depthWrite: false});
  R3.bindInput();
  R3.ready = true; R3.on = true;
  const b = $('view3dBtn'); if (b){ b.hidden = false; b.textContent = 'Вид: 3D'; }
};

// ---- мир из кубов: столбец клетки (x, y) по слоям нынешней карты и изменениям арены ----
const vIdx = (x, y, z) => (y * AW + x) * R3.H + z;
function vGet(x, y, z){ return x < 0 || y < 0 || x >= AW || y >= AH || z >= R3.H ? 0 : (z < 0 ? VM.DIRT : R3.vox[vIdx(x, y, z)]); }
const mapN = (L, x, y) => (MAP[L] && MAP[L][y] && +MAP[L][y][x]) || 0;
// уровень (по 10 фт), на котором стоит боец: как zF в движке (земля, дерево, настил) + высота полёта
function r3Level(f){
  const gone = R3.dynCells.get(f.x + ',' + f.y) || {};
  return (f.up ? mapN('deck', f.x, f.y) : mapN('height', f.x, f.y) + (mapN('tree', f.x, f.y) && !gone[4] ? 2 : 0)) + (f.alt || 0);
}
const r3Z = f => VZ0 + 2 * r3Level(f);   // куб, где ноги
function r3Column(x, y){
  const V = R3.vox, i0 = vIdx(x, y, 0), H = R3.H, d = R3.dynCells.get(x + ',' + y) || {}, city = !!MAP.theme;
  for (let z = 0; z < H; z++) V[i0 + z] = 0;
  const top = VZ0 + 2 * mapN('height', x, y);   // первый куб воздуха над землёй
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
  if (!broken && mapN('tree', x, y)){ put(top, top + 2, VM.WOOD); put(top + 2, top + 4, VM.LEAF); }   // дерево 20 фт
  if (!broken && mapN('climb', x, y)) put(top, top + 1, VM.COVER);   // укрытие: низкий ящик (рисуется в полкуба)
  const d0 = mapN('door', x, y);
  if (d0){   // проём 10 фт, над ним перемычка до высоты стены; закрытая дверь — два куба
    const st = d[6] ? 6 : d[7] ? 7 : d[8] ? 8 : 0, open = st ? st === 6 : d0 === 2;
    put(top + 2, top + 6, VM.STONE); if (!open && st !== 8) put(top, top + 2, VM.DOOR);
  }
  const dk = mapN('deck', x, y);
  if (dk && !d[5] && VZ0 + 2 * dk - 1 >= top) put(VZ0 + 2 * dk - 1, VZ0 + 2 * dk, VM.PLANK);   // настил: стоять на его верху
  if (d[1]) put(top, top + 4, VM.FORCE);   // временная стена способности
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
  R3.fm.forEach(m => { if (m){ R3.scene.remove(m.body, m.shadow, m.ghost); m.mat.dispose(); m.gmat.dispose(); } });
  R3.fm = []; R3.map = MAP; R3.dynFr = null; R3.fogKey = ''; R3.dynCells = r3DynMap(FR[curIdx] && FR[curIdx].dyn); R3.dynKey = JSON.stringify((FR[curIdx] && FR[curIdx].dyn) || []);
  let maxTop = 0;
  for (let y = 0; y < AH; y++) for (let x = 0; x < AW; x++) maxTop = Math.max(maxTop, VZ0 + 2 * Math.max(mapN('height', x, y), mapN('deck', x, y)) + 6);
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
  R3.scene.background = new T.Color(MAP.day === 0 ? 0x141a26 : 0x8fb3d9);
  R3.scene.fog = new T.Fog(R3.scene.background, Math.max(AW, AH) * 1.4, Math.max(AW, AH) * 3);
  const v = R3.view; v.tx = AW / 2; v.tz = AH / 2; v.ty = VZ0; v.dist = Math.max(AW, AH) * 1.05; v.fdist = Math.min(v.dist, 24);
};
// сетка чанка: видимые грани кубов (как в Minecraft); сменная функция «чанк -> сетка» (позже — Surface Nets)
const FACES = [   // [смещение соседа x, z(высота), y], 4 угла грани (X, Y, Z трёх), затенение
  [[0, 1, 0], [[0,1,0],[0,1,1],[1,1,1],[1,1,0]], 1],
  [[0, -1, 0], [[0,0,0],[1,0,0],[1,0,1],[0,0,1]], .5],
  [[1, 0, 0], [[1,0,0],[1,1,0],[1,1,1],[1,0,1]], .76],
  [[-1, 0, 0], [[0,0,0],[0,0,1],[0,1,1],[0,1,0]], .7],
  [[0, 0, 1], [[0,0,1],[1,0,1],[1,1,1],[0,1,1]], .86],
  [[0, 0, -1], [[0,0,0],[0,1,0],[1,1,0],[1,0,0]], .62]];
const vOpaque = m => m !== 0 && m !== VM.COVER;
R3.buildChunk = function(ci){
  const T = THREE, old = R3.chunks[ci];
  if (old && old.mesh){ R3.scene.remove(old.mesh); old.mesh.geometry.dispose(); }
  const cx = ci % R3.NX, cy = Math.floor(ci / R3.NX) % R3.NY, cz = Math.floor(ci / (R3.NX * R3.NY));
  const pos = [], col = [], cel = [], idx = [], c = new T.Color();
  for (let y = cz * VCH; y < Math.min(AH, cz * VCH + VCH); y++) for (let x = cx * VCH; x < Math.min(AW, cx * VCH + VCH); x++)
    for (let z = cy * VCH; z < Math.min(R3.H, cy * VCH + VCH); z++){
      const m = R3.vox[vIdx(x, y, z)]; if (!m) continue;
      const hgt = m === VM.COVER ? .5 : (m === VM.WATER && !vGet(x, y, z + 1) ? .85 : 1);   // ящик — в полкуба, вода чуть ниже берега
      const jit = .94 + hash(x * 7 + z, y * 13 + z) * .12;   // лёгкая пестрота кубов
      for (const [[dx, dz, dy], quad, shade] of FACES){
        const nb = vGet(x + dx, y + dy, z + dz);
        if (vOpaque(nb) && hgt === 1 && !(nb === VM.WATER && m !== VM.WATER)) continue;   // грань закрыта соседом
        if (nb === m && m === VM.WATER) continue;
        if (dz === -1 && z === 0) continue;
        c.set(VCOL[m]); const k = shade * jit, top = dz === 1 && m !== VM.COVER ? 1 : 0, b = pos.length / 3;
        for (const [qx, qy, qz] of quad){ pos.push(x + qx, z + qy * hgt, y + qz); col.push(c.r * k, c.g * k, c.b * k); cel.push(x, y, top); }
        idx.push(b, b + 1, b + 2, b, b + 2, b + 3);
      }
    }
  if (!idx.length){ R3.chunks[ci] = {mesh: null}; return; }
  const g = new T.BufferGeometry();
  g.setAttribute('position', new T.Float32BufferAttribute(pos, 3));
  g.setAttribute('color', new T.Float32BufferAttribute(col, 3));
  g.setAttribute('aC', new T.Float32BufferAttribute(cel, 3));
  g.setIndex(idx); g.computeBoundingSphere();
  const mesh = new T.Mesh(g, R3.mat); mesh.matrixAutoUpdate = false;
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
    for (const [ax, ay] of [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1]]){   // соседний чанк: его грани у границы тоже меняются
      const nx = x + ax, ny = y + ay; if (nx < 0 || ny < 0 || nx >= AW || ny >= AH) continue;
      for (let cy = 0; cy < R3.NY; cy++) dirty.add(Math.floor(nx / VCH) + R3.NX * (cy + R3.NY * Math.floor(ny / VCH)));
    }
  });
  dirty.forEach(ci => R3.buildChunk(ci));
};
// туман войны: та же маска, что у 2D (бит на клетку из запроса хода)
R3.syncFog = function(){
  const q = G.req || G.lastReq, on = !!(q && q.vis && $('fogOn').checked && !G.over && q.visW === AW && q.visH === AH);
  R3.U.uFogOn.value = on ? 1 : 0; if (!on || q.vis === R3.fogKey) return;
  R3.fogKey = q.vis; const bin = atob(q.vis);
  for (let k = 0; k < AW * AH; k++) R3.fogData[4 * k] = ((bin.charCodeAt(k >> 3) >> (k & 7)) & 1) ? 255 : 0;
  R3.U.uFog.value.needsUpdate = true;
};

// ---- бойцы: капсулы цвета стороны, рост по размеру ----
function r3Cap(n){
  let g = R3.capGeo.get(n); if (g) return g;
  const r = n > 1 ? .3 * n : .28, len = n > 1 ? .95 * n - 2 * r : .4;
  g = new THREE.CapsuleGeometry(r, len, 4, 12); g.userData.h = len + 2 * r; R3.capGeo.set(n, g); return g;
}
function r3Colors(f){
  const side = f.kind % 2, tc = f.lead != null && typeof teamCol === 'function' ? teamCol(f.team) : null;
  return f.hp <= 0 ? '#8a8a8a' : (tc || (side ? '#e0644f' : '#4f8fe6'));
}
R3.syncFighters = function(fr, t){
  const out = [];
  fr.f.forEach((f, k) => {
    let m = R3.fm[k];
    if (!m){
      const mat = new THREE.MeshLambertMaterial({color: r3Colors(f)}), gmat = new THREE.MeshBasicMaterial({color: r3Colors(f), transparent: true, opacity: .32, depthTest: false, depthWrite: false});
      m = R3.fm[k] = {mat, gmat, body: new THREE.Mesh(r3Cap(f.n || 1), mat), shadow: new THREE.Mesh(R3.shadowGeo, R3.shadowMat), n: f.n || 1, col: ''};
      m.ghost = new THREE.Mesh(m.body.geometry, gmat); m.ghost.renderOrder = 10;   // силуэт сквозь стены и деревья: бойца не теряем за кубами
      m.body.userData.k = k; R3.scene.add(m.body, m.shadow, m.ghost);
    }
    const n = f.n || 1, boss = f.ld != null ? !!f.ld : f.kind < 2;
    if (m.n !== n){ m.body.geometry = m.ghost.geometry = r3Cap(n); m.n = n; }
    const hide = (f.hid && typeof G !== 'undefined') || (f.hp <= 0 && !boss);
    m.body.visible = m.shadow.visible = m.ghost.visible = !hide; if (hide) return;
    const c = r3Colors(f); if (c !== m.col){ m.mat.color.set(c); m.gmat.color.set(c); m.col = c; }
    const p = posAt(k, t), a = FR[prevIdx].f[k], tele = FR[curIdx].act === 'teleport' && FR[curIdx].actor === k;
    const z = a && !tele ? lerp(r3Z(a), r3Z(f), t) : r3Z(f), h = m.body.geometry.userData.h;
    const X = p.x + n / 2, Z = p.y + n / 2;
    const A = fr.actor, act = k === A && stepT0 > -1e8 && t < 1;
    m.mat.emissive.set(act ? 0x6a5414 : 0x000000);
    if (f.hp <= 0){ m.body.rotation.set(0, 0, Math.PI / 2); m.body.position.set(X, z + .3 * n, Z); }   // павший лежит
    else { m.body.rotation.set(0, 0, 0); m.body.position.set(X, z + h / 2, Z); }
    m.shadow.scale.setScalar(.9 * n); m.shadow.position.set(X, VZ0 + 2 * (r3Level(f) - (f.alt || 0)) + .02, Z);
    m.ghost.position.copy(m.body.position); m.ghost.rotation.copy(m.body.rotation);
    out.push({k, f, X, Y: z, Z, h, n, boss});
  });
  for (let k = fr.f.length; k < R3.fm.length; k++) if (R3.fm[k]) R3.fm[k].body.visible = R3.fm[k].shadow.visible = R3.fm[k].ghost.visible = false;
  return out;
};

// ---- камера: тактическая (вращение, наклон, приближение); режимы кнопки «Камера» — те же, что у 2D ----
R3.camUpdate = function(list){
  const v = R3.view, W = cvW(), H = cvH();
  if (cam.mode === 'fit'){ v.tx = AW / 2; v.tz = AH / 2; v.ty = VZ0; v.dist = Math.max(AW, AH) * 1.05; }
  else if (cam.mode === 'follow'){
    const fr = FR[curIdx], A = fr.actor, me = G.req ? G.req.me : G.me;
    const who = list.find(o => o.k === (A >= 0 && stepT0 > -1e8 && playing ? A : me)) || list.find(o => o.k === me);
    if (who){ const s = cam.snap ? 1 : .12; v.tx += (who.X - v.tx) * s; v.tz += (who.Z - v.tz) * s; v.ty += (who.Y - v.ty) * s; }
    v.dist = v.fdist;
  }
  cam.snap = false;
  const cp = Math.cos(v.pitch), sp = Math.sin(v.pitch), C = R3.camera;
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
    if (last.b === 2 || last.b === 1 || last.sh) R3.pan(dx, dy);
    else { v.yaw -= dx * .008; v.pitch = Math.max(.22, Math.min(1.5, v.pitch + dy * .006)); }
  });
  cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
  cv.addEventListener('wheel', e => { if (!R3.on) return; e.preventDefault(); R3.zoom(e.deltaY < 0 ? 1 / 1.12 : 1.12); }, {passive: false});
};
R3.zoom = function(f){
  const v = R3.view; if (cam.mode === 'fit') setCamMode('free');
  v.dist = Math.max(4, Math.min(Math.max(AW, AH) * 2.4, v.dist * f)); if (cam.mode === 'follow') v.fdist = v.dist;
};
R3.pan = function(dx, dy){
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
  if (hb && (hb === he || !vh || hb.distance <= vh.t)){ const f = fr.f[hb.object.userData.k]; R3.hit = {x: f.x, y: f.y, top: r3Z(f)}; return {x: f.x, y: f.y}; }
  if (!vh){ R3.hit = null; return null; }
  const inObj = G.mode === 'attack' && isObjCell(vh.x, vh.y);   // удар по объекту — сама клетка стены, иначе — куб перед гранью
  const x = inObj ? vh.x : vh.x + vh.nx, y = inObj ? vh.y : vh.y + vh.ny;
  R3.hit = {x, y, top: inObj ? vh.z + 1 : vh.z + vh.nz};
  return {x, y};
};
function r3Voxel(o, d, maxT){
  // клетка луча в координатах карты: X -> x, Z -> y, Y -> z
  let x = Math.floor(o.x), y = Math.floor(o.z), z = Math.floor(o.y);
  const sx = Math.sign(d.x), sy = Math.sign(d.z), sz = Math.sign(d.y);
  const tdx = sx ? Math.abs(1 / d.x) : 1e9, tdy = sy ? Math.abs(1 / d.z) : 1e9, tdz = sz ? Math.abs(1 / d.y) : 1e9;
  let tx = sx ? ((sx > 0 ? x + 1 - o.x : o.x - x) * tdx) : 1e9, ty = sy ? ((sy > 0 ? y + 1 - o.z : o.z - y) * tdy) : 1e9, tz = sz ? ((sz > 0 ? z + 1 - o.y : o.y - z) * tdz) : 1e9;
  let nx = 0, ny = 0, nz = 0, t = 0;
  for (let i = 0; i < 2000 && t <= Math.min(maxT, 1500); i++){
    if (x >= 0 && y >= 0 && x < AW && y < AH && z >= 0 && z < R3.H && R3.vox[vIdx(x, y, z)]) return {x, y, z, nx, ny, nz, t};
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
    if (f.alt) r3Text('🪽 ' + f.alt * 10 + ' фт', px, by - 19, '800 12px system-ui', '#bfe3ff', 3);
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
    else if (e.kind === 'cellbolt' && at(e.a)){ const a = mid(at(e.a)), Y = VZ0 + 2 * mapN('height', e.cx, e.cy), b = r3P(e.cx + .5, Y + .5, e.cy + .5); if (a.ok && b.ok){ ctx.fillStyle = e.col; ctx.shadowColor = e.col; ctx.shadowBlur = 18; ctx.beginPath(); ctx.arc(lerp(a.x, b.x, u), lerp(a.y, b.y, u), 8, 0, 7); ctx.fill(); } }
    else if (e.kind === 'cellring'){ ctx.globalAlpha = (1 - u) * .85; r3Circle(e.cx + .5, VZ0 + 2 * mapN('height', e.cx, e.cy) + .05, e.cy + .5, e.r * (.3 + .7 * Math.min(1, u * 2.2)), e.col, e.col + '33', null, 4); }
    else if ((e.kind === 'aura' || e.kind === 'spawn' || e.kind === 'flash') && (o || e.from)){
      const X = o ? o.X : e.from.x + .5, Z = o ? o.Z : e.from.y + .5, Y = o ? o.Y : r3Z(e.from);
      ctx.globalAlpha = (1 - u) * .8; r3Circle(X, Y + .05, Z, (o ? o.n : 1) * (e.kind === 'spawn' ? 1.4 - u * .7 : .7 + u * .6), e.kind === 'spawn' ? '#d6a8ff' : (e.col || '#fff'), null, null, 4);
    }
    ctx.restore();
  });
}
// подсветка своего хода: дальность движения и оружия, цели, клетка под курсором с расстоянием
function r3Turn(list, now){
  if (!G.req || G.busy || G.over) return;
  const q = G.req, me = list.find(o => o.k === q.me); if (!me) return;
  if (G.mode === 'move' && q.speedLeft >= 5) r3Circle(me.X, me.Y + .04, me.Z, q.speedLeft / 5, 'rgba(95,208,138,.75)', 'rgba(95,208,138,.10)', [8, 6]);
  let range = 0;
  if (G.mode === 'attack') range = q.weapon.range; else if (G.mode.startsWith('attack:')){ const a = (q.alts || []).find(x => x.k === +G.mode.slice(7)); range = a ? a.range : 5; } else if (G.mode.startsWith('spell:')){ const a = q.abils[+G.mode.slice(6)]; range = a.self ? (a.ally ? a.range : 0) : a.range; }
  const ga = G.mode.startsWith('spell:') ? q.abils[+G.mode.slice(6)] : null;
  if (range){
    r3Circle(me.X, me.Y + .04, me.Z, range / 5 + (me.n - 1) / 2, 'rgba(255,214,90,.8)', null, [4, 6]);
    if (!(ga && ga.ground)) q.enemies.forEach(en => { const o = list.find(x => x.k === en.i); if (!o || o.f.hp <= 0) return; const ok = en.dist <= range && en.los && !en.charm; r3Circle(o.X, o.Y + .04, o.Z, .45 * o.n + .12 + Math.sin(now / 160) * .04, ok ? '#ff5d5d' : 'rgba(180,180,190,.7)', null, null, ok ? 4 : 2); });
  }
  const h = R3.hit; if (!h || h.x < 0 || h.y < 0 || h.x >= AW || h.y >= AH) return;
  if (ga && ga.ground){
    if (ga.radius) r3Circle(h.x + .5, h.top + .04, h.y + .5, ga.radius / 5, 'rgba(255,140,90,.85)', 'rgba(255,120,80,.16)');
    if (ga.wallLen){   // клетки барьера — тот же расчёт направления, что у 2D
      const dx = h.x - me.f.x, dy = h.y - me.f.y, ax = Math.abs(dx), ay = Math.abs(dy), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
      let px = 1, py = 0; if (ax === 0 && ay === 0){ px = 1; py = 0; } else if (ax >= 2 * ay){ px = 0; py = 1; } else if (ay >= 2 * ax){ px = 1; py = 0; } else { px = -sy; py = sx; }
      for (let i = -Math.floor((ga.wallLen - 1) / 2); i <= Math.floor(ga.wallLen / 2); i++){ const x = h.x + px * i, y = h.y + py * i; if (x < 0 || y < 0 || x >= AW || y >= AH) continue; r3Square(x, y, h.top, 1, '#d6c4ff', 'rgba(190,160,255,.45)'); }
    }
  }
  r3Square(h.x, h.y, h.top, 1, 'rgba(255,255,255,.95)', 'rgba(255,255,255,.12)');
  const n = me.n, gap = (a, b) => Math.max(0, b - (a + n - 1), a - b), d = 5 * Math.max(gap(me.f.x, h.x), gap(me.f.y, h.y)), p = r3P(h.x + 1, h.top, h.y);
  if (p.ok){ ctx.save(); ctx.textAlign = 'left'; r3Text(d + ' фт', p.x + 22, p.y, '700 13px system-ui', '#fff'); ctx.restore(); }
}
// кадр 3D: из render() (viewer_template.html) вместо 2D-рисования
R3.draw = function(now, t, tt){
  if (R3.map !== MAP) R3.setMap();
  const fr = FR[curIdx]; if (!fr) return;
  const W = cvW(), H = cvH(), dp = R3.ren.getPixelRatio();
  if (R3.cv.width !== Math.round(W * dp) || R3.cv.height !== Math.round(H * dp)) R3.ren.setSize(W, H, false);
  R3.syncDyn(fr); R3.syncFog();
  R3.U.uGrid.value = G.req && !G.busy && !G.over ? 1 : 0;   // сетка 5 фт — в свой ход
  const list = R3.syncFighters(fr, t);
  R3.camUpdate(list);
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
  R3.fpsN++; if (now - R3.fpsT0 >= 1000){ R3.fps = Math.round(R3.fpsN * 1000 / (now - R3.fpsT0)); R3.fpsN = 0; R3.fpsT0 = now; }
  ctx.save(); ctx.font = '700 11px system-ui'; ctx.textAlign = 'left'; ctx.textBaseline = 'bottom'; ctx.fillStyle = 'rgba(255,255,255,.7)'; ctx.fillText(`3D · ${R3.fps} к/с${R3.low ? ' · телефон' : ''}`, 8, H - 6); ctx.restore();
};
// переключатель вида (виден только по флагу ?3d=1 и после загрузки библиотеки): 2D остаётся запасным
$('view3dBtn').onclick = () => {
  if (!R3.ready || R3.failed) return;
  R3.on = !R3.on; R3.cv.style.display = R3.on ? 'block' : 'none'; cam.snap = true; R3.hit = null;
  $('view3dBtn').textContent = R3.on ? 'Вид: 3D' : 'Вид: 2D';
};
