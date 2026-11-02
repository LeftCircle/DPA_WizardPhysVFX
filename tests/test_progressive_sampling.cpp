#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <vector>

#include "progressive_sampling.h"

using namespace lux;

TEST_CASE("progressive passes sample each full-size pixel once") {
    for (const auto dimensions : {std::array<int, 2>{1, 1}, {11, 7}, {20, 10}, {31, 19}}) {
        const int width = dimensions[0], height = dimensions[1];
        const auto xs = progressive_axis_order(width), ys = progressive_axis_order(height);
        auto sorted_x = xs, sorted_y = ys;
        std::sort(sorted_x.begin(), sorted_x.end());
        std::sort(sorted_y.begin(), sorted_y.end());
        for (int x = 0; x < width; ++x) REQUIRE(sorted_x[x] == x);
        for (int y = 0; y < height; ++y) REQUIRE(sorted_y[y] == y);
        std::vector<int> visits(width * height, 0);
        std::vector<std::array<int, 4>> image(width * height, {0, 0, 0, 0});
        int prev_x = 0, prev_y = 0;
        for (int pass = 1; pass <= 10; ++pass) {
            const int nx = progressive_axis_count(width, pass);
            const int ny = progressive_axis_count(height, pass);
            for (int iy = 0; iy < ny; ++iy) for (int ix = 0; ix < nx; ++ix) {
                if (ix < prev_x && iy < prev_y) continue;
                const int index = ys[iy] * width + xs[ix];
                ++visits[index];
                image[index] = {1, 2, 3, 4};
            }
            int active = 0;
            for (int index = 0; index < width * height; ++index) {
                REQUIRE(visits[index] <= 1);
                if (visits[index]) ++active;
                else { const std::array<int, 4> transparent_black{0, 0, 0, 0};
                       REQUIRE(image[index] == transparent_black); }
            }
            REQUIRE(active == nx * ny);
            prev_x = nx; prev_y = ny;
        }
        REQUIRE(std::all_of(visits.begin(), visits.end(), [](int n) { return n == 1; }));
    }
}

TEST_CASE("early progressive samples are spatially distributed") {
    const auto order = progressive_axis_order(16);
    const std::vector<int> first = {0, 8, 4, 12, 2, 10, 6, 14};
    REQUIRE(std::equal(first.begin(), first.end(), order.begin()));
    REQUIRE(progressive_axis_count(101, 1) == 11);
    REQUIRE(progressive_axis_count(101, 10) == 101);
}
