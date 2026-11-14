# PhysVFX tools for Houdini Solaris

The PhysVFX shelf creates parameterized **Python Script LOPs**. They use
Houdini's OpenUSD bindings to author the attributes and relationships read by
`python/usd_sphere_scene.py`. Houdini does not need the `physics_vfx` extension
to author a stage; build it in the renderer environment to render the exported
USD. The tools are shelf-created nodes, not prebuilt HDAs or TAB-menu entries.

## Start Houdini with the tools

Check out the project branch, open a terminal at the repository root,
and launch Houdini from that shell (Linux/macOS):

```bash
export HOUDINI_PATH="$PWD/houdini:${HOUDINI_PATH:-&}"
export PYTHONPATH="$PWD/houdini/python${PYTHONPATH:+:$PYTHONPATH}"
houdini
```

The `&` preserves Houdini's default search paths when `HOUDINI_PATH` was not
already set. The first path makes `toolbar/physvfx.shelf` discoverable; the
second makes the module used by the shelf and nodes importable. Keep both paths
available when reopening a HIP file containing these nodes. Adapt the path
separator and environment-variable syntax for Windows. For persistent setups,
set the same paths using your facility's Houdini package/environment setup.

In Solaris, open a LOP network editor. Add **PhysVFX** from the shelf tabs menu
(the `+` at the end of the shelf row) if it is not already visible. Select the
upstream LOP in that network, then click a PhysVFX shelf tool. It creates a
Python Script LOP, connects the selected node as input, displays the result,
and selects the new node. If nothing is selected, it creates an unconnected
node. The parameters are spare parameters grouped into tabs. Parameters for
other field/operation types remain visible but are ignored by the selected
type; the tables below describe when they apply. The node's built-in **Python
Code** should be left intact. Errors are reported on the LOP.

These tools use USD prim paths, not Houdini node paths or Solaris selection
patterns. Type or copy absolute paths such as `/World/Body`. The operation
inputs must already exist in the upstream LOP stage. Create Digital Asset can
be used on a configured Python LOP if you want a local, shareable HDA; the
`physvfx_houdini` Python module must still be installed wherever it cooks.

## An example workflow

### Wisp spline

1. Select the upstream Solaris node and click **PhysVFX Wisp Spline** on the
   shelf. The node creates `/World/WispSpline` and a viewport-only blue line
   with a cross at each guide. The first two guide positions start at `(0,0,0)`
   and `(0,2,0)` in the spline prim's local coordinates.
2. Under **Guide Particles**, use the multiparm controls to add guides and edit
   their **Position** values. Adjacent positions must differ. A Transform LOP on
   the spline prim moves the whole spline; **Parent Prim Path** can place it
   underneath an existing prim.
3. Leave **Guide Values** set to **Overall Ranges and Ramps**. In **Spline
   Shape**, **Spline Noise 1**, and **Spline Noise 2**, each property has a
   minimum, maximum, and float ramp. At each position, ramp 0 means minimum
   and ramp 1 means maximum; values outside this range are clamped. Vector
   properties use one ramp to mix their minimum and maximum vectors. The
   Position Offset ramp displaces the base guide path; the displayed line shows
   its sampled result. Ramp positions are normalized by distance along the base
   connected segments before offsets are applied.
   **Shape Ramp Sample Offset** shifts and wraps all ramps in the Spline Shape
   tab (including Position Offset), but not the Spline Noise tabs. Zero and
   whole-number offsets preserve the original ramp, including its endpoint;
   0.5 makes the first guide sample the midpoint. Animate the offset to move
   the pattern; negative and greater-than-one values wrap too. For a seamless
   loop, match each ramp's start and end values and slopes. The control is
   ignored in Per Guide mode. Refresh existing Wisp Spline nodes to add it.
