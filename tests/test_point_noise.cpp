#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "point_noise_stamping.h"
#include "wisps.h"

using namespace lux;

TEST_CASE("point noise accumulates and normalizes overlapping colors") {
    auto density = create_float_grid(.25f, 0);
    auto color = create_color_grid(.25f);
    auto plain = create_float_grid(.25f, 0);
    stamp_point_noise(density, Vector(), 2, .5, 1, .5, 3, 1, 2,
                      Vector(), 0, color, Color(1,0,0,1));
    stamp_point_noise(density, Vector(), 2, .5, 3, .5, 3, 1, 2,
                      Vector(), 0, color, Color(0,0,1,1));
    stamp_point_noise(plain, Vector(), 2, .5, 4, .5, 3, 1, 2);
    REQUIRE(density->activeVoxelCount() > 0);
    normalize_wisp_color_grid(density, color);
    for (auto it = color->cbeginValueOn(); it; ++it) {
        const auto c = *it;
        REQUIRE(c.x() == Catch::Approx(.25f).margin(1e-5));
        REQUIRE(c.y() == Catch::Approx(0).margin(1e-5));
        REQUIRE(c.z() == Catch::Approx(.75f).margin(1e-5));
        REQUIRE(density->tree().getValue(it.getCoord()) == Catch::Approx(plain->tree().getValue(it.getCoord())));
    }
    auto wrong_transform = create_color_grid(.5f);
    REQUIRE_THROWS_AS(stamp_point_noise(density, Vector(), 2, .5, 1, .5, 3, 1, 2,
                                       Vector(), 0, wrong_transform), std::invalid_argument);
}
