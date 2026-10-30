# Physics-Based VFX: Volume Rendering and Fields

A C++ and CUDA project for building, manipulating, and rendering volumetric
fields. It combines analytic implicit surfaces, procedural noise and wisps,
OpenVDB grids, and a ray-marching renderer. Python bindings and USD scene
loading provide another way to assemble and render scenes.

## Houdini Solaris authoring

The [`houdini/`](houdini/) tools create Solaris nodes for authoring PhysVFX
fields, operations, cameras, lights, and render settings on a USD stage. See
[`houdini/README.md`](houdini/README.md) for installation, a layout-to-render
workflow, the supported attributes, and testing instructions.

## Connections to volume rendering research

The following parts of the code are relevant to the volume rendering internship
posting. They demonstrate related implementation work and provide a starting
point for further research; they do not by themselves establish experience
with a production renderer or artist collaboration.

| Job topic | Relevant project work | Where to look |
| --- | --- | --- |
| Physically based volume rendering | CPU ray marching accumulates color and exponential transmittance through a density field, with point lights and deep shadow maps. A separate CUDA ray marcher implements a related GPU path. | [`src/ray_marcher.cpp`](src/ray_marcher.cpp), [`src/deep_shadow_map.cpp`](src/deep_shadow_map.cpp), [`src/cuda/ray_marcher_cuda.cu`](src/cuda/ray_marcher_cuda.cu) |
| Rendering efficiency and thin-volume sampling | Randomized ray and shadow stepping, sparse OpenVDB grids, and a CUDA implementation offer places to study sampling cost, resolution and quality. The current renderer uses ray marching; it does not implement delta tracking or path guiding. | [`src/ray_marcher.cpp`](src/ray_marcher.cpp), [`include/deep_shadow_map.h`](include/deep_shadow_map.h), [`src/openvdb_helpers.cpp`](src/openvdb_helpers.cpp) |
| Volume appearance and procedural looks | Noise fields, fractal sums, and wisp construction provide controls for shaping volumetric detail and exploring pyro and wisp looks. | [`include/noise_fields.h`](include/noise_fields.h), [`include/fractal_sum_noise.h`](include/fractal_sum_noise.h), [`include/wisps.h`](include/wisps.h) |
| Surface and volume representations | Analytic implicit surfaces and converted OBJ/USD meshes can be represented as scalar fields and used to construct volume densities. This is a useful basis for exploring surface/volume relationships, not a unified surface, subsurface, and volume transport model. | [`include/implicit_fields.h`](include/implicit_fields.h), [`include/field_interface.h`](include/field_interface.h), [`python/usd_mesh.py`](python/usd_mesh.py) |
| User-facing scene workflows | USD defines camera, lights, geometry, colors and renderer controls; Python helpers turn those into renderer components. The included scene combines analytic fields with meshes. | [`python/usd_sphere_scene.py`](python/usd_sphere_scene.py), [`python/inputs/isf_mesh_scene.usda`](python/inputs/isf_mesh_scene.usda), [`bindings/pybindings.cpp`](bindings/pybindings.cpp) |
| Modern C++ and mathematical foundations | Templated field operations, vector and matrix transforms, implicit functions, stochastic sampling, and numerical integration connect C++ implementation with calculus, linear algebra, and probability. | [`include/volume.h`](include/volume.h), [`include/field_operations.h`](include/field_operations.h), [`include/implicit_fields.h`](include/implicit_fields.h) |
| Verification and debugging | Catch2 tests and Python tests cover field behavior, scene loading, image output and module import. | [`tests/`](tests/), [`python/`](python/), [`CMakeLists.txt`](CMakeLists.txt) |

Potential next steps aligned with the posting include profiling thin volumes,
evaluating delta tracking and path guiding, comparing alternative sampling
strategies, and researching consistent surface/subsurface/volume transport.
These are research directions, not features currently claimed by this project.
The USD interface is a workflow foundation; production suitability and artist
usability would need evaluation with artists and technical directors.

## Procedural look development

Earlier wisp, pyro, and noise experiments used parameter wedges. Their videos
were referenced under `images/wisps`, `images/pyro_wedges`, and
`images/wedges`; those media files are not included in the repository.
The original parameter ranges and defaults are retained below.