4. **Ramp Samples per Segment** controls how many intervals capture the ramp
   between artist guides (default 8). The resolved guide values are written to
   USD, so Houdini itself is not needed by the renderer. For direct values
   instead, choose **Per Guide** and edit the controls in each guide instance;
   the range and ramp controls are then ignored. Dot count and seed apply to
   the whole spline; the underlying spline does not use the per-guide ID or
   dot-count members.
5. Add a camera, light, and render settings, then use Render and View. Wisps
   always stamp a density grid, using the render setting's density voxel size
   unless **Override Density Voxel Size** is enabled. A nonempty **Stamp Cache
   Name** saves and reuses a VDB; delete it or use a new name after any edit.

Wisp dots also deposit density-weighted RGB into an aligned color grid, which
is divided by density after stamping. The eight neighboring voxels receive the
same trilinear weights for both grids. A regular Wisp uses its object color.
For a Wisp Spline, **Color Tint Minimum**, **Color Tint Maximum**, and the
**Color Tint Ramp** vary the dot color along the spline; these tints multiply
the spline's object color. In Per Guide mode each guide has a Color Tint.
Refresh existing spline nodes after restarting Houdini to add the controls.
Set the object color to white to use the tint colors directly. The color grid
uses the density grid's voxel size and stores RGB, not alpha. When different
fields overlap, their colors are mixed according to their sampled densities.

Wisp cache files now store both density and color grids. An older density-only
wisp cache is rebuilt in place the next time it is used; explicitly named
caches must still be cleared or renamed after changing density or color. Build
the Python extension again after updating.

For Python callers, `UniformWisp.stamp_colored(density, color)` adds weighted
RGB and density into existing, identically transformed grids. Accumulate all
the wisps first, then call `normalize_wisp_colors(density, color)` once. Do not
add more wisps to that already-normalized accumulator. A spline's
`stamp_colored()` returns its density and normalized color grids as a pair.
   The guide geometry is excluded from the renderer. Rebuild the Python extension
   and restart Houdini after pulling the spline changes.

The spline path is a polyline between guides, not a smooth spatial curve. The
independent frame vectors are interpolated individually; if changing their
ranges, keep the frame vectors nonzero and choose compatible orientations.
The current generator's Particle Scale multiplies the T-axis component, as in
the existing standalone wisp implementation.

1. Create a **PhysVFX Camera** at `/World/Camera`. Use a normal Transform LOP to
   place it, for example at `(0, 0, 8)` looking along local `-Z`.
2. Create two **PhysVFX Field** LOPs at `/World/A` and `/World/B`, each set to
   **ISF Sphere**. Use a Transform LOP to move `/World/B` sideways. Select the
   relevant upstream LOP before creating each new tool.
3. Create a **PhysVFX Operation** LOP at `/World/Cut`. Choose **Cutout**, with
   input A `/World/A` and input B `/World/B`. The renderer subtracts B from A.
4. Add a **PhysVFX Field** of type **Wisp**, and a **PhysVFX Light**. Place the
   light using a Transform LOP, for example at `(3, 4, 5)`.
5. Add **PhysVFX Render Settings**, referencing `/World/Camera`, then use
   Houdini's USD ROP to write the composed stage, for example
   `outputs/houdini_scene.usda`. If exporting separate layers, preserve the
   layer references and paths, or export a flattened composed stage.
6. In the renderer environment, from the repository root:

   ```bash
   PYTHONPATH="$PWD/build/python:$PWD/python" python python/usd_sphere_scene.py \
     outputs/houdini_scene.usda --output outputs/houdini_scene.exr \
     --cache-dir outputs/stamp_cache
   ```

The script reports elapsed time as it opens the stage, creates or reuses each
field grid, builds each light's shadow map, ray marches, and writes the image.
The final total covers the complete command; the render total includes native
render preparation, shadow maps, ray marching, and image output. Rebuild the
Python extension after updating the native renderer to get its timing messages.

Field and operation metadata can also be authored in another Solaris layer
that sublayers or references the layout. Operation targets refer to the
**composed stage's** absolute prim paths. Referenced inputs are consumed by an
operation before the final scene union; a cutter is not rendered independently
unless it is represented by a separate prim. Operations can be chained.

