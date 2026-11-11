#include "wisps.h"
#include "helper_functions.h"

namespace lux{

CoordinateSys CoordinateSys::lerp(const CoordinateSys& other, float t) const {
    auto theta_t = std::acos(this->that() * other.that());
    auto theta_n = std::acos(this->nhat() * other.nhat());
    auto theta_b = std::acos(this->bhat() * other.bhat());

    //auto tq = std::pow(this->t.magnitude(), omt) * std::pow(other.t.magnitude(), t) ;
    auto interp = [](Vector coord, Vector other_coord, float theta, float t){
        const auto cm = coord.magnitude();
        const auto other_cm = other_coord.magnitude();
        return std::pow(cm, 1.0 - t) * std::pow(other_cm, t) * 
            (coord / cm * std::cos(theta * t) + other_coord / other_cm * std::sin(theta * t));
    };
    return CoordinateSys(
        interp(this->t, other.t, theta_t, t),
        interp(this->n, other.n, theta_n, t),
        interp(this->b, other.b, theta_b, t)
    );
}

WispGuideParticle interpolate(
    const WispGuideParticle& a,
    const WispGuideParticle& b,
    float t
){
    return WispGuideParticle(
        1,
        lerp(a.dens, b.dens, t),
        lerp(a.clump, b.clump, t),
        lerp(a.pscale, b.pscale, t),
        lerp(a.fspn1, b.fspn1, t),
        lerp(a.fspn2, b.fspn2, t),
        lerp(a.displacement, b.displacement, t),
        lerp(a.location, b.location, t),
        a.axis.lerp(b.axis, t),
        1
    );
}


// auto wisp_uniform(
//     size_t n_dots,
//     float dens,
//     float clump,
//     float pscale,
//     FSVals fspn1,
//     FSVals fspn2,
//     Vector displacement,
//     Vector location,
//     CoordinateSys axis,
//     int id,
//     Color col = Color(1, 1, 1, 1),
//     float min = -1,
//     float max = 1
// ){
//     auto rng = [
//         engine = std::mt19937{static_cast<std::mt19937::result_type>(id)},
//         distribution = std::uniform_real_distribution<float>{}
//     ](float min, float max) mutable {
//         using Params = decltype(distribution)::param_type;
//         return distribution(engine, Params{min, max});
//     };
//     return Wisp<decltype(rng)>(
//         rng,
//         n_dots,
//         dens,
//         clump,
//         pscale,
//         std::move(fspn1),
//         std::move(fspn2),
//         std::move(displacement),
//         std::move(location),
//         std::move(axis),
//         id,
//         col,
//         min,
//         max
//     );
// };

} // end namespace lux
