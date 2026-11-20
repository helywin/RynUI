// Readonly Chromium layout/paint facts. No style, DOM, input, or screenshot mutation.
import crypto from 'node:crypto';

export const paintStyleNames = [
  'display','visibility','position','z-index','isolation','opacity','transform','transform-origin',
  'mix-blend-mode','filter','backdrop-filter','clip-path','mask-image','overflow-x','overflow-y','contain',
  'background-color','background-image','background-clip','background-origin','box-shadow',
  'border-top-width','border-right-width','border-bottom-width','border-left-width',
  'border-top-style','border-right-style','border-bottom-style','border-left-style',
  'border-top-color','border-right-color','border-bottom-color','border-left-color',
  'border-top-left-radius','border-top-right-radius','border-bottom-right-radius','border-bottom-left-radius',
  'padding-top','padding-right','padding-bottom','padding-left',
  'outline-color','outline-style','outline-width','outline-offset','color','-webkit-text-fill-color',
  'font-family','font-size','font-weight','font-style','line-height','direction','content',
  'fill','fill-opacity','fill-rule','stroke','stroke-opacity','stroke-width','stroke-linecap','stroke-linejoin',
  'caret-color','caret-animation','text-shadow'
];

// Executed on a resolved real DOM/UA Text node, including a portal-owned popup.
function actualNode() {
  const node = this;
  const element = node.nodeType === Node.ELEMENT_NODE ? node : node.parentElement;
  const rect = b => ({x:b.x,y:b.y,width:b.width,height:b.height});
  if (!element) return {status:'unavailable',reason:'no style-bearing real parent'};
  const clips = [];
  const parentOf=element=>element.parentElement??element.getRootNode()?.host;
  for (let ancestor=parentOf(element);ancestor;ancestor=parentOf(ancestor)) {
    const s=getComputedStyle(ancestor), b=ancestor.getBoundingClientRect();
    if (/(hidden|clip|scroll|auto)/.test(s.overflowX+' '+s.overflowY)) clips.push({
      bounds:{x:b.x+ancestor.clientLeft,y:b.y+ancestor.clientTop,width:ancestor.clientWidth,height:ancestor.clientHeight},
      overflow_x:s.overflowX,overflow_y:s.overflowY,clip_path:s.clipPath,transform:s.transform});
  }
  const result={status:'observed',clips,semantic:window.__reference?.semantic(element)??'',
    bounds:rect(element.getBoundingClientRect()),tag:element.tagName,attributes:Object.fromEntries(
      ['d','viewBox','transform','clip-path','fill-rule','role','aria-label'].map(name=>[name,element.getAttribute(name)]))};
  if(node.nodeType===Node.TEXT_NODE) {
    const range=document.createRange();range.selectNodeContents(node);
    const boxes=[...range.getClientRects()].map(rect);
    result.text={content:node.textContent,boxes,
      semantic_groups:window.__observedTextGroups(node)};
    const button=element.parentElement;
    if (element.tagName==='SPAN' && button?.tagName==='BUTTON' && element.childNodes.length===1 &&
        element.getClientRects().length===1 && boxes.length===1 &&
        Object.keys(result.bounds).every(key=>boxes[0][key]===result.bounds[key])) {
      const style=getComputedStyle(element), buttonStyle=getComputedStyle(button);
      const verticalInsets=[style.paddingTop,style.paddingBottom,style.borderTopWidth,style.borderBottomWidth];
      const generatedContent=['::before','::after'].some(pseudo=>
        !['none','normal'].includes(getComputedStyle(element,pseudo).content));
      let untransformed=Number.isInteger(window.scrollY);
      for (let ancestor=element;ancestor && untransformed;ancestor=ancestor.parentElement) {
        const s=getComputedStyle(ancestor);
        untransformed=s.transform==='none' && s.translate==='none' && s.rotate==='none' &&
          s.scale==='none' && s.zoom==='1' && Number.isInteger(ancestor.scrollTop);
      }
      // The only text fills one line in the blockified flex item. Its top is
      // the line-fragment origin Blink rounds in TextFragmentPainter.
      if (untransformed && !generatedContent && verticalInsets.every(value=>value==='0px') &&
          style.display==='block' && style.writingMode==='horizontal-tb' &&
          ['flex','inline-flex'].includes(buttonStyle.display)) result.text.block_line_top=result.bounds.y;
    }
    const host=node.getRootNode()?.host;
    if(host?.matches('input,textarea') && document.activeElement===host && typeof host.selectionStart==='number') {
      const selected=getComputedStyle(host,'::selection');
      result.text.selection={start:host.selectionStart,end:host.selectionEnd,unit:'UTF-16',
        color:selected.color,background:selected.backgroundColor};
      if(host.selectionStart===host.selectionEnd && host.selectionEnd<=node.textContent.length) {
        const caret=document.createRange();caret.setStart(node,host.selectionEnd);caret.collapse(true);
        result.text.caret={edge:rect(caret.getBoundingClientRect()),color:getComputedStyle(host).caretColor,
          animation:getComputedStyle(host).getPropertyValue('caret-animation'),width_source:'UA caret edge; width not exposed by DOM'};
      }
    }
  }
  if(element.matches('input,textarea')) {
    const focused=document.activeElement===element;
    const selection=getComputedStyle(element,'::selection');
    result.editor={focused,value:element.value,placeholder:element.placeholder,
      selection_start:element.selectionStart,selection_end:element.selectionEnd,
      selection_direction:element.selectionDirection,selection_color:selection.color,
      selection_background:selection.backgroundColor,caret_color:getComputedStyle(element).caretColor,
      caret_animation:getComputedStyle(element).getPropertyValue('caret-animation'),
      scroll_left:element.scrollLeft,scroll_top:element.scrollTop};
  }
  return result;
}

