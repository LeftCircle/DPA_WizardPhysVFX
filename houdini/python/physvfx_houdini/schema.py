"""Parameter descriptions for the PhysVFX Solaris shelf tools.

This file is independent of Houdini, USD, and the PhysVFX extension, so the
node interfaces and USD authoring can be tested outside the application.
"""
from dataclasses import dataclass


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
    ("mesh", "USD Mesh"), ("plane", "Bounded Plane"), ("wisp", "Wisp"),
)
OPERATIONS = (("union", "Union"), ("intersection", "Intersection"),
              ("cutout", "Cutout (A minus B)"), ("add", "Add"),
              ("subtract", "Subtract (A minus B)"), ("pyro", "Pyroclastic"),
              ("noise_warp", "Fractal Noise Warp"))

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
      "World-space bounds in USD stage units; used by planes and bounded operations."),
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


PARAMETERS = {
    "shadow_region": (("Shadow Region", (
        P("path", "Region Prim Path", "string", "/World/ShadowRegion"),
        P("bounds_min", "Local Minimum", "vector", (-1.0, -1.0, -1.0)),
        P("bounds_max", "Local Maximum", "vector", (1.0, 1.0, 1.0)),
        P("show_preview", "Show Region Guide", "toggle", True),
    )),),
    "field": (
        ("Field", (
            P("path", "Prim Path", "string", "/World/PhysVFXField",
              "Absolute USD prim path; use an existing prim or enable creation."),
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
              "If off, a USD Sphere uses its native radius; other prims use 1."),
            P("radius", "Radius", "float", 1.0),
            P("majorRadius", "Major Radius", "float", 1.0),
            P("minorRadius", "Minor Radius", "float", 0.4),
            P("height", "Height", "float", 2.0),
            P("normal", "Normal", "vector", (0.0, 1.0, 0.0)),
            P("theta", "Cone Angle (radians)", "float", 0.5),
            P("roundingExponent", "Box Rounding Exponent", "int", 3),
            P("meshHalfWidth", "Mesh Half Width (voxels)", "float", 3.0),
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
              "Second input for binary operations; omitted for pyro and noise warp."),
        ) + COLOR + STAMP),
        ("Noise and Bounds", (
            P("roughness", "Roughness", "float", 0.5),
            P("octaves", "Octaves", "float", 3.0),
            P("frequency", "Frequency", "float", 1.0),
            P("frequencyJump", "Frequency Jump", "float", 2.0),
            P("noiseTranslation", "Noise Translation", "vector", (0.0, 0.0, 0.0)),
            P("amplitude", "Amplitude", "float", 0.25,
              "Pyro: positive expands and negative contracts. Noise warp requires a non-negative amplitude."),
            P("gamma", "Pyro Gamma", "float", 1.0),
            P("iterations", "Pyro Iterations", "int", 1),
            P("normal", "Warp Direction", "vector", (0.0, 1.0, 0.0)),
            P("use_pyro_bounds", "Use Explicit Pyro Bounds", "toggle", False,
              "Clip pyro to the world-space bounds below; otherwise expanding pyro uses estimated bounds."),
        ) + BOUNDS),
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
