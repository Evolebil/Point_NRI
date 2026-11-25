// Замок главного меню. Камера движется по замкнутой кривой с параметризацией по длине.
function buildCastleScene(T, canvas){
  const renderer = new T.WebGLRenderer({canvas, antialias:true, alpha:false, powerPreference:'low-power'});
  canvas.addEventListener('webglcontextlost',event=>{
    event.preventDefault();castleFailure=true;syncCastle();
  });
  canvas.addEventListener('webglcontextrestored',()=>{
    castleFailure=false;renderer.shadowMap.needsUpdate=true;syncCastle();
  });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.5));
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = T.PCFSoftShadowMap;
  renderer.shadowMap.autoUpdate = false;
  const scene = new T.Scene(), camera = new T.PerspectiveCamera(43, 1, .3, 1100);
  scene.fog = new T.FogExp2(0xb9a59e, .0048);
  scene.add(new T.HemisphereLight(0xffdfc6, 0x848e97, 1.55));
  const sun = new T.DirectionalLight(0xffcd94, 2.7);
  sun.position.set(-80,65,38); sun.castShadow = true;
  Object.assign(sun.shadow.camera,{left:-65,right:65,top:70,bottom:-55,near:5,far:210});
  sun.shadow.camera.updateProjectionMatrix();
  sun.shadow.mapSize.set(1024,1024); sun.shadow.normalBias=.065;
  scene.add(sun);
  let seed = 4067;
  const random = () => ((seed = (Math.imul(seed,1664525)+1013904223) >>> 0) / 4294967296);
  const sky = new T.Mesh(new T.SphereGeometry(850,48,24), new T.ShaderMaterial({
    side:T.BackSide, depthWrite:false,
    vertexShader:'varying vec3 vDir; void main(){vDir=position; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}',
    fragmentShader:`varying vec3 vDir;
      void main(){vec3 d=normalize(vDir); float h=max(d.y,0.);
        vec3 col=mix(vec3(.92,.65,.47),vec3(.27,.48,.63),smoothstep(-.02,.42,h));
        col=mix(col,vec3(.10,.25,.43),smoothstep(.36,.95,h));
        float glow=max(0.,dot(d,normalize(vec3(-.81,.16,-.55))));
        col+=vec3(.34,.19,.07)*pow(glow,12.)+vec3(1.,.82,.48)*smoothstep(.9993,.9998,glow);
        vec2 p=d.xz/max(.12,h+.09);float bands=sin(p.y*3.1+.8*sin(p.x*1.5))+ .55*sin(p.y*8.+p.x*.7)+.23*sin(p.y*19.-p.x*2.);
        col=mix(col,vec3(.86,.76,.70),.32*smoothstep(.15,1.3,bands)*smoothstep(.015,.15,h)*(1.-smoothstep(.28,.65,h)));
        gl_FragColor=vec4(col,1.);
        #include <colorspace_fragment>
      }`
  }));
  scene.add(sky);
  const stone=new T.MeshStandardMaterial({color:0xc4af98,roughness:.94,flatShading:true});
  // Каменные ряды в мировых координатах сохраняют масштаб на башнях и куртинах.
  stone.onBeforeCompile = sh => {
    sh.vertexShader='varying vec3 stonePos;\n'+sh.vertexShader.replace('#include <project_vertex>', '#include <project_vertex>\nstonePos=(modelMatrix*instanceMatrix*vec4(transformed,1.)).xyz;');
    sh.fragmentShader='varying vec3 stonePos;\n'+sh.fragmentShader.replace('#include <color_fragment>',`#include <color_fragment>
      vec3 face=abs(normalize(cross(dFdx(stonePos),dFdy(stonePos))));
      vec2 uv=vec2(face.x>face.z ? stonePos.z : stonePos.x,stonePos.y)/vec2(1.12,.54);
      uv.x+=.5*mod(floor(uv.y),2.);
      vec2 ed=min(fract(uv),1.-fract(uv));
      float seam=smoothstep(.018,.085,min(ed.x,ed.y));
      float grain=fract(sin(dot(floor(uv),vec2(12.9898,78.233)))*43758.5453);
      diffuseColor.rgb*=mix(.71,.91+.15*grain,mix(1.,seam,1.-smoothstep(.15,.8,max(fwidth(uv.x),fwidth(uv.y)))));`);
  };
  const paint = (color,flat=false) => new T.MeshStandardMaterial({color,roughness:.9,flatShading:flat});
  const slate=paint(0x394d61,true), trim=paint(0xddc7a2), timber=paint(0x513b30), brass=paint(0xc49857), green=paint(0x2e5e55,true);
  const wine=paint(0x852e3d), sigil=paint(0xe8b561);
  const light=new T.MeshBasicMaterial({color:0xffcc7c}), dark=paint(0x403c3d);
  const cube=new T.BoxGeometry(1,1,1), drum=new T.CylinderGeometry(1,1,1,16), spire=new T.ConeGeometry(1,1,12);
  const pine=new T.ConeGeometry(1,1,7), batches=new Map(), transform=new T.Object3D();
  function put(shape,material,x,y,z,w,h,d,rot=0,tilt=0,color=null){
    const key=shape.uuid+material.uuid;
    if(!batches.has(key)) batches.set(key,{shape,material,items:[]});
    transform.position.set(x,y,z); transform.rotation.set(0,rot,tilt); transform.scale.set(w,h,d); transform.updateMatrix();
    batches.get(key).items.push([transform.matrix.clone(),color]);
  }
  const box=(mat,x,y,z,w,h,d,rot=0)=>put(cube,mat,x,y,z,w,h,d,rot);
  function tower(x,z,base,r,h,roof){
    if(base<10) put(drum,stone,x,base-4,z,r*1.04,8.1,r*1.04);
    put(drum,stone,x,base+h/2,z,r,h,r);
    for(const y of [.08,.63,.92,1]) put(drum,trim,x,base+h*y,z,r*1.055,.35,r*1.055);
    for(let j=0;j<10;j++){
      const a=j*Math.PI/5, dx=Math.sin(a),dz=Math.cos(a);
      if(!roof) box(stone,x+dx*r,base+h+.65,z+dz*r,.82,1.24,.54,a);
      if(j%2===0){
        box(dark,x+dx*r*.99,base+h*.78,z+dz*r*.99,.64,1.85,.23,a);
        box(light,x+dx*r*1.015,base+h*.78,z+dz*r*1.015,.27,1.44,.22,a);
      }
    }
    if(roof){
      put(spire,slate,x,base+h+roof/2,z,r*1.23,roof,r*1.23);
      put(spire,brass,x,base+h+roof+.42,z,.19,1.2,.19);
    }
  }
  function wall(x,z,w,d,h){
    box(stone,x,8+h/2,z,w,h,d); box(trim,x,8+h-.25,z,w+.32,.5,d+.32);
    const along=w>d, len=Math.max(w,d), count=Math.max(1,Math.round(len/2));
    for(let i=0;i<count;i++){
      const offset=((i+.5)/count-.5)*len;
      box(stone,x+(along?offset:0),8+h+.57,z+(along?0:offset),along?1.12:w,1.15,along?d:1.12);
    }
  }
  // Арка с открытым пролётом, клинчатыми камнями и заполнением над сводом.
  function arch(x,z,y,r,thickness,alongZ=false){
    const wedge=new T.Shape();
    const outer=r+.72, a=Math.PI/15;
    wedge.moveTo(r,0); wedge.lineTo(outer,0);wedge.lineTo(outer*Math.cos(a),outer*Math.sin(a));
    wedge.lineTo(r*Math.cos(a),r*Math.sin(a));wedge.closePath();
    const geom=new T.ExtrudeGeometry(wedge,{depth:thickness,bevelEnabled:false});
    geom.translate(0,0,-thickness/2);
    for(let i=0;i<15;i++) put(geom,trim,x,y,z,1,1,1,alongZ?Math.PI/2:0,i*a);
    for(let i=0;i<20;i++){
      const offset=((i+.5)/20*2-1)*outer, foot=Math.sqrt(Math.max(0,outer*outer-offset*offset)), h=outer+.5-foot;
      box(stone,x+(alongZ?0:offset),y+foot+h/2,z+(alongZ?offset:0),alongZ?thickness:outer/10+.04,h,alongZ?outer/10+.04:thickness);
    }
  }
  for(const x of [-18,18]){wall(x,0,2.4,26,7.2); for(const z of [-13,13]) tower(x,z,7,3.5,11,z<0?5.8:0);}
  wall(0,-13,36,2.4,7.2);
  for(const x of [-12,12]) wall(x,13,12,2.5,7.2);
  for(const x of [-4.9,4.9]) tower(x,14,7.5,2.45,11.2,4.5);
  arch(0,14,10.35,2.3,2.4);
  box(timber,0,10,12.6,4.7,4.8,.35);
  for(let i=-2;i<=2;i++) box(dark,i*.84,10.2,12.86,.12,4.75,.12);
  box(stone,-2,16.2,-3,13,16.4,10);
  box(trim,-2,24.4,-3,13.6,.55,10.6);
  const pitched=new T.CylinderGeometry(0,1,1,4);pitched.rotateY(Math.PI/4);
  put(pitched,slate,-2,27.55,-3,10.8,6.1,8.4);
  for(const x of [-8.3,4.3])for(const z of [-7.7,1.7]) tower(x,z,8,2.15,18+(z<0?3:0),6);
  tower(-4,-7,22,3.1,9,8);
  for(const x of [-6,-3,0,3]) for(const y of [17.6,21.8]){
    box(dark,x,y,2.06,.91,2.15,.22);box(light,x,y,2.2,.53,1.65,.12);
  }
  for(const x of [-2.8,2.8]){
    box(wine,x,14.05,15.42,.85,3.9,.09);box(sigil,x,14.65,15.48,.27,.76,.06);
  }
  for(const x of [-18,18])for(const z of [-8,-2,4,10]){
    box(stone,x,7.6,z,3.1,14.5,1.15);box(trim,x,14.7,z,3.2,.32,1.28);
  }
  for(const x of [-6,-3,0,3])for(const y of [17.6,21.8]){
    box(dark,x,y,-8.05,.95,2.15,.20);box(light,x,y,-8.17,.43,1.55,.10);
  }
  const flags=[];
  for(const [x,y,z] of [[-4,38.9,-7],[-18,24,-13],[18,24,-13]]){
    put(drum,brass,x,y+1.3,z,.065,3,.065);
    const geometry=new T.PlaneGeometry(3.3,1.05,12,3);
    geometry.translate(1.65,-.52,0);
    const material=wine.clone();material.side=T.DoubleSide;
    const flag=new T.Mesh(geometry,material);flag.position.set(x,y+2.65,z);
    scene.add(flag);flags.push(flag);
  }
  for(const side of [-1,1]){
    box(stone,side*12,10.7,-1,4.5,5.4,10);
    put(pitched,slate,side*12,14.8,-1,3.75,3.1,7.65);
  }
  // Виадук опирается на дно ущелья. Его ось ведёт взгляд к надвратным башням.
  for(let k=0;k<4;k++){
    const z=20+k*8;
    arch(0,z,3.55,3.45,4.2,true);
    box(stone,0,-2,z+4,4.2,12,1.2);
  }
  box(stone,0,7.85,32,4.65,.64,35);
  for(const x of [-2.32,2.32]) box(trim,x,8.48,32,.36,.95,35);

  function landHeight(x,z){
    const r=Math.pow((x/24)**4+(z/19)**4,.25);
    const noise=Math.sin(.25*x+.18*z)*.65+Math.sin(z*.53-x*.16)*.37;
    const rim=r+.04*noise;
    if(rim<.95) return 7.96;
    const cliff=8-Math.min(1,(rim-.95)/.41)*18;
    const shore= -11+2*Math.sin(x*.08+z*.09)+noise;
    const approach=7.9-Math.max(0,48-z)*.96-Math.abs(x)*.18;
    return Math.max(cliff+noise*(Math.min(1,(rim-.95)*3)),shore,Math.min(14,approach)+noise*.4);
  }
  function terrain(size,step,height,shade){
    const pos=[],colors=[],indices=[],n=Math.ceil(size/step)+1;
    for(let j=0;j<n;j++)for(let i=0;i<n;i++){
      const x=(i-(n-1)/2)*step+(random()-.5)*step*.52,z=(j-(n-1)/2)*step+(random()-.5)*step*.52;
      const y=height(x,z);pos.push(x,y,z);
      const color=new T.Color(shade(x,y,z)).multiplyScalar(.93+.12*random()); colors.push(color.r,color.g,color.b);
      const v=j*n+i;if(i&&j)indices.push(v-n-1,v-1,v,v-n-1,v,v-n);
    }
    const geo=new T.BufferGeometry();geo.setAttribute('position',new T.Float32BufferAttribute(pos,3));
    geo.setAttribute('color',new T.Float32BufferAttribute(colors,3));geo.setIndex(indices);geo.computeVertexNormals();
    const m=new T.Mesh(geo,new T.MeshStandardMaterial({vertexColors:true,flatShading:true,roughness:1}));
    m.receiveShadow=true;scene.add(m);
  }
  terrain(240,2.8,landHeight,(x,y,z)=>y>6?0x69734d:y>0?0x897e71:0x566564);
  function ridge(x,z){
    const dist=Math.hypot(x,z);
    if(dist<112)return -21;
    const peak=32+17*Math.sin(3*Math.atan2(z,x)+.7)+10*Math.cos(x*.033+z*.012)+10*Math.sin(z*.061);
    return -14+peak*Math.min(1,(dist-112)/75)+10*Math.sin(x*.057)*Math.cos(z*.046);
  }
  terrain(840,11,ridge,(x,y)=>y>39?0xabb0ab:y>18?0x73858a:0x526f77);
  for(let i=0;i<1250;i++){
    const x=(random()-.5)*229,z=(random()-.5)*229,y=landHeight(x,z);
    if(y<-8 || (Math.abs(x)<22&&Math.abs(z)<18) || (Math.abs(x)<4&&z>0)) continue;
    const h=2.4+random()*4.5, tint=new T.Color().setScalar(.7+random()*.38);
    for(let j=0;j<3;j++)put(pine,green,x,y+h*(.3+j*.23),z,h*(.24-j*.048),h*(.63-j*.1),h*(.24-j*.048),0,0,tint);
  }
  const water=new T.Mesh(new T.PlaneGeometry(1000,1000),new T.MeshStandardMaterial({color:0x47838b,metalness:.22,roughness:.34}));
  water.rotation.x=-Math.PI/2; water.position.y=-7.9;scene.add(water);
  for(const {shape,material,items} of batches.values()){
    const mesh=new T.InstancedMesh(shape,material,items.length);
    items.forEach(([matrix,color],i)=>{mesh.setMatrixAt(i,matrix);if(color)mesh.setColorAt(i,color);});
    mesh.castShadow=true;mesh.receiveShadow=true;scene.add(mesh);
  }
  renderer.shadowMap.needsUpdate=true;
  const path=new T.CatmullRomCurve3([[40,24,62],[17,18,67],[-28,22,57],[-49,31,26],[-51,39,-20],[-18,42,-51],[32,34,-42],[53,26,-9],[54,20,31]].map(p=>new T.Vector3(...p)),true);
  let width=0,height=0;
  return elapsed=>{
    if(width!==canvas.clientWidth||height!==canvas.clientHeight){
      width=canvas.clientWidth;height=canvas.clientHeight;
      if(!width||!height)return;
      renderer.setSize(width,height,false);camera.aspect=width/height;
      camera.fov=width<height?58:43;camera.updateProjectionMatrix();
    }
    const t=(elapsed%180)/180, pos=path.getPointAt(t);
    camera.position.copy(pos);camera.lookAt(-1,14.7+1.8*Math.sin(t*2*Math.PI),0);
    camera.filmOffset=width>height?-5.5:0;camera.updateProjectionMatrix();
    for(let i=0;i<flags.length;i++){
      const vertices=flags[i].geometry.attributes.position;
      for(let j=0;j<vertices.count;j++){
        const x=vertices.getX(j);
        vertices.setZ(j,x*.13*Math.sin(2*x-elapsed*1.5+i));
      }
      vertices.needsUpdate=true;
    }
    sky.position.copy(pos);renderer.render(scene,camera);
  };
}