async function readActualNodes(cdp, nodes) {
  const records = new Map();
  const batchSize = 64;
  const objectGroup = 'readonly-paint-actual-nodes';
  const declaration = `function (...nodes) {
    const observe = ${actualNode.toString()};
    return nodes.map(node => observe.call(node));
  }`;
  for (let start = 0; start < nodes.length; start += batchSize) {
    try {
      const batch = await Promise.all(nodes.slice(start, start + batchSize).map(async id => {
        const remote = await cdp.send('DOM.resolveNode', {backendNodeId: id, objectGroup});
        return {id, objectId: remote.object.objectId};
      }));
      const resolved = batch.filter(node => node.objectId);
      if (!resolved.length) continue;
      const query = await cdp.send('Runtime.callFunctionOn', {objectId: resolved[0].objectId,
        functionDeclaration: declaration, arguments: resolved.map(({objectId}) => ({objectId})),
        returnByValue: true});
      if (query.exceptionDetails || !Array.isArray(query.result.value)
          || query.result.value.length !== resolved.length) {
        throw new Error('actual paint-bearing node query failed');
      }
      resolved.forEach(({id}, index) => records.set(id, query.result.value[index]));
    } finally {
      await cdp.send('Runtime.releaseObjectGroup', {objectGroup});
    }
  }
  return records;
}

