import physics_vfx as fx






s1 = fx.sphere(fx.Vector(0, 0, 0), 1.0)
s1 = fx.negate(fx.mask(s1))

fx.render_field(
    s1,
    "outputs/test.exr",
    width=960,
    height=540,
    eye=fx.Vector(0.0, 0.0, 4),
    view=fx.Vector(0.0, 0.0, -1.0),
    up=fx.Vector(0.0, 1.0, 0.0),
    sfar=6,
    ds=0.01,
    kappa=0.0001
)


