#pragma once

#include "field_interface.h"
#include "lights.h"
#include "openvdb_helpers.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lux{

using ShadowRegion = std::pair<Vector, Vector>;

// Integrate only the union of regions, merging overlap to avoid double counting.
template <typename AccumFunc>
float accumulate_in_regions(Vector start, Vector end, float step, AccumFunc func,
                            const std::vector<ShadowRegion>& regions) {
    if (!(step > 0) || !std::isfinite(step)) throw std::invalid_argument("invalid shadow step");
    const auto delta = end - start;
    const float distance = delta.magnitude();
    if (distance == 0) return 0;
    const auto dir = delta.unitvector();
    const double p[3] = {start.X(), start.Y(), start.Z()};
    const double d[3] = {dir.X(), dir.Y(), dir.Z()};
    std::vector<std::pair<float, float>> intervals;
    for (const auto& region : regions) {
        const double lo[3] = {region.first.X(), region.first.Y(), region.first.Z()};
        const double hi[3] = {region.second.X(), region.second.Y(), region.second.Z()};
        double a = 0, b = distance;
        for (int axis = 0; axis < 3 && a <= b; ++axis) {
            if (std::abs(d[axis]) < 1e-12) { if (p[axis] < lo[axis] || p[axis] > hi[axis]) a = b + 1; }
            else { double x = (lo[axis]-p[axis])/d[axis], y = (hi[axis]-p[axis])/d[axis];
                if (x > y) std::swap(x,y); a = std::max(a,x); b = std::min(b,y); }
        }
        if (a < b) intervals.emplace_back(a,b);
    }
    std::sort(intervals.begin(), intervals.end());
    std::vector<std::pair<float,float>> merged;
    for (auto i : intervals) {
        if (merged.empty() || i.first > merged.back().second) merged.push_back(i);
        else merged.back().second = std::max(merged.back().second, i.second);
    }
    float result = 0;
    for (auto i : merged) {
        float s = i.first;
        while (s < i.second) {
            float ds = std::min(step, i.second - s);
            if (!(ds > 0) || s + ds == s) break;
            result += func(start + (s + 0.5f * ds) * dir) * ds;
            s += ds;
        }
    }
    return result;
}


// Preserve the six-argument API, including use as a function pointer.
GridPtr<float> make_dsm_grid(
    const vspf density_field, float voxel_size, const coordbbox& bounds,
    const PointLight& light, float step_size, float kappa
);

GridPtr<float> make_dsm_grid_in_regions(
    const vspf density_field, float voxel_size, const coordbbox& bounds,
    const PointLight& light, float step_size, float kappa,
    const std::vector<ShadowRegion>& regions
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






