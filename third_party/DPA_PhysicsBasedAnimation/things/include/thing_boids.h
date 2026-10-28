#ifndef _THING_BOIDS_H
#define _THING_BOIDS_H


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
#include "boids_acceleration.h"
#include "boids_state_data.h"
#include "boid_solvers.h"

namespace pba{

class BoidThingyDingy : public PbaThingyDingy{
public:

	BoidThingyDingy(const std::string& nam = "Boid Thingy Dingy");
	~BoidThingyDingy() = default;

	void Display() override;

	void Keyboard( unsigned char key, int x, int y ) override;

	void solve() override;

	void Reset() override;

	void Usage() override;

private:
	std::shared_ptr<BoidStateData> _dsd;
	ForceSystem_sp _force_system;
	std::shared_ptr<SimpleGravityForce> _gravity_force;
	std::shared_ptr<GISolverSystem> _solver_system;
	CollisionHandler_sp _collision_handler;
	CollisionSurface_sp _box;
	std::shared_ptr<ParticleEmitter> _particle_emitter_sp;
	Kernel_sp _kernel;
	idx_volume_sp _occupancy_volume;

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

	void _initialize_box_collision_surface(const AABB<Vector>& bounds);
	void _draw_box();
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
	void _adjust_coefficient_of_sticky(const double delta);
	void _adjust_avoidance_strength(const double factor);
	void _adjust_centering_strength(const double factor);
	void _adjust_vel_match_strength(const double factor);


	BoidThingyDingy() = delete;
};


inline std::shared_ptr<BoidThingyDingy> create_boid_thingy_dingy(const std::string& name = "Boid Thingy Dingy"){
	return std::make_shared<BoidThingyDingy>(name);
}

} // end namespace pba


#endif