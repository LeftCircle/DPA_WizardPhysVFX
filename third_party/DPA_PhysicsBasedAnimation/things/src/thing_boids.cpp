#include "thing_boids.h"
#include <algorithm>>

using namespace pba;


BoidThingyDingy::BoidThingyDingy(const std::string& nam)
: PbaThingyDingy(nam) {
	// Start with some default bounds
	AABB bounds(Vector(-3.0, -3.0, -3.0), Vector(3.0, 3.0, 3.0));
	AABB emission_bounds(Vector(-2.9, -2.9, -2.9), Vector(2.9,  2.9, 2.9));
	_particle_emitter_sp = std::make_shared<ParticleEmitter>(emission_bounds);
	_particle_emitter_sp->set_min_speed(7.0);
	_particle_emitter_sp->set_max_speed(15.0);
    _dsd = std::make_shared<BoidStateData>();

    _occupancy_volume = create_idx_occupancy_volume(bounds, 2.0);

	// add a thousand particles to start with
	for (size_t i=0; i<1000; i++){
		_add_random_particle();
	}
	// And now our systems, forces and collision surfaces
	_force_system = std::make_shared<ForceSystem>();
	_solver_system = create_gi_solver_system();
	_box = create_collision_surface();
	_box->set_sticky(1.0);
	_collision_handler = create_collision_handler();
	_collision_handler->register_collision_surface(_box);
	_initialize_box_collision_surface(bounds);
	_gravity_force = std::make_shared<SimpleGravityForce>(Vector(0.0, 0.0, 0.0));
	_force_system->add_forces(_gravity_force);
    
	// And now that the init is basically done. Let's build the solvers
	SetSimulationTimestep(0.01667);
	_set_to_sixth_order_solver();
}

void BoidThingyDingy::_add_random_particle(){
	_dsd->add(1);
	Vector pos, vel;
	_particle_emitter_sp->emit(pos, vel);
	size_t idx = _dsd->n_particles() - 1;
	_dsd->set_position(idx, pos);
	_dsd->set_velocity(idx, vel);
    _dsd->set_double_attribute("range", idx, 1.0);
    _dsd->set_double_attribute("range_amp", idx, 1.0);
    _dsd->set_double_attribute("acc_budget", idx, 20);
    _dsd->set_double_attribute("cos_fov_angle", idx, 0.25);
    _dsd->set_double_attribute("cos_fov_plus_amp", idx, 0);
	double r_cent = ParticleEmitter::rand_d(2.75, 4.5);
	double r_vel_match = ParticleEmitter::rand_d(0.5, 2.8);
	double r_coll_avoid = ParticleEmitter::rand_d(0.5, 1.0);
    _dsd->set_double_attribute("centering_str", idx, r_cent);
    _dsd->set_double_attribute("vel_match_str", idx, r_vel_match);
	_dsd->set_double_attribute("coll_avoid_str", idx, r_coll_avoid);
	Vector col = ParticleEmitter::generate_random_bounded_vector(0.75, 3.0);
	_dsd->set_color(idx, Color(col.X(), col.Y(), col.Z(), 1.0));
}

void BoidThingyDingy::_set_to_backward_euler_solver(){
	// Clear existing solvers
	_solver_system = create_gi_solver_system();

	auto advance_position_solver = std::make_shared<PartialSolverAdvancePosition>(_dsd, _collision_handler, _occupancy_volume);
    auto advance_velocity_solver = std::make_shared<AdvanceBoidVelocityWithForces>(_dsd, _force_system, _occupancy_volume);
	_solver_system->add_solver(advance_velocity_solver, dt);
	_solver_system->add_solver(advance_position_solver, dt);
	printf("Switched to Backward Euler solver.\n");
}

void BoidThingyDingy::_set_to_forward_euler_solver(){
	// Clear existing solvers
	_solver_system = create_gi_solver_system();

	auto advance_position_solver = std::make_shared<PartialSolverAdvancePosition>(_dsd, _collision_handler, _occupancy_volume);
    auto advance_velocity_solver = std::make_shared<AdvanceBoidVelocityWithForces>(_dsd, _force_system, _occupancy_volume);
	_solver_system->add_solver(advance_position_solver, dt);
	_solver_system->add_solver(advance_velocity_solver, dt);
	printf("Switched to Forward Euler solver.\n");
}

