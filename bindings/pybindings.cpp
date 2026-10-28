/*
The majority of this code was generated automatically with Github Copilot using
ChatGPT mini
*/


#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "field_interface.h"
#include "openvdb_helpers.h"
#include "usd_sphere_render.h"
#include "usd_scene_render.h"
#include "Camera.h"
#include "lights.h"

namespace py = pybind11;
using namespace lux;

// Python wrappers own OpenVDB grids, allowing make_grid_field to receive
// usable grids without depending on a separate OpenVDB Python module.
struct FloatGridHandle { openvdb::FloatGrid::Ptr grid; };
struct ColorGridHandle { openvdb::Vec3SGrid::Ptr grid; };

template <typename T>
void bind_field_operations(py::module_& module) {
    using FieldPtr = VSPtr<T>;
    module.def("add", &add<T, T>, py::arg("a"), py::arg("b"));
    module.def("subtract", &subtract<T, T>, py::arg("a"), py::arg("b"));
    module.def("scale", &scale<T, float>, py::arg("field"), py::arg("scale"));
    module.def("translate", &translate<T>, py::arg("field"), py::arg("delta"));
    module.def("translate", [](const FieldPtr& f, const Vector& delta) {
        return translate<T>(f, make_constant<Vector>(delta));
    }, py::arg("field"), py::arg("delta"));
    module.def("rotate", &rotate<T>, py::arg("field"), py::arg("axis"), py::arg("angle"));
    module.def("rotate", [](const FieldPtr& f, const Vector& axis, float angle) {
        return rotate<T>(f, make_constant<Vector>(axis), make_constant<float>(angle));
    }, py::arg("field"), py::arg("axis"), py::arg("angle"));
    module.def("scale_fixed", &scale_fixed<T>, py::arg("field"), py::arg("scale"));
    module.def("translate_fixed", &translate_fixed<T>, py::arg("field"), py::arg("delta"));
    module.def("rotate_fixed", &rotate_fixed<T>, py::arg("field"), py::arg("axis"), py::arg("angle"));
    module.def("make_constant", &make_constant<T>, py::arg("value"));
    module.def("constant", &make_constant<T>, py::arg("value"));
    module.def("make_memofield", &make_memofield<T>, py::arg("field"));
    module.def("make_clear_cache_field", &make_clear_cache_field<T>, py::arg("field"));
    module.def("make_advect_semi_lagrange", &make_advect_semi_lagrange<T>,
               py::arg("field"), py::arg("velocity"), py::arg("dt"));
    module.def("make_warp_field", &make_warp_field<T>, py::arg("field"), py::arg("map"));
    module.def("negate", [](const FieldPtr& field) { return -field; }, py::arg("field"));
}

template <typename T>
void bind_field_class(py::module_& module, const char* name) {
    py::class_<Volume<T>, std::shared_ptr<Volume<T>>>(module, name)
        .def("eval", &Volume<T>::eval)
        .def("grad", &Volume<T>::grad)
        .def("set_fd_size", &Volume<T>::setFDSize)
        .def("set_fd_step", py::overload_cast<double>(&Volume<T>::setFDStep))
        .def("set_fd_step", py::overload_cast<double, double, double>(&Volume<T>::setFDStep))
        .def("clear_cache", &Volume<T>::clear_cache)
        .def("typelabel", &Volume<T>::typelabel);
}

