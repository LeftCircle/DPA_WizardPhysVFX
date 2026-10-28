# Hw 2: Wisps

Videos for wisps, pyro, and noise are in 
images/wisps -> wisps
images/pyro_wedges -> pyro
images/wedges -> noise

look for .mov files for the videos!

noise parameters:
std::vector<float> mins = {1, 0.01, 0.00, 0.01, 1e-6};
std::vector<float> maxes = {8, 4, 1, 5, 3};
std::vector<float> defaults = {2.5, 1.5, 0.5, 2, 0};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"octave", "fade", "roughness", "freq", "fjump"};

pyro parameters:
std::vector<float> mins = {1, 0.01, 0.01, 1e-6, 0.01};
std::vector<float> maxes = {8, 5, 5, 5, 5};
std::vector<float> defaults = {3, 1, 2, 2, 2};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"octave", "gamma", "freq", "fjump", "amp"};

wisp parameters: (for wisp_out_v02.mov)
std::vector<float> mins = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.1, 0.1};
std::vector<float> maxes = {10, 10, 10, 10, 5, 5, 5, 5, 5, 1, 1};
std::vector<float> defaults = {0.75, 1, 3, 5, 2, 1, 2, 1.5, 1, 0.1, 0.2};
std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp};
std::vector<std::string> names = {"clump", "pscale", "o1", "o2", "f1" "f2", "fj1", "fj2", "dips", "rough1", "rough2"};


Most of the code is in in the /include and /src folders.


### Some extra info
The project is built with cmake, hence the build folder and all that goes with it. The third_party folder
currently contains the code for the image processing library from Computer Graphics Images, along with 
the starter kit code. 






## Render a USD sphere

The CPU renderer can load one `UsdGeomSphere`, one `UsdLuxSphereLight`, and
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
and output aspect ratios. One render settings prim and one sphere light are
required. The light is approximated as a point: its RGB contribution is
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
