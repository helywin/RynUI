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
    result.text={content:node.textContent,boxes:[...range.getClientRects()].map(rect),
      semantic_groups:window.__observedTextGroups(node)};
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

export async function observePaintStack(cdp, identities, {nonce,step,revision,viewport}) {
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
  const groupByNode=new Map(layers.filter(l=>l.stacking_context).map(l=>[l.backend_node_id,l]));
  for(const layer of layers)layer.composition_group_path=layer.ancestry.slice().reverse()
    .filter(id=>groupByNode.has(id)).map(id=>({backend_node_id:id,paint_order:groupByNode.get(id).paint_order,
      opacity:groupByNode.get(id).style.opacity}));
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
