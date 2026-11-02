#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace image_processing {

namespace detail {
struct AxisSpan { int lo, hi; float t; };

inline std::vector<AxisSpan> sparse_axis_spans(int length, std::vector<int> samples) {
    if (length <= 0 || samples.empty()) throw std::invalid_argument("empty sparse sample grid");
    std::sort(samples.begin(), samples.end());
    if (samples.front() < 0 || samples.back() >= length ||
        std::adjacent_find(samples.begin(), samples.end()) != samples.end())
        throw std::invalid_argument("invalid sparse sample coordinates");
    std::vector<AxisSpan> spans;
    spans.reserve(static_cast<std::size_t>(length));
    for (int pos = 0; pos < length; ++pos) {
        const auto high = std::lower_bound(samples.begin(), samples.end(), pos);
        if (high == samples.begin()) spans.push_back({*high, *high, 0});
        else if (high == samples.end()) spans.push_back({samples.back(), samples.back(), 0});
        else if (*high == pos) spans.push_back({pos, pos, 0});
        else {
            const int lo = *(high - 1), hi = *high;
            spans.push_back({lo, hi, float(pos - lo) / float(hi - lo)});
        }
    }
    return spans;
}

inline float lerp(float a, float b, float t) {
    if (t == 0) return a; // preserve actual samples, including zero and NaN
    if (t == 1) return b;
    return a + (b - a) * t;
}
}

// The source's sampled positions are the Cartesian product of xs and ys;
// their values, including transparent black, are valid measurements. Samples
// outside that set are ignored. Interpolate into a separate image so the source
// remains suitable for later progressive passes. Values beyond the outside
// samples clamp to the nearest sample. All channels, including alpha, are used.
template <typename Image>
void interpolate_sparse_grid(const Image& source, Image& output,
                             const std::vector<int>& xs, const std::vector<int>& ys) {
    if (&source == &output || source.get_width() != output.get_width() ||
        source.get_height() != output.get_height() || source.get_channels() != output.get_channels())
        throw std::invalid_argument("sparse interpolation needs distinct images of equal size");
    const int width = source.get_width(), height = source.get_height(), channels = source.get_channels();
    const auto xspans = detail::sparse_axis_spans(width, xs);
    const auto yspans = detail::sparse_axis_spans(height, ys);
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        const auto sy = yspans[y];
        for (int x = 0; x < width; ++x) {
            const auto sx = xspans[x];
            for (int c = 0; c < channels; ++c) {
                const float top = detail::lerp(source.get_pixel_value(sx.lo, sy.lo, c),
                                               source.get_pixel_value(sx.hi, sy.lo, c), sx.t);
                const float bottom = detail::lerp(source.get_pixel_value(sx.lo, sy.hi, c),
                                                  source.get_pixel_value(sx.hi, sy.hi, c), sx.t);
                output.set_pixel_value(x, y, c, detail::lerp(top, bottom, sy.t));
            }
        }
    }
}

} // namespace image_processing
