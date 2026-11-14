#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "noise_fields.h"
#include "openvdb_helpers.h"

namespace lux {

// Accumulate negative density and density-weighted RGB. Call
// normalize_wisp_color_grid once after all point stamps have been accumulated.
inline void stamp_point_noise(const openvdb::FloatGrid::Ptr& grid, const Vector& center,
    float radius, float fade, float density, float roughness, float octaves,
    float frequency, float frequency_jump, const Vector& translation = Vector(), int seed = 0,
    const openvdb::Vec3SGrid::Ptr& color_grid = nullptr, const Color& color = Color(1, 1, 1, 1)) {
    auto valid = [](double v) { return std::isfinite(v); };
    if (!grid || !valid(center.x()) || !valid(center.y()) || !valid(center.z()) ||
        !valid(radius) || radius <= 0 || !valid(fade) || fade <= 0 ||
        !valid(density) || density < 0 || !valid(roughness) || roughness < 0 ||
        !valid(octaves) || octaves <= 0 || !valid(frequency) || frequency <= 0 ||
        !valid(frequency_jump) || frequency_jump <= 0 ||
        !valid(translation.x()) || !valid(translation.y()) || !valid(translation.z()) ||
        !valid(color.red()) || !valid(color.green()) || !valid(color.blue()) ||
        color.red() < 0 || color.green() < 0 || color.blue() < 0)
        throw std::invalid_argument("invalid noise stamp parameters");
    if (color_grid && color_grid->transform() != grid->transform())
        throw std::invalid_argument("point noise density and color grids must have matching transforms");
    const Vector extent(radius, radius, radius);
    const auto bounds = world_space_to_bounds(center - extent, center + extent, grid);
    // Perlin's permutation is fixed; choose a reproducible noise-space offset.
    auto hash = [](std::uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
        x *= 0x846ca68bu; x ^= x >> 16; return x;
    };
    auto component = [&](std::uint32_t salt) {
        return (double(hash(static_cast<std::uint32_t>(seed) ^ salt) & 0xffffu) / 512.0 - 64.0) / frequency;
    };
    const Vector seed_offset = seed == 0 ? Vector() : Vector(component(0x243f6a88u), component(0x85a308d3u), component(0x13198a2eu));
    const auto noise = make_fspn(roughness, octaves, translation + seed_offset, frequency, frequency_jump);
    const auto falloff = make_fade_factor(center, fade, radius);
    auto accessor = grid->getAccessor();
    auto colors = color_grid ? std::make_unique<openvdb::Vec3SGrid::Accessor>(color_grid->getAccessor()) : nullptr;
    const auto rgb = GridTypes<Color>::to_grid(color);
    for (int z = bounds.min().z(); z <= bounds.max().z(); ++z)
        for (int y = bounds.min().y(); y <= bounds.max().y(); ++y)
            for (int x = bounds.min().x(); x <= bounds.max().x(); ++x) {
                const openvdb::Coord coord(x, y, z);
                const auto wp = grid->indexToWorld(coord);
                const Vector p(wp.x(), wp.y(), wp.z());
                const float weight = falloff->eval(p);
                if (weight <= 0) continue;
                const float contribution = std::min(noise->eval(p), 0.0f) * weight * density;
                if (contribution < 0) {
                    accessor.setValueOn(coord, accessor.getValue(coord) + contribution);
                    if (colors) colors->setValueOn(coord, colors->getValue(coord) + rgb * contribution);
                }
            }
}

} // namespace lux