let castleRender, castleLoading=false, castleFailure=false, castleFrame=0, castleElapsed=0, castlePrevious=0;
function castleLive(){
  return document.documentElement.dataset.bg==='live' && !document.hidden && !document.body.classList.contains('full') && !matchMedia('(prefers-reduced-motion: reduce)').matches;
}
function syncCastle(){
  const canvas=$('castleScene');
  const live=castleLive(), film=$('castleFallback');
  if(!live || castleFailure){
    film.hidden=!(live && castleFailure);
    if(film.hidden)film.pause();
    else {
      if(!film.hasAttribute('src'))film.src='/assets/castle.mp4';
      film.playbackRate=.55;film.play().catch(()=>{});
    }
    cancelAnimationFrame(castleFrame);castleFrame=0;castlePrevious=0;canvas.hidden=true;return;
  }
  if(!castleRender){
    if(castleLoading)return;
    castleLoading=true;
    loadThreeLibrary().then(T=>{castleRender=buildCastleScene(T,canvas);})
      .catch(err=>{castleFailure=true;console.warn('Фон замка:',err);})
      .finally(()=>{castleLoading=false;syncCastle();});
    return;
  }
  if(!film.hidden){ film.pause();film.hidden=true; }
  canvas.hidden=false;
  if(castleFrame)return;
  function tick(now){
    if(!castlePrevious || now-castlePrevious>=32){
      if(castlePrevious)castleElapsed+=Math.min(now-castlePrevious,120)/1000;
      castlePrevious=now;castleRender(castleElapsed);
    }
    castleFrame=requestAnimationFrame(tick);
  }
  castleFrame=requestAnimationFrame(tick);
}
