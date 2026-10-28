# A large part of this code was generated with github copilot
# although a majority of it was janky and had to be reworked

from pxr import Usd, UsdGeom, Gf
import math
import physics_vfx as fx


def vector(value, scale=1.0):
    return fx.Vector(
        float(value[0]) * scale,
        float(value[1]) * scale,
        float(value[2]) * scale,
    )


def uniform_scale(scale):
    values = [abs(float(x)) for x in scale]

    if max(values) - min(values) > 1e-5:
        raise ValueError(
            f"Non-uniform scale is not supported for this primitive: {scale}"
        )

    return sum(values) / 3.0


def radial_axial_scale(scale, axis):
    """
    Returns (radial_scale, axial_scale).
    """
    scale = [abs(float(x)) for x in scale]

    if axis == "X":
        radial = [scale[1], scale[2]]
        axial = scale[0]
    elif axis == "Y":
        radial = [scale[0], scale[2]]
        axial = scale[1]
    else:
        radial = [scale[0], scale[1]]
        axial = scale[2]

    if abs(radial[0] - radial[1]) > 1e-5:
        raise ValueError("Different X/Y radial scales are not supported")

    return sum(radial) / 2.0, axial


def read_transform(prim, xform_cache, unit_scale):
    """
    Returns translation, scale, rotation-axis, and rotation-angle.

    USD rotation angles are degrees. Lux uses radians here.
    """
    matrix = xform_cache.GetLocalToWorldTransform(prim)

    ok, rotation_matrix, scale, shear, translation, perspective = matrix.Factor()

    if not ok:
        raise ValueError(f"Could not factor transform for {prim.GetPath()}")

    rotation = rotation_matrix.ExtractRotation()

    angle = math.radians(float(rotation.GetAngle()))
    axis = vector(rotation.GetAxis())

    position = vector(matrix.ExtractTranslation(), unit_scale)

    attribute = prim.GetAttribute("xformOp:rotateXYZ")
    xyz_rotation = None
    if attribute:
        xyz_rotation = attribute.Get()
        xyz_rotation[0] = math.radians(float(xyz_rotation[0]))
        xyz_rotation[1] = math.radians(float(xyz_rotation[1]))
        xyz_rotation[2] = math.radians(float(xyz_rotation[2]))


    return {
        "position": position,
        "scale": scale,
        "axis": axis,
        "angle": angle,
        "xyz_rotation": xyz_rotation,
        "rotation_matrix": rotation_matrix
    }


def apply_transform(field, transform):
    if abs(transform["angle"]) > 1e-8:
        axis = transform["axis"]
        print("axis = ", axis)
        print("angle = ", transform["angle"])

        # inverse_axis = fx.Vector(
        #     -axis.x,
        #     -axis.y,
        #     -axis.z,
        # )
        # field = fx.rotate(
        #     field,
        #     transform["axis"],
        #     -transform["angle"],
        # )
    if transform["xyz_rotation"]:
        field = fx.rotate(
            field,
            fx.Vector(1, 0, 0),
            -transform["xyz_rotation"][0]
        )
        field = fx.rotate(
                    field,
                    fx.Vector(0, 1, 0),
                    -transform["xyz_rotation"][1]
                )
        field = fx.rotate(
                    field,
                    fx.Vector(0, 0, 1),
                    -transform["xyz_rotation"][2]
                )

    field = fx.translate(field, transform["position"])
    return field


def canonical_axis(axis_name):
    axis_name = str(axis_name).upper()

    if axis_name == "X":
        return fx.Vector(1.0, 0.0, 0.0)
    elif axis_name == "Y":
        return fx.Vector(0.0, 1.0, 0.0)
    else:
        return fx.Vector(0.0, 0.0, 1.0)


def get_rotation(stage, prim_path):
    prim = stage.GetPrimAtPath(prim_path)

    if not prim:
        raise ValueError(f"Prim not found: {prim_path}")

    attribute = prim.GetAttribute("xformOp:rotateXYZ")
    if not attribute:
        return None

    return attribute.Get()


