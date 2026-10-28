#ifndef _THING_SOFT_BUNNY_H
#define _THING_SOFT_BUNNY_H


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

namespace pba{

class SoftBunnyThingyDingy : public PbaThingyDingy{
public:

	SoftBunnyThingyDingy(const std::string& nam = "Soft Bunny Thingy Dingy");
	~SoftBunnyThingyDingy() = default;

	void Init( const std::vector<std::string>& args ) override;

	void Display() override;

	void Keyboard( unsigned char key, int x, int y ) override;

	void solve() override;

	void Reset() override;

	void Usage() override;

private:
	std::shared_ptr<SoftBody> _dsd;
	CollisionSurface_sp _main_collision_surface;
	ForceSystem_sp _force_system;
	std::shared_ptr<SimpleGravityForce> _gravity_force;
	std::shared_ptr<GISolverSystem> _solver_system;
	CollisionHandler_sp _collision_handler;
    std::shared_ptr<UniformStrutForce> _uniform_strut_force;
	CollisionSurface_sp _box;

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

	void _draw_tris();
	void _draw_particles();
	void _set_to_leapfrog_solver();
	void _set_to_forward_euler_solver();
	void _set_to_backward_euler_solver();
	void _set_to_sixth_order_solver();
	void _adjust_gravity(const Vector& delta);
	void _adjust_coefficient_of_restitution(const double delta);
	void _emit_particles(const size_t n);
	void _adjust_timestep(const double factor);
    void _adjust_strut_force(const double delta);
    void _adjust_strut_friction(const double delta);
	void _draw_box();

    void _create_uniform_soft_body_from_obj(const std::string& file_name, const Vector& center = Vector(0, 0, 0));
	CollisionSurface_sp _create_collision_geo_from(const std::string& file_name);
	void _initialize_box_collision_surface(const AABB<Vector>& bounds);

	SoftBunnyThingyDingy() = delete;
	const std::string DEFAULT_COLL_PATH = "../../models/bigsphere.obj";
	const std::string DEFAULT_SOFT_BODY_PATH = "../../models/bunny_lo.obj";
};


inline std::shared_ptr<SoftBunnyThingyDingy> create_soft_bunny_thing(const std::string& name = "SPH Thingy Dingy"){
	return std::make_shared<SoftBunnyThingyDingy>(name);
}

} // end namespace pba




#endif