## Nodes and USD attributes

| Shelf tool | Parameters and authored USD |
| --- | --- |
| **PhysVFX Field** | Select an existing prim or create one. Supports all eight analytic ISF types, a USD mesh, bounded plane, unbounded plane, and wisp. Authors `pbvfx:isf_type` or `pbvfx:field_type`; mesh mode requires an existing `UsdGeomMesh` and leaves it untagged. |
| **PhysVFX Wisp Spline** | Authors a guide path and sampled per-guide spline parameters, driven by overall ranges and ramps or manual values. |
| **PhysVFX Shadow Region** | Creates a shadow region Xform and a red viewport guide. |
| **PhysVFX Render and View** | Passes the scene through; saves a composed USDA, runs the renderer in the background, then opens the image viewer. |
| **PhysVFX Shadow Cache** | Targets an existing region, saves its per-light shadow maps on demand, and enables disk-cache loading. |
| **PhysVFX Operation** | `union`, `intersection`, `cutout`, `add`, `subtract`, `pyro`, `noise_warp`, and `terrain_displace`. Authors `pbvfx:operation` and the ordered `pbvfx:inputs` relationship. Analytic operation fields are not clipped to operation bounds. |
| **PhysVFX Camera** | Creates or edits a USD perspective Camera, authoring focal length, horizontal/vertical aperture, near/far clipping and zero aperture offsets. A toggleable green guide shows the near/far ray-distance boundaries. Place it with Houdini's transform tools. |
| **PhysVFX Light** | Creates or edits a USD SphereLight's color, intensity, exposure, and optional `pbvfx:shadowKappa`. Place it with Houdini's transform tools. |
| **PhysVFX Render Settings** | Creates or edits a USD RenderSettings prim, its camera relationship, resolution and square-pixel aspect, plus all `pbvfx` ray-step, extinction, shadow and voxel controls used by the loader. |

Field controls include `radius`, `majorRadius`, `minorRadius`, `height`,
`normal`, `theta` (radians), `roundingExponent`, `meshHalfWidth`, `boundsMin`,
`boundsMax`, `stamp`, and `cacheName`, all under the `pbvfx:` namespace.
For a USD Sphere, leave **Override Sphere Radius** off to use the native USD
radius. If a stronger upstream `pbvfx:radius` is already authored, remove it
upstream or enable the override. To use a different field type on an existing
prim, remove incompatible upstream PhysVFX tags first; the tools do not silently change conflicting tags
on composed layers. A mesh must be a closed triangle/quad polygon mesh with
`subdivisionScheme = "none"` for a useful signed distance.

Both plane types show a blue grid on the plane when **Show Bounds Preview** is
enabled. **Plane Grid Half Size** changes only the viewport guide, in prim-local
units. Its orientation follows **Normal** and its position follows the prim
transform. The bounded plane still clips to its world-space bounds. Select
**Plane (Unbounded)** for a true infinite cutting field: use the plane as Input B
of Cutout, or use it in Intersection with a bounded field. A renderable result
must have finite bounds. To stamp an unbounded plane, enable **Stamp Field** and
**Use Specified Stamp Bounds**. The grid is a tagged viewport guide, excluded
from rendering. To add the new menu option to existing Field nodes, select them
and use **Refresh PhysVFX Parameters** after restarting Houdini.

To parent a new field, set **Parent Prim Path** to an existing upstream prim,
and set **Prim Path** to a path with the desired child name. For example,
parent `/World/Parent` and prim path `/World/Child` create
`/World/Parent/Child`. The child begins at the parent's local origin and
inherits its transform; use a Transform LOP on the child for an offset. Operation
inputs should reference the resulting full child path. Leave Parent Prim Path
blank for the original absolute-path behavior. This creates or edits the child
path; it does not move or remove an existing prim at the original path.
Refresh existing Field nodes to add the control. Clear or rename stamp and
shadow caches after moving fields. Bounded planes use world-space bounds,
which do not automatically follow their parent.

