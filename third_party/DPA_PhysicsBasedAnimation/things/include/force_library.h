#ifndef _FORCE_LIBRARY_H
#define _FORCE_LIBRARY_H

#include <vector>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <execution>
#include <algorithm>

#include "the_wheel.h"
#include "force.h"
#include "occupancy_volume.h"
#include "sph_kernel.h"
#include "sph_data.h"
#include "soft_body_data.h"
#include "boids_acceleration.h"
#include "collision_handler.h"
#include "soft_edge.h"

namespace pba{

struct IndexedForce {
	size_t idx;
	Vector force;
};

using idx_force_vec = std::vector<IndexedForce>;

static void reduce_indexed_force_into(idx_force_vec& idx_force, span<Vector> results);

class SimpleGravityForce : public ForceBase{
public:
	SimpleGravityForce(const Vector& gravity) : _gravity(gravity) {}
	~SimpleGravityForce() = default;

	void compute(DSD_sp dsd, const double dt) const override;

	const Vector& get_gravity() const noexcept { return _gravity; }
	void set_gravity(const Vector& gravity) { _gravity = gravity; }

private:
	Vector _gravity;

};

class SPHPressureForce : public ForceBase {
public:
	SPHPressureForce(idx_volume_sp occupancy_volume, Kernel_sp kernel) : 
		_occupancy_volume(occupancy_volume), _kernel(kernel) {}
	SPHPressureForce() = default;
	~SPHPressureForce() = default;

	// This is a bit of a hack to let us use the same force interface for SPH and standard forces
	void compute(DSD_sp dsd, const double dt) const override {
		auto sph_data = std::dynamic_pointer_cast<SPHData>(dsd);
		if (!sph_data) throw std::runtime_error("SPHPressureForce requires SPHData");
		compute_sph(sph_data, dt);
	};

	virtual void compute_sph(SPHData_sp sph_data, const double dt) const;

protected:
	Vector _get_pressure_with_uniform_h(
		size_t i,
		const span<const size_t>& neighbor_indices,
		const SPHData_sp sph_data,
		const span<const double>& densities
	) const;

	double _get_pressure_at_density(
		double density,
		double rest_pressure,
		double rest_density,
		double gamma
	) const;


	//SPHPressureForce() = delete;
	mutable idx_volume_sp _occupancy_volume;
	Kernel_sp _kernel;
};

class PciSPHPressureForce : public SPHPressureForce {
public:
	explicit PciSPHPressureForce(
		idx_volume_sp occupancy_volume,
		Kernel_sp kernel,
		CollisionHandler_sp collision_handler
	) : _collision_handler(collision_handler) { 
		_occupancy_volume = duplicate_idx_volume(occupancy_volume); _kernel = kernel;
	};

	void compute(DSD_sp dsd, const double dt) const override {
		auto sph_data = std::dynamic_pointer_cast<SPHData>(dsd);
		if (!sph_data) throw std::runtime_error("SPHPressureForce requires SPHData");
		compute_sph(sph_data, dt);
	};

	virtual void compute_sph(SPHData_sp sph_data, const double dt) const override;

protected:
	//void _accumulate_pressure_force(double dt);
	double _accumulate_delta(SPHData_sp dsd, const double dt) const;
	double _compute_beta(SPHData_sp dsd, const double dt) const;
	void _accumulate_pressure_gradient_force(
		SPHData_sp dsd,
		span<Vector>& pos,
		span<double>& densities,
		span<double>& pressures,
		span<Vector>& pressure_forces
	) const;

	CollisionHandler_sp _collision_handler;


private:
	double _max_density_error_ratio = 0.01;
	unsigned int _max_iterations = 5;
};


class SPHViscosityForce : public ForceBase {
public:
	SPHViscosityForce(
		idx_volume_sp occupancy_volume,
		Kernel_sp kernel
	) : _occupancy_volume(occupancy_volume), _kernel(kernel) {}
	
	~SPHViscosityForce() = default;

	void compute(DSD_sp dsd, const double dt) const override {
		auto sph_data = std::dynamic_pointer_cast<SPHData>(dsd);
		if (!sph_data) throw std::runtime_error("SPHViscosityForce requires SPHData");
		compute_sph(sph_data, dt);
	};

	void compute_sph(SPHData_sp sph_data, const double dt) const;

private:
	Vector _compute_viscosity_force_for_neighbors(
		size_t i,
		const span<const size_t>& neighbor_indices,
		const SPHData_sp sph_data
	) const;

	Vector _doyub_viscocity_for_neighbors(
		size_t i, 
		const span<const size_t>& neighbor_indices,
		const SPHData_sp sph_data
	) const;

	double _avg_speed_of_sound(double desnity,
								double one_over_rest_density,
								double rest_pressure,
								double gamma
	) const noexcept;
	
	double _mu_ab(const Vector& vel_a,
				  const Vector& vel_b,
				  const Vector& pos_a,
				  const Vector& pos_b,
				  double h,
				  double epsilon
	) const;
	
	double _pi_ab(double c_ab,
		          double mu_ab,
		          double density_a,
		          double density_b,
		          double alpha,
		          double beta
	) const;

	SPHViscosityForce() = delete;
	idx_volume_sp _occupancy_volume;
	Kernel_sp _kernel;
};

class StrutForce : public ForceBase{
public:
	StrutForce() = default;
	~StrutForce() = default;

	void compute(DSD_sp dsd, const double dt) const override{
		auto soft_body_sp = std::dynamic_pointer_cast<SoftBody>(dsd);
		if (!soft_body_sp) throw std::runtime_error("Strut forces require a soft body");
		_compute(soft_body_sp, dt); 
	}

private:
	void _compute(std::shared_ptr<SoftBody> sb, const double dt) const;
};

class UniformStrutForce : public ForceBase{
public:
	UniformStrutForce(const double spring, const double friction)
	 : _spring_force(spring), _friction(friction) {}

	void compute(DSD_sp dsd, const double dt) const override{
		auto soft_body_sp = std::dynamic_pointer_cast<SoftBody>(dsd);
		if (!soft_body_sp) throw std::runtime_error("Strut forces require a soft body");
		_compute(soft_body_sp, dt); 
	}
	
	idx_force_vec compute_acceleration(DSD_scp dsd) const;

	void set_spring_force(double new_force) noexcept {_spring_force = new_force; }
	void set_friction(double new_friction) noexcept {_friction = new_friction; }
	
	double get_spring_force() const noexcept { return _spring_force; }
	double get_friction() const noexcept { return _friction; }

private:
	void _compute(std::shared_ptr<SoftBody> sb, const double dt) const;
	UniformStrutForce() = delete;

	double _spring_force;
	double _friction;
};

using TriForces = std::array<IndexedForce, 3>;

class SoftTriangleForce : public ForceBase{
public:
	explicit SoftTriangleForce() {}
	void compute(DSD_sp dsd, const double dt) const override;
	void compute(DSD_sp dsd, const double dt, span<const SoftTriangle> soft_triangles) const;
	TriForces compute_soft_tri_forces(span<const Vector> pos, const SoftTriangle& tri) const;
	std::vector<Vector> compute_acceleration(const DSD_scp dsd, span<const SoftTriangle> soft_triangles) const; 
};

} // end namespace pba


#endif