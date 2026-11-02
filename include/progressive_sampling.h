#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lux {

// Bit-reversal spreads early samples across the axis. Prefixes remain nested,
// so a pixel sampled by an earlier pass is never marched a second time.
inline std::vector<int> progressive_axis_order(int length) {
    if (length <= 0) throw std::invalid_argument("progressive axis length must be positive");
    std::uint64_t capacity = 1;
    int bits = 0;
    while (capacity < static_cast<std::uint64_t>(length)) { capacity <<= 1; ++bits; }
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(length));
    for (std::uint64_t value = 0; value < capacity; ++value) {
        auto input = value;
        std::uint64_t reversed = 0;
        for (int bit = 0; bit < bits; ++bit) {
            reversed = (reversed << 1) | (input & 1);
            input >>= 1;
        }
        if (reversed < static_cast<std::uint64_t>(length))
            order.push_back(static_cast<int>(reversed));
    }
    return order;
}

inline int progressive_axis_count(int length, int pass) {
    if (length <= 0 || pass < 1 || pass > 10)
        throw std::invalid_argument("invalid progressive pass");
    return static_cast<int>((static_cast<std::int64_t>(length) * pass + 9) / 10);
}

} // namespace lux