Analytic surface previews update when their applicable radius, height, normal,
cone angle, or box rounding controls change. Editing a sphere's **Radius** in
the UI enables **Override Sphere Radius**; disabling the override instead uses
the upstream USD sphere radius. Existing field nodes need **Refresh PhysVFX
Parameters** for this automatic toggle. The native USD sphere is updated when
overridden so its viewport surface and PhysVFX field agree. **Mesh Half Width**
controls the level-set voxelization band; it does not alter the source mesh's
viewport surface. Analytic proxy meshes are low-resolution layout previews.

The wisp tab provides `wispDots`, `wispDensity`, `wispClump`, `wispPscale`,
`wispSeed`, `wispMin`, `wispMax`, `wispDisplacement`, and both sets of
`wispNoise1*` / `wispNoise2*` roughness, octaves, translation, frequency, and
frequency-jump controls. Wisps are always grid-backed. Setting **Stamp Field**
on an analytic field or operation samples it to a grid; a nonempty **Stamp
Cache Name** enables a reusable cache. Meshes and wisps use their cache name
without the stamp toggle. Empty names disable caching, including an upstream
cache name. Cache names must be unique in a scene. Cached grids are not
invalidated automatically when the scene changes: delete the cache or change
its name when the source or settings change.

Noise operations expose `roughness`, `octaves`, `frequency`, `frequencyJump`,
`noiseTranslation`, and `amplitude`. Pyro additionally uses `gamma` and
`iterations`; noise warp uses `normal` as its direction. The normal for a field
is local; the noise-warp direction is in stage world coordinates.
For arbitrary analytic fields or USD meshes, choose **Terrain Displace** on a
PhysVFX Operation and set Input A to the field or mesh prim. The nearest-point
iterations and positive/negative terrain controls shape the displacement;
Amplitude multiplies both terrain amplitudes. Meshes use a sampled level set,
so voxel resolution and mesh half width affect the result. Clear old stamp or
shadow caches after edits. Refresh existing operation nodes to add the menu
item and iteration control; rebuild the Python extension after updating.
For terrain, select **Fractal Noise Warp** on a PhysVFX Operation, connect a
bounded plane as Input A, and choose **Fractal Sum Terrain** under **Warp Noise
Type**. The positive and negative terrain amplitudes and gammas shape the two
halves of the noise independently; gamma transitions linearly from 1 at zero
to its selected value at the positive **Terrain Gamma Transition** magnitude.
The regular warp Amplitude scales the resulting displacement. Existing scenes default to Fractal Sum Perlin. For an existing
operation node, select it and use **Refresh PhysVFX Parameters** on the shelf
to add the new controls.

Python exposes `make_fstn(values, amp_pos, amp_neg, gamma_pos, gamma_neg,
t_point)`, where `values` is an `FSVals`. Amplitudes must be non-negative;
gammas and the transition magnitude must be positive. Rebuild the Python
extension after updating the source.
Positive pyro amplitude expands the field; negative amplitude contracts it.
Operations are evaluated between the camera near and far distances without an
operation clipping box. This may take longer than an accelerated bounded render.
Automatic shadow regions and implicit stamp grids still use estimated extents;
use a shadow region or specified stamp bounds if those estimates are too small.
Clear or rename caches after changing parameters.

**Color Authoring** can inherit upstream color, write a constant USD
`primvars:displayColor` (also useful for the viewport), or write
`pbvfx:color`. The renderer prefers `displayColor` over `pbvfx:color`; the
node rejects the latter mode when a display color already has a value upstream.
Operations inherit their first input's color unless a color mode is selected.
An override left off preserves any value authored on an upstream layer; it
does not remove an existing upstream color or light shadow-kappa override.
Remove the upstream opinion if you want the renderer's default. Cache Name is
the exception: an empty name is explicitly authored to disable an upstream key.

## Renderer and preview constraints

