#include "thing_sph.h"


using namespace pba;

void _add_particles_in_box(AABB<Vector>& bounds, float step_size, float mass, DSD_sp dsd){
	for (float x = bounds.lower_left().X(); x < bounds.upper_right().X(); x += step_size){
		for (float y = bounds.lower_left().Y(); y < bounds.upper_right().Y(); y += step_size){
			for (float z = bounds.lower_left().Z(); z < bounds.upper_right().Z(); z += step_size){
				dsd->add(1);
				size_t idx = dsd->n_particles() - 1;
				dsd->set_position(idx, Vector(x, y, z));
				dsd->set_mass(idx, mass);
			}
		}
		printf("on row %f\n", x);
	}
}

Color lerp_color(const Color& c1, const Color& c2, double t){
    Color result;
    result[0] = static_cast<float>(c1.red() + (c2.red() - c1.red()) * t);
    result[1] = static_cast<float>(c1.green() + (c2.green() - c1.green()) * t);
    result[2] = static_cast<float>(c1.blue() + (c2.blue() - c1.blue()) * t);
    return result;
}

SPHThingyDingy::SPHThingyDingy(const std::string& nam)
: PbaThingyDingy(nam) {
	// Start with some default bounds
	AABB bounds(Vector(-3.0, -3.0, -3.0), Vector(3.0, 3.0, 3.0));
	AABB emission_bounds(Vector(-1.25, -1.25, -1.25), Vector(1.25,  1.25, 1.25));
	_particle_emitter_sp = std::make_shared<ParticleEmitter>(emission_bounds);
	_particle_emitter_sp->set_min_speed(0.0);
	_particle_emitter_sp->set_max_speed(0.0);
	// Let's add a single bouncing ball particle
	_dsd = std::make_shared<SPHData>();

    _dsd->set_h(0.30);
    _occupancy_volume = create_idx_occupancy_volume(bounds, _dsd->h() * 2.0);
    //_kernel = std::make_shared<CubicSplineKernel3>(_dsd->h());
	_kernel = std::make_shared<SphSpikyKernel3>(_dsd->h());

	_add_particles_in_box(emission_bounds, _dsd->h() / 2.25, 0.015, _dsd);
	// add a thousand particles to start with
	// for (size_t i=0; i<5000; i++){
	// 	_add_random_particle();
	// }
	// And now our systems, forces and collision surfaces
	_add_random_particle();
	_force_system = std::make_shared<ForceSystem>();
	_solver_system = create_gi_solver_system();
	_box = create_collision_surface();
	_box->set_restitution(0.5);
	_box->set_sticky(0.5);
	_collision_handler = create_collision_handler();
	_collision_handler->register_collision_surface(_box);
	_initialize_box_collision_surface(bounds);
	_gravity_force = std::make_shared<SimpleGravityForce>(Vector(0.0, -9.81, 0.0));
    _viscosity_force = std::make_shared<SPHViscosityForce>(_occupancy_volume, _kernel);
    _pressure_force = std::make_shared<PciSPHPressureForce>(_occupancy_volume, _kernel, _collision_handler);
	_other_pressure_force = std::make_shared<SPHPressureForce>(_occupancy_volume, _kernel);
	_force_system->add_forces(_gravity_force, _viscosity_force, _other_pressure_force);
	
    
	// And now that the init is basically done. Let's build the solvers
	SetSimulationTimestep(0.01667);
	_dsd->set_rest_density(25); // 18
	_dsd->set_rest_pressure(8); // 8
	_dsd->set_gamma(1.0);
	_dsd->set_viscosity_beta(0.75);
	_dsd->set_max_particle_acceleration(40);
	_dsd->set_max_particle_speed(40);
	//_set_to_sixth_order_solver();
	_set_to_leapfrog_solver();
}

void SPHThingyDingy::_add_random_particle(){
	_dsd->add(1);
	Vector pos, vel;
	_particle_emitter_sp->emit(pos, vel);
	size_t idx = _dsd->n_particles() - 1;
	_dsd->set_position(idx, pos);
	_dsd->set_velocity(idx, vel);
	_dsd->set_mass(idx, 0.01);
	Vector col = ParticleEmitter::generate_random_bounded_vector(0.0, 1.0);
	_dsd->set_color(idx, Color(col.X(), col.Y(), col.Z(), 1.0));
}

