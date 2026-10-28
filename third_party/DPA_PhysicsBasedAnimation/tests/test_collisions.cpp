#include "catch_amalgamated.hpp"
#include "catch_helpers.h"

#include <map>
#include <vector>
#include <iostream>

#include "dynamical_state_data.h"
#include "Vector.h"
#include "partial_solvers.h"
#include "GISolver.h"
#include "collision_object.h"

using namespace pba;


TEST_CASE( " Test Particle Plane Collision "){

    // Define a horizontal plane at y=0 with normal pointing up
    auto collision_plane_sp = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    auto other_plane_sp = create_collision_plane(Vector(0.0, -10.0, 0.0), Vector(0.0, 1.0, 0.0));
    const double dt = 1.0;
    const Vector initial_position = Vector(0.0, 1.0, 0.0);
    const Vector updated_position = Vector(0.0, -1.0, 0.0);
    const Vector velocity = (updated_position - initial_position) / dt;

    // The collision test should return the time that the collision occurs, or -1 if no collision
    CollisionHitInfo hit_info;
    CollisionHitInfo miss_info;
    collision_plane_sp->hit(initial_position, updated_position, velocity, dt, hit_info);
    REQUIRE_FALSE(other_plane_sp->hit(initial_position, updated_position, velocity, dt, miss_info));
    
    REQUIRE(hit_info.time_of_impact == 0.5);
    REQUIRE(hit_info.position == Vector(0.0, 0.0, 0.0));
    REQUIRE(hit_info.normal == Vector(0.0, 1.0, 0.0));   
}

TEST_CASE( " Test Multiple Collision Planes "){
    auto plane1 = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    auto plane2 = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(1.0, 0.0, 0.0));
    const double dt = 10.0;
    const Vector initial_pos = Vector(2.0, 1.0, 0.0);
    const Vector initial_velocity = Vector(-1.0, -1.0, 0.0);

    auto coll_surf = create_collision_surface();
    coll_surf->add_collision_object(plane1);
    coll_surf->add_collision_object(plane2);
    
    auto collision_handler = create_collision_handler();
    collision_handler->register_collision_surface(coll_surf);
    
    DynamicalStateData_sp dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, initial_pos);
    dsd->set_velocity(0, initial_velocity);

    auto solver = create_advance_position_with_collisions(dsd, collision_handler);
    solver->solve(dt);

    REQUIRE(dsd->get_position(0).X() > initial_pos.X());
    REQUIRE(dsd->get_position(0).Y() > initial_pos.Y());
    REQUIRE(dsd->get_velocity(0).X() > 0.0);
    REQUIRE(dsd->get_velocity(0).Y() > 0.0);


}

TEST_CASE( " Test point on plane does not fall through"){
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, MIN_END_DIST_FROM_COLLISION, 0.0));
    dsd->set_velocity(0, Vector(0.0, -1.0, 0.0));
    const double dt = 1.0;

    auto collision_plane = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    auto collision_surface = create_collision_surface();
    collision_surface->add_collision_object(collision_plane);

    auto collision_handler = create_collision_handler();
    collision_handler->register_collision_surface(collision_surface);

    auto solver = create_advance_position_with_collisions(dsd, collision_handler);
    solver->solve(dt);

    REQUIRE_VECTOR_APPROX(dsd->get_position(0), Vector(0.0, 1.0, 0.0), 0.0001);
}

TEST_CASE( " Test no infinite loop for static particle on plane"){
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 0.0, 0.0));
    dsd->set_velocity(0, Vector(0.0, 0.0, 0.0));
    const double dt = 1.0;

    auto collision_plane = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    auto collision_surface = create_collision_surface();
    collision_surface->add_collision_object(collision_plane);

    auto collision_handler = create_collision_handler();
    collision_handler->register_collision_surface(collision_surface);

    auto solver = create_advance_position_with_collisions(dsd, collision_handler);
    solver->solve(dt);

    //REQUIRE(dsd->get_position(0) == Vector(0.0, 0.0, 0.0));
    REQUIRE_VECTOR_APPROX(dsd->get_position(0), Vector(0.0, 0.0, 0.0), 0.0001);
}

