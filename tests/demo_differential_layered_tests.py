"""Synthetic CPU stratum controls; these are not GPU calibration evidence."""
from copy import deepcopy
import math
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.comparison import (_layered_context,_actual_material,css_shadows,_composite_point,
                                          _layer_coverage,_native_paint_inventory,_paint_bins,_contains,_inset,
                                          _support,_PAINT_BIN_PHYSICAL_PX,_glyph_witness_gate,
                                          _hard_shadow_outline_basis,_paint_coordinates,_palette_basis,_bounded_paint_basis,
                                          _web_paint_inventory)
from demo_differential.calibration import CAPS
from demo_differential.adapters import RunError
from demo_differential.glyph_witness import GlyphWitness,mask_identity,encoded_coverage
from demo_differential.protocol import ProtocolError


def box(x=10,y=10,width=40,height=30):
    return dict(x=x,y=y,width=width,height=height)


def synthetic():
    target=dict(identity='control',bounds=box(),text=[],icons=[],tokens={
        'backgroundColor':'rgb(255, 255, 255)','borderColor':'rgb(0, 0, 0)',
        'borderWidth':'1px','borderRadius':'0px','boxShadow':'none'})
    web={'targets':[dict(identity='sample',bounds=box(),tokens={},text=[],icons=[]),target]}
    native=deepcopy(web)
    native['targets'][1]['tokens']['paint']=dict(source='published-retained-input-fragment',status='published',branch_active=True,
        owner=dict(index=0,generation=1),layers=[dict(role=role,exists=True,hidden=False,clipped=False,
        in_fragment=True,primitive=dict(index=index,generation=1),actual=dict(paint_present=False,translation=[0,0],
        color=color,opacity=1,kind=0,offset=[0,0],blur=0,spread=0,bounds=box(),radius=0))
        for index,(role,color) in enumerate((('background',[1,1,1,1]),('border',[0,0,0,1])))])
    for side in (native,web):
        side.update(nonce='run',step='synthetic',captures=[dict(nonce='capture',scene_revision=1,sha256='capture-hash')])
        side['resources']={}
    raw=dict(run_nonce='run',nonce='capture',checkpoint='synthetic',scene_revision=1,readback_id=1,fence_id=2,renderer_owner=3,device_epoch=4)
    native['resources']['readback']=raw
    commands=[dict(ordinal=i,kind=0,first=i,count=1,atlas_page=4294967295) for i in range(2)]
    layers=[]
    for i,(bounds,color) in enumerate(((box(),[0,0,0,1]),(box(11,11,38,28),[1,1,1,1]))):
        layers.append(dict(source_material_id='quad:'+str(i),owner=dict(index=0,generation=1),
            fragment=dict(index=0,generation=1),publication_revision=1,in_fragment=True,in_submitted_scene=True,
            packed_index=i,kind=0,atlas_page=4294967295,submitted_membership=[dict(command=i,instance=0)],
            actual=dict(bounds=bounds,color=color,opacity=1,translation=[0,0],radius=0),role='fill'))
    native['resources']['paint_stack']=dict(status='published',source='actual-submitter-visible',scene_revision=1,
        capture_binding=raw,commands=commands,layers=layers,glyph_inventory=[],composition_requests=[],snapshot_digest='native')
    style={'background-color':'rgb(255, 255, 255)',**{'border-'+e+'-width':'1px' for e in ('top','right','bottom','left')},
        **{'border-'+e+'-style':'solid' for e in ('top','right','bottom','left')},
        **{'border-'+e+'-color':'rgb(0, 0, 0)' for e in ('top','right','bottom','left')}}
    web['resources']['paint_stack']=dict(status='published',scene_revision=1,snapshot_digest='web',
        capture_binding=dict(run_nonce='run',capture_nonce='capture',checkpoint='synthetic',scene_revision=1,capture_sha256='capture-hash'),
        layers=[dict(source_material_id='node:1',backend_node_id=1,node_type=1,node_name='INPUT',style=style,actual=dict(bounds=box(),clips=[]),
                     paint_order=1,layout_ordinal=0,composition_group_path=[],bounds=box())])
    case=dict(identity='antd665.input.controlled-edit.v1',native=dict(sample_id='sample'),
              fixture=dict(viewport=dict(dpr=1)))
    declaration=dict(step='synthetic',targets=['sample','control'],rois=[dict(kind='sample-context',guard_px=2)])
    border={(x,y) for x in range(10,50) for y in (10,39)}|{(x,y) for y in range(10,40) for x in (10,49)}
    def pixels(ink):
        return b''.join(bytes((0,0,0,255) if (x,y) in ink else (255,255,255,255)) for y in range(60) for x in range(80))
    images=[(pixels(border),(80,60)),(pixels(border),(80,60))]
    origins=(box(),box())
    return case,declaration,native,web,images,dict(CAPS),origins


