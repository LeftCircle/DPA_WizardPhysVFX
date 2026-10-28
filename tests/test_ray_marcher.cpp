#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "ray_marcher.h"

using namespace lux;

TEST_CASE("test ray marcher image size"){

    ImageData rm;
    int test_width = 1920;
    int test_height = 1080;
    rm.set_dimensions(test_width, test_height, 1);
    REQUIRE(rm.get_width() == test_width);
    REQUIRE(rm.get_height() == test_height);
}
