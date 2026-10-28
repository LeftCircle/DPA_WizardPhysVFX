#pragma once

#include <utility>

#include "Vector.h"

namespace lux{

struct FSVals{
    FSVals() = default;
    FSVals(
        float r_,
        float octaves_,
        Vector translation_,
        float freq_,
        float fjump_
    ) : r(r_), octaves(octaves_), translation(std::move(translation_)), freq(freq_), fjump(fjump_) {}
    float r = 0.5;
    float octaves = 2.0;
    Vector translation = {0, 0, 0};
    float freq = 1.0;
    float fjump = 1.0;
};

template <typename T, typename Vec, typename Func>
constexpr T fractal_sum(
    const Vec& eval_point,
    Func&& func,
    float r, // roughness
    float octaves,
    const Vec& translation,
    float freq,
    float fjump
) {
    octaves = std::max(octaves, 1.0f);
    int n0 = (int)std::floor(octaves);
    auto nf = octaves - n0;
    auto scale = (1.0 - r) / (1.0 - std::pow(r, octaves));
    scale = r == 1 ? 1.0 / octaves : scale; // handle division by zero for r == 1;
    const auto x = eval_point - translation;
    T result{};
    for (int i = 0; i < n0; i++){
        result += std::pow(r, i) * func(x * freq * std::pow(fjump, i));
    }
    // add the fractional part
    result += nf * std::pow(r, octaves - 1) * func(x * freq * std::pow(fjump, octaves - 1));
    return scale * result;
}

template <typename T, typename Vec, typename Func>
constexpr T fractal_sum(const Vec& eval_point, Func&& func, const FSVals& vals) {
    return fractal_sum<T, Vec>(eval_point, std::forward<Func>(func), vals.r,
        vals.octaves, vals.translation, vals.freq, vals.fjump);
}



} // end namespace lux