Noise parameters:
```cpp
std::vector<float> mins = {1, 0.01, 0.00, 0.01, 1e-6};
std::vector<float> maxes = {8, 4, 1, 5, 3};
std::vector<float> defaults = {2.5, 1.5, 0.5, 2, 0};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"octave", "fade", "roughness", "freq", "fjump"};

```

Pyro parameters:
```cpp
std::vector<float> mins = {1, 0.01, 0.01, 1e-6, 0.01};
std::vector<float> maxes = {8, 5, 5, 5, 5};
std::vector<float> defaults = {3, 1, 2, 2, 2};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"octave", "gamma", "freq", "fjump", "amp"};

```

Wisp parameters (for `wisp_out_v02.mov`):
```cpp
std::vector<float> mins = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.1, 0.1};
std::vector<float> maxes = {10, 10, 10, 10, 5, 5, 5, 5, 5, 1, 1};
std::vector<float> defaults = {0.75, 1, 3, 5, 2, 1, 2, 1.5, 1, 0.1, 0.2};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"clump", "pscale", "o1", "o2", "f1" "f2", "fj1", "fj2", "dips", "rough1", "rough2"};

```

Most of the code is in the `include/` and `src/` folders.


## Build dependencies
The project is built with cmake, hence the build folder and all that goes with it. The third_party folder
currently contains the code for the image processing library from Computer Graphics Images, along with 
the starter kit code. 






## Render a USD sphere

The sphere example loads one `UsdGeomSphere`, one `UsdLuxSphereLight`, and
one perspective `UsdGeomCamera` through `UsdRenderSettings`. See
[`python/inputs/sphere.usda`](python/inputs/sphere.usda). Build this project
with `PBVFX_BUILD_PYTHON=ON`, and make the OpenUSD Python bindings (`pxr`)
available in the same Python environment as the built `physics_vfx` module.
The project also requires its existing CUDA, OpenVDB, image-processing and
submodule dependencies even for this CPU render path.

```bash
cmake -S . -B build -DPBVFX_BUILD_PYTHON=ON
cmake --build build --parallel
PYTHONPATH="$PWD/build/python" python python/usd_to_pbvfx.py \
  python/inputs/sphere.usda --output outputs/sphere.exr
```

Use `--volume-meters-per-unit` to change renderer units (default: meters).
Stage positions, radius, clip distances, and authored step and voxel sizes
are converted using the USD stage's `metersPerUnit`. Custom settings are on
the RenderSettings prim: `pbvfx:minStep`, `pbvfx:maxStep`, `pbvfx:kappa`,
`pbvfx:shadowKappa`, `pbvfx:shadowStep`, `pbvfx:densityVoxelSize`,
`pbvfx:colorVoxelSize`, and `pbvfx:shadowVoxelSize`. Unauthored values use
`RenderSettings` defaults in renderer units. A `pbvfx:shadowKappa` on the
light overrides the render setting. The kappa values are renderer parameters;
they are not automatically rescaled when changing renderer units.
Grid bounds are derived from the sphere instead of `model_halfsize`;
`cam_dist` is replaced by the camera's authored world transform.

The sphere's transform must have uniform scale and no shear; the camera must
be perspective with zero aperture offsets, square pixels and matching aperture
and output aspect ratios. One render settings prim is required. Each USD sphere
light is approximated as a point: its RGB contribution is
`color * intensity * 2^exposure`; its USD area/radius and physical light units
are not simulated. Normalized and color-temperature lights are rejected.
Clipping distances are used along the ray, an approximation of USD camera clip
planes away from the image center. The sphere is a filled volume with constant
white color, not an opaque surface shader. The loader reads default time only.