void BoidThingyDingy::_set_to_leapfrog_solver(){
	_solver_system = create_gi_solver_system();
	auto advance_position_solver = std::make_shared<PartialSolverAdvancePosition>(_dsd, _collision_handler, _occupancy_volume);
    auto advance_velocity_solver = std::make_shared<AdvanceBoidVelocityWithForces>(_dsd, _force_system, _occupancy_volume);
	_solver_system->add_solver(advance_position_solver, dt / 2.0);
	_solver_system->add_solver(advance_velocity_solver, dt);
	_solver_system->add_solver(advance_position_solver, dt / 2.0);
	printf("Switched to Leapfrog solver.\n");
}

void BoidThingyDingy::_set_to_sixth_order_solver(){
	_solver_system = create_gi_solver_system();
	auto advance_position_solver = std::make_shared<PartialSolverAdvancePosition>(_dsd, _collision_handler, _occupancy_volume);
    auto advance_velocity_solver = std::make_shared<AdvanceBoidVelocityWithForces>(_dsd, _force_system, _occupancy_volume);
	auto leapfrog_solver = std::make_shared<GISolverLeapfrog>(advance_position_solver, advance_velocity_solver);
	auto sixth_order_solver = std::make_shared<GISolverSixthOrder>(leapfrog_solver);
	_solver_system->add_solver(sixth_order_solver, dt);
	printf("Switched to Sixth Order solver.\n");
}

void BoidThingyDingy::solve(){
	_solver_system->solve(dt);
}

void BoidThingyDingy::Display(){
	_draw_box();
	_draw_particles();
}

void BoidThingyDingy::Keyboard( unsigned char key, int x, int y ){
	switch( key ){
		case 'a':{
			_adjust_avoidance_strength(0.9);
			break;
		}
		case 'A':{
			_adjust_avoidance_strength(1.1);
			break;
		}
		case 'b':{
			break;
		}
		case 'B':{
			break;
		}
		case 'c':{
			_adjust_coefficient_of_restitution(-0.05);
			break;
		}
		case 'C':{
			_adjust_coefficient_of_restitution(0.05);
			break;
		}
		case 'd':{
			_adjust_centering_strength(0.9);
			break;
		}
		case 'D':{
			_adjust_centering_strength(1.1);
			break;
		}
		case 'e':{
			_emit_particles(100);
			break;
		}
		case 'g':{
			_adjust_gravity(Vector(0.0, -0.1, 0.0));
			break;
		}
		case 'G':{
			_adjust_gravity(Vector(0.0, 0.1, 0.0));
			break;
		}
		case 'l':{
			_set_to_leapfrog_solver();
			break;
		}
		case 'L':{
			_set_to_sixth_order_solver();
			break;
		}
		case 'm':{
			_adjust_vel_match_strength(0.9);
			break;
		}
		case 'M':{
			_adjust_vel_match_strength(1.1);
			break;
		}
		case 'p':{
			break;
		}
		case 'P':{
			break;
		}
		case 'r':{
			Reset();
			break;
		}
		case 's':{
			_adjust_coefficient_of_sticky(-0.05);
			break;
		}
		case 'S':{
			_adjust_coefficient_of_sticky(0.05);
			break;
		}
		case 't':{
			_adjust_timestep(0.9);
			break;
		}
		case 'T':{
			_adjust_timestep(1.1);
			break;
		}
		case 'u':
		case 'U':
			Usage();
			break;
		case 'v': {
			break;
		}
		case 'V': {
			break;
		}
		default:
			break;
	}
}

void BoidThingyDingy::_emit_particles(const size_t n){
	for (size_t i=0; i<n; i++){
		_add_random_particle();
	}
	printf("Emitted %zu new particles. Total particle count is now %zu.\n", n, _dsd->n_particles());
}

