# PhysVFX tools for Houdini Solaris

The PhysVFX shelf creates five parameterized **Python Script LOPs**. They use
Houdini's OpenUSD bindings to author the attributes and relationships read by
`python/usd_sphere_scene.py`. Houdini does not need the `physics_vfx` extension
to author a stage; build it in the renderer environment to render the exported
USD. The tools are shelf-created nodes, not prebuilt HDAs or TAB-menu entries.

## Start Houdini with the tools

Check out the `houdini_plugin` branch, open a terminal at the repository root,
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
| **PhysVFX Field** | Select an existing prim or create one. Supports all eight analytic ISF types, a USD mesh, bounded plane, and wisp. Authors `pbvfx:isf_type` or `pbvfx:field_type`; mesh mode requires an existing `UsdGeomMesh` and leaves it untagged. |
| **PhysVFX Operation** | `union`, `intersection`, `cutout`, `add`, `subtract`, `pyro`, and `noise_warp`. Authors `pbvfx:operation` and the ordered `pbvfx:inputs` relationship. Add, subtract, and noise warp clip to authored world-space bounds. |
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
is local; the noise-warp direction and bounds are in stage world coordinates.
Positive pyro amplitude expands the field; negative amplitude contracts it.
The automatic bounds for expanding pyro are an estimate, so enable **Use
Explicit Pyro Bounds** and set **Bounds Minimum/Maximum** in world space when
the result is clipped or when reliable bounds are needed. Those controls also
apply to planes and the three bounded operations. They are not Houdini
transform handles. Clear or rename any stamp cache after changing parameters.

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
- Sphere transforms must be uniform and shear-free. USD stage `metersPerUnit`
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
Existing world-space bounds for planes and arithmetic operations and **Use
Explicit Pyro Bounds** keep their original meaning and can further clip the
field independently of stamp bounds.

Existing Python LOPs keep their old spare parameters. After restarting Houdini,
select one or more PhysVFX nodes and click **Refresh PhysVFX Parameters** on the
PhysVFX shelf. Missing controls are added in a **PhysVFX Updates** tab without
resetting existing values. This also exposes any older missing controls, such
as **Use Explicit Pyro Bounds**. Newly created nodes already contain them.

### Shadow map regions

Use **PhysVFX Shadow Region** in Solaris to place one or more boxes around the
volumes that need shadow maps. Each node authors an Xform with local minimum and
maximum bounds and a viewport-only orange guide. Place/transform the prim with
normal USD layout tools. The renderer uses the world-axis-aligned box enclosing
the transformed local box, so a rotated guide can enclose a larger region.

Without authored regions, the renderer uses its normal scene-wide shadow map.
With regions present, cover every volume that should receive or cast shadows;
outside all regions the shadow lookup returns unattenuated light. Every light
gets a map in each region. Rays integrate through the union of all regions,
including separate regions farther toward the light; gaps are skipped and
overlaps are counted once. Regions do not crop the main camera render. This
only limits the CPU USD renderer's shadow-map work, not the CUDA render paths.