Run the loader tests with the built module and `pxr` on `PYTHONPATH`:

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python -m unittest discover -s python -p 'test_usd_sphere*.py'
```

Set `PBVFX_RUN_RENDER_TESTS=1` to run the image integration test; it also
requires the OpenImageIO Python bindings and NumPy.


## Python field and noise bindings

The `physics_vfx` module exposes the functions in `field_interface.h` and
`noise_fields.h`, except the templated `funcfield` callback factory. Existing
short names such as `sphere`, `plane`, and `constant` remain available; the
corresponding C++ names (`isf_sphere`, `make_plane`, and `make_constant`) are
also available. `Field`, `VectorField`, and `ColorField` represent scalar,
vector, and color volumes. Their `eval` methods sample world-space positions;
`grad`, finite-difference settings, and `clear_cache` are also exposed.
Vector-field gradients are returned as `Matrix` values (`matrix.get(row, col)`).
The existing C++ color field gradient type is an integer, not a color gradient.

Most field operations accept scalar, vector, or color fields of matching types.
For `scale`, the second argument is a scalar **field** and scales the position
at which the input is evaluated; `scale_fixed` takes a `Vector` instead.
`translate` and `rotate` accept either field-valued arguments or fixed
`Vector`/float arguments. Their `_fixed` variants take fixed arguments directly.
Angles are in radians and use the C++ field operation's rotation convention.

```python
import physics_vfx as fx

origin = fx.Vector(0, 0, 0)
sphere = fx.isf_sphere(origin, 1.0)
moved = fx.translate_fixed(sphere, fx.Vector(2, 0, 0))
print(moved.eval(fx.Vector(2, 0, 0)))  # -1

noise_values = fx.FSVals(0.5, 3.0, origin, 1.0, 2.0)
noise = fx.make_fspn(noise_values)
print(noise.eval(fx.Vector(0.1, 0.2, 0.3)))
```

`FloatGrid(voxel_size, background=0)` and `ColorGrid(voxel_size)` own OpenVDB
grids. `set_value(x, y, z, value)` and `get_value(x, y, z)` use integer voxel
coordinates. Pass either grid to `make_grid_field` to sample it at world-space
positions. Color grids store RGB; sampled alpha is zero.

With the module built and available on `PYTHONPATH`, run the field tests with:

```bash
PYTHONPATH="$PWD/build/python" python -m unittest discover -s python -p 'test_field_bindings.py' -v
```

## Meshes as scalar fields

Use the built Python module to turn an OBJ mesh into a signed-distance field:

```python
import physics_vfx as fx

field = fx.obj_mesh_to_grid_field("models/box.obj", voxel_size=0.05, half_width=3.0)
print(field.eval(fx.Vector(0, 0, 0)))
# To retain the OpenVDB grid instead:
grid = fx.obj_mesh_to_level_set_f("models/box.obj", voxel_size=0.05)
fx.save_to_disk(grid, "outputs/box.vdb")
```

Python OBJ imports preserve the positions in the file by default; use
`recenter=True` to move the vertex centroid to the origin. Existing C++ calls
retain their previous default of recentering. OBJ vertex coordinates and voxel
size use the same units. Faces may be triangles or quads, including negative
OBJ indices; triangulate larger polygons before import. Unsupported or invalid
faces and missing files raise an error.

For USD, add both the built module and the `python/` directory to `PYTHONPATH`.
The `pxr` bindings must be available in that same Python environment:

```python
from usd_mesh import usd_mesh_to_field
import physics_vfx as fx

field = usd_mesh_to_field("scene.usda", voxel_size=0.05,
                          prim_path="/World/Model")
print(field.eval(fx.Vector(0, 0, 0)))
```

If the stage has exactly one mesh, `prim_path` can be omitted. The loader
applies the mesh's composed world transform and the stage's `metersPerUnit`.
Renderer coordinates and `voxel_size` are in meters by default; pass
`volume_meters_per_unit` to use another renderer unit. `half_width` is the
narrow-band half width in voxels, and `time` selects an authored USD time
(default: USD default time). `usd_mesh_to_grid` returns the underlying grid.
`create_volume_from_usd` in `usd_to_pbvfx.py` also recognizes mesh prims; pass
`mesh_voxel_size` and `mesh_half_width` to configure them.

USD mesh conversion currently supports polygon meshes with triangle and quad
faces and `subdivisionScheme = "none"`. Subdivision surfaces, mesh holes, and
larger polygons must be converted to closed triangle/quad meshes first. For a
meaningful signed distance, the surface must be closed. The result is a
narrow-band level set: values are negative inside, positive outside, and
clamped to the band width away from the surface. It is a scalar field rather
than an automatic density or image render.

`CoordBBox`, `world_space_to_bounds`, grid creation, mesh conversion, field
stamping, trilinear deposit and `save_to_disk` are also available in Python.
Bounds are inclusive voxel coordinates. `stamp_grid` and
`stamp_grid_parallel` accept a C++ field; their optional `negative_only` flag
selects negative scalar values. The callback-based C++ templates are not
exposed as arbitrary Python callbacks.

Run the integration tests with the built module and OpenUSD available:

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python -m unittest discover \
  -s python -p 'test_mesh_import.py' -v
```

