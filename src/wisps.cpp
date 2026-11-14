#include "wisps.h"
#include "helper_functions.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lux{

void normalize_wisp_color_grid(const openvdb::FloatGrid::Ptr& density,
                               const openvdb::Vec3SGrid::Ptr& color) {
    if (!density || !color || density->transform() != color->transform())
        throw std::invalid_argument("wisp density and color grids must have matching transforms");
    auto density_accessor = density->getConstAccessor();
    for (auto it = color->beginValueOn(); it; ++it) {
        const float value = density_accessor.getValue(it.getCoord());
        it.setValue(value != 0.0f ? *it / value : openvdb::Vec3s(0.0f));
    }
}

CoordinateSys CoordinateSys::lerp(const CoordinateSys& other, float t) const {
    auto interp = [](const Vector& coord, const Vector& other_coord, float t){
        const auto cm = coord.magnitude();
        const auto other_cm = other_coord.magnitude();
        const auto a = coord / cm, b = other_coord / other_cm;
        const double dot = std::clamp(double(a * b), -1.0, 1.0);
        Vector direction;
        if (dot > 0.999999) {
            direction = (a * (1.0 - t) + b * t).unitvector();
        } else if (dot < -0.999999) {
            const Vector ref = std::abs(a.X()) < 0.9 ? Vector(1,0,0) : Vector(0,1,0);
            const Vector perpendicular = (ref - a * (ref * a)).unitvector();
            const double angle = std::acos(-1.0) * t;
            direction = a * std::cos(angle) + perpendicular * std::sin(angle);
        } else {
            const double angle = std::acos(dot), denom = std::sin(angle);
            direction = a * (std::sin((1.0-t)*angle)/denom) + b * (std::sin(t*angle)/denom);
        }
        return direction * (cm * (1.0 - t) + other_cm * t);
    };
    return CoordinateSys(
        interp(this->t, other.t, t),
        interp(this->n, other.n, t),
        interp(this->b, other.b, t)
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
        1,
        a.color * (1.0 - t) + b.color * t
    );
}


WispSpline::WispSpline(
        const std::vector<WispGuideParticle>& guide_particles,
        size_t n_dots,
        float voxel_size,
        int seed
) : _guide_particles(guide_particles), _n_dots(n_dots), _voxel_size(voxel_size), _seed(seed) 
{
    if (_guide_particles.size() < 2 || _n_dots == 0 || !std::isfinite(_voxel_size) || _voxel_size <= 0)
        throw std::invalid_argument("wisp spline needs at least two guides, positive dot count and voxel size");
    // determine the length of the segment
    float l = 0;
    _cumulative_lengths.reserve(_guide_particles.size());
    _cumulative_lengths.push_back(0.0f);
    for (size_t i = 0; i + 1 < _guide_particles.size(); i++){
        const auto length = (_guide_particles[i + 1].location - _guide_particles[i].location).magnitude();
        if (!std::isfinite(length) || length <= 1e-10)
            throw std::invalid_argument("wisp spline neighboring guides must have distinct finite positions");
        const float next = l + static_cast<float>(length);
        if (!(next > l)) throw std::invalid_argument("wisp spline segment too short for float precision");
        l = next;
        _cumulative_lengths.push_back(l);
    };
    _spline_length = l;
    if (!std::isfinite(_spline_length)) throw std::invalid_argument("invalid wisp spline length");
    for (const auto& guide : _guide_particles) {
        for (const auto& axis : {guide.axis.t, guide.axis.n, guide.axis.b})
            if (!std::isfinite(axis.magnitude()) || axis.magnitude() < 1e-10)
                throw std::invalid_argument("wisp guide axes must be finite and nonzero");
    }
}

vspf WispSpline::stamp_wisp_spline(){
    return make_grid_field<float>(stamp_wisp_spline_grid());
}

openvdb::FloatGrid::Ptr WispSpline::stamp_wisp_spline_grid(openvdb::Vec3SGrid::Ptr color){
    auto grid = create_float_grid(_voxel_size, 0.0f);
    if (color && color->transform() != grid->transform())
        throw std::invalid_argument("wisp density and color grids must have matching transforms");
    auto accessor = grid->getAccessor();
    auto color_accessor = color ? std::make_unique<openvdb::Vec3SGrid::Accessor>(color->getAccessor()) : nullptr;
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

        _stamp_one_wisp_dot(params, rng, accessor, transform, color_accessor.get());
    }
    return grid;
}

std::pair<openvdb::FloatGrid::Ptr, openvdb::Vec3SGrid::Ptr> WispSpline::stamp_wisp_spline_colored() {
    auto color = create_color_grid(_voxel_size, openvdb::Vec3s(0.0f));
    auto density = stamp_wisp_spline_grid(color);
    normalize_wisp_color_grid(density, color);
    return {density, color};
}


std::pair<int, float> WispSpline::_locate_segment(float t){
    const float dist = _spline_length * std::clamp(t, 0.0f, 1.0f);
    auto end = std::upper_bound(_cumulative_lengths.begin(), _cumulative_lengths.end(), dist);
    size_t i = end == _cumulative_lengths.end() ? _guide_particles.size() - 2 :
               static_cast<size_t>(end - _cumulative_lengths.begin() - 1);
    const float u = (dist - _cumulative_lengths[i]) / (_cumulative_lengths[i+1] - _cumulative_lengths[i]);
    return {static_cast<int>(i), u};
}

} // end namespace lux
