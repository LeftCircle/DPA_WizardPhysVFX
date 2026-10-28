#include "field_interface.h"
#include "LinearAlgebra.h"

namespace lux{



VolumeSPtr<float> exp(const VolumeSPtr<float>& a){
    return std::make_shared<ExpField>(a);
}

VolumeSPtr<float> log(const VolumeSPtr<float>& a){
    return std::make_shared<LogField>(a);
}

VolumeSPtr<float> sin(const VolumeSPtr<float>& a){
    return std::make_shared<SinField>(a);
}

VolumeSPtr<float> cos(const VolumeSPtr<float>& a){
    return std::make_shared<CosField>(a);
}

vspf abs(const vspf& a){
    auto eval = [a](const Vector& v){
        return std::abs(a->eval(v));
    }; 
    return funcfield<float>(eval);
}

VolumeSPtr<float> pow(const VolumeSPtr<float>& a, const VolumeSPtr<float>& to_power){
    return std::make_shared<PowField>(a, to_power);
}

vspf pow(const vspf& a, float power){
    auto eval = [a, power](const Vector& v){
        return std::pow(a->eval(v), power);
    };
    auto grad = [a, power](const Vector& p){
        return power * std::pow(a->eval(p), power - 1.0) * a->grad(p);
    };
    return funcfield<float>(eval, grad);
}

vspf union_fields(const vspf a, const vspf b){
    return std::make_shared<UnionField>(std::move(a), std::move(b));
}

vspf intersection(const vspf a, const vspf b){
    return std::make_shared<IntersectionField>(std::move(a), std::move(b));
}

vspf cutout(const vspf a, const vspf b){
    return std::make_shared<CutoutField>(std::move(a), std::move(b));
}

vspf clamp(const vspf a, const vspf min, const vspf max){
    return std::make_shared<ClampField>(std::move(a), std::move(min), std::move(max));
}

vspf clamp_fixed(const vspf a, float min, float max){
    auto eval = [a, min, max](const Vector& p){return std::clamp(a->eval(p), min, max); };
    return funcfield<float>(eval);
}

vspf mask(const vspf a){
    return std::make_shared<MaskField>(std::move(a));
}

vspf blinn_blend(std::shared_ptr<const std::vector<vspf>> fields, float blend_factor, float shape_broadness){
    return std::make_shared<BlinnBlendField>(std::move(fields), blend_factor, shape_broadness);
}

vspf dilation(vspf a, float d){
    auto eval_func = [a, d](const Vector& p){ return a->eval(p) - d ;};
    auto grad_func = [a](const Vector& p){ return a->grad(p); };
    return funcfield<float>(eval_func, grad_func);
}

vspf shell(vspf a, float d){
    d /= 2.0;
    auto outer = dilation(a, d);
    auto inner = dilation(a, -d);
    return cutout(outer, inner);
}


float smoothstep(float edge0, float edge1, float x){
    if (edge0 == edge1) {
        return x < edge0 ? 0.0f : 1.0f;
    }

    float t = (x - edge0) / (edge1 - edge0);
    t = std::clamp(t, 0.0f, 1.0f);

    return t * t * (3.0f - 2.0f * t);
}

Vector smoothstep_lerp(
    const Vector& a,
    const Vector& b,
    float t
){
    t = smoothstep(0.0f, 1.0f, t);
    return a + (b - a) * t;
}

vspf make_pyro(
    const vspf& f,
    float r, // roughness
    float octaves,
    const Vector& translation,
    float freq,
    float fjump,
    float amp,
    float gamma,
    int iters
){
    auto fspn = make_fspn(r, octaves, translation, freq, fjump);
    auto iter_npt = make_iteratedNPT(f, iters);
    auto fspn_of_iternpt = make_warp_field(fspn, iter_npt);
    return f + pow(abs(fspn_of_iternpt), gamma) * make_constant<float>(amp);
}


// ---------------------------------------------------------------------------------
// fields!
// ---------------------------------------------------------------------------------


VolumeSPtr<float> make_plane(const Vector& point, const Vector& normal) {
    return std::make_shared<PlaneField>(point, normal);
}
    

VolumeSPtr<float> isf_sphere(const Vector& center, const float radius){
    return std::make_shared<SphereField>(center, radius);
}

VolumeSPtr<float> isf_torus(const Vector& center, float r_major, float r_minor, const Vector& n_hat){
    return std::make_shared<TorusField>(center, r_major, r_minor, n_hat);
}

VolumeSPtr<float> isf_cone(const Vector& x0, const Vector& nhat, float height, float theta){
    return std::make_shared<ConeField>(x0, nhat, height, theta);
}

VolumeSPtr<float> isf_box(const Vector& center, float radius, int rounding_exponent){
    return std::make_shared<BoxField>(center, radius, rounding_exponent);
}


VolumeSPtr<float> isf_icosahedron(const Vector& center){
    return std::make_shared<IcosahedronField>(center);
}

VolumeSPtr<float> isf_steiner_patch(const Vector& center){
    return std::make_shared<SteinerPatchField>(center);
}

VolumeSPtr<float> isf_ellipse(const Vector& center, const Vector& normal, float r_major, float r_minor){
    return std::make_shared<EllipseField>(center, normal, r_major, r_minor);
}

VolumeSPtr<float> isf_cylinder(const Vector& center, const Vector& normal, float r, float h){
    vspf cyl = std::make_shared<CylinderField>(center, normal, r, h);
    return cyl;
}


// ---------------------------------------------------------------------------------
// Vector fields!
// ---------------------------------------------------------------------------------

VSPtr<Vector> make_identity_field(){
    auto eval = [](const Vector& v){
        return v;
    };
    auto grad = [](const Vector& v){
        return unitMatrix();
    };
    return funcfield<Vector>(eval, grad);
}

VSPtr<Vector> make_grad_field(const vspf& f){
    return funcfield<Vector>([f](const Vector& v){ return f->grad(v); });
}

// VSPtr<float> make_nearpoint_transform(const vspf& f){
//     auto eval = [f](const Vector& p){
//         Vector grad_f = f->grad(p);
//         auto mag_sq_gradf = grad_f.magnitude_squared();
//         return p - (f->eval(p) * grad_f) / mag_sq_gradf;
//     };
//     return funcfield<float>(eval);
// }

VSPtr<Vector> make_eval_dot_grad_over_gradsq(const vspf& f){
    auto eval = [f](const Vector& v){
        auto grad_f = f->grad(v);
        return (f->eval(v) * grad_f) / (grad_f * grad_f);
    };
    return funcfield<Vector>(eval);
}

VSPtr<Vector> makeNPT(const vspf& f){
    return make_identity_field() - make_eval_dot_grad_over_gradsq(f);
}

VSPtr<Vector> make_iteratedNPT(const vspf& f, int iters){
    auto npt = makeNPT(f);
    auto result = make_identity_field();
    for(int i = 0; i < iters; i++){
        result = make_warp_field(result, npt);
    }
    return result;
}


} // end namespace lux
