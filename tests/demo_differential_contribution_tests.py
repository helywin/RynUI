"""Pure Python fixed-source math contract; no browser or native launch."""
import pathlib
import sys
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from tools.demo_differential.color_metrics import visible_basis, solve_visible_contributions, contribution_domain, in_contribution_domain,visible_segment
from tools.demo_differential.protocol import ProtocolError


class Contributions(unittest.TestCase):
    def test_two_axes_and_order_feasibility(self):
        basis=visible_basis((1,1,1),[(0,0,0),(0,0,1)])
        value=solve_visible_contributions((.5,.5,.75),basis,alphas=(.5,.5))
        self.assertTrue(value['valid'])
        for actual in value['contributions']:
            self.assertAlmostEqual(actual,.25)
        self.assertFalse(solve_visible_contributions((.1,.1,.5),basis,alphas=(.1,.5))['valid'])

    def test_known_wrong_color_is_difference(self):
        basis=visible_basis((1,1,1),[(0,0,0)])
        self.assertFalse(solve_visible_contributions((1,0,0),basis,alphas=(1,))['valid'])

    def test_three_axes_sum_nonnegative(self):
        basis=visible_basis((0,0,0),[(1,0,0),(0,1,0),(0,0,1)])
        self.assertTrue(solve_visible_contributions((.2,.3,.4),basis)['valid'])
        self.assertFalse(solve_visible_contributions((.5,.5,.5),basis)['valid'])

    def test_same_color_preserves_joint_only(self):
        basis=visible_basis((1,1,1),[(0,0,0),(.5,.5,.5),(0,0,0)])
        value=solve_visible_contributions((.75,.75,.75),basis)
        self.assertTrue(value['valid'])
        self.assertIsNone(value['contributions'])
        self.assertFalse(solve_visible_contributions((.75,.5,.75),basis)['valid'])

    def test_rank_and_precision_reject_before_pixels(self):
        for colors in [[(0,0,0),(0,0,.00001)],[(.999,.999,.999)],
                       [(1,0,0),(0,1,0),(.5,.5,0)]]:
            with self.assertRaises(ProtocolError):
                visible_basis((0,0,0) if len(colors)==3 else (1,1,1),colors)

    def test_joint_opacity_cannot_produce_opaque_ink(self):
        basis=visible_basis((1,1,1),[(0,0,0),(0,0,0)])
        self.assertFalse(solve_visible_contributions((0,0,0),basis,alphas=(.1,.1))['valid'])
        value=solve_visible_contributions((.81,.81,.81),basis,alphas=(.1,.1))
        self.assertTrue(value['valid'])
        self.assertAlmostEqual(value['feasible_contrast'][1],.19)
        self.assertIsNone(value['contributions'])
        self.assertFalse(solve_visible_contributions((.99,.99,.99),basis,alphas=(0,0))['valid'])

    def test_joint_opposite_colors_keep_order_extrema(self):
        basis=visible_basis((.5,.5,.5),[(0,0,0),(1,1,1)])
        self.assertTrue(solve_visible_contributions((.75,.75,.75),basis,alphas=(.5,.5))['valid'])
        self.assertFalse(solve_visible_contributions((0,0,0),basis,alphas=(.5,.5))['valid'])

    def test_joint_quantization_uses_declared_byte_error(self):
        basis=visible_basis((1,1,1),[(0,0,0),(0,0,0)],byte_error=.01)
        self.assertTrue(solve_visible_contributions((.495,.5,.505),basis)['valid'])

    def test_joint_rejects_invalid_source_opacity_metadata(self):
        basis=visible_basis((1,1,1),[(0,0,0),(0,0,0)])
        for alphas in [(1,),(-.1,.5),(float('nan'),.5)]:
            with self.assertRaises(ProtocolError):
                solve_visible_contributions((.5,.5,.5),basis,alphas=alphas)

    def test_finite_domain_rank_one_two_three(self):
        for vertices,inside,outside in [
                ([(0,0),(1,1)],(.5,.5),(.5,.6)),
                ([(0,0),(1,0),(0,1)],(.25,.25),(.75,.75)),
                ([(0,0,0),(1,0,0),(0,1,0),(0,0,1)],(.2,.2,.2),(.4,.4,.4))]:
            domain=contribution_domain(vertices)
            self.assertTrue(in_contribution_domain(inside,domain,(1e-6,)*len(inside)))
            self.assertFalse(in_contribution_domain(outside,domain,(1e-6,)*len(outside)))

    def test_cube_hull_and_quantization(self):
        domain=contribution_domain([(x,y,z) for x in (0,1) for y in (0,1) for z in (0,1)])
        self.assertTrue(in_contribution_domain((.5,.3,.7),domain,(0,0,0)))
        self.assertFalse(in_contribution_domain((1.1,.3,.7),domain,(.001,)*3))
        self.assertTrue(in_contribution_domain((1.001,.3,.7),domain,(.002,)*3))

    def test_weak_opacity_keeps_visible_alpha_units(self):
        result=visible_segment((.99,.99,.99),(1,1,1),(.98,.98,.98),.02)
        self.assertTrue(result['valid'])
        self.assertAlmostEqual(result['contribution'],.01)
        self.assertLess(result['quantization_bound'],.01)
        self.assertFalse(visible_segment((1,0,0),(1,1,1),(.98,.98,.98),.02)['valid'])


if __name__=='__main__':unittest.main()
