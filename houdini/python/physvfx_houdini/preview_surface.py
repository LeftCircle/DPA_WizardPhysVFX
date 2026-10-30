"""Low-resolution, dependency-free polygonization of analytic PhysVFX fields.

The equations mirror implicit_fields.cpp; this is only a layout aid, not the
renderer. Marching tetrahedra produces triangles in the prim's local space.
"""
import math

ISF_KINDS = ('isf_sphere', 'isf_torus', 'isf_cone', 'isf_box',
             'isf_icosahedron', 'isf_steiner_patch', 'isf_ellipse', 'isf_cylinder')


def _field(kind, v, sphere_radius):
    n = v['normal']
    norm = math.sqrt(sum(x*x for x in n))
    axis = tuple(x/norm for x in n) if norm else (0.0, 1.0, 0.0)
    radius, major, minor, height = v['radius'], v['majorRadius'], v['minorRadius'], v['height']
    def value(p):
        x, y, z = p
        r2 = x*x + y*y + z*z
        axial = sum(p[i]*axis[i] for i in range(3))
        radial2 = max(0.0, r2 - axial*axial)
        if kind == 'isf_sphere': return math.sqrt(r2) - sphere_radius
        if kind == 'isf_torus': return (r2 + major*major - minor*minor)**2 - 4*major*major*radial2
        if kind == 'isf_cylinder': return max(math.sqrt(radial2) - radius, abs(axial) - height/2)
        if kind == 'isf_ellipse': return axial*axial/(major*major) + radial2/(minor*minor) - 1
        if kind == 'isf_cone':
            if axial > height: return axial-height
            if axial < 0: return -axial
            if r2 == 0: return 0.0
            return math.acos(max(-1.0, min(1.0, axial/math.sqrt(r2)))) - v['theta']
        if kind == 'isf_box':
            exp = 2*v['roundingExponent']
            return x**exp + y**exp + z**exp - radius**exp
        if kind == 'isf_steiner_patch':
            return x*x*y*y + x*x*z*z + y*y*z*z - x*y*z
        if kind == 'isf_icosahedron':
            if math.sqrt(r2) > math.pi*1.8: return math.pi*1.8
            t = 1.61803399
            return 2-math.cos(x+t*y)-math.cos(x-t*y)-math.cos(y+t*z)-math.cos(y-t*z)-math.cos(z+t*x)-math.cos(z-t*x)
        raise ValueError('Unsupported ISF preview: ' + kind)
    return value


def polygonize(kind, values, sphere_radius, half_extent, resolution=18):
    """Return triangle vertices. Approximations depend on grid resolution."""
    if kind not in ISF_KINDS: return []
    sample = _field(kind, values, sphere_radius)
    extent = float(half_extent)*1.05
    n = resolution
    step = 2*extent/n
    size = n+1
    def idx(i,j,k): return (k*size+j)*size+i
    def point(i,j,k): return (-extent+i*step, -extent+j*step, -extent+k*step)
    grid = [sample(point(i,j,k)) for k in range(size) for j in range(size) for i in range(size)]
    corners = ((0,0,0),(1,0,0),(1,1,0),(0,1,0),(0,0,1),(1,0,1),(1,1,1),(0,1,1))
    tetrahedra = ((0,5,1,6),(0,1,2,6),(0,2,3,6),(0,3,7,6),(0,7,4,6),(0,4,5,6))
    triangles = []
    for k in range(n):
        for j in range(n):
            for i in range(n):
                vals = [grid[idx(i+dx,j+dy,k+dz)] for dx,dy,dz in corners]
                if all(x >= 0 for x in vals) or all(x < 0 for x in vals): continue
                pts = [point(i+dx,j+dy,k+dz) for dx,dy,dz in corners]
                def edge(a,b):
                    va,vb=vals[a],vals[b]
                    t=va/(va-vb) if va != vb else 0.5
                    return tuple(pts[a][m]+t*(pts[b][m]-pts[a][m]) for m in range(3))
                for tet in tetrahedra:
                    inside = [q for q in tet if vals[q] < 0]
                    outside = [q for q in tet if vals[q] >= 0]
                    if len(inside) in (0,4): continue
                    if len(inside) == 1:
                        triangles.extend(edge(inside[0], q) for q in outside)
                    elif len(inside) == 3:
                        triangles.extend(edge(outside[0], q) for q in inside)
                    else:
                        a,b=inside; c,d=outside
                        p0,p1,p2,p3=edge(a,c),edge(a,d),edge(b,d),edge(b,c)
                        triangles.extend((p0,p1,p2,p0,p2,p3))
    return triangles