- The importer reads the default USD time, not an animation frame. Node
  parameters author default values; use this workflow for static scene exports.
- Use exactly one RenderSettings prim and at least one SphereLight. The camera
  must be perspective, have zero aperture offsets and square pixels, with
  aperture aspect matching output resolution. The tools' camera and settings
  defaults match a 960×540 render.
- The camera's green range guide depicts distances along rays, with curved
  near/far boundaries; it is not the scene's volume grid bounds. It follows
  the camera prim, so use unit camera scale for the guide to match the renderer,
  which ignores camera scale. **Show Camera Range** hides the guide; existing
  camera nodes use the default after restart but need recreation for the toggle.
- Spheres support non-uniform scale and other invertible affine transforms;
  their transformed fields are not necessarily exact world-space distances.
  USD stage `metersPerUnit`
  is respected by the renderer. Length parameters and authored bounds are in
  stage units, except voxel half-width (in voxels) and other dimensionless
  settings. Camera focal length and aperture follow their usual USD camera
  conventions.
- The current renderer treats SphereLights as point lights. It rejects USD
  light normalization and color temperature; disable them upstream. Its use of
  display color is separate from material shading.
- Houdini's viewport does not execute the PhysVFX volume renderer. Each Field
  node creates a visible orange wireframe bounds proxy beneath its field prim at
  `__physvfx_bounds`. Analytic ISFs also have a low-resolution surface proxy
  at `__physvfx_surface` (a native USD sphere supplies its own surface when
  its radius is not overridden). The surface uses the ISF's parameters and
  local normal, and both proxies follow the prim's transform. They are layout
  approximations, not the final render; fine or thin details may be missed.
  The renderer ignores both via their `pbvfx:preview` tag. Existing USD meshes
  also remain visible. **Show Bounds Preview** controls both proxies. Existing
  nodes made with older versions use the preview defaults after restarting
  Houdini; create a new Field node if you want the new controls on its parameter
  interface.
- Wisp bounds are estimated before stamping; **Wisp Preview Half Size** can
  override the estimate in local units. Actual wisp render bounds come from
  the stamped grid. Plane bounds are authored in world coordinates: apply its
  layout transform upstream of the Field node so its preview reflects those
  bounds. Other fields' wireframe boxes follow their parent transforms.
- Operation Xforms do not yet display the resulting CSG or pyro field. The
  source fields' bounds can be used to lay out the operation. Other USD
  renderers may display the preview geometry; `pbvfx:preview` is interpreted by
  PhysVFX's scene loader, not a standard USD rendering exclusion.
- PhysVFX-only object types must have compatible tags: an operation gets its
  own Xform, separate from its source fields. Existing USD prims are not
  deleted or retyped by the authoring tools. For unused inputs, avoid stray
  untagged non-mesh geometry because the renderer rejects it.

## Tests

Run dependency-free interface and authoring contract tests with stand-ins:

```bash
PYTHONPATH="$PWD/houdini/python" python -m unittest discover -s houdini/tests -v
```

Run the same suite inside a headless Houdini environment for actual OpenUSD and
LOP tests:

```bash
PYTHONPATH="$PWD/houdini/python:$PWD/python:$PWD/build/python${PYTHONPATH:+:$PYTHONPATH}" \
  hython -m unittest discover -s houdini/tests -v
```

The renderer integration test runs only when `physics_vfx` can be imported by
that Python interpreter. Houdini's Python version must match the version used
to build the extension; otherwise run the exported scene with the separately
configured renderer Python. The headless LOP smoke test skips in an interactive
Houdini session and cleans up the nodes it creates.

### Per-object stamping

Field and Operation nodes have **Override Density Voxel Size**, **Object Density
Voxel Size**, **Use Specified Stamp Bounds**, **Stamp Bounds Minimum/Maximum
(Local)**, and **Show Stamp Bounds (Red)** in their main tab. The voxel size is
in stage units; without an override the render setting is used. It applies to
field stamps, mesh voxelization, and wisp grids. Named caches are not
invalidated when settings change: delete or rename the cache after edits.