void BoidThingyDingy::_adjust_gravity(const Vector& delta){
	_gravity_force->set_gravity(_gravity_force->get_gravity() + delta);
	printf("New gravity vector is (%f, %f, %f) m/s^2\n",
		_gravity_force->get_gravity().X(),
		_gravity_force->get_gravity().Y(),
		_gravity_force->get_gravity().Z()
	);
}

void BoidThingyDingy::_adjust_coefficient_of_sticky(const double delta){
	_box->set_sticky( _box->get_sticky() + delta );
	printf("New coefficient of sticky is %f\n", _box->get_sticky());
}

void BoidThingyDingy::_adjust_coefficient_of_restitution(const double delta){
	_box->set_restitution( _box->get_restitution() + delta );
	printf("New coefficient of restitution is %f\n", _box->get_restitution());
}

void BoidThingyDingy::Reset(){
	//_dsd->resize(0);
	_dsd->clear();
	_occupancy_volume->clear();
	printf("size of dsd positions = %zu /n", _dsd->get_vector_attribute_span("positions").size());
	_emit_particles(1);
	printf("Simulation reset.\n");
}

void BoidThingyDingy::_adjust_timestep(const double factor){
	dt *= factor;
	SetSimulationTimestep(dt);
	printf("New timestep is %f seconds. RESET SOLVER TO APPLY CHANGES\n", dt);
}

void BoidThingyDingy::_adjust_avoidance_strength(const double factor){
	auto attr = _dsd->get_double_attribute_span("coll_avoid_str");
	for (size_t i = 0; i < _dsd->n_particles(); i++){
		attr[i] *= factor;
	}
	double max_val = *std::max_element(attr.begin(), attr.end());
	double min_val = *std::min_element(attr.begin(), attr.end());
	printf("Avoidance range is now %f - %f\n", min_val, max_val);
}

void BoidThingyDingy::_adjust_vel_match_strength(const double factor){
	auto attr = _dsd->get_double_attribute_span("vel_match_str");
	for (size_t i = 0; i < _dsd->n_particles(); i++){
		attr[i] *= factor;
	}
	double max_val = *std::max_element(attr.begin(), attr.end());
	double min_val = *std::min_element(attr.begin(), attr.end());
	printf("vel match range is now %f - %f\n", min_val, max_val);
}

void BoidThingyDingy::_adjust_centering_strength(const double factor){
	auto attr = _dsd->get_double_attribute_span("centering_str");
	for (size_t i = 0; i < _dsd->n_particles(); i++){
		attr[i] *= factor;
	}
	double max_val = *std::max_element(attr.begin(), attr.end());
	double min_val = *std::min_element(attr.begin(), attr.end());
	printf("centering strength range is now %f - %f\n", min_val, max_val);
}

void BoidThingyDingy::Usage(){
	printf("Boid Controls:\n");
	printf("  a/A: adjust avoidance strength\n");
	printf("  c/C: Decrease/Increase coefficient of restitution for box collisions\n");
	printf("  d/D adjust centering strength\n");
	printf("  e: Emit 100 new particles\n");
	printf("  g/G: Increase/Decrease gravity strength\n");
	printf("  l: Switch to Leapfrog solver\n");
	printf("  L: Switch to Sixth Order solver\n");
	printf("  m/M adjust velocity match strength\n");
	printf("  r: Reset the simulation\n");
	printf("  s/S: Decrease/Increase coefficient of sticky\n");
	printf("  t/T: Decrease/Increase simulation timestep\n");
	printf("  u/U: Print this usage information\n");
}


