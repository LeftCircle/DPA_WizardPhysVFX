"""Parameter descriptions for the PhysVFX Solaris shelf tools.

This file is independent of Houdini, USD, and the PhysVFX extension, so the
node interfaces and USD authoring can be tested outside the application.
"""
from dataclasses import dataclass
from .spline_channels import CHANNELS


@dataclass(frozen=True)
class Parameter:
    name: str
    label: str
    kind: str
    default: object
    help: str = ""
    choices: tuple = ()


def P(name, label, kind, default, help="", choices=()):
    return Parameter(name, label, kind, default, help, choices)


FIELD_KINDS = (
    ("isf_sphere", "ISF Sphere"), ("isf_torus", "ISF Torus"),
    ("isf_cone", "ISF Cone"), ("isf_box", "ISF Box"),
    ("isf_icosahedron", "ISF Icosahedron"), ("isf_steiner_patch", "ISF Steiner Patch"),
    ("isf_ellipse", "ISF Ellipse"), ("isf_cylinder", "ISF Cylinder"),
    ("mesh", "USD Mesh"), ("plane", "Bounded Plane"), ("infinite_plane", "Plane (Unbounded)"), ("wisp", "Wisp"),
)
OPERATIONS = (("union", "Union"), ("intersection", "Intersection"),
              ("cutout", "Cutout (A minus B)"), ("add", "Add"),
              ("subtract", "Subtract (A minus B)"), ("pyro", "Pyroclastic"),
              ("noise_warp", "Fractal Noise Warp"), ("terrain_displace", "Terrain Displace"))

COLOR = (
    P("color_mode", "Color Authoring", "menu", "inherit",
      "Display Color also updates the USD viewport; PhysVFX Color writes pbvfx:color.",
      (("inherit", "Inherit"), ("display", "USD Display Color"), ("physvfx", "PhysVFX Color"))),
    P("color", "Color", "color", (1.0, 1.0, 1.0)),
)
STAMP = (
    P("stamp", "Stamp Field", "toggle", False,
      "Sample the result into a grid. Wisps and meshes are already grid-backed."),
    P("use_stamp_voxel_size", "Override Density Voxel Size", "toggle", False),
    P("stamp_voxel_size", "Object Density Voxel Size", "float", 0.025,
      "Voxel size in stage units; overrides render settings for this object."),
    P("use_stamp_bounds", "Use Specified Stamp Bounds", "toggle", False,
      "Clip a stamped field to the world-axis-aligned box enclosing these local bounds."),
    P("stamp_bounds_min", "Stamp Bounds Minimum (Local)", "vector", (-2.0, -2.0, -2.0)),
    P("stamp_bounds_max", "Stamp Bounds Maximum (Local)", "vector", (2.0, 2.0, 2.0)),
    P("show_stamp_bounds", "Show Stamp Bounds (Red)", "toggle", False),
    P("cache_name", "Stamp Cache Name", "string", "",
      "Optional unique cache key. Empty means no cache; delete old caches after edits."),
)
BOUNDS = (
    P("bounds_min", "Bounds Minimum", "vector", (-2.0, -2.0, -2.0),
      "World-space bounds in USD stage units; used by bounded planes."),
    P("bounds_max", "Bounds Maximum", "vector", (2.0, 2.0, 2.0)),
)


def noise_fields(prefix, label):
    return (
        P(prefix + "Roughness", label + " Roughness", "float", 0.5),
        P(prefix + "Octaves", label + " Octaves", "float", 2.0),
        P(prefix + "Translation", label + " Translation", "vector", (0.0, 0.0, 0.0)),
        P(prefix + "Frequency", label + " Frequency", "float", 1.0),
        P(prefix + "FrequencyJump", label + " Frequency Jump", "float", 1.0),
    )


def spline_range_fields(channels):
    result = []
    for name, label, kind, default in channels:
        result.extend((P('splineMin_' + name, label + ' Minimum', kind, default),
                       P('splineMax_' + name, label + ' Maximum', kind, default),
                       P('splineRamp_' + name, label + ' Ramp', 'ramp', None,
                         'Ramp values from 0 to 1 mix the minimum and maximum along arc length.')))
    return tuple(result)


