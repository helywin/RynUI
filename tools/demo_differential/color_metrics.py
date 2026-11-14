"""Fixed encoded-sRGB/D65 math. No fitting, registration, or renderer changes.

Equations: IEC sRGB transfer/primaries as documented by W3C CSS Color 4;
CIEDE2000: Sharma, Wu, Dalal (2005), DOI 10.1002/col.20070, equations 2-22.
This is an independent implementation of the equations, not copied program code.
Lab here explicitly uses D65, with no CSS Lab/D50 chromatic adaptation.
"""
from __future__ import annotations

import math
from .protocol import ProtocolError


def vector(value, length=3, *, unit=False):
    if (len(value) != length or any(type(x) not in (int, float) or not math.isfinite(x) for x in value)
            or unit and any(not 0 <= x <= 1 for x in value)):
        raise ProtocolError('color vector is not finite or outside its declared domain')
    return tuple(value)


def srgb_to_linear(rgb):
    return tuple(x / 12.92 if x <= .04045 else ((x + .055) / 1.055) ** 2.4
                 for x in vector(rgb, unit=True))


def srgb_to_lab(rgb):
    r, g, b = srgb_to_linear(rgb)
    # D65 xy=(.3127,.3290), rational sRGB primary matrix. Y is normalized to 1.
    xyz = (506752/1228815*r + 87881/245763*g + 12673/70218*b,
           87098/409605*r + 175762/245763*g + 12673/175545*b,
           7918/409605*r + 87881/737289*g + 1001167/1053270*b)
    white = (.3127 / .3290, 1, (1 - .3127 - .3290) / .3290)
    epsilon = 216 / 24389
    kappa = 24389 / 27
    f = [x ** (1/3) if x > epsilon else (kappa*x + 16)/116 for x in
         (v/w for v, w in zip(xyz, white))]
    return 116*f[1] - 16, 500*(f[0] - f[1]), 200*(f[1] - f[2])


def delta_e00(first, second):
    l1, a1, b1 = vector(first)
    l2, a2, b2 = vector(second)
    chroma = (math.hypot(a1, b1) + math.hypot(a2, b2)) / 2
    g = (1 - math.sqrt(chroma**7 / (chroma**7 + 25**7))) / 2
    aa1, aa2 = (1+g)*a1, (1+g)*a2
    c1, c2 = math.hypot(aa1, b1), math.hypot(aa2, b2)
    h1 = math.degrees(math.atan2(b1, aa1)) % 360 if c1 else 0
    h2 = math.degrees(math.atan2(b2, aa2)) % 360 if c2 else 0
    dh = h2-h1
    if c1*c2 == 0:
        dh = 0
    elif dh > 180:
        dh -= 360
    elif dh < -180:
        dh += 360
    dl, dc = l2-l1, c2-c1
    dh_term = 2*math.sqrt(c1*c2)*math.sin(math.radians(dh/2))
    mean_l, mean_c = (l1+l2)/2, (c1+c2)/2
    if c1*c2 == 0:
        mean_h = h1+h2
    elif abs(h1-h2) <= 180:
        mean_h = (h1+h2)/2
    else:
        mean_h = (h1+h2 + (360 if h1+h2 < 360 else -360))/2
    cos = lambda degrees: math.cos(math.radians(degrees))
    t = 1 - .17*cos(mean_h-30) + .24*cos(2*mean_h) + .32*cos(3*mean_h+6) - .20*cos(4*mean_h-63)
    rotation = -2*math.sqrt(mean_c**7/(mean_c**7+25**7))*math.sin(
        math.radians(60*math.exp(-((mean_h-275)/25)**2)))
    sl = 1 + .015*(mean_l-50)**2/math.sqrt(20+(mean_l-50)**2)
    sc, sh = 1+.045*mean_c, 1+.015*mean_c*t
    ll, cc, hh = dl/sl, dc/sc, dh_term/sh
    return math.sqrt(max(0, ll*ll + cc*cc + hh*hh + rotation*cc*hh))


