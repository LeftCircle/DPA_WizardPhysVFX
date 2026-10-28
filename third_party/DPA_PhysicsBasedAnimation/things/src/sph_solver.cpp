#include "sph_solver.h"

using namespace pba;


double pba::get_density_with_uniform_h_parallel(
    size_t i,
    const pba::span<const size_t>& neighbor_indices,
    const DynamicalStateData_sp& dsd,
    const Kernel& kernel)
{
    const Vector& pos_i = dsd->get_position(i);
	const auto positions = dsd->get_vector_attribute_span("positions");
	const auto masses = dsd->get_float_attribute_span("mass");
	return std::transform_reduce(
		std::execution::par_unseq,
		neighbor_indices.begin(),
		neighbor_indices.end(),
		0.0,
		std::plus<>(),
		[&pos_i, &positions, &masses, &kernel](size_t j){
			double distance = (pos_i - positions[j]).magnitude();
			return masses[j] * kernel(distance);
		}
	);
}

double pba::get_density_with_uniform_h(
    size_t i,
    const pba::span<const size_t>& neighbor_indices,
    const DynamicalStateData_sp& dsd,
    const Kernel& kernel)
{
	const Vector& pos_i = dsd->get_position(i);
	const auto positions = dsd->get_vector_attribute_span("positions");
	const auto masses = dsd->get_float_attribute_span("mass");
	return std::accumulate(neighbor_indices.begin(), neighbor_indices.end(), 0.0, [&pos_i, &positions, &masses, &kernel](double acc, size_t j){
        double distance = (pos_i - positions[j]).magnitude();
        return acc + masses[j] * kernel(distance);
    });
}

double pba::get_density_with_uniform_h_silly_loop(
    size_t i,
    const pba::span<const size_t>& neighbor_indices,
    const DynamicalStateData_sp& dsd,
    const Kernel& kernel)
{
	const Vector& pos_i = dsd->get_position(i);
	double density = 0.0;
	for (size_t j : neighbor_indices) {
		const Vector& pos_j = dsd->get_position(j);
		double mass_j = dsd->get_mass(j);
		double distance = (pos_i - pos_j).magnitude();
		density += mass_j * kernel(distance);
	}
	return density;
}

SPHPositionSolver::SPHPositionSolver(
	SPHData_sp dsd,
	CollisionHandler_sp collision_handler,
	idx_volume_sp occupancy_volume,
	Kernel_sp kernel
) : 
	AdvancePositionWithCollisions(dsd, collision_handler),
	_occupancy_volume(occupancy_volume),
	_kernel(kernel) {
		if (!dsd->has_double_attribute("density")) {
			// Create the density attribute if it doesn't exist
			dsd->add_attribute<double>("density", DSAd("density", 1.0));
		}
}

void SPHPositionSolver::solve(const double dt) {
	AdvancePositionWithCollisions::solve(dt);
	// Now update densities and the occupancy grid
	const auto positions = _state_data->get_vector_attribute_span("positions");
	const auto next_pos = _state_data->get_vector_attribute_span("new_positions");
	_occupancy_volume->populate(
		next_pos,
		[](std::vector<size_t>& cell, size_t idx){
			cell.push_back(idx);
		}
	);

	pba::span<double> densities = _state_data->get_double_attribute_span("density");
	#pragma omp parallel for
	for (size_t i = 0; i < _state_data->n_particles(); i++) {
		double density = _occupancy_volume->accumulate_neighbor_cells(
			i,
			_state_data->get_position(i),
			0.0,
			[&densities, this](size_t idx, double acc, const std::vector<size_t>& neighbor_indices){
				return acc + get_density_with_uniform_h(idx, pba::span<const size_t>(neighbor_indices), _state_data, *_kernel);	
			}
		);
		densities[i] = density;
	}

	std::copy(next_pos.begin(), next_pos.end(), positions.begin());
}

void SPHAdvanceVelocityWithForces::solve(const double dt) {
	_force_system->compute(_state_data, dt);
	// Cast the dsd to SPHData
	SPHData_sp sph_data = std::dynamic_pointer_cast<SPHData>(_state_data);
	// At this point the accelerations have all been computed. Time to update velocities
	const size_t n = sph_data->n_particles();
	#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		const Vector& vel = sph_data->get_velocity(i);
		const Vector& acc = sph_data->get_acceleration(i);
		Vector clamped_acc = _clamp_vector(acc, sph_data->get_max_particle_acceleration());
		Vector new_vel = vel + clamped_acc * dt;
		new_vel = _clamp_vector(new_vel, sph_data->get_max_particle_speed());
		sph_data->set_velocity(i, new_vel);
	}
}

Vector SPHAdvanceVelocityWithForces::_clamp_vector(const Vector& vec, double max_magnitude) const noexcept {
	double mag = vec.magnitude();
	if (std::isnan(mag)) [[unlikely]] {
		printf("NAAN vector");
		throw std::runtime_error("naan mag");
	}
	if (mag > max_magnitude) {
		Vector clamped_vec = vec;
		clamped_vec.normalize();
		clamped_vec = clamped_vec * max_magnitude;
		return clamped_vec;
	}
	return vec;
}

//} // end namespace pba