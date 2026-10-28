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
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> variation(0.01f, 7.0f);
    const float distance = (end - start).magnitude();
    if (distance == 0) return initial;
    const auto dir = (end - start).unitvector();
    float smax = (end - start).magnitude();
    float s = 0;
    while (s < smax){
        const float ds = std::min(step_size * variation(rng), distance - s);
        auto pos = start + s * dir;
        initial += func(pos) * ds;
        const float remaining = smax - s;
        s += ds;
    }
    return initial;
}


} // end namespace lux