def rgb8_delta_e00(first, second):
    for value in (first, second):
        if len(value) != 3 or any(type(x) is not int or not 0 <= x <= 255 for x in value):
            raise ProtocolError('DeltaE input must be encoded RGB8, not premultiplied/unknown bytes')
    if first == second:
        return 0
    return delta_e00(srgb_to_lab([x/255 for x in first]), srgb_to_lab([x/255 for x in second]))


def source_over(foreground, background, alpha):
    foreground, background = vector(foreground, unit=True), vector(background, unit=True)
    vector([alpha], 1, unit=True)
    return tuple(alpha*f+(1-alpha)*b for f, b in zip(foreground, background))


def dual_background(observed0, observed1, background0, background1, *, tolerance=1/255):
    """Solve in the predeclared composition domain. Alpha zero cannot identify F."""
    o0, o1, b0, b1 = [vector(v, unit=True) for v in (observed0, observed1, background0, background1)]
    estimates = [1-(y-x)/(d-c) for x,y,c,d in zip(o0,o1,b0,b1) if abs(d-c) > 2*tolerance]
    if not estimates:
        raise ProtocolError('dual backgrounds cannot identify alpha')
    alpha = sum(estimates)/len(estimates)
    if not 0 <= alpha <= 1 or max(estimates)-min(estimates) > 2*tolerance:
        raise ProtocolError('dual background channels contradict source-over')
    if alpha <= 2*tolerance:
        if any(abs(x-c) > tolerance or abs(y-d) > tolerance for x,y,c,d in zip(o0,o1,b0,b1)):
            raise ProtocolError('alpha-zero observation differs from background')
        return {'alpha': alpha, 'foreground': None}
    foreground = tuple((x-(1-alpha)*b)/alpha for x,b in zip(o0,b0))
    if any(x < -tolerance/alpha or x > 1+tolerance/alpha for x in foreground):
        raise ProtocolError('source-over solution is outside the declared color domain')
    # Do not clamp an invalid solution into validity.
    if any(abs(alpha*f+(1-alpha)*b-y) > tolerance for f,b,y in zip(foreground,b1,o1)):
        raise ProtocolError('second background does not reconstruct')
    return {'alpha': alpha, 'foreground': foreground}


def unmix_coverage(observed, foreground, background, alpha, *, byte_error=.5/255):
    """Known material only; return k and perpendicular error without clamping k."""
    o, f, b = [vector(v, unit=True) for v in (observed, foreground, background)]
    vector([alpha], 1, unit=True)
    direction = tuple(x-y for x,y in zip(f,b))
    norm = sum(x*x for x in direction)
    if alpha == 0 or alpha*math.sqrt(norm) <= 4*byte_error:
        raise ProtocolError('material/background/alpha cannot identify coverage')
    k = sum((x-y)*d for x,y,d in zip(o,b,direction))/(alpha*norm)
    error = max(abs(x-y-alpha*k*d) for x,y,d in zip(o,b,direction))
    uncertainty = math.sqrt(3)*byte_error/(alpha*math.sqrt(norm))
    if k < -uncertainty or k > 1+uncertainty or error > 2*byte_error:
        raise ProtocolError('pixel does not satisfy the declared material/coverage model')
    return {'coverage': k, 'perpendicular_error': error, 'quantization_bound': uncertainty}