void SPHThingyDingy::_set_to_backward_euler_solver(){
	// Clear existing solvers
	_solver_system = create_gi_solver_system();

	auto advance_position_solver = std::make_shared<SPHPositionSolver>(_dsd, _collision_handler, _occupancy_volume, _kernel);
    auto advance_velocity_solver = std::make_shared<SPHAdvanceVelocityWithForces>(_dsd, _force_system);
	_solver_system->add_solver(advance_velocity_solver, dt);
	_solver_system->add_solver(advance_position_solver, dt);
	printf("Switched to Backward Euler solver.\n");
}

void SPHThingyDingy::_set_to_forward_euler_solver(){
	// Clear existing solvers
	_solver_system = create_gi_solver_system();

	auto advance_position_solver = std::make_shared<SPHPositionSolver>(_dsd, _collision_handler, _occupancy_volume, _kernel);
    auto advance_velocity_solver = std::make_shared<SPHAdvanceVelocityWithForces>(_dsd, _force_system);
	_solver_system->add_solver(advance_position_solver, dt);
	_solver_system->add_solver(advance_velocity_solver, dt);
	printf("Switched to Forward Euler solver.\n");
}

void SPHThingyDingy::_set_to_leapfrog_solver(){
	_solver_system = create_gi_solver_system();
	auto advance_position_solver = std::make_shared<SPHPositionSolver>(_dsd, _collision_handler, _occupancy_volume, _kernel);
    auto advance_velocity_solver = std::make_shared<SPHAdvanceVelocityWithForces>(_dsd, _force_system);
	_solver_system->add_solver(advance_position_solver, dt / 2.0);
	_solver_system->add_solver(advance_velocity_solver, dt);
	_solver_system->add_solver(advance_position_solver, dt / 2.0);
	printf("Switched to Leapfrog solver.\n");
}

void SPHThingyDingy::_set_to_sixth_order_solver(){
	_solver_system = create_gi_solver_system();
	auto advance_position_solver = std::make_shared<SPHPositionSolver>(_dsd, _collision_handler, _occupancy_volume, _kernel);
    auto advance_velocity_solver = std::make_shared<SPHAdvanceVelocityWithForces>(_dsd, _force_system);
	auto leapfrog_solver = std::make_shared<GISolverLeapfrog>(advance_position_solver, advance_velocity_solver);
	auto sixth_order_solver = std::make_shared<GISolverSixthOrder>(leapfrog_solver);
	_solver_system->add_solver(sixth_order_solver, dt);
	printf("Switched to Sixth Order solver.\n");
}

void SPHThingyDingy::solve(){
	_solver_system->solve(dt);
}

void SPHThingyDingy::Display(){
	_draw_box();
	_draw_particles();
}

