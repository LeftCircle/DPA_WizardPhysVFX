#ifndef _SPH_DATA_H
#define _SPH_DATA_H


#include "dynamical_state_data.h"

// The SPHData class adds the required data components to DSD
// that are used in SPH solvers and forces
namespace pba {
class SPHData : public DynamicalStateData {
public:
	SPHData();
	~SPHData() = default;

	double get_max_particle_speed() const { return _max_particle_speed; }
	double get_max_particle_acceleration() const { return _max_particle_acceleration; }
	void set_max_particle_speed(double speed) { _max_particle_speed = speed; }
	void set_max_particle_acceleration(double accel) { _max_particle_acceleration = accel; }

	void set_h(double h) { _h = h; }
	void set_rest_density(double rest_density) { _rest_density = rest_density; }
	void set_rest_pressure(double rest_pressure) { _rest_pressure = rest_pressure; }
	void set_gamma(double gamma) { _gamma = gamma; }
	void set_viscosity_alpha(double alpha) { _viscosity_alpha = alpha; }
	void set_viscosity_beta(double beta) { _viscosity_beta = beta; }
	void set_viscosity_epsilon(double epsilon) { _viscosity_epsilon = epsilon; }
	
	const double h() const { return _h; }
	const double rest_density() const { return _rest_density; }
	const double rest_pressure() const { return _rest_pressure; }
	const double gamma() const { return _gamma; }
	const double viscosity_alpha() const { return _viscosity_alpha; }
	const double viscosity_beta() const { return _viscosity_beta; }
	const double viscosity_epsilon() const { return _viscosity_epsilon; }

private:
	double _h = 0.05;
	double _rest_density = 1.0;
	double _gamma = 1.0;
	double _viscosity_alpha = 1.0;
	double _viscosity_beta = 1.0;
	double _viscosity_epsilon = 0.01;
	double _rest_pressure = 1.0;
	double _max_particle_speed = 100.0;
	double _max_particle_acceleration = 400.0;
};

using SPHData_sp = std::shared_ptr<SPHData>;

} // end namespace pba


#endif