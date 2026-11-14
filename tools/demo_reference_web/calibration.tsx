// Independent controls. No Button/Input/Select pilot raster is rendered or trained.
import React from 'react';
import { createRoot } from 'react-dom/client';
import { flushSync } from 'react-dom';
import * as Antd from 'antd';
import * as Icons from '@ant-design/icons';
import zhCN from 'antd/locale/zh_CN';
import enUS from 'antd/locale/en_US';
import ButtonBasic from 'official-demo:components/button/demo/basic.tsx';
import InputClear from 'official-demo:components/input/demo/allowClear.tsx';
import InputAffixes from 'official-demo:components/input/demo/presuffix.tsx';
import SelectBasic from 'official-demo:components/select/demo/basic.tsx';
import SelectSearch from 'official-demo:components/select/demo/search.tsx';

const w=window as any;
const input=w.__controlRequest;
if(!Array.isArray(input.cells)||input.cells.length>384)throw new Error('control cell count exceeds 384');
// Full official import closure is retained and audited, without executing pilots.
w.__controlSourceImports={ButtonBasic,InputClear,InputAffixes,SelectBasic,SelectSearch,enUS};
const color=(rgb:number[],alpha=1) => `rgba(${rgb.join(',')},${alpha})`;
function Cell({cell}:any) {
  const {rect:r,kind}=cell;
  const rgb=cell.rgb ?? cell.foreground;
  const alpha=cell.source_alpha ?? 1;
  const style:any={position:'absolute',left:r.x,top:r.y,width:r.width,height:r.height,boxSizing:'border-box'};
  let child:any=null;
  if (['fill','alpha','sourceover','round','clip'].includes(kind)) {
    Object.assign(style,{background:color(rgb,alpha),borderRadius:cell.radius ?? 0});
    if(kind==='alpha')style.opacity=cell.paint_opacity ?? 1; // A single painted leaf, never a fabricated native group.
    if(kind==='clip')style.clipPath=`inset(0px ${r.width-cell.clip_width}px 0px 0px)`;
    if (kind==='sourceover') child=<div style={{position:'absolute',inset:0,background:color(cell.second_rgb,cell.second_alpha)}} />;
  } else if (kind==='border') {
    Object.assign(style,{border:`1px solid ${color(rgb)}`,borderRadius:cell.radius});
    if (cell.omit_right_edge) style.borderRightColor='transparent';
  } else if (kind==='shadow') {
    const s=cell.shadow;
    Object.assign(style,{borderRadius:cell.radius,boxShadow:`${s.offset[0]}px ${s.offset[1]}px ${s.blur}px ${s.spread}px ${color(s.rgb,s.alpha)}`,
      background:color(cell.occluding_fill.rgb,cell.occluding_fill.alpha)});
  } else if (kind==='text' || kind==='icon') {
    const family=cell.replace_font ? 'ReferenceFont0' : input.font_files
      .map((font:any) => font.weight===cell.font_weight && font.identity!=='noto-emoji' ?
        'ReferenceFont'+input.font_files.findIndex((other:any)=>other.identity===font.identity) : null).filter(Boolean).join(',');
    const glyphStyle:any={fontFamily:family,fontSize:cell.font_size,fontWeight:cell.font_weight,
      lineHeight:kind==='text'?'32px':`${cell.font_size}px`,color:color(cell.foreground),
      display:'inline-block',whiteSpace:'pre',fontSynthesis:'none',verticalAlign:'top',
      WebkitTextFillColor:cell.omit_draw?'transparent':undefined};
    if (kind==='text') child=<Antd.Typography.Text style={glyphStyle}>{cell.content}</Antd.Typography.Text>;
    else {
      const Icon=(Icons as any)[cell.render_icon ?? cell.content];
      if (!Icon) throw new Error('unknown formal control icon');
      child=<Icon spin={false} style={{...glyphStyle,opacity:cell.omit_draw?0:1}} />;
    }
    if (cell.clip_glyph_right_px) Object.assign(style,{overflow:'hidden'});
  } else throw new Error('unknown independent material');
  return <>{kind==='text' && <div data-control-frame={cell.identity} style={{position:'absolute',left:r.x-4,top:r.y-4,
    width:r.width+8,height:r.height+8,boxSizing:'border-box',border:'1px solid '+color(cell.background[0]===0?[255,255,255]:[0,0,0])}} />}
    <div data-control={cell.identity} style={style}>{child}</div></>;
}
flushSync(() => createRoot(document.getElementById('root')!).render(
  <Antd.ConfigProvider direction="ltr" locale={zhCN} theme={{token:{motion:false}}}>
    {input.panels.map((p:any,i:number)=><div key={'p'+i} style={{position:'absolute',left:p.x,top:p.y,width:p.width,height:p.height,background:color(p.rgb)}} />)}
    {input.cells.map((cell:any)=><Cell key={cell.identity} cell={cell} />)}
  </Antd.ConfigProvider>));
w.__controlReady=true;