def visible_basis(background, colors, *, byte_error=.5/255, condition_cap=64, uncertainty_cap=.05):
    """Freeze a source-only RGB basis before inspecting either capture.

    Only a full column rank basis of at most three colors identifies individual
    visible contributions. Collinear sources retain their identities externally
    and produce one joint contrast plane, never per-layer coverages.
    """
    b = vector(background, unit=True)
    fs = tuple(vector(c, unit=True) for c in colors)
    if not fs:
        return {'mode': 'flat', 'background': b, 'colors': fs, 'rank': 0, 'byte_error': byte_error}
    directions = [tuple(f-c for f,c in zip(color,b)) for color in fs]
    pivot = max(directions, key=lambda d: sum(x*x for x in d))
    norm = sum(x*x for x in pivot)
    if math.sqrt(norm) <= 4*byte_error:
        raise ProtocolError('visible colors have no quantization-safe contrast')
    factors = [sum(x*y for x,y in zip(d,pivot))/norm for d in directions]
    collinear = all(max(abs(x-k*y) for x,y in zip(d,pivot)) <= 1e-10 for d,k in zip(directions,factors))
    if collinear and len(fs) > 1:
        lo, hi = min([0,*factors]), max([0,*factors])
        return {'mode': 'joint-collinear', 'background': b, 'colors': fs, 'rank': 1,
                'endpoints': (tuple(c+lo*d for c,d in zip(b,pivot)),
                              tuple(c+hi*d for c,d in zip(b,pivot))),
                'source_factors': tuple(factors), 'byte_error': byte_error}
    if len(fs) > 3:
        raise ProtocolError('more than three independent visible color directions')
    # Modified Gram-Schmidt, then back substitution: no image-fitted colors.
    q, r = [], [[0.0]*len(fs) for _ in fs]
    for j,direction in enumerate(directions):
        v = list(direction)
        for i,axis in enumerate(q):
            r[i][j] = sum(x*y for x,y in zip(axis,v))
            v = [x-r[i][j]*y for x,y in zip(v,axis)]
        r[j][j] = math.sqrt(sum(x*x for x in v))
        if r[j][j] <= 1e-10:
            raise ProtocolError('rank-deficient noncollinear source mixture is not identifiable')
        q.append(tuple(x/r[j][j] for x in v))
    inverse = [[0.0]*3 for _ in fs]
    for channel in range(3):
        for j in reversed(range(len(fs))):
            inverse[j][channel] = (q[j][channel]-sum(r[j][k]*inverse[k][channel]
                                     for k in range(j+1,len(fs))))/r[j][j]
    condition = max(sum(abs(d[c]) for d in directions) for c in range(3))*max(sum(abs(x) for x in row) for row in inverse)
    uncertainty = tuple(byte_error*sum(abs(x) for x in row) for row in inverse)
    if condition > condition_cap or max(uncertainty) > uncertainty_cap:
        raise ProtocolError('visible contribution basis is ill-conditioned at RGB8 precision')
    return {'mode': 'visible-contribution', 'background': b, 'colors': fs, 'rank': len(fs),
            'inverse': tuple(tuple(row) for row in inverse), 'condition': condition,
            'quantization_bounds': uncertainty, 'byte_error': byte_error}


