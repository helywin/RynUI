// Locked independent headless producer. No reference-cache publication.
import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import os from 'node:os';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import {createRequire} from 'node:module';
import {fontStyle,platformFontRuns} from './font_faces.mjs';

const config=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
const {prepared,output,build,actual_font_sources}=config;
const requestRaw=fs.readFileSync(config.request_path);
const request=JSON.parse(requestRaw.toString('utf8'));
const require=createRequire(path.join(prepared.install,'package.json'));
const {chromium}=require('playwright');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const expect=(v,m)=>{if(!v)throw new Error(m);};
expect(Array.isArray(request.cells)&&request.cells.length<=384,'control cell count exceeds 384');
expect(process.env.RYNUI_CONTROL_LAUNCH_ID===request.process_launch_id,'actual process launch ID differs');
const processStarted=process.hrtime.bigint().toString();
function decodePNG(png){
  expect(png.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10])),'PNG signature');
  let offset=8,width,height,channels;const chunks=[];
  while(offset<png.length){
    const size=png.readUInt32BE(offset),kind=png.toString('ascii',offset+4,offset+8),data=png.subarray(offset+8,offset+8+size);
    expect(data.length===size && offset+12+size<=png.length,'PNG chunk extent');
    if(kind==='IHDR'){width=data.readUInt32BE(0);height=data.readUInt32BE(4);channels=data[9]===2?3:data[9]===6?4:0;
      expect(data[8]===8 && channels && data[10]===0 && data[11]===0 && data[12]===0,'PNG RGB8/RGBA8 encoding');}
    if(kind==='IDAT')chunks.push(data);offset+=size+12;if(kind==='IEND')break;
  }
  expect(width===Math.round(1280*request.actual_dpr)&&height===Math.round(900*request.actual_dpr),'PNG actual viewport extent');
  const stream=zlib.inflateSync(Buffer.concat(chunks),{maxOutputLength:(1280*4*4+1)*900*4});const stride=width*channels;
  expect(stream.length===(stride+1)*height,'PNG inflated extent');
  const unfiltered=Buffer.alloc(stride*height),raw=Buffer.alloc(width*height*4);
  const paeth=(a,b,c)=>{const p=a+b-c,aa=Math.abs(p-a),bb=Math.abs(p-b),cc=Math.abs(p-c);return aa<=bb&&aa<=cc?a:bb<=cc?b:c;};
  for(let y=0;y<height;y++){
    const filter=stream[y*(stride+1)];expect(filter<=4,'PNG filter');
    for(let x=0;x<stride;x++){
      const i=y*stride+x,a=x>=channels?unfiltered[i-channels]:0,b=y?unfiltered[i-stride]:0,c=y&&x>=channels?unfiltered[i-stride-channels]:0;
      const predict=[0,a,b,Math.floor((a+b)/2),paeth(a,b,c)][filter];
      unfiltered[i]=(stream[y*(stride+1)+1+x]+predict)&255;
    }
  }
  for(let i=0;i<width*height;i++){for(let c=0;c<3;c++)raw[i*4+c]=unfiltered[i*channels+c];raw[i*4+3]=channels===4?unfiltered[i*channels+3]:255;}
  return {width,height,data:raw};
}
const assets=new Map([
  ['/',[Buffer.from(`<!doctype html><meta charset="utf-8"><style>${fontStyle(request.font_files)}html,body{margin:0;background:white}</style>${fs.existsSync(path.join(build,'app.css'))?'<link rel="stylesheet" href="/app.css">':''}<div id="root"></div><script src="/request.js"></script><script src="/app.js"></script>`),'text/html; charset=utf-8']],
  ['/request.js',[Buffer.from('window.__controlRequest='+JSON.stringify(request).replaceAll('<','\\u003c')+';'),'application/javascript']],
  ['/app.js',[fs.readFileSync(path.join(build,'app.js')),'application/javascript']]
]);
if(fs.existsSync(path.join(build,'app.css')))assets.set('/app.css',[fs.readFileSync(path.join(build,'app.css')),'text/css']);
request.font_files.forEach((font,i)=>assets.set('/font/'+i,[fs.readFileSync(font.path),'font/ttf']));
const errors=[];
const served=new Map();
let server,browser,context;
try {
  server=http.createServer((req,res)=>{const a=assets.get(req.url);if(!a){errors.push(req.url);res.writeHead(404).end();return;}
    const old=served.get(req.url);served.set(req.url,{url:req.url,sha256:hash(a[0]),bytes:a[0].length,requests:(old?.requests??0)+1});
    res.writeHead(200,{'Content-Type':a[1],'Cache-Control':'no-store'}).end(a[0]);});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const origin='http://127.0.0.1:'+server.address().port;
  const launch_args=['--force-color-profile=srgb','--disable-lcd-text','--disable-background-networking',
    '--disable-component-update','--disable-default-apps','--disable-smooth-scrolling','--no-first-run','--lang=zh-CN'];
  browser=await chromium.launch({executablePath:prepared.browser,headless:true,args:launch_args,
    env:{...process.env,TMPDIR:path.join(output,'tmp'),XDG_CACHE_HOME:path.join(output,'cache'),XDG_CONFIG_HOME:path.join(output,'config')},timeout:20000});
  expect(browser.version()==='141.0.7390.37','browser revision drift');
  context=await browser.newContext({viewport:{width:1280,height:900},deviceScaleFactor:request.actual_dpr,
    locale:'zh-CN',timezoneId:'UTC',reducedMotion:'reduce',colorScheme:'light',serviceWorkers:'block',acceptDownloads:false});
  await context.route('**/*',async route=>{const url=new URL(route.request().url());
    if(url.origin!==origin || !assets.has(url.pathname) || url.search || url.hash){errors.push('network '+url.href);await route.abort();}
    else await route.continue();});
  const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));
  await page.goto(origin,{waitUntil:'load'});await page.waitForFunction(()=>window.__controlReady===true);
  await page.evaluate(async()=>{await document.fonts.ready;});
  await page.evaluate(()=>{for(const animation of document.getAnimations()){animation.pause();animation.currentTime=0;}});
  const animationFacts=await page.evaluate(()=>document.getAnimations().map(a=>({play_state:a.playState,current_time:a.currentTime})));
  expect(animationFacts.every(a=>a.play_state==='paused'&&a.current_time===0),'static independent control animation is not observable at zero');
  expect(await page.evaluate(()=>devicePixelRatio)===request.actual_dpr,'actual Web DPR differs');
  const cdp=await context.newCDPSession(page);await cdp.send('DOM.enable');await cdp.send('CSS.enable');
  const session=await browser.newBrowserCDPSession();const info=await session.send('SystemInfo.getInfo');
  const keys=['glVendor','glRenderer','glVersion','glImplementationParts','displayType','glWsVendor','glWsVersion',
    'skiaBackendType','subpixelFontRendering','directRenderingVersion','sandboxed'];
  const actual_gpu={devices:info.gpu.devices,aux_attributes:Object.fromEntries(keys.filter(k=>Object.hasOwn(info.gpu.auxAttributes,k)).map(k=>[k,info.gpu.auxAttributes[k]])),feature_status:info.gpu.featureStatus};
  const browserFacts={version:browser.version(),executable_sha256:prepared.browser_sha256,revision:'1194',headless:true,
    launch_args,renderer:info.gpu.auxAttributes.glRenderer ?? 'Chromium headless backend not reported',actual_gpu,
    os:os.platform(),release:os.release(),arch:os.arch(),strict_mode:false,color_profile:'sRGB-SDR'};
  const observe=async()=>await page.evaluate(cells=>cells.map(cell=>{
    const owner=document.querySelector('[data-control="'+cell.identity+'"]');
    if(!owner)throw new Error('actual control absent');
    const r=owner.getBoundingClientRect();const rect=b=>({x:b.x,y:b.y,width:b.width,height:b.height});
    const computed=getComputedStyle(owner);
    const parseColor=value=>{const m=value.match(/rgba?\(([^)]+)\)/);if(!m)throw new Error('computed control color unavailable');
      const v=m[1].split(',').map(Number);return {rgb:v.slice(0,3),alpha:v[3]??1};};
    let color=parseColor(cell.kind==='border'?computed.borderTopColor:cell.kind==='shadow'?computed.boxShadow:computed.backgroundColor);
    if(cell.kind==='text'||cell.kind==='icon')color=parseColor(getComputedStyle(owner.firstElementChild).color);
    const opacity=Number(computed.opacity);
    const source={foreground:color.rgb,alpha:color.alpha*opacity,source_alpha:color.alpha,paint_opacity:opacity};
    if(cell.kind==='border')source.border_width=parseFloat(computed.borderTopWidth);
    if(cell.kind==='shadow'){
      const values=computed.boxShadow.replace(/rgba?\([^)]+\)/,'').trim().split(/\s+/).map(parseFloat);
      if(values.length!==4||values.some(v=>!Number.isFinite(v)))throw new Error('actual shadow parameters unavailable');
      source.offset=values.slice(0,2);source.blur=values[2];source.spread=values[3];
      const fill=parseColor(computed.backgroundColor);
      source.occluding_fill={foreground:fill.rgb,source_alpha:fill.alpha,paint_opacity:1};
    }
    if(cell.radius!==undefined)source.radius=parseFloat(computed.borderRadius);
    if(cell.kind==='sourceover'){const second=parseColor(getComputedStyle(owner.firstElementChild).backgroundColor);
      source.second_foreground=second.rgb;source.second_alpha=second.alpha;}
    if(cell.kind==='clip'){const m=computed.clipPath.match(/^inset\(0px ([\d.]+)px 0px 0px\)$/);
      if(!m)throw new Error('actual control clip unavailable');source.clip_rect={x:r.x,y:r.y,width:r.width-Number(m[1]),height:r.height};}
    if(cell.kind==='text'||cell.kind==='icon'){
      const child=owner.firstElementChild;const style=getComputedStyle(child);
      const range=document.createRange();range.selectNodeContents(child);
      Object.assign(source,{content:cell.kind==='text'?child.textContent:child.getAttribute('data-icon') ?? '',font_size:parseFloat(style.fontSize),font_weight:parseInt(style.fontWeight),
        advance:range.getBoundingClientRect().width,ink_boxes:[...range.getClientRects()].map(rect)});
      if(cell.kind==='icon'){
        const svg=child.matches('svg')?child:child.querySelector('svg');
        if(!svg)throw new Error('actual formal SVG unavailable');
        source.svg={source_kind:'svg',view_box:svg.getAttribute('viewBox'),paths:[...svg.querySelectorAll('path')].map((part,ordinal)=>{
          const style=getComputedStyle(part);const ancestry=[];
          for(let node=part;node;node=node.parentElement){
            const s=getComputedStyle(node);ancestry.push({transform_attr:node.getAttribute('transform'),transform_css:s.transform,
              clip_path:s.clipPath,mask:s.maskImage,filter:s.filter});
          }
          return {ordinal,d:part.getAttribute('d'),fill_rule:style.fillRule,fill_opacity:style.fillOpacity,fill:style.fill,
            stroke:style.stroke,ancestry};})};
        source.content=svg.getAttribute('data-icon') ?? child.getAttribute('aria-label') ?? '';
      }
      if(cell.clip_glyph_right_px){owner.style.clipPath=`inset(0px ${r.width-cell.outline_right_logical+cell.clip_glyph_right_px}px 0px 0px)`;}
    }
    return {identity:cell.identity,kind:cell.kind,rect:rect(r),source};
  }),request.cells);
  const observedSnapshot=async()=>{
    const observed=await observe();
    const documentNode=await cdp.send('DOM.getDocument',{depth:-1,pierce:true});
    for(const cell of observed.filter(c=>c.kind==='text')){
      const node=await cdp.send('DOM.querySelector',{nodeId:documentNode.root.nodeId,selector:'[data-control="'+cell.identity+'"]'});
      const runs=await platformFontRuns(cdp,node.nodeId,actual_font_sources,true);
      const selected=runs.flatMap(r=>r.sources);
      expect(selected.length>0,'actual control font unavailable');
      const names=selected.map(s=>s.postScriptName);
      const font=actual_font_sources.find(f=>f.names['6'].some(n=>names.includes(n)) && f.weight===cell.source.font_weight);
      expect(font,'control font face/weight differs');cell.source.actual_font_sha256=font.sha256;
      cell.source.platform_font_runs=runs;
    }
    return observed;
  };
  const actualClear=await page.evaluate(()=>{const value=getComputedStyle(document.body).backgroundColor;
    const m=value.match(/rgba?\(([^)]+)\)/);if(!m)throw new Error('actual canvas clear unavailable');
    const parts=m[1].split(',').map(Number);return [...parts.slice(0,3).map(v=>v/255),parts[3]??1];});
  fs.writeFileSync(path.join(output,'browser.json'),JSON.stringify(browserFacts)+'\n',{flag:'wx'});
  for(let i=0;i<request.repetitions;i++){
    const observed=await observedSnapshot();
    const snapshotJSON=JSON.stringify({controls:observed,commands:[]});
    // Static controls; allow means screenshot cannot fast-forward an animation.
    const png=await page.screenshot({type:'png',scale:'device',fullPage:false,animations:'allow',caret:'initial'});
    expect(JSON.stringify({controls:await observedSnapshot(),commands:[]})===snapshotJSON,'actual source snapshot changed during capture');
    const decoded=decodePNG(png);const raw=decoded.data;
    const name='capture-'+i+'.png',rawName='capture-'+i+'.rgba';
    fs.writeFileSync(path.join(output,name),png,{flag:'wx'});fs.writeFileSync(path.join(output,rawName),raw,{flag:'wx'});
    let min=255,max=0,opaque=0;for(let p=3;p<raw.length;p+=4){min=Math.min(min,raw[p]);max=Math.max(max,raw[p]);opaque+=raw[p]===255;}
    const record={schema_version:1,calibration_kind:'pilot-independent-v1',side:'web',nonce:request.nonce+'-'+i,
      process_nonce:process.pid+':'+processStarted,process:{pid:process.pid,launch_id:process.env.RYNUI_CONTROL_LAUNCH_ID,started_ticks:processStarted},
      request_sha256:hash(requestRaw),sample_ordinal:i,snapshot_sha256:hash(Buffer.from(snapshotJSON)),snapshot_json:snapshotJSON,
      submitted_commands:[],board:request.board,fault:request.fault,restart:request.restart,
      provenance:{binding:request.binding,browser:browserFacts,source_closure:JSON.parse(fs.readFileSync(path.join(build,'closure.json'),'utf8'))},
      capture:{path:name,sha256:hash(png),raw:rawName,raw_sha256:hash(raw),width:decoded.width,height:decoded.height,dpr:request.actual_dpr,
        clear_rgba:actualClear,readback_id:i+1,fence_id:i+1,fence_completed:true,codec_fence_id:i+1,codec_readback_id:i+1,codec_raw_match:true,
        raw_alpha_available:true,raw_alpha_min:min,raw_alpha_max:max,
        native_receipt:null,
        raw_opaque_pixels:opaque,raw_total_pixels:raw.length/4,source_channel_order:'RGBA',framebuffer_format:png[25]===2?'PNG RGB8':'PNG RGBA8',
        color_profile:'sRGB-SDR',alpha:png[25]===2?'opaque':'straight'},controls:observed,complete:true};
    expect(!errors.length,errors.join(';'));fs.writeFileSync(path.join(output,'stream-'+i+'.json'),JSON.stringify(record)+'\n',{flag:'wx'});
  }
  fs.writeFileSync(path.join(output,'served-assets.json'),JSON.stringify({assets:[...served.values()]})+'\n',{flag:'wx'});
  fs.writeFileSync(path.join(output,'served-request.js'),assets.get('/request.js')[0],{flag:'wx'});
  fs.writeFileSync(path.join(output,'served-index.html'),assets.get('/')[0],{flag:'wx'});
}finally{if(context)await context.close();if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));}