## General USD scene loading

`python/usd_sphere_scene.py` exposes helpers that accept an open USD stage:
`get_camera`, `get_render_settings`, `get_lights`, `get_isf_object`,
`get_mesh_object`, and `get_scene_objects`. The first three construct native
PhysVFX camera, settings, and point-light objects. `load_scene` combines those
pieces and exposes the objects as well as scalar and color fields:

```python
from usd_sphere_scene import load_scene

scene = load_scene("python/inputs/isf_mesh_scene.usda")
settings = scene.settings                 # fx.RenderSettings
camera = scene.camera                     # fx.Camera
lights = scene.lights                     # list[fx.PointLight]
objects = scene.objects                   # path, kind, field and color
implicit_field = scene.sdf                # union of the object fields
density = scene.density                   # negative inside, zero outside
color = scene.color                       # fx.ColorField
```

The example contains each of the eight `isf_*` types as red tagged prims and
four untagged green meshes. A prim selects an analytic field using a custom
`token pbvfx:isf_type`, such as `"isf_sphere"` or `"isf_torus"`. An untagged
geometry prim must be a USD `Mesh` and is voxelized using the density voxel
size. Tagged meshes are interpreted as their ISF instead. Untagged non-mesh
geometry and unknown ISF tags raise an error.

Fields use local coordinates and the composed USD world transform. Tagged
spheres currently require a uniform, shear-free world scale. Other ISFs
preserve their zero surface and inside/outside sign under affine transforms;
these fields and their union are not necessarily exact world-space distance
fields. ISF parameters can be authored using `pbvfx:radius`, `majorRadius`,
`minorRadius`, `height`, `normal`, `theta` (radians), and `roundingExponent`,
where applicable. A sphere without a custom radius uses its USD Sphere radius.
The example shows the accepted tags. Meshes can override the narrow-band width
with `pbvfx:meshHalfWidth` (in voxels).

A single `primvars:displayColor` value supplies each object's color; otherwise
`pbvfx:color` or white is used. If fields overlap, the first object in stage
traversal order supplies the color. Each prim can set `custom bool pbvfx:stamp = true`
to sample its scalar field into a separate OpenVDB grid, using the render
settings' density voxel size and padded object bounds. The default is `false`,
which leaves analytic ISFs as fields. Meshes always require an initial OpenVDB
level-set conversion; enabling `pbvfx:stamp` resamples that mesh field as well.
The example scene uses both settings. Stamping approximates the field at voxel
resolution; values outside its padded grid use a positive background. Objects
are unioned after this optional conversion, and their fields determine both
density and color. `scene.render(path)` evaluates those combined fields directly,
builds a shadow map for each light, and writes an image. Render bounds
are derived from the world-space mesh points and conservative ISF bounds, with
padding for the voxel sizes. The bounding volume can be inspected through
`scene.bounds_min` and `scene.bounds_max`; ISF bounds account for all supported
analytic shapes and affine transforms. Lights use the existing approximate
point-light and deep-shadow-map model.

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python python/usd_sphere_scene.py \
  python/inputs/isf_mesh_scene.usda --output outputs/isf_mesh_scene.exr
```

Mesh surfaces must be closed for meaningful inside/outside signs. Very small
voxels and large scene bounds can require substantial memory and render time.
Run the optional image test with `PBVFX_RUN_RENDER_TESTS=1`; it also needs the
OpenImageIO Python bindings and NumPy.

Run the new scene tests using the built module and OpenUSD:

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python -m unittest discover \
  -s python -p 'test_usd_general_scene.py' -v
```