async function observeEmptySelectCaret(cdp, layer, compositorLayers, viewport) {
  const {editor,attributes,bounds}=layer.actual;
  if (attributes?.role!=='combobox' || !editor?.focused || editor.value!=='' || editor.selection_end!==0 ||
      editor.selection_start!==0 || editor.caret_animation!=='manual' ||
      !layer.composition_group_path.some(group=>Number(group.opacity)>0 && Number(group.opacity)<1)) return;
  const rtl=layer.style.direction==='rtl';
  const edge=rtl?bounds.x+bounds.width:bounds.x;
  if (edge<0 || edge>=viewport.width || bounds.y+bounds.height<=0 || bounds.y>=viewport.height) return;
  const matches=compositorLayers.filter(candidate=>{
    const transform=candidate.transform;
    const identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
    if (!candidate.drawsContent || candidate.backendNodeId || candidate.width!==1 || candidate.height<=0 ||
        transform?.length!==identity.length || transform.some((value,i)=>i!==12 && i!==13 && value!==identity[i]))
      return false;
    // The anonymous caret layer must belong to the actual focused editor's
    // inline start and fit vertically in that editor's real layout box.
    return Math.abs(transform[12]+(rtl?candidate.width:0)-edge)<=1 &&
      transform[13]>=bounds.y && transform[13]+candidate.height<=bounds.y+bounds.height;
  });
  if (matches.length!==1) throw new Error('visible empty Select caret has no unique compositor layer');
  const candidate=matches[0];
  const {snapshotId}=await cdp.send('LayerTree.makeSnapshot',{layerId:candidate.layerId});
  try {
    const {commandLog}=await cdp.send('LayerTree.snapshotCommandLog',{snapshotId});
    const color=commandLog[0]?.params?.paint?.color;
    if (commandLog.length!==1 || commandLog[0].method!=='drawPaint' || !/^#[0-9A-Fa-f]{8}$/.test(color))
      throw new Error('empty Select caret does not have a single compositor paint');
    return {source:'Chromium LayerTree.snapshotCommandLog',compositor_layer_id:candidate.layerId,
      bounds:{x:candidate.transform[12],y:candidate.transform[13],width:candidate.width,height:candidate.height},
      argb:color};
  } finally {
    await cdp.send('LayerTree.releaseSnapshot',{snapshotId});
  }
}

function isolatedShadowCommands(log, layer, bounds) {
  if (log[0]?.method!=='drawPaint' || log[0].params?.paint?.color!=='#00000000' ||
      log[0].params?.paint?.blendMode!=='Src') return null;
  let offset=layer.transform.slice(12,14);
  const offsets=[];
  const matches=[];
  const coordinate=(shape,spread)=>shape?.type==='Simple' && offset &&
    shape.left+offset[0]===bounds.x-spread && shape.top+offset[1]===bounds.y-spread &&
    shape.right+offset[0]===bounds.x+bounds.width+spread &&
    shape.bottom+offset[1]===bounds.y+bounds.height+spread;
  const radius=(shape,value)=>['upperLeft','upperRight','lowerRight','lowerLeft'].every(corner=>
    shape[corner+'Radius']?.xRadius===value && shape[corner+'Radius']?.yRadius===value);
  for (let index=1;index<log.length;index++) {
    const {method,params}=log[index];
    if (method==='save' || method==='saveLayer') offsets.push(offset);
    if (method==='restore') offset=offsets.pop()??null;
    if (method==='translate' && offset) offset=[offset[0]+params.dx,offset[1]+params.dy];
    if (['setMatrix','concat44','scale','setM44','concat'].includes(method)) offset=null;
    if (method!=='clipRRect' || params['SkRegion::Op']!=='kDifference_Op' ||
        params.softClipEdgeStyle!==true || !coordinate(params.rrect,-1)) continue;
    const casterRadius=params.rrect.upperLeftRadius?.xRadius+1;
    if (!(casterRadius>1) || !radius(params.rrect,casterRadius-1)) continue;
    const shadows=[];
    let next=index+1;
    for (;next+3<log.length;next+=4) {
      const [save,translate,draw,restore]=log.slice(next,next+4);
      const shape=draw.params?.rrect;
      const spread=shape && bounds.x-shape.left-offset[0];
      if (save.method!=='save' || translate.method!=='translate' || draw.method!=='drawRRect' ||
          restore.method!=='restore' || draw.params?.paint?.color!=='#FF000000' ||
          !coordinate(shape,spread) || !radius(shape,casterRadius+spread)) break;
      shadows.push({spread,offset:[translate.params.dx,translate.params.dy]});
    }
    if (log[next]?.method==='restore' && shadows.length>=2 && shadows.length<=4)
      matches.push({radius:casterRadius,shadows});
  }
  return matches.length===1 ? matches[0] : null;
}

async function observeIsolatedShadowGroup(cdp, node, compositorLayers) {
  const bounds=node.actual.bounds;
  const matrix=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
  const layers=compositorLayers.filter(layer=>layer.drawsContent && layer.transform?.length===matrix.length &&
    layer.transform.every((value,i)=>i===12 || i===13 || value===matrix[i]) &&
    layer.transform[12]<=bounds.x && layer.transform[13]<=bounds.y &&
    layer.transform[12]+layer.width>=bounds.x+bounds.width && layer.transform[13]+layer.height>=bounds.y+bounds.height);
  const matches=[];
  for (const layer of layers) {
    // A popup's compositor layer can have another backend owner. Match the
    // submitted content rect and the unique rounded knockout draw in that layer.
    const handler=layer.scrollRects?.some(({rect,type})=>type==='TouchEventHandler' &&
      rect.x+layer.transform[12]===bounds.x && rect.y+layer.transform[13]===bounds.y &&
      rect.width===bounds.width && rect.height===bounds.height);
    if (!handler) continue;
    const {snapshotId}=await cdp.send('LayerTree.makeSnapshot',{layerId:layer.layerId});
    try {
      const {commandLog}=await cdp.send('LayerTree.snapshotCommandLog',{snapshotId});
      const witness=isolatedShadowCommands(commandLog,layer,bounds);
      if (witness) matches.push({source:'Chromium LayerTree.snapshotCommandLog',layer_id:layer.layerId,
        bounds,...witness});
    } finally {
      await cdp.send('LayerTree.releaseSnapshot',{snapshotId});
    }
  }
  return matches.length===1 ? matches[0] : null;
}

async function observeOpacitySurface(cdp, node, layers, compositorLayers) {
  const bounds=node.actual.bounds;
  const owner=compositorLayers.filter(layer=>layer.drawsContent && layer.backendNodeId===node.backend_node_id &&
    layer.scrollRects?.some(({type,rect})=>type==='TouchEventHandler' && rect.x===0 && rect.y===0 &&
      rect.width===bounds.width && rect.height===bounds.height));
  if (owner.length!==1) return null;
  const descendants=new Set(layers.filter(layer=>layer.backend_node_id!==node.backend_node_id &&
    layer.ancestry.includes(node.backend_node_id) &&
    layer.composition_group_path.filter(group=>group.opacity!=='1').length===1).map(layer=>layer.backend_node_id));
  const children=compositorLayers.filter(layer=>layer.drawsContent && descendants.has(layer.backendNodeId) &&
    layer.parentLayerId===owner[0].parentLayerId && layer.width>0 && layer.height>0);
  if (children.length!==1) return null;
  const [child]=children;
  const childReasons=await cdp.send('LayerTree.compositingReasons',{layerId:child.layerId});
  if (!childReasons.compositingReasonIds?.includes('ActiveTransformAnimation')) return null;
  return {source:'Chromium LayerTree',owner:node.backend_node_id,member:child.backendNodeId,
    owner_layer:owner[0].layerId,member_layer:child.layerId,parent_layer:child.parentLayerId};
}

export async function observePaintStack(cdp, identities, {nonce,step,revision,viewport,compositorLayers=[]}) {
  const liveViewport=(await cdp.send('Runtime.evaluate',{expression:'({scroll_x:scrollX,scroll_y:scrollY,width:innerWidth,height:innerHeight,dpr:devicePixelRatio})',returnByValue:true})).result.value;
  const objectGroup = 'readonly-paint-roots';
  let roots;
  try {
    const resolved = await Promise.all([...new Set(identities)].map(async identity => {
      const remote = await cdp.send('Runtime.evaluate',{
        expression: 'window.__reference.element(' + JSON.stringify(identity) + ')', objectGroup});
      if (!remote.result.objectId) return null;
      const {node} = await cdp.send('DOM.describeNode',{objectId:remote.result.objectId});
      return {identity,backend_node_id:node.backendNodeId};
    }));
    roots = resolved.filter(Boolean);
  } finally {
    await cdp.send('Runtime.releaseObjectGroup', {objectGroup});
  }
  const snapshot=await cdp.send('DOMSnapshot.getSnapshot',{computedStyleWhitelist:paintStyleNames,
    includeUserAgentShadowTree:true,includePaintOrder:true});
  const parent=new Map();
  for(let i=0;i<snapshot.domNodes.length;i++)for(const child of snapshot.domNodes[i].childNodeIndexes??[])parent.set(child,i);
  const indexByBackend=new Map(snapshot.domNodes.map((n,i)=>[n.backendNodeId,i]));
  const rootIndices=roots.map(r=>({...r,index:indexByBackend.get(r.backend_node_id)}));
  const owners=index=>rootIndices.filter(root=>{
    for(let current=index;current!==undefined;current=parent.get(current))if(current===root.index)return true;
    return false;
  }).map(root=>root.identity);
  const ownedLayouts=[];
  const backendNodes=new Set();
  for(let ordinal=0;ordinal<snapshot.layoutTreeNodes.length;ordinal++) {
    const layout=snapshot.layoutTreeNodes[ordinal], node=snapshot.domNodes[layout.domNodeIndex];
    const owned=owners(layout.domNodeIndex);
    if(!owned.length)continue;
    ownedLayouts.push({ordinal,layout,node,owned});
    if(!node.pseudoType)backendNodes.add(node.backendNodeId);
  }
  // Read each real node once per snapshot, even when it has multiple layout
  // fragments. Keep the final layer and ancestry order from DOMSnapshot.
  const actualNodes=await readActualNodes(cdp,[...backendNodes]);
  const layers=[];
  for(const {ordinal,layout,node,owned} of ownedLayouts) {
    const style=Object.fromEntries((snapshot.computedStyles[layout.styleIndex]?.properties??[]).map(p=>[p.name,p.value]));
    const ancestry=[];
    for(let current=layout.domNodeIndex;current!==undefined;current=parent.get(current))ancestry.push(snapshot.domNodes[current].backendNodeId);
    const actual=actualNodes.get(node.backendNodeId)??{status:'unavailable',reason:node.pseudoType
      ?'pseudo node has snapshot style/geometry only':'real node is unavailable in the current snapshot'};
    layers.push({source_material_id:nonce+'/'+node.backendNodeId+'/'+(node.pseudoType??'node'),
      backend_node_id:node.backendNodeId,owner_targets:owned,ancestry,pseudo:node.pseudoType??null,
      node_name:node.nodeName,node_type:node.nodeType,node_value:node.nodeValue,
      layout_ordinal:ordinal,paint_order:layout.paintOrder??null,stacking_context:layout.isStackingContext===true,
      bounds:{...layout.boundingBox,x:layout.boundingBox.x-liveViewport.scroll_x,y:layout.boundingBox.y-liveViewport.scroll_y},
      raw_document_bounds:layout.boundingBox,layout_text:layout.layoutText??null,inline_text_nodes:layout.inlineTextNodes??[],style,actual});
  }
  // DOMSnapshot can copy an element's opacity and stacking flag to a Text row.
  // Only element boxes establish an opacity group, including generated elements.
  const groupByNode=new Map(layers.filter(l=>l.node_type===1&&l.stacking_context).map(l=>[l.backend_node_id,l]));
  for(const layer of layers)layer.composition_group_path=layer.ancestry.slice().reverse()
    .filter(id=>groupByNode.has(id)).map(id=>({backend_node_id:id,paint_order:groupByNode.get(id).paint_order,
      opacity:groupByNode.get(id).style.opacity}));
  for (const layer of layers) {
    const bounds=layer.actual.bounds;
    const opacity=Number(layer.style.opacity);
    if (layer.node_type===1 && bounds && layer.stacking_context && opacity>0 && opacity<1 &&
        bounds.y<liveViewport.height && bounds.y+bounds.height>0 &&
        layer.style.transform==='none' && layer.style['mix-blend-mode']==='normal' &&
        layer.composition_group_path.every(group=>group.backend_node_id===layer.backend_node_id || group.opacity==='1')) {
      const witness=await observeOpacitySurface(cdp,layer,layers,compositorLayers);
      if (witness) layer.opacity_surface=witness;
    }
    if (layer.node_type!==1 || !bounds || !layer.stacking_context ||
        !layer.owner_targets.some(identity=>identity.endsWith('.popup')) ||
        !layer.style['box-shadow'] || layer.style['box-shadow']==='none' ||
        bounds.y>=liveViewport.height || bounds.y+bounds.height<=0 ||
        layer.style.transform!=='none' || layer.composition_group_path.some(group=>group.opacity!=='1')) continue;
    const witness=await observeIsolatedShadowGroup(cdp,layer,compositorLayers);
    if (witness) layer.isolated_shadow_group=witness;
  }
  const focused=layers.find(layer=>layer.actual.editor?.focused);
  if(focused) {
    const caret=await observeEmptySelectCaret(cdp,focused,compositorLayers,liveViewport);
    if(caret)focused.compositor_caret=caret;
  }
  // Chromium can omit paint order for a retained placeholder hidden by its
  // computed visibility or a zero-opacity group. Keep its DOM evidence and
  // require order for each node that can contribute to the screenshot.
  const complete=layers.every(l=>Number.isInteger(l.paint_order) || l.style.display==='none' ||
    ['hidden','collapse'].includes(l.style.visibility) || l.style.opacity==='0' ||
    l.composition_group_path.some(group=>group.opacity==='0'));
  const stack={status:complete?'published':'unsupported',
    reason:complete?'':'Chromium did not expose actual paint order',
    source:'Chromium DOMSnapshot.getSnapshot actual paintOrder and UA shadow layout',
    scene_revision:revision,viewport:{...viewport,...liveViewport},roots,layers};
  stack.snapshot_digest=crypto.createHash('sha256').update(JSON.stringify(stack)).digest('hex');
  return stack;
}