PARAMETERS = {
    'point_noise': (("Point Noise", (
        P('path', 'Point Cloud Prim Path', 'string', '/World/points'),
        P('radius', 'Default Radius', 'float', 1.0),
        P('fade', 'Default Fade', 'float', 0.5),
        P('density', 'Default Density', 'float', 1.0),
        P('frequency', 'Default Frequency', 'float', 1.0),
        P('roughness', 'Default Roughness', 'float', 0.5),
        P('octaves', 'Default Octaves', 'float', 3.0),
        P('frequency_jump', 'Default Frequency Jump', 'float', 2.0),
        P('use_stamp_voxel_size', 'Override Density Voxel Size', 'toggle', False),
        P('stamp_voxel_size', 'Object Density Voxel Size', 'float', 0.1),
        P('cache_name', 'Stamp Cache Name', 'string', ''),
    ) + COLOR),),
    'wisp_spline': (
        ('Wisp Spline', (
            P('path', 'Spline Prim Path', 'string', '/World/WispSpline'),
            P('parent_path', 'Parent Prim Path', 'string', '',
              'Optional existing parent; the spline uses its local coordinates.'),
            P('mode', 'Guide Values', 'menu', 'ramp', choices=(('ramp', 'Overall Ranges and Ramps'), ('manual', 'Per Guide'))),
            P('dots', 'Total Dot Count', 'int', 100000),
            P('seed', 'Seed', 'int', 1),
            P('use_stamp_voxel_size', 'Override Density Voxel Size', 'toggle', False),
            P('stamp_voxel_size', 'Object Density Voxel Size', 'float', 0.025),
            P('cache_name', 'Stamp Cache Name', 'string', ''),
            P('show_preview', 'Show Guide Line and Points', 'toggle', True),
            P('guide_size', 'Guide Marker Size', 'float', 0.12),
            P('ramp_subdivisions', 'Ramp Samples per Segment', 'int', 8,
              'Subdivide straight segments to capture curved ramps between guides.'),
        ) + COLOR + (P('guides', 'Guide Particles', 'guides', None),)),
        ('Spline Shape', spline_range_fields(CHANNELS[:8])),
        ('Spline Noise 1', spline_range_fields(CHANNELS[8:13])),
        ('Spline Noise 2', spline_range_fields(CHANNELS[13:])),
    ),
    "render_view": (("Render and View", (
        P("usd_path", "USD Save Path", "string", "$HIP/outputs/physvfx_scene.usda",
          "The composed stage is saved here as a flattened USDA."),
        P("image_path", "Image Save Path", "string", "$HIP/outputs/physvfx_render.exr"),
        P("render_python", "Renderer Python", "string", "python3",
          "Python interpreter executable (e.g. python3 or /path/to/venv/bin/python), not physics_vfx.so."),
        P("render_script", "Renderer Script", "string", "",
          "Optional path to python/usd_sphere_scene.py; blank uses the repository copy."),
        P("viewer_path", "Image Viewer", "string", "",
          "Optional executable path; blank finds dpa_imgviewer in the repository build directory."),
        P("render_stamp_dir", "Stamp Cache Directory", "string", "cache"),
        P("progressive_preview", "Progressive Preview", "toggle", False,
          "Show ten progressively denser full-size passes in one updating viewer."),
        P("render_view", "Render and View", "button", None,
          "Save the stage, render in the background and open the resulting image."),
    )), ("Continuous Preview", (
        P("continuous_preview", "Continuous Preview", "toggle", False,
          "Automatically update a separate watched viewer when the scene changes."),
        P("live_resolution_scale", "Resolution Scale", "float", 0.25,
          "Fraction of full width and height, in (0, 1]."),
        P("live_step_multiplier", "Ray and Shadow Step Multiplier", "float", 4.0,
          "Preview steps are this multiple of the scene render settings."),
        P("live_voxel_multiplier", "Grid Voxel Size Multiplier", "float", 2.0,
          "Makes preview grids coarser without changing the saved scene."),
        P("live_dot_fraction", "Wisp Dot Fraction", "float", 0.25,
          "Fraction of wisp dots used in previews; density is approximately compensated for dots and voxel size."),
        P("live_delay", "Delay After Edit (seconds)", "float", 0.75,
          "Wait for edits to settle before starting a new render."),
    )),),
    "shadow_cache": (("Shadow Cache", (
        P("region_path", "Shadow Region Prim Path", "string", "/World/ShadowRegion",
          "Path of an existing PhysVFX shadow region on the input stage."),
        P("shadow_cache_dir", "Shadow Cache Directory", "string", "$HIP/physvfx_shadow_cache"),
        P("shadow_cache_load", "Load from Disk", "toggle", False,
          "Use saved shadow maps at render time; fail if missing or incompatible."),
        P("cache_scene_lop", "Final Scene LOP", "string", "",
          "Optional final scene node; blank uses this network's display node."),
        P("cache_python", "Renderer Python", "string", "python3",
          "Python interpreter executable (e.g. python3 or /path/to/venv/bin/python), not physics_vfx.so."),
        P("cache_stamp_dir", "Stamp Cache Directory", "string", "cache"),
        P("cache_renderer_script", "Renderer Script", "string", "",
          "Optional path to python/usd_sphere_scene.py."),
        P("save_shadow_cache", "Save to Disk", "button", None,
          "Build and save this region's shadow maps from the final scene."),
    )),),
    "shadow_region": (("Shadow Region", (
        P("path", "Region Prim Path", "string", "/World/ShadowRegion"),
        P("bounds_min", "Local Minimum", "vector", (-1.0, -1.0, -1.0)),
        P("bounds_max", "Local Maximum", "vector", (1.0, 1.0, 1.0)),
        P("show_preview", "Show Region Guide", "toggle", True),
    )),),
    "field": (
        ("Field", (
            P("path", "Prim Path", "string", "/World/PhysVFXField",
              "Absolute USD prim path; when Parent Prim Path is set, only its final name is used."),
            P("parent_path", "Parent Prim Path", "string", "",
              "Optional existing parent prim. The field is created beneath it at local origin; the prim path uses the name above. This does not move existing prims."),
            P("kind", "Field Type", "menu", "isf_sphere", choices=FIELD_KINDS),
            P("create", "Create Missing Prim", "toggle", True,
              "Create a Sphere or Xform. Mesh fields require an existing USD Mesh."),
        ) + COLOR + STAMP + (
            P("show_preview", "Show Bounds Preview", "toggle", True,
              "Show a viewport-only wireframe bounding box; excluded from the PhysVFX render."),
            P("preview_wisp_radius", "Wisp Preview Half Size", "float", 0.0,
              "Wisp only: local-space half size. Zero estimates a size from the wisp settings."),
        )),
        ("Analytic and Mesh", (
            P("override_radius", "Override Sphere Radius", "toggle", False,
              "If off, a USD Sphere uses its native radius; other prims use 1. Editing Radius enables this for spheres."),
            P("radius", "Radius", "float", 1.0,
              "Sphere, box or cylinder radius. Editing a sphere's Radius enables its override."),
            P("majorRadius", "Major Radius", "float", 1.0),
            P("minorRadius", "Minor Radius", "float", 0.4),
            P("height", "Height", "float", 2.0),
            P("normal", "Normal", "vector", (0.0, 1.0, 0.0)),
            P("plane_guide_size", "Plane Grid Half Size", "float", 5.0,
              "Local-space half size of the viewport grid; does not limit the plane."),
            P("theta", "Cone Angle (radians)", "float", 0.5),
            P("roundingExponent", "Box Rounding Exponent", "int", 3),
            P("meshHalfWidth", "Mesh Half Width (voxels)", "float", 3.0,
              "Controls mesh-to-level-set voxelization band, not the source mesh viewport shape."),
        ) + BOUNDS),
        ("Wisp", (
            P("wispDots", "Dot Count", "int", 100000),
            P("wispDensity", "Density per Dot", "float", 1.0),
            P("wispClump", "Clump", "float", 1.0),
            P("wispPscale", "Particle Scale", "float", 1.0),
            P("wispSeed", "Seed", "int", 1),
            P("wispMin", "Sample Minimum", "float", -1.0),
            P("wispMax", "Sample Maximum", "float", 1.0),
            P("wispDisplacement", "Noise Displacement", "vector", (1.0, 1.0, 1.0)),
        )),
        ("Wisp Noise 1", noise_fields("wispNoise1", "Noise 1")),
        ("Wisp Noise 2", noise_fields("wispNoise2", "Noise 2")),
    ),
    "operation": (
        ("Operation", (
            P("path", "Operation Prim Path", "string", "/World/PhysVFXOperation"),
            P("operation", "Operation", "menu", "cutout", choices=OPERATIONS),
            P("input_a", "Input A Prim Path", "string", "",
              "Absolute path of a field or operation. A is the retained field for cutout."),
            P("input_b", "Input B Prim Path", "string", "",
              "Second input for binary operations; omitted for pyro, noise warp and terrain displacement."),
        ) + COLOR + STAMP),
        ("Noise and Bounds", (
            P("roughness", "Roughness", "float", 0.5),
            P("octaves", "Octaves", "float", 3.0),
            P("frequency", "Frequency", "float", 1.0),
            P("frequencyJump", "Frequency Jump", "float", 2.0),
            P("noiseTranslation", "Noise Translation", "vector", (0.0, 0.0, 0.0)),
            P("noise_type", "Warp Noise Type", "menu", "fspn", choices=(("fspn", "Fractal Sum Perlin"), ("fstn", "Fractal Sum Terrain"))),
            P("terrainAmpPos", "Terrain Positive Amplitude", "float", 1.0),
            P("terrainAmpNeg", "Terrain Negative Amplitude", "float", 1.0),
            P("terrainGammaPos", "Terrain Positive Gamma", "float", 1.0),
            P("terrainGammaNeg", "Terrain Negative Gamma", "float", 1.0),
            P("terrainTransition", "Terrain Gamma Transition", "float", 0.3,
              "Positive noise magnitude where gamma reaches its positive or negative value."),
            P("terrainIterations", "Terrain Nearest Point Iterations", "int", 1,
              "Number of nearest-point iterations for Terrain Displace."),
            P("amplitude", "Amplitude", "float", 0.25,
              "Pyro: positive expands. Noise warp and terrain displacement: non-negative overall strength."),
            P("gamma", "Pyro Gamma", "float", 1.0),
            P("iterations", "Pyro Iterations", "int", 1),
            P("normal", "Warp Direction", "vector", (0.0, 1.0, 0.0)),
        )),
    ),
    "camera": (("Camera", (
        P("path", "Camera Prim Path", "string", "/World/Camera"),
        P("create", "Create Missing Camera", "toggle", True),
        P("focal_length", "Focal Length", "float", 28.0),
        P("horizontal_aperture", "Horizontal Aperture", "float", 20.955),
        P("vertical_aperture", "Vertical Aperture", "float", 11.7871875),
        P("clipping", "Near / Far Clipping", "vector2", (0.1, 100.0)),
        P("show_camera_preview", "Show Camera Range", "toggle", True,
          "Show the camera ray-distance range as viewport guide geometry."),
    )),),
    "light": (("Sphere Light", (
        P("path", "Light Prim Path", "string", "/World/Light"),
        P("create", "Create Missing Light", "toggle", True),
        P("color", "Light Color", "color", (1.0, 1.0, 1.0)),
        P("intensity", "Intensity", "float", 1.0),
        P("exposure", "Exposure", "float", 0.0),
        P("override_shadow", "Override Shadow Kappa", "toggle", False),
        P("shadowKappa", "Shadow Kappa", "float", 3.0),
    )),),
    "settings": (("Render Settings", (
        P("path", "Settings Prim Path", "string", "/World/Settings"),
        P("camera", "Camera Prim Path", "string", "/World/Camera"),
        P("resolution", "Resolution", "int2", (960, 540)),
        P("minStep", "Minimum Ray Step", "float", 0.01),
        P("maxStep", "Maximum Ray Step", "float", 0.05),
        P("kappa", "Extinction Kappa", "float", 3.0),
        P("shadowKappa", "Default Shadow Kappa", "float", 3.0),
        P("shadowStep", "Shadow Step", "float", 0.01),
        P("densityVoxelSize", "Density Voxel Size", "float", 0.025),
        P("colorVoxelSize", "Color Voxel Size", "float", 0.025),
        P("shadowVoxelSize", "Shadow Voxel Size", "float", 0.025),
    )),),
}


def parameters(kind):
    return tuple(param for _, section in PARAMETERS[kind] for param in section)


def defaults(kind):
    return {param.name: param.default for param in parameters(kind)}