void SPHThingyDingy::Keyboard( unsigned char key, int x, int y ){
	switch( key ){
		case 'a':{
			_adjust_acceleration_max_val(0.9);
			break;
		}
		case 'A':{
			_adjust_acceleration_max_val(1.1);
			break;
		}
		case 'b':{
			_adjust_pressure_power(0.9);
			break;
		}
		case 'B':{
			_adjust_pressure_power(1.1);
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
			_adjust_base_density(0.9);
			break;
		}
		case 'D':{
			_adjust_base_density(1.1);
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
		case 'p':{
			_adjust_pressure_strength(0.9);			
			break;
		}
		case 'P':{
			_adjust_pressure_strength(1.1);
			break;
		}
		case 'r':{
			Reset();
			break;
		}
		case 's':{
			_adjust_velocity_max_val(0.9);
			break;
		}
		case 'S':{
			_adjust_velocity_max_val(1.1);
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
			_adjust_viscosity(0.9);
			break;
		}
		case 'V': {
			_adjust_viscosity(1.1);
			break;
		}
		default:
			break;
	}
}

void SPHThingyDingy::_emit_particles(const size_t n){
	for (size_t i=0; i<n; i++){
		_add_random_particle();
	}
	printf("Emitted %zu new particles. Total particle count is now %zu.\n", n, _dsd->n_particles());
}

void SPHThingyDingy::_adjust_gravity(const Vector& delta){
	_gravity_force->set_gravity(_gravity_force->get_gravity() + delta);
	printf("New gravity vector is (%f, %f, %f) m/s^2\n",
		_gravity_force->get_gravity().X(),
		_gravity_force->get_gravity().Y(),
		_gravity_force->get_gravity().Z()
	);
}

void SPHThingyDingy::_adjust_coefficient_of_restitution(const double delta){
	_box->set_restitution( _box->get_restitution() + delta );
	printf("New coefficient of restitution is %f\n", _box->get_restitution());
}

void SPHThingyDingy::Reset(){
	//_dsd->resize(0);
	_dsd->clear();
	_occupancy_volume->clear();
	printf("size of dsd positions = %zu /n", _dsd->get_vector_attribute_span("positions").size());
	_emit_particles(1);
	printf("Simulation reset.\n");
}

void SPHThingyDingy::_adjust_timestep(const double factor){
	dt *= factor;
	SetSimulationTimestep(dt);
	printf("New timestep is %f seconds. RESET SOLVER TO APPLY CHANGES\n", dt);
}

void SPHThingyDingy::_adjust_viscosity(const double factor){
	_dsd->set_viscosity_beta(_dsd->viscosity_beta() * factor);
	printf("New viscosity beta is %f\n", _dsd->viscosity_beta());
}

void SPHThingyDingy::_adjust_pressure_strength(const double factor){
	_dsd->set_rest_pressure(_dsd->rest_pressure() * factor);
	printf("New rest pressure is %f\n", _dsd->rest_pressure());
}

void SPHThingyDingy::_adjust_base_density(const double factor){
	_dsd->set_rest_density(_dsd->rest_density() * factor);
	printf("New rest density is %f\n", _dsd->rest_density());
}

void SPHThingyDingy::_adjust_pressure_power(const double factor){
	_dsd->set_gamma(_dsd->gamma() * factor);
	printf("New pressure power (gamma) is %f\n", _dsd->gamma());
}

void SPHThingyDingy::_adjust_velocity_max_val(const double factor){
	_dsd->set_max_particle_speed(_dsd->get_max_particle_speed() * factor);
	printf("New max particle speed is %f m/s\n", _dsd->get_max_particle_speed());
}

void SPHThingyDingy::_adjust_acceleration_max_val(const double factor){
	_dsd->set_max_particle_acceleration(_dsd->get_max_particle_acceleration() * factor);
	printf("New max particle acceleration is %f m/s^2\n", _dsd->get_max_particle_acceleration());
}

void SPHThingyDingy::Usage(){
	printf("SPH Controls:\n");
	printf("  a/A: Decrease/Increase max particle acceleration\n");
	printf("  b/B: Decrease/Increase pressure power (gamma)\n");
	printf("  c/C: Decrease/Increase coefficient of restitution for box collisions\n");
	printf("  d/D: Decrease/Increase base density of the fluid\n");
	printf("  e: Emit 100 new particles\n");
	printf("  g/G: Increase/Decrease gravity strength\n");
	printf("  l: Switch to Leapfrog solver\n");
	printf("  L: Switch to Sixth Order solver\n");
	printf("  p/P: Decrease/Increase pressure strength\n");
	printf("  r: Reset the simulation\n");
	printf("  s/S: Decrease/Increase max particle speed\n");
	printf("  t/T: Decrease/Increase simulation timestep\n");
	printf("  u/U: Print this usage information\n");
	printf("  v/V: Decrease/Increase viscosity\n");
}


void SPHThingyDingy::_initialize_box_collision_surface(const AABB<Vector>& bounds){
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

void SPHThingyDingy::_draw_box(){
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

void SPHThingyDingy::_draw_particles(){
	//glColor3d(1.0, 0.0, 0.0);
	glPointSize(4.0f);
	glBegin(GL_POINTS);
	for (size_t i=0; i<_dsd->n_particles(); i++){
		Vector pos = _dsd->get_position(i);
		glVertex3f(pos.X(), pos.Y(), pos.Z());
		// Color col = _dsd->get_color(i);
		// glColor3d(col.red(), col.green(), col.blue());
		Vector vel = _dsd->get_velocity(i);
		double speed = vel.magnitude();
		double thresh = 6.0;
		if (speed < thresh){
			double t = speed / thresh; // Normalize speed to [0, 1]
			Color col = lerp_color(Color(0.0, 0.0, 1.0, 1.0), Color(1.0, 1.0, 1.0, 1.0), t);
			glColor4d(col.red(), col.green(), col.blue(), 1.0 - col.red());
		} else {
			glColor4d(1.0, 1.0, 1.0, 0.01);
		}
	}
	glEnd();
}
