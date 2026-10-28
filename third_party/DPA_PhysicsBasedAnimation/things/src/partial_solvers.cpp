#include "partial_solvers.h"

namespace{

	
};

//}
using namespace pba;

using vec_st = std::vector<size_t>;

void PartialSolverAdvancePosition::partial_update(DSD_sp dsd, const double dt){
	auto start_pos = dsd->get_vector_attribute_span("positions");
	auto vel = dsd->get_vector_attribute_span("velocities");
	for (size_t i = 0; i < dsd->n_particles(); i++){
		start_pos[i] += vel[i] * dt;	
	}
}

void PartialSolverAdvancePosition::solve(const double dt){
	const size_t n = _state_data->n_particles();
	auto pos = _state_data->get_vector_attribute_span("positions");
	auto new_pos = _state_data->get_vector_attribute_span("new_positions");
	auto vel = _state_data->get_vector_attribute_span("velocities");
	std::transform(std::execution::par_unseq, pos.begin(), pos.end(), vel.begin(), new_pos.begin(),
		[dt](const Vector& p, const Vector& v) -> Vector {
			return p + v * dt;
		}
	);
	if (_collision_handler) {
		_collision_handler->handle_collisions(_state_data, "new_positions", dt);
	}
	std::copy(new_pos.begin(), new_pos.end(), pos.begin());
	if (_occupancy_volume) {
		_occupancy_volume->populate(
			_state_data->get_vector_attribute_span("positions"),
			[](vec_st& cell, size_t i){ cell.push_back(i); }
		);
	}
}

AdvancePositionWithCollisions::AdvancePositionWithCollisions(DynamicalStateData_sp dsd, CollisionHandler_sp collision_handler) :
	_state_data(dsd), _collision_handler(collision_handler) {
	if (!dsd->has_vector_attribute("new_positions")) {
		// Create the new_positions attribute if it doesn't exist
		dsd->add_attribute<Vector>("new_positions", DSAv("new_positions", Vector(0.0f, 0.0f, 0.0f)));
	}
}

void AdvancePositionWithCollisions::solve(const double dt){
	const size_t n = _state_data->n_particles();
	auto new_positions = _state_data->get_vector_attribute_span("new_positions");
	#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		const Vector& pos = _state_data->get_position(i);
		const Vector& vel = _state_data->get_velocity(i);
		new_positions[i] = pos + vel * dt;
	}
	_collision_handler->handle_collisions(_state_data, "new_positions", dt);
}

void AdvanceVelocityWithForces::solve(const double dt){
	_force_system->compute(_state_data, dt);
	// At this point the accelerations have all been computed. Time to update velocities
	const size_t n = _state_data->n_particles();
	#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		const Vector& vel = _state_data->get_velocity(i);
		const Vector& acc = _state_data->get_acceleration(i);
		_state_data->set_velocity(i, vel + acc * dt);
	}
}