void BoidThingyDingy::_initialize_box_collision_surface(const AABB<Vector>& bounds){
	// Let's start by just defining the 8 points of the box
	Vector bll = Vector(bounds.lower_left().X(), bounds.lower_left().Y(), bounds.lower_left().Z());
	Vector blf = Vector(bounds.lower_left().X(), bounds.lower_left().Y(),  bounds.upper_right().Z());
	Vector brl = Vector(bounds.upper_right().X(), bounds.lower_left().Y(), bounds.lower_left().Z());
	Vector brf = Vector(bounds.upper_right().X(), bounds.lower_left().Y(),  bounds.upper_right().Z());
	Vector tll = Vector(bounds.lower_left().X(),  bounds.upper_right().Y(), bounds.lower_left().Z());
	Vector tlf = Vector(bounds.lower_left().X(),  bounds.upper_right().Y(),  bounds.upper_right().Z());
	Vector trl = Vector(bounds.upper_right().X(),  bounds.upper_right().Y(), bounds.lower_left().Z());
	Vector trf = Vector(bounds.upper_right().X(),  bounds.upper_right().Y(),  bounds.upper_right().Z());

	// Now let's create the 12 triangles that make up the box
	// All the normals will point into the box
	Triangle b1 = Triangle(bll, brf, brl);
	Triangle b2 = Triangle(bll, blf, brf);
	_box->add_collision_object(std::make_shared<CollisionTriangle>(b1));
	_box->add_collision_object(std::make_shared<CollisionTriangle>(b2));
	Triangle t1 = Triangle(tll, trl, trf);
	Triangle t2 = Triangle(tll, trf, tlf);
	_box->add_collision_object(std::make_shared<CollisionTriangle>(t1));
	_box->add_collision_object(std::make_shared<CollisionTriangle>(t2));
	Triangle l1 = Triangle(bll, tlf, blf);
	Triangle l2 = Triangle(bll, tll, tlf);
	_box->add_collision_object(std::make_shared<CollisionTriangle>(l1));
	_box->add_collision_object(std::make_shared<CollisionTriangle>(l2));
	Triangle r1 = Triangle(brl, brf, trf);
	Triangle r2 = Triangle(brl, trf, trl);
	_box->add_collision_object(std::make_shared<CollisionTriangle>(r1));
	_box->add_collision_object(std::make_shared<CollisionTriangle>(r2));
	Triangle back1 = Triangle(bll, brl, trl);
	Triangle back2 = Triangle(bll, trl, tll);
	_box->add_collision_object(std::make_shared<CollisionTriangle>(back1));
	_box->add_collision_object(std::make_shared<CollisionTriangle>(back2));
	Triangle front1 = Triangle(blf, trf, brf);
	Triangle front2 = Triangle(blf, tlf, trf);
	_box->add_collision_object(std::make_shared<CollisionTriangle>(front1));
	_box->add_collision_object(std::make_shared<CollisionTriangle>(front2));
	
	_tris_to_draw.push_back(b1);
	_tris_to_draw.push_back(b2);
	_tris_to_draw.push_back(t1);
	_tris_to_draw.push_back(t2);
	_tris_to_draw.push_back(l1);
	_tris_to_draw.push_back(l2);
	_tris_to_draw.push_back(r1);
	_tris_to_draw.push_back(r2);
	_tris_to_draw.push_back(back1);
	_tris_to_draw.push_back(back2);
	_tris_to_draw.push_back(front1);
	_tris_to_draw.push_back(front2);
}

void BoidThingyDingy::_draw_box(){
	// Draw all of the triangles except the last two in the box collision surface
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	for (size_t i=0; i<_tris_to_draw.size(); i++){
		Color col = _tri_colors[i % _tri_colors.size()];
		glColor3f(col.X(), col.Y(), col.Z());
		const Triangle& tri = _tris_to_draw[i];
		glBegin(GL_TRIANGLES);
			glVertex3d(tri.v0.X(), tri.v0.Y(), tri.v0.Z());
			glVertex3d(tri.v1.X(), tri.v1.Y(), tri.v1.Z());
			glVertex3d(tri.v2.X(), tri.v2.Y(), tri.v2.Z());
		glEnd();
	}
}

void BoidThingyDingy::_draw_particles(){
	glColor3d(1.0, 0.0, 0.0);
	glPointSize(5.0f);
	glBegin(GL_POINTS);
	for (size_t i=0; i<_dsd->n_particles(); i++){
		Vector pos = _dsd->get_position(i);
		Color col = _dsd->get_color(i);
		glColor3d(col.red(), col.green(), col.blue());
		glVertex3f(pos.X(), pos.Y(), pos.Z());
	}
	glEnd();
}