For an analytic field or operation, turn on **Stamp Field** and **Use Specified
Stamp Bounds**. Bounds are offsets in the prim's local coordinate system, with
fixed initial values of -2 and 2; they are not automatically fitted to the
shape. The loader transforms the eight corners into world space and uses their
axis-aligned enclosing box to clip and stamp the result. Rotation or scale can
therefore enlarge that world-aligned box. The red guide shows that box; it is a
viewport-only proxy and the loader excludes it. Two extra voxels of padding are
sampled outside the displayed box for interpolation. Set the prim's transform
upstream of the PhysVFX authoring node so the guide recomputes when it changes.
Operation prims often start at the world origin; their local bounds are relative
to the operation prim, not automatically relative to an input field.

Explicit stamp bounds apply to analytic/operation stamps and optional mesh
resampling, not the initial mesh-to-level-set conversion or wisp deposition.
The loader reports an error if specified bounds are enabled for an unstamped
field or a wisp. Bounds clip the result, so leave room for displacement.
Bounded planes retain their own world-space clipping bounds. Legacy operation
bounds and **Use Explicit Pyro Bounds** are ignored for analytic operations.

Existing Python LOPs keep their old spare parameters. After restarting Houdini,
select one or more PhysVFX nodes and click **Refresh PhysVFX Parameters** on the
PhysVFX shelf. Missing controls are added in a **PhysVFX Updates** tab without
resetting existing values. This also exposes any older missing controls, such
as stamp bounds. Refresh also hides obsolete operation bounds controls without
deleting their saved values. Newly created nodes already contain current controls.

### Shadow map regions

Use **PhysVFX Shadow Region** in Solaris to place one or more boxes around the
volumes that need shadow maps. Each node authors an Xform with local minimum and
maximum bounds and a viewport-only red guide. Place/transform the prim with
normal USD layout tools. The renderer uses the world-axis-aligned box enclosing
the transformed local box, so a rotated guide can enclose a larger region.

Without authored regions, the renderer uses its normal scene-wide shadow map.
With regions present, cover every volume that should receive or cast shadows;
outside all regions the shadow lookup returns unattenuated light. Every light
gets a map in each region. Rays integrate through the union of all regions,
including separate regions farther toward the light; gaps are skipped and
overlaps are counted once. Regions do not crop the main camera render. This
only limits the CPU USD renderer's shadow-map work, not the CUDA render paths.


### Shadow region disk caches

Create a **PhysVFX Shadow Cache** LOP downstream of a **PhysVFX Shadow Region**
LOP; it is a separate node and authors cache settings onto the target region.
When the region node is selected, the shelf tool fills in its prim path. If
adding the cache after a merge, set **Shadow Region Prim Path** manually. Add a
cache node per region, and give every region a distinct USD prim path.

1. In the cache node, set **Shadow Cache Directory** to an absolute writable
   directory and, if necessary, set **Renderer Python** and **Renderer Script**
   to the Python and `python/usd_sphere_scene.py` used for rendering.
2. Set the display flag on the final scene LOP after all geometry, regions,
   cache nodes, lights, camera and render settings, or enter its absolute path
   in **Final Scene LOP**. The final stage must include this cache node.
3. Click **Save to Disk**. It exports a temporary composed stage and computes a
   grid per light for this region without rendering an image. Progress and any
   subprocess errors go to the Houdini console; Houdini waits for it to finish.
4. Enable **Load from Disk** and export/render the final stage as usual. Missing
   or incompatible requested caches produce an error. With the toggle off,
   rendering builds the maps in memory and does not save them.

