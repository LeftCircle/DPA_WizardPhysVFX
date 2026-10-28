#pragma once

#include <vector>
#include <algorithm>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <utility>
#include <type_traits>

#include "volume.h"
#include "field_operations.h"
#include "implicit_fields.h"
#include "constructive_solid_geometry.h"
#include "noise_fields.h"
#include "recursive_mem.h"

namespace lux{


// ---------------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------------

using vspf = VolumeSPtr<float>;
using vspv = VSPtr<Vector>;

template<typename T, typename Eval, typename Grad = std::nullptr_t>
VolumeSPtr<T> funcfield(
    Eval&& eval_func,
    Grad&& grad_func = {}
){
    // Converts lhv to be copied instead of referenced.
    // Can save time by passing large caputre lambdas as rhv
    using EvalType = std::decay_t<Eval>;
    using GradType = std::decay_t<Grad>;
    return std::make_shared<FunctionField<T, EvalType, GradType>>(
        std::forward<Eval>(eval_func),
        std::forward<Grad>(grad_func)
    );
}

template<typename T, typename U>
std::shared_ptr<Volume<T>> add(const VolumeSPtr<T>& a, const VolumeSPtr<U>& b);

template<typename T, typename U>
std::shared_ptr<Volume<T>> subtract(const VolumeSPtr<T>& a, const VolumeSPtr<U>& b) {
    return std::make_shared<SubtractFields<T, U>>(a, b);
}

template<typename T, typename U>
VolumeSPtr<T> scale(const VolumeSPtr<T>& a, const VolumeSPtr<U>& b){
    return std::make_shared<ScaleField<T, U>>(a, b);
}

template<typename T>
VolumeSPtr<T> translate(const VolumeSPtr<T>& a, const VolumeSPtr<Vector>& delta){
    return std::make_shared<TranslateField<T>>(a, delta);
}

template<typename T>
VolumeSPtr<T> rotate(const VolumeSPtr<T> a, const VolumeSPtr<Vector> axis, const VolumeSPtr<float> angle){
    return std::make_shared<RotateField<T>>(std::move(a), std::move(axis), std::move(angle));
}

template<typename T>
VolumeSPtr<T> scale_fixed(const VolumeSPtr<T>& a, const Vector& scale){
    Vector one_over_scale = Vector(1.0 / scale.X(), 1.0 / scale.Y(), 1.0 / scale.Z()); 
    auto eval_func = [a, one_over_scale](const Vector& p){
        return a->eval(Vector(p.X() * one_over_scale.X(), p.Y() * one_over_scale.Y(), p.Z() * one_over_scale.Z()));
    };
    return funcfield<T>(eval_func);
}
template<typename T>
VolumeSPtr<T> translate_fixed(const VolumeSPtr<T>& a, const Vector& delta){
    auto eval_func = [a, delta](const Vector& p){
        return a->eval(p - delta);
    };
    return funcfield<T>(eval_func);   
}
template<typename T>
VolumeSPtr<T> rotate_fixed(const VolumeSPtr<T> a, const Vector& axis, float angle){
    auto eval_func = [a, axis, angle](const Vector& p){
        return a->eval(rotation(p, axis, angle));
    };
    return funcfield<T>(eval_func);   
}


float smoothstep(float edge0, float edge1, float x);

Vector smoothstep_lerp(const Vector& a, const Vector& b, float t);

template<typename T>
VSPtr<T> make_memofield(const VSPtr<T>& a){
    return std::make_shared<MemoField<T>>(a);
}

template <typename T>
vspf make_clear_cache_field(const VSPtr<T>& a){
    auto eval = [a](const Vector& p){
        a->clear_cache();
        return a->eval(p);
    };
    auto grad = [a](const Vector& p){ return a->grad(p); };
    return funcfield<T>(eval, grad);
}


// ---------------------------------------------------------------------------------
// float operations
// ---------------------------------------------------------------------------------

VolumeSPtr<float> exp(const VolumeSPtr<float>& a);

VolumeSPtr<float> log(const VolumeSPtr<float>& a);

VolumeSPtr<float> sin(const VolumeSPtr<float>& a);

VolumeSPtr<float> cos(const VolumeSPtr<float>& a);

VolumeSPtr<float> pow(const VolumeSPtr<float>& a, const VolumeSPtr<float>& to_power);
vspf pow(const vspf& a, float power);
vspf abs(const vspf& a);

vspf make_pyro(
    const vspf& f,
    float r, // roughness
    float octaves,
    const Vector& translation,
    float freq,
    float fjump,
    float amp,
    float gamma,
    int iters = 1
);

template<typename T>
VSPtr<T> make_advect_semi_lagrange(const VSPtr<T>& a, const vspv& u, float dt){
    auto eval = [a, u, dt](const Vector& p){
        return a->eval(p - u->eval(p) * dt);
    };
    auto grad = [a, u, dt](const Vector& p){
        auto x = p - u->eval(p) * dt;
        auto M = unitMatrix() - (u->grad(p) * dt);
        return M * a->grad(x);
    };
    return funcfield<T>(eval, grad);
}

// ---------------------------------------------------------------------------------
// constructive solid geometry
// ---------------------------------------------------------------------------------

vspf union_fields(const vspf a, const vspf b);

vspf intersection(const vspf a, const vspf b);

vspf cutout(const vspf a, const vspf b);

vspf clamp(const vspf a, const vspf min, const vspf max);

vspf clamp_fixed(const vspf a, float min, float max);

vspf mask(const vspf a);

vspf blinn_blend(std::shared_ptr<const std::vector<vspf>> fields, float blend_factor, float shape_broadness);

vspf dilation(const vspf a, float d);

vspf shell(const vspf a, float d);

// ---------------------------------------------------------------------------------
// fields!
// ---------------------------------------------------------------------------------
template<typename T>
VolumeSPtr<T> make_constant(const T& t){
    return std::make_shared<ConstantField<T>>(t);
}

VolumeSPtr<float> make_plane(const Vector& point, const Vector& normal) ;

VolumeSPtr<float> isf_sphere(const Vector& center, const float radius);

VolumeSPtr<float> isf_torus(const Vector& center, float r_major, float r_minor, const Vector& n_hat);

VolumeSPtr<float> isf_cone(const Vector& x0, const Vector& nhat, float height, float theta);

VolumeSPtr<float> isf_box(const Vector& center, float radius, int rounding_exponent);

VolumeSPtr<float> isf_icosahedron(const Vector& center);

VolumeSPtr<float> isf_steiner_patch(const Vector& center);

VolumeSPtr<float> isf_ellipse(const Vector& center, const Vector& normal, float r_major, float r_minor);

VolumeSPtr<float> isf_cylinder(const Vector& center, const Vector& normal, float r, float h);


// ---------------------------------------------------------------------------------
// Grids!
// ---------------------------------------------------------------------------------
template<typename T>
struct GridTypes;

template<>
struct GridTypes<float>{
    using GridType = openvdb::FloatGrid::Ptr;

