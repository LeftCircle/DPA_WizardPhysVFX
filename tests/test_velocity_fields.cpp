#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "field_interface.h"
#include "openvdb_helpers.h"

using namespace lux;

TEST_CASE("Test stamping a warp field"){
    // Warp fields are special cases. Outside of the grid we should
    // return the identity, while inside the grid we should return 
    // the deviation from the identity. 

    // -> how we interact with stamped warp fields is also important then. 
    // We probably want to return the warped value, not the deviation from 
    // identity. 

    // Start with a basic warp that is npt to a sphere
    Vector center(3, 4, 5);
    float radius = 1.0;
    Vector rvec = Vector(1.0, 1.0, 1.0) * radius;
    auto sphere = isf_sphere(center, radius);
    auto map = makeNPT(sphere);
    auto grid_spacing = 0.25;
    auto bounds = world_space_to_bounds(center - 2.0 * rvec, center +  2.0 * rvec, grid_spacing);

    auto stamped_map = stamp_map_to_grid(map, bounds, grid_spacing);
    auto map_field = make_grid_field_map(stamped_map);

    auto identity_warped = make_warp_field<Vector>(make_identity_field(), map_field);

    REQUIRE(identity_warped->eval(center + Vector(grid_spacing, 0, 0)).x() == Catch::Approx(center[0] + radius));
    auto out_eval = identity_warped->eval({100, 0, 0});
    REQUIRE(out_eval.x() == 100.0f);
    REQUIRE(out_eval.y() == 0.0f);
    REQUIRE(out_eval.z() == 0.0f);
}


// TEST_CASE("Test generating arbitrary sdf"){
//     // Create an arbitrary sdf from 
//     // fsdf(x) = |x - INPT(f, N)| * sign(f(x))
//     REQUIRE(false, "to do");
// }