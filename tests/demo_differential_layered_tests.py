"""Synthetic CPU stratum controls; these are not GPU calibration evidence."""
from copy import deepcopy
import math
import struct
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.comparison import (_layered_context,_actual_material,css_shadows,_composite_point,
                                          _layer_coverage,_native_paint_inventory,_paint_bins,_contains,_inset,
                                          _support,_PAINT_BIN_PHYSICAL_PX,_glyph_witness_gate,
                                          _joint_paint_output_basis,_paint_coordinates,_palette_basis,_bounded_paint_basis,
                                          _web_paint_inventory,_hard_shadow_box_domain,_paint_endpoints,_status_input_caret,
                                          _web_glyph_support,_witnessed_glyph_rounding,_shadow_tail,_text_advance_error,
                                          _matches_soft_shadow_rgb)
from demo_differential.calibration import CAPS
from demo_differential.adapters import RunError
from demo_differential.color_metrics import in_contribution_domain
from demo_differential.glyph_witness import GlyphWitness,mask_identity,encoded_coverage,browser_placements
from demo_differential.protocol import ProtocolError,_plain_json


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
    def test_repeated_source_over_retains_coverage_order_and_group_opacity(self):
        red=dict(color=(1.,0.,0.),alpha=1.,opacity=1.,groups=[('group',.5)])
        blue=dict(red,color=(0.,0.,1.))
        layers=[red,blue]
        coverages=[1.,.5]
        self.assertEqual(_composite_point(layers,coverages),(.75,.5,.75))
        self.assertEqual(_composite_point(layers,[1.,1.]),(.5,.5,1.))
        self.assertEqual(_composite_point(list(reversed(layers)),coverages),(.75,.5,.75))
        self.assertEqual(_composite_point(list(reversed(layers)),[1.,1.]),(1.,.5,.5))
        self.assertEqual(_composite_point([dict(row,groups=[('group',.25)]) for row in layers],
                                         coverages),(.875,.75,.875))
        self.assertEqual(_composite_point(layers,coverages),(.75,.5,.75))

    def test_soft_shadow_reference_allows_only_the_open_hard_edge(self):
        hard=dict(kind='shadow',blur=0,alpha=.1,opacity=1,groups=[],color=(5/255,145/255,1))
        soft=dict(kind='shadow',blur=16,alpha=.2,opacity=1,groups=[],color=(0,0,0))
        layers=[hard,soft]
        coverages=[0.,.12]
        predicted=_composite_point(layers,coverages)

        def pixel(hard_coverage, soft_coverage):
            return tuple(round(channel*255) for channel in
                         _composite_point(layers,[hard_coverage,soft_coverage]))

        self.assertTrue(_matches_soft_shadow_rgb(layers,coverages,[0,1],pixel(.4,.12),predicted))
        self.assertFalse(_matches_soft_shadow_rgb(layers,coverages,[0,1],pixel(.4,0),predicted))
        self.assertFalse(_matches_soft_shadow_rgb(layers,coverages,[1],pixel(.4,.12),predicted))

    def test_chromium_range_floor_exposes_only_admitted_horizontal_glyph_phases(self):
        directwrite='directwrite-3x1'
        self.assertEqual(list(browser_placements(directwrite,(121.078125,50),1.5)),
                         [(181,75,2),(181,75,3)])
        self.assertEqual(list(browser_placements(directwrite,(.125-1/64,0),1)),[(0,0,0)])
        self.assertEqual(list(browser_placements('fontations-skia-a8',(.5,19),1)),[(1,19,0)])
        first=GlyphWitness(0,0,2,1,bytes((255,0)))
        second=GlyphWitness(0,0,2,1,bytes((255,128)))
        glyph=dict(kind='glyph',bounds=box(0,0,2,1),web_glyph_mask=(first,0,0),
                   web_glyph_alternates=((second,0,0),))
        self.assertEqual(_layer_coverage(glyph,(1.5,.5),1),(1,True))
        self.assertEqual(_layer_coverage(glyph,(2.5,.5),1),(0,False))

    def test_color_coverage_uses_the_observed_canonical_a8_luminance(self):
        color=tuple(channel/255 for channel in (250,173,20))
        self.assertEqual(encoded_coverage(128,color,'directwrite-3x1'),164/255)
        self.assertEqual(encoded_coverage(128,color,'fontations-skia-a8'),136/255)
        self.assertEqual(encoded_coverage(128,(0.,0.,0.),'directwrite-3x1'),119/255)
        self.assertEqual(encoded_coverage(128,(1.,1.,1.),'fontations-skia-a8'),143/255)
        # Frozen Chromium grayscale samples expose the fixed-point gamma step.
        self.assertEqual(encoded_coverage(131,(0.,0.,0.),'fontations-skia-a8'),128/255)
        self.assertEqual(encoded_coverage(74,(1.,0.,0.),'fontations-skia-a8'),76/255)

    def test_packed_solid_fill_uses_same_byte_grid_as_exact_css_color(self):
        white=dict(kind='fill',color=(1.,)*3,alpha=1,opacity=1,groups=[])
        def basis(code):
            native=struct.unpack('<f',struct.pack('<f',code/255))[0]
            gray=dict(white,color=(native,)*3)
            return _bounded_paint_basis([white,gray],[1,0],[1])
        self.assertEqual(basis(245)['quantization_bounds'],(.05,))
        self.assertTrue(_paint_coordinates((250,250,250),basis(245))[0])
        self.assertFalse(_paint_coordinates((250,246,251),basis(245))[0])
        with self.assertRaises(RunError):
            basis(246)

    def test_dom_range_advance_charges_only_bounded_endpoint_quantization(self):
        web=dict(content='n',advance=5.,boxes=[box(10+1/64,0,5,22)])
        self.assertEqual(_text_advance_error(web,{'advance':5.02},layered=True),0.)
        self.assertAlmostEqual(_text_advance_error(web,{'advance':5.05},layered=True),.05-2/64)
        self.assertAlmostEqual(_text_advance_error(web,{'advance':5.51},layered=True),.51)
        self.assertAlmostEqual(_text_advance_error(web,{'advance':5.02},layered=False),.02)
        self.assertAlmostEqual(_text_advance_error(dict(web,boxes=[box(10.01,0,5,22)]),
                                                  {'advance':5.02},layered=True),.02)

    def test_opaque_css_border_uses_a_bounded_two_step_rgb8_domain(self):
        source=object()
        common=dict(source=source,alpha=1.,opacity=1.,groups=[])
        fill=dict(common,kind='fill',color=(1.,1.,1.))
        border=dict(common,kind='border',color=tuple(channel/255 for channel in (250,173,20)))
        observed=(252,226,173)
        basis=_bounded_paint_basis([fill,border],[0.,0.],[1])
        self.assertTrue(_paint_coordinates(observed,basis)[0])
        self.assertFalse(_paint_coordinates((250,226,173),basis)[0])
        wrong_owner=_bounded_paint_basis([fill,dict(border,source=object())],[0.,0.],[1])
        self.assertFalse(_paint_coordinates(observed,wrong_owner)[0])

    def test_shadow_tail_starts_after_spread_and_offset(self):
        row=dict(bounds=box(0,132,260,32),kind='shadow',offset=[0,0],spread=2,blur=0,radii=6)
        self.assertFalse(_shadow_tail(row,(261,133+2/3),1.5))
        self.assertTrue(_shadow_tail(row,(263,148),1.5))
        self.assertFalse(_shadow_tail(dict(row,offset=[3,0]),(263,148),1.5))
        self.assertFalse(_shadow_tail(dict(row,blur=2),(263,148),1.5))
        inset=dict(row,kind='inset-shadow',offset=[3,0],blur=2)
        self.assertTrue(_shadow_tail(inset,(130,148),1.5))
        self.assertFalse(_shadow_tail(inset,(5.25,148),1.5))

    def test_glyph_rounding_requires_identical_witnesses_and_exposed_ink(self):
        source=dict(font=dict(sha256='font',face_index=0,logical_pixel_size=14),display_scale=1,
                    phase=0,padding=0,bearing_x=0,bearing_y=0,coverage_width=1,coverage_height=1)
        masks={mask_identity(scalar,source):GlyphWitness(0,0,1,1,bytes([byte]))
               for scalar,byte in [('A',5),('B',4)]}
        priors={'scenario_masks':('directwrite-3x1',masks)}
        ink=dict(color=(0,0,0),kind='glyph',groups=[],alpha=.88,opacity=1)
        web=[dict(ink,web_glyph_mask=(masks[mask_identity(scalar,source)],0,0),
                  outline_source=dict(actual_font_sha256='font',content=scalar)) for scalar in 'AB']
        native=[dict(ink,bounds=box(0,0,1,1),glyph_source=(dict(content=scalar),
                     dict(atlas_source=source,byte_begin=0,byte_end=1))) for scalar in 'AB']
        sides=[dict(variable=[0,1],active=rows,coverages=[1,1]) for rows in (web,native)]
        basis={'mode':'visible-contribution','rank':1}
        rgb=((247,)*3,(249,)*3)
        self.assertEqual(_witnessed_glyph_rounding(sides,basis,rgb,priors,(.5,.5),1),(248,)*3)
        self.assertIsNone(_witnessed_glyph_rounding(sides,dict(basis,rank=2),rgb,priors,(.5,.5),1))
        self.assertIsNone(_witnessed_glyph_rounding(sides,basis,(rgb[0],(252,)*3),priors,(.5,.5),1))
        self.assertIsNone(_witnessed_glyph_rounding(sides,basis,((255,)*3,(255,)*3),priors,(.5,.5),1))
        native[1]=dict(native[1],bounds=box(1,0,1,1))
        self.assertIsNone(_witnessed_glyph_rounding(sides,basis,rgb,priors,(.5,.5),1))

        masks[mask_identity('A',source)]=GlyphWitness(0,0,1,1,bytes([255]))
        web[0]['web_glyph_mask']=(masks[mask_identity('A',source)],0,0)
        sides=[dict(variable=[0],active=rows[:1],coverages=[1]) for rows in (web,native)]
        core=((30,)*3,(31,)*3)
        self.assertEqual(_witnessed_glyph_rounding(sides,basis,core,priors,(.5,.5),1),(31,)*3)
        self.assertIsNone(_witnessed_glyph_rounding(sides,basis,(core[0],(34,)*3),priors,(.5,.5),1))

        masks[mask_identity('A',source)]=GlyphWitness(0,0,1,1,bytes([252]))
        web[0]['web_glyph_mask']=(masks[mask_identity('A',source)],0,0)
        color=tuple(channel/255 for channel in (250,173,20))
        web[0].update(color=color,alpha=1.,groups=[('identity',1.)])
        native[0].update(color=struct.unpack('<3f',struct.pack('<3f',*color)),alpha=1.)
        tint=((250,173,21),(250,174,22))
        self.assertEqual(_witnessed_glyph_rounding(sides,basis,tint,priors,(.5,.5),1),(250,174,22))
        native[0]['color']=(250/255,172/255,20/255)
        self.assertIsNone(_witnessed_glyph_rounding(sides,basis,tint,priors,(.5,.5),1))

    def test_web_outline_admits_a_subpixel_stroke_and_rejects_a_disconnected_halo(self):
        row=dict(content='I',outline_origin=(0,1),text_group=dict(font_size='1px',font_weight='400'),
                 source={'style':{'font-family':'ReferenceFont0'}})
        priors={'web_fonts':{('ReferenceFont0',400):'font'},'font:I':dict(units=1,bounds=[1,0,1.3,1],
                  contours=[[(1,0),(1.3,0),(1.3,1),(1,1),(1,0)]])}
        common=([.004]*4,(4,1),(0,0))
        self.assertTrue(_web_glyph_support(row,priors,1,[0,.3,0,0],*common))
        self.assertFalse(_web_glyph_support(row,priors,1,[0,.3,0,.05],*common))
        row['text_group']['font_weight']='600'
        with self.assertRaises(RunError):
            _web_glyph_support(row,priors,1,[0,.3,0,0],*common)

    def test_status_caret_requires_the_focused_owner_source_and_location(self):
        case=dict(features=['native-status-caret'],fixture={'props':{'controls':{
            'editor':dict(component='Input',status='warning')}}})
        target=dict(state=dict(focused=True),tokens=dict(editor_clip=box(12,14,36,22)))
        targets=({'editor':target},{'editor':target})
        source=dict(owner_targets=['editor'],style={'color':'rgba(0,0,0,.88)'})
        web=dict(kind='caret',source=source,groups=[],color=(0,0,0),alpha=.88,opacity=1,
                 caret_edge=box(18,15,0,20))
        border=dict(kind='border',source=source,color=(1,.7,.1),alpha=1,opacity=1)
        native=dict(kind='caret',source=source,groups=[],color=border['color'],alpha=1,opacity=1,
                    bounds=box(18,14,1,22))
        self.assertEqual(_status_input_caret(case,targets,([web,border],[native]),(box(0,0),box(0,0)),1),
                         (web,native))
        bad_cases=([],[dict(native,color=(0,0,0))],[dict(native,bounds=box(20,14,1,22))],
                   [dict(native,bounds=box(18,14,2,22))],[dict(native,source={'owner_targets':['other']})])
        for bad in bad_cases:
            with self.assertRaises(ProtocolError):
                _status_input_caret(case,targets,([web,border],bad),(box(0,0),box(0,0)),1)

    def test_status_caret_pixels_have_independent_stroke_and_guard_checks(self):
        args=synthetic();case,_,native,web,images,_,_=args
        case['features']=['native-status-caret']
        case['fixture']['props']={'controls':{'control':dict(component='Input',status='warning')}}
        for view in (native,web):
            view['targets'][1]['state']={'focused':True}
        native['targets'][1]['tokens']['editor_clip']=box(14,14,30,10)
        ws=web['resources']['paint_stack']['layers']
        ws[0]['owner_targets']=['control']
        edge=box(20,14,0,10)
        ws.append(dict(source_material_id='ua-text',node_type=3,node_name='#text',layout_text='x',
                       bounds=box(19,14,10,10),owner_targets=['control'],composition_group_path=[],
                       style={'color':'rgba(0,0,0,.88)'},paint_order=2,layout_ordinal=1,
                       actual=dict(clips=[],text=dict(content='x',semantic_groups=[],
                                   caret=dict(edge=edge,color='rgba(0,0,0,.88)',animation='manual')))))
        ns=native['resources']['paint_stack']
        ns['commands'].append(dict(ordinal=2,kind=0,first=2,count=1,atlas_page=4294967295))
        ns['layers'].append(dict(ns['layers'][0],source_material_id='native-caret',owner_targets=['control'],
                                 packed_index=2,submitted_membership=[dict(command=2,instance=0)],role='caret',
                                 actual=dict(bounds=box(20,14,1,10),color=[0,0,0,1],opacity=1,
                                             translation=[0,0],radius=0)))
        def pixel(image,x,y,value):
            data,width=bytearray(image[0]),image[1][0]
            offset=(y*width+x)*4
            data[offset:offset+4]=bytes((value,value,value,255))
            return bytes(data),image[1]
        for y in range(14,24):
            images[0]=pixel(images[0],20,y,31)
            images[1]=pixel(images[1],20,y,0)
        self.assertEqual(_layered_context(*args)[0],[])
        images[0]=pixel(images[0],20,18,255)
        self.assertTrue(any(f['message']=='browser caret has a missing exposed physical stroke row'
                            for f in _layered_context(*args)[0]))
        images[0]=pixel(images[0],20,18,31)
        images[1]=pixel(images[1],23,18,0)
        self.assertTrue(any(f['message']=='independent flat/material/guard color differs'
                            for f in _layered_context(*args)[0]))

    def test_hard_shadow_box_bounds_rounding_in_physical_rgb8(self):
        layers=[dict(kind='shadow',blur=0,groups=[],color=(1,215/255,5/255),alpha=.1,opacity=1),
                dict(kind='fill',groups=[],color=(1,1,1),alpha=1,opacity=1),
                dict(kind='border',groups=[],color=(250/255,173/255,20/255),alpha=1,opacity=1)]
        source={}
        for row in layers:
            row['source']=source
            row['groups']=[('framed-input',1.)]
        geometry=tuple(_paint_endpoints(layers,[1.,0.,0.],[0,1,2]))
        domain=_hard_shadow_box_domain(layers,geometry)
        rgb=tuple(c/255 for c in (255,249,222))
        self.assertFalse(in_contribution_domain(rgb,domain,(.5/255,)*3))
        self.assertTrue(in_contribution_domain(rgb,domain,(1/255,)*3))
        self.assertFalse(in_contribution_domain((1,249/255,210/255),domain,(1/255,)*3))
        self.assertIsNone(_hard_shadow_box_domain(layers[::-1],geometry))
        self.assertIsNone(_hard_shadow_box_domain([dict(layers[0],blur=2),*layers[1:]],geometry))
        self.assertIsNone(_hard_shadow_box_domain([dict(layers[0],source={}),*layers[1:]],geometry))
        self.assertIsNone(_hard_shadow_box_domain([dict(layers[0],groups=[('a',.5)]),*layers[1:]],geometry))

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

    def test_formal_svg_subpixel_path_can_paint_outside_its_center_bounds(self):
        svg=dict(bounds=box(4.6,6.2,3,3),kind='glyph',formal_svg=True)
        self.assertEqual(_layer_coverage(svg,(4.5,6.5),1.5),(0,True))
        self.assertEqual(_layer_coverage(svg,(4.1,6.5),1.5),(0,False))
        self.assertEqual(_layer_coverage(svg,(4.5,5.7),1.5),(0,False))
        self.assertEqual(_layer_coverage(svg,(6.5,8.5),1.5),(1,True))
        clipped=dict(svg,clips=[box(4.6,6,3,4)])
        self.assertEqual(_layer_coverage(clipped,(4.5,6.5),1.5),(0,False))

    def test_rgb8_paint_writes_allow_fixed_rounding_without_accepting_wrong_material(self):
        group=[('body',.65)]
        body=dict(kind='fill',color=(22/255,119/255,1.),alpha=1,opacity=1,groups=group)
        self.assertTrue(_paint_coordinates((103,166,255),_bounded_paint_basis([body],[1.],[0]))[0])
        self.assertFalse(_paint_coordinates((103,162,241),_bounded_paint_basis([body],[1.],[0]))[0])
        cast=dict(kind='shadow',blur=0,color=(5/255,145/255,1.),alpha=.1,opacity=1,groups=group)
        self.assertTrue(_paint_coordinates((103,166,255),_bounded_paint_basis([cast,body],[1.,1.],[1]))[0])
        ink=dict(kind='glyph',color=(1.,1.,1.),alpha=1,opacity=1,groups=group)
        self.assertTrue(_paint_coordinates((103,166,255),_bounded_paint_basis([body,ink],[1.,0.],[1]))[0])
        self.assertFalse(_paint_coordinates((95,165,255),_bounded_paint_basis([body,ink],[1.,0.],[1]))[0])
        disabled=[dict(body,color=(0.,0.,0.),alpha=.04,groups=[]),
                  dict(ink,color=(0.,0.,0.),alpha=.25,groups=[])]
        basis=_bounded_paint_basis(disabled,[1.,1.],[1])
        self.assertTrue(_paint_coordinates((183,183,183),basis)[0])
        self.assertTrue(_paint_coordinates((184,184,184),basis)[0])
        self.assertFalse(_paint_coordinates((181,181,181),basis)[0])

    def test_near_parallel_hard_shadow_and_outline_keep_a_physical_output_gate(self):
        layers=[dict(kind='shadow',blur=0,groups=[],color=(5/255,145/255,1),alpha=.1,opacity=1),
                dict(kind='outline',groups=[],color=(145/255,202/255,1),alpha=1,opacity=1)]
        with self.assertRaises(RunError):
            _palette_basis(layers,(1,1,1))
        basis=_joint_paint_output_basis(layers,[0.,0.],[0,1])
        original=tuple(round(value*255) for value in _composite_point(layers,[1.,.2]))
        missing=tuple(round(value*255) for value in _composite_point(layers,[0.,.2]))
        visible,a=_paint_coordinates(original,basis)
        self.assertTrue(visible)
        visible,b=_paint_coordinates(missing,basis)
        self.assertTrue(visible)
        self.assertGreater(max(abs(x-y) for x,y in zip(a,b,strict=True)),CAPS['overlap.contribution_max_error'])
        self.assertFalse(_paint_coordinates((220,239,240),basis)[0])
        self.assertIsNone(_joint_paint_output_basis(layers[::-1],[0.,0.],[0,1]))
        fixed=_bounded_paint_basis(layers,[1.,0.],[1])
        self.assertEqual(fixed['rank'],1)
        self.assertTrue(_paint_coordinates(original,fixed)[0])

    def test_shared_opacity_group_preserves_joint_color_and_material_gates(self):
        bounds=dict(x=4,y=7,width=20,height=16)
        group=[('paint:0',.65)]
        layers=[dict(kind='shadow',blur=0,groups=group,bounds=bounds,
                     color=(5/255,145/255,1),alpha=.1,opacity=1),
                dict(kind='fill',groups=group,bounds=bounds,
                     color=(64/255,150/255,1),alpha=1,opacity=1)]
        variable=[0,1]
        coverage=[1.,1.]
        basis=_bounded_paint_basis(layers,coverage,variable)
        self.assertEqual(basis['mode'],'joint-paint-output')
        self.assertTrue(_paint_coordinates((131,186,255),basis)[0])
        self.assertTrue(_paint_coordinates((157,202,255),basis)[0])
        self.assertFalse(_paint_coordinates((131,186,245),basis)[0])
        expected=tuple(round(channel*255) for channel in _composite_point(layers,[1.,.2]))
        no_shadow=tuple(round(channel*255) for channel in _composite_point(layers,[0.,.2]))
        a=_paint_coordinates(expected,basis)[1]
        b=_paint_coordinates(no_shadow,basis)[1]
        self.assertGreater(max(abs(x-y) for x,y in zip(a,b,strict=True)),
                           CAPS['overlap.contribution_max_error'])
        self.assertIsNone(_joint_paint_output_basis(layers[::-1],coverage,variable))
        self.assertIsNone(_joint_paint_output_basis(
            [layers[0],dict(layers[1],groups=[('paint:1',.65)])],coverage,variable))

    def test_opaque_focus_outline_overlapped_by_floating_shadow_keeps_joint_gate(self):
        layers=[dict(kind='shadow',blur=0,groups=[],color=(145/255,202/255,1),alpha=1,opacity=1),
                dict(kind='fill',groups=[],color=(1,1,1),alpha=1,opacity=1),
                dict(kind='outline',groups=[],color=(22/255,119/255,1),alpha=1,opacity=1),
                dict(kind='shadow',blur=12,groups=[],color=(0,0,0),alpha=.05,opacity=1)]
        variable=[0,1,2,3]
        with self.assertRaises(RunError):
            _palette_basis(layers,(1,1,1))
        coverage=[1.,0.,.25,.17]
        basis=_bounded_paint_basis(layers,coverage,variable)
        self.assertEqual(basis['mode'],'joint-paint-output')
        raw=lambda values:tuple(round(x*255) for x in _composite_point(layers,values))
        accepted,output=_paint_coordinates(raw(coverage),basis)
        self.assertTrue(accepted)
        accepted,missing=_paint_coordinates(raw([0.,0.,0.,.17]),basis)
        self.assertTrue(accepted)
        self.assertGreater(max(abs(x-y) for x,y in zip(output,missing,strict=True)),
                           CAPS['overlap.contribution_max_error'])
        self.assertFalse(_paint_coordinates((137,185,205),basis)[0])
        self.assertIsNone(_joint_paint_output_basis(
            [dict(row,color=(254/255,254/255,254/255)) for row in layers],coverage,variable))
        layers.append(dict(layers[-1],blur=20,alpha=.08))
        basis=_bounded_paint_basis(layers,coverage+[0.001],variable+[4])
        self.assertEqual(basis['mode'],'joint-paint-output')
        self.assertTrue(_paint_coordinates(raw(coverage+[0.001]),basis)[0])

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
        layer=dict(kind='glyph',bounds=box(4,6,8/3,8/3),color=(0,0,0),alpha=1,opacity=1,
                   glyph_source=(dict(content='I'),dict(atlas_source=source,byte_begin=0,byte_end=1)))
        rasterizer='directwrite-3x1'
        key=mask_identity('I',source)
        priors={'font:I':dict(glyph_id=44,units=1000),
                'scenario_masks':(rasterizer,{key:GlyphWitness(-2,-3,2,2,bytes((255,0,192,0)))})}
        gate=lambda point,pixel: _glyph_witness_gate(layer,point,1.5,priors,(1,1,1),pixel)
        self.assertTrue(gate((7.5/1.5,10.5/1.5),(0,0,0)))
        self.assertFalse(gate((7.5/1.5,10.5/1.5),(255,255,255)))
        from demo_differential.glyph_witness import scenario_origin
        mask=priors['scenario_masks'][1][key]
        layer['native_glyph_mask']=(mask,*scenario_origin(mask,source,layer['bounds'],1.5))
        self.assertEqual(_layer_coverage(layer,(7.5/1.5,10.5/1.5),1.5),(1,True))
        self.assertEqual(_layer_coverage(layer,(8.5/1.5,10.5/1.5),1.5),(0,False))
        disabled=dict(layer,alpha=.25)
        priors['scenario_masks'][1][key]=GlyphWitness(-2,-3,2,2,bytes((60,0,192,0)))
        self.assertTrue(_glyph_witness_gate(disabled,(5,7),1.5,priors,(245/255,)*3,(233,)*3))
        self.assertFalse(_glyph_witness_gate(disabled,(5,7),1.5,priors,(245/255,)*3,(245,)*3))
        translucent=dict(layer,alpha=.88)
        priors['scenario_masks'][1][key]=GlyphWitness(-2,-3,2,2,bytes((198,246,192,0)))
        self.assertTrue(_glyph_witness_gate(translucent,(5,7),1.5,priors,(1,1,1),(87,87,87)))
        self.assertTrue(_glyph_witness_gate(translucent,(8.5/1.5,7),1.5,priors,(1,1,1),(35,35,35)))
        self.assertFalse(_glyph_witness_gate(translucent,(5,7),1.5,priors,(1,1,1),(255,255,255)))
        priors['scenario_masks'][1][key]=mask
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
            self.assertTrue(_plain_json(failures),failures)
            self.assertTrue(any(type(f.get('pixel')) is list for f in failures))

    def test_white_guard_spans_still_check_a_shared_stray_pixel(self):
        values=synthetic()
        region=(-10,-10,70,50)
        clean,clean_measurements=_layered_context(*values,physical_region=region)
        self.assertEqual(clean,[])
        x,y=73,44
        index=(y*80+x)*4
        for side,(source,size) in enumerate(values[4]):
            pixels=bytearray(source)
            pixels[index:index+4]=bytes((255,0,0,255))
            values[4][side]=(bytes(pixels),size)
        failures,measurements=_layered_context(*values,physical_region=region)
        self.assertEqual([(f.get('side'),f.get('pixel')) for f in failures],
                         [(0,[x-9.5,y-9.5]),(1,[x-9.5,y-9.5])])
        self.assertEqual([m['counts'] for m in clean_measurements if m['category']=='paint-strata'],
                         [m['counts'] for m in measurements if m['category']=='paint-strata'])

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