PYBIND11_MODULE(physics_vfx, module)
{
    module.doc() = "Physics-based VFX field operations";

    py::class_<Vector>(module, "Vector")
        .def(py::init<>())
        .def(py::init<double, double, double>())
        .def_property_readonly("x", &Vector::X)
        .def_property_readonly("y", &Vector::Y)
        .def_property_readonly("z", &Vector::Z)
        .def("magnitude", &Vector::magnitude)
        .def("unitvector", &Vector::unitvector)
        .def("__repr__", [](const Vector& value) {
            return "Vector(" +
                   std::to_string(value.X()) + ", " +
                   std::to_string(value.Y()) + ", " +
                   std::to_string(value.Z()) + ")";
        });

    py::class_<RenderSettings>(module, "RenderSettings")
        .def(py::init<>())
        .def_readwrite("model_halfsize", &RenderSettings::model_halfsize)
        .def_readwrite("width", &RenderSettings::width)
        .def_readwrite("height", &RenderSettings::height)
        .def_readwrite("near_dist", &RenderSettings::near_dist)
        .def_readwrite("far_dist", &RenderSettings::far_dist)
        .def_readwrite("min_step", &RenderSettings::min_step)
        .def_readwrite("max_step", &RenderSettings::max_step)
        .def_readwrite("kappa", &RenderSettings::kappa)
        .def_readwrite("shadow_kappa", &RenderSettings::shadow_kappa)
        .def_readwrite("shadow_step", &RenderSettings::shadow_step)
        .def_readwrite("cam_dist", &RenderSettings::cam_dist)
        .def_property("density_voxel_size", &RenderSettings::get_density_vs, &RenderSettings::set_density_vs)
        .def_property("color_voxel_size", &RenderSettings::get_color_vs, &RenderSettings::set_color_vs)
        .def_property("shadow_voxel_size", &RenderSettings::get_shadow_vs, &RenderSettings::set_shadow_vs);

    module.def("render_usd_sphere", &render_usd_sphere,
        py::arg("sphere"), py::arg("center"), py::arg("radius"),
        py::arg("eye"), py::arg("view"), py::arg("up"), py::arg("horizontal_fov"),
        py::arg("light_position"), py::arg("light_color"), py::arg("light_shadow_kappa"),
        py::arg("settings"), py::arg("filename"));

    module.def("render_usd_scene", &render_usd_scene, py::arg("density"), py::arg("color"),
        py::arg("camera"), py::arg("lights"), py::arg("settings"), py::arg("bounds_min"),
        py::arg("bounds_max"), py::arg("filename"), py::call_guard<py::gil_scoped_release>());

    py::class_<Color>(module, "Color")
        .def(py::init<>())
        .def(py::init<double, double, double, double>(), py::arg("red"), py::arg("green"),
             py::arg("blue"), py::arg("alpha") = 0.0)
        .def_property_readonly("red", &Color::red)
        .def_property_readonly("green", &Color::green)
        .def_property_readonly("blue", &Color::blue)
        .def_property_readonly("alpha", &Color::alpha);

    py::class_<PointLight>(module, "PointLight")
        .def(py::init<Color, Vector, float>(), py::arg("color"), py::arg("position"), py::arg("kappa") = 1.0f)
        .def_readwrite("color", &PointLight::color)
        .def_readwrite("position", &PointLight::position)
        .def_readwrite("kappa", &PointLight::kappa);

    py::class_<Camera>(module, "Camera")
        .def(py::init<>())
        .def("set_eye_view_up", &Camera::setEyeViewUp)
        .def("set_fov", &Camera::setFov)
        .def("set_aspect_ratio", &Camera::setAspectRatio)
        .def_property_readonly("eye", &Camera::eye)
        .def_property_readonly("view", py::overload_cast<>(&Camera::view, py::const_))
        .def_property_readonly("up", &Camera::up)
        .def_property_readonly("fov", &Camera::fov)
        .def_property_readonly("aspect_ratio", &Camera::aspectRatio);

    py::class_<Matrix>(module, "Matrix")
        .def(py::init<>())
        .def("get", [](const Matrix& m, int row, int col) {
            if (row < 0 || row >= 3 || col < 0 || col >= 3) throw py::index_error();
            return m.Get(row, col);
        }, py::arg("row"), py::arg("col"));

    bind_field_class<float>(module, "Field");
    bind_field_class<Vector>(module, "VectorField");
    bind_field_class<Color>(module, "ColorField");
    bind_field_operations<float>(module);
    bind_field_operations<Vector>(module);
    bind_field_operations<Color>(module);

    module.def("affine_transform_field", [](const vspf& field, const Vector& origin,
        const Vector& x_axis, const Vector& y_axis, const Vector& z_axis) {
        if (!field) throw py::value_error("field cannot be null");
        return funcfield<float>([field, origin, x_axis, y_axis, z_axis](const Vector& p) {
            return field->eval(origin + p.X() * x_axis + p.Y() * y_axis + p.Z() * z_axis);
        });
    }, py::arg("field"), py::arg("origin"), py::arg("x_axis"), py::arg("y_axis"), py::arg("z_axis"));

    module.def("make_scene_color_field", [](const std::vector<vspf>& fields, const std::vector<Color>& colors) {
        if (fields.size() != colors.size()) throw py::value_error("one color is required for each field");
        for (const auto& field : fields) if (!field) throw py::value_error("fields cannot be null");
        return funcfield<Color>([fields, colors](const Vector& p) {
            for (size_t i = 0; i < fields.size(); ++i) if (fields[i]->eval(p) < 0) return colors[i];
            return Color(0, 0, 0, 0);
        });
    }, py::arg("fields"), py::arg("colors"));

    py::class_<FSVals>(module, "FSVals")
        .def(py::init<>())
        .def(py::init<float, float, Vector, float, float>(),
             py::arg("roughness"), py::arg("octaves"), py::arg("translation"),
             py::arg("frequency"), py::arg("frequency_jump"))
        .def_readwrite("r", &FSVals::r)
        .def_readwrite("octaves", &FSVals::octaves)
        .def_readwrite("translation", &FSVals::translation)
        .def_readwrite("freq", &FSVals::freq)
        .def_readwrite("fjump", &FSVals::fjump);

    py::class_<FloatGridHandle>(module, "FloatGrid")
        .def(py::init([](float voxel_size, float background) {
            if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive");
            openvdb::initialize();
            auto grid = openvdb::FloatGrid::create(background);
            grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
            return FloatGridHandle{grid};
        }), py::arg("voxel_size"), py::arg("background") = 0.0f)
        .def("set_value", [](FloatGridHandle& g, int x, int y, int z, float value) {
            g.grid->getAccessor().setValue(openvdb::Coord(x, y, z), value);
        })
        .def("get_value", [](const FloatGridHandle& g, int x, int y, int z) {
            return g.grid->getConstAccessor().getValue(openvdb::Coord(x, y, z));
        });

    py::class_<ColorGridHandle>(module, "ColorGrid")
        .def(py::init([](float voxel_size) {
            if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive");
            openvdb::initialize();
            auto grid = openvdb::Vec3SGrid::create(openvdb::Vec3s(0, 0, 0));
            grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
            return ColorGridHandle{grid};
        }), py::arg("voxel_size"))
        .def("set_value", [](ColorGridHandle& g, int x, int y, int z, const Color& value) {
            g.grid->getAccessor().setValue(openvdb::Coord(x, y, z), GridTypes<Color>::to_grid(value));
        })
        .def("get_value", [](const ColorGridHandle& g, int x, int y, int z) {
            return GridTypes<Color>::from_grid(g.grid->getConstAccessor().getValue(openvdb::Coord(x, y, z)));
        });
    module.def("make_grid_field", [](const FloatGridHandle& g) { return make_grid_field<float>(g.grid); }, py::arg("grid"));
    module.def("make_grid_field", [](const ColorGridHandle& g) { return make_grid_field<Color>(g.grid); }, py::arg("grid"));

    module.def(
        "sphere",
        &isf_sphere,
        py::arg("center"),
        py::arg("radius")
    );

    module.def(
        "torus",
        &isf_torus,
        py::arg("center"),
        py::arg("major_radius"),
        py::arg("minor_radius"),
        py::arg("normal")
    );

    module.def(
        "cone",
        &isf_cone,
        py::arg("origin"),
        py::arg("normal"),
        py::arg("height"),
        py::arg("theta")
    );

    module.def(
        "box",
        &isf_box,
        py::arg("center"),
        py::arg("radius"),
        py::arg("rounding_exponent")
    );

    module.def(
        "cylinder",
        &isf_cylinder,
        py::arg("center"),
        py::arg("normal"),
        py::arg("radius"),
        py::arg("height")
    );

    module.def(
        "plane",
        &make_plane,
        py::arg("point"),
        py::arg("normal")
    );

    module.def("icosahedron", &isf_icosahedron, py::arg("center"));

    module.def(
        "steiner_patch",
        &isf_steiner_patch,
        py::arg("center")
    );

    module.def(
        "ellipse",
        &isf_ellipse,
        py::arg("center"),
        py::arg("normal"),
        py::arg("major_radius"),
        py::arg("minor_radius")
    );

    // Scalar field operations
    module.def(
        "exp",
        [](const lux::vspf& field) {
            return lux::exp(field);
        },
        py::arg("field")
    );
    module.def("log", [](const lux::vspf& field) {
        return lux::log(field);
    }, py::arg("field"));

    module.def("sin", [](const lux::vspf& field) {
        return lux::sin(field);
    }, py::arg("field"));

    module.def("cos", [](const lux::vspf& field) {
        return lux::cos(field);
    }, py::arg("field"));

    module.def(
        "pow",
        [](const lux::vspf& field, const lux::vspf& exponent) {
            return lux::pow(field, exponent);
        },
        py::arg("field"),
        py::arg("exponent")
    );

    // Field constructors
    

    module.def("union_fields", &union_fields);
    module.def("intersection", &intersection);
    module.def("cutout", &cutout);
    module.def("mask", &mask);
    module.def("clamp", &clamp);
    module.def("shell", &shell);
    module.def("dilation", &dilation);
    module.def(
    "blinn_blend",
    [](const std::vector<vspf>& fields,
        float blend_factor,
        float shape_broadness) {
        auto field_list =
            std::make_shared<const std::vector<vspf>>(fields);

        return blinn_blend(
            std::move(field_list),
            blend_factor,
            shape_broadness
        );
    },
    py::arg("fields"),
    py::arg("blend_factor"),
    py::arg("shape_broadness")
);


    module.def("pow", py::overload_cast<const vspf&, float>(&lux::pow), py::arg("field"), py::arg("exponent"));
    module.def("abs", &lux::abs, py::arg("field"));
    module.def("clamp_fixed", &clamp_fixed, py::arg("field"), py::arg("min"), py::arg("max"));
    module.def("smoothstep", &smoothstep, py::arg("edge0"), py::arg("edge1"), py::arg("x"));
    module.def("smoothstep_lerp", &smoothstep_lerp, py::arg("a"), py::arg("b"), py::arg("t"));
    module.def("make_pyro", &make_pyro, py::arg("field"), py::arg("roughness"),
        py::arg("octaves"), py::arg("translation"), py::arg("frequency"),
        py::arg("frequency_jump"), py::arg("amplitude"), py::arg("gamma"), py::arg("iters") = 1);
    module.def("make_identity_field", &make_identity_field);
    module.def("make_grad_field", &make_grad_field, py::arg("field"));
    module.def("makeNPT", &makeNPT, py::arg("field"));
    module.def("make_iteratedNPT", &make_iteratedNPT, py::arg("field"), py::arg("iters"));
    module.def("make_eval_dot_grad_over_gradsq", &make_eval_dot_grad_over_gradsq, py::arg("field"));

    module.def("fspn", &fspn<float, Vector>, py::arg("values"), py::arg("point"));
    module.def("make_fspn", py::overload_cast<float, float, const Vector&, float, float>(&make_fspn),
        py::arg("roughness"), py::arg("octaves"), py::arg("translation"),
        py::arg("frequency"), py::arg("frequency_jump"));
    module.def("make_fspn", [](const FSVals& vals) { return make_fspn(FSVals(vals)); }, py::arg("values"));
    module.def("make_fade_factor", &make_fade_factor, py::arg("center"), py::arg("fade"), py::arg("radius"));

    // Preserve the shorter existing names while exposing the C++ names too.
    module.attr("make_plane") = module.attr("plane");
    module.attr("isf_sphere") = module.attr("sphere");
    module.attr("isf_torus") = module.attr("torus");
    module.attr("isf_cone") = module.attr("cone");
    module.attr("isf_box") = module.attr("box");
    module.attr("isf_icosahedron") = module.attr("icosahedron");
    module.attr("isf_steiner_patch") = module.attr("steiner_patch");
    module.attr("isf_ellipse") = module.attr("ellipse");
    module.attr("isf_cylinder") = module.attr("cylinder");

    py::class_<coordbbox>(module, "CoordBBox")
        .def(py::init([](const std::array<int, 3>& lower, const std::array<int, 3>& upper) {
            for (int i = 0; i < 3; ++i) if (lower[i] > upper[i]) throw py::value_error("lower bounds must not exceed upper bounds");
            return coordbbox(ocoord(lower[0], lower[1], lower[2]), ocoord(upper[0], upper[1], upper[2]));
        }), py::arg("minimum"), py::arg("maximum"))
        .def_property_readonly("minimum", [](const coordbbox& b) { return std::array<int, 3>{b.min().x(), b.min().y(), b.min().z()}; })
        .def_property_readonly("maximum", [](const coordbbox& b) { return std::array<int, 3>{b.max().x(), b.max().y(), b.max().z()}; });

    module.def("world_space_to_bounds", [](const Vector& lower, const Vector& upper, float voxel_size) {
        return world_space_to_bounds(lower, upper, voxel_size);
    }, py::arg("lower"), py::arg("upper"), py::arg("voxel_size"));
    module.def("world_space_to_bounds", [](const Vector& lower, const Vector& upper, const FloatGridHandle& grid) {
        return world_space_to_bounds(lower, upper, grid.grid);
    }, py::arg("lower"), py::arg("upper"), py::arg("grid"));

    module.def("create_float_grid", [](float voxel_size, float background) {
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize(); return FloatGridHandle{create_float_grid(voxel_size, background)};
    }, py::arg("voxel_size"), py::arg("background") = 0.0f);
    module.def("create_color_grid", [](float voxel_size, const Color& background) {
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize(); return ColorGridHandle{create_color_grid(voxel_size, GridTypes<Color>::to_grid(background))};
    }, py::arg("voxel_size"), py::arg("background") = Color(0, 0, 0, 0));
    module.def("create_grid", [](float voxel_size, float background) {
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize(); return FloatGridHandle{create_grid<float>(voxel_size, background)};
    }, py::arg("voxel_size"), py::arg("background") = 0.0f);
    module.def("create_grid", [](float voxel_size, const Color& background) {
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize(); return ColorGridHandle{create_grid<Color>(voxel_size, GridTypes<Color>::to_grid(background))};
    }, py::arg("voxel_size"), py::arg("background"));

    module.def("obj_mesh_to_level_set_f", [](const std::string& path, float voxel_size, float half_width, bool recenter) {
        return FloatGridHandle{obj_mesh_to_level_set_f(path, voxel_size, half_width, recenter)};
    }, py::arg("path"), py::arg("voxel_size"), py::arg("half_width") = 3.0f, py::arg("recenter") = false,
       "Voxelize OBJ triangles/quads into a signed-distance grid. Positions are preserved by default.");
    module.def("obj_mesh_to_grid_field", [](const std::string& path, float voxel_size, float half_width, bool recenter) {
        return obj_mesh_to_grid_field(path, voxel_size, half_width, recenter);
    }, py::arg("path"), py::arg("voxel_size"), py::arg("half_width") = 3.0f, py::arg("recenter") = false,
       "Voxelize an OBJ as a signed-distance scalar field.");
    module.attr("obj_mesh_to_field") = module.attr("obj_mesh_to_grid_field");

    module.def("mesh_to_levelset", [](float voxel_size, const std::vector<Vector>& vertices,
        const std::vector<std::array<int, 3>>& triangles, const std::vector<std::array<int, 4>>& quads, float half_width) {
        std::vector<openvdb::Vec3s> points; points.reserve(vertices.size());
        for (const auto& v : vertices) points.emplace_back(v.X(), v.Y(), v.Z());
        std::vector<openvdb::Vec3I> tris; tris.reserve(triangles.size());
        for (const auto& f : triangles) tris.emplace_back(f[0], f[1], f[2]);
        std::vector<openvdb::Vec4I> qs; qs.reserve(quads.size());
        for (const auto& f : quads) qs.emplace_back(f[0], f[1], f[2], f[3]);
        return FloatGridHandle{mesh_to_levelset(voxel_size, std::move(points), std::move(tris), std::move(qs), half_width)};
    }, py::arg("voxel_size"), py::arg("points"), py::arg("triangles"),
       py::arg("quads") = std::vector<std::array<int, 4>>{}, py::arg("half_width") = 3.0f);

    module.def("stamp_isf_to_grid", [](float voxel_size, float background, const vspf& field, const coordbbox& bounds, const std::string& filename) {
        if (!field) throw py::value_error("field cannot be null");
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize();
        return FloatGridHandle{stamp_isf_to_grid(voxel_size, background, field, bounds, filename)};
    }, py::arg("voxel_size"), py::arg("background"), py::arg("field"), py::arg("bounds"), py::arg("filename") = "");
    module.def("stamp_color_to_grid", [](const vspc& field, const coordbbox& bounds, float voxel_size, const std::string& filename) {
        if (!field) throw py::value_error("field cannot be null");
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize();
        return ColorGridHandle{stamp_color_to_grid(field, bounds, voxel_size, filename)};
    }, py::arg("field"), py::arg("bounds"), py::arg("voxel_size"), py::arg("filename") = "");

    module.def("stamp_grid", [](FloatGridHandle& grid, const vspf& field, const coordbbox& bounds, bool negative_only) {
        if (!field) throw py::value_error("field cannot be null");
        stamp_grid<float>(grid.grid, [field](const Vector& p) { return field->eval(p); },
            [negative_only](float v) { return !negative_only || v < 0; }, bounds);
    }, py::arg("grid"), py::arg("field"), py::arg("bounds"), py::arg("negative_only") = false);
    module.def("stamp_grid", [](ColorGridHandle& grid, const vspc& field, const coordbbox& bounds) {
        if (!field) throw py::value_error("field cannot be null");
        stamp_grid<Color>(grid.grid, [field](const Vector& p) { return field->eval(p); }, [](const Color&) { return true; }, bounds);
    }, py::arg("grid"), py::arg("field"), py::arg("bounds"));
    module.def("stamp_grid_parallel", [](float voxel_size, float background, const vspf& field, const coordbbox& bounds, bool negative_only, const std::string& filename) {
        if (!field) throw py::value_error("field cannot be null");
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize();
        return FloatGridHandle{stamp_grid_parallel<float>(voxel_size, background,
            [field](const Vector& p) { return field->eval(p); }, [negative_only](float v) { return !negative_only || v < 0; }, bounds, filename)};
    }, py::arg("voxel_size"), py::arg("background"), py::arg("field"), py::arg("bounds"), py::arg("negative_only") = false, py::arg("filename") = "");
    module.def("stamp_grid_parallel", [](float voxel_size, const Color& background, const vspc& field, const coordbbox& bounds, const std::string& filename) {
        if (!field) throw py::value_error("field cannot be null");
        if (!std::isfinite(voxel_size) || voxel_size <= 0) throw py::value_error("voxel_size must be positive and finite");
        openvdb::initialize();
        return ColorGridHandle{stamp_grid_parallel<Color>(voxel_size, GridTypes<Color>::to_grid(background),
            [field](const Vector& p) { return field->eval(p); }, [](const Color&) { return true; }, bounds, filename)};
    }, py::arg("voxel_size"), py::arg("background"), py::arg("field"), py::arg("bounds"), py::arg("filename") = "");
    module.def("save_to_disk", [](const FloatGridHandle& grid, std::string filename) { save_to_disk(grid.grid, filename); }, py::arg("grid"), py::arg("filename"));
    module.def("save_to_disk", [](const ColorGridHandle& grid, std::string filename) { save_to_disk(grid.grid, filename); }, py::arg("grid"), py::arg("filename"));
    module.def("trilinear_deposit", [](FloatGridHandle& grid, const Vector& position, float value) {
        auto accessor = grid.grid->getAccessor();
        trilinear_deposit(accessor, grid.grid->transform(), openvdb::Vec3s(position.X(), position.Y(), position.Z()), value);
    }, py::arg("grid"), py::arg("position"), py::arg("value"));
    module.def("trilinear_deposit", [](ColorGridHandle& grid, const Vector& position, const Color& value) {
        auto accessor = grid.grid->getAccessor();
        trilinear_deposit(accessor, grid.grid->transform(), openvdb::Vec3s(position.X(), position.Y(), position.Z()), GridTypes<Color>::to_grid(value));
    }, py::arg("grid"), py::arg("position"), py::arg("value"));

}
