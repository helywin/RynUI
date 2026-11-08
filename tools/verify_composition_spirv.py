#!/usr/bin/env python3
"""Reflect the actual composition SPIR-V sidecar layouts, not HLSL text.

This does not establish DXIL reflection or real GPU execution.
"""
import argparse
from pathlib import Path
import shutil
import struct
import tempfile
from verify_effect_paint_spirv import Module

VERTEX=('quad_composition','glyph_composition','image_composition','rounded_effect_composition','rounded_effect_paint_composition')
FRAGMENT={
    'quad_composition':((0,2,()),(1,1,(0,))),
    'rounded_effect_composition':((0,2,()),(1,1,(0,)),(2,4,())),
    'rounded_effect_paint_composition':((0,2,()),),
    'masked_group':((0,2,()),(1,4,())),
    'backdrop_filter':((0,2,()),(1,2,(1,))),
    'backdrop_composite':((0,2,()),(1,6,())),
}
def uniform(module,binding,count,integer=(),descriptor_set=3):
    matches=[v for v,d in module.resources().items() if d.get(34)==(descriptor_set,) and d.get(33)==(binding,)]
    module.require(len(matches)==1,f'missing or duplicate uniform set{descriptor_set} binding{binding}')
    record=module.pointed(matches[0])
    opcode,fields=module.types[record]
    module.require(opcode==30 and len(fields)==count,f'uniform {binding} field count')
    for i,t in enumerate(fields):
        module.require(module.members.get((record,i),{}).get(35)==(i*16,),f'uniform {binding} member {i} offset')
        module.require(module.vector(t,i in integer),f'uniform {binding} member {i} vector type')
    return record
def validate(directory):
    for name in VERTEX:
        uniform(Module(directory/(name+'.vertex.spv')),0,2,descriptor_set=1)
    for name,blocks in FRAGMENT.items():
        module=Module(directory/(name+'.fragment.spv'))
        for binding,count,integer in blocks: uniform(module,binding,count,integer)
    module=Module(directory/'backdrop_filter.fragment.spv')
    buffers=[v for v,(_,kind) in module.variables.items() if kind in (2,12) and module.decorations.get(v,{}).get(34)==(2,)]
    module.require(len(buffers)==1,'Gaussian must have exactly one independent storage buffer')
    variable=buffers[0]
    module.require(module.decorations[variable].get(33)==(1,),'Gaussian weights follow source sampler at t1/set2 binding1')
    opcode,fields=module.types[module.pointed(variable)]
    module.require(opcode==30 and len(fields)==1,'Gaussian storage wrapper')
    opcode,element=module.types[fields[0]]
    module.require(opcode==29 and module.decorations.get(fields[0],{}).get(6)==(16,),'Gaussian weight stride16')
    module.require(module.vector(element[0]),'Gaussian weight float4')
def negative(directory,output):
    output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output,prefix='clip-layout-') as temp:
        root=Path(temp)
        for path in directory.glob('*.spv'): shutil.copyfile(path,root/path.name)
        path=root/'masked_group.fragment.spv'
        record=uniform(Module(path),1,4)
        words=list(struct.unpack('<'+'I'*(path.stat().st_size//4),path.read_bytes()))
        offset=5
        changed=False
        while offset<len(words):
            size=words[offset]>>16
            if words[offset]&0xffff==72 and words[offset+1]==record and words[offset+2]==3 and words[offset+3]==35:
                words[offset+4]=64; changed=True; break
            offset+=size
        if not changed: raise ValueError('negative fixture has no source-origin member offset')
        path.write_bytes(struct.pack('<'+'I'*len(words),*words))
        try: validate(root)
        except ValueError: return
        raise ValueError('corrupt clip source-origin offset passed reflection')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--negative-test',type=Path)
    args=parser.parse_args()
    validate(args.directory)
    if args.negative_test: negative(args.directory,args.negative_test)
    print('composition SPIR-V: viewport32/control16/clip64/filter32/weight16/replacement96; actual offsets and scalar kinds')
if __name__=='__main__': main()
