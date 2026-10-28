#include "catch2/catch_amalgamated.hpp"
#include "catch_helpers.h"


#include "rigid_body.h"

using namespace pba;

using RB = RigidBodyStateData; 

TEST_CASE("test compute COM on add/resize"){
	RB rb = RB();

	REQUIRE(rb.center_of_mass == Vector(0.0, 0.0, 0.0));
	rb.add();
	rb.add();
	rb.set_initial_position(0, Vector(0, 0, 0));
	rb.set_initial_position(1, Vector(10, 0, 0));
	rb.init_rbd();
	REQUIRE(rb.center_of_mass == Vector(5, 0, 0));
	rb.add(50);
	rb.init_rbd();
	REQUIRE(rb.center_of_mass.X() < 5.0);
	REQUIRE(rb.center_of_mass.X() > 0.0);
}