    static float from_grid(float val) noexcept { return val; }
    static float to_grid(float val) noexcept { return val; }
};

template<>
struct GridTypes<Color>{
    using GridType = openvdb::Vec3SGrid::Ptr;

    static Color from_grid(const openvdb::Vec3s& val) noexcept {
        return Color(val.x(), val.y(), val.z(), 0);
    }

    static openvdb::Vec3s to_grid(const Color& col) noexcept {
        return {(float)col.red(), (float)col.green(), (float)col.blue()};
    }
};

template <typename T>
using GridPtr = typename GridTypes<T>::GridType;

template <typename T>
using Grid = typename GridPtr<T>::element_type;

// Note -> Grid _should_ be a ptr
template<typename T>
VSPtr<T> make_grid_field(const typename GridTypes<T>::GridType grid){
    auto eval_func = [grid](const Vector& p){
        auto index = grid->worldToIndex(openvdb::Vec3d(p.X(), p.Y(), p.Z()));
        auto val = openvdb::tools::BoxSampler::sample(grid->tree(), index);
        return GridTypes<T>::from_grid(val);
    };
    return funcfield<T>(eval_func);
}



// ---------------------------------------------------------------------------------
// Volumes!
// ---------------------------------------------------------------------------------

template <typename T>
VSPtr<T> make_warp_field(const VSPtr<T>& f, const VSPtr<Vector> map){
    auto eval = [f, map](const Vector& p){
        return f->eval(map->eval(p));
    };
    auto grad = [f, map](const Vector& p){
        return map->grad(p) * f->grad(map->eval(p));
    };
    return funcfield<T>(eval, grad);
}

// The identity field has eval f(x) = x and grad = Identity
//template <typename Vec>
VSPtr<Vector> make_identity_field();
VSPtr<Vector> make_grad_field(const vspf& f);
// VSPtr<float> make_nearpoint_transform(const vspf& f);
VSPtr<Vector> makeNPT(const vspf& f);
VSPtr<Vector> make_iteratedNPT(const vspf& f, int iters);
VSPtr<Vector> make_eval_dot_grad_over_gradsq(const vspf& f);



} // end namespace lux