def solve_visible_contributions(observed, basis, *, alphas=None):
    """Return unclamped evidence plus validity; a bad known pixel is a difference.

    `alphas` are source_alpha * paint_opacity in actual bottom-to-top order.
    Group opacity must have been resolved as a group, never multiplied per child.
    """
    o = vector(observed, unit=True)
    b = basis['background']
    if alphas is not None:
        if len(alphas) != len(basis['colors']):
            raise ProtocolError('visible contribution/source order length differs')
        vector(alphas, len(alphas), unit=True)
    if basis['mode'] == 'flat':
        residual = max(abs(x-y) for x,y in zip(o,b))
        return {'valid': residual <= basis['byte_error'], 'residual': residual, 'contributions': ()}
    if basis['mode'] == 'joint-collinear':
        low,high = basis['endpoints']
        d = tuple(x-y for x,y in zip(high,low))
        norm = sum(x*x for x in d)
        value = sum((x-y)*z for x,y,z in zip(o,low,d))/norm
        residual = max(abs(x-y-value*z) for x,y,z in zip(o,low,d))
        uncertainty = math.sqrt(3)*basis['byte_error']/math.sqrt(norm)
        feasible_low, feasible_high = 0.0, 1.0
        if alphas is not None:
            # Coverage can vary independently in [0,1], but its source opacity
            # remains fixed. Propagate the extrema of actual source-over order.
            lower, upper = 0.0, 0.0
            factors = basis['source_factors']
            for factor, alpha in zip(factors, alphas):
                lower = min(lower, (1-alpha)*lower + alpha*factor)
                upper = max(upper, (1-alpha)*upper + alpha*factor)
            origin, end = min(0, *factors), max(0, *factors)
            feasible_low = (lower-origin)/(end-origin)
            feasible_high = (upper-origin)/(end-origin)
        return {'valid': feasible_low-uncertainty <= value <= feasible_high+uncertainty
                         and residual <= 2*basis['byte_error'],
                'joint_contrast': value, 'residual': residual, 'quantization_bound': uncertainty,
                'feasible_contrast': (feasible_low, feasible_high), 'contributions': None}
    delta = tuple(x-y for x,y in zip(o,b))
    weights = tuple(sum(x*y for x,y in zip(row,delta)) for row in basis['inverse'])
    errors = basis['quantization_bounds']
    reconstructed = tuple(c+sum(w*(f[i]-c) for w,f in zip(weights,basis['colors'])) for i,c in enumerate(b))
    residual = max(abs(x-y) for x,y in zip(o,reconstructed))
    valid = all(w >= -e for w,e in zip(weights,errors)) and sum(weights) <= 1+sum(errors)
    valid = valid and residual <= 2*basis['byte_error']
    if alphas is not None:
        remaining,remaining_error = 1.0,0.0
        for weight,error,alpha in reversed(list(zip(weights,errors,alphas))):
            valid = valid and weight <= alpha*remaining + error + alpha*remaining_error
            remaining -= weight
            remaining_error += error
    return {'valid': valid, 'contributions': weights, 'residual': residual,
            'quantization_bounds': errors, 'condition': basis['condition']}