def field_from_prim(
    prim,
    xform_cache,
    unit_scale,
    rounding_exponent=3,
):
    transform = read_transform(
        prim,
        xform_cache,
        unit_scale,
    )

    origin = fx.Vector(0.0, 0.0, 0.0)
    type_name = str(prim.GetTypeName())
    print("Typename = ", type_name)
    scale = transform["scale"]

    if type_name == "Cube":
        cube = UsdGeom.Cube(prim)

        # USD Cube.size defaults to 2.
        size = float(cube.GetSizeAttr().Get())
        half_extent = 0.5 * size

        s = uniform_scale(scale)

        field = fx.box(
            origin,
            half_extent * s * unit_scale,
            rounding_exponent,
        )

    elif type_name == "Sphere":
        sphere = UsdGeom.Sphere(prim)

        # USD Sphere.radius defaults to 1.
        radius = float(sphere.GetRadiusAttr().Get())

        s = uniform_scale(scale)

        field = fx.sphere(
            origin,
            radius * s * unit_scale,
        )

    elif type_name == "Cylinder":
        cylinder = UsdGeom.Cylinder(prim)

        # USD defaults: radius=1, height=2, axis=Z.
        radius = float(cylinder.GetRadiusAttr().Get())
        height = float(cylinder.GetHeightAttr().Get())
        axis_name = cylinder.GetAxisAttr().Get()
        print("Axis name = ", axis_name)
        axis = canonical_axis(axis_name)

        radial_scale, axial_scale = radial_axial_scale(
            scale,
            str(axis_name).upper(),
        )

        field = fx.cylinder(
            origin,
            axis,
            radius,
            height,
        )

    elif type_name == "Cone":
        cone = UsdGeom.Cone(prim)

        # USD defaults: radius=1, height=2, axis=Z.
        radius = float(cone.GetRadiusAttr().Get())
        height = float(cone.GetHeightAttr().Get())
        axis_name = cone.GetAxisAttr().Get()
        axis = canonical_axis(axis_name)

        radial_scale, axial_scale = radial_axial_scale(
            scale,
            str(axis_name).upper(),
        )

        radius *= radial_scale * unit_scale
        height *= axial_scale * unit_scale

        theta = math.atan2(radius, height)
        # local_axis = Gf.Vec3d(
        #     float(axis.x),
        #     float(axis.y),
        #     float(axis.z),
        # )

        # world_axis = transform["rotation_matrix"].TransformDir(local_axis)

        # transform["position"] += vector(
        #     world_axis,
        #     height / 2.0,
        # )
        trans_to_match_usd = height / 2 * axis

        field = fx.cone(
            origin,
            axis,
            height,
            theta,
        )
        #field = fx.translate(field, trans_to_match_usd)

    elif type_name == "Plane":
        plane = UsdGeom.Plane(prim)

        axis_name = plane.GetAxisAttr().Get()
        normal = canonical_axis(axis_name)

        field = fx.plane(
            origin,
            normal,
        )

    else:
        return None

    return apply_transform(field, transform)


def union_fields(fields):
    fields = list(fields)

    if not fields:
        return None

    result = fields[0]

    for field in fields[1:]:
        result = fx.union_fields(result, field)

    return result


def create_volume_from_usd(
    filename,
    volume_meters_per_unit=1.0,
):
    stage = Usd.Stage.Open(filename)

    if not stage:
        raise RuntimeError(f"Could not open USD file: {filename}")

    stage_meters_per_unit = UsdGeom.GetStageMetersPerUnit(stage)
    unit_scale = stage_meters_per_unit / volume_meters_per_unit

    xform_cache = UsdGeom.XformCache(Usd.TimeCode.Default())


    fields = []
    field_names = []

    for prim in stage.Traverse():
        field = field_from_prim(
            prim,
            xform_cache,
            unit_scale,
        )

        if field is not None:
            fields.append(field)
            field_names.append(str(prim.GetPath()))

    return union_fields(fields), field_names


scene_field, names = create_volume_from_usd(
    "inputs/model.usda",
    volume_meters_per_unit=0.01,
)

scene_field = fx.negate(fx.mask(scene_field))

print(names)
# ['/Cube1', '/Cone1', '/Cylinder1', '/Sphere1']

fx.render_field(
    scene_field,
    "outputs/test.exr",
    width=960,
    height=540,
    eye=fx.Vector(0.0, 0.0, 8),
    view=fx.Vector(0.0, 0.0, -1.0),
    up=fx.Vector(0.0, 1.0, 0.0),
    sfar=15,
    ds=0.05,
)