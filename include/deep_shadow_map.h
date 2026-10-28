#pragma once

#include "field_interface.h"
#include "lights.h"
#include "openvdb_helpers.h"
#include <random>

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
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> variation(0.01f, 7.0f);

    const auto dir = (end - start).unitvector();
    float smax = (end - start).magnitude();
    float s = 0;
    while (s < smax){
        auto pos = start + s * dir;
        initial += func(pos);
        const float remaining = smax - s;
        const float ds = std::min(step_size * variation(rng), remaining);
        s += ds;
    }
    return initial;
}


} // end namespace lux