def contribution_domain(vertices):
    """Convex source-over endpoint domain in <=3 visible axes, source-only.

    A finite stack's multilinear coverage map lies in the convex hull of its
    endpoint images. This is a feasibility guard, not recovered layer alpha.
    """
    points=list(dict.fromkeys(tuple(vector(v,len(v))) for v in vertices))
    if not points or not 1<=len(points[0])<=3 or any(len(p)!=len(points[0]) for p in points):
        raise ProtocolError('invalid finite contribution vertices')
    origin=points[0];axes=[]
    dot=lambda a,b:sum(x*y for x,y in zip(a,b))
    for point in points[1:]:
        v=[x-y for x,y in zip(point,origin)]
        for axis in axes:
            projection=dot(v,axis);v=[x-projection*y for x,y in zip(v,axis)]
        norm=math.sqrt(dot(v,v))
        if norm>1e-10:axes.append(tuple(x/norm for x in v))
    projected=[tuple(dot([x-y for x,y in zip(p,origin)],axis) for axis in axes) for p in points]
    facets=[]
    if len(axes)==1:
        lo=min(p[0] for p in projected);hi=max(p[0] for p in projected)
        facets=[((-1.,),-lo),((1.,),hi)]
    elif len(axes)==2:
        ordered=sorted(set(projected))
        cross=lambda a,b,c:(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        halves=[]
        for sequence in (ordered,list(reversed(ordered))):
            half=[]
            for point in sequence:
                while len(half)>1 and cross(half[-2],half[-1],point)<=1e-12:half.pop()
                half.append(point)
            halves.append(half[:-1])
        hull=halves[0]+halves[1]
        for a,b in zip(hull,hull[1:]+hull[:1]):
            normal=(b[1]-a[1],a[0]-b[0]);facets.append((normal,dot(normal,a)))
    elif len(axes)==3:
        # Fixed 3-D incremental hull. The original interior tetrahedron remains
        # interior while additional endpoint vertices are added.
        cross=lambda a,b:(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
        sub=lambda a,b:tuple(x-y for x,y in zip(a,b))
        ids=[0]
        ids.append(next(i for i,p in enumerate(projected) if dot(sub(p,projected[0]),sub(p,projected[0]))>1e-12))
        ids.append(next(i for i,p in enumerate(projected) if dot(cross(sub(projected[ids[1]],projected[0]),sub(p,projected[0])),cross(sub(projected[ids[1]],projected[0]),sub(p,projected[0])))>1e-12))
        normal=cross(sub(projected[ids[1]],projected[0]),sub(projected[ids[2]],projected[0]))
        ids.append(next(i for i,p in enumerate(projected) if abs(dot(normal,sub(p,projected[0])))>1e-10))
        center=tuple(sum(projected[i][c] for i in ids)/4 for c in range(3))
        def face(a,b,c):
            n=cross(sub(projected[b],projected[a]),sub(projected[c],projected[a]))
            return (a,c,b) if dot(n,sub(center,projected[a]))>0 else (a,b,c)
        faces=[face(ids[a],ids[b],ids[c]) for a,b,c in ((0,1,2),(0,1,3),(0,2,3),(1,2,3))]
        for index,point in enumerate(projected):
            if index in ids:continue
            visible=[]
            for f in faces:
                a,b,c=[projected[i] for i in f];n=cross(sub(b,a),sub(c,a))
                if dot(n,sub(point,a))>1e-10:visible.append(f)
            if not visible:continue
            edges={}
            for a,b,c in visible:
                for edge in ((a,b),(b,c),(c,a)):
                    key=tuple(sorted(edge));edges[key]=edges.get(key,0)+1
            faces=[f for f in faces if f not in visible]
            faces.extend(face(a,b,index) for (a,b),count in edges.items() if count==1)
        for f in faces:
            a,b,c=[projected[i] for i in f];n=cross(sub(b,a),sub(c,a));facets.append((n,dot(n,a)))
    return {'origin':origin,'axes':axes,'facets':facets}


def visible_segment(observed,lower,upper,maximum,*,byte_error=1/255):
    """One exposed source's visible alpha, without dividing by weak opacity.

    The endpoint images and maximum source weight come only from the fixed
    source-over expression, including group/upper-layer attenuation.
    """
    o,l,u=[vector(v,unit=True) for v in (observed,lower,upper)]
    vector([maximum],1,unit=True)
    if maximum==0:return {'valid':max(abs(x-y) for x,y in zip(o,l))<=2*byte_error,'contribution':0.,'quantization_bound':0.}
    d=tuple((x-y)/maximum for x,y in zip(u,l));norm=sum(x*x for x in d)
    if norm<=1e-20:raise ProtocolError('exposed source has no visible color direction')
    contribution=sum((x-y)*z for x,y,z in zip(o,l,d))/norm
    error=max(abs(x-y-contribution*z) for x,y,z in zip(o,l,d))
    uncertainty=math.sqrt(3)*byte_error/math.sqrt(norm)
    return {'valid':-uncertainty<=contribution<=maximum+uncertainty and error<=2*byte_error,
            'contribution':contribution,'quantization_bound':uncertainty,'residual':error}


def in_contribution_domain(value,domain,quantization_bounds):
    point=vector(value,len(domain['origin']));errors=vector(quantization_bounds,len(point))
    delta=tuple(x-y for x,y in zip(point,domain['origin']))
    projected=tuple(sum(x*y for x,y in zip(delta,axis)) for axis in domain['axes'])
    rebuilt=tuple(sum(p*axis[c] for p,axis in zip(projected,domain['axes'])) for c in range(len(point)))
    if any(abs(x-y)>e+1e-10 for x,y,e in zip(delta,rebuilt,errors)):return False
    for normal,bound in domain['facets']:
        world=tuple(sum(n*axis[c] for n,axis in zip(normal,domain['axes'])) for c in range(len(point)))
        error=sum(abs(n)*e for n,e in zip(world,errors))
        if sum(x*y for x,y in zip(normal,projected))>bound+error+1e-10:return False
    return True
