"""Synthetic CPU stratum controls; these are not GPU calibration evidence."""
from copy import deepcopy
import math
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.comparison import _layered_context,_actual_material,css_shadows,_composite_point
from demo_differential.calibration import CAPS
from demo_differential.adapters import RunError


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
                    effect_kind=0,outline_width=0,outline_offset=0,offset=[0,0],blur=2,spread=0)
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


if __name__=='__main__':unittest.main()
