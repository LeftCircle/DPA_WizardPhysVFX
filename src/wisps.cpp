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


WispSpline::WispSpline(
        const std::vector<WispGuideParticle>& guide_particles,
        size_t n_dots,
        float voxel_size,
        int seed
) : _guide_particles(guide_particles), _n_dots(n_dots), _voxel_size(voxel_size), _seed(seed) 
{
    // determine the length of the segment
    float l = 0;
    for (int i = 0; i < _guide_particles.size() - 1; i++){
        l += (_guide_particles[i + 1].location - _guide_particles[i].location).magnitude();
    };
    _spline_length = l;
}

vspf WispSpline::stamp_wisp_spline(){
    auto grid = create_float_grid(_voxel_size, 0.0001);
    auto accessor = grid->getAccessor();
    const auto& transform = grid->transform();
    auto rng = [
        engine = std::mt19937{static_cast<std::mt19937::result_type>(_seed)},
        distribution = std::uniform_real_distribution<float>{}
    ](float min, float max) mutable {
        using Params = decltype(distribution)::param_type;
        return distribution(engine, Params{min, max});
    };
    for (size_t i = 0; i < _n_dots; ++i) {
        float t = rng(0, 1);

        // Locate neighboring guides and the local parameter u.
        auto [segment, u] = _locate_segment(t);

        // A small, stack-allocated value: no heap allocation.
        auto params = interpolate(_guide_particles[segment], _guide_particles[segment + 1], u);

        _stamp_one_wisp_dot(params, rng, accessor, transform);
    }
}


std::pair<int, float> WispSpline::_locate_segment(float t){
    float dist = _spline_length * t;
    float accumulated_length = 0;
    for (int i = 0; i < _guide_particles.size() - 1; i++){
        float segment_length = (_guide_particles[i + 1].location - _guide_particles[i].location).magnitude();
        if (accumulated_length + segment_length >= dist){
            float u = (dist - accumulated_length) / segment_length;
            return {i, u};
        }
        accumulated_length += segment_length;
    }
    return {_guide_particles.size() - 2, 1.0f};
}

} // end namespace lux
