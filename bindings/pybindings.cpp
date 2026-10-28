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
#include "usd_sphere_render.h"

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

    py::class_<Color>(module, "Color")
        .def(py::init<>())
        .def(py::init<double, double, double, double>(), py::arg("red"), py::arg("green"),
             py::arg("blue"), py::arg("alpha") = 0.0)
        .def_property_readonly("red", &Color::red)
        .def_property_readonly("green", &Color::green)
        .def_property_readonly("blue", &Color::blue)
        .def_property_readonly("alpha", &Color::alpha);

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
}
