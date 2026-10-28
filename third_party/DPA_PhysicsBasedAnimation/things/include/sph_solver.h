#ifndef _SPH_SOLVER_H
#define _SPH_SOLVER_H

#include <memory>
#include <vector>
#include <numeric>
#include <execution>
#include <cmath>
#include <stdexcept>
#include <algorithm>

#include "the_wheel.h"
#include "partial_solvers.h"
#include "sph_kernel.h"
#include "sph_data.h"
#include "dynamical_state_data.h"
#include "occupancy_volume.h"

namespace pba{

// Returns the SPH density for a particle assuming uniform h of 
// all particles in the kernel
double get_density_with_uniform_h_parallel(
	size_t i,
	const pba::span<const size_t>& neighbor_indices,
	const DynamicalStateData_sp& dsd,
	const Kernel& kernel
);

double get_density_with_uniform_h(
	size_t i,
	const pba::span<const size_t>& neighbor_indices,
	const DynamicalStateData_sp& dsd,
	const Kernel& kernel
);

double get_density_with_uniform_h_silly_loop(
	size_t i,
	const pba::span<const size_t>& neighbor_indices,
	const DynamicalStateData_sp& dsd,
	const Kernel& kernel
);


class SPHPositionSolver : public AdvancePositionWithCollisions {
public:
	explicit SPHPositionSolver(
		SPHData_sp dsd,
		CollisionHandler_sp collision_handler,
		idx_volume_sp occupancy_volume,
		Kernel_sp kernel
	);
	~SPHPositionSolver() = default;	

	virtual void solve(const double dt) override;

protected:
	SPHPositionSolver() = delete;
	idx_volume_sp _occupancy_volume;
	Kernel_sp _kernel;
};

// class PciSPHPressureForce : public SPHPositionSolver {
// public:
// 	PciSPHPositionSolver(
// 		SPHData_sp dsd,
// 		CollisionHandler_sp collision_handler,
// 		idx_volume_sp occupancy_volume,
// 		Kernel_sp kernel
// 	);

// 	virtual void solve(const double dt) override;

// protected:
// 	void _accumulate_pressure_force(double dt);
// 	double _accumulate_delta(const double dt);
// 	double _compute_beta(const double dt);
// 	void _accumulate_pressure_gradient_force(
// 		::span<Vector>& pos,
// 		::span<double>& densities,
// 		::span<double>& pressures,
// 		::span<Vector>& pressure_forces
// 	);

// private:
// 	double _max_density_error_ratio = 0.01;
// 	unsigned int _max_iterations = 5;
// };

class SPHAdvanceVelocityWithForces : public AdvanceVelocityWithForces {

public:
	explicit SPHAdvanceVelocityWithForces(SPHData_sp dsd, ForceSystem_sp force_system)
	: AdvanceVelocityWithForces(dsd, force_system) {}
	
	virtual void solve(const double dt) override;

private:
	Vector _clamp_vector(const Vector& vec, double max_magnitude) const noexcept;
	SPHAdvanceVelocityWithForces() = delete;
};



} // end namespace pba





#endif