TEST_CASE(" Test no fall through for cr of 0"){
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 0.5, 0.0));
    dsd->set_velocity(0, Vector(0.0, -1.0, 0.0));
    const double dt = 1.0;

    auto collision_plane = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    auto collision_surface = create_collision_surface();
    collision_surface->add_collision_object(collision_plane);
    collision_surface->set_restitution(0.0);

    auto collision_handler = create_collision_handler();
    collision_handler->register_collision_surface(collision_surface);

    auto solver = create_advance_position_with_collisions(dsd, collision_handler);
    solver->solve(dt);

    REQUIRE_VECTOR_APPROX(dsd->get_position(0), Vector(0.0, MIN_END_DIST_FROM_COLLISION, 0.0), 0.0001);
    REQUIRE(dsd->get_velocity(0) == Vector(0.0, 0.0, 0.0));
}

TEST_CASE("Triangle Collision Test"){
    // A test to see if a particle collides with a triangle. 
    // Also checks to see if a particle that collides with the same plane
    // as the triangle but not within it does not register a collision
    auto triangle = std::make_shared<CollisionTriangle>(
        Vector(0.0, 0.0, 0.0),
        Vector(1.0, 0.0, 0.0),
        Vector(0.0, 1.0, 0.0)
    );
    const double dt = 1.0;
    const Vector initial_pos_hit = Vector(0.25, 0.25, 1.0);
    const Vector updated_pos_hit = Vector(0.25, 0.25, -1.0);
    const Vector velocity = (updated_pos_hit - initial_pos_hit) / dt;

    const Vector initial_pos_miss = Vector(1.5, 1.5, 1.0);
    const Vector updated_pos_miss = Vector(1.5, 1.5, -1.0);
    const Vector velocity_miss = (updated_pos_miss - initial_pos_miss) / dt;

    // Test hit
    CollisionHitInfo hit_info;
    REQUIRE(triangle->hit(initial_pos_hit, updated_pos_hit, velocity, dt, hit_info));
    REQUIRE_FALSE(triangle->hit(initial_pos_miss, updated_pos_miss, velocity_miss, dt, hit_info));
}

TEST_CASE("Test exact collision pushes the particle back slightly by epsilon"){
    // To handle edge cases in the collisions, we should push the particle slightly
    // above (the same side of the plane that the particle started on) the collision
    // surface when the particle ends exactly on the collision 
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 1.0, 0.0));
    dsd->set_velocity(0, Vector(0.0, -1.0, 0.0));
    const double dt = 1.0;
    auto collision_plane = create_collision_plane(Vector(0.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0));
    auto collision_surface = create_collision_surface();
    collision_surface->add_collision_object(collision_plane);
    auto collision_handler = create_collision_handler();
    collision_handler->register_collision_surface(collision_surface);

    auto solver = create_advance_position_with_collisions(dsd, collision_handler);
    solver->solve(dt);
    REQUIRE(dsd->get_position(0).Y() == MIN_END_DIST_FROM_COLLISION);
}

TEST_CASE("Test collisions play nice with sixth order solver"){
    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 1.0, 0.0));
    // random velocity between 0 and -1
    double rand_vel = -1.0 * (static_cast<double>(rand()) / RAND_MAX);
    dsd->set_velocity(0, Vector(0.0, rand_vel, 0.0));
    const double dt = 0.01;
    auto collision_plane = std::make_shared<CollisionTriangle>(
        Vector(0.0, 0.0, 0.0),
        Vector(0.0, 0.0, 10.0),
        Vector(10.0, 0.0, 0.0)
    );
    auto collision_surface = create_collision_surface();
    collision_surface->add_collision_object(collision_plane);
    auto collision_handler = create_collision_handler();
    collision_handler->register_collision_surface(collision_surface);

    auto advance_position_solver = create_advance_position_with_collisions(dsd, collision_handler);
    auto advance_velocity_solver = std::make_shared<AdvanceVelocityWithForces>(dsd, std::make_shared<ForceSystem>());
    auto leapfrog_solver = std::make_shared<GISolverLeapfrog>(advance_position_solver, advance_velocity_solver);
    auto sixth_order_solver = std::make_shared<GISolverSixthOrder>(leapfrog_solver);
    auto solver_system = create_gi_solver_system();
    solver_system->add_solver(sixth_order_solver, dt);
    solver_system->init();
    for (int i = 0; i < 1000; i++){
        solver_system->solve(dt);
    }
    REQUIRE(dsd->get_position(0).Y() > 0.0);


}