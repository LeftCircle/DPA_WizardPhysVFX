#include "catch_helpers.h"
#include "boids_acceleration.h"
#include "boid_solvers.h"
#include "boids_state_data.h"
#include "force.h"

using namespace pba;

TEST_CASE("test boid forces"){
	/*
		<-o

	<-o   x   <-o
		
		<-o
	*/
	// The x boid above should have a centering of zero
	// avoidance of zero, and vel match in <- direction
	std::shared_ptr<BoidStateData> dsd = std::make_shared<BoidStateData>();
	dsd->add(5);
	dsd->set_position(0, Vector(-1, 0, 0));
	dsd->set_position(1, Vector(0, 1, 0));
	dsd->set_position(2, Vector(1, 0, 0));
	dsd->set_position(3, Vector(0, -1, 0));
	dsd->set_position(4, Vector(0, 0, 0));

	dsd->set_velocity(0, Vector(-1, 0, 0));
	dsd->set_velocity(1, Vector(-1, 0, 0));
	dsd->set_velocity(2, Vector(-1, 0, 0));
	dsd->set_velocity(3, Vector(-1, 0, 0));
	dsd->set_all_view_and_view_ramp(-1, -1);

	dsd->set_double_attribute("acc_budget", 4, 100.0);

	// Test the advance velocity with forces gets correct acceleration
	const AABB bounds(Vector(-10, -10, -10), Vector(10, 10, 10));
	idx_volume_sp ov = create_idx_occupancy_volume(bounds, 3.0);
	std::shared_ptr<ForceSystem> fs = std::make_shared<ForceSystem>();
	AdvanceBoidVelocityWithForces vel_update(dsd, fs, ov);
	vel_update.solve(1.0);
	REQUIRE(dsd->get_acceleration(4).X() < 1.0);
	REQUIRE(std::abs(dsd->get_acceleration(4).Y()) < 0.001);
	REQUIRE(std::abs(dsd->get_acceleration(4).Z()) < 0.001);
}