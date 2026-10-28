#include "catch2/catch_amalgamated.hpp"
#include "catch_helpers.h"


#include "dynamical_state_data.h"
#include "force_library.h"
#include "partial_solvers.h"
#include "GISolver.h"

using namespace pba;

TEST_CASE("Particle with gravity "){
    // Test that a particle with a gravity accumulating force falls. 

    auto dsd = create_dynamical_state_data();
    dsd->add(1);
    dsd->set_position(0, Vector(0.0, 10.0, 0.0));
    dsd->set_velocity(0, Vector(0.0, 0.0, 0.0));

    auto gravity_force = std::make_shared<SimpleGravityForce>(Vector(0.0, -9.81, 0.0));
    
    auto accumulating_force = std::make_shared<ForceSystem>();
    accumulating_force->add_force(gravity_force);

    const double dt = 1.0;
    auto position_solver = std::make_shared<PartialSolverAdvancePosition>(dsd);
    auto velocity_solver = std::make_shared<AdvanceVelocityWithForces>(dsd, accumulating_force);

    auto solver_system = create_gi_solver_system();
    solver_system->add_solver(velocity_solver, dt);
    solver_system->add_solver(position_solver, dt);
    solver_system->init();
    solver_system->solve(dt);

    // Now confirm that the particle has fallen and the velocity is correct
    Vector expected_position = Vector(0.0, 10.0 - 9.81, 0.0);
    Vector expected_velocity = Vector(0.0, -9.81, 0.0);

    REQUIRE(dsd->get_position(0) == expected_position);
    REQUIRE(dsd->get_velocity(0) == expected_velocity);
}
