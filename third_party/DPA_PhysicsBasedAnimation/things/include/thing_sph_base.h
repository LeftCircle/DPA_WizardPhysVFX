#ifndef _THING_SPH_BASE_H
#define _THING_SPH_BASE_H


#include <GL/gl.h>   // OpenGL itself.
#include <GL/glu.h>  // GLU support library.
#include <GL/glut.h> // GLUT support library.
#include <iostream>

#include "dynamical_state_data.h"
#include "GISolver.h"
#include "force_library.h"
#include "partial_solvers.h"
#include "PbaThing.h"
#include "collision_handler.h"
#include "shapes.h"
#include "Color.h"
#include "particle_emitter.h"
#include "AABB.h"
#include "sph_solver.h"
#include "sph_data.h"
#include "sph_kernel.h"

namespace pba{

class SPHThingyDingy : public PbaThingyDingy{
public:

	SPHThingyDingy(const std::string& nam = "SPH Thingy Dingy");
	~SPHThingyDingy() = default;

	virtual void Display() override;

	virtual void Keyboard( unsigned char key, int x, int y ) override;

	virtual void solve() override;

	virtual void Reset() override;

	virtual void Usage() override;

protected:
	SPHData_sp _dsd_sph;
	std::shared_ptr<ParticleEmitter> _particle_emitter_sp;
	Kernel_sp _kernel;
	idx_volume_sp _occupancy_volume;
    std::shared_ptr<SPHViscosityForce> _viscosity_force;
    std::shared_ptr<SPHPressureForce> _pressure_force;

	void _draw_particles();
	void _set_to_leapfrog_solver();
	void _set_to_forward_euler_solver();
	void _set_to_backward_euler_solver();
	void _set_to_sixth_order_solver();
	void _adjust_gravity(const Vector& delta);
	void _adjust_coefficient_of_restitution(const double delta);
	void _emit_particles(const size_t n);
	void _add_random_particle();
	void _adjust_timestep(const double factor);
	void _adjust_viscosity(const double factor);
	void _adjust_pressure_strength(const double factor);
	void _adjust_pressure_power(const double factor);
	void _adjust_pressure_base(const double factor);
	void _adjust_velocity_max_val(const double factor);
	void _adjust_acceleration_max_val(const double factor);
	void _adjust_base_density(const double factor);

	SPHThingyDingy() = delete;
};


inline std::shared_ptr<SPHThingyDingy> create_sph_thingy_dingy(const std::string& name = "SPH Thingy Dingy"){
	return std::make_shared<SPHThingyDingy>(name);
}

} // end namespace pba




#endif








