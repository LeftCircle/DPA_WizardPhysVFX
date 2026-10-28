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