class LayeredTests(unittest.TestCase):
    def test_only_provably_hidden_dom_nodes_can_omit_paint_order(self):
        _,_,_,web,_,_,origins=synthetic()
        node=web['resources']['paint_stack']['layers'][0]
        node['paint_order']=None
        node['style']['visibility']='hidden'
        self.assertEqual(_web_paint_inventory(web,origins[0],1)[0],[])
        node['style']['visibility']='visible'
        node['composition_group_path']=[dict(backend_node_id=5,opacity='0')]
        self.assertEqual(_web_paint_inventory(web,origins[0],1)[0],[])
        node['composition_group_path'][0]['opacity']='1'
        with self.assertRaises(RunError):
            _web_paint_inventory(web,origins[0],1)

    def test_empty_actual_web_clip_never_contributes_paint(self):
        _,_,_,web,_,_,origins=synthetic()
        layer=web['resources']['paint_stack']['layers'][0]
        layer['actual']['clips']=[dict(clip_path='none',transform='none',bounds=box(height=0))]
        self.assertEqual(_web_paint_inventory(web,origins[0],1)[0],[])

    def test_square_css_underline_owns_only_the_visible_bottom_miters(self):
        _,_,_,web,_,_,origins=synthetic()
        node=web['resources']['paint_stack']['layers'][0]
        for edge in ('top','left','right'):
            node['style']['border-'+edge+'-color']='rgba(0, 0, 0, 0)'
        paint,_=_web_paint_inventory(web,origins[0],1)
        fill,underline=paint
        self.assertEqual((fill['kind'],underline['kind']),('fill','border-side'))
        self.assertEqual(_support(underline),box(0,29,40,1))
        self.assertEqual(_layer_coverage(underline,(.5,29.5),1),(.5,True))
        self.assertEqual(_layer_coverage(underline,(39.5,29.5),1),(.5,True))
        self.assertEqual(_layer_coverage(underline,(20.5,29.5),1),(1,False))
        self.assertEqual(_layer_coverage(underline,(.5,.5),1),(0,False))
        self.assertEqual(_layer_coverage(underline,(20.5,28.5),1),(0,False))
        node['style']['border-top-left-radius']='5px'
        with self.assertRaises(RunError):
            _web_paint_inventory(web,origins[0],1)

    def test_near_parallel_hard_shadow_and_outline_keep_a_physical_output_gate(self):
        layers=[dict(kind='shadow',blur=0,groups=[],color=(5/255,145/255,1),alpha=.1,opacity=1),
                dict(kind='outline',groups=[],color=(145/255,202/255,1),alpha=1,opacity=1)]
        with self.assertRaises(RunError):
            _palette_basis(layers,(1,1,1))
        basis=_hard_shadow_outline_basis(layers,[0.,0.],[0,1])
        original=tuple(round(value*255) for value in _composite_point(layers,[1.,.2]))
        missing=tuple(round(value*255) for value in _composite_point(layers,[0.,.2]))
        visible,a=_paint_coordinates(original,basis)
        self.assertTrue(visible)
        visible,b=_paint_coordinates(missing,basis)
        self.assertTrue(visible)
        self.assertGreater(max(abs(x-y) for x,y in zip(a,b,strict=True)),CAPS['overlap.contribution_max_error'])
        self.assertFalse(_paint_coordinates((220,239,240),basis)[0])
        self.assertIsNone(_hard_shadow_outline_basis(layers[::-1],[0.,0.],[0,1]))
        fixed=_bounded_paint_basis(layers,[1.,0.],[1])
        self.assertEqual(fixed['rank'],1)
        self.assertTrue(_paint_coordinates(original,fixed)[0])

    def test_submitted_degenerate_quad_validates_membership_without_paint(self):
        _,_,native,_,_,_,origins=synthetic()
        layer=native['resources']['paint_stack']['layers'][0]
        layer['actual']['bounds']['width']=0
        layers,_=_native_paint_inventory(native,origins[1])
        self.assertEqual([part['identity'] for part in layers],['quad:1'])
        layer['submitted_membership']=[]
        with self.assertRaises(RunError):
            _native_paint_inventory(native,origins[1])

    def test_device_glyph_witness_uses_fixed_hinting_and_rejects_missing_ink(self):
        source=dict(glyph_id=44,display_scale=1.5,padding=1,bearing_x=-2,bearing_y=3,
                    coverage_width=2,coverage_height=2,phase=0,
                    font=dict(sha256='font',logical_pixel_size=14,face_index=0))
        layer=dict(bounds=box(4,6,8/3,8/3),color=(0,0,0),alpha=1,opacity=1,
                   glyph_source=(dict(content='I'),dict(atlas_source=source,byte_begin=0,byte_end=1)))
        rasterizer='directwrite-3x1'
        key=mask_identity('I',source)
        priors={'font:I':dict(glyph_id=44,units=1000),
                'scenario_masks':(rasterizer,{key:GlyphWitness(-2,-3,2,2,bytes((255,0,192,0)))})}
        gate=lambda point,pixel: _glyph_witness_gate(layer,point,1.5,priors,(1,1,1),pixel)
        self.assertTrue(gate((7.5/1.5,10.5/1.5),(0,0,0)))
        self.assertFalse(gate((7.5/1.5,10.5/1.5),(255,255,255)))
        self.assertIsNone(gate((8.5/1.5,10.5/1.5),(255,255,255)))
        level=round(255*(1-encoded_coverage(192,(0,0,0),rasterizer)))
        self.assertTrue(gate((7.5/1.5,11.5/1.5),(level,)*3))
        source['bearing_x']=0
        with self.assertRaises(ProtocolError):
            gate((7.5/1.5,10.5/1.5),(0,0,0))

    def test_fractional_dpi_keeps_a_gapped_focus_outline_separate(self):
        fill=dict(kind='fill',bounds=box(10,10,40,24),radii=4)
        focus=dict(fill,kind='outline',offset_width=1,width=3)
        # At 150% DPI the distinct shape edges are 1.5 physical px apart.
        self.assertEqual(_layer_coverage(fill,(30,9.5),1.5),(0,False))
        self.assertEqual(_layer_coverage(focus,(30,9.5),1.5),(0,False))
        self.assertEqual(_layer_coverage(fill,(30,9.75),1.5),(0,True))
        self.assertEqual(_layer_coverage(focus,(30,9.25),1.5),(0,True))

    def test_zero_shadow_cannot_paint_through_its_identical_knockout(self):
        shadow=dict(kind='shadow',bounds=box(10,10,40,24),radii=4,knockout=True,blur=0,spread=0,offset=(0,0))
        for point in ((10,22),(10.25,10.5),(30,22),(25,34.25)):
            self.assertEqual(_layer_coverage(shadow,point,1.5),(0,False))

    def test_sparse_physical_bins_preserve_guard_shadow_and_paint_order(self):
        layers=[dict(identity='fill',kind='fill',bounds=box(-34.25,0,57,51)),
                dict(identity='shadow',kind='shadow',bounds=box(5.5,9.5,40,24),offset=(1,4),spread=2,blur=6),
                dict(identity='outline',kind='outline',bounds=box(21,37,10,20),offset_width=3,width=1)]
        frame=(-60,-12,100,96)
        for dpr in (1,1.5):
            index=_paint_bins(layers,frame,dpr)
            step=_PAINT_BIN_PHYSICAL_PX
            for y in range(frame[1],frame[3]):
                for x in range(frame[0],frame[2]):
                    point=((x+.5)/dpr,(y+.5)/dpr)
                    actual=[row['identity'] for row,support in index.get((x//step,y//step),())
                            if _contains(support,point)]
                    expected=[row['identity'] for row in layers if _contains(_inset(_support(row),-1/dpr),point)]
                    self.assertEqual(actual,expected)

    def test_identical_known_white_fill_is_flat_and_visible_border_still_present(self):
        failures,measurements=_layered_context(*synthetic())
        self.assertEqual(failures,[])
        summary=next(m for m in measurements if m['category']=='paint-strata')
        self.assertEqual(sum(summary['counts'].values()),summary['classified_pixels'])
        self.assertGreater(summary['counts']['edge'],0)

    def test_input_material_cannot_fall_back_to_theme_colors(self):
        target=dict(tokens=dict(background=[1,1,1,1]))
        with self.assertRaises(RunError):
            _actual_material(target,'background',(1,1,1,1),require_input_paint=True)
        values=synthetic()
        values[2]['resources']['paint_stack']['status']='unsynchronized'
        with self.assertRaises(RunError):_layered_context(*values)
        values=synthetic()
        values[2]['resources']['paint_stack']['status']='absent'
        with self.assertRaises(RunError):_layered_context(*values)

    def test_observed_wrong_material_is_a_visual_difference_not_missing_environment(self):
        values=synthetic()
        values[2]['resources']['paint_stack']['layers'][0]['actual']['color']=[1,0,0,1]
        failures,measurements=_layered_context(*values)
        self.assertTrue(any(f['category']=='visual' for f in failures))
        self.assertGreater(next(m for m in measurements if m['category']=='paint-strata')['classified_pixels'],40*30)

    def test_preserved_control_border_cannot_supply_two_missing_glyphs(self):
        values=synthetic()
        for side in (values[2],values[3]):
            groups=[dict(byte_begin=i,byte_end=i+1,line=0,advance=4,content=char,
                         boxes=[box(20+i*8,20,3,5)],color='rgb(0, 0, 0)',opacity=1)
                    for i,char in enumerate('HI')]
            row=dict(content='HI',semantic_groups=groups,scene_identity=dict(index=1,generation=1),
                     ink_instances=[dict(byte_begin=i,byte_end=i+1,line=0,color=[0,0,0,1],opacity=1,
                         rotation_degrees=0,translation=[0,0],clip=[10,10,50,40],bounds=box(20+i*8,20,3,5))
                                    for i in range(2)])
            side['targets'][1]['text']=[row]
        native,web=values[2:4];text=native['targets'][1]['text'][0]
        for i,ink in enumerate(text['ink_instances']):
            ink.update(instance_index=2+i,atlas_page=0,atlas_uv=[0,0,1,1],
                atlas_source=dict(glyph_id=i+1,font=dict(sha256='font',logical_pixel_size=1),display_scale=1,padding=0,bearing_x=0,bearing_y=5,phase=0))
            native['resources']['paint_stack']['commands'].append(dict(ordinal=2+i,kind=1,first=2+i,count=1,atlas_page=0))
            native['resources']['paint_stack']['layers'].append(dict(source_material_id='glyph:'+str(i),owner=dict(index=1,generation=1),
                fragment=dict(index=1,generation=1),publication_revision=1,in_fragment=True,in_submitted_scene=True,
                packed_index=2+i,kind=1,atlas_page=0,submitted_membership=[dict(command=2+i,instance=0)],
                actual=dict(bounds=ink['bounds'],color=ink['color'],opacity=1,translation=[0,0],clip_bounds=ink['clip']),role='glyph'))
        native['resources']['paint_stack']['glyph_inventory']=[text]
        web['resources']['paint_stack']['layers'].append(dict(source_material_id='text:1',node_type=3,node_name='#text',style={},
            actual=dict(clips=[],text=dict(content='HI',semantic_groups=web['targets'][1]['text'][0]['semantic_groups'])),
            paint_order=1,layout_ordinal=1,composition_group_path=[],bounds=box(20,20,11,5),layout_text='HI'))
        priors={'font:'+char:dict(glyph_id=i+1,units=1,contours=[[(0,0),(3,0),(3,5),(0,5),(0,0)]]) for i,char in enumerate('HI')}
        failures,_=_layered_context(*values,glyph_priors=priors)
        missing={f['source'] for f in failures if f['message']=='independent SFNT interior glyph witness has no ink'}
        self.assertEqual(missing,{'glyph:0','glyph:1'})

    def test_nested_rgba_shadow_list_preserves_all_lengths_and_order(self):
        shadows=css_shadows('rgba(1, 2, 3, 0.2) 1px 2px 3px -1px, inset rgb(4, 5, 6) -2px 0px 0px 1px')
        self.assertEqual(len(shadows),2)
        self.assertEqual((shadows[0]['offset'],shadows[0]['blur'],shadows[0]['spread']),([1,2],3,-1))
        self.assertTrue(shadows[1]['inset'])
        self.assertEqual(shadows[1]['ordinal'],1)

    def test_opacity_group_is_composited_once_after_children(self):
        rows=[dict(groups=[('group',.5)],color=color,alpha=1,opacity=1) for color in ((1,0,0),(0,0,1))]
        self.assertEqual(_composite_point(rows,[1,1]),(.5,.5,1))

    def test_wrong_capture_or_command_membership_cannot_claim_publication(self):
        values=synthetic();values[2]['resources']['paint_stack']['capture_binding']['fence_id']=99
        values[2]['resources']['readback']=dict(values[2]['resources']['readback'],fence_id=2)
        with self.assertRaises(RunError) as caught:_layered_context(*values)
        self.assertEqual(caught.exception.code,4)
        values=synthetic();values[2]['resources']['paint_stack']['layers'][0]['submitted_membership']=[]
        with self.assertRaises(RunError) as caught:_layered_context(*values)
        self.assertEqual(caught.exception.code,4)

    def test_one_wrong_known_flat_or_guard_pixel_is_a_difference(self):
        for x,y in ((20,20),(8,8)):
            values=synthetic();pixels=bytearray(values[4][1][0]);i=(y*80+x)*4;pixels[i:i+4]=bytes((255,0,0,255))
            values[4][1]=(bytes(pixels),(80,60))
            failures,_=_layered_context(*values)
            self.assertTrue(any(f['category']=='visual' for f in failures))

    def test_actual_order_change_does_not_become_unknown_environment(self):
        values=synthetic();stack=values[2]['resources']['paint_stack'];stack['commands'].reverse()
        for i,command in enumerate(stack['commands']):command['ordinal']=i
        for layer in stack['layers']:layer['submitted_membership'][0]['command']=1-layer['packed_index']
        failures,_=_layered_context(*values)
        self.assertTrue(any(f['category']=='visual' for f in failures))

    def test_focus_outline_and_zero_blur_shadow_are_supported(self):
        for focus in (True,False):
            values=synthetic();native,web=values[2:4];stack=native['resources']['paint_stack']
            for layer in stack['layers']:layer['submitted_membership'][0]['command']+=1
            stack['commands'].insert(0,dict(ordinal=0,kind=2,first=0,count=1,atlas_page=4294967295))
            actual=dict(bounds=box(),color=[0,0,1,1 if focus else .2],opacity=1,translation=[0,0],radius=0,
                        effect_kind=2 if focus else 0,outline_width=1,outline_offset=1,offset=[0,0],blur=0,spread=2)
            stack['layers'].append(dict(source_material_id='effect:1',owner=dict(index=0,generation=1),fragment=dict(index=0,generation=1),
                publication_revision=1,in_fragment=True,in_submitted_scene=True,packed_index=0,kind=2,atlas_page=4294967295,
                submitted_membership=[dict(command=0,instance=0)],primitive=dict(index=0,generation=1),actual=actual,role='focus' if focus else 'shadow'))
            style=web['resources']['paint_stack']['layers'][0]['style']
            if focus:style.update({'outline-color':'rgb(0, 0, 255)','outline-style':'solid','outline-width':'1px','outline-offset':'1px'})
            else:style['box-shadow']='rgba(0, 0, 255, 0.2) 0px 0px 0px 2px'
            for side in range(2):
                pixels=bytearray(values[4][side][0])
                for y in range(60):
                    for x in range(80):
                        ring=8<=x<52 and 8<=y<42 and not ((9<=x<51 and 9<=y<41) if focus else (10<=x<50 and 10<=y<40))
                        if ring:pixels[(y*80+x)*4:(y*80+x+1)*4]=bytes((0,0,255,255) if focus else (204,204,255,255))
                values[4][side]=(bytes(pixels),(80,60))
            failures,_=_layered_context(*values)
            self.assertEqual(failures,[])

    def test_uncaptured_shadow_support_is_incomplete_without_clamping(self):
        values=synthetic();values[3]['resources']['paint_stack']['layers'][0]['style']['box-shadow']='rgb(0, 0, 0) 0px 0px 80px 0px'
        with self.assertRaises(RunError) as caught:_layered_context(*values)
        self.assertEqual(caught.exception.code,4)

    def test_soft_shadow_uses_independent_rect_gaussian_and_tail_metrics(self):
        values=synthetic();native,web=values[2:4];stack=native['resources']['paint_stack']
        for layer in stack['layers']:layer['submitted_membership'][0]['command']+=1
        stack['commands'].insert(0,dict(ordinal=0,kind=2,first=0,count=1,atlas_page=4294967295))
        actual=dict(bounds=box(),color=[0,0,0,.2],opacity=1,translation=[0,0],radius=0,
                    effect_kind=6,outline_width=0,outline_offset=0,offset=[0,0],blur=2,spread=0)
        stack['layers'].append(dict(source_material_id='effect:1',owner=dict(index=0,generation=1),fragment=dict(index=0,generation=1),
            publication_revision=1,in_fragment=True,in_submitted_scene=True,packed_index=0,kind=2,atlas_page=4294967295,
            submitted_membership=[dict(command=0,instance=0)],primitive=dict(index=0,generation=1),actual=actual,role='shadow'))
        web['resources']['paint_stack']['layers'][0]['style']['box-shadow']='rgba(0, 0, 0, 0.2) 0px 0px 2px 0px'
        values[5].update({'shadow.coverage_mass_relative_error':.05,'shadow.projection_relative_error':.05,
                          'shadow.profile_max_error':.02,'shadow.tail_max_error':2/255,'overlap.contribution_max_error':.05})
        for side in range(2):
            pixels=bytearray(values[4][side][0])
            for y in range(60):
                for x in range(80):
                    if 10<=x<50 and 10<=y<40:continue
                    px,py=x+.5,y+.5;root2=math.sqrt(2)
                    # Closed-form separable Gaussian for a rectangle; no call
                    # to the consumer's midpoint-rounded-box integration.
                    alpha=.2/4*(math.erf((50-px)/root2)-math.erf((10-px)/root2))*(math.erf((40-py)/root2)-math.erf((10-py)/root2))
                    byte=round(255*(1-alpha));pixels[(y*80+x)*4:(y*80+x+1)*4]=bytes((byte,byte,byte,255))
            values[4][side]=(bytes(pixels),(80,60))
        failures,measured=_layered_context(*values)
        self.assertEqual(failures,[])
        self.assertTrue(any(row['category']=='shadow' for row in measured))
        shifted_origin=box(0,0)
        rows,_=_native_paint_inventory(native,shifted_origin)
        shadow=next(row for row in rows if row['kind']=='shadow')
        self.assertTrue(shadow['knockout'])
        self.assertEqual(_layer_coverage(shadow,(12,20),1),(0,False))
        self.assertEqual(_layer_coverage(shadow,(10.5,20),1),(0,False))
        actual['effect_kind']=0
        rows,_=_native_paint_inventory(native,shifted_origin)
        legacy=next(row for row in rows if row['kind']=='shadow')
        self.assertFalse(legacy['knockout'])
        self.assertGreater(_layer_coverage(legacy,(12,20),1)[0],.9)


if __name__=='__main__':unittest.main()
