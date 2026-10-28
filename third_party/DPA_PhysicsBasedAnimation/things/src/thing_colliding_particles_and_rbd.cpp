#include "thing_colliding_particles_and_rbd.h"


using namespace pba;

Color lerp_color(const Color& c1, const Color& c2, double t){
    Color result;
    result[0] = static_cast<float>(c1.red() + (c2.red() - c1.red()) * t);
    result[1] = static_cast<float>(c1.green() + (c2.green() - c1.green()) * t);
    result[2] = static_cast<float>(c1.blue() + (c2.blue() - c1.blue()) * t);
    return result;
}


CollidingParticlesRBDThing::CollidingParticlesRBDThing(const std::string& nam)
: PbaThingyDingy(nam) {
	AABB bounds(Vector(-3.0, -3.0, -3.0), Vector(3.0, 3.0, 3.0));
	_dsd = std::make_shared<SPHData>();
	_dsd->set_h(0.25);
	_occupancy_volume = create_idx_occupancy_volume(bounds, _dsd->h() * 2.0);
	_kernel = std::make_shared<SphSpikyKernel3>(_dsd->h());
 	_viscosity_force = std::make_shared<SPHViscosityForce>(_occupancy_volume, _kernel);
    _pressure_force = std::make_shared<SPHPressureForce>(_occupancy_volume, _kernel);

    _rbd = std::make_shared<RigidBodyStateData>();
    AABB emission_bounds(Vector(-2.7, -2.7, -2.7), Vector(-2.9, 2.9, 2.9));
    _particle_emitter_sp = std::make_shared<ParticleEmitter>(emission_bounds);
	_particle_emitter_sp->set_min_speed(0.1);
	_particle_emitter_sp->set_max_speed(3.8);

	_create_rigid_body_from_obj(DEFAULT_SOFT_BODY_PATH, Vector(0, 0, 0));

    // And now our systems, forces and collision surfaces
	_force_system = std::make_shared<ForceSystem>();
	_sph_force_system = std::make_shared<ForceSystem>();
	_solver_system = create_gi_solver_system();
	
	_collision_handler = std::make_shared<ParticleRBDCollisionHandler>();
	_box = create_collision_surface();
	_initialize_box_collision_surface(bounds);
	_main_collision_surface = _box;//_create_collision_geo_from(DEFAULT_COLL_PATH);
	_main_collision_surface->set_restitution(0.5);
	_main_collision_surface->set_sticky(0.9);
	_collision_handler->register_collision_surface(_main_collision_surface);
    _collision_handler->register_rbd(_rbd);

	_gravity_force = std::make_shared<SimpleGravityForce>(Vector(0.0, -9.81, 0.0));
    _force_system->add_forces(_gravity_force);
	_sph_force_system->add_forces(_gravity_force);
	_sph_force_system->add_force(_viscosity_force);
	_sph_force_system->add_force(_pressure_force);

	_dsd->set_rest_density(18);
	_dsd->set_rest_pressure(8);
	_dsd->set_gamma(0.8);
	_dsd->set_viscosity_beta(0.2);
	_dsd->set_max_particle_acceleration(20);
	_dsd->set_max_particle_speed(20);

    _emit_particles(2000);
	
	// And now that the init is basically done. Let's build the solvers
	SetSimulationTimestep(0.01667 / 2.0);
	//_set_to_sixth_order_solver();
	//_set_to_backward_euler_solver();
	_set_to_leapfrog_solver();
	//_set_to_forward_euler_solver();
}

void CollidingParticlesRBDThing::Init( const std::vector<std::string>& args ) {
	//void SetCameraEyeViewUp( float eyex, float eyey, float eyez, float viewx, float viewy, float viewz, float upx, float upy, float upz ); 
	viewer->SetCameraEyeViewUp(0, 0, -22, 0, 0, 1, 0, 1, 0);
}


void CollidingParticlesRBDThing::_set_to_backward_euler_solver(){
	// Clear existing solvers
	// _solver_system = create_gi_solver_system();

	// auto advance_position_solver = std::make_shared<AdvanceRotationAndCOMWithCollisions>(_dsd, _collision_handler);
   	// //auto advance_position_solver = std::make_shared<AdvanceRotationAndCOM>(_dsd);

	// auto advance_velocity_solver = std::make_shared<AdvanceAngularVelocityAndVelocity>(_dsd, _force_system);
	// _solver_system->add_solver(advance_velocity_solver, dt);
	// _solver_system->add_solver(advance_position_solver, dt);
	// printf("Switched to Backward Euler solver.\n");
}

void CollidingParticlesRBDThing::_set_to_forward_euler_solver(){
	// Clear existing solvers
	// _solver_system = create_gi_solver_system();

	// auto advance_position_solver = std::make_shared<AdvanceRotationAndCOMWithCollisions>(_dsd, _collision_handler);
   	// //auto advance_position_solver = std::make_shared<AdvanceRotationAndCOM>(_dsd);

	// auto advance_velocity_solver = std::make_shared<AdvanceAngularVelocityAndVelocity>(_dsd, _force_system);
	// _solver_system->add_solver(advance_position_solver, dt);
	// _solver_system->add_solver(advance_velocity_solver, dt);
	// printf("Switched to Forward Euler solver.\n");
}

