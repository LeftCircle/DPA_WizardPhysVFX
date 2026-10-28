#ifndef _THING_COLLIDING_PARTICLES_AND_RBD_H
#define _THING_COLLIDING_PARTICLES_AND_RBD_H


#include <GL/gl.h>   // OpenGL itself.
#include <GL/glu.h>  // GLU support library.
#include <GL/glut.h> // GLUT support library.
#include <iostream>
#include <string>
#include <filesystem>
#include <execution>

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
#include "partial_solvers.h"
#include "obj_reader.h"
#include "cato_vector.h"
#include "PbaViewer.h"
#include "rigid_body.h"
#include "rbd_solvers.h"
#include "particle_rbd_collisions.h"
#include "sph_data.h"
#include "sph_kernel.h"
#include "sph_solver.h"

namespace pba{

class CollidingParticlesRBDThing : public PbaThingyDingy{
public:

	CollidingParticlesRBDThing(const std::string& nam = "Colliding Particles RBD Thingy Dingy");
	~CollidingParticlesRBDThing() = default;

	void Init( const std::vector<std::string>& args ) override;

	void Display() override;

	void Keyboard( unsigned char key, int x, int y ) override;

	void solve() override;

	void Reset() override;

	void Usage() override;
    
    
private:
    std::shared_ptr<SPHData> _dsd;
    RB_sp _rbd;
    CollisionSurface_sp _main_collision_surface;
    ForceSystem_sp _force_system;
	ForceSystem_sp _sph_force_system;
    std::shared_ptr<SimpleGravityForce> _gravity_force;
    std::shared_ptr<GISolverSystem> _solver_system;
    std::shared_ptr<ParticleRBDCollisionHandler> _collision_handler;
    CollisionSurface_sp _box;
	std::shared_ptr<ParticleEmitter> _particle_emitter_sp;
	Kernel_sp _kernel;
	idx_volume_sp _occupancy_volume;
    std::shared_ptr<SPHViscosityForce> _viscosity_force;
    std::shared_ptr<SPHPressureForce> _pressure_force;

    std::vector<Triangle> _tris_to_draw;
    std::vector<Color> _tri_colors = {
        Color(0.8, 0.3, 0.3, 1.0),
        Color(0.3, 0.8, 0.3, 1.0),
        Color(0.3, 0.3, 0.8, 1.0),
        Color(0.9, 0.7, 0.4, 1.0),
        Color(0.4, 0.9, 0.7, 1.0),
        Color(0.7, 0.4, 0.9, 1.0),
        Color(0.9, 0.4, 0.7, 1.0),
    };

    void _add_random_particle();
	void _draw_tris();
	void _draw_particles();
	void _set_to_leapfrog_solver();
	void _set_to_forward_euler_solver();
	void _set_to_backward_euler_solver();
	void _set_to_sixth_order_solver();
	void _adjust_gravity(const Vector& delta);
	void _adjust_coefficient_of_restitution(const double delta);
	void _adjust_coefficient_of_sticky(const double delta);
	void _emit_particles(const size_t n);
	void _adjust_timestep(const double factor);


	void _adjust_viscosity(const double factor);
	void _adjust_pressure_strength(const double factor);
	void _adjust_pressure_power(const double factor);
	void _adjust_pressure_base(const double factor);
	void _adjust_velocity_max_val(const double factor);
	void _adjust_acceleration_max_val(const double factor);
	void _adjust_base_density(const double factor);

    void _create_rigid_body_from_obj(const std::string& file_name, const Vector& center = Vector(0, 0, 0));
	CollisionSurface_sp _create_collision_geo_from(const std::string& file_name);
	void _initialize_box_collision_surface(const AABB<Vector>& bounds);

	CollidingParticlesRBDThing() = delete;
	const std::string DEFAULT_COLL_PATH = "../../models/box.obj";
	const std::string DEFAULT_SOFT_BODY_PATH = "../../models/simple_box.obj";
};


inline std::shared_ptr<CollidingParticlesRBDThing> create_colliding_particles_rbd_thing(const std::string& name = "Colliding Particles RBD ThingyDingy"){
	return std::make_shared<CollidingParticlesRBDThing>(name);
}

} // end namespace pba




#endif