## USD field operations

Operation prims select input fields with the ordered `pbvfx:inputs` relationship.
An operation consumes its inputs: only unreferenced fields and operation
results are included in the final scene union. Operations can refer to other
operations, can share inputs, and may set `pbvfx:stamp` to stamp the result.
The result inherits the first input's color unless the operation prim supplies
`primvars:displayColor` or `pbvfx:color`.

```usda
def Xform "Cutout" {
    custom token pbvfx:operation = "cutout"
    custom rel pbvfx:inputs = [</World/Body>, </World/Cutter>]
}
def Xform "Pyro" {
    custom token pbvfx:operation = "pyro"
    custom rel pbvfx:inputs = </World/Body>
    custom float pbvfx:amplitude = 0.35
}
```

`cutout` means first input minus second. Built-in binary operations are
`cutout`, `union`, `intersection`, `add`, and `subtract`; `pyro` takes one
input and calls `fx.make_pyro`. Pyro supports `pbvfx:roughness` (0.5),
`octaves` (3), `frequency` (1), `frequencyJump` (2), `amplitude` (0.25),
`gamma` (1), `iterations` (1), and `noiseTranslation` ((0, 0, 0)).
Amplitude and translation are in stage length units; the existing pyro field
adds a non-negative displacement for non-negative amplitude. The operation
inherits conservative input bounds. Arithmetic operations may extend beyond
those bounds, so they require explicit `point3f pbvfx:boundsMin` and
`pbvfx:boundsMax` in stage world units; the result is clipped to that box. Invalid relationships, unknown
operations, and cyclic graphs raise errors.

Define operations in the same stage as the layout, or put them in a layer
that composes the layout. [`python/inputs/operations.usda`](python/inputs/operations.usda)
sublayers [`operation_layout.usda`](python/inputs/operation_layout.usda) and
adds a cutout and a pyro sphere. Render the composed stage with:

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python python/usd_sphere_scene.py \
  python/inputs/operations.usda --output outputs/operations.exr
```

For other operations exposed by Python, applications can add an entry to
`usd_sphere_scene.FIELD_OPERATIONS` before loading a stage. Each entry is
`(input_count, callback, bounds_policy)`, where `callback(prim, inputs,
unit_scale)` returns a scalar field and `inputs` are `SceneObject` instances.
Use the `explicit` bounds policy and author the bounds above unless the
operation is guaranteed to remain within the union of its input bounds; use
`inputs` only with that guarantee. This is an extension point for arbitrary
Python field operations, not unrestricted execution of names from USD.

## Wisps in Python and USD

`wisp_uniform` (also exposed as `UniformWisp`) creates a seeded generator.
Its `stamp` method deposits dots into a `FloatGrid`; use a negative density
for the renderer's density convention. `CoordinateSys` exposes the generator's
`t`, `n`, and `b` basis vectors, and `FSVals` controls the two noise fields.
`FloatGrid.active_world_bounds()` returns the active voxel bounds or `None`.

```python
import physics_vfx as fx

origin = fx.Vector(0, 0, 0)
noise = fx.FSVals(0.5, 2, origin, 1, 2)
grid = fx.FloatGrid(0.1, 0)
wisp = fx.wisp_uniform(100000, -1, 1, 1, noise, noise,
                       fx.Vector(1, 1, 1), origin, fx.CoordinateSys(), 11)
wisp.stamp(grid)
density = fx.make_grid_field(grid)
```

A USD prim tagged `custom token pbvfx:field_type = "wisp"` generates its
own grid at scene load time. See [`python/inputs/wisps.usda`](python/inputs/wisps.usda):

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python python/usd_sphere_scene.py \
  python/inputs/wisps.usda --output outputs/wisps.exr
```

The prim's composed world transform places the local grid. Its active voxels
and transform determine render bounds. The render setting `pbvfx:densityVoxelSize`
sets the local grid spacing in stage units before the prim transform; scaling
the prim scales the resulting voxels. The wisp is already grid-backed, so
`pbvfx:stamp` does not cause another resampling. Wisp density magnitudes are
preserved in the final density union, using the minimum of overlapping negative
fields; this is not additive blending. A wisp can be referenced by an operation,
but an operation result is interpreted as an implicit field and converted to a
binary density mask, so its original density magnitudes are not retained.

