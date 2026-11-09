#include <catch2/catch_test_macros.hpp>


#include "field_interface.h"
#include "openvdb_helpers.h"

using namespace lux;



TEST_CASE("Test particle collision with vdb"){
    // particles should be able to collide with any sdf. Even procedural ones
    auto plane = isf_plane({}, {0, 1, 0});
    Vector start_pos(0, 1, 0);
    Vector vel(0, -3, 0);

    // in one timestep, the particle should move towards the plane, collide, 
    // and bounce back higher
    // We can register an sdf as a collision_object if we create a wrapper for sdfs. 
    // Then it should just work out of the box. (inherit from collision_object.h)


}


