#include "catch_amalgamated.hpp"
#include "catch_helpers.h"

#include <map>
#include <vector>

#include "dynamical_state_data.h"
#include "Vector.h"
#include "partial_solvers.h"
#include "GISolver.h"
#include "collision_object.h"
#include "collision_surface.h"
#include "collision_handler.h"

using namespace pba;


TEST_CASE( "Test advance position "){
    auto dsd = create_dynamical_state_data();
    dsd->add(10);
    dsd->set_velocity(0, Vector(1.0, 0.0, 0.0));
    GISolver_sp GIAdvancePosition = std::make_shared<PartialSolverAdvancePosition>(dsd);
    GIAdvancePosition->init();
    const double dt = 0.1;
    GIAdvancePosition->solve(dt);

    REQUIRE(dsd->get_velocity(0) == Vector(1.0, 0.0, 0.0));

}

TEST_CASE(" Handle Collision With Static Plane") {
    // Test that a particle colliding with a plane rebounds correctly
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 1.0, 0.0));
    dsd->set_velocity(0, Vector(0.0, -1.0, 0.0));

    // Create a plane at a 45 degree angle
    auto collision_plane_sp = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    
    // Create a collision surface and register the plane to it
    auto collision_surface_sp = std::make_shared<CollisionSurface>();
    collision_surface_sp->add_collision_object(collision_plane_sp);

    // Create a collision handler with the collision surface. 
    auto collision_handler_sp = create_collision_handler();
    collision_handler_sp->register_collision_surface(collision_surface_sp);

    // Create a solver that advances positions and handles collisions
    GISolver_sp advance_with_collisions = create_advance_position_with_collisions(dsd, collision_handler_sp);

    advance_with_collisions->init();
    const double dt = 3.0;
    advance_with_collisions->solve(dt);
    // After collision, the particle should have bounced off the plane
    // Assume fully elastic collision for this test
    Vector expected_position = Vector(0.0, 2.0, 0.0);
    Vector expected_velocity = Vector(0.0, 1.0, 0.0);
    REQUIRE(dsd->get_position(0) == expected_position);
    REQUIRE(dsd->get_velocity(0) == expected_velocity);
}

TEST_CASE(" partial solver with leapfrog "){
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 0.0, 0.0));
    dsd->set_velocity(0, Vector(1.0, 0.0, 0.0));
    dsd->set_acceleration(0, Vector(0.0, -9.8, 0.0));
    
    // A standard leapfrog solver does position -> vel -> position
    auto pos_solv = std::make_shared<PartialSolverAdvancePosition>(dsd);
    auto grav = std::make_shared<SimpleGravityForce>(Vector(0.0, -9.8, 0.0));
    auto fs = std::make_shared<ForceSystem>();
    fs->add_force(grav);
    auto vel_solv = std::make_shared<AdvanceVelocityWithForces>(dsd, fs);
    auto leapfrog_solver = std::make_shared<GISolverLeapfrog>(pos_solv, vel_solv);   
    const double dt = 1.0;
    
    auto sixth_order_solver = std::make_shared<GISolverSixthOrder>(leapfrog_solver);
    sixth_order_solver->init();
    sixth_order_solver->solve(dt);

    REQUIRE_VECTOR_APPROX(dsd->get_position(0), Vector(1.0, -4.9, 0.0), 1e-5);
    REQUIRE_VECTOR_APPROX(dsd->get_velocity(0), Vector(1.0, -9.8, 0.0), 1e-5);
}