void CollidingParticlesRBDThing::_set_to_leapfrog_solver(){
	_solver_system = create_gi_solver_system();
	// The position partial solver should also update the rbd
    // auto advance_position_solver = std::make_shared<PartialSolverAdvancePosition>(_dsd, _collision_handler);
    // auto advance_velocity_solver = std::make_shared<AdvanceVelocityWithForces>(_dsd, _force_system);
	auto advance_position_solver = std::make_shared<SPHPositionSolver>(_dsd, _collision_handler, _occupancy_volume, _kernel);
    auto advance_velocity_solver = std::make_shared<SPHAdvanceVelocityWithForces>(_dsd, _sph_force_system);

    // auto advance_p_rbd = std::make_shared<AdvanceRotationAndCOMWithCollisions>(_rbd, _collision_handler);
    auto advance_v_rbd = std::make_shared<AdvanceAngularVelocityAndVelocity>(_rbd, _force_system);
	_solver_system->add_solver(advance_position_solver, dt / 2.0);
	_solver_system->add_solver(advance_velocity_solver, dt);
	_solver_system->add_solver(advance_v_rbd, dt);
    _solver_system->add_solver(advance_position_solver, dt / 2.0);

	printf("Switched to Leapfrog solver.\n");
}

void CollidingParticlesRBDThing::_set_to_sixth_order_solver(){
	// _solver_system = create_gi_solver_system();
	// auto advance_position_solver = std::make_shared<AdvanceRotationAndCOMWithCollisions>(_dsd, _collision_handler);
   	// //auto advance_position_solver = std::make_shared<AdvanceRotationAndCOM>(_dsd);

	// auto advance_velocity_solver = std::make_shared<AdvanceAngularVelocityAndVelocity>(_dsd, _force_system);
	// auto leapfrog_solver = std::make_shared<GISolverLeapfrog>(advance_position_solver, advance_velocity_solver);
	// auto sixth_order_solver = std::make_shared<GISolverSixthOrder>(leapfrog_solver);
	// _solver_system->add_solver(sixth_order_solver, dt);
	// printf("Switched to Sixth Order solver.\n");
}

void CollidingParticlesRBDThing::solve(){
	_solver_system->solve(dt);
}

void CollidingParticlesRBDThing::Display(){
	_draw_tris();
	_draw_particles();
}

