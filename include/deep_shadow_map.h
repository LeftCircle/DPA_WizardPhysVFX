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
    float smax = (end - start).magnitude();
    if (smax == 0.0f) return initial;
    const auto dir = (end - start).unitvector();
    float s = 0;
    while (s < smax){
        const float ds = std::min(step_size, smax - s);
        auto pos = start + (s + 0.5f * ds) * dir;
        initial += func(pos) * ds;
        s += ds;
    }
    return initial;
}


} // end namespace lux