The external renderer must have OpenUSD and the built `physics_vfx` module.
For **Renderer Python**, specify the interpreter executable (such as `python3`
or a virtual environment's `bin/python`), not the `physics_vfx*.so` module. The
button prepends the repository's `build/python` and `python` to `PYTHONPATH`.
Set **Stamp Cache Directory** to match `--cache-dir` in the normal render. The
USD loader uses its default time; this is not an animated frame cache. Each save
creates a new generation and atomically replaces the manifest only after all
grids are written; old generations may be removed manually.

The manifest checks all region bounds, light positions and shadow coefficients,
shadow voxel size and shadow step. **Changes to geometry, field operations or
stamp caches are not automatically detected.** Re-save each affected region
when these change. A region can shadow another region, so edits may require
re-saving multiple caches. With no authored regions, the usual scene-wide map
is built and no explicit region cache is used.

Existing PhysVFX LOPs retain their old interfaces. Restart Houdini and use the
**PhysVFX Shadow Cache** shelf tool to create the new cache node. If an existing
region LOP has an empty Python Code parameter, select it and use **Refresh
PhysVFX Parameters**; this restores an empty generated script without replacing
custom code. Recreate the region node if it lacks PhysVFX node metadata.


### Point noise stamps

Import the scattered SOP points into Solaris as a USD Points prim, then select
the scene LOP and create **PhysVFX Point Noise** from the shelf. Set **Point
Cloud Prim Path** to that Points prim. Each point stamps fractal-sum Perlin
noise multiplied by `make_fade_factor` into one sparse density grid. The grid
uses the scene density voxel size unless overridden. Overlapping negative
noise values add; positive noise is discarded because PhysVFX represents smoke
density with negative values. All points share the same world-space noise.

The node's defaults are overridden by USD point primvars named `radius`,
`fade`, `density`, `frequency`, `roughness`, `octaves`, and `frequency_jump`.
Each must be a float scalar, a constant one-element array, or one value per
point; indexed primvars are flattened. Radius and frequency use local point
cloud units. Fade, radius, frequency, octaves and frequency jump must be
positive; density and roughness may be zero. Non-uniform point-cloud transforms
are not supported. An optional stamp cache is explicitly named, so delete or
rename it after changing the points or their attributes; live preview bypasses
the cache. Rebuild the Python extension after updating.

For example, before SOP Import put an Attribute Wrangle after Scatter and
connect the volcano's polygon geometry to its second input, in the same space:

```c
float d = xyzdist(1, @P);
float t = clamp(d / 20.0, 0.0, 1.0);
f@radius = 1.0;
f@fade = lerp(0.3, 2.0, t);
f@density = lerp(3.0, 0.3, t);
f@frequency = fit01(rand(@ptnum + 15), 0.8, 1.2);
f@roughness = 0.5;
f@octaves = 3.0;
```

Ensure SOP Import exports these point attributes as USD primvars. Changing the
scatter topology may change `@ptnum` and its random values.

### Render and view

Select the final scene LOP and create a **PhysVFX Render and View** node from
the shelf. Set **USD Save Path** (a `.usda` file) and **Image Save Path**, then
click **Render and View**. The node exports its input stage as a flattened USD
and starts `python/usd_sphere_scene.py` in a background process. Houdini stays
responsive. The renderer's progress and errors appear in the Houdini console;
an unexpected worker exit is reported there with its exit code or signal.
With progressive preview off, the node opens `dpa_imgviewer -image <image>`
after a successful render. A failed render does not replace an existing image;
with progressive preview on, a viewer may already be displaying partial work.

The default viewer is `build/DPA_ImageProcessing/dpa_imgviewer`. Set **Image
Viewer** if your build puts it elsewhere. Set **Renderer Python** to the Python
interpreter executable with OpenUSD (for example `python3` or
`/path/to/venv/bin/python`). Do not enter `physics_vfx.cpython-*.so`: that is a
module imported by Python, not an executable. The interpreter's Python version
must match the built extension. Set **Renderer Script** if the Houdini plugin
is installed outside the repository. The node
adds `build/python` and `python` under the renderer's repository to
`PYTHONPATH` and removes Houdini's `PYTHONHOME` from the external process so
that the selected interpreter can locate its own standard library. Houdini's
environment is not changed. The interpreter still needs compatible OpenUSD and
PhysVFX bindings. **Stamp Cache Directory** is passed to the USD renderer; shadow
cache settings are read from the scene. Relative stamp cache directories are
resolved from the renderer repository. Build the viewer and extension first:

```bash
cmake -S . -B build
cmake --build build --parallel
```

Enable **Progressive Preview** to open one viewer window after the first pass.
The CPU renderer publishes ten passes at 10%, 20%, …, 100% of the image's
width and height. Every published image retains the final dimensions. Missing
pixels are filled by bilinear interpolation from neighboring sampled pixels,
including alpha; true sampled values (including transparent black) are kept.
Samples from earlier passes are reused, and interpolation is applied only to a
separate preview, never to the accumulated samples or final render. The viewer
watches one temporary preview file and refreshes automatically. The final image
path is replaced only after the complete render succeeds. The preview file is
removed after the viewer closes.

For a manually started progressive render, scene loading, field stamping and
shadow-map creation happen before the first pass, so complex scenes may still
take time to show the first image. Shadow-region caches can reduce repeated
shadow-map work. A custom **Image Viewer** must support `--watch`.

To render a sequence, enable **Render Frame Range** on the **Frame Range** tab
and set Start Frame, End Frame and Frame Increment. End is inclusive. The node
exports one composed USDA per frame before starting a background worker, then
renders the frames in order and opens the last completed image. The export
step runs in Houdini and may take time; the original timeline frame is restored
afterwards. Animated USD values are sampled at each frame and copied to default
values in that frame's snapshot because the renderer reads default values.

Include `$F4` in the USD and image paths for explicit frame numbering. If the
paths do not produce distinct names, the node inserts a four-digit frame
number before the extension (for example `render.0001.exr`). The range uses
fresh temporary stamp caches for each frame and ignores saved shadow-map caches
to prevent stale animation. Sequence rendering does not use the single-frame
progressive viewer; the viewer opens once, after the final frame. Existing
Render and View nodes need **Refresh PhysVFX Parameters** to add the controls.

Click **Render and View** again on the same node to stop its current render and
viewer before starting a new render from the latest scene. If a render is
cancelled, the previous completed image stays on disk until the replacement
finishes. Different Render and View nodes can still run concurrently; use
different USD and image paths for those nodes. Single-frame rendering uses the
USD loader's default time.

### Continuous preview

Enable **Continuous Preview** on the Render and View node to open an updating
viewer without pressing the render button. After changes to scene parameters,
connections or the current frame, the node exports a new temporary scene and
replaces any in-progress preview. Rapid changes are combined using **Delay
After Edit**. The viewer is reused for progressive updates and the last image
stays visible while the next scene is prepared. If you close the viewer it will
reopen when another preview image is available. Disable the toggle to stop the
preview and close its viewer.

The **Continuous Preview** tab offers quality and speed controls. **Resolution
Scale** scales width and height independently (the default 0.25 renders 1/16 as
many pixels); **Ray and Shadow Step Multiplier** increases sampling distances;
**Grid Voxel Size Multiplier** coarsens grids; and **Wisp Dot Fraction** reduces
the number of stamped dots. Wisp density is approximately adjusted to offset
the lower dot count and coarser voxels, so preview brightness and fine detail
may differ from the full render. Set these multipliers and fractions to 1 for
full-quality sampling. A complex scene can still take time to rebuild its
fields and shadow maps before the first pass.

Continuous preview bypasses named stamp and shadow caches so edits are visible;
it does not overwrite those caches, the node's saved USD, or its final image.
The **Render and View** button still produces the usual full-quality output.
The viewer and Python settings are shared with that button. As with the normal
renderer, time-sampled USD data is read at the USD default time.

After updating, restart Houdini and select an existing Render and View node,
then click **Refresh PhysVFX Parameters** on the PhysVFX shelf. Its new controls
appear in **PhysVFX Updates**; newly created nodes have a **Continuous Preview**
tab. The installed viewer must support `--watch`.