void CollidingParticlesRBDThing::Keyboard( unsigned char key, int x, int y ){
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
			_adjust_coefficient_of_restitution(0.9);
			break;
		}
		case 'C':{
			_adjust_coefficient_of_restitution(1.1);
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
			_emit_particles(10);
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
			_adjust_coefficient_of_sticky(0.9);
			break;
		}
		case 'S':{
			_adjust_coefficient_of_sticky(1.1);
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

void CollidingParticlesRBDThing::_emit_particles(const size_t n){
    //_create_rigid_body_from_obj(DEFAULT_SOFT_BODY_PATH, Vector(0, 0, 0));
	//printf("Emitted %zu new particles. Total particle count is now %zu.\n", n, _dsd->n_particles());
    for (size_t i = 0; i < n; i++){
        _add_random_particle();
    }
}

void CollidingParticlesRBDThing::_add_random_particle(){
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

void CollidingParticlesRBDThing::_adjust_gravity(const Vector& delta){
	_gravity_force->set_gravity(_gravity_force->get_gravity() + delta);
	printf("New gravity vector is (%f, %f, %f) m/s^2\n",
		_gravity_force->get_gravity().X(),
		_gravity_force->get_gravity().Y(),
		_gravity_force->get_gravity().Z()
	);
}

void CollidingParticlesRBDThing::_adjust_coefficient_of_restitution(const double delta){
	_main_collision_surface->set_restitution( _main_collision_surface->get_restitution() * delta );
	printf("New coefficient of restitution is %f\n", _main_collision_surface->get_restitution());
}

void CollidingParticlesRBDThing::_adjust_coefficient_of_sticky(const double delta){
	_main_collision_surface->set_sticky( _main_collision_surface->get_sticky() * delta );
	printf("New coefficient of sticky is %f\n", _main_collision_surface->get_sticky());
}

void CollidingParticlesRBDThing::Reset(){
	_dsd->resize(0);
	_emit_particles(1);
	printf("Simulation reset.\n");
}

void CollidingParticlesRBDThing::_adjust_timestep(const double factor){
	dt *= factor;
	SetSimulationTimestep(dt);
	printf("New timestep is %f seconds. RESET SOLVER TO APPLY CHANGES\n", dt);
}


void CollidingParticlesRBDThing::_create_rigid_body_from_obj(const std::string& file_name, const Vector& center){
    printf("Hard coding file for now \n");
    std::filesystem::path current_dir = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::path obj_file_path = current_dir / file_name;

    // we are just going to read it every time for now
    ObjReader<Vector> r(obj_file_path);
    _rbd->create_from_obj(r, center);
}

CollisionSurface_sp CollidingParticlesRBDThing::_create_collision_geo_from(const std::string& file_name){
	printf("Hard coding collision geo for now");
	std::filesystem::path current_dir = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::path obj_file_path = current_dir / file_name;
	 ObjReader<Vector> r(obj_file_path);
    printf("Reading obj from file %s\n", obj_file_path.string().c_str());
    auto verts = r.get_verts();
    auto faces = r.get_faces();

	// We need to create collision tris from the faces/verts
	std::vector<Triangle> tris(faces.size(), Triangle(Vector(0, 0, 0), Vector(0, 0, 0), Vector(0, 0, 0)));
	std::transform(std::execution::par, faces.begin(), faces.end(), tris.begin(),
		 [&verts](const cato::Vec3i& face) {
			return Triangle(
				verts[face.x() - 1],
				verts[face.y() - 1],
				verts[face.z() - 1]
			);
	});

	// Add the tris to the collision surface
	CollisionSurface_sp collision_geo = create_collision_surface();
	for (const auto& tri : tris){
		collision_geo->add_collision_object(std::make_shared<CollisionTriangle>(tri));
		_tris_to_draw.push_back(tri);
	}
	return collision_geo;
}

void CollidingParticlesRBDThing::_adjust_viscosity(const double factor){
	_dsd->set_viscosity_beta(_dsd->viscosity_beta() * factor);
	printf("New viscosity beta is %f\n", _dsd->viscosity_beta());
}

void CollidingParticlesRBDThing::_adjust_pressure_strength(const double factor){
	_dsd->set_rest_pressure(_dsd->rest_pressure() * factor);
	printf("New rest pressure is %f\n", _dsd->rest_pressure());
}

void CollidingParticlesRBDThing::_adjust_base_density(const double factor){
	_dsd->set_rest_density(_dsd->rest_density() * factor);
	printf("New rest density is %f\n", _dsd->rest_density());
}

void CollidingParticlesRBDThing::_adjust_pressure_power(const double factor){
	_dsd->set_gamma(_dsd->gamma() * factor);
	printf("New pressure power (gamma) is %f\n", _dsd->gamma());
}

void CollidingParticlesRBDThing::_adjust_velocity_max_val(const double factor){
	_dsd->set_max_particle_speed(_dsd->get_max_particle_speed() * factor);
	printf("New max particle speed is %f m/s\n", _dsd->get_max_particle_speed());
}

void CollidingParticlesRBDThing::_adjust_acceleration_max_val(const double factor){
	_dsd->set_max_particle_acceleration(_dsd->get_max_particle_acceleration() * factor);
	printf("New max particle acceleration is %f m/s^2\n", _dsd->get_max_particle_acceleration());
}

void CollidingParticlesRBDThing::Usage(){
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

void CollidingParticlesRBDThing::_draw_tris(){
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

void CollidingParticlesRBDThing::_draw_particles(){
	glColor3d(1.0, 0.0, 0.0);
	glPointSize(10.5f);
	glBegin(GL_POINTS);
	for (size_t i=0; i<_dsd->n_particles(); i++){
		Vector pos = _dsd->get_position(i);
		glVertex3f(pos.X(), pos.Y(), pos.Z());
		//Color col = _dsd->get_color(i);
		//glColor3d(col.red(), col.green(), col.blue());
		// The goal is to set the particles to be blue when stationary and white
		// when vel is over 1.0
		Vector vel = _dsd->get_velocity(i);
		double speed = vel.magnitude();
		double thresh = 6.0;
		if (speed < thresh){
			double t = speed / thresh; // Normalize speed to [0, 1]
			Color col = lerp_color(Color(0.0, 0.0, 1.0, 1.0), Color(1.0, 1.0, 1.0, 1.0), t);
			glColor3d(col.red(), col.green(), col.blue());
		} else {
			glColor3d(1.0, 1.0, 1.0);
		}
	}
    glColor3d(0.0, 1.0, 0.0);
    glPointSize(20.0f);
    for (size_t i=0; i<_rbd->n_particles(); i++){
        Vector pos = _rbd->get_vert_pos(i);
		glVertex3f(pos.X(), pos.Y(), pos.Z());
    }
	glEnd();
	// draw triangles from the box of the rbd
	glBegin(GL_TRIANGLES);
	auto face_indices = _rbd->get_face_indices();
	for (size_t i=0; i<face_indices.size(); i++){
		auto face = face_indices[i];
		Vector v0 = _rbd->get_vert_pos(face.x());
		Vector v1 = _rbd->get_vert_pos(face.y());
		Vector v2 = _rbd->get_vert_pos(face.z());
		glVertex3d(v0.X(), v0.Y(), v0.Z());
		glVertex3d(v1.X(), v1.Y(), v1.Z());
		glVertex3d(v2.X(), v2.Y(), v2.Z());
	}
	glEnd();
}

void CollidingParticlesRBDThing::_initialize_box_collision_surface(const AABB<Vector>& bounds){
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