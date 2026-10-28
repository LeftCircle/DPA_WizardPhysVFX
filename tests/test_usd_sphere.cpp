#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "deep_shadow_map.h"
#include "field_interface.h"
#include "render_settings.h"

using namespace lux;

TEST_CASE("USD sphere field and density signs") {
    auto sphere = isf_sphere(Vector(2, 0, 0), 1.5f);
    auto density = -mask(sphere);
    REQUIRE(sphere->eval(Vector(2, 0, 0)) == Catch::Approx(-1.5f));
    REQUIRE(sphere->eval(Vector(3.5, 0, 0)) == Catch::Approx(0));
    REQUIRE(sphere->eval(Vector(4, 0, 0)) > 0);
    REQUIRE(density->eval(Vector(2, 0, 0)) == Catch::Approx(-1));
    REQUIRE(density->eval(Vector(4, 0, 0)) == Catch::Approx(0));
}

TEST_CASE("shadow integration includes the final partial step") {
    auto value = accumulate_over_steps(Vector(0, 0, 0), Vector(1, 0, 0), 0.3f,
        [](const Vector&) { return -2.0f; });
    REQUIRE(value == Catch::Approx(-2.0f));
}

TEST_CASE("shadow voxel size is independent of color voxel size") {
    RenderSettings rs;
    rs.set_color_vs(0.2f);
    rs.set_shadow_vs(0.5f);
    REQUIRE(rs.get_shadow_vs() == Catch::Approx(0.5f));
}
