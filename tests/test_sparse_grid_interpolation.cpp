#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "sparse_grid_interpolation.h"

namespace {
class TestImage {
    int width_, height_, channels_;
    std::vector<float> pixels_;
public:
    TestImage(int w, int h, int c, float value = 0)
        : width_(w), height_(h), channels_(c), pixels_(w*h*c, value) {}
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    int get_channels() const { return channels_; }
    float get_pixel_value(int x, int y, int c) const { return pixels_[(y*width_+x)*channels_+c]; }
    void set_pixel_value(int x, int y, int c, float value) { pixels_[(y*width_+x)*channels_+c] = value; }
};
}

TEST_CASE("interpolation fills only a separate preview from actual samples") {
    TestImage src(3, 3, 4, -42), out(3, 3, 4);
    for (int y : {0, 2}) for (int x : {0, 2}) {
        src.set_pixel_value(x, y, 0, float(x + 2*y));
        src.set_pixel_value(x, y, 1, float(2*x + y));
        src.set_pixel_value(x, y, 2, 0);
        src.set_pixel_value(x, y, 3, x == 0 && y == 0 ? 0 : 1);
    }
    image_processing::interpolate_sparse_grid(src, out, {2, 0}, {2, 0});
    REQUIRE(out.get_pixel_value(1, 1, 0) == 3);
    REQUIRE(out.get_pixel_value(1, 1, 1) == 3);
    REQUIRE(out.get_pixel_value(1, 1, 3) == 0.75f);
    REQUIRE(out.get_pixel_value(0, 0, 3) == 0); // valid transparent black, not missing
    REQUIRE(out.get_pixel_value(2, 2, 0) == 6);  // sampled values are preserved
    REQUIRE(src.get_pixel_value(1, 1, 0) == -42); // source was not changed
}

TEST_CASE("interpolation clamps edges and handles one sample") {
    TestImage src(5, 5, 1, -99), out(5, 5, 1);
    src.set_pixel_value(1, 1, 0, 7); src.set_pixel_value(3, 1, 0, 9);
    src.set_pixel_value(1, 3, 0, 11); src.set_pixel_value(3, 3, 0, 13);
    image_processing::interpolate_sparse_grid(src, out, {1, 3}, {1, 3});
    REQUIRE(out.get_pixel_value(0, 0, 0) == 7);
    REQUIRE(out.get_pixel_value(4, 4, 0) == 13);
    REQUIRE(out.get_pixel_value(2, 2, 0) == 10);
    image_processing::interpolate_sparse_grid(src, out, {1}, {1});
    REQUIRE(out.get_pixel_value(4, 4, 0) == 7);
    REQUIRE_THROWS_AS(image_processing::interpolate_sparse_grid(src, src, {1}, {1}), std::invalid_argument);
    REQUIRE_THROWS_AS(image_processing::interpolate_sparse_grid(src, out, {1, 1}, {1}), std::invalid_argument);
}
