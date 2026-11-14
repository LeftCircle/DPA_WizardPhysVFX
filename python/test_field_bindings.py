"""Exercise the supported Python field and noise bindings."""
import math
import tempfile
from pathlib import Path
import unittest

import physics_vfx as fx


def xyz(v):
    return (v.x, v.y, v.z)


class FieldBindingsTests(unittest.TestCase):
    def setUp(self):
        self.origin = fx.Vector(0, 0, 0)
        self.point = fx.Vector(1, 2, 3)

    def test_wisp_color_grid_is_density_weighted(self):
        noise = fx.FSVals(.5, 2, self.origin, 1, 2)
        density = fx.FloatGrid(.25, 0)
        color = fx.ColorGrid(.25)
        for value, tint in ((-1, fx.Color(1, 0, 0)), (-3, fx.Color(0, 1, 0))):
            wisp = fx.wisp_uniform(500, value, 1, 1, noise, noise,
                fx.Vector(1, 1, 1), self.origin, fx.CoordinateSys(), 17, tint)
            wisp.stamp_colored(density, color)
        fx.normalize_wisp_colors(density, color)
        hit = False
        for x in range(-12, 13):
            for y in range(-12, 13):
                for z in range(-12, 13):
                    if density.get_value(x, y, z) < -1e-5:
                        value = color.get_value(x, y, z)
                        self.assertAlmostEqual(value.red, .25, places=4)
                        self.assertAlmostEqual(value.green, .75, places=4)
                        self.assertAlmostEqual(value.blue, 0, places=4)
                        hit = True
        self.assertTrue(hit)

    def test_point_noise_stamps_additively_with_bounded_falloff(self):
        grid = fx.FloatGrid(0.2, 0)
        args = (grid, self.origin, 1.0, .5, 2.0, .5, 3.0, 1.0, 2.0)
        fx.stamp_point_noise(*args)
        values = {(x,y,z): grid.get_value(x,y,z) for x in range(-5,6)
                  for y in range(-5,6) for z in range(-5,6)}
        self.assertTrue(any(value < 0 for value in values.values()))
        self.assertEqual(grid.get_value(10, 0, 0), 0)
        fx.stamp_point_noise(*args)
        for (x,y,z), value in values.items():
            self.assertAlmostEqual(grid.get_value(x,y,z), value * 2, places=5)
        with self.assertRaises(ValueError):
            fx.stamp_point_noise(grid, self.origin, 1, 0, 1, .5, 3, 1, 2)

    def test_point_noise_seed_and_translation_are_deterministic(self):
        def stamp(*extra):
            grid = fx.FloatGrid(.2, 0)
            fx.stamp_point_noise(grid, self.origin, 2, .5, 1, .5, 3, 1, 2, *extra)
            return [grid.get_value(x, y, z) for x in range(-8, 9)
                    for y in range(-8, 9) for z in range(-8, 9)]
        original = stamp()
        self.assertEqual(original, stamp(self.origin, 0))
        self.assertEqual(stamp(self.origin, 42), stamp(self.origin, 42))
        self.assertNotEqual(original, stamp(self.origin, 42))
        self.assertNotEqual(original, stamp(fx.Vector(.7, .2, -.3), 0))

    def test_point_noise_stamps_weighted_colors(self):
        density, color = fx.FloatGrid(.2, 0), fx.ColorGrid(.2)
        for strength, tint in ((1, fx.Color(1, 0, 0)), (3, fx.Color(0, 0, 1))):
            fx.stamp_point_noise(density, self.origin, 2, .5, strength, .5, 3, 1, 2,
                                 self.origin, 0, color, tint)
        fx.normalize_wisp_colors(density, color)
        hits = 0
        for x in range(-8, 9):
            for y in range(-8, 9):
                for z in range(-8, 9):
                    if density.get_value(x, y, z) < -1e-5:
                        value = color.get_value(x, y, z)
                        self.assertAlmostEqual(value.red, .25, places=4)
                        self.assertAlmostEqual(value.blue, .75, places=4)
                        self.assertAlmostEqual(value.green, 0, places=4)
                        hits += 1
        self.assertGreater(hits, 0)
        with self.assertRaises(ValueError):
            fx.stamp_point_noise(density, self.origin, 2, .5, 1, .5, 3, 1, 2,
                                 self.origin, 0, fx.ColorGrid(.5), fx.Color(1, 0, 0))

    def assertVectorAlmostEqual(self, actual, expected, places=4):
        for value, target in zip(xyz(actual), expected):
            self.assertAlmostEqual(value, target, places=places)

    def test_primitives_and_scalar_operations(self):
        sphere = fx.isf_sphere(self.origin, 1)
        self.assertAlmostEqual(sphere.eval(self.origin), -1)
        self.assertEqual(fx.sphere(self.origin, 1).eval(self.origin), -1)
        two, three = fx.make_constant(2.0), fx.constant(3.0)
        self.assertEqual(fx.add(two, three).eval(self.origin), 5)
        self.assertEqual(fx.subtract(two, three).eval(self.origin), -1)
        self.assertEqual(fx.pow(two, 3.0).eval(self.origin), 8)
        self.assertEqual(fx.pow(two, three).eval(self.origin), 8)
        self.assertEqual(fx.abs(fx.constant(-2.0)).eval(self.origin), 2)
        self.assertEqual(fx.clamp_fixed(three, 0.0, 2.0).eval(self.origin), 2)
        self.assertAlmostEqual(fx.smoothstep(0, 1, 0.5), 0.5)
        self.assertVectorAlmostEqual(fx.smoothstep_lerp(self.origin, self.point, 0.5), (0.5, 1, 1.5))

    def test_affine_fields_scene_colors_and_native_components(self):
        sphere = fx.isf_sphere(self.origin, 1)
        moved = fx.affine_transform_field(sphere, fx.Vector(-3, 0, 0),
            fx.Vector(1, 0, 0), fx.Vector(0, 1, 0), fx.Vector(0, 0, 1))
        self.assertLess(moved.eval(fx.Vector(3, 0, 0)), 0)
        self.assertGreater(moved.eval(self.origin), 0)
        stretched = fx.affine_transform_field(sphere, fx.Vector(0, 0, 0),
            fx.Vector(0.5, 0, 0), fx.Vector(0, 1, 0), fx.Vector(0, 0, 2))
        self.assertAlmostEqual(stretched.eval(fx.Vector(2, 0, 0)), 0, places=5)
        self.assertAlmostEqual(stretched.eval(fx.Vector(0, 1, 0)), 0, places=5)
        self.assertAlmostEqual(stretched.eval(fx.Vector(0, 0, 0.5)), 0, places=5)
        self.assertLess(stretched.eval(self.origin), 0)
        self.assertGreater(stretched.eval(fx.Vector(3, 0, 0)), 0)
        other = fx.isf_sphere(fx.Vector(-3, 0, 0), 1)
        colors = fx.make_scene_color_field([moved, other], [fx.Color(1, 0, 0), fx.Color(0, 1, 0)])
        red, green = colors.eval(fx.Vector(3, 0, 0)), colors.eval(fx.Vector(-3, 0, 0))
        self.assertEqual((red.red, red.green, red.blue), (1, 0, 0))
        self.assertEqual((green.red, green.green, green.blue), (0, 1, 0))
        with self.assertRaises(ValueError):
            fx.make_scene_color_field([moved], [])
        light = fx.PointLight(fx.Color(1, 0.5, 0.25), fx.Vector(0, 1, 2), 0.7)
        self.assertAlmostEqual(light.kappa, 0.7, places=5)
        self.assertEqual(light.position.y, 1)
        camera = fx.Camera()
        camera.set_eye_view_up(fx.Vector(0, 0, 8), fx.Vector(0, 0, -1), fx.Vector(0, 1, 0))
        camera.set_fov(40)
        camera.set_aspect_ratio(2)
        self.assertEqual(camera.eye.z, 8)
        self.assertEqual(camera.view.z, -1)
        self.assertAlmostEqual(camera.fov, 40)

    def test_general_renderer_rejects_invalid_bounds(self):
        sphere = fx.isf_sphere(self.origin, 1)
        density = fx.negate(fx.mask(sphere))
        color = fx.make_scene_color_field([sphere], [fx.Color(1, 0, 0)])
        with self.assertRaises(ValueError):
            fx.render_usd_scene(density, color, fx.Camera(), [], fx.RenderSettings(),
                                fx.Vector(1, 1, 1), fx.Vector(-1, -1, -1), 'unused.exr')

    def test_requested_interface_is_exposed(self):
        names = ('add', 'subtract', 'scale', 'translate', 'rotate', 'scale_fixed',
                 'translate_fixed', 'rotate_fixed', 'smoothstep', 'smoothstep_lerp',
                 'make_memofield', 'make_clear_cache_field', 'exp', 'log', 'sin', 'cos',
                 'pow', 'abs', 'make_pyro', 'make_advect_semi_lagrange', 'union_fields',
                 'intersection', 'cutout', 'clamp', 'clamp_fixed', 'mask', 'blinn_blend',
                 'dilation', 'shell', 'make_constant', 'make_plane', 'isf_sphere',
                 'isf_torus', 'isf_cone', 'isf_box', 'isf_icosahedron', 'isf_steiner_patch',
                 'isf_ellipse', 'isf_cylinder', 'make_grid_field', 'make_warp_field',
                 'make_identity_field', 'make_grad_field', 'makeNPT', 'make_iteratedNPT',
                 'make_eval_dot_grad_over_gradsq', 'fspn', 'make_fspn', 'make_fstn', 'make_fade_factor')
        for name in names:
            with self.subTest(name=name):
                self.assertTrue(callable(getattr(fx, name, None)))
        self.assertFalse(hasattr(fx, 'funcfield'))

    def test_vector_fields_transforms_and_warp(self):
        identity = fx.make_identity_field()
        self.assertVectorAlmostEqual(identity.eval(self.point), (1, 2, 3))
        matrix = identity.grad(self.point)
        self.assertAlmostEqual(matrix.get(0, 0), 1)
        displacement = fx.constant(fx.Vector(1, 0, 0))
        self.assertVectorAlmostEqual(fx.translate(identity, displacement).eval(self.point), (0, 2, 3))
        self.assertVectorAlmostEqual(fx.translate(identity, fx.Vector(1, 0, 0)).eval(self.point), (0, 2, 3))
        self.assertVectorAlmostEqual(fx.translate_fixed(identity, fx.Vector(1, 0, 0)).eval(self.point), (0, 2, 3))
        self.assertVectorAlmostEqual(fx.scale(identity, fx.constant(2.0)).eval(self.point), (0.5, 1, 1.5))
        self.assertVectorAlmostEqual(fx.scale_fixed(identity, fx.Vector(2, 2, 2)).eval(self.point), (0.5, 1, 1.5))
        self.assertVectorAlmostEqual(fx.make_warp_field(identity, displacement).eval(self.point), (1, 0, 0))
        self.assertVectorAlmostEqual(fx.make_advect_semi_lagrange(identity, displacement, 0.5).eval(self.point), (0.5, 2, 3))
        axis = fx.Vector(0, 1, 0)
        self.assertVectorAlmostEqual(fx.rotate(identity, axis, 0).eval(self.point), xyz(self.point))
        self.assertAlmostEqual(fx.rotate(identity, axis, 0).grad(self.point).get(0, 0), 1, places=3)
        self.assertVectorAlmostEqual(fx.rotate_fixed(identity, axis, 0).eval(self.point), xyz(self.point))
        self.assertVectorAlmostEqual(fx.make_clear_cache_field(fx.make_memofield(identity)).eval(self.point), xyz(self.point))

    def test_color_fields(self):
        red = fx.make_constant(fx.Color(1, 0, 0, 0.5))
        green = fx.make_constant(fx.Color(0, 1, 0, 0.25))
        color = fx.add(red, green).eval(self.point)
        self.assertEqual((color.red, color.green, color.blue, color.alpha), (1, 1, 0, 0.75))
        cached = fx.make_clear_cache_field(fx.make_memofield(red))
        self.assertEqual(cached.eval(self.point).red, 1)
        self.assertEqual(fx.make_warp_field(red, fx.make_identity_field()).eval(self.point).red, 1)

    def test_gradients_and_nearest_point(self):
        plane = fx.make_plane(self.origin, fx.Vector(1, 0, 0))
        self.assertVectorAlmostEqual(fx.make_grad_field(plane).eval(self.point), (1, 0, 0))
        self.assertVectorAlmostEqual(fx.make_eval_dot_grad_over_gradsq(plane).eval(self.point), (1, 0, 0))
        self.assertVectorAlmostEqual(fx.makeNPT(plane).eval(self.point), (0, 2, 3))
        self.assertVectorAlmostEqual(fx.make_iteratedNPT(plane, 1).eval(self.point), (0, 2, 3))

    def test_float_and_color_grid_fields(self):
        grid = fx.FloatGrid(1.0)
        grid.set_value(1, 2, 3, 4.0)
        self.assertEqual(grid.get_value(1, 2, 3), 4.0)
        self.assertAlmostEqual(fx.make_grid_field(grid).eval(self.point), 4.0)
        color_grid = fx.ColorGrid(1.0)
        color_grid.set_value(1, 2, 3, fx.Color(0.2, 0.4, 0.6))
        color = fx.make_grid_field(color_grid).eval(self.point)
        self.assertAlmostEqual(color.red, 0.2, places=5)
        self.assertAlmostEqual(color.green, 0.4, places=5)
        self.assertAlmostEqual(color.blue, 0.6, places=5)
        with self.assertRaises(ValueError):
            fx.FloatGrid(0)

    def test_shadow_region_grid_cache_roundtrip(self):
        density = fx.negate(fx.mask(fx.isf_sphere(self.origin, 1)))
        region = (fx.Vector(-1.5, -1.5, -1.5), fx.Vector(1.5, 1.5, 1.5))
        light = fx.PointLight(fx.Color(1, 1, 1), fx.Vector(0, 0, 3), 1)
        grid = fx.make_shadow_region_grid(density, 0.5, region, light, 0.25, [region])
        self.assertLess(grid.get_value(0, 0, 0), 0)
        with tempfile.TemporaryDirectory() as tmp:
            path = str(Path(tmp) / 'shadow.vdb')
            fx.save_to_disk(grid, path)
            restored = fx.load_float_grid(path)
            self.assertAlmostEqual(restored.get_value(0, 0, 0), grid.get_value(0, 0, 0), places=5)
            camera = fx.Camera()
            camera.set_eye_view_up(fx.Vector(0, 0, 4), fx.Vector(0, 0, -1), fx.Vector(0, 1, 0))
            camera.set_fov(45); camera.set_aspect_ratio(1)
            settings = fx.RenderSettings()
            settings.width = settings.height = 4
            settings.near_dist, settings.far_dist = 0.1, 10
            settings.min_step = settings.max_step = 0.2
            settings.shadow_step = 0.25
            settings.density_voxel_size = settings.color_voxel_size = settings.shadow_voxel_size = 0.5
            field = fx.isf_sphere(self.origin, 1)
            color = fx.make_scene_color_field([field], [fx.Color(1, 1, 1)])
            output = str(Path(tmp) / 'cached.exr')
            preview = str(Path(tmp) / 'preview.exr')
            fx.render_usd_scene(density, color, camera, [light], settings, region[0], region[1],
                                output, [region], [[restored]], preview)
            self.assertTrue(Path(output).is_file())
            self.assertTrue(Path(preview).is_file())

    def test_noise_warp_samples_displaced_position(self):
        plane = fx.make_plane(self.origin, fx.Vector(0, 1, 0))
        warped = fx.noise_warp_field(plane, fx.make_constant(0.5), fx.Vector(0, 2, 0), 2.0)
        self.assertAlmostEqual(warped.eval(self.origin), 1.0)
        with self.assertRaises(ValueError):
            fx.noise_warp_field(plane, fx.make_constant(1.0), self.origin, 1.0)

    def test_terrain_displace_zero_noise_preserves_field(self):
        vals = fx.FSVals(0.5, 2, self.origin, 1, 2)
        field = fx.isf_sphere(self.origin, 1)
        displaced = fx.terrain_displace(field, vals, 0, 0, 1, 1, 0.3, 1)
        for p in (fx.Vector(2, 0, 0), fx.Vector(0, 0.5, 0)):
            self.assertAlmostEqual(displaced.eval(p), field.eval(p), places=5)
        with self.assertRaises(ValueError):
            fx.terrain_displace(field, vals, 1, 1, 1, 1, 0.3, 0)

    def test_cone_gradient_caps_and_nearest_point_are_stable(self):
        cone = fx.isf_cone(self.origin, fx.Vector(0, 2, 0), 2, 0.5)
        self.assertLess(cone.eval(fx.Vector(0.1, 1.99999, 0)), 0)
        self.assertGreater(cone.eval(fx.Vector(0.1, 2.00001, 0)), 0)
        for p in ((0.7, 1, 0), (0.1, 1.5, 0), (0.5, 2.2, 0.1), (0.8, -0.2, 0)):
            v = fx.Vector(*p)
            g = xyz(cone.grad(v))
            for i in range(3):
                plus, minus = list(p), list(p)
                plus[i] += 0.0001; minus[i] -= 0.0001
                numerical = (cone.eval(fx.Vector(*plus)) - cone.eval(fx.Vector(*minus))) / 0.0002
                self.assertAlmostEqual(g[i], numerical, delta=0.002)
        npt = fx.make_iteratedNPT(cone, 5)
        for p in (self.origin, fx.Vector(0, 1, 0), fx.Vector(0, 2, 0)):
            self.assertTrue(all(math.isfinite(x) for x in xyz(npt.eval(p))))
        transformed = fx.affine_transform_field(cone, self.origin, fx.Vector(0.5, 0, 0),
                                                fx.Vector(0, 1, 0), fx.Vector(0, 0, 2))
        g = xyz(transformed.grad(fx.Vector(1.4, 1, 0)))
        source = xyz(cone.grad(fx.Vector(0.7, 1, 0)))
        for a, b in zip(g, (source[0]*0.5, source[1], source[2]*2)):
            self.assertAlmostEqual(a, b, places=5)
        with self.assertRaises(ValueError):
            fx.isf_cone(self.origin, self.origin, 2, 0.5)

    def test_noise_overloads(self):
        vals = fx.FSVals(0.5, 2.0, self.origin, 1.0, 2.0)
        direct = fx.fspn(vals, self.point)
        a = fx.make_fspn(vals).eval(self.point)
        b = fx.make_fspn(vals.r, vals.octaves, vals.translation, vals.freq, vals.fjump).eval(self.point)
        self.assertTrue(math.isfinite(direct))
        self.assertAlmostEqual(a, direct, places=5)
        self.assertAlmostEqual(b, direct, places=5)
        self.assertAlmostEqual(fx.make_fade_factor(self.origin, 1, 2).eval(self.origin), 1)
        sphere = fx.sphere(self.origin, 1)
        pyro = fx.make_pyro(sphere, 0.5, 2, self.origin, 1, 2, 0, 1)
        self.assertAlmostEqual(pyro.eval(fx.Vector(2, 0, 0)), 1, places=4)

    def test_terrain_noise_matches_slide_formula(self):
        vals = fx.FSVals(0.5, 3.0, self.origin, 1.0, 2.0)
        field = fx.make_fstn(vals, 2.0, 3.0, 3.0, 0.5, 0.3)
        covered = set()
        for x in range(-15, 16):
            for z in range(-15, 16):
                point = fx.Vector(x * 0.13, 0.27, z * 0.17)
                n = fx.fspn(vals, point)
                if n == 0:
                    expected = 0.0
                else:
                    covered.add((n > 0, abs(n) >= 0.3))
                    gamma = 1 + min(abs(n) / 0.3, 1) * ((3.0 if n > 0 else 0.5) - 1)
                    expected = (2.0 if n > 0 else -3.0) * abs(n) ** gamma
                self.assertAlmostEqual(field.eval(point), expected, places=5)
        self.assertEqual(len(covered), 4)
        with self.assertRaises(ValueError):
            fx.make_fstn(vals, 1, 1, 1, 1, 0)


if __name__ == '__main__':
    unittest.main()