Wisp attributes and defaults:

- `pbvfx:wispDots` (100000), `wispDensity` (1, positive deposit magnitude),
  `wispClump` (1), `wispPscale` (1), and `wispSeed` (1).
- `pbvfx:wispMin` (-1), `wispMax` (1), and `wispDisplacement` ((1, 1, 1))
  configure local sampling and displacement.
- `pbvfx:wispNoise1Roughness` / `wispNoise2Roughness` (0.5),
  `wispNoise1Octaves` / `wispNoise2Octaves` (2),
  `wispNoise1Frequency` / `wispNoise2Frequency` (1),
  `wispNoise1FrequencyJump` / `wispNoise2FrequencyJump` (1), and
  `wispNoise1Translation` / `wispNoise2Translation` ((0, 0, 0)) control the
  two fractal noise fields. All attributes have the `pbvfx:` prefix.
- `primvars:displayColor` or `pbvfx:color` controls the object's color.

Run the binding and USD loader tests with:

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python -m unittest test_wisps -v
```


## Operations and wisps showcase

[`showcase_layout.usda`](python/inputs/showcase_layout.usda) defines the scene,
and [`showcase_operations.usda`](python/inputs/showcase_operations.usda)
composes it and defines union, intersection, cutout, add, subtract, pyro, and
noise-warp operations. Two independently seeded wisps and a bounded plane are
included. To render the composed scene from the repository root:

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python python/usd_sphere_scene.py \
  python/inputs/showcase_operations.usda --output outputs/showcase.exr
```

`pbvfx:field_type = "plane"` creates a plane through the prim's local origin,
using `pbvfx:normal` (default `(0, 1, 0)`). It requires `pbvfx:boundsMin` and
`pbvfx:boundsMax` in stage world units and clips the field to that box.
`pbvfx:operation = "noise_warp"` takes one input and evaluates it at
`p + normal * amplitude * fspn(p)`. Its `roughness`, `octaves`, `frequency`,
`frequencyJump`, `noiseTranslation`, `normal`, and `amplitude` attributes
control the noise; it also requires explicit bounds, which clip the result.
Frequency is cycles per stage unit and amplitude is in stage units. Since the
input plane is itself clipped, its bounds should cover the displaced surface.

## Named stamp caches

Add `custom string pbvfx:cacheName = "my_stamp"` to a USD prim to reuse its
stamped OpenVDB grid across runs. Analytic and operation prims must also set
`custom bool pbvfx:stamp = true`. Wisps and meshes already produce grids, so
they use the name without needing that flag. If a mesh also requests a stamp,
the cache stores the final resampled grid. Without a cache name, a stamp is
computed every time and no cache file is created. Render-time deep shadow maps
are generated per light and are not covered by these per-prim caches.

The loader writes `<name>.vdb` to `cache/` relative to the current working
directory and loads it on subsequent runs. Names must be unique among cached
prims in a stage and use letters, digits, `_`, `-`, or `.`, beginning with a
letter or digit. Cache writes are atomic. The showcase assigns names to one
sphere and one wisp; the other stamps remain uncached.

```bash
PYTHONPATH="$PWD/build/python:$PWD/python" python python/usd_sphere_scene.py \
  python/inputs/showcase_operations.usda --output outputs/showcase.exr \
  --cache-dir outputs/stamp_cache
```

Python callers can use `load_scene(path, cache_dir="outputs/stamp_cache")`.
`SceneObject.cache_hit` records whether the grid was loaded from cache.
`physics_vfx.load_float_grid(path)` also reads a single-float-grid VDB file.

The name is the cache key: the loader does **not** detect changes to geometry,
transforms, operation inputs, voxel size, or wisp settings. Delete the relevant
`.vdb`, use a new name, or choose a different cache directory after such a
change. Use separate cache directories for unrelated scenes to avoid reusing a
name for different content. A corrupt or incompatible cache raises an error;
remove it to regenerate the stamp.
