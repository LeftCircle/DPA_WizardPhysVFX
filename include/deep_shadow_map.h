#pragma once

#include "field_interface.h"
#include "lights.h"
#include "openvdb_helpers.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lux{

GridPtr<float> make_dsm_grid(
    const vspf density_field,
    float voxel_size,
    const coordbbox& bounds,
    const PointLight& light,
    float step_size,
    float kappa
);

vspf make_deep_shadow_map(
    vspf density_field,
    float voxel_size,
    const coordbbox& bounds,
    const PointLight& light,
    float step_size,
    float kappa
);

vspf make_deep_shadow_map(const GridPtr<float> grid);

vspf make_deep_shadow_map_parallel(
    const vspf density_field,
    float voxel_size,
    const coordbbox& bounds,
    const PointLight& light,
    float step_size,
    float kappa
);


template <typename AccumFunc>
float accumulate_over_steps(
    Vector start,
    Vector end,
    float step_size,
    AccumFunc func,
    float initial = 0
){
    if (!(step_size > 0) || !std::isfinite(step_size)) {
        throw std::invalid_argument("shadow step size must be finite and positive");
    }
    const float distance = (end - start).magnitude();
    if (distance == 0) return initial;
    const auto dir = (end - start).unitvector();
    float s = 0;
    while (s < distance) {
        const float ds = std::min(step_size, distance - s);
        const auto pos = start + (s + 0.5f * ds) * dir;
        initial += func(pos) * ds;
        s += ds;
    }
    return initial;
}


} // end namespace lux






